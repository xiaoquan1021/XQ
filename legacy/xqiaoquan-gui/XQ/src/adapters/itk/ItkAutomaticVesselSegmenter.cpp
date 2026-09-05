#include "adapters/itk/ItkAutomaticVesselSegmenter.h"

#include "adapters/itk/VascularRoiPriorFingerprint.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/IVoxelSource.h"

#include "itkAndImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkBoundedReciprocalImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkFlatStructuringElement.h"
#include "itkGeodesicActiveContourLevelSetImageFilter.h"
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"
#include "itkImage.h"
#include "itkImportImageFilter.h"
#include "itkMacro.h"
#include "itkMaskImageFilter.h"
#include "itkOrImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
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

using FloatImage = itk::Image<float, kDimension>;
using BinaryImage = itk::Image<unsigned char, kDimension>;
using LabelImage = itk::Image<std::uint32_t, kDimension>;
using FloatImporter = itk::ImportImageFilter<float, kDimension>;
using StructuringElement = itk::FlatStructuringElement<kDimension>;
using BinaryDilateFilter =
    itk::BinaryDilateImageFilter<BinaryImage, BinaryImage, StructuringElement>;
using BinaryOrFilter = itk::OrImageFilter<BinaryImage, BinaryImage, BinaryImage>;
using SignedDistanceFilter =
    itk::SignedMaurerDistanceMapImageFilter<BinaryImage, FloatImage>;
using RescaleFilter = itk::RescaleIntensityImageFilter<FloatImage, FloatImage>;
using GradientFilter =
    itk::GradientMagnitudeRecursiveGaussianImageFilter<FloatImage, FloatImage>;
using ReciprocalFilter =
    itk::BoundedReciprocalImageFilter<FloatImage, FloatImage>;
using EdgeMaskFilter =
    itk::MaskImageFilter<FloatImage, BinaryImage, FloatImage>;
using LevelSetFilter =
    itk::GeodesicActiveContourLevelSetImageFilter<FloatImage, FloatImage>;
using LevelSetThresholdFilter =
    itk::BinaryThresholdImageFilter<FloatImage, BinaryImage>;
using BinaryAndFilter =
    itk::AndImageFilter<BinaryImage, BinaryImage, BinaryImage>;
using ConnectedFilter =
    itk::ConnectedComponentImageFilter<BinaryImage, LabelImage>;
using RelabelFilter = itk::RelabelComponentImageFilter<LabelImage, LabelImage>;
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

    void addByte(std::uint8_t value) { addBytes(&value, sizeof(value)); }

    void addUnsigned(std::uint64_t value)
    {
        std::uint8_t bytes[8] = {};
        for (std::size_t index = 0; index < 8; ++index) {
            bytes[index] = static_cast<std::uint8_t>(
                (value >> (index * 8)) & 0xffu);
        }
        addBytes(bytes, sizeof(bytes));
    }

    void addDouble(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t),
                      "Fingerprint encoding requires binary64 storage");
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
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
}

bool cancellationRequested(
    const AutomaticVesselSegmentationCancellation* cancellation)
{
    return cancellation != nullptr && cancellation->isCancellationRequested();
}

void addDiagnostic(AutomaticVesselSegmentationResult* result,
                   DiagnosticSeverity severity,
                   AutomaticVesselSegmentationDiagnosticCode code,
                   AutomaticVesselSegmentationStage stage,
                   std::initializer_list<double> numericContext = {})
{
    if (result == nullptr) {
        return;
    }
    AutomaticVesselSegmentationDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.stage = stage;
    diagnostic.messageKey = automaticVesselSegmentationDiagnosticToken(code);
    diagnostic.numericContext.assign(numericContext.begin(),
                                     numericContext.end());
    result->diagnostics.push_back(std::move(diagnostic));
}

AutomaticVesselSegmentationResult failure(
    Clock::time_point start,
    AutomaticVesselSegmentationStatus status,
    AutomaticVesselSegmentationStage stage,
    AutomaticVesselSegmentationDiagnosticCode code,
    std::initializer_list<double> numericContext = {})
{
    AutomaticVesselSegmentationResult result;
    result.status = status;
    result.stage = stage;
    result.elapsedMilliseconds = elapsedMilliseconds(start);
    result.output.reset();
    addDiagnostic(&result, DiagnosticSeverity::Error, code, stage,
                  numericContext);
    return result;
}

AutomaticVesselSegmentationDiagnosticCode stageFailureCode(
    AutomaticVesselSegmentationStage stage)
{
    switch (stage) {
    case AutomaticVesselSegmentationStage::BuildDomain:
        return AutomaticVesselSegmentationDiagnosticCode::ItkDomainCompositionFailed;
    case AutomaticVesselSegmentationStage::InitializeLevelSet:
        return AutomaticVesselSegmentationDiagnosticCode::ItkInitialSurfaceFailed;
    case AutomaticVesselSegmentationStage::BuildEdgePotential:
        return AutomaticVesselSegmentationDiagnosticCode::ItkEdgePotentialFailed;
    case AutomaticVesselSegmentationStage::EvolveLevelSet:
        return AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetFailed;
    case AutomaticVesselSegmentationStage::ThresholdLevelSet:
        return AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetThresholdFailed;
    case AutomaticVesselSegmentationStage::ConnectedComponents:
        return AutomaticVesselSegmentationDiagnosticCode::ItkConnectedComponentsFailed;
    default:
        return AutomaticVesselSegmentationDiagnosticCode::ItkException;
    }
}

bool checkedVoxelCount(const int dimensions[3], std::size_t* count)
{
    if (count == nullptr) {
        return false;
    }
    std::size_t value = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimensions[axis] <= 0) {
            return false;
        }
        const std::size_t dimension =
            static_cast<std::size_t>(dimensions[axis]);
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

