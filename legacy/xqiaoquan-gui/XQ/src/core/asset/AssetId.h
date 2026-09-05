#ifndef XQ_CORE_ASSET_ID_H
#define XQ_CORE_ASSET_ID_H

#include <cstddef>
#include <functional>
#include <string>

namespace xq {

// Stable identity of a data asset, independent of NodeId and of the asset's
// content hash. Mirrors NodeId's value-type style but is a DISTINCT type:
// AssetId and NodeId share no conversion, so the compiler rejects passing one
// where the other is expected (D5 type isolation). Construction is explicit;
// there is no implicit conversion to/from NodeId or any integer.
class AssetId {
public:
    typedef unsigned long long ValueType;

    AssetId();
    explicit AssetId(ValueType value);

    static AssetId invalid();

    bool is_valid() const;
    ValueType value() const;

    std::string serialize() const;
    static bool parse(const std::string& text, AssetId* out);
    static bool deserialize(const std::string& text, AssetId* out);

    bool operator==(const AssetId& other) const;
    bool operator!=(const AssetId& other) const;
    bool operator<(const AssetId& other) const;

private:
    ValueType value_;
};

} // namespace xq

namespace std {

template <>
struct hash<xq::AssetId> {
    std::size_t operator()(const xq::AssetId& id) const noexcept
    {
        return std::hash<xq::AssetId::ValueType>()(id.value());
    }
};

} // namespace std

#endif // XQ_CORE_ASSET_ID_H
