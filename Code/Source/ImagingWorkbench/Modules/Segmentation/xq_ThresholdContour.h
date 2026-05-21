// XQ ThresholdContour: value-initialized threshold contour with in-class defaults
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkContourModel.h>
#include <mitkImage.h>
#include <mitkPlaneGeometry.h>

#include <vector>

class XQMODULESEGMENTATION_EXPORT xq_ThresholdContour : public mitk::ContourModel
{
public:
    mitkClassMacro(xq_ThresholdContour, mitk::ContourModel);
    itkFactorylessNewMacro(Self);

    void SetThresholdValue(double val);
    double GetThresholdValue() const;

    void SetImageData(mitk::Image::Pointer image);
    mitk::Image::Pointer GetImageData() const;

    void SetPlaneGeometry(const mitk::PlaneGeometry* planeGeometry);
    const std::vector<mitk::Point3D>& GetContourPoints() const;
    void GenerateContourPoints();

protected:
    xq_ThresholdContour();
    xq_ThresholdContour(const xq_ThresholdContour& other);
    ~xq_ThresholdContour() override = default;

    double m_ThresholdValue = 0.0;
    mitk::Image::Pointer m_ImageData = nullptr;
    mitk::PlaneGeometry::ConstPointer m_PlaneGeometry;
    std::vector<mitk::Point3D> m_ContourPoints;
};
