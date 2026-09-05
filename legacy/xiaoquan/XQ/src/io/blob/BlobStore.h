#ifndef XQ_IO_BLOB_BLOB_STORE_H
#define XQ_IO_BLOB_BLOB_STORE_H

#include "core/asset/BufferRef.h"

#include <cstdint>
#include <string>
#include <vector>

namespace xq {

// Content-addressed, headerless blob store (M8a design §4, decisions D3/D6).
//
// A BlobStore is rooted at an asset directory (e.g. "<stem>.assets/"). Each blob
// file holds ONLY canonical little-endian raw bytes -- no header. Every
// self-describing field (elementType / components / elementCount / byteCount /
// endianness / formatVersion / sha256) lives in the returned BufferRef, which
// the caller persists in the main document. Identity is the SHA-256 of the
// whole file, so a blob lands at "blobs/<sha[0:2]>/<sha>.bin" and identical
// content is written once (dedup).
//
// Encoding is explicit per field (never memcpy of a struct / std::array), so it
// is stable across compilers and ABIs:
//   F64 -> 8 bytes LE   (double)
//   I32 -> 4 bytes LE   (int, encoded as exactly 32 bits)
//   U8  -> 1 byte
//   F32 -> 4 bytes LE   (float)
//   U32 -> 4 bytes LE   (std::uint32_t)
//   U64 -> 8 bytes LE   (std::uint64_t)
//
// This class does NOT touch the main document or any node/payload type; wiring
// payloads through it is a later step.
class BlobStore {
public:
    enum class Status {
        Ok,
        InvalidMetadata,      // BufferRef shape/path/type metadata is invalid
        MissingBlob,         // file at relPath does not exist
        TruncatedBlob,       // file shorter than byteCount, or too short to decode
        ByteCountMismatch,   // file size != BufferRef.byteCount
        ChecksumMismatch     // recomputed SHA-256 != BufferRef.sha256
    };

    explicit BlobStore(const std::string& assetRootDir);

    // Canonicalize the typed elements to little-endian, hash, and atomically
    // publish to blobs/<sha[0:2]>/<sha>.bin (skipped if already present). The
    // element vector size must equal elementCount * components, and the input
    // type must match elementType's width class (F64<-double, I32<-int,
    // U8<-uint8_t); a mismatch returns false with no file written.
    bool put(BlobElementType elementType, std::uint16_t components,
             std::uint64_t elementCount, const std::vector<double>& elements,
             BufferRef* out);
    bool put(BlobElementType elementType, std::uint16_t components,
             std::uint64_t elementCount, const std::vector<int>& elements,
             BufferRef* out);
    bool put(BlobElementType elementType, std::uint16_t components,
             std::uint64_t elementCount, const std::vector<std::uint8_t>& elements,
             BufferRef* out);

    // Read ref.relPath, verify size + SHA-256, then decode back to the typed
    // vector. The decode overload must match elementType's width class. Any
    // mismatch returns the corresponding Status and leaves *out untouched.
    Status get(const BufferRef& ref, std::vector<double>* out) const;
    Status get(const BufferRef& ref, std::vector<int>* out) const;
    Status get(const BufferRef& ref, std::vector<std::uint8_t>* out) const;

    // Canonical little-endian encoding of typed elements, exposed for tests
    // (byte-for-byte stability) and reuse. byteWidth(elementType) is the per-
    // scalar size.
    static std::vector<unsigned char> encode(BlobElementType elementType,
                                             const std::vector<double>& elements);
    static std::vector<unsigned char> encode(BlobElementType elementType,
                                             const std::vector<int>& elements);
    static std::vector<unsigned char> encode(BlobElementType elementType,
                                             const std::vector<std::uint8_t>& elements);

    static std::size_t byteWidth(BlobElementType elementType);

    const std::string& rootDir() const { return rootDir_; }

private:
    // Shared publish path: takes already-canonical bytes + metadata, writes the
    // content-addressed file (atomic, dedup), fills *out.
    bool publish(BlobElementType elementType, std::uint16_t components,
                 std::uint64_t elementCount, const std::vector<unsigned char>& bytes,
                 BufferRef* out) const;

    // Shared read+verify path: returns raw verified bytes via *bytes.
    Status readVerified(const BufferRef& ref, std::vector<unsigned char>* bytes) const;

    std::string rootDir_;
};

} // namespace xq

#endif // XQ_IO_BLOB_BLOB_STORE_H