bool nearlyEqual(double left, double right)
{
    const double scale = (std::max)({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= kGeometryTolerance * scale;
}

bool sameGeometry(const ImageGeometry& left, const ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || !nearlyEqual(left.spacing[axis], right.spacing[axis])
            || !nearlyEqual(left.origin[axis], right.origin[axis])) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (!nearlyEqual(left.direction[axis][column],
                             right.direction[axis][column])) {
                return false;
            }
        }
    }
    return true;
}

bool validLayerSourceGeometry(const ImageGeometry& geometry)
{
    std::size_t ignored = 0;
    if (geometry.coordinateSystem != ImageCoordinateSystem::LPS
        || !checkedVoxelCount(geometry.dimensions, &ignored)) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(geometry.spacing[axis])
            || geometry.spacing[axis] <= 0.0
            || !std::isfinite(geometry.origin[axis])) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (!std::isfinite(geometry.direction[axis][column])) {
                return false;
            }
        }
    }
    return std::abs(directionDeterminant(geometry.direction))
        > kDirectionDeterminantEpsilon;
}

template <typename TImage>
bool sameItkGeometry(const TImage* image, const ImageGeometry& expected)
{
    if (image == nullptr) {
        return false;
    }
    const auto region = image->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    const auto index = region.GetIndex();
    const auto spacing = image->GetSpacing();
    const auto origin = image->GetOrigin();
    const auto direction = image->GetDirection();
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        if (index[axis] != 0
            || size[axis]
                != static_cast<typename TImage::SizeType::SizeValueType>(
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
    builder.addUnsigned(geometry.coordinateSystem == ImageCoordinateSystem::LPS
                            ? 1u
                            : 2u);
    builder.addUnsigned(static_cast<std::uint64_t>(view.bytes.size()));
    builder.addBytes(view.bytes.data(), view.bytes.size());
    return builder.finish("xq-vascular-input-v1:sha256:");
}

std::string preprocessProfileFingerprint(
    const VascularPreprocessProfileV1& profile)
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
    builder.addUnsigned(
        profile.sigmaStepMethod == VascularSigmaStepMethod::Logarithmic
            ? 1u
            : 2u);
    builder.addDouble(profile.alpha);
    builder.addDouble(profile.beta);
    builder.addDouble(profile.gamma);
    builder.addByte(profile.brightObject ? 1u : 0u);
    builder.addByte(profile.scaleObjectness ? 1u : 0u);
    return builder.finish("xq-vascular-profile-v1:sha256:");
}

std::string vesselnessOutputFingerprint(const XQVesselnessVolume& vesselness)
{
    const ImageGeometry& geometry = vesselness.image.geometry();
    const std::vector<std::uint8_t>& bytes = vesselness.buffer->bytes();
    FingerprintBuilder builder;
    builder.addString("xq-vesselness-output-v1");
    builder.addString(vesselness.inputFingerprint);
    builder.addString(vesselness.profileFingerprint);
    builder.addString(vesselness.algorithmVersion);
    builder.addString(vesselness.itkVersion);
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

std::string segmentationProfileFingerprint(
    const AutomaticVesselSegmentationProfileV2& profile)
{
    FingerprintBuilder builder;
    builder.addString("xq-automatic-vessel-segmentation-profile-v3-gac");
    builder.addString(profile.profileId);
    builder.addUnsigned(profile.schemaVersion);
    builder.addDouble(profile.coarseDomainDilationMm);
    builder.addDouble(profile.edgeSigmaMm);
    builder.addDouble(profile.propagationScaling);
    builder.addDouble(profile.advectionScaling);
    builder.addDouble(profile.curvatureScaling);
    builder.addDouble(profile.maximumRmsError);
    builder.addUnsigned(profile.maximumIterations);
    builder.addDouble(profile.isosurfaceValue);
    builder.addByte(profile.useImageSpacing ? 1u : 0u);
    return builder.finish("xq-automatic-vessel-profile-v3:sha256:");
}

std::string outputFingerprint(
    const ImageGeometry& geometry,
    const std::vector<std::uint8_t>& labels,
    const std::string& input,
    const std::string& vesselness,
    const std::string& roiPrior,
    const std::string& profile,
    const std::string& algorithmVersion,
    const std::string& itkVersion)
{
    FingerprintBuilder builder;
    builder.addString("xq-automatic-vessel-mask-v3-gac");
    builder.addString(input);
    builder.addString(vesselness);
    builder.addString(roiPrior);
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
    builder.addUnsigned(static_cast<std::uint64_t>(labels.size()));
    builder.addBytes(labels.data(), labels.size());
    return builder.finish("xq-automatic-vessel-mask-v3:sha256:");
}

const float* alignedFloatData(const std::uint8_t* bytes,
                              std::size_t voxelCount,
                              std::vector<float>* owned)
{
    if (bytes == nullptr || owned == nullptr) {
        return nullptr;
    }
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(bytes);
    if (address % alignof(float) == 0) {
        return reinterpret_cast<const float*>(bytes);
    }
    owned->resize(voxelCount);
    std::memcpy(owned->data(), bytes, voxelCount * sizeof(float));
    return owned->data();
}

template <typename TImage>
void setItkGeometry(TImage* image, const ImageGeometry& geometry)
{
    typename TImage::SpacingType spacing;
    typename TImage::PointType origin;
    typename TImage::DirectionType direction;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        spacing[axis] = geometry.spacing[axis];
        origin[axis] = geometry.origin[axis];
        for (unsigned int column = 0; column < kDimension; ++column) {
            direction[axis][column] = geometry.direction[axis][column];
        }
    }
    image->SetSpacing(spacing);
    image->SetOrigin(origin);
    image->SetDirection(direction);
}

FloatImage::Pointer importFloatImage(const ImageGeometry& geometry,
                                     const float* values,
                                     std::size_t voxelCount)
{
    if (values == nullptr) {
        return nullptr;
    }
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

    FloatImporter::Pointer importer = FloatImporter::New();
    importer->SetRegion(region);
    FloatImporter::SpacingType spacing;
    FloatImporter::OriginType origin;
    FloatImporter::DirectionType direction;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        spacing[axis] = geometry.spacing[axis];
        origin[axis] = geometry.origin[axis];
        for (unsigned int column = 0; column < kDimension; ++column) {
            direction[axis][column] = geometry.direction[axis][column];
        }
    }
    importer->SetSpacing(spacing);
    importer->SetOrigin(origin);
    importer->SetDirection(direction);
    importer->SetImportPointer(const_cast<float*>(values), voxelCount, false);
    importer->Update();
    FloatImage::Pointer output = importer->GetOutput();
    output->DisconnectPipeline();
    return output;
}

