#include "core/XQImageVolume.h"
#include "core/source/IVoxelSource.h"
#include "io/blob/MerkleSidecar.h"
#include "io/blob/Sha256.h"
#include "io/source/MappedVoxelSource.h"

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
    const fs::path dir = fs::temp_directory_path() / "xq_mapped_voxel_source_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

void writeFile(const fs::path& p, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream f(p.string(), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

// dims 16x16x16, UInt16, 1 component -> 8192 bytes. segBytes 1024 -> 8 segments.
// A single slab (z fixed) = 256 voxels = 512 bytes, fully inside one segment.
constexpr int kDimX = 16;
constexpr int kDimY = 16;
constexpr int kDimZ = 16;
constexpr std::uint64_t kSegBytes = 1024;

std::vector<std::uint8_t> makeVoxelBlob()
{
    const std::size_t voxels =
        static_cast<std::size_t>(kDimX) * kDimY * kDimZ;
    std::vector<std::uint8_t> v(voxels * 2); // UInt16
    for (std::size_t i = 0; i < voxels; ++i) {
        const std::uint16_t val = static_cast<std::uint16_t>(i * 7 + 3);
        v[i * 2] = static_cast<std::uint8_t>(val & 0xFF);
        v[i * 2 + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
    }
    return v;
}

// AC3: whole-volume span is byte-for-byte the source blob; both integrity modes.
void test_whole_matches_source(const fs::path& dir)
{
    const std::vector<std::uint8_t> blob = makeVoxelBlob();
    const fs::path blobPath = dir / "vol.bin";
    writeFile(blobPath, blob);

    const int dims[3] = {kDimX, kDimY, kDimZ};

    // FullVerify (no sidecar).
    {
        xq::MappedVoxelSource src(blobPath.string(), dims, xq::ScalarType::UInt16,
                                  1, xq::MappedVoxelSource::IntegrityMode::FullVerify);
        check(src.valid(), "FullVerify source valid");

        const xq::VoxelMeta m = src.meta();
        check(m.valid && m.dims[0] == kDimX && m.dims[1] == kDimY
                  && m.dims[2] == kDimZ && m.components == 1,
              "FullVerify meta correct");

        xq::VoxelLease lease = src.acquire_whole();
        const xq::VoxelView& view = lease.view();
        check(view.valid, "FullVerify acquire_whole valid");
        check(view.bytes.size() == blob.size(), "whole span size == blob size");
        bool same = view.bytes.size() == blob.size();
        for (std::size_t i = 0; same && i < blob.size(); ++i) {
            same = view.bytes[i] == blob[i];
        }
        check(same, "FullVerify whole span == source bytes (AC3)");
    }

    // SegmentedMerkle (with sidecar).
    {
        const fs::path merklePath = dir / "vol.merkle";
        check(xq::write_sidecar(blob, kSegBytes, merklePath.string()),
              "write sidecar ok");

        xq::MappedVoxelSource src(
            blobPath.string(), dims, xq::ScalarType::UInt16, 1,
            xq::MappedVoxelSource::IntegrityMode::SegmentedMerkle,
            merklePath.string());
        check(src.valid(), "SegmentedMerkle source valid");

        xq::VoxelLease lease = src.acquire_whole();
        const xq::VoxelView& view = lease.view();
        check(view.valid, "SegmentedMerkle acquire_whole valid");
        bool same = view.bytes.size() == blob.size();
        for (std::size_t i = 0; same && i < blob.size(); ++i) {
            same = view.bytes[i] == blob[i];
        }
        check(same, "SegmentedMerkle whole span == source bytes (AC3)");
    }
}

// AC4: a single slab touches only its segment -> bytesHashed < whole-blob bytes.
void test_lazy_segment_hashing(const fs::path& dir)
{
    const std::vector<std::uint8_t> blob = makeVoxelBlob();
    const fs::path blobPath = dir / "lazy.bin";
    const fs::path merklePath = dir / "lazy.merkle";
    writeFile(blobPath, blob);
    check(xq::write_sidecar(blob, kSegBytes, merklePath.string()),
          "write sidecar ok (lazy)");

    const int dims[3] = {kDimX, kDimY, kDimZ};
    xq::MappedVoxelSource src(
        blobPath.string(), dims, xq::ScalarType::UInt16, 1,
        xq::MappedVoxelSource::IntegrityMode::SegmentedMerkle,
        merklePath.string());
    check(src.valid(), "lazy source valid");

    // Slab z=0 lives entirely in segment 0 (512 bytes < 1024 segment).
    xq::VoxelLease slab = src.acquire_slab(0);
    check(slab.view().valid, "acquire_slab(0) valid");

    const xq::MappedVoxelSource::IntegrityStats s = src.stats();
    check(s.bytesHashed < blob.size(),
          "slab hashes fewer bytes than whole blob (AC4 lazy)");
    check(s.segmentsChecked == 1, "slab touches exactly 1 segment");

    // Re-acquiring the same slab does not re-hash (memoized).
    xq::VoxelLease slabAgain = src.acquire_slab(0);
    check(slabAgain.view().valid, "re-acquire slab valid");
    const xq::MappedVoxelSource::IntegrityStats s2 = src.stats();
    check(s2.bytesHashed == s.bytesHashed, "memoized: no re-hash on 2nd touch");
}

// AC4: tampering a segment makes acquires that touch it return an invalid lease,
// while acquires on untouched segments stay valid.
void test_tamper_detection(const fs::path& dir)
{
    const std::vector<std::uint8_t> clean = makeVoxelBlob();
    const fs::path cleanBlob = dir / "clean.bin";
    const fs::path merklePath = dir / "clean.merkle";
    writeFile(cleanBlob, clean);
    check(xq::write_sidecar(clean, kSegBytes, merklePath.string()),
          "write sidecar ok (tamper)");

    // Make a tampered copy: flip a byte inside segment 7 (last). The sidecar
    // built from the clean blob is reused, so the touched segment must fail.
    std::vector<std::uint8_t> tampered = clean;
    const std::size_t tamperOffset =
        static_cast<std::size_t>(7 * kSegBytes) + 20; // inside segment 7
    tampered[tamperOffset] ^= 0x01;
    const fs::path tamperedBlob = dir / "tampered.bin";
    writeFile(tamperedBlob, tampered);

    const int dims[3] = {kDimX, kDimY, kDimZ};
    xq::MappedVoxelSource src(
        tamperedBlob.string(), dims, xq::ScalarType::UInt16, 1,
        xq::MappedVoxelSource::IntegrityMode::SegmentedMerkle,
        merklePath.string());
    // Root still verifies (sidecar is clean) so the source maps fine; the
    // mismatch is only caught lazily when the tampered segment is touched.
    check(src.valid(), "tampered-blob source still maps (clean sidecar root)");

    // Segment 7 byte range [7168, 8192) -> voxel 3584.. -> z = 14 and 15.
    // Slab z=14 starts at byte 14*256*2 = 7168 -> segment 7.
    xq::VoxelLease bad = src.acquire_slab(14);
    check(!bad.view().valid, "acquire touching tampered segment -> invalid (AC4)");

    // Slab z=0 lives in segment 0 (clean) -> still valid.
    xq::VoxelLease good = src.acquire_slab(0);
    check(good.view().valid, "acquire on untouched/clean segment still valid");
}

// AC3: out-of-range region / slab -> invalid lease.
void test_out_of_range(const fs::path& dir)
{
    const std::vector<std::uint8_t> blob = makeVoxelBlob();
    const fs::path blobPath = dir / "oob.bin";
    writeFile(blobPath, blob);

    const int dims[3] = {kDimX, kDimY, kDimZ};
    xq::MappedVoxelSource src(blobPath.string(), dims, xq::ScalarType::UInt16, 1,
                              xq::MappedVoxelSource::IntegrityMode::FullVerify);
    check(src.valid(), "oob source valid");

    const int badExtent[6] = {0, kDimX, 0, kDimY - 1, 0, kDimZ - 1}; // x1 == dimX
    xq::VoxelLease r = src.acquire_region(badExtent);
    check(!r.view().valid, "out-of-range region -> invalid lease");

    xq::VoxelLease s = src.acquire_slab(kDimZ); // z == dimZ
    check(!s.view().valid, "out-of-range slab -> invalid lease");

    const int negExtent[6] = {-1, 0, 0, 0, 0, 0};
    xq::VoxelLease rn = src.acquire_region(negExtent);
    check(!rn.view().valid, "negative region -> invalid lease");
}

// region materialization matches the source sub-block element by element.
void test_region_materialize(const fs::path& dir)
{
    const std::vector<std::uint8_t> blob = makeVoxelBlob();
    const fs::path blobPath = dir / "region.bin";
    writeFile(blobPath, blob);

    const int dims[3] = {kDimX, kDimY, kDimZ};
    xq::MappedVoxelSource src(blobPath.string(), dims, xq::ScalarType::UInt16, 1,
                              xq::MappedVoxelSource::IntegrityMode::FullVerify);
    check(src.valid(), "region source valid");

    const int extent[6] = {2, 5, 3, 7, 1, 4}; // inclusive
    xq::VoxelLease lease = src.acquire_region(extent);
    const xq::VoxelView& view = lease.view();
    check(view.valid, "region lease valid");
    check(view.dims[0] == 4 && view.dims[1] == 5 && view.dims[2] == 4,
          "region sub-dims correct");

    // Compare each materialized voxel to the source linear index.
    const std::size_t stride = 2; // UInt16, 1 component
    bool same = true;
    std::size_t dst = 0;
    for (int z = 1; same && z <= 4; ++z) {
        for (int y = 3; same && y <= 7; ++y) {
            for (int x = 2; same && x <= 5; ++x) {
                const std::size_t srcVoxel =
                    static_cast<std::size_t>(x)
                    + static_cast<std::size_t>(kDimX)
                    * (static_cast<std::size_t>(y)
                       + static_cast<std::size_t>(kDimY)
                       * static_cast<std::size_t>(z));
                for (std::size_t b = 0; b < stride; ++b) {
                    if (view.bytes[dst * stride + b]
                        != blob[srcVoxel * stride + b]) {
                        same = false;
                    }
                }
                ++dst;
            }
        }
    }
    check(same, "region materialized bytes == source sub-block");
}

// FullVerify with a content-address anchor (BufferRef.sha256): a correct anchor
// passes acquire; a blob tampered on disk under the stale anchor must fail the
// whole-blob verify so acquire_whole returns an invalid lease. Regression guard
// for the FullVerify integrity fix (digest was previously discarded).
void test_fullverify_anchor(const fs::path& dir)
{
    const std::vector<std::uint8_t> clean = makeVoxelBlob();
    const std::string sha = xq::Sha256::hashHex(clean);
    const int dims[3] = {kDimX, kDimY, kDimZ};

    // Correct anchor -> acquire_whole valid.
    {
        const fs::path bp = dir / "anchor_ok.bin";
        writeFile(bp, clean);
        xq::MappedVoxelSource src(bp.string(), dims, xq::ScalarType::UInt16, 1,
                                  xq::MappedVoxelSource::IntegrityMode::FullVerify,
                                  std::string(), sha);
        check(src.valid(), "anchored voxel source valid (correct sha)");
        check(src.acquire_whole().view().valid,
              "FullVerify matching anchor -> acquire valid");
    }

    // Tampered blob, stale anchor -> whole-blob digest mismatch -> invalid lease.
    {
        std::vector<std::uint8_t> tampered = clean;
        tampered[100] ^= 0x01;
        const fs::path bp = dir / "anchor_tampered.bin";
        writeFile(bp, tampered);
        xq::MappedVoxelSource src(bp.string(), dims, xq::ScalarType::UInt16, 1,
                                  xq::MappedVoxelSource::IntegrityMode::FullVerify,
                                  std::string(), sha); // stale anchor
        check(!src.acquire_whole().view().valid,
              "FullVerify anchor mismatch (tampered blob) -> invalid lease");
    }
}

} // namespace

int main()
{
    const fs::path dir = freshDir();

    test_whole_matches_source(dir);
    test_lazy_segment_hashing(dir);
    test_tamper_detection(dir);
    test_fullverify_anchor(dir);
    test_out_of_range(dir);
    test_region_materialize(dir);

    std::error_code ec;
    fs::remove_all(dir, ec);

    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all mapped voxel source checks passed" << std::endl;
    return 0;
}
