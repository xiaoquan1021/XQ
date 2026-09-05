#ifndef XQ_CORE_ASSET_BUFFER_REF_H
#define XQ_CORE_ASSET_BUFFER_REF_H

#include <cstdint>
#include <string>

namespace xq {

// Element encoding tag for a sidecar blob (D6: the blob file is headerless, so
// every self-describing field lives here in the main-document BufferRef).
enum class BlobElementType {
    F64 = 1, // double
    F32 = 2, // float
    U32 = 3, // std::uint32_t
    U64 = 4, // std::uint64_t
    U8  = 5, // std::uint8_t
    I32 = 6  // 32-bit signed (int encoded as exactly 32 bits)
};

// Main-document reference to one content-addressed sidecar blob plus the full
// metadata needed to decode it. The blob on disk is raw canonical little-endian
// bytes with NO header; all of the description below is the single source of
// truth (D6). Identity of the blob file is its SHA-256 (content addressing).
struct BufferRef {
    std::string relPath;          // relative to the .xqproj directory
    std::uint64_t byteCount = 0;  // == elementCount * components * sizeof(elementType)
    std::string sha256;           // 64-hex SHA-256 of the whole blob (= file name stem)
    std::uint32_t formatVersion = 1;
    std::uint8_t endianness = 0;  // 0 = little (the canonical baseline)
    BlobElementType elementType = BlobElementType::F64;
    std::uint16_t components = 1; // Point3=3, Tri=3, Tet=4, faceId=1, voxel=1
    std::uint64_t elementCount = 0;
};

} // namespace xq

#endif // XQ_CORE_ASSET_BUFFER_REF_H
