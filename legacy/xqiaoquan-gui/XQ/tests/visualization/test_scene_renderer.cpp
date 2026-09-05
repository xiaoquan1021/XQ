#include <core/GeometryTypes.h>
#include <core/XQFlowResult.h>
#include <core/XQFlowResultPayload.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQSegmentationMask.h>
#include <core/XQSegmentationMaskPayload.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <visualization/XQSceneRenderer.h>

#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

bool any_non_black(const std::vector<unsigned char>& rgba)
{
    // RGB channels only; alpha is forced to 255, so it never proves a render.
    for (std::size_t i = 0; i + 2 < rgba.size(); i += 4) {
        if (rgba[i] != 0 || rgba[i + 1] != 0 || rgba[i + 2] != 0) {
            return true;
        }
    }
    return false;
}

// Unit tetrahedron with its 4 triangular faces (consistent winding).
xq::XQTriangleSurfaceGeometryHandle make_tet_surface()
{
    xq::XQTriangleSurfaceGeometryHandle surface;
    surface.addPoint({0.0, 0.0, 0.0});
    surface.addPoint({1.0, 0.0, 0.0});
    surface.addPoint({0.0, 1.0, 0.0});
    surface.addPoint({0.0, 0.0, 1.0});
    surface.addTriangle(0, 2, 1, 1);
    surface.addTriangle(0, 1, 3, 1);
    surface.addTriangle(0, 3, 2, 1);
    surface.addTriangle(1, 2, 3, 2);
    return surface;
}

xq::ImageGeometry make_geometry(int nx, int ny, int nz)
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = nx;
    geometry.dimensions[1] = ny;
    geometry.dimensions[2] = nz;
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 1.0;
    geometry.origin[0] = 0.0;
    geometry.origin[1] = 0.0;
    geometry.origin[2] = 0.0;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            geometry.direction[row][column] = row == column ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

} // namespace

