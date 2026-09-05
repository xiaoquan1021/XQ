#include "SyntheticLoad.h"

#include "io/blob/Sha256.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xq {
namespace probe {

namespace fs = std::filesystem;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Cheap deterministic 32-bit mixer (splitmix-style finalizer). Used for tiny
// coordinate jitter and voxel fill so the byte stream has real entropy (matters
// for SHA / mmap fault behavior) while staying fully reproducible.
inline std::uint32_t mix32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// --- streaming, content-addressed blob writer ------------------------------
//
// Replicates BlobStore byte-for-byte (encode + content addressing + atomic
// publish) WITHOUT holding the whole element vector and the whole encoded byte
// buffer at once. Scalars are pushed in (appendF64/I32/U8), buffered into a
// fixed-size chunk, and on each flush the chunk is both written to a temp file
// and fed to an incremental SHA-256. Because chunked and one-shot hashing of an
// identical byte stream yield an identical digest (Sha256 class contract), and
// because the emitted bytes are exactly BlobStore::encode's output, the final
// digest + file path + BufferRef match BlobStore::put exactly.
//
// Publish dance: BlobStore knows the sha before writing (it hashes the in-memory
// byte vector) so it can name the temp inside blobs/<prefix>/. We only learn the
// sha after the last byte, so we stream into a root-level temp file and rename
// into the content-addressed path at finalize(). The final file's bytes are
// identical; only the temp location differs (no observable effect on the blob).
class StreamingBlobWriter {
public:
    explicit StreamingBlobWriter(std::string assetRootDir)
        : rootDir_(std::move(assetRootDir))
    {
        buf_.reserve(kChunkBytes + 8);

        const fs::path root(rootDir_);
        fs::path blobsDir = root / "blobs";
        std::error_code ec;
        fs::create_directories(blobsDir, ec);
        if (ec) {
            throw std::runtime_error("StreamingBlobWriter: cannot create blobs dir");
        }
        // Root-level temp; renamed to its content-addressed home at finalize().
        tmpPath_ = (blobsDir / (".probe-stream-tmp-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".bin")).string();
        os_.open(tmpPath_, std::ios::binary | std::ios::trunc);
        if (!os_) {
            throw std::runtime_error("StreamingBlobWriter: cannot open temp file");
        }
    }

    // double -> 8 bytes LE of its IEEE-754 bit pattern (BlobStore.cpp:34-39,95).
    void appendF64(double v)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        appendLE(bits, 8);
    }

    // int -> 4 bytes two's-complement LE (BlobStore.cpp:109 with w==4).
    void appendI32(int v)
    {
        appendLE(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)), 4);
    }

    // uint8 -> raw byte (BlobStore::encode U8 overload is a passthrough copy).
    void appendU8(std::uint8_t v)
    {
        buf_.push_back(static_cast<unsigned char>(v));
        if (buf_.size() >= kChunkBytes) {
            flush();
        }
    }

    // Finalize: flush remainder, compute digest, move temp into
    // blobs/<sha[0:2]>/<sha>.bin (dedup-skip if present), fill BufferRef.
    BufferRef finalize(BlobElementType elementType, std::uint16_t components,
                       std::uint64_t elementCount)
    {
        flush();
        os_.flush();
        if (!os_) {
            throw std::runtime_error("StreamingBlobWriter: temp write failed");
        }
        os_.close();

        const std::string sha = sha_.finalHex();
        const std::string prefix = sha.substr(0, 2);
        const std::string relPath = "blobs/" + prefix + "/" + sha + ".bin";

        const fs::path dir = fs::path(rootDir_) / "blobs" / prefix;
        const fs::path finalPath = dir / (sha + ".bin");

        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            throw std::runtime_error("StreamingBlobWriter: cannot create prefix dir");
        }

        if (!fs::exists(finalPath, ec)) {
            fs::rename(tmpPath_, finalPath, ec);
            if (ec) {
                fs::remove(tmpPath_, ec);
                if (!fs::exists(finalPath)) {
                    throw std::runtime_error("StreamingBlobWriter: publish rename failed");
                }
            }
        } else {
            // Identical content already published: drop our temp (dedup).
            fs::remove(tmpPath_, ec);
        }

        BufferRef out;
        out.relPath = relPath;
        out.byteCount = byteCount_;
        out.sha256 = sha;
        out.formatVersion = 1;
        out.endianness = 0;
        out.elementType = elementType;
        out.components = components;
        out.elementCount = elementCount;
        return out;
    }

private:
    static constexpr std::size_t kChunkBytes = std::size_t(4) << 20; // 4 MiB

    void appendLE(std::uint64_t bits, std::size_t width)
    {
        for (std::size_t i = 0; i < width; ++i) {
            buf_.push_back(static_cast<unsigned char>((bits >> (8 * i)) & 0xFFu));
        }
        if (buf_.size() >= kChunkBytes) {
            flush();
        }
    }

    void flush()
    {
        if (buf_.empty()) {
            return;
        }
        os_.write(reinterpret_cast<const char*>(buf_.data()),
                  static_cast<std::streamsize>(buf_.size()));
        sha_.update(buf_.data(), buf_.size());
        byteCount_ += buf_.size();
        buf_.clear();
    }

    std::string rootDir_;
    std::string tmpPath_;
    std::ofstream os_;
    Sha256 sha_;
    std::vector<unsigned char> buf_;
    std::uint64_t byteCount_ = 0;
};

} // namespace

