#include "xq_ViewerPerspective.h"
#include <berryIViewLayout.h>

xq_ViewerPerspective::xq_ViewerPerspective()
{
}

void xq_ViewerPerspective::CreateInitialLayout(berry::IPageLayout::Pointer layout)
{
    QString editorArea = layout->GetEditorArea();

    // Very narrow Data Manager on the left (12%)
    layout->AddView("org.xq.views.datamanager",
                    berry::IPageLayout::LEFT, 0.12f, editorArea);

    // Thin bottom info bar (15% height)
    berry::IPlaceholderFolderLayout::Pointer bottomFolder =
        layout->CreatePlaceholderFolder("bottom",
                                        berry::IPageLayout::BOTTOM,
                                        0.85f, editorArea);
    bottomFolder->AddPlaceholder("org.xq.views.projectmanager");
    bottomFolder->AddPlaceholder("org.blueberry.views.logview");

    layout->SetEditorAreaVisible(true);
}
