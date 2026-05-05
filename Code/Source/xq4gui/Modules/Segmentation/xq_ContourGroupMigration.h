// xq_ContourGroupMigration: one-way adapter from legacy xq_ContourGroup
// to canonical xq_ProfileGroup.
//
// Purpose: provide a stable code-level migration seam before the UI rewrite.
//          All new code that needs ProfileGroup data from ContourGroup sources
//          must go through this boundary rather than inventing ad-hoc conversions.
//
// Usage:
//   xq_ProfileGroup::Pointer pg =
//       xq_ContourGroupMigration::ToProfileGroup(contourGroup);
#pragma once

#include <xqModuleSegmentationExports.h>

#include "xq_ProfileGroup.h"
#include "xq_SegmentationUtils.h"

class xq_ContourGroup;

class XQMODULESEGMENTATION_EXPORT xq_ContourGroupMigration
{
public:
    // Convert every ContourSlice in src into a canonical xq_PolygonalProfile
    // and collect them into a new xq_ProfileGroup.
    //
    // Semantics preserved:
    //   • contour points   → profile anchor points (SetAnchorPoints)
    //   • contour method   → profile method string (SetMethod)
    //   • group path name  → "path_name" attribute on the returned ProfileGroup
    //
    // Path-position index: xq_ContourGroup has no path-position index; for
    // truly unbound groups (empty path name), the ordinal index of each
    // ContourSlice in the source vector (0, 1, 2, …) is used as a transitional
    // canonical pathPosIndex. Path-bound groups must supply placement frames or
    // they are returned unresolved instead of receiving synthetic ordinal slots.
    //
    // Returns a non-null ProfileGroup (empty if src is null or has no contours).
    static xq_ProfileGroup::Pointer ToProfileGroup(const xq_ContourGroup* src);

    // When placement-frame context is available, bind migrated profiles directly
    // to real path frames instead of using transitional ordinal indices.
    static xq_ProfileGroup::Pointer ToProfileGroup(
        const xq_ContourGroup* src,
        const std::vector<xq_ProfilePlacementFrame>& placementFrames);

    xq_ContourGroupMigration() = delete;  // static-only utility class
};
