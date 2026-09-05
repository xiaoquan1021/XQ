#include <core/NodeId.h>
#include <core/XQDataNode.h>

#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

} // namespace

int main()
{
    const xq::NodeId invalid;
    if (invalid.is_valid()) {
        return fail("default NodeId is invalid");
    }

    const xq::NodeId also_invalid = xq::NodeId::invalid();
    if (also_invalid.is_valid()) {
        return fail("NodeId::invalid is invalid");
    }
    if (!(invalid == also_invalid)) {
        return fail("invalid NodeId values compare equal");
    }

    const xq::NodeId first(1);
    const xq::NodeId second(2);
    const xq::NodeId first_copy(1);
    if (!first.is_valid()) {
        return fail("positive NodeId is valid");
    }
    if (!(first == first_copy)) {
        return fail("equal NodeId values compare equal");
    }
    if (first != first_copy) {
        return fail("equal NodeId values do not compare unequal");
    }
    if (!(first != second)) {
        return fail("different NodeId values compare unequal");
    }
    if (!(first < second)) {
        return fail("NodeId ordering follows integer value");
    }

    std::unordered_map<xq::NodeId, std::string> hashed_names;
    hashed_names[first] = "first";
    if (hashed_names[first] != "first") {
        return fail("NodeId can be used as unordered_map key");
    }

    std::map<xq::NodeId, std::string> names;
    names[second] = "second";
    names[first] = "first";
    if (names.begin()->first != first) {
        return fail("NodeId can be ordered as map key");
    }

    const std::string serialized = second.serialize();
    xq::NodeId parsed;
    if (!xq::NodeId::parse(serialized, &parsed)) {
        return fail("serialized NodeId parses");
    }
    if (parsed != second) {
        return fail("serialize-parse round-trip preserves NodeId");
    }

    xq::NodeId parsed_invalid(99);
    if (!xq::NodeId::parse(invalid.serialize(), &parsed_invalid)) {
        return fail("serialized invalid NodeId parses");
    }
    if (parsed_invalid.is_valid()) {
        return fail("parsed invalid NodeId remains invalid");
    }
    if (xq::NodeId::parse("not-a-node-id", &parsed_invalid)) {
        return fail("invalid NodeId text is rejected");
    }

    const xq::XQDataNode node(first, "image", "Head MRI");
    if (node.id() != first) {
        return fail("XQDataNode id accessor returns constructor value");
    }
    if (node.domain_type() != "image") {
        return fail("XQDataNode domain_type accessor returns constructor value");
    }
    if (node.display_name() != "Head MRI") {
        return fail("XQDataNode display_name accessor returns constructor value");
    }

    return 0;
}
