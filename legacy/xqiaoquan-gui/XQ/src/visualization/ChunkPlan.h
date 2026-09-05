#ifndef XQ_VISUALIZATION_CHUNK_PLAN_H
#define XQ_VISUALIZATION_CHUNK_PLAN_H

#include <algorithm>
#include <cstddef>
#include <vector>

namespace xq {

// One contiguous half-open cell-index range [begin, end) for a progressive
// upload chunk (M9b-C). Split out into this VTK-free header so the partition
// invariant (chunks tile [0, cellCount) exactly, no gaps/overlaps) can be unit
// tested directly -- a rendered-image test cannot catch a few dropped/duplicated
// cells (sub-pixel holes), so the plan is verified at the source.
struct ChunkRange {
    std::size_t begin = 0;
    std::size_t end = 0; // exclusive
};

// Splits [0, cellCount) into contiguous ranges of at most maxCellsPerChunk.
// Postcondition (the invariant tests rely on): the returned ranges tile
// [0, cellCount) exactly -- chunks[0].begin == 0, each chunk's end == the next
// chunk's begin, the last chunk's end == cellCount, and every chunk is
// non-empty. maxCellsPerChunk == 0 means "one chunk covering everything".
inline std::vector<ChunkRange> plan_chunks(std::size_t cellCount,
                                           std::size_t maxCellsPerChunk)
{
    std::vector<ChunkRange> chunks;
    if (cellCount == 0) {
        return chunks;
    }
    const std::size_t step = maxCellsPerChunk > 0 ? maxCellsPerChunk : cellCount;
    for (std::size_t begin = 0; begin < cellCount; begin += step) {
        const std::size_t end = std::min(begin + step, cellCount);
        chunks.push_back({begin, end});
    }
    return chunks;
}

} // namespace xq

#endif // XQ_VISUALIZATION_CHUNK_PLAN_H
