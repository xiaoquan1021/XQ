#include "adapters/vtk/VtkImageAdapter.h"

#include <vtkErrorCode.h>
#include <vtkImageData.h>
#include <vtkMatrix3x3.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>
#include <vtkXMLImageDataReader.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace xq {
namespace {

ScalarType map_scalar_type(int vtkScalarType)
{
    switch (vtkScalarType) {
    case VTK_SIGNED_CHAR:
        return ScalarType::Int8;
    case VTK_CHAR:
#if VTK_TYPE_CHAR_IS_SIGNED
        return ScalarType::Int8;
#else
        return ScalarType::UInt8;
#endif
    case VTK_UNSIGNED_CHAR:
        return ScalarType::UInt8;
    case VTK_SHORT:
        return ScalarType::Int16;
    case VTK_UNSIGNED_SHORT:
        return ScalarType::UInt16;
    case VTK_INT:
        return ScalarType::Int32;
    case VTK_UNSIGNED_INT:
        return ScalarType::UInt32;
    case VTK_FLOAT:
        return ScalarType::Float32;
    case VTK_DOUBLE:
        return ScalarType::Float64;
    default:
        return ScalarType::Unknown;
    }
}

void set_identity_direction(ImageGeometry* geometry)
{
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            geometry->direction[row][column] = row == column ? 1.0 : 0.0;
        }
    }
}

// Reads a .vti and fills *volume with its geometry/type metadata. On success
// returns Ok and sets *outImage to the read vtkImageData (owned by *reader,
// which the caller must keep alive while using outImage). The buffer handle on
// the volume is left unset; callers that need real scalars decode from
// *outImage separately.
VtkImageAdapter::LoadStatus read_vti_metadata(const std::string& path,
                                              vtkSmartPointer<vtkXMLImageDataReader>* reader,
                                              vtkImageData** outImage,
                                              XQImageVolume* volume)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input.good()) {
        return VtkImageAdapter::LoadStatus::FileNotFound;
    }

    *reader = vtkSmartPointer<vtkXMLImageDataReader>::New();
    (*reader)->SetFileName(path.c_str());
    (*reader)->Update();
    if ((*reader)->GetErrorCode() != vtkErrorCode::NoError) {
        return VtkImageAdapter::LoadStatus::ReadFailed;
    }

    vtkImageData* image = (*reader)->GetOutput();
    if (image == nullptr) {
        return VtkImageAdapter::LoadStatus::InvalidImage;
    }

    ImageGeometry geometry = {};
    image->GetDimensions(geometry.dimensions);
    const double* spacing = image->GetSpacing();
    const double* origin = image->GetOrigin();
    for (int i = 0; i < 3; ++i) {
        geometry.spacing[i] = spacing[i];
        geometry.origin[i] = origin[i];
        if (geometry.dimensions[i] <= 0 || geometry.spacing[i] <= 0.0) {
            return VtkImageAdapter::LoadStatus::InvalidImage;
        }
    }

    vtkMatrix3x3* directionMatrix = image->GetDirectionMatrix();
    if (directionMatrix != nullptr) {
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                geometry.direction[row][column] = directionMatrix->GetElement(row, column);
            }
        }
    } else {
        set_identity_direction(&geometry);
    }
    geometry.coordinateSystem = ImageCoordinateSystem::LPS;

    const int componentCount = image->GetNumberOfScalarComponents();
    if (componentCount < 1) {
        return VtkImageAdapter::LoadStatus::InvalidImage;
    }

    vtkPointData* pointData = image->GetPointData();
    if (pointData == nullptr || pointData->GetScalars() == nullptr) {
        return VtkImageAdapter::LoadStatus::InvalidImage;
    }

    const ScalarType scalarType = map_scalar_type(image->GetScalarType());

    double scalarRange[2] = {0.0, 0.0};
    image->GetScalarRange(scalarRange);

    volume->setGeometry(geometry);
    volume->setScalarType(scalarType);
    volume->setComponentCount(componentCount);
    volume->setIntensityRange({scalarRange[0], scalarRange[1]});
    volume->setModality(ImageModality::Unknown);

    *outImage = image;
    return VtkImageAdapter::LoadStatus::Ok;
}

} // namespace

VtkImageAdapter::LoadStatus VtkImageAdapter::loadVti(const std::string& path, XQImageVolume* out)
{
    if (out == nullptr) {
        return LoadStatus::InvalidImage;
    }

    vtkSmartPointer<vtkXMLImageDataReader> reader;
    vtkImageData* image = nullptr;
    XQImageVolume volume;
    const LoadStatus status = read_vti_metadata(path, &reader, &image, &volume);
    if (status != LoadStatus::Ok) {
        return status;
    }

    volume.setBuffer(std::make_shared<ImageBufferHandle>());
    *out = volume;
    return LoadStatus::Ok;
}

VtkImageAdapter::LoadStatus VtkImageAdapter::loadVtiWithBuffer(
    const std::string& path,
    XQImageVolume* outImage,
    std::shared_ptr<XQMemoryImageBufferHandle>* outBuffer)
{
    if (outImage == nullptr || outBuffer == nullptr) {
        return LoadStatus::InvalidImage;
    }

    vtkSmartPointer<vtkXMLImageDataReader> reader;
    vtkImageData* image = nullptr;
    XQImageVolume volume;
    const LoadStatus status = read_vti_metadata(path, &reader, &image, &volume);
    if (status != LoadStatus::Ok) {
        return status;
    }

    const ScalarType scalarType = volume.scalarType();
    const int componentCount = volume.componentCount();
    const std::size_t scalarSize = XQMemoryImageBufferHandle::scalarSize(scalarType);
    if (scalarSize == 0) {
        return LoadStatus::InvalidImage;
    }

    const ImageGeometry& geometry = volume.geometry();
    const std::size_t voxelCount = static_cast<std::size_t>(geometry.dimensions[0])
        * static_cast<std::size_t>(geometry.dimensions[1])
        * static_cast<std::size_t>(geometry.dimensions[2]);
    const std::size_t byteCount =
        voxelCount * static_cast<std::size_t>(componentCount) * scalarSize;

    // VTK stores point scalars contiguously in native byte order with x fastest,
    // then y, then z, components interleaved per voxel -- exactly the layout
    // XQMemoryImageBufferHandle expects, so a single memcpy from the scalar
    // pointer is correct.
    const void* scalarPointer = image->GetScalarPointer();
    if (scalarPointer == nullptr) {
        return LoadStatus::InvalidImage;
    }

    std::vector<std::uint8_t> bytes(byteCount);
    if (byteCount > 0) {
        std::memcpy(bytes.data(), scalarPointer, byteCount);
    }

    auto buffer = std::make_shared<XQMemoryImageBufferHandle>(
        scalarType, geometry.dimensions, componentCount, std::move(bytes));
    if (!buffer->is_valid()) {
        return LoadStatus::InvalidImage;
    }

    volume.setBuffer(std::make_shared<ImageBufferHandle>());
    *outImage = volume;
    *outBuffer = std::move(buffer);
    return LoadStatus::Ok;
}

} // namespace xq
