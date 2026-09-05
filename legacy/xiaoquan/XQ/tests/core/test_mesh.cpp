#include <core/XQMesh.h>

#include <cstdio>
#include <memory>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): a failing condition prints and returns 1
// so the test still validates under Release /DNDEBUG. Side-effecting calls are
// evaluated into a variable first, never inside the checked expression.
#define CHECK(cond)                            \
    do {                                       \
        if (!(cond)) {                         \
            return fail(#cond, __LINE__);      \
        }                                      \
    } while (0)

int main()
{
    xq::VolumeMeshHandle default_volume;
    CHECK(!default_volume.is_valid());

    xq::VolumeMeshHandle volume;
    volume.setCounts(5000, 24000);
    CHECK(volume.is_valid());
    CHECK(volume.pointCount() == 5000);
    CHECK(volume.cellCount() == 24000);

    xq::SurfaceMeshHandle default_surface;
    CHECK(!default_surface.is_valid());

    xq::SurfaceMeshHandle surface;
    surface.setCounts(1200, 2380);
    CHECK(surface.is_valid());
    CHECK(surface.pointCount() == 1200);
    CHECK(surface.cellCount() == 2380);

    xq::XQMesh unset_source_mesh;
    CHECK(!unset_source_mesh.hasSourceModelNode());
    CHECK(!unset_source_mesh.sourceModelNode().is_valid());

    const xq::PreservedMeshArrays default_arrays = unset_source_mesh.preservedArrays();
    CHECK(!default_arrays.hasGlobalNodeID);
    CHECK(!default_arrays.hasGlobalElementID);
    CHECK(!default_arrays.hasModelFaceID);
    CHECK(!default_arrays.hasCapID);

    xq::XQMesh mesh;
    const xq::MeshId mesh_id(91);
    mesh.setId(mesh_id);
    CHECK(mesh.id() == mesh_id);

    std::shared_ptr<xq::VolumeMeshHandle> volume_handle(new xq::VolumeMeshHandle());
    volume_handle->setCounts(5000, 24000);
    mesh.setVolumeGrid(volume_handle);
    CHECK(mesh.volumeGrid() == volume_handle);
    CHECK(mesh.volumeGrid()->pointCount() == 5000);
    CHECK(mesh.volumeGrid()->cellCount() == 24000);

    std::shared_ptr<xq::SurfaceMeshHandle> surface_handle(new xq::SurfaceMeshHandle());
    surface_handle->setCounts(1200, 2380);
    mesh.setSurfaceMesh(surface_handle);
    CHECK(mesh.surfaceMesh() == surface_handle);
    CHECK(mesh.surfaceMesh()->pointCount() == 1200);
    CHECK(mesh.surfaceMesh()->cellCount() == 2380);

    xq::MeshBoundaryFace wall = {};
    wall.faceId = 10;
    wall.name = "wall";
    wall.kind = xq::FaceKind::Wall;
    wall.cellIds = {100, 101, 102};

    xq::MeshBoundaryFace inlet = {};
    inlet.faceId = 20;
    inlet.name = "inlet";
    inlet.kind = xq::FaceKind::Inlet;
    inlet.capId = 1;
    inlet.cellIds = {200, 201};

    xq::MeshBoundaryFace outlet = {};
    outlet.faceId = 30;
    outlet.name = "outlet";
    outlet.kind = xq::FaceKind::Outlet;
    outlet.capId = 2;
    outlet.cellIds = {300, 301, 302};

    mesh.addBoundaryFace(wall);
    mesh.addBoundaryFace(inlet);
    mesh.addBoundaryFace(outlet);

    CHECK(mesh.boundaryFaces().size() == 3);
    CHECK(!mesh.boundaryFaces()[0].capId.has_value());
    CHECK(mesh.boundaryFaces()[1].capId.has_value());
    CHECK(*mesh.boundaryFaces()[1].capId == 1);

    xq::MeshBoundaryFace found = {};
    const bool found_inlet = mesh.boundaryFaceById(20, &found);
    CHECK(found_inlet);
    CHECK(found.faceId == inlet.faceId);
    CHECK(found.name == inlet.name);
    CHECK(found.kind == inlet.kind);
    CHECK(found.cellIds == inlet.cellIds);
    CHECK(!found.cellIds.empty());

    found = {};
    const bool found_outlet = mesh.boundaryFaceById(30, &found);
    CHECK(found_outlet);
    CHECK(found.name == outlet.name);
    CHECK(found.kind == outlet.kind);
    CHECK(found.cellIds == outlet.cellIds);
    CHECK(!found.cellIds.empty());

    found = {};
    const bool found_wall = mesh.boundaryFaceById(10, &found);
    CHECK(found_wall);
    CHECK(found.faceId == wall.faceId);
    CHECK(found.cellIds == wall.cellIds);

    const bool found_missing = mesh.boundaryFaceById(999, &found);
    CHECK(!found_missing);

    xq::MeshRegion lumen = {};
    lumen.regionId = 1;
    lumen.name = "lumen";

    xq::MeshRegion branch = {};
    branch.regionId = 2;
    branch.name = "branch";

    mesh.addRegion(lumen);
    mesh.addRegion(branch);

    CHECK(mesh.regions().size() == 2);
    CHECK(mesh.regions()[0].regionId == 1);
    CHECK(mesh.regions()[0].name == "lumen");
    CHECK(mesh.regions()[1].regionId == 2);
    CHECK(mesh.regions()[1].name == "branch");

    xq::MeshQualitySummary quality = {};
    quality.minQuality = 0.12;
    quality.maxQuality = 0.98;
    quality.meanQuality = 0.76;
    quality.elementCount = 24000;
    mesh.setQuality(quality);

    CHECK(mesh.quality().minQuality == 0.12);
    CHECK(mesh.quality().maxQuality == 0.98);
    CHECK(mesh.quality().meanQuality == 0.76);
    CHECK(mesh.quality().elementCount == 24000);

    xq::PreservedMeshArrays arrays = {};
    arrays.hasGlobalNodeID = true;
    arrays.hasGlobalElementID = true;
    arrays.hasModelFaceID = true;
    arrays.hasCapID = true;
    mesh.setPreservedArrays(arrays);

    CHECK(mesh.preservedArrays().hasGlobalNodeID);
    CHECK(mesh.preservedArrays().hasGlobalElementID);
    CHECK(mesh.preservedArrays().hasModelFaceID);
    CHECK(mesh.preservedArrays().hasCapID);

    const xq::NodeId source_model(42);
    mesh.setSourceModelNode(source_model);
    CHECK(mesh.hasSourceModelNode());
    CHECK(mesh.sourceModelNode() == source_model);

    return 0;
}
