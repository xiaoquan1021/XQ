#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQPayload.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

} // namespace

int main()
{
    // Legacy string-typed node still works and reports Unknown domain / no payload.
    {
        const xq::XQDataNode legacy(xq::NodeId(1), "volume", "Legacy Volume");
        if (legacy.domain_type() != "volume") {
            return fail("legacy domain_type preserved", __LINE__);
        }
        if (legacy.domainType() != xq::XQDomainType::Unknown) {
            return fail("legacy node has Unknown typed domain", __LINE__);
        }
        if (legacy.payload() != nullptr) {
            return fail("legacy node has no payload", __LINE__);
        }
    }

    // Payload-aware node carries a typed domain and an XQ-owned payload, and the
    // legacy string accessor reflects the domain token.
    {
        std::shared_ptr<xq::XQPayload> payload =
            std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Path, "Paths/aorta.pth");
        const xq::XQDataNode node(xq::NodeId(2), xq::XQDomainType::Path, "aorta", payload);
        if (node.domainType() != xq::XQDomainType::Path) {
            return fail("typed node reports Path domain", __LINE__);
        }
        if (node.domain_type() != "path") {
            return fail("typed node string domain is path", __LINE__);
        }
        if (node.payload() == nullptr) {
            return fail("typed node holds payload", __LINE__);
        }
        if (node.payload()->domainType() != xq::XQDomainType::Path) {
            return fail("payload reports Path domain", __LINE__);
        }
        const xq::XQSourcePayload* source =
            dynamic_cast<const xq::XQSourcePayload*>(node.payload().get());
        if (source == nullptr || source->sourcePath() != "Paths/aorta.pth") {
            return fail("payload retains source binding", __LINE__);
        }
    }

    // clone() copies payload data into an independent handle.
    {
        std::shared_ptr<xq::XQPayload> payload =
            std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Mesh, "Meshes/0090_0001.msh");
        std::shared_ptr<xq::XQPayload> copy = payload->clone();
        if (copy.get() == payload.get()) {
            return fail("clone yields a distinct payload object", __LINE__);
        }
        if (copy->domainType() != xq::XQDomainType::Mesh) {
            return fail("clone preserves domain", __LINE__);
        }
        const xq::XQSourcePayload* source =
            dynamic_cast<const xq::XQSourcePayload*>(copy.get());
        if (source == nullptr || source->sourcePath() != "Meshes/0090_0001.msh") {
            return fail("clone preserves source binding", __LINE__);
        }
    }

    // setPayload replaces payload and domain in place.
    {
        xq::XQDataNode node(xq::NodeId(3), xq::XQDomainType::Image, "img", nullptr);
        node.setPayload(xq::XQDomainType::SurfaceModel,
                        std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::SurfaceModel,
                                                              "Models/x.mdl"));
        if (node.domainType() != xq::XQDomainType::SurfaceModel) {
            return fail("setPayload updates typed domain", __LINE__);
        }
        if (node.domain_type() != "surface_model") {
            return fail("setPayload updates string domain", __LINE__);
        }
    }

    // groupForDomain maps every domain to its project-tree group.
    {
        if (xq::XQScene::groupForDomain(xq::XQDomainType::Image) != xq::XQScene::Group::Images) {
            return fail("Image -> Images", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::Path) != xq::XQScene::Group::Paths) {
            return fail("Path -> Paths", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::VesselProfile)
            != xq::XQScene::Group::Paths) {
            return fail("VesselProfile -> Paths", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::ContourGroup)
            != xq::XQScene::Group::Segmentations) {
            return fail("ContourGroup -> Segmentations", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::SegmentationMask)
            != xq::XQScene::Group::Segmentations) {
            return fail("SegmentationMask -> Segmentations", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::SurfaceModel)
            != xq::XQScene::Group::Models) {
            return fail("SurfaceModel -> Models", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::Mesh) != xq::XQScene::Group::Meshes) {
            return fail("Mesh -> Meshes", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::SimulationCase)
            != xq::XQScene::Group::Simulations) {
            return fail("SimulationCase -> Simulations", __LINE__);
        }
        if (xq::XQScene::groupForDomain(xq::XQDomainType::Unknown)
            != xq::XQScene::Group::Ungrouped) {
            return fail("Unknown -> Ungrouped", __LINE__);
        }
    }

    return 0;
}
