#ifndef XQ_DEFAULT_PERSPECTIVE_H
#define XQ_DEFAULT_PERSPECTIVE_H

#include <berryIPerspectiveFactory.h>
#include <array>

// Declarative region descriptor for config-driven layout construction.
struct XqPanelRegion
{
    const char* viewId;
    int         placement;   // berry::IPageLayout::{LEFT,RIGHT,BOTTOM,TOP}
    float       ratio;
    const char* refId;       // anchor reference (nullptr ⇒ editor area)
    bool        closeable;
};

class xq_DefaultPerspective : public QObject, public berry::IPerspectiveFactory
{
    Q_OBJECT
    Q_INTERFACES(berry::IPerspectiveFactory)

public:
    xq_DefaultPerspective();

    void CreateInitialLayout(berry::IPageLayout::Pointer layout) override;

private:
    // Materialise a sequence of panel regions against the page layout.
    static void ApplyRegions(berry::IPageLayout::Pointer layout,
                             const XqPanelRegion* regions,
                             std::size_t count,
                             const QString& editorArea);

    // Populate placeholder slots for deferred tool views.
    static void RegisterToolSlots(berry::IPageLayout::Pointer layout,
                                  const QString& editorArea);
};

#endif // XQ_DEFAULT_PERSPECTIVE_H
