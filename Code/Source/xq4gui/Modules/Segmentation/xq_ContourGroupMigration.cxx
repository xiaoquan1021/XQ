#include "xq_ContourGroupMigration.h"

#include "xq_ContourGroup.h"
#include "xq_PolygonalProfile.h"

#include <mitkLogMacros.h>

#include <set>
#include <string>

xq_ProfileGroup::Pointer
xq_ContourGroupMigration::ToProfileGroup(const xq_ContourGroup* src)
{
    static const std::vector<xq_ProfilePlacementFrame> kNoPlacementFrames;
    return ToProfileGroup(src, kNoPlacementFrames);
}

xq_ProfileGroup::Pointer
xq_ContourGroupMigration::ToProfileGroup(
    const xq_ContourGroup* src,
    const std::vector<xq_ProfilePlacementFrame>& placementFrames)
{
    auto dst = xq_ProfileGroup::New();

    if (!src)
    {
        return dst;
    }

    // Preserve the path name so downstream code can reconstruct the association
    // once a true path-lookup mechanism exists.
    const std::string pathName = src->GetPathName();
    if (!pathName.empty())
    {
        dst->SetAttribute("path_name", pathName);
    }

    const int count = src->GetContourCount();
    if (!pathName.empty() && placementFrames.empty())
    {
        int unresolvedPlacementCount = 0;
        for (int ordinal = 0; ordinal < count; ++ordinal)
        {
            if (src->GetContour(ordinal))
                ++unresolvedPlacementCount;
        }

        if (unresolvedPlacementCount > 0)
        {
            MITK_WARN << "xq_ContourGroupMigration: legacy contour group '" << pathName
                      << "' has no usable placement frames; rejecting "
                      << unresolvedPlacementCount
                      << " path-bound contour(s) instead of inventing ordinal fallback slots.";
            dst->SetAttribute(
                "migration_unresolved_count",
                std::to_string(unresolvedPlacementCount));
        }

        return dst;
    }

    std::set<int> usedFrameIndices;
    int migrationSlotCollisionCount = 0;
    int unresolvedPlacementCount = 0;
    for (int ordinal = 0; ordinal < count; ++ordinal)
    {
        const ContourSlice* slice = src->GetContour(ordinal);
        if (!slice)
        {
            continue;
        }

        // Use xq_PolygonalProfile as the canonical container for migrated
        // contour data.  Other profile types (circular, elliptic) cannot be
        // inferred reliably from ContourGroup data — the polygon representation
        // is the safe, lossless choice.
        auto* profile = new xq_PolygonalProfile();

        // Contour points → canonical anchor points.
        profile->SetAnchorPoints(slice->points);

        // Method string preserved verbatim (e.g. "manual", "threshold", "regiongrow").
        profile->SetMethod(slice->method);

        int pathPosIndex = ordinal;
        int frameIndex = -1;
        if (!placementFrames.empty())
        {
            if (!xq_SegmentationUtils::FindNearestPlacementFrameIndex(
                    placementFrames, profile->GetProfileCenter(), frameIndex, &usedFrameIndices))
            {
                ++unresolvedPlacementCount;
                MITK_WARN << "xq_ContourGroupMigration: contour " << ordinal
                          << " could not be matched to a unique placement frame; skipping migration instead of inventing a synthetic canonical slot.";
                delete profile;
                continue;
            }

            usedFrameIndices.insert(frameIndex);
            pathPosIndex = placementFrames[frameIndex].pathPosIndex;
            profile->SetSlicePlane(
                xq_SegmentationUtils::CreatePlacementPlane(placementFrames[frameIndex]));

            if (dst->HasProfile(pathPosIndex))
            {
                ++migrationSlotCollisionCount;
                ++unresolvedPlacementCount;
                MITK_WARN << "xq_ContourGroupMigration: contour " << ordinal
                          << " matched placement frame " << frameIndex
                          << " at canonical slot " << pathPosIndex
                          << ", but that slot was already occupied; skipping migration instead of inventing a synthetic fallback index.";
                delete profile;
                continue;
            }
        }

        // Without placement-frame context we still fall back to ordinal indices.
        dst->AppendProfile(profile, pathPosIndex, /*timeStep=*/0);
    }

    if (migrationSlotCollisionCount > 0)
    {
        dst->SetAttribute(
            "migration_slot_collision_count",
            std::to_string(migrationSlotCollisionCount));
    }

    if (unresolvedPlacementCount > 0)
    {
        dst->SetAttribute(
            "migration_unresolved_count",
            std::to_string(unresolvedPlacementCount));
    }

    return dst;
}
