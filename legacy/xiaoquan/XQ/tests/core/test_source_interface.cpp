#include <core/GeometryTypes.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <core/source/IGeometrySource.h>
#include <core/source/IVoxelSource.h>
#include <core/source/ReadLease.h>
#include <core/source/ReadSpan.h>
#include <core/source/ResidentSurfaceSource.h>
#include <core/source/ResidentTetSource.h>
#include <core/source/ResidentVoxelSource.h>
#include <core/source/SourceViews.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

// --- builders ---------------------------------------------------------------

// Builds a UInt8 volume with value = voxel linear index (mod 256), 1 component.
std::shared_ptr<xq::XQMemoryImageBufferHandle> make_uint8_volume(int dx, int dy, int dz)
{
    const int dims[3] = {dx, dy, dz};
    const std::size_t n = static_cast<std::size_t>(dx) * dy * dz;
    std::vector<std::uint8_t> bytes(n);
    for (std::size_t i = 0; i < n; ++i) {
        bytes[i] = static_cast<std::uint8_t>(i & 0xFF);
    }
    return std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, dims, 1, std::move(bytes));
}

// Builds an Int16 volume with value = i*7 - 100 (native byte order).
std::shared_ptr<xq::XQMemoryImageBufferHandle> make_int16_volume(int dx, int dy, int dz)
{
    const int dims[3] = {dx, dy, dz};
    const std::size_t n = static_cast<std::size_t>(dx) * dy * dz;
    std::vector<std::uint8_t> bytes(n * sizeof(std::int16_t));
    for (std::size_t i = 0; i < n; ++i) {
        const std::int16_t v = static_cast<std::int16_t>(static_cast<int>(i) * 7 - 100);
        std::memcpy(bytes.data() + i * sizeof(std::int16_t), &v, sizeof(v));
    }
    return std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::Int16, dims, 1, std::move(bytes));
}

// Builds a Float32 volume with value = i*0.5f + 0.25f.
std::shared_ptr<xq::XQMemoryImageBufferHandle> make_float32_volume(int dx, int dy, int dz)
{
    const int dims[3] = {dx, dy, dz};
    const std::size_t n = static_cast<std::size_t>(dx) * dy * dz;
    std::vector<std::uint8_t> bytes(n * sizeof(float));
    for (std::size_t i = 0; i < n; ++i) {
        const float v = static_cast<float>(i) * 0.5f + 0.25f;
        std::memcpy(bytes.data() + i * sizeof(float), &v, sizeof(v));
    }
    return std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::Float32, dims, 1, std::move(bytes));
}

std::shared_ptr<xq::XQTriangleSurfaceGeometryHandle> make_surface()
{
    auto h = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    h->addPoint({0.0, 0.0, 0.0});
    h->addPoint({1.0, 0.0, 0.0});
    h->addPoint({0.0, 1.0, 0.0});
    h->addPoint({0.0, 0.0, 1.0});
    h->addTriangle(0, 1, 2, 11);
    h->addTriangle(0, 1, 3, 22);
    h->addTriangle(1, 2, 3, 33);
    return h;
}

std::shared_ptr<xq::XQTetVolumeMeshHandle> make_tet()
{
    auto h = std::make_shared<xq::XQTetVolumeMeshHandle>();
    h->addPoint({0.0, 0.0, 0.0});
    h->addPoint({1.0, 0.0, 0.0});
    h->addPoint({0.0, 1.0, 0.0});
    h->addPoint({0.0, 0.0, 1.0});
    h->addPoint({1.0, 1.0, 1.0});
    h->addTet(0, 1, 2, 3);
    h->addTet(1, 2, 3, 4);
    return h;
}

// --- AC7 counting wrapper ---------------------------------------------------

// Wraps an IVoxelSource and counts acquire_* virtual calls, to prove a whole
// traversal acquires exactly once (not O(N)).
class CountingVoxelSource : public xq::IVoxelSource {
public:
    explicit CountingVoxelSource(std::shared_ptr<xq::IVoxelSource> inner)
        : inner_(std::move(inner))
    {
    }

