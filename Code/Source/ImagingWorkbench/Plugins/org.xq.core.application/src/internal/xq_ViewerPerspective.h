#ifndef XQ_VIEWER_PERSPECTIVE_H
#define XQ_VIEWER_PERSPECTIVE_H

#include <berryIPerspectiveFactory.h>

class xq_ViewerPerspective : public QObject, public berry::IPerspectiveFactory
{
    Q_OBJECT
    Q_INTERFACES(berry::IPerspectiveFactory)

public:
    xq_ViewerPerspective();
    void CreateInitialLayout(berry::IPageLayout::Pointer layout) override;
};

#endif
