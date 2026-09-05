#include <core/XQSurfaceModel.h>

#include <cstdio>
#include <memory>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survives Release /DNDEBUG. Side-effecting
// calls (e.g. faceById with an out-param) are evaluated to a variable first.
#define CHECK(cond)                            \
    do {                                       \
        if (!(cond)) {                         \
            return fail(#cond, __LINE__);      \
        }                                      \
    } while (0)

int main()
{
    xq::XQSurfaceModel unset_source_model;
    CHECK(!unset_source_model.hasSourceContourGroupNode());

    const xq::PreservedVtpArrays default_arrays = unset_source_model.preservedArrays();
    CHECK(!default_arrays.hasGlobalNodeID);
    CHECK(!default_arrays.hasGlobalElementID);
    CHECK(!default_arrays.hasModelFaceID);
    CHECK(!default_arrays.hasCapID);

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

    CHECK(model.faces().size() == 2);
    CHECK(!model.faces()[0].capId.has_value());
    CHECK(model.faces()[1].capId.has_value());
    CHECK(*model.faces()[1].capId == 7);

    xq::ModelFace found = {};
    const bool found_cap = model.faceById(20, &found);
    CHECK(found_cap);
    CHECK(found.faceId == cap.faceId);
    CHECK(found.name == cap.name);
    CHECK(found.kind == cap.kind);
    CHECK(found.capId.has_value());
    CHECK(*found.capId == *cap.capId);

    found = {};
    const bool found_wall = model.faceById(10, &found);
    CHECK(found_wall);
    CHECK(found.name == wall.name);
    CHECK(found.kind == wall.kind);
    CHECK(!found.capId.has_value());

    const bool found_missing = model.faceById(999, &found);
    CHECK(!found_missing);

    xq::SurfaceGeometryHandle empty_geometry;
    CHECK(!empty_geometry.is_valid());

    std::shared_ptr<xq::SurfaceGeometryHandle> geometry(new xq::SurfaceGeometryHandle());
    geometry->setCounts(1000, 1980);
    CHECK(geometry->is_valid());
    CHECK(geometry->pointCount() == 1000);
    CHECK(geometry->cellCount() == 1980);

    model.setGeometry(geometry);
    CHECK(model.geometry() == geometry);
    CHECK(model.geometry()->pointCount() == 1000);
    CHECK(model.geometry()->cellCount() == 1980);

    xq::PreservedVtpArrays arrays = {};
    arrays.hasGlobalNodeID = true;
    arrays.hasGlobalElementID = true;
    arrays.hasModelFaceID = true;
    arrays.hasCapID = true;
    model.setPreservedArrays(arrays);

    CHECK(model.preservedArrays().hasGlobalNodeID);
    CHECK(model.preservedArrays().hasGlobalElementID);
    CHECK(model.preservedArrays().hasModelFaceID);
    CHECK(model.preservedArrays().hasCapID);

    const xq::NodeId source_contour_group(42);
    model.setSourceContourGroupNode(source_contour_group);
    CHECK(model.hasSourceContourGroupNode());
    CHECK(model.sourceContourGroupNode() == source_contour_group);

    model.setSource(xq::ModelSource::Loaded);
    CHECK(model.source() == xq::ModelSource::Loaded);

    return 0;
}
