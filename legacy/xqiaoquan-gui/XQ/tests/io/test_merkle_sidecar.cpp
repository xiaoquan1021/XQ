#include "io/blob/MerkleSidecar.h"
#include "io/blob/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cout << "check failed: " << what << std::endl;
        ++g_failures;
    }
}

fs::path freshDir()
{
    const fs::path dir = fs::temp_directory_path() / "xq_merkle_sidecar_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::vector<std::uint8_t> makeBlob(std::size_t n)
{
    std::vector<std::uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xFF);
    }
    return v;
}

// write -> load round-trip; the loaded sidecar's root verifies.
void test_roundtrip(const fs::path& dir)
{
    const std::uint64_t segBytes = 1024;
    const std::vector<std::uint8_t> blob = makeBlob(1024 * 3 + 500); // 4 segments
    const std::string merklePath = (dir / "rt.merkle").string();

    const bool wrote = xq::write_sidecar(blob, segBytes, merklePath);
    check(wrote, "write_sidecar ok");

    xq::MerkleSidecar sc;
    const bool loaded = xq::load_sidecar(merklePath, &sc);
    check(loaded, "load_sidecar ok");
    check(sc.segmentBytes == segBytes, "segmentBytes round-trips");
    check(sc.segCount == 4, "segCount = 4 segments");
    check(sc.blobBytes == blob.size(), "blobBytes round-trips");
    check(sc.segShas.size() == 4, "4 segment digests");
    check(sc.verify_root(), "loaded root verifies");

    // Each listed digest matches a direct hash of that segment of the blob.
    bool allSeg = true;
    for (std::uint64_t s = 0; s < sc.segCount; ++s) {
        const std::size_t off = static_cast<std::size_t>(s * segBytes);
        std::size_t n = static_cast<std::size_t>(segBytes);
        if (off + n > blob.size()) {
            n = blob.size() - off;
        }
        const std::string direct = xq::Sha256::hashHex(blob.data() + off, n);
        if (direct != sc.segShas[static_cast<std::size_t>(s)]) {
            allSeg = false;
        }
        // verify_segment against the in-memory blob also passes.
        const bool vs = sc.verify_segment(blob.data(), blob.size(), s);
        check(vs, "verify_segment passes on clean blob");
    }
    check(allSeg, "listed digests equal direct segment hashes");
}

// A sidecar whose root field is wrong fails verify_root.
void test_root_mismatch(const fs::path& dir)
{
    const std::uint64_t segBytes = 1024;
    const std::vector<std::uint8_t> blob = makeBlob(1024 * 2);
    const std::string merklePath = (dir / "badroot.merkle").string();
    check(xq::write_sidecar(blob, segBytes, merklePath), "write ok for root test");

    xq::MerkleSidecar sc;
    check(xq::load_sidecar(merklePath, &sc), "load ok for root test");
    // Corrupt the stored root; recompute-and-compare must now fail.
    sc.root = std::string(64, '0');
    const bool rootOk = sc.verify_root();
    check(!rootOk, "verify_root fails on tampered root");
}

// Tamper one segment's bytes: verify_segment for that segment FAILs, others OK.
void test_tamper_segment(const fs::path& dir)
{
    const std::uint64_t segBytes = 1024;
    std::vector<std::uint8_t> blob = makeBlob(1024 * 4); // 4 full segments
    const std::string merklePath = (dir / "tamper.merkle").string();
    check(xq::write_sidecar(blob, segBytes, merklePath), "write ok for tamper test");

    xq::MerkleSidecar sc;
    check(xq::load_sidecar(merklePath, &sc), "load ok for tamper test");

    // Flip a byte inside segment 2.
    const std::size_t tamperOffset = static_cast<std::size_t>(2 * segBytes) + 10;
    blob[tamperOffset] ^= 0x01;

    const bool seg2 = sc.verify_segment(blob.data(), blob.size(), 2);
    check(!seg2, "tampered segment 2 fails verify");

    const bool seg0 = sc.verify_segment(blob.data(), blob.size(), 0);
    const bool seg1 = sc.verify_segment(blob.data(), blob.size(), 1);
    const bool seg3 = sc.verify_segment(blob.data(), blob.size(), 3);
    check(seg0 && seg1 && seg3, "untouched segments still verify");
}

// load_sidecar rejects a bad-magic file.
void test_bad_magic(const fs::path& dir)
{
    const std::string p = (dir / "bad.merkle").string();
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f << "NOTMERKLE\nversion 1\n";
    }
    xq::MerkleSidecar sc;
    const bool loaded = xq::load_sidecar(p, &sc);
    check(!loaded, "load_sidecar rejects bad magic");
}

} // namespace

int main()
{
    const fs::path dir = freshDir();

    test_roundtrip(dir);
    test_root_mismatch(dir);
    test_tamper_segment(dir);
    test_bad_magic(dir);

    std::error_code ec;
    fs::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all merkle sidecar checks passed" << std::endl;
    return 0;
}