BinaryImage::Pointer importBinaryImage(
    const ImageGeometry& geometry,
    const std::vector<XQSegmentationMask::LabelType>& values,
    std::size_t voxelCount)
{
    if (values.size() != voxelCount) {
        return nullptr;
    }
    BinaryImage::SizeType size;
    BinaryImage::IndexType start;
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        size[axis] = static_cast<BinaryImage::SizeType::SizeValueType>(
            geometry.dimensions[axis]);
        start[axis] = 0;
    }
    BinaryImage::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    BinaryImage::Pointer image = BinaryImage::New();
    image->SetRegions(region);
    setItkGeometry(image.GetPointer(), geometry);
    image->Allocate();
    static_assert(sizeof(XQSegmentationMask::LabelType)
                      == sizeof(unsigned char),
                  "ROI mask labels must be byte-sized");
    std::memcpy(image->GetBufferPointer(), values.data(), voxelCount);
    return image;
}

bool validateBinaryLayer(const XQVascularRoiLayer& layer,
                         const ImageGeometry& referenceGeometry,
                         std::size_t referenceVoxelCount,
                         std::size_t* foregroundCount)
{
    if (foregroundCount == nullptr || !layer.isValid() || layer.mask == nullptr
        || !validLayerSourceGeometry(layer.sourceGeometry)
        || !sameGeometry(layer.mask->geometry(), referenceGeometry)
        || layer.mask->voxelCount() != referenceVoxelCount) {
        return false;
    }
    std::size_t count = 0;
    for (XQSegmentationMask::LabelType value : layer.mask->voxels()) {
        if (value > 1) {
            return false;
        }
        if (value != 0) {
            ++count;
        }
    }
    std::size_t sourceVoxelCount = 0;
    if (!checkedVoxelCount(layer.sourceGeometry.dimensions, &sourceVoxelCount)
        || layer.sourceForegroundVoxelCount >= sourceVoxelCount
        || count == 0 || count >= referenceVoxelCount
        || count != layer.alignedForegroundVoxelCount) {
        return false;
    }
    *foregroundCount = count;
    return true;
}

std::size_t countForeground(const BinaryImage* image, std::size_t voxelCount)
{
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        return 0;
    }
    const unsigned char* values = image->GetBufferPointer();
    std::size_t count = 0;
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (values[index] != 0) {
            ++count;
        }
    }
    return count;
}

bool validInitialSurface(const FloatImage* image,
                         std::size_t voxelCount,
                         double isosurfaceValue)
{
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        return false;
    }
    double minimum = (std::numeric_limits<double>::max)();
    double maximum = (std::numeric_limits<double>::lowest)();
    const float* values = image->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        const double value = static_cast<double>(values[index]);
        if (!std::isfinite(value)) {
            return false;
        }
        minimum = (std::min)(minimum, value);
        maximum = (std::max)(maximum, value);
    }
    return minimum < isosurfaceValue && maximum > isosurfaceValue;
}

bool scanEdgePotential(const FloatImage* edgePotential,
                       const BinaryImage* domain,
                       std::size_t voxelCount,
                       std::size_t* positiveCount,
                       double* minimum,
                       double* maximum)
{
    if (edgePotential == nullptr || domain == nullptr
        || positiveCount == nullptr || minimum == nullptr || maximum == nullptr
        || edgePotential->GetBufferPointer() == nullptr
        || domain->GetBufferPointer() == nullptr) {
        return false;
    }
    const float* edgeValues = edgePotential->GetBufferPointer();
    const unsigned char* domainValues = domain->GetBufferPointer();
    std::size_t count = 0;
    double minValue = (std::numeric_limits<double>::max)();
    double maxValue = (std::numeric_limits<double>::lowest)();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        const double value = static_cast<double>(edgeValues[index]);
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            return false;
        }
        if (domainValues[index] == 0) {
            if (value != 0.0) {
                return false;
            }
            continue;
        }
        if (value <= 0.0) {
            return false;
        }
        ++count;
        minValue = (std::min)(minValue, value);
        maxValue = (std::max)(maxValue, value);
    }
    if (count == 0) {
        return false;
    }
    *positiveCount = count;
    *minimum = minValue;
    *maximum = maxValue;
    return true;
}

bool allFinite(const FloatImage* image, std::size_t voxelCount)
{
    if (image == nullptr || image->GetBufferPointer() == nullptr) {
        return false;
    }
    const float* values = image->GetBufferPointer();
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (!std::isfinite(static_cast<double>(values[index]))) {
            return false;
        }
    }
    return true;
}

