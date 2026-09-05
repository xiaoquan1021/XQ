#ifndef XQ_IO_BLOB_SHA256_H
#define XQ_IO_BLOB_SHA256_H

#include <cstddef>
#include <string>
#include <vector>

namespace xq {

// Streaming SHA-256 wrapper over the vendored header-only picosha2. Feed bytes
// in any number of chunks via update(); finalHex() returns the 64-char lower
// hex digest. Chunked and one-shot hashing of the same byte stream produce an
// identical digest (test_blob_store proves this against the NIST vectors).
//
// finalHex() finalizes the internal state; call it exactly once per logical
// message. Construct a fresh Sha256 for the next message.
class Sha256 {
public:
    Sha256();

    void update(const void* data, std::size_t byteCount);
    std::string finalHex();

    // Convenience one-shot over a contiguous byte range.
    static std::string hashHex(const void* data, std::size_t byteCount);
    static std::string hashHex(const std::vector<unsigned char>& bytes);

private:
    // picosha2's hash256_one_by_one held by void* to keep this header free of
    // the third-party include (it pulls in <vector>/<algorithm> internals).
    void* impl_;
    bool finalized_;

    Sha256(const Sha256&);
    Sha256& operator=(const Sha256&);

public:
    ~Sha256();
};

} // namespace xq

#endif // XQ_IO_BLOB_SHA256_H