// --- scale presets ----------------------------------------------------------

LoadSpec specFor(Scale scale)
{
    LoadSpec s;
    if (scale == Scale::Small) {
        // ~1.0M tris, ~0.51M tets, 128^3 = ~2.1M voxels.
        s.surfCircumferential = 500;
        s.surfAxialBands = 1000;   // 500*1000*2 = 1,000,000 tris
        s.tetNx = 44;
        s.tetNy = 44;
        s.tetNz = 44;              // 44^3*6 = 511,104 tets
        s.voxDims[0] = 128;
        s.voxDims[1] = 128;
        s.voxDims[2] = 128;
    } else {
        // ~20.0M tris, ~10.02M tets, 512^3 = ~134M voxels (128 MiB).
        s.surfCircumferential = 2000;
        s.surfAxialBands = 5000;   // 2000*5000*2 = 20,000,000 tris
        s.tetNx = 120;
        s.tetNy = 120;
        s.tetNz = 116;             // 120*120*116*6 = 10,022,400 tets
        s.voxDims[0] = 512;
        s.voxDims[1] = 512;
        s.voxDims[2] = 512;
    }
    return s;
}

// --- geometry ---------------------------------------------------------------

GeometryBlobs generateGeometry(const std::string& assetRootDir, const LoadSpec& spec)
{
    GeometryBlobs out;

    const int C = spec.surfCircumferential;
    const int B = spec.surfAxialBands;     // axial bands
    const std::size_t ringCount = static_cast<std::size_t>(B) + 1;
    const std::size_t pointCount = ringCount * static_cast<std::size_t>(C);
    const std::size_t triCount = static_cast<std::size_t>(B)
                                 * static_cast<std::size_t>(C) * 2u;

    out.pointCount = pointCount;
    out.triCount = triCount;

    const double axialLength = 100.0;
    const double radius = 10.0;

    // points (F64x3): cylinder surface, ring-major. Index(r,c) = r*C + c.
    {
        StreamingBlobWriter w(assetRootDir);
        for (std::size_t r = 0; r < ringCount; ++r) {
            const double z = axialLength * static_cast<double>(r) / static_cast<double>(B);
            for (int c = 0; c < C; ++c) {
                const double ang = 2.0 * kPi * static_cast<double>(c) / static_cast<double>(C);
                const std::uint32_t idx = static_cast<std::uint32_t>(r * static_cast<std::size_t>(C) + static_cast<std::size_t>(c));
                const double jit = static_cast<double>(mix32(idx ^ spec.seed) & 0xFFFFu) / 65535.0 * 1e-3;
                w.appendF64(radius * std::cos(ang) + jit);
                w.appendF64(radius * std::sin(ang) + jit);
                w.appendF64(z + jit);
            }
        }
        out.points = w.finalize(BlobElementType::F64, 3, pointCount);
    }

    // tris (I32x3): two triangles per (band, circ) quad, indices into points.
    {
        StreamingBlobWriter w(assetRootDir);
        for (int b = 0; b < B; ++b) {
            const int r0 = b;
            const int r1 = b + 1;
            for (int c = 0; c < C; ++c) {
                const int c1 = (c + 1) % C;
                const int v00 = r0 * C + c;
                const int v01 = r0 * C + c1;
                const int v10 = r1 * C + c;
                const int v11 = r1 * C + c1;
                // tri A: v00, v10, v11 ; tri B: v00, v11, v01
                w.appendI32(v00); w.appendI32(v10); w.appendI32(v11);
                w.appendI32(v00); w.appendI32(v11); w.appendI32(v01);
            }
        }
        out.tris = w.finalize(BlobElementType::I32, 3, triCount);
    }

    // faceId (I32x1): one per triangle. Banded ids so values vary deterministically.
    {
        StreamingBlobWriter w(assetRootDir);
        for (std::size_t t = 0; t < triCount; ++t) {
            w.appendI32(static_cast<int>(t % 64u));
        }
        out.faceId = w.finalize(BlobElementType::I32, 1, triCount);
    }

    // --- tet volume: structured grid, 6 tets per cell (Freudenthal) ---------
    const int nx = spec.tetNx;
    const int ny = spec.tetNy;
    const int nz = spec.tetNz;
    const int px = nx + 1;
    const int py = ny + 1;
    const int pz = nz + 1;
    const std::size_t volPointCount = static_cast<std::size_t>(px)
                                      * static_cast<std::size_t>(py)
                                      * static_cast<std::size_t>(pz);
    const std::size_t tetCount = static_cast<std::size_t>(nx)
                                 * static_cast<std::size_t>(ny)
                                 * static_cast<std::size_t>(nz) * 6u;
    out.volPointCount = volPointCount;
    out.tetCount = tetCount;

    const double h = 1.0;

    // volPoints (F64x3): grid points, index(i,j,k) = i + px*(j + py*k).
    {
        StreamingBlobWriter w(assetRootDir);
        for (int k = 0; k < pz; ++k) {
            for (int j = 0; j < py; ++j) {
                for (int i = 0; i < px; ++i) {
                    const std::uint32_t idx = static_cast<std::uint32_t>(
                        static_cast<std::size_t>(i)
                        + static_cast<std::size_t>(px)
                        * (static_cast<std::size_t>(j) + static_cast<std::size_t>(py) * static_cast<std::size_t>(k)));
                    const double jit = static_cast<double>(mix32(idx ^ spec.seed) & 0xFFFFu) / 65535.0 * 1e-3;
                    w.appendF64(i * h + jit);
                    w.appendF64(j * h + jit);
                    w.appendF64(k * h + jit);
                }
            }
        }
        out.volPoints = w.finalize(BlobElementType::F64, 3, volPointCount);
    }

    // tets (I32x4): 6 tets per cube cell. Corner numbering (binary xyz):
    //   0:(i,j,k) 1:(i+1,j,k) 2:(i,j+1,k) 3:(i+1,j+1,k)
    //   4:(i,j,k+1) 5:(i+1,j,k+1) 6:(i,j+1,k+1) 7:(i+1,j+1,k+1)
    // Freudenthal split (6 tets sharing the 0-7 diagonal), all non-degenerate.
    {
        StreamingBlobWriter w(assetRootDir);
        auto vid = [px, py](int i, int j, int k) -> int {
            return i + px * (j + py * k);
        };
        // tet vertex quadruplets expressed as corner indices 0..7.
        static const int kTets[6][4] = {
            {0, 1, 3, 7},
            {0, 3, 2, 7},
            {0, 2, 6, 7},
            {0, 6, 4, 7},
            {0, 4, 5, 7},
            {0, 5, 1, 7},
        };
        for (int k = 0; k < nz; ++k) {
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    int corner[8];
                    corner[0] = vid(i, j, k);
                    corner[1] = vid(i + 1, j, k);
                    corner[2] = vid(i, j + 1, k);
                    corner[3] = vid(i + 1, j + 1, k);
                    corner[4] = vid(i, j, k + 1);
                    corner[5] = vid(i + 1, j, k + 1);
                    corner[6] = vid(i, j + 1, k + 1);
                    corner[7] = vid(i + 1, j + 1, k + 1);
                    for (int t = 0; t < 6; ++t) {
                        w.appendI32(corner[kTets[t][0]]);
                        w.appendI32(corner[kTets[t][1]]);
                        w.appendI32(corner[kTets[t][2]]);
                        w.appendI32(corner[kTets[t][3]]);
                    }
                }
            }
        }
        out.tets = w.finalize(BlobElementType::I32, 4, tetCount);
    }

    return out;
}

