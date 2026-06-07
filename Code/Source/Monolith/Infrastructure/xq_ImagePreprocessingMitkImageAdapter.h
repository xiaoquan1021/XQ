#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGMITKIMAGEADAPTER_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGMITKIMAGEADAPTER_H

#include <QString>

#include <mitkDataNode.h>
#include <mitkImage.h>

#include <vtkImageData.h>

namespace xq::infrastructure
{

struct ImagePreprocessingMitkInputImageResult
{
    bool Succeeded = false;
    QString Message;
    vtkImageData* Image = nullptr;
};

struct ImagePreprocessingMitkImageCreationResult
{
    bool Succeeded = false;
    QString Message;
    mitk::Image::Pointer Image;
};

class ImagePreprocessingMitkImageAdapter
{
public:
    ImagePreprocessingMitkInputImageResult ExtractInputImage(
        mitk::DataNode* node) const;

    ImagePreprocessingMitkImageCreationResult CreateMitkImage(
        vtkImageData* image) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGMITKIMAGEADAPTER_H
