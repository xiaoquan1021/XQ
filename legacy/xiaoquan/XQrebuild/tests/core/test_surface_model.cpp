#include <core/XQSurfaceModel.h>

#include <cassert>
#include <memory>

int main()
{
    xq::XQSurfaceModel unset_source_model;
    assert(!unset_source_model.hasSourceContourGroupNode());

    const xq::PreservedVtpArrays default_arrays = unset_source_model.preservedArrays();
    assert(!default_arrays.hasGlobalNodeID);
    assert(!default_arrays.hasGlobalElementID);
    assert(!default_arrays.hasModelFaceID);
    assert(!default_arrays.hasCapID);

    xq::XQSurfaceModel model;

    xq::ModelFace wall = {};
    wall.faceId = 10;
    wall.name = "aortic_wall";
    wall.kind = xq::FaceKind::Wall;
    wall.boundaryLoopIds = {1, 2};

    xq::ModelFace cap = {};
    cap.faceId = 20;
    cap.name = "inlet_cap";
    cap.kind = xq::FaceKind::Cap;
    cap.capId = 7;
    cap.boundaryLoopIds = {3};

    model.addFace(wall);
    model.addFace(cap);

    assert(model.faces().size() == 2);
    assert(!model.faces()[0].capId.has_value());
    assert(model.faces()[1].capId.has_value());
    assert(*model.faces()[1].capId == 7);

    xq::ModelFace found = {};
    assert(model.faceById(20, &found));
    assert(found.faceId == cap.faceId);
    assert(found.name == cap.name);
    assert(found.kind == cap.kind);
    assert(found.capId.has_value());
    assert(*found.capId == *cap.capId);

    found = {};
    assert(model.faceById(10, &found));
    assert(found.name == wall.name);
    assert(found.kind == wall.kind);
    assert(!found.capId.has_value());

    assert(!model.faceById(999, &found));

    xq::SurfaceGeometryHandle empty_geometry;
    assert(!empty_geometry.is_valid());

    std::shared_ptr<xq::SurfaceGeometryHandle> geometry(new xq::SurfaceGeometryHandle());
    geometry->setCounts(1000, 1980);
    assert(geometry->is_valid());
    assert(geometry->pointCount() == 1000);
    assert(geometry->cellCount() == 1980);

    model.setGeometry(geometry);
    assert(model.geometry() == geometry);
    assert(model.geometry()->pointCount() == 1000);
    assert(model.geometry()->cellCount() == 1980);

    xq::PreservedVtpArrays arrays = {};
    arrays.hasGlobalNodeID = true;
    arrays.hasGlobalElementID = true;
    arrays.hasModelFaceID = true;
    arrays.hasCapID = true;
    model.setPreservedArrays(arrays);

    assert(model.preservedArrays().hasGlobalNodeID);
    assert(model.preservedArrays().hasGlobalElementID);
    assert(model.preservedArrays().hasModelFaceID);
    assert(model.preservedArrays().hasCapID);

    const xq::NodeId source_contour_group(42);
    model.setSourceContourGroupNode(source_contour_group);
    assert(model.hasSourceContourGroupNode());
    assert(model.sourceContourGroupNode() == source_contour_group);

    model.setSource(xq::ModelSource::Loaded);
    assert(model.source() == xq::ModelSource::Loaded);

    return 0;
}
