#include "adapters/itk/ItkVascularPreprocessor.h"

#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/IVoxelSource.h"

#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkHessianToObjectnessMeasureImageFilter.h"
#include "itkImage.h"
#include "itkMacro.h"
#include "itkMultiScaleHessianBasedMeasureImageFilter.h"
#include "itkSymmetricSecondRankTensor.h"
#include "itkVersion.h"

#include "picosha2.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

constexpr unsigned int kDimension = 3;
constexpr double kDirectionDeterminantEpsilon = 1e-10;
constexpr double kGeometryTolerance = 1e-12;
constexpr double kMaximumUnspacedDiffusionTimeStep = 0.0625;

using FloatImage = itk::Image<float, kDimension>;
using HessianPixel = itk::SymmetricSecondRankTensor<float, kDimension>;
using HessianImage = itk::Image<HessianPixel, kDimension>;
using DiffusionFilter =
    itk::CurvatureAnisotropicDiffusionImageFilter<FloatImage, FloatImage>;
using ObjectnessFilter =
    itk::HessianToObjectnessMeasureImageFilter<HessianImage, FloatImage>;
using MultiScaleFilter =
    itk::MultiScaleHessianBasedMeasureImageFilter<FloatImage, HessianImage, FloatImage>;

using Clock = std::chrono::steady_clock;

class FingerprintBuilder {
public:
    void addBytes(const void* data, std::size_t size)
    {
        if (size == 0) {
            return;
        }
        const auto* first = static_cast<const picosha2::byte_t*>(data);
        hasher_.process(first, first + size);
    }

    void addByte(std::uint8_t value)
    {
        addBytes(&value, sizeof(value));
    }

    void addUnsigned(std::uint64_t value)
    {
        std::uint8_t bytes[8] = {};
        for (std::size_t index = 0; index < 8; ++index) {
            bytes[index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xffu);
        }
        addBytes(bytes, sizeof(bytes));
    }

    void addDouble(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t),
                      "Fingerprint encoding requires IEEE-754 binary64 storage");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        addUnsigned(bits);
    }

    void addString(const std::string& value)
    {
        addUnsigned(static_cast<std::uint64_t>(value.size()));
        addBytes(value.data(), value.size());
    }

    std::string finish(const char* prefix)
    {
        hasher_.finish();
        return std::string(prefix) + picosha2::get_hash_hex_string(hasher_);
    }

private:
    picosha2::hash256_one_by_one hasher_;
};

double elapsedMilliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool cancellationRequested(const VascularPreprocessCancellation* cancellation)
{
    return cancellation != nullptr && cancellation->isCancellationRequested();
}

void addDiagnostic(VascularPreprocessResult* result,
                   DiagnosticSeverity severity,
                   VascularPreprocessDiagnosticCode code,
                   VascularPreprocessStage stage,
                   std::initializer_list<double> numericContext = {})
{
    if (result == nullptr) {
        return;
    }
    VascularPreprocessDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.stage = stage;
    diagnostic.messageKey = vascularPreprocessDiagnosticToken(code);
    diagnostic.numericContext.assign(numericContext.begin(), numericContext.end());
    result->diagnostics.push_back(std::move(diagnostic));
}

VascularPreprocessResult failure(Clock::time_point start,
                                 VascularPreprocessStatus status,
                                 VascularPreprocessStage stage,
                                 VascularPreprocessDiagnosticCode code,
                                 std::initializer_list<double> numericContext = {})
{
    VascularPreprocessResult result;
    result.status = status;
    result.stage = stage;
    result.elapsedMilliseconds = elapsedMilliseconds(start);
    result.output.reset();
    addDiagnostic(&result, DiagnosticSeverity::Error, code, stage, numericContext);
    return result;
}

bool checkedVoxelCount(const int dimensions[3], std::size_t* count)
{
    if (count == nullptr || dimensions[0] <= 0 || dimensions[1] <= 0
        || dimensions[2] <= 0) {
        return false;
    }
    std::size_t value = 1;
    for (int axis = 0; axis < 3; ++axis) {
        const std::size_t dimension = static_cast<std::size_t>(dimensions[axis]);
        if (value > (std::numeric_limits<std::size_t>::max)() / dimension) {
            return false;
        }
        value *= dimension;
    }
    *count = value;
    return true;
}

