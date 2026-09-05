#ifndef XQ_PROBE_SYNTHETIC_LOAD_H
#define XQ_PROBE_SYNTHETIC_LOAD_H

// scale-probe — deterministic synthetic load generator (task 06-30-scale-probe,
// R1). Disposable spike code; only compiles under -DXQ_ENABLE_SCALE_PROBE=ON.
//
// Produces real-scale geometry (up to 20M triangles / 10M tets) and a large
// uint8 voxel volume, written as M8a-format content-addressed sidecar blobs.
// The blobs are *byte-for-byte identical* to what BlobStore::put would emit, but
// are produced by a streaming writer (chunked ofstream + incremental SHA-256) so
// peak memory stays bounded — BlobStore::put holds the full element vector AND
// the full encoded byte vector simultaneously, which blows up at 20M scale
// (see design.md §1.2). Everything here is deterministic (fixed seed) so a run
// is reproducible and the produced SHA can be diffed against BlobStore::put on a
// small slice (see REPORT / verification notes).
//
// Role / components conventions are pinned to the mainline writer
// (XQProjectWriter.cpp:750-827): points F64x3, tris I32x3, faceId I32x1,
// volPoints F64x3, tets I32x4, voxels U8x1.

#include "core/XQImageVolume.h"        // ScalarType
#include "core/asset/BufferRef.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace xq {
namespace probe {

// Two fixed scale presets. Small = smoke (~1M tris / ~0.5M tets / 128^3 voxels);
// Large = the real target (~20M tris / ~10M tets / 512^3 voxels).
enum class Scale {
    Small,
    Large
};

// Explicit knobs behind the presets, exposed so a caller can also drive a custom
// scale. All counts are derived from these and are guaranteed to fit int32 index
// space at both presets (point index < 2^31).
struct LoadSpec {
    // Surface: a subdivided cylinder ("tube wall"). triCount = axialBands *
    // circumferential * 2; pointCount = (axialBands + 1) * circumferential.
    int surfCircumferential = 0;
    int surfAxialBands = 0;

    // Tet volume: a structured cube grid, each cell split into 6 tets
    // (Freudenthal). tetCount = nx*ny*nz*6; volPointCount = (nx+1)(ny+1)(nz+1).
    int tetNx = 0;
    int tetNy = 0;
    int tetNz = 0;

    // Voxel volume: uint8, x-fastest. dims[0]*dims[1]*dims[2] bytes.
    int voxDims[3] = {0, 0, 0};

    // Deterministic perturbation seed for coordinates / voxel values.
    std::uint32_t seed = 0x5ca1 ;
};

LoadSpec specFor(Scale scale);

// The five geometry blobs plus their element counts. Each BufferRef is filled
// exactly as BlobStore::publish would (relPath, byteCount, sha256,
// formatVersion=1, endianness=0, elementType, components, elementCount).
struct GeometryBlobs {
    BufferRef points;     // F64x3, surface vertices
    BufferRef tris;       // I32x3, triangle vertex indices (into points)
    BufferRef faceId;     // I32x1, one face id per triangle
    BufferRef volPoints;  // F64x3, tet-mesh vertices
    BufferRef tets;       // I32x4, tet vertex indices (into volPoints)

    std::size_t pointCount = 0;
    std::size_t triCount = 0;
    std::size_t volPointCount = 0;
    std::size_t tetCount = 0;
};

struct VoxelBlob {
    BufferRef voxels;            // U8x1
    int dims[3] = {0, 0, 0};
    ScalarType scalarType = ScalarType::UInt8;
    std::size_t voxelCount = 0;
};

// Generate the geometry assets and stream them to content-addressed blobs under
// <assetRootDir>/blobs/<sha[0:2]>/<sha>.bin. Memory stays bounded: points /
// tris / faceId / tets are produced in chunks and never fully materialized.
GeometryBlobs generateGeometry(const std::string& assetRootDir, const LoadSpec& spec);

// Generate the voxel volume and stream it to a content-addressed blob. Written
// one z-slab at a time, so peak extra memory is one slab (dims[0]*dims[1]).
VoxelBlob generateVoxels(const std::string& assetRootDir, const LoadSpec& spec);

} // namespace probe
} // namespace xq

#endif // XQ_PROBE_SYNTHETIC_LOAD_H