    xq::VoxelMeta meta() const override { return inner_->meta(); }

    xq::VoxelLease acquire_whole() const override
    {
        ++acquireCount;
        return inner_->acquire_whole();
    }
    xq::VoxelLease acquire_region(const int extent[6]) const override
    {
        ++acquireCount;
        return inner_->acquire_region(extent);
    }
    xq::VoxelLease acquire_slab(int z) const override
    {
        ++acquireCount;
        return inner_->acquire_slab(z);
    }

    mutable int acquireCount = 0;

private:
    std::shared_ptr<xq::IVoxelSource> inner_;
};

// === AC1: whole-block geometry equivalence ==================================
int test_ac1_geometry_whole_equivalence()
{
    auto surf = make_surface();
    xq::ResidentSurfaceSource ssrc(surf);

    auto pl = ssrc.acquire_points();
    if (pl.span().size() != surf->pointCount()) {
        return fail("AC1 surface point count");
    }
    for (std::size_t i = 0; i < surf->pointCount(); ++i) {
        const xq::Point3& a = pl.span()[i];
        const xq::Point3& b = surf->point(i);
        if (!close(a.x, b.x) || !close(a.y, b.y) || !close(a.z, b.z)) {
            return fail("AC1 surface point value");
        }
    }

    auto tl = ssrc.acquire_triangles();
    if (tl.view().triangles.size() != surf->triangleCount()) {
        return fail("AC1 triangle count");
    }
    for (std::size_t i = 0; i < surf->triangleCount(); ++i) {
        if (tl.view().triangles[i] != surf->triangle(i)) {
            return fail("AC1 triangle value");
        }
    }

    auto tet = make_tet();
    xq::ResidentTetSource tsrc(tet);
    auto tetl = tsrc.acquire_tetrahedra();
    if (tetl.span().size() != tet->tetCount()) {
        return fail("AC1 tet count");
    }
    for (std::size_t i = 0; i < tet->tetCount(); ++i) {
        if (tetl.span()[i] != tet->tet(i)) {
            return fail("AC1 tet value");
        }
    }
    return 0;
}

// === AC2: faceId parallel conservation ======================================
int test_ac2_faceid_parallel()
{
    auto surf = make_surface();
    xq::ResidentSurfaceSource ssrc(surf);
    auto tl = ssrc.acquire_triangles();

    if (tl.view().faceIds.size() != tl.view().triangles.size()) {
        return fail("AC2 faceId length == triangle length");
    }
    for (std::size_t i = 0; i < surf->triangleCount(); ++i) {
        if (tl.view().faceIds[i] != surf->triangleFaceId(i)) {
            return fail("AC2 faceId value matches handle");
        }
    }
    // M9b-A: faceIds are borrowed zero-copy from the handle's contiguous buffer,
    // not materialized into a fresh vector. The span must point straight at the
    // handle's storage.
    if (tl.view().faceIds.data() != surf->triangleFaceIds().data()) {
        return fail("AC2 faceId span borrows handle buffer (zero-copy, no materialize)");
    }
    return 0;
}

// === AC3: voxel layout (x-fastest) across scalar types ======================
int test_ac3_voxel_layout()
{
    struct Case {
        std::shared_ptr<xq::XQMemoryImageBufferHandle> h;
        const char* name;
    };
    Case cases[3] = {
        {make_uint8_volume(3, 4, 2), "UInt8"},
        {make_int16_volume(3, 4, 2), "Int16"},
        {make_float32_volume(3, 4, 2), "Float32"},
    };

    for (const Case& c : cases) {
        xq::ResidentVoxelSource src(c.h);
        auto lease = src.acquire_whole();
        const xq::VoxelView& v = lease.view();
        if (!v.valid) {
            return fail("AC3 whole view valid");
        }
        for (int z = 0; z < c.h->dimensionZ(); ++z) {
            for (int y = 0; y < c.h->dimensionY(); ++y) {
                for (int x = 0; x < c.h->dimensionX(); ++x) {
                    const std::size_t hi = c.h->voxelIndex(x, y, z);
                    const std::size_t vi = v.voxelIndex(x, y, z);
                    if (hi != vi) {
                        return fail("AC3 voxelIndex matches handle");
                    }
                    if (!close(v.scalarAt(vi), c.h->scalarAt(hi))) {
                        return fail("AC3 scalarAt matches handle");
                    }
                }
            }
        }
    }
    return 0;
}

