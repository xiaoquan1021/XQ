#include "visualization/XQImageViewer.h"

#include "core/XQImageVolume.h"

#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkObject.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>
#include <vtkWindowToImageFilter.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace xq {
namespace {

struct RenderErrorState {
    bool sawError;
};

ImageRenderResult failed_result()
{
    return {false, 0, 0};
}

bool valid_geometry(const ImageGeometry& geometry)
{
    for (int i = 0; i < 3; ++i) {
        if (geometry.dimensions[i] <= 0 || geometry.spacing[i] <= 0.0) {
            return false;
        }
    }
    return true;
}

double fallback_range_width(const IntensityRange& range)
{
    if (range.maximum > range.minimum) {
        return range.maximum - range.minimum;
    }
    return 255.0;
}

vtkSmartPointer<vtkImageData> build_vtk_image(const XQImageVolume& image)
{
    const ImageGeometry& geometry = image.geometry();
    if (!valid_geometry(geometry)) {
        return nullptr;
    }

    vtkSmartPointer<vtkImageData> vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->SetDimensions(geometry.dimensions);
    vtkImage->SetSpacing(geometry.spacing);
    vtkImage->SetOrigin(geometry.origin);

    double direction[9] = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            direction[row * 3 + column] = geometry.direction[row][column];
        }
    }
    vtkImage->SetDirectionMatrix(direction);
    vtkImage->AllocateScalars(VTK_FLOAT, 1);

    if (vtkImage->GetScalarPointer() == nullptr) {
        return nullptr;
    }

    const IntensityRange& range = image.intensityRange();
    const double minimum = range.maximum > range.minimum ? range.minimum : 0.0;
    const double span = fallback_range_width(range);
    const double xDenominator =
        geometry.dimensions[0] > 1 ? static_cast<double>(geometry.dimensions[0] - 1) : 1.0;
    const double yDenominator =
        geometry.dimensions[1] > 1 ? static_cast<double>(geometry.dimensions[1] - 1) : 1.0;
    const double zDenominator =
        geometry.dimensions[2] > 1 ? static_cast<double>(geometry.dimensions[2] - 1) : 1.0;

    for (int z = 0; z < geometry.dimensions[2]; ++z) {
        for (int y = 0; y < geometry.dimensions[1]; ++y) {
            for (int x = 0; x < geometry.dimensions[0]; ++x) {
                const double normalized =
                    0.55 * (static_cast<double>(x) / xDenominator)
                    + 0.35 * (static_cast<double>(y) / yDenominator)
                    + 0.10 * (static_cast<double>(z) / zDenominator);
                float* scalar = static_cast<float*>(vtkImage->GetScalarPointer(x, y, z));
                *scalar = static_cast<float>(minimum + span * normalized);
            }
        }
    }

    return vtkImage;
}

void install_error_observer(vtkObject* object, vtkCallbackCommand* callback)
{
    if (object != nullptr && callback != nullptr) {
        object->AddObserver(vtkCommand::ErrorEvent, callback);
    }
}

} // namespace

class XQImageViewer::Impl {
public:
    ImageRenderResult renderOffscreen(const XQImageVolume& image, int width, int height)
    {
        return renderOffscreenRgb(image, width, height, nullptr);
    }

