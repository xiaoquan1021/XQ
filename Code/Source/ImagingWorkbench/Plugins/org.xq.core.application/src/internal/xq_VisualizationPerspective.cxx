#include "xq_VisualizationPerspective.h"
#include <berryIViewLayout.h>

xq_VisualizationPerspective::xq_VisualizationPerspective()
{
}

void xq_VisualizationPerspective::CreateInitialLayout(berry::IPageLayout::Pointer layout)
{
    QString editorArea = layout->GetEditorArea();

    // Left: Data Manager (narrow, 15%)
    layout->AddView("org.xq.views.datamanager",
                    berry::IPageLayout::LEFT, 0.15f, editorArea);
    berry::IViewLayout::Pointer dmLayout =
        layout->GetViewLayout("org.xq.views.datamanager");
    if (dmLayout)
    {
        dmLayout->SetCloseable(false);
    }

    // Bottom placeholder for log
    berry::IPlaceholderFolderLayout::Pointer bottomFolder =
        layout->CreatePlaceholderFolder("bottom",
                                        berry::IPageLayout::BOTTOM,
                                        0.85f, editorArea);
    bottomFolder->AddPlaceholder("org.blueberry.views.logview");

    layout->SetEditorAreaVisible(true);
}