// === AC4: region correctness + out-of-range ================================
int test_ac4_region()
{
    auto h = make_int16_volume(5, 4, 3);
    xq::ResidentVoxelSource src(h);
    auto whole = src.acquire_whole();

    // Interior + boundary-touching inclusive extent.
    const int extent[6] = {1, 4, 0, 2, 1, 2};
    auto region = src.acquire_region(extent);
    const xq::VoxelView& rv = region.view();
    if (!rv.valid) {
        return fail("AC4 region valid");
    }
    const int subX = extent[1] - extent[0] + 1;
    const int subY = extent[3] - extent[2] + 1;
    const int subZ = extent[5] - extent[4] + 1;
    if (rv.dims[0] != subX || rv.dims[1] != subY || rv.dims[2] != subZ) {
        return fail("AC4 region dims == inclusive extent");
    }
    if (rv.voxelCount() != static_cast<std::size_t>(subX) * subY * subZ) {
        return fail("AC4 region voxel count == volume");
    }
    for (int z = 0; z < subZ; ++z) {
        for (int y = 0; y < subY; ++y) {
            for (int x = 0; x < subX; ++x) {
                const double got = rv.scalarAt(rv.voxelIndex(x, y, z));
                const double want = whole.view().scalarAt(
                    whole.view().voxelIndex(x + extent[0], y + extent[2], z + extent[4]));
                if (!close(got, want)) {
                    return fail("AC4 region content == whole sub-region");
                }
            }
        }
    }

    // Out-of-range extents -> invalid lease (valid == false), no exception.
    const int oob_hi[6] = {0, 5, 0, 3, 0, 2}; // x1=5 == dimX out of range
    if (src.acquire_region(oob_hi).view().valid) {
        return fail("AC4 out-of-range high extent invalid");
    }
    const int oob_neg[6] = {-1, 2, 0, 3, 0, 2};
    if (src.acquire_region(oob_neg).view().valid) {
        return fail("AC4 negative extent invalid");
    }
    const int oob_order[6] = {3, 1, 0, 3, 0, 2}; // x1 < x0
    if (src.acquire_region(oob_order).view().valid) {
        return fail("AC4 inverted extent invalid");
    }
    return 0;
}

// === AC5: slab correctness + out-of-range ===================================
int test_ac5_slab()
{
    auto h = make_uint8_volume(4, 3, 5);
    xq::ResidentVoxelSource src(h);
    auto whole = src.acquire_whole();

    for (int z = 0; z < h->dimensionZ(); ++z) {
        auto slab = src.acquire_slab(z);
        const xq::VoxelView& sv = slab.view();
        if (!sv.valid) {
            return fail("AC5 slab valid");
        }
        if (sv.dims[0] != h->dimensionX() || sv.dims[1] != h->dimensionY()
            || sv.dims[2] != 1) {
            return fail("AC5 slab dims == dimX x dimY x 1");
        }
        if (sv.voxelCount() != static_cast<std::size_t>(h->dimensionX()) * h->dimensionY()) {
            return fail("AC5 slab voxel count");
        }
        for (int y = 0; y < h->dimensionY(); ++y) {
            for (int x = 0; x < h->dimensionX(); ++x) {
                const double got = sv.scalarAt(sv.voxelIndex(x, y, 0));
                const double want =
                    whole.view().scalarAt(whole.view().voxelIndex(x, y, z));
                if (!close(got, want)) {
                    return fail("AC5 slab content == whole z plane");
                }
            }
        }
    }

    if (src.acquire_slab(-1).view().valid) {
        return fail("AC5 negative slab invalid");
    }
    if (src.acquire_slab(h->dimensionZ()).view().valid) {
        return fail("AC5 out-of-range slab invalid");
    }
    return 0;
}