bool makePhysicalRadius(const ImageGeometry& geometry,
                        double radiusMm,
                        StructuringElement::RadiusType* radius,
                        unsigned int recorded[3])
{
    if (radius == nullptr || recorded == nullptr) {
        return false;
    }
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        const double voxelRadius =
            std::ceil(radiusMm / geometry.spacing[axis]);
        if (!std::isfinite(voxelRadius) || voxelRadius < 1.0
            || voxelRadius
                > static_cast<double>(
                    (std::numeric_limits<unsigned int>::max)())) {
            return false;
        }
        const unsigned int value = static_cast<unsigned int>(voxelRadius);
        (*radius)[axis] =
            static_cast<StructuringElement::RadiusType::SizeValueType>(value);
        recorded[axis] = value;
    }
    return true;
}

AutomaticVesselFilterParameter filterParameter(const char* name,
                                                double value,
                                                const char* unit)
{
    AutomaticVesselFilterParameter parameter;
    parameter.name = name;
    parameter.value = value;
    parameter.unit = unit;
    return parameter;
}

void appendUpstreamStage(
    std::vector<AutomaticVesselUpstreamStage>* stages,
    const char* stageId,
    const char* implementation,
    const std::string& implementationVersion,
    std::initializer_list<AutomaticVesselFilterParameter> parameters = {})
{
    AutomaticVesselUpstreamStage stage;
    stage.stageId = stageId;
    stage.implementation = implementation;
    stage.implementationVersion = implementationVersion;
    stage.parameters.assign(parameters.begin(), parameters.end());
    stages->push_back(std::move(stage));
}

} // namespace

const char* ItkAutomaticVesselSegmenter::algorithmId()
{
    return "xq.itk.geodesic-active-contour-vessel-segmentation";
}

const char* ItkAutomaticVesselSegmenter::algorithmVersion()
{
    return "3.0.0";
}

