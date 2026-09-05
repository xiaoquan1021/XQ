#include "io/blob/MerkleSidecar.h"

#include "io/blob/Sha256.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace xq {

namespace {

constexpr const char* kMagic = "XQMERKLE";
constexpr int kVersion = 1;

bool isHex64(const std::string& s)
{
    if (s.size() != 64) {
        return false;
    }
    for (char c : s) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string merkle_root_of(const std::vector<std::string>& segShas)
{
    Sha256 h;
    for (const std::string& s : segShas) {
        h.update(s.data(), s.size());
    }
    return h.finalHex();
}

MerkleSidecar build_sidecar(const void* blobBytes, std::size_t blobSize,
                            std::uint64_t segmentBytes)
{
    MerkleSidecar sc;
    if (segmentBytes == 0) {
        return sc;
    }

    sc.segmentBytes = segmentBytes;
    sc.blobBytes = static_cast<std::uint64_t>(blobSize);

    const std::uint8_t* base = static_cast<const std::uint8_t*>(blobBytes);
    std::size_t offset = 0;
    while (offset < blobSize) {
        std::size_t n = static_cast<std::size_t>(segmentBytes);
        if (offset + n > blobSize) {
            n = blobSize - offset;
        }
        sc.segShas.push_back(Sha256::hashHex(base + offset, n));
        offset += n;
    }
    // An empty blob yields zero segments; callers should not build sidecars for
    // 0-byte blobs (those use FullVerify), but keep this well defined.
    sc.segCount = static_cast<std::uint64_t>(sc.segShas.size());
    sc.root = merkle_root_of(sc.segShas);
    return sc;
}

bool MerkleSidecar::verify_root() const
{
    if (segShas.empty() || segShas.size() != static_cast<std::size_t>(segCount)) {
        return false;
    }
    for (const std::string& s : segShas) {
        if (!isHex64(s)) {
            return false;
        }
    }
    return merkle_root_of(segShas) == root;
}

bool MerkleSidecar::verify_segment(const void* base, std::size_t mappedSize,
                                   std::uint64_t segIndex) const
{
    if (segmentBytes == 0 || segIndex >= segCount) {
        return false;
    }
    if (mappedSize != static_cast<std::size_t>(blobBytes)) {
        return false;
    }
    if (segIndex >= segShas.size()) {
        return false;
    }

    const std::size_t offset =
        static_cast<std::size_t>(segIndex) * static_cast<std::size_t>(segmentBytes);
    if (offset >= mappedSize) {
        return false;
    }
    std::size_t n = static_cast<std::size_t>(segmentBytes);
    if (offset + n > mappedSize) {
        n = mappedSize - offset;
    }

    const std::uint8_t* p = static_cast<const std::uint8_t*>(base) + offset;
    return Sha256::hashHex(p, n) == segShas[static_cast<std::size_t>(segIndex)];
}

bool write_sidecar(const std::vector<std::uint8_t>& blobBytes,
                   std::uint64_t segmentBytes, const std::string& outMerklePath)
{
    if (segmentBytes == 0) {
        return false;
    }
    const MerkleSidecar sc =
        build_sidecar(blobBytes.data(), blobBytes.size(), segmentBytes);

    std::ofstream out(outMerklePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << kMagic << "\n";
    out << "version " << kVersion << "\n";
    out << "segmentBytes " << sc.segmentBytes << "\n";
    out << "segCount " << sc.segCount << "\n";
    out << "blobBytes " << sc.blobBytes << "\n";
    out << "root " << sc.root << "\n";
    for (const std::string& s : sc.segShas) {
        out << "seg " << s << "\n";
    }
    out.flush();
    return static_cast<bool>(out);
}

bool write_sidecar_for_file(const std::string& blobPath,
                            std::uint64_t segmentBytes,
                            const std::string& outMerklePath)
{
    std::ifstream in(blobPath, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), size)) {
        return false;
    }
    return write_sidecar(bytes, segmentBytes, outMerklePath);
}

bool load_sidecar(const std::string& merklePath, MerkleSidecar* out)
{
    if (out == nullptr) {
        return false;
    }
    std::ifstream in(merklePath, std::ios::binary);
    if (!in) {
        return false;
    }

    MerkleSidecar sc;
    std::string line;

    // Magic line.
    if (!std::getline(in, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != kMagic) {
        return false;
    }

    bool haveVersion = false;
    bool haveSegBytes = false;
    bool haveSegCount = false;
    bool haveBlobBytes = false;
    bool haveRoot = false;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "version") {
            int v = 0;
            ls >> v;
            if (v != kVersion) {
                return false;
            }
            haveVersion = true;
        } else if (key == "segmentBytes") {
            ls >> sc.segmentBytes;
            haveSegBytes = true;
        } else if (key == "segCount") {
            ls >> sc.segCount;
            haveSegCount = true;
        } else if (key == "blobBytes") {
            ls >> sc.blobBytes;
            haveBlobBytes = true;
        } else if (key == "root") {
            ls >> sc.root;
            haveRoot = true;
        } else if (key == "seg") {
            std::string digest;
            ls >> digest;
            sc.segShas.push_back(digest);
        } else {
            // Unknown key: ignore for forward tolerance.
        }
    }

    if (!haveVersion || !haveSegBytes || !haveSegCount || !haveBlobBytes
        || !haveRoot) {
        return false;
    }
    if (sc.segShas.size() != static_cast<std::size_t>(sc.segCount)) {
        return false;
    }

    *out = std::move(sc);
    return true;
}

} // namespace xq
