#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQScene.h>
#include <core/XQScaleSlot.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>

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
    if (std::string(xq::scaleSlotToToken(xq::ScaleSlot::Organ)) != "organ"
        || std::string(xq::scaleSlotToToken(xq::ScaleSlot::Micro)) != "micro"
        || std::string(xq::scaleSlotToToken(xq::ScaleSlot::Cell)) != "cell") {
        return fail("ScaleSlot stable tokens", __LINE__);
    }
    if (std::string(xq::scaleSlotToToken(static_cast<xq::ScaleSlot>(99))) != "") {
        return fail("invalid ScaleSlot has no token", __LINE__);
    }

    xq::ScaleSlot parsed = xq::ScaleSlot::Cell;
    if (!xq::scaleSlotFromToken("organ", &parsed) || parsed != xq::ScaleSlot::Organ) {
        return fail("parse organ token", __LINE__);
    }
    if (!xq::scaleSlotFromToken("micro", &parsed) || parsed != xq::ScaleSlot::Micro) {
        return fail("parse micro token", __LINE__);
    }
    if (!xq::scaleSlotFromToken("cell", &parsed) || parsed != xq::ScaleSlot::Cell) {
        return fail("parse cell token", __LINE__);
    }
    parsed = xq::ScaleSlot::Micro;
    if (xq::scaleSlotFromToken("Organ", &parsed) || parsed != xq::ScaleSlot::Micro) {
        return fail("invalid token rejected without mutating output", __LINE__);
    }
    if (xq::scaleSlotFromToken("organ", nullptr)) {
        return fail("null ScaleSlot parse output rejected", __LINE__);
    }

    xq::XQDataNode legacy(xq::NodeId(1), "image", "Legacy image");
    if (legacy.hasScaleSlot() || legacy.scaleSlot().has_value()) {
        return fail("legacy node scale is absent", __LINE__);
    }

    xq::XQDataNode typed(xq::NodeId(2), xq::XQDomainType::Image, "Typed image", nullptr);
    if (typed.hasScaleSlot() || typed.scaleSlot().has_value()) {
        return fail("typed node default scale is absent", __LINE__);
    }

    typed.setScaleSlot(xq::ScaleSlot::Organ);
    if (!typed.hasScaleSlot() || typed.scaleSlot().value() != xq::ScaleSlot::Organ) {
        return fail("explicit Organ scale stored", __LINE__);
    }

    const xq::XQDataNode copy = typed;
    if (!copy.hasScaleSlot() || copy.scaleSlot().value() != xq::ScaleSlot::Organ) {
        return fail("node copy preserves scale", __LINE__);
    }

    typed.setScaleSlot(xq::ScaleSlot::Micro);
    if (typed.scaleSlot().value() != xq::ScaleSlot::Micro
        || copy.scaleSlot().value() != xq::ScaleSlot::Organ) {
        return fail("scale is node-owned value state", __LINE__);
    }

    typed.clearScaleSlot();
    if (typed.hasScaleSlot() || typed.scaleSlot().has_value()) {
        return fail("clear restores absent scale", __LINE__);
    }

    xq::XQScene scene;
    xq::XQCommandStack stack;
    xq::XQDataNode commanded(
        xq::NodeId(3), xq::XQDomainType::Image, "Commanded image", nullptr);
    commanded.setScaleSlot(xq::ScaleSlot::Cell);
    if (!stack.push(std::make_unique<xq::AddNodeCommand>(&scene, commanded))) {
        return fail("AddNodeCommand accepts scaled node", __LINE__);
    }
    const xq::XQDataNode* inserted = scene.find(commanded.id());
    if (inserted == nullptr || !inserted->hasScaleSlot()
        || inserted->scaleSlot().value() != xq::ScaleSlot::Cell) {
        return fail("command execute preserves scale", __LINE__);
    }
    if (!stack.undo() || scene.find(commanded.id()) != nullptr) {
        return fail("command undo removes scaled node", __LINE__);
    }
    if (!stack.redo()) {
        return fail("command redo restores scaled node", __LINE__);
    }
    inserted = scene.find(commanded.id());
    if (inserted == nullptr || !inserted->hasScaleSlot()
        || inserted->scaleSlot().value() != xq::ScaleSlot::Cell) {
        return fail("command redo preserves scale", __LINE__);
    }

    return 0;
}
