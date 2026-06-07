#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGALGORITHMADAPTER_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGALGORITHMADAPTER_H

#include <QString>
#include <QVariantMap>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace xq::infrastructure
{

struct ImagePreprocessingAlgorithmResult
{
    bool Succeeded = false;
    QString Message;
    vtkSmartPointer<vtkImageData> Image;
};

class ImagePreprocessingAlgorithmAdapter
{
public:
    ImagePreprocessingAlgorithmResult RunBinaryThreshold(
        vtkImageData* input,
        const QVariantMap& parameters) const;

    ImagePreprocessingAlgorithmResult RunGaussianSmoothing(
        vtkImageData* input,
        const QVariantMap& parameters) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGALGORITHMADAPTER_H