// --- voxels -----------------------------------------------------------------

VoxelBlob generateVoxels(const std::string& assetRootDir, const LoadSpec& spec)
{
    VoxelBlob out;
    out.dims[0] = spec.voxDims[0];
    out.dims[1] = spec.voxDims[1];
    out.dims[2] = spec.voxDims[2];
    out.scalarType = ScalarType::UInt8;

    const int dx = spec.voxDims[0];
    const int dy = spec.voxDims[1];
    const int dz = spec.voxDims[2];
    out.voxelCount = static_cast<std::size_t>(dx)
                     * static_cast<std::size_t>(dy)
                     * static_cast<std::size_t>(dz);

    // x-fastest, one z-slab buffered at a time (peak extra = dx*dy bytes).
    // Value = deterministic radial-ish pattern so the volume is non-trivial.
    StreamingBlobWriter w(assetRootDir);
    const int cx = dx / 2;
    const int cy = dy / 2;
    const int cz = dz / 2;
    std::vector<std::uint8_t> slab(static_cast<std::size_t>(dx) * static_cast<std::size_t>(dy));
    for (int z = 0; z < dz; ++z) {
        std::size_t o = 0;
        for (int y = 0; y < dy; ++y) {
            for (int x = 0; x < dx; ++x) {
                const int rx = x - cx;
                const int ry = y - cy;
                const int rz = z - cz;
                const std::uint32_t r2 = static_cast<std::uint32_t>(rx * rx + ry * ry + rz * rz);
                const std::uint32_t noise = mix32(r2 ^ spec.seed ^ static_cast<std::uint32_t>(z * 73856093));
                slab[o++] = static_cast<std::uint8_t>((r2 ^ (noise >> 24)) & 0xFFu);
            }
        }
        for (std::size_t i = 0; i < slab.size(); ++i) {
            w.appendU8(slab[i]);
        }
    }
    out.voxels = w.finalize(BlobElementType::U8, 1, out.voxelCount);

    return out;
}

} // namespace probe
} // namespace xq
