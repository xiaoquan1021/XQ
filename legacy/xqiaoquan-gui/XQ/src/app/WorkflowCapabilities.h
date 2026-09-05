#ifndef XQ_APP_WORKFLOW_CAPABILITIES_H
#define XQ_APP_WORKFLOW_CAPABILITIES_H

#if !defined(XQ_ENABLE_FLOW)
#error "XQ_ENABLE_FLOW must be defined by the xq_app_shell build"
#endif

namespace xq {

// Thin runtime projection of the workflow execution capabilities compiled into
// this shell. Persistent project data is deliberately not represented here:
// disabling execution must never hide or rewrite historical FlowResult data.
struct WorkflowCapabilities {
#if XQ_ENABLE_FLOW
    bool flowSolver1D = true;
#else
    bool flowSolver1D = false;
#endif

    static constexpr bool flowCompiledIn()
    {
#if XQ_ENABLE_FLOW
        return true;
#else
        return false;
#endif
    }

    static constexpr WorkflowCapabilities compiledDefaults()
    {
        return WorkflowCapabilities{};
    }

    static constexpr WorkflowCapabilities withoutFlow()
    {
        return WorkflowCapabilities{false};
    }

    // Runtime requests can only remove a compiled capability, never manufacture
    // one that the current binary does not contain.
    constexpr WorkflowCapabilities constrainedToBuild() const
    {
        return WorkflowCapabilities{flowSolver1D && flowCompiledIn()};
    }
};

} // namespace xq

#endif // XQ_APP_WORKFLOW_CAPABILITIES_H