double directionDeterminant(const double direction[3][3])
{
    return direction[0][0]
            * (direction[1][1] * direction[2][2]
               - direction[1][2] * direction[2][1])
        - direction[0][1]
            * (direction[1][0] * direction[2][2]
               - direction[1][2] * direction[2][0])
        + direction[0][2]
            * (direction[1][0] * direction[2][1]
               - direction[1][1] * direction[2][0]);
}

std::uint64_t scalarTypeFingerprintId(ScalarType type)
{
    switch (type) {
    case ScalarType::Int8: return 1;
    case ScalarType::UInt8: return 2;
    case ScalarType::Int16: return 3;
    case ScalarType::UInt16: return 4;
    case ScalarType::Int32: return 5;
    case ScalarType::UInt32: return 6;
    case ScalarType::Float32: return 7;
    case ScalarType::Float64: return 8;
    case ScalarType::Unknown: return 0;
    }
    return 0;
}

std::string profileFingerprint(const VascularPreprocessProfileV1& profile)
{
    FingerprintBuilder builder;
    builder.addString("xq-vascular-preprocess-profile-v1");
    builder.addString(profile.profileId);
    builder.addUnsigned(profile.schemaVersion);
    builder.addUnsigned(profile.diffusionIterations);
    builder.addDouble(profile.diffusionTimeStep);
    builder.addDouble(profile.diffusionConductance);
    builder.addDouble(profile.sigmaMinimumMm);
    builder.addDouble(profile.sigmaMaximumMm);
    builder.addUnsigned(profile.sigmaSteps);
    builder.addUnsigned(profile.sigmaStepMethod == VascularSigmaStepMethod::Logarithmic
                            ? 1u
                            : 2u);
    builder.addDouble(profile.alpha);
    builder.addDouble(profile.beta);
    builder.addDouble(profile.gamma);
    builder.addByte(profile.brightObject ? 1u : 0u);
    builder.addByte(profile.scaleObjectness ? 1u : 0u);
    return builder.finish("xq-vascular-profile-v1:sha256:");
}

std::string inputFingerprint(const XQImageVolume& image, const VoxelView& view)
{
    const ImageGeometry& geometry = image.geometry();
    FingerprintBuilder builder;
    builder.addString("xq-vascular-input-v1");
    builder.addUnsigned(scalarTypeFingerprintId(view.type));
    builder.addUnsigned(static_cast<std::uint64_t>(view.components));
    for (int axis = 0; axis < 3; ++axis) {
        builder.addUnsigned(static_cast<std::uint64_t>(geometry.dimensions[axis]));
        builder.addDouble(geometry.spacing[axis]);
        builder.addDouble(geometry.origin[axis]);
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            builder.addDouble(geometry.direction[row][column]);
        }
    }
    builder.addUnsigned(geometry.coordinateSystem == ImageCoordinateSystem::LPS ? 1u : 2u);
    builder.addUnsigned(static_cast<std::uint64_t>(view.bytes.size()));
    builder.addBytes(view.bytes.data(), view.bytes.size());
    return builder.finish("xq-vascular-input-v1:sha256:");
}

std::string outputFingerprint(const XQImageVolume& image,
                              const std::vector<std::uint8_t>& bytes,
                              const std::string& input,
                              const std::string& profile,
                              const std::string& algorithmVersion,
                              const std::string& itkVersion)
{
    const ImageGeometry& geometry = image.geometry();
    FingerprintBuilder builder;
    builder.addString("xq-vesselness-output-v1");
    builder.addString(input);
    builder.addString(profile);
    builder.addString(algorithmVersion);
    builder.addString(itkVersion);
    for (int axis = 0; axis < 3; ++axis) {
        builder.addUnsigned(static_cast<std::uint64_t>(geometry.dimensions[axis]));
        builder.addDouble(geometry.spacing[axis]);
        builder.addDouble(geometry.origin[axis]);
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            builder.addDouble(geometry.direction[row][column]);
        }
    }
    builder.addUnsigned(static_cast<std::uint64_t>(bytes.size()));
    builder.addBytes(bytes.data(), bytes.size());
    return builder.finish("xq-vesselness-output-v1:sha256:");
}

