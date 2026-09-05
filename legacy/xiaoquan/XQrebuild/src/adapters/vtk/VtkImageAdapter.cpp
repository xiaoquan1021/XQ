#include "adapters/vtk/VtkImageAdapter.h"

#include <vtkErrorCode.h>
#include <vtkImageData.h>
#include <vtkMatrix3x3.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>
#include <vtkXMLImageDataReader.h>

#include <fstream>
#include <memory>

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

} // namespace

VtkImageAdapter::LoadStatus VtkImageAdapter::loadVti(const std::string& path, XQImageVolume* out)
{
    if (out == nullptr) {
        return LoadStatus::InvalidImage;
    }

    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input.good()) {
        return LoadStatus::FileNotFound;
    }

    vtkSmartPointer<vtkXMLImageDataReader> reader =
        vtkSmartPointer<vtkXMLImageDataReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    if (reader->GetErrorCode() != vtkErrorCode::NoError) {
        return LoadStatus::ReadFailed;
    }

    vtkImageData* image = reader->GetOutput();
    if (image == nullptr) {
        return LoadStatus::InvalidImage;
    }

    ImageGeometry geometry = {};
    image->GetDimensions(geometry.dimensions);
    const double* spacing = image->GetSpacing();
    const double* origin = image->GetOrigin();
    for (int i = 0; i < 3; ++i) {
        geometry.spacing[i] = spacing[i];
        geometry.origin[i] = origin[i];
        if (geometry.dimensions[i] <= 0 || geometry.spacing[i] <= 0.0) {
            return LoadStatus::InvalidImage;
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
        return LoadStatus::InvalidImage;
    }

    vtkPointData* pointData = image->GetPointData();
    if (pointData == nullptr || pointData->GetScalars() == nullptr) {
        return LoadStatus::InvalidImage;
    }

    double scalarRange[2] = {0.0, 0.0};
    image->GetScalarRange(scalarRange);

    XQImageVolume volume;
    volume.setGeometry(geometry);
    volume.setScalarType(map_scalar_type(image->GetScalarType()));
    volume.setComponentCount(componentCount);
    volume.setIntensityRange({scalarRange[0], scalarRange[1]});
    volume.setModality(ImageModality::Unknown);
    volume.setBuffer(std::make_shared<ImageBufferHandle>());

    *out = volume;
    return LoadStatus::Ok;
}

} // namespace xq
