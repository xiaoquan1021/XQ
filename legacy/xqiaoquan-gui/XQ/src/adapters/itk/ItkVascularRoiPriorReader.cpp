#include "adapters/itk/ItkVascularRoiPriorReader.h"
#include "adapters/itk/VascularRoiPriorFingerprint.h"

#include "itkIdentityTransform.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkNiftiImageIO.h"
#include "itkResampleImageFilter.h"

#include "picosha2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>

namespace xq {

namespace {

constexpr unsigned int kDimension = 3;
using BinaryImage = itk::Image<unsigned char, kDimension>;

Diagnostic error(const char* code)
{
    return Diagnostic(DiagnosticSeverity::Error, code, code);
}

bool finiteGeometry(const ImageGeometry& geometry)
{
    if (geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
        return false;
    }
    double determinant = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0
            || !std::isfinite(geometry.spacing[axis])
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
    determinant =
        geometry.direction[0][0]
            * (geometry.direction[1][1] * geometry.direction[2][2]
               - geometry.direction[1][2] * geometry.direction[2][1])
        - geometry.direction[0][1]
            * (geometry.direction[1][0] * geometry.direction[2][2]
               - geometry.direction[1][2] * geometry.direction[2][0])
        + geometry.direction[0][2]
            * (geometry.direction[1][0] * geometry.direction[2][1]
               - geometry.direction[1][1] * geometry.direction[2][0]);
    return std::isfinite(determinant) && std::abs(determinant) > 1e-12;
}

ImageGeometry geometryFromImage(const BinaryImage* image)
{
    ImageGeometry geometry{};
    const BinaryImage::SizeType size =
        image->GetLargestPossibleRegion().GetSize();
    const BinaryImage::SpacingType spacing = image->GetSpacing();
    const BinaryImage::PointType origin = image->GetOrigin();
    const BinaryImage::DirectionType direction = image->GetDirection();
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        geometry.dimensions[axis] = static_cast<int>(size[axis]);
        geometry.spacing[axis] = spacing[axis];
        geometry.origin[axis] = origin[axis];
        for (unsigned int column = 0; column < kDimension; ++column) {
            geometry.direction[axis][column] = direction[axis][column];
        }
    }
    geometry.coordinateSystem = ImageCoordinateSystem::LPS;
    return geometry;
}

bool nearlyEqual(double left, double right)
{
    const double scale = std::max(1.0, std::max(std::abs(left), std::abs(right)));
    return std::abs(left - right) <= 1e-6 * scale;
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

std::array<double, 3> worldPoint(const ImageGeometry& geometry,
                                 const std::array<double, 3>& index)
{
    std::array<double, 3> point{};
    for (int row = 0; row < 3; ++row) {
        point[row] = geometry.origin[row];
        for (int column = 0; column < 3; ++column) {
            point[row] += geometry.direction[row][column]
                * geometry.spacing[column] * index[column];
        }
    }
    return point;
}

void physicalBounds(const ImageGeometry& geometry,
                    std::array<double, 3>* minimum,
                    std::array<double, 3>* maximum)
{
    minimum->fill((std::numeric_limits<double>::max)());
    maximum->fill((std::numeric_limits<double>::lowest)());
    for (int corner = 0; corner < 8; ++corner) {
        std::array<double, 3> index{};
        for (int axis = 0; axis < 3; ++axis) {
            index[axis] = (corner & (1 << axis)) != 0
                ? static_cast<double>(geometry.dimensions[axis] - 1)
                : 0.0;
        }
        const std::array<double, 3> point = worldPoint(geometry, index);
        for (int axis = 0; axis < 3; ++axis) {
            (*minimum)[axis] = std::min((*minimum)[axis], point[axis]);
            (*maximum)[axis] = std::max((*maximum)[axis], point[axis]);
        }
    }
}

bool physicalOverlap(const ImageGeometry& left, const ImageGeometry& right)
{
    std::array<double, 3> leftMinimum{};
    std::array<double, 3> leftMaximum{};
    std::array<double, 3> rightMinimum{};
    std::array<double, 3> rightMaximum{};
    physicalBounds(left, &leftMinimum, &leftMaximum);
    physicalBounds(right, &rightMinimum, &rightMaximum);
    for (int axis = 0; axis < 3; ++axis) {
        if (std::min(leftMaximum[axis], rightMaximum[axis])
            <= std::max(leftMinimum[axis], rightMinimum[axis])) {
            return false;
        }
    }
    return true;
}

bool hashFile(const std::string& path, std::string* digest)
{
    if (digest == nullptr) {
        return false;
    }
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        return false;
    }
    picosha2::hash256_one_by_one hasher;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            const unsigned char* first =
                reinterpret_cast<const unsigned char*>(buffer.data());
            hasher.process(first, first + count);
        }
    }
    if (!input.eof()) {
        return false;
    }
    hasher.finish();
    *digest = picosha2::get_hash_hex_string(hasher);
    return true;
}

