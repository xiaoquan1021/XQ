#include <core/XQMesh.h>

#include <cassert>
#include <memory>
#include <vector>

int main()
{
    xq::VolumeMeshHandle default_volume;
    assert(!default_volume.is_valid());

    xq::VolumeMeshHandle volume;
    volume.setCounts(5000, 24000);
    assert(volume.is_valid());
    assert(volume.pointCount() == 5000);
    assert(volume.cellCount() == 24000);

    xq::SurfaceMeshHandle default_surface;
    assert(!default_surface.is_valid());

    xq::SurfaceMeshHandle surface;
    surface.setCounts(1200, 2380);
    assert(surface.is_valid());
    assert(surface.pointCount() == 1200);
    assert(surface.cellCount() == 2380);

    xq::XQMesh unset_source_mesh;
    assert(!unset_source_mesh.hasSourceModelNode());
    assert(!unset_source_mesh.sourceModelNode().is_valid());

    const xq::PreservedMeshArrays default_arrays = unset_source_mesh.preservedArrays();
    assert(!default_arrays.hasGlobalNodeID);
    assert(!default_arrays.hasGlobalElementID);
    assert(!default_arrays.hasModelFaceID);
    assert(!default_arrays.hasCapID);

    xq::XQMesh mesh;
    const xq::MeshId mesh_id(91);
    mesh.setId(mesh_id);
    assert(mesh.id() == mesh_id);

    std::shared_ptr<xq::VolumeMeshHandle> volume_handle(new xq::VolumeMeshHandle());
    volume_handle->setCounts(5000, 24000);
    mesh.setVolumeGrid(volume_handle);
    assert(mesh.volumeGrid() == volume_handle);
    assert(mesh.volumeGrid()->pointCount() == 5000);
    assert(mesh.volumeGrid()->cellCount() == 24000);

    std::shared_ptr<xq::SurfaceMeshHandle> surface_handle(new xq::SurfaceMeshHandle());
    surface_handle->setCounts(1200, 2380);
    mesh.setSurfaceMesh(surface_handle);
    assert(mesh.surfaceMesh() == surface_handle);
    assert(mesh.surfaceMesh()->pointCount() == 1200);
    assert(mesh.surfaceMesh()->cellCount() == 2380);

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

    assert(mesh.boundaryFaces().size() == 3);
    assert(!mesh.boundaryFaces()[0].capId.has_value());
    assert(mesh.boundaryFaces()[1].capId.has_value());
    assert(*mesh.boundaryFaces()[1].capId == 1);

    xq::MeshBoundaryFace found = {};
    assert(mesh.boundaryFaceById(20, &found));
    assert(found.faceId == inlet.faceId);
    assert(found.name == inlet.name);
    assert(found.kind == inlet.kind);
    assert(found.cellIds == inlet.cellIds);
    assert(!found.cellIds.empty());

    found = {};
    assert(mesh.boundaryFaceById(30, &found));
    assert(found.name == outlet.name);
    assert(found.kind == outlet.kind);
    assert(found.cellIds == outlet.cellIds);
    assert(!found.cellIds.empty());

    found = {};
    assert(mesh.boundaryFaceById(10, &found));
    assert(found.faceId == wall.faceId);
    assert(found.cellIds == wall.cellIds);

    assert(!mesh.boundaryFaceById(999, &found));

    xq::MeshRegion lumen = {};
    lumen.regionId = 1;
    lumen.name = "lumen";

    xq::MeshRegion branch = {};
    branch.regionId = 2;
    branch.name = "branch";

    mesh.addRegion(lumen);
    mesh.addRegion(branch);

    assert(mesh.regions().size() == 2);
    assert(mesh.regions()[0].regionId == 1);
    assert(mesh.regions()[0].name == "lumen");
    assert(mesh.regions()[1].regionId == 2);
    assert(mesh.regions()[1].name == "branch");

    xq::MeshQualitySummary quality = {};
    quality.minQuality = 0.12;
    quality.maxQuality = 0.98;
    quality.meanQuality = 0.76;
    quality.elementCount = 24000;
    mesh.setQuality(quality);

    assert(mesh.quality().minQuality == 0.12);
    assert(mesh.quality().maxQuality == 0.98);
    assert(mesh.quality().meanQuality == 0.76);
    assert(mesh.quality().elementCount == 24000);

    xq::PreservedMeshArrays arrays = {};
    arrays.hasGlobalNodeID = true;
    arrays.hasGlobalElementID = true;
    arrays.hasModelFaceID = true;
    arrays.hasCapID = true;
    mesh.setPreservedArrays(arrays);

    assert(mesh.preservedArrays().hasGlobalNodeID);
    assert(mesh.preservedArrays().hasGlobalElementID);
    assert(mesh.preservedArrays().hasModelFaceID);
    assert(mesh.preservedArrays().hasCapID);

    const xq::NodeId source_model(42);
    mesh.setSourceModelNode(source_model);
    assert(mesh.hasSourceModelNode());
    assert(mesh.sourceModelNode() == source_model);

    return 0;
}