// === AC6: meta without materialization ======================================
int test_ac6_meta()
{
    auto h = make_int16_volume(6, 7, 8);
    xq::ResidentVoxelSource vsrc(h);
    xq::VoxelMeta vm = vsrc.meta();
    if (!vm.valid || vm.type != xq::ScalarType::Int16 || vm.components != 1) {
        return fail("AC6 voxel meta type/components");
    }
    if (vm.dims[0] != 6 || vm.dims[1] != 7 || vm.dims[2] != 8) {
        return fail("AC6 voxel meta dims");
    }
    if (vm.voxelCount != static_cast<std::size_t>(6 * 7 * 8)) {
        return fail("AC6 voxel meta count");
    }

    auto surf = make_surface();
    xq::ResidentSurfaceSource ssrc(surf);
    xq::GeometryMeta sm = ssrc.meta();
    if (!sm.valid || sm.pointCount != surf->pointCount()
        || sm.triangleCount != surf->triangleCount() || sm.tetCount != 0) {
        return fail("AC6 surface meta");
    }

    auto tet = make_tet();
    xq::ResidentTetSource tsrc(tet);
    xq::GeometryMeta tm = tsrc.meta();
    if (!tm.valid || tm.pointCount != tet->pointCount()
        || tm.tetCount != tet->tetCount() || tm.triangleCount != 0) {
        return fail("AC6 tet meta");
    }
    return 0;
}

// === AC7: one acquire per whole traversal ===================================
int test_ac7_single_acquire()
{
    auto h = make_uint8_volume(8, 8, 4);
    auto inner = std::make_shared<xq::ResidentVoxelSource>(h);
    CountingVoxelSource counting(inner);

    auto lease = counting.acquire_whole();
    const xq::VoxelView& v = lease.view();
    double sum = 0.0;
    for (std::size_t i = 0; i < v.voxelCount(); ++i) {
        sum += v.scalarAt(i); // in-view access, no virtual dispatch
    }
    (void)sum;

    if (counting.acquireCount != 1) {
        return fail("AC7 whole traversal acquires exactly once");
    }
    return 0;
}

// === AC8: lease outlives adapter ============================================
int test_ac8_lease_lifetime()
{
    auto h = make_int16_volume(3, 3, 3);
    auto src = std::make_unique<xq::ResidentVoxelSource>(h);
    auto lease = src->acquire_whole();

    // Destroy the adapter; the borrowed view must stay valid via keepalive.
    src.reset();

    const xq::VoxelView& v = lease.view();
    if (!v.valid) {
        return fail("AC8 view valid after adapter reset");
    }
    if (!close(v.scalarAt(0), h->scalarAt(0))) {
        return fail("AC8 borrowed data readable after adapter reset");
    }

    // Move the lease; the moved-to lease keeps a valid view.
    xq::VoxelLease moved = std::move(lease);
    if (!close(moved.view().scalarAt(v.voxelCount() - 1),
               h->scalarAt(v.voxelCount() - 1))) {
        return fail("AC8 view valid after lease move");
    }

    // Geometry borrow: span survives adapter destruction too.
    auto surf = make_surface();
    auto ssrc = std::make_unique<xq::ResidentSurfaceSource>(surf);
    auto pl = ssrc->acquire_points();
    ssrc.reset();
    if (pl.span().size() != surf->pointCount()) {
        return fail("AC8 geometry span valid after adapter reset");
    }
    return 0;
}