BinaryImage::Pointer alignToReference(BinaryImage* source,
                                      const ImageGeometry& reference)
{
    using Resample = itk::ResampleImageFilter<BinaryImage, BinaryImage>;
    using Interpolator =
        itk::NearestNeighborInterpolateImageFunction<BinaryImage, double>;
    using Transform = itk::IdentityTransform<double, kDimension>;

    BinaryImage::SpacingType spacing;
    BinaryImage::PointType origin;
    BinaryImage::DirectionType direction;
    BinaryImage::SizeType size;
    BinaryImage::IndexType start;
    start.Fill(0);
    for (unsigned int axis = 0; axis < kDimension; ++axis) {
        spacing[axis] = reference.spacing[axis];
        origin[axis] = reference.origin[axis];
        size[axis] = static_cast<BinaryImage::SizeType::SizeValueType>(
            reference.dimensions[axis]);
        for (unsigned int column = 0; column < kDimension; ++column) {
            direction[axis][column] = reference.direction[axis][column];
        }
    }
    Resample::Pointer resample = Resample::New();
    resample->SetInput(source);
    resample->SetTransform(Transform::New());
    resample->SetInterpolator(Interpolator::New());
    resample->SetOutputSpacing(spacing);
    resample->SetOutputOrigin(origin);
    resample->SetOutputDirection(direction);
    resample->SetSize(size);
    resample->SetOutputStartIndex(start);
    resample->SetDefaultPixelValue(0);
    resample->Update();
    BinaryImage::Pointer output = resample->GetOutput();
    output->DisconnectPipeline();
    return output;
}

struct LayerReadResult {
    VascularRoiPriorReadStatus status =
        VascularRoiPriorReadStatus::ProcessingFailed;
    std::optional<XQVascularRoiLayer> layer;
};

LayerReadResult readLayer(const ImageGeometry& reference,
                          const VascularRoiFileInput& input)
{
    LayerReadResult result;
    std::string sourceFingerprint;
    if (!hashFile(input.path, &sourceFingerprint)) {
        result.status = VascularRoiPriorReadStatus::SourceReadFailed;
        return result;
    }

    using Reader = itk::ImageFileReader<BinaryImage>;
    Reader::Pointer reader = Reader::New();
    reader->SetImageIO(itk::NiftiImageIO::New());
    reader->SetFileName(input.path);
    reader->Update();
    BinaryImage::Pointer source = reader->GetOutput();
    source->DisconnectPipeline();
    const ImageGeometry sourceGeometry = geometryFromImage(source);
    if (!finiteGeometry(sourceGeometry)) {
        result.status = VascularRoiPriorReadStatus::InvalidSourceGeometry;
        return result;
    }
    if (!physicalOverlap(reference, sourceGeometry)) {
        result.status = VascularRoiPriorReadStatus::NoPhysicalOverlap;
        return result;
    }

    const bool resampled = !sameGeometry(reference, sourceGeometry);
    BinaryImage::Pointer aligned = resampled
        ? alignToReference(source, reference)
        : source;
    const std::size_t sourceVoxelCount =
        source->GetLargestPossibleRegion().GetNumberOfPixels();
    const std::size_t alignedVoxelCount =
        aligned->GetLargestPossibleRegion().GetNumberOfPixels();
    const unsigned char* sourceValues = source->GetBufferPointer();
    const unsigned char* alignedValues = aligned->GetBufferPointer();
    std::size_t sourceForeground = 0;
    std::size_t alignedForeground = 0;
    for (std::size_t index = 0; index < sourceVoxelCount; ++index) {
        sourceForeground += sourceValues[index] != 0 ? 1 : 0;
    }
    for (std::size_t index = 0; index < alignedVoxelCount; ++index) {
        alignedForeground += alignedValues[index] != 0 ? 1 : 0;
    }
    if (sourceForeground == 0 || alignedForeground == 0) {
        result.status = VascularRoiPriorReadStatus::EmptyForeground;
        return result;
    }
    if (sourceForeground == sourceVoxelCount
        || alignedForeground == alignedVoxelCount) {
        result.status = VascularRoiPriorReadStatus::AllForeground;
        return result;
    }

    std::shared_ptr<XQSegmentationMask> mask =
        std::make_shared<XQSegmentationMask>(reference.dimensions);
    mask->setGeometry(reference);
    mask->setLabels({SegmentationLabel{
        1, std::string("roi_") + vascularRoiRoleToken(input.role)}});
    for (std::size_t index = 0; index < alignedVoxelCount; ++index) {
        if (alignedValues[index] != 0) {
            mask->setLabelAt(index, 1);
        }
    }

    XQVascularRoiLayer layer;
    layer.role = input.role;
    layer.mask = mask;
    layer.sourceGeometry = sourceGeometry;
    layer.hasSourceGeometry = true;
    layer.resampledToReference = resampled;
    layer.sourceForegroundVoxelCount = sourceForeground;
    layer.alignedForegroundVoxelCount = alignedForeground;
    layer.sourceFingerprint = sourceFingerprint;
    layer.alignedFingerprint = itk_detail::vascularRoiAlignedFingerprint(
        input.role, reference, mask->voxels());
    layer.generatorId = input.generatorId;
    layer.generatorVersion = input.generatorVersion;
    if (!layer.isValid()) {
        result.status = VascularRoiPriorReadStatus::ProcessingFailed;
        return result;
    }
    result.status = VascularRoiPriorReadStatus::Ok;
    result.layer = std::move(layer);
    return result;
}

} // namespace

