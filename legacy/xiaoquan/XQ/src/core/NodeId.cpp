#include "core/NodeId.h"

#include <limits>

namespace xq {
namespace {

const NodeId::ValueType kInvalidValue = 0;

} // namespace

NodeId::NodeId()
    : value_(kInvalidValue)
{
}

NodeId::NodeId(ValueType value)
    : value_(value)
{
}

NodeId NodeId::invalid()
{
    return NodeId(kInvalidValue);
}

bool NodeId::is_valid() const
{
    return value_ != kInvalidValue;
}

NodeId::ValueType NodeId::value() const
{
    return value_;
}

std::string NodeId::serialize() const
{
    if (!is_valid()) {
        return "invalid";
    }
    return std::to_string(value_);
}

bool NodeId::parse(const std::string& text, NodeId* out)
{
    if (out == 0) {
        return false;
    }

    if (text == "invalid") {
        *out = NodeId::invalid();
        return true;
    }

    if (text.empty()) {
        return false;
    }

    ValueType value = 0;
    const ValueType max_value = std::numeric_limits<ValueType>::max();
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        if (*it < '0' || *it > '9') {
            return false;
        }
        const ValueType digit = static_cast<ValueType>(*it - '0');
        if (value > (max_value - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }

    *out = NodeId(value);
    return true;
}

bool NodeId::deserialize(const std::string& text, NodeId* out)
{
    return parse(text, out);
}

bool NodeId::operator==(const NodeId& other) const
{
    return value_ == other.value_;
}

bool NodeId::operator!=(const NodeId& other) const
{
    return !(*this == other);
}

bool NodeId::operator<(const NodeId& other) const
{
    return value_ < other.value_;
}

} // namespace xq