    ImageRenderResult renderToRgba(const XQImageVolume& image,
                                   int width,
                                   int height,
                                   std::vector<unsigned char>* outRgba)
    {
        if (outRgba == nullptr) {
            return failed_result();
        }

        outRgba->clear();
        return renderOffscreenRgb(image, width, height, outRgba);
    }

private:
    ImageRenderResult renderOffscreenRgb(const XQImageVolume& image,
                                         int width,
                                         int height,
                                         std::vector<unsigned char>* outRgba)
    {
        if (width <= 0 || height <= 0) {
            return failed_result();
        }

        vtkSmartPointer<vtkImageData> vtkImage = build_vtk_image(image);
        if (vtkImage == nullptr) {
            return failed_result();
        }

        RenderErrorState errorState = {false};
        vtkSmartPointer<vtkCallbackCommand> errorCallback =
            vtkSmartPointer<vtkCallbackCommand>::New();
        errorCallback->SetClientData(&errorState);
        errorCallback->SetCallback(
            [](vtkObject*, unsigned long, void* clientData, void*) {
                RenderErrorState* state = static_cast<RenderErrorState*>(clientData);
                if (state != nullptr) {
                    state->sawError = true;
                }
            });

        vtkSmartPointer<vtkImageActor> actor = vtkSmartPointer<vtkImageActor>::New();
        actor->SetInputData(vtkImage);
        actor->SetDisplayExtent(
            0,
            image.geometry().dimensions[0] - 1,
            0,
            image.geometry().dimensions[1] - 1,
            image.geometry().dimensions[2] / 2,
            image.geometry().dimensions[2] / 2);
        actor->SetInterpolate(0);
        install_error_observer(actor, errorCallback);

        vtkImageProperty* property = actor->GetProperty();
        if (property != nullptr) {
            const IntensityRange& range = image.intensityRange();
            const double fallbackWindow = fallback_range_width(range);
            const double window =
                image.windowWidth() > 0.0 ? image.windowWidth() : fallbackWindow;
            const double level =
                image.windowWidth() > 0.0
                    ? image.windowCenter()
                    : (range.maximum > range.minimum ? (range.minimum + range.maximum) * 0.5 : 127.5);
            property->SetColorWindow(window);
            property->SetColorLevel(level);
            property->SetInterpolationTypeToNearest();
        }

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddActor(actor);
        renderer->SetBackground(0.0, 0.0, 0.0);
        renderer->ResetCamera();
        install_error_observer(renderer, errorCallback);

        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(width, height);
        renderWindow->AddRenderer(renderer);
        install_error_observer(renderWindow, errorCallback);

        vtkSmartPointer<vtkWindowToImageFilter> capture =
            vtkSmartPointer<vtkWindowToImageFilter>::New();
        capture->SetInput(renderWindow);
        capture->SetInputBufferTypeToRGB();
        capture->ReadFrontBufferOff();
        install_error_observer(capture, errorCallback);

        try {
            renderWindow->Render();
            capture->Modified();
            capture->Update();
        } catch (...) {
            return failed_result();
        }

        vtkImageData* output = capture->GetOutput();
        int actualDimensions[3] = {0, 0, 0};
        if (output != nullptr) {
            output->GetDimensions(actualDimensions);
        }

        const bool ok =
            !errorState.sawError
            && output != nullptr
            && output->GetScalarPointer() != nullptr
            && actualDimensions[0] == width
            && actualDimensions[1] == height;

        ImageRenderResult result = {ok, actualDimensions[0], actualDimensions[1]};
        if (!ok || outRgba == nullptr) {
            return result;
        }

        const int componentCount = output->GetNumberOfScalarComponents();
        const unsigned char* rgb =
            static_cast<const unsigned char*>(output->GetScalarPointer());
        if (componentCount < 3 || rgb == nullptr) {
            outRgba->clear();
            return {false, actualDimensions[0], actualDimensions[1]};
        }

        const std::size_t pixelCount =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        outRgba->resize(pixelCount * 4U);
        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const std::size_t source = pixel * static_cast<std::size_t>(componentCount);
            const std::size_t target = pixel * 4U;
            (*outRgba)[target] = rgb[source];
            (*outRgba)[target + 1U] = rgb[source + 1U];
            (*outRgba)[target + 2U] = rgb[source + 2U];
            (*outRgba)[target + 3U] = 255U;
        }

        return result;
    }
};

XQImageViewer::XQImageViewer()
    : impl_(new Impl())
{
}

XQImageViewer::~XQImageViewer() = default;

ImageRenderResult XQImageViewer::renderOffscreen(
    const XQImageVolume& image,
    int width,
    int height)
{
    return impl_->renderOffscreen(image, width, height);
}

ImageRenderResult XQImageViewer::renderToRgba(const XQImageVolume& image,
                                              int width,
                                              int height,
                                              std::vector<unsigned char>* outRgba)
{
    return impl_->renderToRgba(image, width, height, outRgba);
}

} // namespace xq