VascularRoiPriorReadResult ItkVascularRoiPriorReader::read(
    const XQImageVolume& reference,
    const std::string& ctInputFingerprint,
    const std::vector<VascularRoiFileInput>& inputs) const
{
    VascularRoiPriorReadResult result;
    if (!reference.hasGeometry() || ctInputFingerprint.empty()
        || inputs.size() != 2) {
        result.diagnostics.push_back(error("vascular_roi.invalid_argument"));
        return result;
    }
    if (!finiteGeometry(reference.geometry())) {
        result.status = VascularRoiPriorReadStatus::InvalidReferenceGeometry;
        result.diagnostics.push_back(
            error("vascular_roi.invalid_reference_geometry"));
        return result;
    }
    bool hasOrgan = false;
    bool hasVessel = false;
    for (const VascularRoiFileInput& input : inputs) {
        if (input.path.empty() || input.generatorId.empty()
            || input.generatorVersion.empty()) {
            result.diagnostics.push_back(error("vascular_roi.invalid_input"));
            return result;
        }
        bool* seen = input.role == VascularRoiRole::Organ
            ? &hasOrgan : &hasVessel;
        if (*seen) {
            result.status = VascularRoiPriorReadStatus::DuplicateRole;
            result.diagnostics.push_back(error("vascular_roi.duplicate_role"));
            return result;
        }
        *seen = true;
    }
    if (!hasOrgan || !hasVessel) {
        result.diagnostics.push_back(error("vascular_roi.missing_role"));
        return result;
    }

    try {
        XQVascularRoiPriorV1 prior;
        prior.ctInputFingerprint = ctInputFingerprint;
        for (VascularRoiRole role :
             {VascularRoiRole::Organ, VascularRoiRole::CoarseVessel}) {
            const auto found = std::find_if(
                inputs.begin(), inputs.end(),
                [role](const VascularRoiFileInput& input) {
                    return input.role == role;
                });
            const LayerReadResult read = readLayer(reference.geometry(), *found);
            if (read.status != VascularRoiPriorReadStatus::Ok
                || !read.layer.has_value()) {
                result.status = read.status;
                result.diagnostics.push_back(error(
                    std::string("vascular_roi.")
                        .append(vascularRoiPriorReadStatusToken(read.status))
                        .c_str()));
                return result;
            }
            prior.layers.push_back(*read.layer);
        }
        prior.priorFingerprint = itk_detail::vascularRoiPriorFingerprint(
            prior.ctInputFingerprint, prior.layers);
        if (!prior.isValid()) {
            result.status = VascularRoiPriorReadStatus::ProcessingFailed;
            result.diagnostics.push_back(error("vascular_roi.invalid_output"));
            return result;
        }
        result.status = VascularRoiPriorReadStatus::Ok;
        result.prior = std::move(prior);
        return result;
    } catch (const itk::ExceptionObject&) {
        result.status = VascularRoiPriorReadStatus::SourceReadFailed;
        result.diagnostics.push_back(error("vascular_roi.itk_exception"));
    } catch (const std::bad_alloc&) {
        result.status = VascularRoiPriorReadStatus::AllocationFailed;
        result.diagnostics.push_back(error("vascular_roi.allocation_failed"));
    } catch (...) {
        result.status = VascularRoiPriorReadStatus::ProcessingFailed;
        result.diagnostics.push_back(error("vascular_roi.unexpected_exception"));
    }
    result.prior.reset();
    return result;
}

} // namespace xq
