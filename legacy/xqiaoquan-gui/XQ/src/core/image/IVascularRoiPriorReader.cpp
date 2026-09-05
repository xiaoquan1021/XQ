#include "core/image/IVascularRoiPriorReader.h"

namespace xq {

const char* vascularRoiPriorReadStatusToken(
    VascularRoiPriorReadStatus status)
{
    switch (status) {
    case VascularRoiPriorReadStatus::Ok: return "ok";
    case VascularRoiPriorReadStatus::InvalidArgument: return "invalid_argument";
    case VascularRoiPriorReadStatus::InvalidReferenceGeometry:
        return "invalid_reference_geometry";
    case VascularRoiPriorReadStatus::DuplicateRole: return "duplicate_role";
    case VascularRoiPriorReadStatus::SourceReadFailed:
        return "source_read_failed";
    case VascularRoiPriorReadStatus::InvalidSourceGeometry:
        return "invalid_source_geometry";
    case VascularRoiPriorReadStatus::NoPhysicalOverlap:
        return "no_physical_overlap";
    case VascularRoiPriorReadStatus::EmptyForeground: return "empty_foreground";
    case VascularRoiPriorReadStatus::AllForeground: return "all_foreground";
    case VascularRoiPriorReadStatus::AllocationFailed:
        return "allocation_failed";
    case VascularRoiPriorReadStatus::ProcessingFailed:
        return "processing_failed";
    }
    return "unknown";
}

bool VascularRoiPriorReadResult::ok() const
{
    return status == VascularRoiPriorReadStatus::Ok && prior.has_value()
        && prior->isValid();
}

} // namespace xq
