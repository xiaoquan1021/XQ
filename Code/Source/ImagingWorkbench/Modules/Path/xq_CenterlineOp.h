#pragma once

// XQ design: lightweight undo/redo operation for centerline edits.
// XQ keeps one focused struct with in-class init and scoped enum.

#include <xqModulePathExports.h>

#include <mitkOperation.h>
#include <mitkPoint.h>

class XQMODULEPATH_EXPORT xq_CenterlineOp : public mitk::Operation
{
public:
    // XQ design: scoped enum with backward-compatible unscoped constants
    enum class CenterlineAction {
        ActInsertAnchor    = 100,
        ActRemoveAnchor    = 101,
        ActRelocateAnchor      = 102,
        ActHighlightAnchor = 103,
        ActClearHighlights           = 104
    };

    // Backward-compatible int aliases for callers using xq_CenterlineOp::OpFOO
    // as mitk::OperationType (int) values or unordered_map<int> keys.
    static constexpr int ActInsertAnchor      = static_cast<int>(CenterlineAction::ActInsertAnchor);
    static constexpr int ActRemoveAnchor      = static_cast<int>(CenterlineAction::ActRemoveAnchor);
    static constexpr int ActRelocateAnchor        = static_cast<int>(CenterlineAction::ActRelocateAnchor);
    static constexpr int ActHighlightAnchor = static_cast<int>(CenterlineAction::ActHighlightAnchor);
    static constexpr int ActClearHighlights             = static_cast<int>(CenterlineAction::ActClearHighlights);

    xq_CenterlineOp(mitk::OperationType opType, unsigned int timeStep,
                     const mitk::Point3D& point, int index);
    ~xq_CenterlineOp() override = default;

    [[nodiscard]] mitk::Point3D  GetPosition()    const;
    [[nodiscard]] int            GetAnchorIndex()    const;
    [[nodiscard]] unsigned int   GetFrameIndex() const;

private:
    mitk::Point3D m_Position{};
    int           m_AnchorIdx    = -1;
    unsigned int  m_FrameIdx = 0;
};
