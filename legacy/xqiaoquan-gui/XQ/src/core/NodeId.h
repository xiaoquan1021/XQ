#ifndef XQ_CORE_NODE_ID_H
#define XQ_CORE_NODE_ID_H

#include <cstddef>
#include <functional>
#include <string>

namespace xq {

class NodeId {
public:
    typedef unsigned long long ValueType;

    NodeId();
    explicit NodeId(ValueType value);

    static NodeId invalid();

    bool is_valid() const;
    ValueType value() const;

    std::string serialize() const;
    static bool parse(const std::string& text, NodeId* out);
    static bool deserialize(const std::string& text, NodeId* out);

    bool operator==(const NodeId& other) const;
    bool operator!=(const NodeId& other) const;
    bool operator<(const NodeId& other) const;

private:
    ValueType value_;
};

} // namespace xq

namespace std {

template <>
struct hash<xq::NodeId> {
    std::size_t operator()(const xq::NodeId& id) const noexcept
    {
        return std::hash<xq::NodeId::ValueType>()(id.value());
    }
};

} // namespace std

#endif // XQ_CORE_NODE_ID_H
