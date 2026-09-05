#include "core/asset/BufferRef.h"
#include "io/blob/BlobStore.h"
#include "io/blob/Sha256.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

fs::path freshRoot()
{
    const fs::path root = fs::temp_directory_path() / "xq_blob_store_test.assets";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    return root;
}

void truncateFile(const fs::path& path, std::uintmax_t newSize)
{
    std::error_code ec;
    fs::resize_file(path, newSize, ec);
}

void flipFirstByte(const fs::path& path)
{
    std::fstream f(path.string(), std::ios::binary | std::ios::in | std::ios::out);
    char c = 0;
    f.read(&c, 1);
    c = static_cast<char>(c ^ 0x01);
    f.seekp(0);
    f.write(&c, 1);
}

// --- SHA-256 NIST vectors ---------------------------------------------------

void test_sha256_vectors()
{
    check(xq::Sha256::hashHex("", 0) ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256 empty string");

    const std::string abc = "abc";
    check(xq::Sha256::hashHex(abc.data(), abc.size()) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256 \"abc\"");

    const std::string longMsg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    check(xq::Sha256::hashHex(longMsg.data(), longMsg.size()) ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
          "sha256 NIST 448-bit message");
}

void test_sha256_chunked_equals_oneshot()
{
    std::string msg;
    for (int i = 0; i < 1000; ++i) {
        msg += static_cast<char>('a' + (i % 26));
    }
    const std::string oneShot = xq::Sha256::hashHex(msg.data(), msg.size());

    // Feed in irregular chunks; digest must match the one-shot result.
    xq::Sha256 h;
    std::size_t off = 0;
    const std::size_t chunks[] = {1, 7, 63, 64, 65, 128, 200};
    std::size_t ci = 0;
    while (off < msg.size()) {
        std::size_t n = chunks[ci % 7];
        if (off + n > msg.size()) {
            n = msg.size() - off;
        }
        h.update(msg.data() + off, n);
        off += n;
        ++ci;
    }
    check(h.finalHex() == oneShot, "sha256 chunked == one-shot");
}

// --- canonical encoding stability ------------------------------------------

void test_encoding_byte_stable()
{
    // double: 1.0 LE is a known bit pattern (3FF0000000000000 -> bytes reversed)
    const std::vector<double> ones = {1.0};
    const std::vector<unsigned char> enc = xq::BlobStore::encode(xq::BlobElementType::F64, ones);
    const unsigned char expect1[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F};
    bool match = enc.size() == 8;
    for (std::size_t i = 0; match && i < 8; ++i) {
        match = enc[i] == expect1[i];
    }
    check(match, "encode double 1.0 little-endian byte pattern");

    // int32: -1 is 0xFFFFFFFF; 258 is 02 01 00 00 LE
    const std::vector<int> ints = {-1, 258};
    const std::vector<unsigned char> ei = xq::BlobStore::encode(xq::BlobElementType::I32, ints);
    const unsigned char expectI[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x01, 0x00, 0x00};
    bool mi = ei.size() == 8;
    for (std::size_t i = 0; mi && i < 8; ++i) {
        mi = ei[i] == expectI[i];
    }
    check(mi, "encode int32 -1,258 little-endian byte pattern");

    // Encoding the same input twice is byte-for-byte identical.
    check(xq::BlobStore::encode(xq::BlobElementType::F64, ones) == enc,
          "encode is deterministic byte-for-byte");
}

// --- write/read round-trip --------------------------------------------------

void test_put_get_roundtrip(const fs::path& root)
{
    xq::BlobStore store(root.string());

    // F64: 4 points worth of coordinates (components=3).
    const std::vector<double> coords = {0.0, 1.5, -2.25, 3.0, 4.0, 5.0,
                                        -10.0, 0.125, 99.0, 1e-7, -1e8, 42.0};
    xq::BufferRef refD;
    check(store.put(xq::BlobElementType::F64, 3, 4, coords, &refD), "put f64 ok");
    check(refD.byteCount == coords.size() * 8, "f64 byteCount");
    check(refD.sha256.size() == 64, "f64 sha length");
    check(refD.elementCount == 4 && refD.components == 3, "f64 metadata");

    std::vector<double> backD;
    check(store.get(refD, &backD) == xq::BlobStore::Status::Ok, "get f64 ok");
    check(backD == coords, "f64 round-trip values exact");

    // I32: tets, 4 components.
    const std::vector<int> tets = {0, 1, 2, 3, 4, 5, 6, 7, -1, 100, 200, 300};
    xq::BufferRef refI;
    check(store.put(xq::BlobElementType::I32, 4, 3, tets, &refI), "put i32 ok");
    std::vector<int> backI;
    check(store.get(refI, &backI) == xq::BlobStore::Status::Ok, "get i32 ok");
    check(backI == tets, "i32 round-trip values exact");

    // U8: voxels.
    const std::vector<std::uint8_t> voxels = {0, 1, 2, 255, 128, 7, 0, 9};
    xq::BufferRef refU;
    check(store.put(xq::BlobElementType::U8, 1, 8, voxels, &refU), "put u8 ok");
    std::vector<std::uint8_t> backU;
    check(store.get(refU, &backU) == xq::BlobStore::Status::Ok, "get u8 ok");
    check(backU == voxels, "u8 round-trip values exact");

    // Empty blob round-trips.
    const std::vector<double> none;
    xq::BufferRef refE;
    check(store.put(xq::BlobElementType::F64, 3, 0, none, &refE), "put empty ok");
    check(refE.byteCount == 0, "empty byteCount 0");
    std::vector<double> backE;
    check(store.get(refE, &backE) == xq::BlobStore::Status::Ok, "get empty ok");
    check(backE.empty(), "empty round-trip");

    // Element-count / component mismatch rejected (no file written).
    xq::BufferRef bad;
    check(!store.put(xq::BlobElementType::F64, 3, 5, coords, &bad), "put rejects size mismatch");
    // Wrong typed overload for elementType rejected.
    check(!store.put(xq::BlobElementType::I32, 1, 1, std::vector<double>{1.0}, &bad),
          "put rejects type/width mismatch");
}

// --- dedup ------------------------------------------------------------------

void test_dedup(const fs::path& root)
{
    xq::BlobStore store(root.string());
    const std::vector<double> v = {7.0, 8.0, 9.0};
    xq::BufferRef a;
    xq::BufferRef b;
    check(store.put(xq::BlobElementType::F64, 1, 3, v, &a), "dedup put a");
    check(store.put(xq::BlobElementType::F64, 1, 3, v, &b), "dedup put b");
    check(a.sha256 == b.sha256 && a.relPath == b.relPath, "dedup same content same path");

    // Exactly one file under blobs/<prefix>/ for this content.
    const fs::path p = fs::path(root) / a.relPath;
    check(fs::exists(p), "dedup blob file exists");
}

// --- four structured errors -------------------------------------------------

void test_errors(const fs::path& root)
{
    xq::BlobStore store(root.string());
    const std::vector<int> data = {10, 20, 30, 40};
    xq::BufferRef ref;
    check(store.put(xq::BlobElementType::I32, 1, 4, data, &ref), "errors: put base");
    const fs::path blobPath = fs::path(root) / ref.relPath;

    // MissingBlob: reference a non-existent path.
    {
        xq::BufferRef missing = ref;
        missing.relPath = "blobs/zz/deadbeef.bin";
        std::vector<int> out;
        check(store.get(missing, &out) == xq::BlobStore::Status::MissingBlob,
              "error MissingBlob");
    }

    // InvalidMetadata: relPath must stay inside the asset root.
    {
        xq::BufferRef traversal = ref;
        traversal.relPath = "../outside.bin";
        std::vector<int> out;
        check(store.get(traversal, &out) == xq::BlobStore::Status::InvalidMetadata,
              "error InvalidMetadata for parent traversal relPath");
    }

    // ByteCountMismatch: keep metadata self-consistent but leave extra bytes on disk.
    {
        truncateFile(blobPath, ref.byteCount + 4);
        std::vector<int> out;
        check(store.get(ref, &out) == xq::BlobStore::Status::ByteCountMismatch,
              "error ByteCountMismatch");
        truncateFile(blobPath, ref.byteCount);
    }

    // InvalidMetadata: byteCount must also match logical elementCount *
    // components * byteWidth, even when the file size equals byteCount.
    {
        xq::BufferRef badShape = ref;
        badShape.elementCount = 2; // logical bytes = 8, file/byteCount = 16
        std::vector<int> out;
        check(store.get(badShape, &out) == xq::BlobStore::Status::InvalidMetadata,
              "error InvalidMetadata for logical byteCount mismatch");
    }

    // InvalidMetadata: multiplication overflow is rejected before allocation.
    {
        xq::BufferRef overflow = ref;
        overflow.elementCount = (std::numeric_limits<std::uint64_t>::max)();
        overflow.components = 2;
        overflow.byteCount = ref.byteCount;
        std::vector<int> out;
        check(store.get(overflow, &out) == xq::BlobStore::Status::InvalidMetadata,
              "error InvalidMetadata for logical byteCount overflow");
    }

    // InvalidMetadata: typed get overload must match the BufferRef element type.
    {
        std::vector<double> out;
        check(store.get(ref, &out) == xq::BlobStore::Status::InvalidMetadata,
              "error InvalidMetadata for typed overload mismatch");
    }

    // InvalidMetadata: unsupported BufferRef format/endian is rejected.
    {
        xq::BufferRef badVersion = ref;
        badVersion.formatVersion = 2;
        std::vector<int> out;
        check(store.get(badVersion, &out) == xq::BlobStore::Status::InvalidMetadata,
              "error InvalidMetadata for unsupported formatVersion");
    }

    // TruncatedBlob: shrink the file while keeping metadata self-consistent, so
    // the file is shorter than the declared logical byte count.
    {
        truncateFile(blobPath, 8); // 2 int32s of bytes on disk
        std::vector<int> out;
        const xq::BlobStore::Status st = store.get(ref, &out);
        check(st == xq::BlobStore::Status::TruncatedBlob, "error TruncatedBlob");
    }

    // ChecksumMismatch: flip a byte but keep byteCount/elementCount consistent.
    {
        // Rebuild a clean blob first (previous test truncated it).
        std::error_code ec;
        fs::remove_all(fs::path(root) / "blobs", ec);
        xq::BufferRef fresh;
        store.put(xq::BlobElementType::I32, 1, 4, data, &fresh);
        const fs::path freshPath = fs::path(root) / fresh.relPath;
        flipFirstByte(freshPath);
        std::vector<int> out;
        check(store.get(fresh, &out) == xq::BlobStore::Status::ChecksumMismatch,
              "error ChecksumMismatch");
    }
}

// --- junction escape (physical confinement) ---------------------------------

// A relPath can be lexically clean ("blobs/<prefix>/<sha>.bin") yet resolve
// outside the asset root when an intermediate directory is an NTFS junction
// pointing elsewhere. is_symlink does NOT catch junctions on Windows; only
// isPathWithinRoot (weakly_canonical prefix compare) does. This test proves the
// physical-confinement gate rejects such a blob instead of reading its content.
void test_junction_escape()
{
    // Real blob outside the asset root, whose bytes match a valid ref.
    const fs::path outside = fs::temp_directory_path() / "xq_blob_store_test.outside";
    std::error_code ec;
    fs::remove_all(outside, ec);
    fs::create_directories(outside, ec);

    const fs::path root = freshRoot();

    // Produce a valid blob inside a throwaway root so we get a real ref+content.
    xq::BufferRef ref;
    {
        const fs::path seed = fs::temp_directory_path() / "xq_blob_store_test.seed";
        fs::remove_all(seed, ec);
        fs::create_directories(seed, ec);
        xq::BlobStore seedStore(seed.string());
        const std::vector<int> data = {11, 22, 33, 44};
        check(seedStore.put(xq::BlobElementType::I32, 1, 4, data, &ref),
              "junction: seed put");
        // Copy the real blob bytes to the OUTSIDE directory under the same relPath,
        // so a followed junction would serve genuine, checksum-valid content.
        const fs::path seedBlob = seed / ref.relPath;
        const fs::path outBlob = outside / ref.relPath;
        fs::create_directories(outBlob.parent_path(), ec);
        fs::copy_file(seedBlob, outBlob, fs::copy_options::overwrite_existing, ec);
        check(!ec, "junction: copy blob to outside");
        fs::remove_all(seed, ec);
    }

    // Replace <root>/blobs with a junction to the outside directory's blobs.
    const fs::path rootBlobs = root / "blobs";
    fs::remove_all(rootBlobs, ec);
    const std::string cmd = "cmd /c mklink /J \"" + rootBlobs.string() + "\" \"" +
                            (outside / "blobs").string() + "\" >nul 2>&1";
    const int rc = std::system(cmd.c_str());
    if (rc != 0 || !fs::exists(rootBlobs)) {
        // Never silently pass: a junction we cannot create means the escape path
        // is untested. Record a visible failure rather than a fake green.
        check(false, "junction: cannot create junction (environment unsupported)");
        fs::remove_all(outside, ec);
        return;
    }

    // Sanity: without confinement this would resolve through the junction to the
    // real outside blob and return Ok. With confinement it must be rejected.
    xq::BlobStore store(root.string());
    std::vector<int> out;
    const xq::BlobStore::Status st = store.get(ref, &out);
    check(st != xq::BlobStore::Status::Ok,
          "junction: escaping blob is rejected (not read through junction)");
    check(st == xq::BlobStore::Status::MissingBlob,
          "junction: escape reported as MissingBlob");

    // Remove only the junction, never the outside target's contents.
    const std::string rm = "cmd /c rmdir \"" + rootBlobs.string() + "\" >nul 2>&1";
    std::system(rm.c_str());
    fs::remove_all(root, ec);
    fs::remove_all(outside, ec);
}

} // namespace

int main()
{
    test_sha256_vectors();
    test_sha256_chunked_equals_oneshot();
    test_encoding_byte_stable();

    const fs::path root = freshRoot();
    test_put_get_roundtrip(root);
    test_dedup(root);
    test_errors(root);

    std::error_code ec;
    fs::remove_all(root, ec);

    test_junction_escape();

    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all blob store checks passed" << std::endl;
    return 0;
}
