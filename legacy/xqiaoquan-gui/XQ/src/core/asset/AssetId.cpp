#include "core/asset/AssetId.h"

#include <limits>

namespace xq {
namespace {

const AssetId::ValueType kInvalidValue = 0;

} // namespace

AssetId::AssetId()
    : value_(kInvalidValue)
{
}

AssetId::AssetId(ValueType value)
    : value_(value)
{
}

AssetId AssetId::invalid()
{
    return AssetId(kInvalidValue);
}

bool AssetId::is_valid() const
{
    return value_ != kInvalidValue;
}

AssetId::ValueType AssetId::value() const
{
    return value_;
}

std::string AssetId::serialize() const
{
    if (!is_valid()) {
        return "invalid";
    }
    return std::to_string(value_);
}

bool AssetId::parse(const std::string& text, AssetId* out)
{
    if (out == 0) {
        return false;
    }

    if (text == "invalid") {
        *out = AssetId::invalid();
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

    *out = AssetId(value);
    return true;
}

bool AssetId::deserialize(const std::string& text, AssetId* out)
{
    return parse(text, out);
}

bool AssetId::operator==(const AssetId& other) const
{
    return value_ == other.value_;
}

bool AssetId::operator!=(const AssetId& other) const
{
    return !(*this == other);
}

bool AssetId::operator<(const AssetId& other) const
{
    return value_ < other.value_;
}

} // namespace xq