template <typename T>
bool copyTypedScalars(const std::uint8_t* bytes,
                      std::size_t count,
                      float* output,
                      double* minimum,
                      double* maximum,
                      bool* outOfFloatRange)
{
    for (std::size_t index = 0; index < count; ++index) {
        T value{};
        std::memcpy(&value, bytes + index * sizeof(T), sizeof(T));
        const double scalar = static_cast<double>(value);
        if (!std::isfinite(scalar)) {
            return false;
        }
        if (scalar > static_cast<double>((std::numeric_limits<float>::max)())
            || scalar < -static_cast<double>((std::numeric_limits<float>::max)())) {
            *outOfFloatRange = true;
            return false;
        }
        const float converted = static_cast<float>(scalar);
        if (!std::isfinite(converted)) {
            *outOfFloatRange = true;
            return false;
        }
        output[index] = converted;
        *minimum = (std::min)(*minimum, scalar);
        *maximum = (std::max)(*maximum, scalar);
    }
    return true;
}

bool copyScalarsToFloat(const VoxelView& view,
                        float* output,
                        double* minimum,
                        double* maximum,
                        bool* outOfFloatRange)
{
    if (output == nullptr || minimum == nullptr || maximum == nullptr
        || outOfFloatRange == nullptr) {
        return false;
    }
    *minimum = (std::numeric_limits<double>::max)();
    *maximum = (std::numeric_limits<double>::lowest)();
    *outOfFloatRange = false;
    const std::size_t count = view.voxelCount();
    const std::uint8_t* bytes = view.bytes.data();
    switch (view.type) {
    case ScalarType::Int8:
        return copyTypedScalars<std::int8_t>(bytes, count, output, minimum, maximum,
                                             outOfFloatRange);
    case ScalarType::UInt8:
        return copyTypedScalars<std::uint8_t>(bytes, count, output, minimum, maximum,
                                              outOfFloatRange);
    case ScalarType::Int16:
        return copyTypedScalars<std::int16_t>(bytes, count, output, minimum, maximum,
                                              outOfFloatRange);
    case ScalarType::UInt16:
        return copyTypedScalars<std::uint16_t>(bytes, count, output, minimum, maximum,
                                               outOfFloatRange);
    case ScalarType::Int32:
        return copyTypedScalars<std::int32_t>(bytes, count, output, minimum, maximum,
                                              outOfFloatRange);
    case ScalarType::UInt32:
        return copyTypedScalars<std::uint32_t>(bytes, count, output, minimum, maximum,
                                               outOfFloatRange);
    case ScalarType::Float32:
        return copyTypedScalars<float>(bytes, count, output, minimum, maximum,
                                       outOfFloatRange);
    case ScalarType::Float64:
        return copyTypedScalars<double>(bytes, count, output, minimum, maximum,
                                        outOfFloatRange);
    case ScalarType::Unknown:
        break;
    }
    return false;
}

FloatImage::Pointer makeItkImage(const ImageGeometry& geometry,
                                 const VoxelView& view,
                                 double* minimum,
                                 double* maximum,
                                 bool* outOfFloatRange)
{
    FloatImage::SizeType size;
    FloatImage::IndexType start;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        size[axis] = static_cast<FloatImage::SizeType::SizeValueType>(
            geometry.dimensions[axis]);
        start[axis] = 0;
    }
    FloatImage::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    FloatImage::Pointer itkImage = FloatImage::New();
    itkImage->SetRegions(region);

    FloatImage::SpacingType spacing;
    FloatImage::PointType origin;
    FloatImage::DirectionType direction;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        spacing[axis] = geometry.spacing[axis];
        origin[axis] = geometry.origin[axis];
        for (unsigned int column = 0; column < kDimension; ++column) {
            direction[axis][column] = geometry.direction[axis][column];
        }
    }
    itkImage->SetSpacing(spacing);
    itkImage->SetOrigin(origin);
    itkImage->SetDirection(direction);
    itkImage->Allocate();

    if (!copyScalarsToFloat(view, itkImage->GetBufferPointer(), minimum, maximum,
                            outOfFloatRange)) {
        return nullptr;
    }
    return itkImage;
}