int main()
{
    const int width = 128;
    const int height = 96;

    // --- surface (consistent winding) ---
    {
        xq::XQTriangleSurfaceGeometryHandle surface = make_tet_surface();
        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addSurface(surface);
        if (!added.ok) {
            return fail("addSurface succeeds for a tetrahedron surface");
        }
        if (added.actorCount != 1) {
            return fail("addSurface assembles exactly one actor");
        }
        if (added.pointCount != 4) {
            return fail("addSurface reports the input point count");
        }

        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("renderOffscreenToRgba succeeds for a surface scene");
        }
        if (rgba.size() != static_cast<std::size_t>(width) * height * 4U) {
            return fail("surface RGBA buffer is width*height*4 bytes");
        }
        if (!any_non_black(rgba)) {
            return fail("surface render produces a non-black image");
        }
    }

    // --- surface with deliberately inconsistent winding (ironrule 3) ---
    {
        xq::XQTriangleSurfaceGeometryHandle surface;
        surface.addPoint({0.0, 0.0, 0.0});
        surface.addPoint({1.0, 0.0, 0.0});
        surface.addPoint({0.0, 1.0, 0.0});
        surface.addPoint({0.0, 0.0, 1.0});
        // Two faces wound outward, two wound the opposite way: the input is NOT
        // globally consistent. addSurface must auto-orient (not crash, still
        // assemble) -- this exercises the orientation path.
        surface.addTriangle(0, 2, 1, 1);
        surface.addTriangle(0, 1, 3, 1);
        surface.addTriangle(0, 2, 3, 1); // flipped vs. 0,3,2
        surface.addTriangle(1, 3, 2, 2); // flipped vs. 1,2,3

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addSurface(surface);
        if (!added.ok) {
            return fail("addSurface assembles an actor for inconsistent winding");
        }
        if (added.actorCount != 1) {
            return fail("inconsistent-winding surface assembles one actor");
        }

        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("inconsistent-winding surface renders without error");
        }
        if (!any_non_black(rgba)) {
            return fail("inconsistent-winding surface render is non-black");
        }
    }

    // --- volume mesh (one tet) ---
    {
        xq::XQTetVolumeMeshHandle mesh;
        mesh.addPoint({0.0, 0.0, 0.0});
        mesh.addPoint({1.0, 0.0, 0.0});
        mesh.addPoint({0.0, 1.0, 0.0});
        mesh.addPoint({0.0, 0.0, 1.0});
        mesh.addTet(0, 1, 2, 3);

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addVolumeMesh(mesh);
        if (!added.ok) {
            return fail("addVolumeMesh succeeds for a single tet");
        }
        if (added.actorCount != 1) {
            return fail("addVolumeMesh assembles one actor");
        }
        if (added.pointCount != 4) {
            return fail("addVolumeMesh reports the input point count");
        }

        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("volume mesh renders offscreen");
        }
        if (!any_non_black(rgba)) {
            return fail("volume mesh render is non-black");
        }
    }

    // --- invalid surface connectivity is rejected before VTK upload ---
    {
        xq::XQTriangleSurfaceGeometryHandle surface;
        surface.addPoint({0.0, 0.0, 0.0});
        surface.addPoint({1.0, 0.0, 0.0});
        surface.addPoint({0.0, 1.0, 0.0});
        surface.addTriangle(0, 1, 3, 1); // 3 is outside [0, pointCount)

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addSurface(surface);
        if (added.ok) {
            return fail("invalid surface connectivity is rejected");
        }
        if (added.actorCount != 0) {
            return fail("invalid surface connectivity adds no actor");
        }
    }

    // --- invalid tet connectivity is rejected before VTK upload ---
    {
        xq::XQTetVolumeMeshHandle mesh;
        mesh.addPoint({0.0, 0.0, 0.0});
        mesh.addPoint({1.0, 0.0, 0.0});
        mesh.addPoint({0.0, 1.0, 0.0});
        mesh.addPoint({0.0, 0.0, 1.0});
        mesh.addTet(0, 1, 2, 4); // 4 is outside [0, pointCount)

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addVolumeMesh(mesh);
        if (added.ok) {
            return fail("invalid tet connectivity is rejected");
        }
        if (added.actorCount != 0) {
            return fail("invalid tet connectivity adds no actor");
        }
    }

    // --- path ---
    {
        xq::XQPath path;
        std::vector<xq::PathControlPoint> controls = {
            {{0.0, 0.0, 0.0}}, {{2.0, 0.0, 0.0}}, {{2.0, 2.0, 0.0}}, {{2.0, 2.0, 2.0}}};
        path.setControlPoints(controls);
        xq::XQPathPayload payload(path);

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addPath(payload);
        if (!added.ok) {
            return fail("addPath succeeds for a control-point path");
        }
        if (added.actorCount != 1) {
            return fail("addPath assembles one actor");
        }
        if (added.pointCount != 4) {
            return fail("addPath reports the control point count");
        }

        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("path renders offscreen");
        }
        if (!any_non_black(rgba)) {
            return fail("path render is non-black");
        }
    }

    // --- image slice with a real scalar buffer ---
    {
        const int nx = 8;
        const int ny = 6;
        const int nz = 4;
        xq::XQImageVolume volume;
        volume.setGeometry(make_geometry(nx, ny, nz));
        volume.setScalarType(xq::ScalarType::UInt8);
        volume.setComponentCount(1);
        volume.setIntensityRange({0.0, 255.0});
        volume.setWindowCenter(128.0);
        volume.setWindowWidth(255.0);

        const int dims[3] = {nx, ny, nz};
        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(nx) * ny * nz, 0);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            bytes[i] = static_cast<std::uint8_t>((i * 7) % 256);
        }
        xq::XQMemoryImageBufferHandle buffer(
            xq::ScalarType::UInt8, dims, 1, std::move(bytes));
        if (!buffer.is_valid()) {
            return fail("synthetic image buffer is valid");
        }

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addImageSlice(volume, &buffer, 2, nz / 2);
        if (!added.ok) {
            return fail("addImageSlice succeeds with a real buffer");
        }
        if (added.actorCount != 1) {
            return fail("addImageSlice assembles one slice prop");
        }
        if (added.pointCount != static_cast<long long>(nx) * ny * nz) {
            return fail("addImageSlice reports the image point count");
        }

        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("image slice renders offscreen");
        }
        if (!any_non_black(rgba)) {
            return fail("image slice render is non-black");
        }
    }

    // --- image slice degraded (null buffer) still assembles ---
    {
        xq::XQImageVolume volume;
        volume.setGeometry(make_geometry(8, 8, 4));
        volume.setIntensityRange({0.0, 255.0});

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addImageSlice(volume, nullptr, 2, 1);
        if (!added.ok) {
            return fail("addImageSlice degrades (null buffer) but still assembles");
        }
        if (added.actorCount != 1) {
            return fail("degraded image slice assembles one prop");
        }
    }

    // --- segmentation mask ---
    {
        const int dims[3] = {6, 6, 4};
        xq::XQSegmentationMask mask(dims);
        if (!mask.is_valid()) {
            return fail("synthetic mask is valid");
        }
        mask.setGeometry(make_geometry(6, 6, 4));
        // Light up a small foreground block.
        for (int z = 1; z <= 2; ++z) {
            for (int y = 1; y <= 2; ++y) {
                for (int x = 1; x <= 2; ++x) {
                    mask.setLabelAt(mask.voxelIndex(x, y, z), 1);
                }
            }
        }
        const std::size_t foreground = mask.foregroundVoxelCount();
        if (foreground == 0) {
            return fail("mask has foreground voxels");
        }
        xq::XQSegmentationMaskPayload payload(mask);

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addSegmentationMask(payload);
        if (!added.ok) {
            return fail("addSegmentationMask succeeds for a foreground block");
        }
        if (added.actorCount != 1) {
            return fail("addSegmentationMask assembles one actor");
        }
        if (added.pointCount != static_cast<long long>(foreground)) {
            return fail("addSegmentationMask reports the foreground voxel count");
        }
    }

    // --- flow result (schematic) ---
    {
        xq::XQFlowResult result;
        std::vector<double> times = {0.0, 0.5, 1.0};
        result.setTimes(times);
        for (int s = 0; s < 3; ++s) {
            xq::FlowSegment segment;
            segment.segmentId = s;
            segment.arcLengthStart = static_cast<double>(s);
            segment.arcLengthEnd = static_cast<double>(s + 1);
            result.addSegment(segment);
        }
        std::vector<std::vector<double>> q = {
            {1.0, 1.0, 1.0}, {2.0, 2.0, 2.0}, {3.0, 3.0, 3.0}};
        std::vector<std::vector<double>> p = {
            {10.0, 11.0, 12.0}, {8.0, 9.0, 10.0}, {6.0, 7.0, 8.0}};
        std::vector<std::vector<double>> a = {
            {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}};
        result.setSeries(q, p, a);
        xq::XQFlowResultPayload payload(result);

        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addFlowResult(payload);
        if (!added.ok) {
            return fail("addFlowResult succeeds for a 3-segment result");
        }
        if (added.actorCount != 1) {
            return fail("addFlowResult assembles one actor");
        }
        if (added.pointCount != 3) {
            return fail("addFlowResult reports the segment count");
        }
    }

    // --- clear() empties the scene ---
    {
        xq::XQTriangleSurfaceGeometryHandle surface = make_tet_surface();
        xq::XQSceneRenderer renderer;
        const xq::RenderStats added = renderer.addSurface(surface);
        if (!added.ok || added.actorCount != 1) {
            return fail("clear test: surface added");
        }
        renderer.clear();
        std::vector<unsigned char> rgba;
        const xq::RenderStats rendered =
            renderer.renderOffscreenToRgba(width, height, &rgba);
        if (!rendered.ok) {
            return fail("clear test: empty scene still renders");
        }
        if (rendered.actorCount != 0) {
            return fail("clear empties the scene's actors");
        }
    }

    return 0;
}
