#ifndef XQ_VISUALIZATION_PERSPECTIVE_H
#define XQ_VISUALIZATION_PERSPECTIVE_H

#include <berryIPerspectiveFactory.h>

class xq_VisualizationPerspective : public QObject, public berry::IPerspectiveFactory
{
    Q_OBJECT
    Q_INTERFACES(berry::IPerspectiveFactory)

public:
    xq_VisualizationPerspective();
    void CreateInitialLayout(berry::IPageLayout::Pointer layout) override;
};

#endif