bool nearlyEqual(double left, double right)
{
    const double scale = (std::max)({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= kGeometryTolerance * scale;
}

bool sameItkGeometry(const FloatImage* image, const ImageGeometry& expected)
{
    if (image == nullptr) {
        return false;
    }
    const FloatImage::RegionType region = image->GetLargestPossibleRegion();
    const FloatImage::SizeType size = region.GetSize();
    const FloatImage::SpacingType spacing = image->GetSpacing();
    const FloatImage::PointType origin = image->GetOrigin();
    const FloatImage::DirectionType direction = image->GetDirection();
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        if (size[axis] != static_cast<FloatImage::SizeType::SizeValueType>(
                              expected.dimensions[axis])
            || !nearlyEqual(spacing[axis], expected.spacing[axis])
            || !nearlyEqual(origin[axis], expected.origin[axis])) {
            return false;
        }
        for (unsigned int column = 0; column < kDimension; ++column) {
            if (!nearlyEqual(direction[axis][column],
                             expected.direction[axis][column])) {
                return false;
            }
        }
    }
    return true;
}

bool matchingSourceMeta(const VoxelMeta& meta,
                        const XQImageVolume& image,
                        std::size_t expectedVoxelCount)
{
    const ImageGeometry& geometry = image.geometry();
    return meta.valid && meta.type == image.scalarType()
        && meta.components == image.componentCount()
        && meta.voxelCount == expectedVoxelCount
        && meta.dims[0] == geometry.dimensions[0]
        && meta.dims[1] == geometry.dimensions[1]
        && meta.dims[2] == geometry.dimensions[2];
}

} // namespace

const char* ItkVascularPreprocessor::algorithmId()
{
    return "itk.curvature-anisotropic-diffusion.multiscale-hessian-objectness";
}

const char* ItkVascularPreprocessor::algorithmVersion()
{
    return "xq.itk-vascular-preprocess.v1";
}

