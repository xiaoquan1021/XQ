#ifndef XQ_CORE_XQ_DOMAIN_TYPE_H
#define XQ_CORE_XQ_DOMAIN_TYPE_H

#include <string>

namespace xq {

// Stable domain classification for a scene node's payload.
// Order mirrors the main-line source/derived chain (image -> ... -> simulation).
// Unknown covers legacy string-typed nodes that predate the payload mechanism.
enum class XQDomainType {
    Unknown,
    Image,
    Path,
    ContourGroup,
    SegmentationMask,
    SurfaceModel,
    Mesh,
    SimulationCase,
    FlowResult,
    AiAnalysis,
    VesselProfile
};

// Stable string token for a domain type. Used as the legacy domain_type()
// value on payload-aware nodes and in serialized / diagnostic output.
inline const char* domainTypeToString(XQDomainType domain)
{
    switch (domain) {
    case XQDomainType::Image:
        return "image";
    case XQDomainType::Path:
        return "path";
    case XQDomainType::ContourGroup:
        return "contour_group";
    case XQDomainType::SegmentationMask:
        return "segmentation_mask";
    case XQDomainType::SurfaceModel:
        return "surface_model";
    case XQDomainType::Mesh:
        return "mesh";
    case XQDomainType::SimulationCase:
        return "simulation_case";
    case XQDomainType::FlowResult:
        return "flow_result";
    case XQDomainType::AiAnalysis:
        return "ai_analysis";
    case XQDomainType::VesselProfile:
        return "vessel_profile";
    case XQDomainType::Unknown:
        break;
    }
    return "unknown";
}

// Inverse of domainTypeToString(). Project readers and unresolved source
// payloads must share one exhaustive token table so adding a domain cannot
// silently degrade it to Unknown in one persistence path.
inline bool domainTypeFromString(const std::string& token, XQDomainType* out)
{
    if (out == nullptr) {
        return false;
    }
    if (token == "image") {
        *out = XQDomainType::Image;
    } else if (token == "path") {
        *out = XQDomainType::Path;
    } else if (token == "contour_group") {
        *out = XQDomainType::ContourGroup;
    } else if (token == "segmentation_mask") {
        *out = XQDomainType::SegmentationMask;
    } else if (token == "surface_model") {
        *out = XQDomainType::SurfaceModel;
    } else if (token == "mesh") {
        *out = XQDomainType::Mesh;
    } else if (token == "simulation_case") {
        *out = XQDomainType::SimulationCase;
    } else if (token == "flow_result") {
        *out = XQDomainType::FlowResult;
    } else if (token == "ai_analysis") {
        *out = XQDomainType::AiAnalysis;
    } else if (token == "vessel_profile") {
        *out = XQDomainType::VesselProfile;
    } else if (token == "unknown") {
        *out = XQDomainType::Unknown;
    } else {
        return false;
    }
    return true;
}

} // namespace xq

#endif // XQ_CORE_XQ_DOMAIN_TYPE_H
