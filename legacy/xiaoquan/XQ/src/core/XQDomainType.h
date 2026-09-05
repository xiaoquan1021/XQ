#ifndef XQ_CORE_XQ_DOMAIN_TYPE_H
#define XQ_CORE_XQ_DOMAIN_TYPE_H

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
    AiAnalysis
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
    case XQDomainType::Unknown:
        break;
    }
    return "unknown";
}

} // namespace xq

#endif // XQ_CORE_XQ_DOMAIN_TYPE_H