AutomaticVesselSegmentationResult ItkAutomaticVesselSegmenter::run(
    const XQImageVolume& image,
    const IVoxelSource& source,
    const XQVesselnessVolume& vesselness,
    const XQVascularRoiPriorV1& roiPrior,
    const AutomaticVesselSegmentationProfileV2& profile,
    const AutomaticVesselSegmentationCancellation* cancellation) const
{
    const Clock::time_point start = Clock::now();
    AutomaticVesselSegmentationStage currentStage =
        AutomaticVesselSegmentationStage::ValidateInput;
    try {
        if (profile.schemaVersion != 3
            || validateAutomaticVesselSegmentationProfileV2(profile)
                != AutomaticVesselSegmentationProfileV2ValidationCode::Ok) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidProfile,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::InvalidProfile);
        }
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }
        if (!image.hasGeometry()) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidGeometry,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::MissingGeometry);
        }
        const ImageGeometry& geometry = image.geometry();
        if (geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
            return failure(
                start, AutomaticVesselSegmentationStatus::InvalidGeometry,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::UnsupportedCoordinateSystem);
        }
        std::size_t voxelCount = 0;
        if (!checkedVoxelCount(geometry.dimensions, &voxelCount)) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidGeometry,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::InvalidDimensions);
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(geometry.spacing[axis])
                || !std::isfinite(geometry.origin[axis])) {
                return failure(
                    start, AutomaticVesselSegmentationStatus::InvalidGeometry,
                    currentStage,
                    AutomaticVesselSegmentationDiagnosticCode::NonFiniteGeometry);
            }
            if (geometry.spacing[axis] <= 0.0) {
                return failure(
                    start, AutomaticVesselSegmentationStatus::InvalidGeometry,
                    currentStage,
                    AutomaticVesselSegmentationDiagnosticCode::NonPositiveSpacing);
            }
            for (int column = 0; column < 3; ++column) {
                if (!std::isfinite(geometry.direction[axis][column])) {
                    return failure(
                        start,
                        AutomaticVesselSegmentationStatus::InvalidGeometry,
                        currentStage,
                        AutomaticVesselSegmentationDiagnosticCode::NonFiniteGeometry);
                }
            }
        }
        if (std::abs(directionDeterminant(geometry.direction))
            <= kDirectionDeterminantEpsilon) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidGeometry,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SingularDirection);
        }
        if (image.componentCount() != 1) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::UnsupportedInput,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::NotSingleComponent);
        }
        if (image.scalarType() != ScalarType::Float32) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::UnsupportedInput,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::UnsupportedScalarType);
        }
        if (image.hasDicomIdentity()
            && (image.dicomIdentity().studyInstanceUid.empty()
                || image.dicomIdentity().seriesInstanceUid.empty())) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidArgument,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::InvalidArgument);
        }
        if (!vesselness.isValid()) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidVesselness,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::VesselnessInvalid);
        }
        if (!sameGeometry(geometry, vesselness.image.geometry())) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::LineageMismatch,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::GeometryMismatch);
        }
        if (!roiPrior.isValid()) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidRoiPrior,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::InvalidRoiPrior);
        }
        const XQVascularRoiLayer* organ =
            roiPrior.layer(VascularRoiRole::Organ);
        const XQVascularRoiLayer* coarse =
            roiPrior.layer(VascularRoiRole::CoarseVessel);
        if (organ == nullptr || coarse == nullptr) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidRoiPrior,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::MissingRoiRole);
        }
        std::size_t organForeground = 0;
        std::size_t coarseForeground = 0;
        if (!validateBinaryLayer(*organ, geometry, voxelCount,
                                 &organForeground)
            || !validateBinaryLayer(*coarse, geometry, voxelCount,
                                    &coarseForeground)) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidRoiPrior,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::RoiGeometryMismatch);
        }

        const VoxelMeta meta = source.meta();
        if (!meta.valid) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidSource,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SourceMetadataInvalid);
        }
        if (meta.type != ScalarType::Float32 || meta.components != 1
            || meta.voxelCount != voxelCount
            || meta.dims[0] != geometry.dimensions[0]
            || meta.dims[1] != geometry.dimensions[1]
            || meta.dims[2] != geometry.dimensions[2]) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidSource,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SourceMetadataMismatch);
        }

        currentStage = AutomaticVesselSegmentationStage::AcquireInput;
        VoxelLease whole = source.acquire_whole();
        const VoxelView& view = whole.view();
        if (!view.valid || view.bytes.data() == nullptr) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidSource,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SourceAcquireFailed);
        }
        if (view.type != ScalarType::Float32 || view.components != 1
            || view.voxelCount() != voxelCount
            || view.dims[0] != geometry.dimensions[0]
            || view.dims[1] != geometry.dimensions[1]
            || view.dims[2] != geometry.dimensions[2]) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidSource,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SourceMetadataMismatch);
        }
        if (voxelCount > (std::numeric_limits<std::size_t>::max)()
                / sizeof(float)
            || view.bytes.size() != voxelCount * sizeof(float)) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidSource,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::SourceByteSizeMismatch);
        }
        if (vesselness.buffer == nullptr || !vesselness.buffer->is_valid()
            || vesselness.buffer->scalarType() != ScalarType::Float32
            || vesselness.buffer->componentCount() != 1
            || vesselness.buffer->voxelCount() != voxelCount
            || vesselness.buffer->bytes().size()
                != voxelCount * sizeof(float)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::InvalidVesselness,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::VesselnessBufferInvalid);
        }
        if (preprocessProfileFingerprint(vesselness.profile)
                != vesselness.profileFingerprint
            || vesselnessOutputFingerprint(vesselness)
                != vesselness.outputFingerprint) {
            return failure(
                start, AutomaticVesselSegmentationStatus::LineageMismatch,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::VesselnessFingerprintMismatch);
        }
        const std::string sourceHash = inputFingerprint(image, view);
        if (sourceHash != vesselness.inputFingerprint
            || sourceHash != roiPrior.ctInputFingerprint) {
            return failure(
                start, AutomaticVesselSegmentationStatus::LineageMismatch,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::InputFingerprintMismatch);
        }
        if (!itk_detail::vascularRoiPriorFingerprintsMatch(roiPrior)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::LineageMismatch,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::RoiFingerprintMismatch);
        }
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        std::vector<float> ownedInput;
        std::vector<float> ownedVesselness;
        const float* inputValues = alignedFloatData(
            view.bytes.data(), voxelCount, &ownedInput);
        const float* vesselnessValues = alignedFloatData(
            vesselness.buffer->bytes().data(), voxelCount, &ownedVesselness);
        if (inputValues == nullptr || vesselnessValues == nullptr) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::AllocationFailed,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::AllocationFailed);
        }
        for (std::size_t index = 0; index < voxelCount; ++index) {
            if (!std::isfinite(static_cast<double>(inputValues[index]))) {
                return failure(
                    start, AutomaticVesselSegmentationStatus::InvalidSource,
                    currentStage,
                    AutomaticVesselSegmentationDiagnosticCode::InputScalarNonFinite);
            }
            if (!std::isfinite(static_cast<double>(vesselnessValues[index]))) {
                return failure(
                    start,
                    AutomaticVesselSegmentationStatus::InvalidVesselness,
                    currentStage,
                    AutomaticVesselSegmentationDiagnosticCode::VesselnessScalarNonFinite);
            }
            if ((index & 0xffffu) == 0
                && cancellationRequested(cancellation)) {
                return failure(start,
                               AutomaticVesselSegmentationStatus::Cancelled,
                               currentStage,
                               AutomaticVesselSegmentationDiagnosticCode::Cancelled);
            }
        }
        std::vector<float>().swap(ownedInput);

        currentStage = AutomaticVesselSegmentationStage::ImportImages;
        FloatImage::Pointer vesselnessImage = importFloatImage(
            geometry, vesselnessValues, voxelCount);
        BinaryImage::Pointer organImage = importBinaryImage(
            geometry, organ->mask->voxels(), voxelCount);
        BinaryImage::Pointer coarseImage = importBinaryImage(
            geometry, coarse->mask->voxels(), voxelCount);
        if (vesselnessImage == nullptr || organImage == nullptr
            || coarseImage == nullptr
            || !sameItkGeometry(vesselnessImage.GetPointer(), geometry)
            || !sameItkGeometry(organImage.GetPointer(), geometry)
            || !sameItkGeometry(coarseImage.GetPointer(), geometry)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::OutputGeometryMismatch);
        }

        currentStage = AutomaticVesselSegmentationStage::BuildDomain;
        StructuringElement::RadiusType dilationRadius;
        unsigned int recordedDilationRadius[3] = {0, 0, 0};
        if (!makePhysicalRadius(geometry, profile.coarseDomainDilationMm,
                                &dilationRadius, recordedDilationRadius)) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::InvalidProfile,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::InvalidProfile);
        }
        const StructuringElement dilationKernel =
            StructuringElement::Ball(dilationRadius, true);
        BinaryDilateFilter::Pointer dilateCoarse = BinaryDilateFilter::New();
        dilateCoarse->SetInput(coarseImage);
        dilateCoarse->SetKernel(dilationKernel);
        dilateCoarse->SetDilateValue(1);
        dilateCoarse->Update();
        BinaryImage::Pointer dilatedCoarse = dilateCoarse->GetOutput();
        dilatedCoarse->DisconnectPipeline();

        BinaryOrFilter::Pointer domainUnion = BinaryOrFilter::New();
        domainUnion->SetInput1(organImage);
        domainUnion->SetInput2(dilatedCoarse);
        domainUnion->Update();
        BinaryImage::Pointer domainMask = domainUnion->GetOutput();
        domainMask->DisconnectPipeline();
        if (!sameItkGeometry(domainMask.GetPointer(), geometry)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::ItkDomainCompositionFailed);
        }
        const std::size_t domainVoxelCount =
            countForeground(domainMask.GetPointer(), voxelCount);
        if (domainVoxelCount == 0) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::EmptyDomain,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::EmptyRoiDomain);
        }
        dilateCoarse = nullptr;
        dilatedCoarse = nullptr;
        domainUnion = nullptr;
        organImage = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::InitializeLevelSet;
        SignedDistanceFilter::Pointer signedDistance =
            SignedDistanceFilter::New();
        signedDistance->SetInput(coarseImage);
        signedDistance->SetBackgroundValue(0);
        signedDistance->SetInsideIsPositive(false);
        signedDistance->SetSquaredDistance(false);
        signedDistance->SetUseImageSpacing(profile.useImageSpacing);
        signedDistance->Update();
        FloatImage::Pointer initialLevelSet = signedDistance->GetOutput();
        initialLevelSet->DisconnectPipeline();
        if (!sameItkGeometry(initialLevelSet.GetPointer(), geometry)
            || !validInitialSurface(initialLevelSet.GetPointer(), voxelCount,
                                    profile.isosurfaceValue)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::EmptyInitialSurface,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::EmptyInitialSurface);
        }
        signedDistance = nullptr;
        coarseImage = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::BuildEdgePotential;
        RescaleFilter::Pointer rescale = RescaleFilter::New();
        rescale->SetInput(vesselnessImage);
        rescale->SetOutputMinimum(0.0f);
        rescale->SetOutputMaximum(1.0f);

        GradientFilter::Pointer gradient = GradientFilter::New();
        gradient->SetInput(rescale->GetOutput());
        gradient->SetSigma(profile.edgeSigmaMm);
        gradient->SetNormalizeAcrossScale(false);

        ReciprocalFilter::Pointer reciprocal = ReciprocalFilter::New();
        reciprocal->SetInput(gradient->GetOutput());

        EdgeMaskFilter::Pointer maskEdge = EdgeMaskFilter::New();
        maskEdge->SetInput(reciprocal->GetOutput());
        maskEdge->SetMaskImage(domainMask);
        maskEdge->SetMaskingValue(0);
        maskEdge->SetOutsideValue(0.0f);
        maskEdge->Update();
        FloatImage::Pointer edgePotential = maskEdge->GetOutput();
        edgePotential->DisconnectPipeline();
        std::size_t edgePotentialPositiveVoxelCount = 0;
        double edgePotentialMinimum = 0.0;
        double edgePotentialMaximum = 0.0;
        if (!sameItkGeometry(edgePotential.GetPointer(), geometry)
            || !scanEdgePotential(
                edgePotential.GetPointer(), domainMask.GetPointer(), voxelCount,
                &edgePotentialPositiveVoxelCount, &edgePotentialMinimum,
                &edgePotentialMaximum)
            || edgePotentialPositiveVoxelCount != domainVoxelCount) {
            return failure(
                start, AutomaticVesselSegmentationStatus::EmptyEdgePotential,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::EmptyEdgePotential);
        }
        maskEdge = nullptr;
        reciprocal = nullptr;
        gradient = nullptr;
        rescale = nullptr;
        vesselnessImage = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::EvolveLevelSet;
        LevelSetFilter::Pointer levelSet = LevelSetFilter::New();
        levelSet->SetInput(initialLevelSet);
        levelSet->SetFeatureImage(edgePotential);
        levelSet->SetPropagationScaling(profile.propagationScaling);
        levelSet->SetAdvectionScaling(profile.advectionScaling);
        levelSet->SetCurvatureScaling(profile.curvatureScaling);
        levelSet->SetMaximumRMSError(profile.maximumRmsError);
        levelSet->SetNumberOfIterations(profile.maximumIterations);
        levelSet->SetIsoSurfaceValue(
            static_cast<float>(profile.isosurfaceValue));
        levelSet->SetUseImageSpacing(profile.useImageSpacing);
        levelSet->Update();
        FloatImage::Pointer evolvedLevelSet = levelSet->GetOutput();
        evolvedLevelSet->DisconnectPipeline();
        const std::uint32_t levelSetElapsedIterations =
            static_cast<std::uint32_t>(levelSet->GetElapsedIterations());
        const double levelSetRmsChange = levelSet->GetRMSChange();
        const bool levelSetConverged =
            levelSetRmsChange <= profile.maximumRmsError;
        if (!sameItkGeometry(evolvedLevelSet.GetPointer(), geometry)
            || !allFinite(evolvedLevelSet.GetPointer(), voxelCount)
            || levelSetElapsedIterations == 0
            || levelSetElapsedIterations > profile.maximumIterations
            || !std::isfinite(levelSetRmsChange)
            || levelSetRmsChange < 0.0) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetFailed);
        }
        levelSet = nullptr;
        initialLevelSet = nullptr;
        edgePotential = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::ThresholdLevelSet;
        LevelSetThresholdFilter::Pointer threshold =
            LevelSetThresholdFilter::New();
        threshold->SetInput(evolvedLevelSet);
        threshold->SetLowerThreshold(
            (std::numeric_limits<float>::lowest)());
        threshold->SetUpperThreshold(
            static_cast<float>(profile.isosurfaceValue));
        threshold->SetInsideValue(1);
        threshold->SetOutsideValue(0);

        BinaryAndFilter::Pointer enforceDomain = BinaryAndFilter::New();
        enforceDomain->SetInput1(threshold->GetOutput());
        enforceDomain->SetInput2(domainMask);
        enforceDomain->Update();
        BinaryImage::Pointer finalMask = enforceDomain->GetOutput();
        finalMask->DisconnectPipeline();
        if (!sameItkGeometry(finalMask.GetPointer(), geometry)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::ItkLevelSetThresholdFailed);
        }
        const std::size_t foregroundVoxelCount =
            countForeground(finalMask.GetPointer(), voxelCount);
        if (foregroundVoxelCount == 0) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::EmptyOutput,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::EmptyOutput);
        }
        if (foregroundVoxelCount == voxelCount) {
            return failure(start,
                           AutomaticVesselSegmentationStatus::ProcessingFailed,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::AllForegroundOutput);
        }
        enforceDomain = nullptr;
        threshold = nullptr;
        evolvedLevelSet = nullptr;
        domainMask = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::ConnectedComponents;
        ConnectedFilter::Pointer connected = ConnectedFilter::New();
        connected->SetInput(finalMask);
        connected->SetFullyConnected(true);
        connected->Update();
        LabelImage::Pointer connectedLabels = connected->GetOutput();
        connectedLabels->DisconnectPipeline();
        connected = nullptr;

        RelabelFilter::Pointer relabel = RelabelFilter::New();
        relabel->SetInput(connectedLabels);
        relabel->SetMinimumObjectSize(0);
        relabel->SetSortByObjectSize(true);
        relabel->Update();
        LabelImage::Pointer relabeled = relabel->GetOutput();
        relabeled->DisconnectPipeline();
        if (!sameItkGeometry(relabeled.GetPointer(), geometry)) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::ItkConnectedComponentsFailed);
        }
        std::vector<std::size_t> componentVoxelCounts;
        componentVoxelCounts.reserve(relabel->GetNumberOfObjects());
        std::size_t componentTotal = 0;
        for (const auto size : relabel->GetSizeOfObjectsInPixels()) {
            const std::size_t componentSize = static_cast<std::size_t>(size);
            componentVoxelCounts.push_back(componentSize);
            componentTotal += componentSize;
        }
        if (componentVoxelCounts.empty()
            || componentTotal != foregroundVoxelCount) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::ItkConnectedComponentsFailed);
        }
        relabel = nullptr;
        relabeled = nullptr;
        connectedLabels = nullptr;
        if (cancellationRequested(cancellation)) {
            return failure(start, AutomaticVesselSegmentationStatus::Cancelled,
                           currentStage,
                           AutomaticVesselSegmentationDiagnosticCode::Cancelled);
        }

        currentStage = AutomaticVesselSegmentationStage::MaterializeOutput;
        const unsigned char* finalValues = finalMask->GetBufferPointer();
        if (finalValues == nullptr) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::OutputMaskInvalid);
        }
        std::vector<std::uint8_t> outputLabels(voxelCount, 0);
        std::shared_ptr<XQSegmentationMask> outputMask =
            std::make_shared<XQSegmentationMask>(geometry.dimensions);
        if (!outputMask->is_valid()) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::OutputMaskInvalid);
        }
        outputMask->setGeometry(geometry);
        outputMask->setLabels({SegmentationLabel{1, "vessel"}});
        for (std::size_t index = 0; index < voxelCount; ++index) {
            if (finalValues[index] != 0) {
                outputLabels[index] = 1;
                outputMask->setLabelAt(index, 1);
            }
            if ((index & 0xffffu) == 0
                && cancellationRequested(cancellation)) {
                return failure(start,
                               AutomaticVesselSegmentationStatus::Cancelled,
                               currentStage,
                               AutomaticVesselSegmentationDiagnosticCode::Cancelled);
            }
        }
        if (outputMask->foregroundVoxelCount() != foregroundVoxelCount) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::OutputMaskInvalid);
        }
        finalMask = nullptr;

        const std::string profileHash =
            segmentationProfileFingerprint(profile);
        const std::string algorithmVersionValue = algorithmVersion();
        const std::string itkVersionValue = ITK_VERSION;
        const std::string maskHash = outputFingerprint(
            geometry, outputLabels, sourceHash, vesselness.outputFingerprint,
            roiPrior.priorFingerprint, profileHash, algorithmVersionValue,
            itkVersionValue);

        std::vector<AutomaticVesselUpstreamStage> upstreamStages;
        appendUpstreamStage(
            &upstreamStages, "roi.coarse_physical_dilation",
            "itk::BinaryDilateImageFilter", itkVersionValue,
            {filterParameter("radius", profile.coarseDomainDilationMm, "mm"),
             filterParameter("radius_x", recordedDilationRadius[0], "voxels"),
             filterParameter("radius_y", recordedDilationRadius[1], "voxels"),
             filterParameter("radius_z", recordedDilationRadius[2], "voxels"),
             filterParameter("parametric_ball", 1.0, "boolean")});
        appendUpstreamStage(&upstreamStages, "roi.allowed_domain",
                            "itk::OrImageFilter", itkVersionValue);
        appendUpstreamStage(
            &upstreamStages, "initialization.signed_distance",
            "itk::SignedMaurerDistanceMapImageFilter", itkVersionValue,
            {filterParameter("inside_is_positive", 0.0, "boolean"),
             filterParameter("squared_distance", 0.0, "boolean"),
             filterParameter("use_image_spacing",
                             profile.useImageSpacing ? 1.0 : 0.0, "boolean"),
             filterParameter("isosurface_value", profile.isosurfaceValue,
                             "level_set_value")});
        appendUpstreamStage(
            &upstreamStages, "edge.vesselness_rescale",
            "itk::RescaleIntensityImageFilter", itkVersionValue,
            {filterParameter("output_minimum", 0.0, "unitless"),
             filterParameter("output_maximum", 1.0, "unitless")});
        appendUpstreamStage(
            &upstreamStages, "edge.gradient_magnitude",
            "itk::GradientMagnitudeRecursiveGaussianImageFilter",
            itkVersionValue,
            {filterParameter("sigma", profile.edgeSigmaMm, "mm"),
             filterParameter("normalize_across_scale", 0.0, "boolean")});
        appendUpstreamStage(
            &upstreamStages, "edge.bounded_reciprocal",
            "itk::BoundedReciprocalImageFilter", itkVersionValue);
        appendUpstreamStage(&upstreamStages, "edge.allowed_domain_mask",
                            "itk::MaskImageFilter", itkVersionValue);
        appendUpstreamStage(
            &upstreamStages, "evolution.geodesic_active_contour",
            "itk::GeodesicActiveContourLevelSetImageFilter",
            itkVersionValue,
            {filterParameter("propagation_scaling",
                             profile.propagationScaling, "unitless"),
             filterParameter("advection_scaling", profile.advectionScaling,
                             "unitless"),
             filterParameter("curvature_scaling", profile.curvatureScaling,
                             "unitless"),
             filterParameter("maximum_rms_error", profile.maximumRmsError,
                             "rms_change"),
             filterParameter("maximum_iterations", profile.maximumIterations,
                             "iterations"),
             filterParameter("isosurface_value", profile.isosurfaceValue,
                             "level_set_value"),
             filterParameter("use_image_spacing",
                             profile.useImageSpacing ? 1.0 : 0.0, "boolean"),
             filterParameter("elapsed_iterations",
                             levelSetElapsedIterations, "iterations"),
             filterParameter("rms_change", levelSetRmsChange, "rms_change"),
             filterParameter("converged", levelSetConverged ? 1.0 : 0.0,
                             "boolean")});
        appendUpstreamStage(
            &upstreamStages, "output.zero_level_threshold",
            "itk::BinaryThresholdImageFilter", itkVersionValue,
            {filterParameter("upper_threshold", profile.isosurfaceValue,
                             "level_set_value")});
        appendUpstreamStage(&upstreamStages, "output.domain_intersection",
                            "itk::AndImageFilter", itkVersionValue);
        appendUpstreamStage(
            &upstreamStages, "reporting.connected_components",
            "itk::ConnectedComponentImageFilter", itkVersionValue,
            {filterParameter("fully_connected", 1.0, "boolean")});
        appendUpstreamStage(
            &upstreamStages, "reporting.relabel_components",
            "itk::RelabelComponentImageFilter", itkVersionValue,
            {filterParameter("sort_by_object_size", 1.0, "boolean"),
             filterParameter("minimum_object_size", 0.0, "voxels")});

        AutomaticVesselSegmentationResult result;
        result.status = AutomaticVesselSegmentationStatus::Ok;
        result.stage = AutomaticVesselSegmentationStage::Complete;
        result.elapsedMilliseconds = elapsedMilliseconds(start);

        XQAutomaticVesselSegmentationV2 output;
        output.mask = std::move(outputMask);
        output.roiPrior = roiPrior;
        output.profile = profile;
        output.upstreamStages = std::move(upstreamStages);
        output.componentVoxelCounts = std::move(componentVoxelCounts);
        output.inputFingerprint = sourceHash;
        output.vesselnessFingerprint = vesselness.outputFingerprint;
        output.roiPriorFingerprint = roiPrior.priorFingerprint;
        output.profileFingerprint = profileHash;
        output.outputFingerprint = maskHash;
        output.algorithmId = algorithmId();
        output.algorithmVersion = algorithmVersionValue;
        output.itkVersion = itkVersionValue;
        output.hasDicomIdentity = image.hasDicomIdentity();
        if (output.hasDicomIdentity) {
            output.dicomIdentity = image.dicomIdentity();
        }
        output.domainVoxelCount = domainVoxelCount;
        output.initialSurfaceVoxelCount = coarseForeground;
        output.edgePotentialPositiveVoxelCount =
            edgePotentialPositiveVoxelCount;
        output.edgePotentialMinimum = edgePotentialMinimum;
        output.edgePotentialMaximum = edgePotentialMaximum;
        output.levelSetElapsedIterations = levelSetElapsedIterations;
        output.levelSetRmsChange = levelSetRmsChange;
        output.levelSetConverged = levelSetConverged;
        for (int axis = 0; axis < 3; ++axis) {
            output.coarseDomainDilationRadiusVoxels[axis] =
                recordedDilationRadius[axis];
        }
        output.foregroundVoxelCount = foregroundVoxelCount;
        output.componentCount = output.componentVoxelCounts.size();
        output.elapsedMilliseconds = result.elapsedMilliseconds;
        result.output.emplace(std::move(output));
        if (!result.output->isValid()) {
            return failure(
                start, AutomaticVesselSegmentationStatus::ProcessingFailed,
                currentStage,
                AutomaticVesselSegmentationDiagnosticCode::OutputMaskInvalid);
        }
        return result;
    } catch (const itk::MemoryAllocationError&) {
        return failure(start,
                       AutomaticVesselSegmentationStatus::AllocationFailed,
                       currentStage,
                       AutomaticVesselSegmentationDiagnosticCode::AllocationFailed);
    } catch (const std::bad_alloc&) {
        return failure(start,
                       AutomaticVesselSegmentationStatus::AllocationFailed,
                       currentStage,
                       AutomaticVesselSegmentationDiagnosticCode::AllocationFailed);
    } catch (const itk::ExceptionObject&) {
        return failure(start,
                       AutomaticVesselSegmentationStatus::ProcessingFailed,
                       currentStage, stageFailureCode(currentStage));
    } catch (const std::exception&) {
        return failure(start,
                       AutomaticVesselSegmentationStatus::ProcessingFailed,
                       currentStage, stageFailureCode(currentStage));
    } catch (...) {
        return failure(start,
                       AutomaticVesselSegmentationStatus::ProcessingFailed,
                       currentStage, stageFailureCode(currentStage));
    }
}

} // namespace xq