VascularPreprocessResult ItkVascularPreprocessor::run(
    const XQImageVolume& image,
    const IVoxelSource& source,
    const VascularPreprocessProfileV1& profile,
    const VascularPreprocessCancellation* cancellation) const
{
    const Clock::time_point start = Clock::now();
    VascularPreprocessStage currentStage = VascularPreprocessStage::ValidateInput;

    try {
        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        const VascularPreprocessProfileValidationCode profileStatus =
            validateVascularPreprocessProfile(profile);
        if (profileStatus != VascularPreprocessProfileValidationCode::Ok) {
            return failure(start, VascularPreprocessStatus::InvalidProfile,
                           currentStage,
                           VascularPreprocessDiagnosticCode::InvalidProfile,
                           {static_cast<double>(profileStatus)});
        }
        if (!image.hasGeometry()) {
            return failure(start, VascularPreprocessStatus::InvalidGeometry,
                           currentStage,
                           VascularPreprocessDiagnosticCode::MissingGeometry);
        }
        const ImageGeometry& geometry = image.geometry();
        if (geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
            return failure(start, VascularPreprocessStatus::InvalidGeometry,
                           currentStage,
                           VascularPreprocessDiagnosticCode::UnsupportedCoordinateSystem);
        }

        std::size_t voxelCount = 0;
        if (!checkedVoxelCount(geometry.dimensions, &voxelCount)) {
            return failure(start, VascularPreprocessStatus::InvalidGeometry,
                           currentStage,
                           VascularPreprocessDiagnosticCode::InvalidDimensions);
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(geometry.spacing[axis])
                || !std::isfinite(geometry.origin[axis])) {
                return failure(start, VascularPreprocessStatus::InvalidGeometry,
                               currentStage,
                               VascularPreprocessDiagnosticCode::NonFiniteGeometry,
                               {static_cast<double>(axis)});
            }
            if (geometry.spacing[axis] <= 0.0) {
                return failure(start, VascularPreprocessStatus::InvalidGeometry,
                               currentStage,
                               VascularPreprocessDiagnosticCode::NonPositiveSpacing,
                               {static_cast<double>(axis), geometry.spacing[axis]});
            }
            for (int column = 0; column < 3; ++column) {
                if (!std::isfinite(geometry.direction[axis][column])) {
                    return failure(start, VascularPreprocessStatus::InvalidGeometry,
                                   currentStage,
                                   VascularPreprocessDiagnosticCode::NonFiniteGeometry,
                                   {static_cast<double>(axis),
                                    static_cast<double>(column)});
                }
            }
        }
        const double determinant = directionDeterminant(geometry.direction);
        if (!std::isfinite(determinant)
            || std::abs(determinant) <= kDirectionDeterminantEpsilon) {
            return failure(start, VascularPreprocessStatus::InvalidGeometry,
                           currentStage,
                           VascularPreprocessDiagnosticCode::SingularDirection,
                           {determinant});
        }
        if (image.componentCount() != 1) {
            return failure(start, VascularPreprocessStatus::UnsupportedInput,
                           currentStage,
                           VascularPreprocessDiagnosticCode::NotSingleComponent,
                           {static_cast<double>(image.componentCount())});
        }
        if (XQMemoryImageBufferHandle::scalarSize(image.scalarType()) == 0) {
            return failure(start, VascularPreprocessStatus::UnsupportedInput,
                           currentStage,
                           VascularPreprocessDiagnosticCode::UnsupportedScalarType);
        }

        const double minimumSpacing =
            (std::min)({geometry.spacing[0], geometry.spacing[1], geometry.spacing[2]});
        const double stableTimeStep =
            (std::min)(kMaximumUnspacedDiffusionTimeStep, minimumSpacing / 16.0);
        if (profile.diffusionTimeStep > stableTimeStep) {
            return failure(start, VascularPreprocessStatus::InvalidProfile,
                           currentStage,
                           VascularPreprocessDiagnosticCode::DiffusionTimeStepUnstable,
                           {profile.diffusionTimeStep, stableTimeStep, minimumSpacing});
        }

        currentStage = VascularPreprocessStage::AcquireInput;
        const VoxelMeta meta = source.meta();
        if (!meta.valid) {
            return failure(start, VascularPreprocessStatus::InvalidSource,
                           currentStage,
                           VascularPreprocessDiagnosticCode::SourceMetadataInvalid);
        }
        if (!matchingSourceMeta(meta, image, voxelCount)) {
            return failure(start, VascularPreprocessStatus::InvalidSource,
                           currentStage,
                           VascularPreprocessDiagnosticCode::SourceMetadataMismatch);
        }
        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        FloatImage::Pointer itkInput;
        std::string sourceFingerprint;
        double inputMinimum = 0.0;
        double inputMaximum = 0.0;
        {
            VoxelLease whole = source.acquire_whole();
            const VoxelView& view = whole.view();
            if (!view.valid) {
                return failure(start, VascularPreprocessStatus::InvalidSource,
                               currentStage,
                               VascularPreprocessDiagnosticCode::SourceAcquireFailed);
            }
            if (view.type != meta.type || view.components != meta.components
                || view.dims[0] != meta.dims[0] || view.dims[1] != meta.dims[1]
                || view.dims[2] != meta.dims[2]
                || view.voxelCount() != voxelCount) {
                return failure(start, VascularPreprocessStatus::InvalidSource,
                               currentStage,
                               VascularPreprocessDiagnosticCode::SourceMetadataMismatch);
            }
            const std::size_t scalarSize =
                XQMemoryImageBufferHandle::scalarSize(view.type);
            if (voxelCount > (std::numeric_limits<std::size_t>::max)() / scalarSize
                || view.bytes.size() != voxelCount * scalarSize) {
                return failure(start, VascularPreprocessStatus::InvalidSource,
                               currentStage,
                               VascularPreprocessDiagnosticCode::SourceByteSizeMismatch,
                               {static_cast<double>(view.bytes.size()),
                                static_cast<double>(voxelCount * scalarSize)});
            }

            sourceFingerprint = inputFingerprint(image, view);
            currentStage = VascularPreprocessStage::ImportImage;
            bool outOfFloatRange = false;
            itkInput = makeItkImage(geometry, view, &inputMinimum, &inputMaximum,
                                    &outOfFloatRange);
            if (itkInput.IsNull()) {
                return failure(start, VascularPreprocessStatus::UnsupportedInput,
                               currentStage,
                               outOfFloatRange
                                   ? VascularPreprocessDiagnosticCode::InputScalarOutOfFloatRange
                                   : VascularPreprocessDiagnosticCode::InputScalarNonFinite);
            }
        }

        if (!sameItkGeometry(itkInput, geometry)) {
            return failure(start, VascularPreprocessStatus::ProcessingFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::OutputGeometryMismatch);
        }
        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        currentStage = VascularPreprocessStage::Diffusion;
        DiffusionFilter::Pointer diffusion = DiffusionFilter::New();
        diffusion->SetInput(itkInput);
        diffusion->SetNumberOfIterations(profile.diffusionIterations);
        diffusion->SetTimeStep(profile.diffusionTimeStep);
        diffusion->SetConductanceParameter(profile.diffusionConductance);
        diffusion->SetUseImageSpacing(true);
        diffusion->Update();

        FloatImage::Pointer diffused = diffusion->GetOutput();
        diffused->DisconnectPipeline();
        if (!sameItkGeometry(diffused, geometry)) {
            return failure(start, VascularPreprocessStatus::ProcessingFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::OutputGeometryMismatch);
        }
        diffusion = nullptr;
        itkInput = nullptr;

        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        currentStage = VascularPreprocessStage::Vesselness;
        ObjectnessFilter::Pointer objectness = ObjectnessFilter::New();
        objectness->SetObjectDimension(1);
        objectness->SetBrightObject(profile.brightObject);
        objectness->SetScaleObjectnessMeasure(profile.scaleObjectness);
        objectness->SetAlpha(profile.alpha);
        objectness->SetBeta(profile.beta);
        objectness->SetGamma(profile.gamma);

        MultiScaleFilter::Pointer multiScale = MultiScaleFilter::New();
        multiScale->SetInput(diffused);
        multiScale->SetHessianToMeasureFilter(objectness);
        multiScale->SetSigmaMinimum(profile.sigmaMinimumMm);
        multiScale->SetSigmaMaximum(profile.sigmaMaximumMm);
        multiScale->SetNumberOfSigmaSteps(profile.sigmaSteps);
        multiScale->SetNonNegativeHessianBasedMeasure(true);
        if (profile.sigmaStepMethod == VascularSigmaStepMethod::Logarithmic) {
            multiScale->SetSigmaStepMethodToLogarithmic();
        } else {
            multiScale->SetSigmaStepMethodToEquispaced();
        }
        multiScale->Update();

        FloatImage::Pointer vesselness = multiScale->GetOutput();
        vesselness->DisconnectPipeline();
        if (!sameItkGeometry(vesselness, geometry)
            || vesselness->GetLargestPossibleRegion().GetNumberOfPixels()
                != voxelCount) {
            return failure(start, VascularPreprocessStatus::ProcessingFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::OutputGeometryMismatch);
        }
        multiScale = nullptr;
        objectness = nullptr;
        diffused = nullptr;

        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        currentStage = VascularPreprocessStage::MaterializeOutput;
        const float* values = vesselness->GetBufferPointer();
        float outputMinimum = (std::numeric_limits<float>::max)();
        float outputMaximum = (std::numeric_limits<float>::lowest)();
        std::size_t positiveCount = 0;
        for (std::size_t index = 0; index < voxelCount; ++index) {
            const float value = values[index];
            if (!std::isfinite(value)) {
                return failure(start, VascularPreprocessStatus::ProcessingFailed,
                               currentStage,
                               VascularPreprocessDiagnosticCode::OutputScalarNonFinite,
                               {static_cast<double>(index)});
            }
            outputMinimum = (std::min)(outputMinimum, value);
            outputMaximum = (std::max)(outputMaximum, value);
            if (value > 0.0f) {
                ++positiveCount;
            }
        }
        if (positiveCount == 0 || !(outputMaximum > 0.0f)) {
            return failure(start, VascularPreprocessStatus::EmptyOutput,
                           currentStage,
                           VascularPreprocessDiagnosticCode::EmptyVesselness);
        }

        if (voxelCount > (std::numeric_limits<std::size_t>::max)() / sizeof(float)) {
            return failure(start, VascularPreprocessStatus::AllocationFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::AllocationFailed);
        }
        std::vector<std::uint8_t> outputBytes(voxelCount * sizeof(float));
        std::memcpy(outputBytes.data(), values, outputBytes.size());
        vesselness = nullptr;

        XQImageVolume outputImage;
        outputImage.setGeometry(geometry);
        outputImage.setScalarType(ScalarType::Float32);
        outputImage.setComponentCount(1);
        outputImage.setIntensityRange(
            {static_cast<double>(outputMinimum), static_cast<double>(outputMaximum)});
        outputImage.setBuffer(std::make_shared<ImageBufferHandle>());
        outputImage.setModality(image.modality());
        outputImage.setWindowCenter(
            0.5 * (static_cast<double>(outputMinimum)
                   + static_cast<double>(outputMaximum)));
        outputImage.setWindowWidth(
            static_cast<double>(outputMaximum) - static_cast<double>(outputMinimum));
        outputImage.setRescaleSlope(1.0);
        outputImage.setRescaleIntercept(0.0);

        const std::string profileHash = profileFingerprint(profile);
        const std::string itkVersion = ITK_VERSION;
        const std::string algorithmVersionValue = algorithmVersion();
        const std::string outputHash = outputFingerprint(
            outputImage, outputBytes, sourceFingerprint, profileHash,
            algorithmVersionValue, itkVersion);
        std::shared_ptr<const XQMemoryImageBufferHandle> outputBuffer =
            std::make_shared<XQMemoryImageBufferHandle>(
                ScalarType::Float32, geometry.dimensions, 1,
                std::move(outputBytes));
        if (!outputBuffer->is_valid()) {
            return failure(start, VascularPreprocessStatus::ProcessingFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::OutputBufferInvalid);
        }
        if (cancellationRequested(cancellation)) {
            return failure(start, VascularPreprocessStatus::Cancelled, currentStage,
                           VascularPreprocessDiagnosticCode::Cancelled);
        }

        VascularPreprocessResult result;
        result.status = VascularPreprocessStatus::Ok;
        result.stage = VascularPreprocessStage::Complete;
        result.elapsedMilliseconds = elapsedMilliseconds(start);
        XQVesselnessVolume output;
        output.image = std::move(outputImage);
        output.buffer = std::move(outputBuffer);
        output.profile = profile;
        output.inputFingerprint = std::move(sourceFingerprint);
        output.profileFingerprint = profileHash;
        output.outputFingerprint = outputHash;
        output.algorithmId = algorithmId();
        output.algorithmVersion = algorithmVersionValue;
        output.itkVersion = itkVersion;
        output.inputScalarMinimum = inputMinimum;
        output.inputScalarMaximum = inputMaximum;
        output.scalarMinimum = static_cast<double>(outputMinimum);
        output.scalarMaximum = static_cast<double>(outputMaximum);
        output.positiveVoxelCount = positiveCount;
        output.elapsedMilliseconds = result.elapsedMilliseconds;
        result.output.emplace(std::move(output));
        if (!result.output->isValid()) {
            return failure(start, VascularPreprocessStatus::ProcessingFailed,
                           currentStage,
                           VascularPreprocessDiagnosticCode::OutputBufferInvalid);
        }
        return result;
    } catch (const itk::MemoryAllocationError&) {
        return failure(start, VascularPreprocessStatus::AllocationFailed,
                       currentStage,
                       VascularPreprocessDiagnosticCode::AllocationFailed);
    } catch (const std::bad_alloc&) {
        return failure(start, VascularPreprocessStatus::AllocationFailed,
                       currentStage,
                       VascularPreprocessDiagnosticCode::AllocationFailed);
    } catch (const itk::ExceptionObject&) {
        return failure(start, VascularPreprocessStatus::ProcessingFailed,
                       currentStage,
                       VascularPreprocessDiagnosticCode::ItkException);
    } catch (const std::exception&) {
        return failure(start, VascularPreprocessStatus::ProcessingFailed,
                       currentStage,
                       VascularPreprocessDiagnosticCode::UnexpectedException);
    } catch (...) {
        return failure(start, VascularPreprocessStatus::ProcessingFailed,
                       currentStage,
                       VascularPreprocessDiagnosticCode::UnexpectedException);
    }
}

} // namespace xq