// === AC9: read-only contract (compile-time) =================================
int test_ac9_readonly_contract()
{
    // ReadSpan element access yields const references; not assignable.
    static_assert(
        std::is_same<decltype(std::declval<xq::ReadSpan<int>>()[0]), const int&>::value,
        "ReadSpan::operator[] must return const reference");
    static_assert(
        std::is_same<decltype(std::declval<xq::ReadSpan<int>>().data()), const int*>::value,
        "ReadSpan::data must return const pointer");
    static_assert(!std::is_copy_constructible<xq::VoxelLease>::value,
                  "VoxelLease must be move-only");
    static_assert(!std::is_copy_constructible<xq::TriangleLease>::value,
                  "TriangleLease must be move-only");
    static_assert(!std::is_copy_constructible<xq::GeometryLease<xq::Point3>>::value,
                  "GeometryLease must be move-only");
    return 0;
}

// === AC10: external-library feed shape (contiguous reinterpret) =============
int test_ac10_external_feed()
{
    auto surf = make_surface();
    xq::ResidentSurfaceSource ssrc(surf);
    auto pl = ssrc.acquire_points();

    // reinterpret_cast<const double*>(points.data()) -> flat 3*n REAL array.
    const double* reals = reinterpret_cast<const double*>(pl.span().data());
    for (std::size_t i = 0; i < pl.span().size(); ++i) {
        const xq::Point3& p = pl.span()[i];
        if (!close(reals[3 * i + 0], p.x) || !close(reals[3 * i + 1], p.y)
            || !close(reals[3 * i + 2], p.z)) {
            return fail("AC10 Point3 reinterpret to double[3n]");
        }
    }

    // Triangle = array<int,3> -> flat int[3n].
    auto tl = ssrc.acquire_triangles();
    const int* tints = reinterpret_cast<const int*>(tl.view().triangles.data());
    for (std::size_t i = 0; i < tl.view().triangles.size(); ++i) {
        const xq::SourceTriangle& t = tl.view().triangles[i];
        for (int k = 0; k < 3; ++k) {
            if (tints[3 * i + k] != t[k]) {
                return fail("AC10 Triangle reinterpret to int[3n]");
            }
        }
    }

    // Tet = array<int,4> -> flat int[4n].
    auto tet = make_tet();
    xq::ResidentTetSource tsrc(tet);
    auto tetl = tsrc.acquire_tetrahedra();
    const int* tetints = reinterpret_cast<const int*>(tetl.span().data());
    for (std::size_t i = 0; i < tetl.span().size(); ++i) {
        const xq::SourceTet& q = tetl.span()[i];
        for (int k = 0; k < 4; ++k) {
            if (tetints[4 * i + k] != q[k]) {
                return fail("AC10 Tet reinterpret to int[4n]");
            }
        }
    }
    return 0;
}

// === AC11: missing-kind empty leases (no regression on partial sources) =====
int test_ac11_missing_kinds()
{
    // Surface source has no tets.
    auto surf = make_surface();
    xq::ResidentSurfaceSource ssrc(surf);
    if (!ssrc.acquire_tetrahedra().span().empty()) {
        return fail("AC11 surface acquire_tetrahedra empty");
    }

    // Tet source has no triangles.
    auto tet = make_tet();
    xq::ResidentTetSource tsrc(tet);
    auto tl = tsrc.acquire_triangles();
    if (!tl.view().triangles.empty() || !tl.view().faceIds.empty()) {
        return fail("AC11 tet acquire_triangles empty");
    }
    return 0;
}

} // namespace

int main()
{
    if (int r = test_ac1_geometry_whole_equivalence()) {
        return r;
    }
    if (int r = test_ac2_faceid_parallel()) {
        return r;
    }
    if (int r = test_ac3_voxel_layout()) {
        return r;
    }
    if (int r = test_ac4_region()) {
        return r;
    }
    if (int r = test_ac5_slab()) {
        return r;
    }
    if (int r = test_ac6_meta()) {
        return r;
    }
    if (int r = test_ac7_single_acquire()) {
        return r;
    }
    if (int r = test_ac8_lease_lifetime()) {
        return r;
    }
    if (int r = test_ac9_readonly_contract()) {
        return r;
    }
    if (int r = test_ac10_external_feed()) {
        return r;
    }
    if (int r = test_ac11_missing_kinds()) {
        return r;
    }
    return 0;
}
