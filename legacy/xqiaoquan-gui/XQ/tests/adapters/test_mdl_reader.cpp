#include <adapters/vtk/MDLModelReader.h>

#include <cstdio>
#include <memory>
#include <string>

#ifndef XQ_MODELS_DIR
#error "XQ_MODELS_DIR must be defined"
#endif

namespace {

const char* face_kind_name(xq::FaceKind kind)
{
    switch (kind) {
    case xq::FaceKind::Wall:
        return "Wall";
    case xq::FaceKind::Cap:
        return "Cap";
    case xq::FaceKind::Inlet:
        return "Inlet";
    case xq::FaceKind::Outlet:
        return "Outlet";
    case xq::FaceKind::Unknown:
    default:
        return "Unknown";
    }
}

std::string mdl_path(const std::string& name)
{
    return std::string(XQ_MODELS_DIR) + "/" + name + ".mdl";
}

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

} // namespace

int main()
{
    xq::MDLReadResult result = {};
    const xq::MDLModelReader::Status status =
        xq::MDLModelReader::read(mdl_path("0090_0001"), &result);
    if (status != xq::MDLModelReader::Status::Ok) {
        return fail("mdl read status", __LINE__);
    }

    if (result.model.faces().size() != 7) {
        return fail("mdl face count", __LINE__);
    }
    bool sawWall = false;
    for (const xq::ModelFace& face : result.model.faces()) {
        std::printf("face id=%d name=%s kind=%s\n",
                    face.faceId,
                    face.name.c_str(),
                    face_kind_name(face.kind));
        if (face.kind == xq::FaceKind::Wall) {
            sawWall = true;
        }
    }
    if (!sawWall) {
        return fail("mdl saw wall face", __LINE__);
    }

    xq::ModelFace wall = {};
    const bool foundWall = result.model.faceById(1, &wall);
    if (!foundWall) {
        return fail("mdl faceById wall", __LINE__);
    }
    if (wall.name != "wall") {
        return fail("mdl wall name", __LINE__);
    }
    if (wall.kind != xq::FaceKind::Wall) {
        return fail("mdl wall kind", __LINE__);
    }

    const std::shared_ptr<xq::SurfaceGeometryHandle> geometry = result.model.geometry();
    if (!geometry) {
        return fail("mdl geometry", __LINE__);
    }
    if (!geometry->is_valid()) {
        return fail("mdl geometry valid", __LINE__);
    }
    if (geometry->pointCount() == 0) {
        return fail("mdl geometry point count", __LINE__);
    }
    if (geometry->cellCount() == 0) {
        return fail("mdl geometry cell count", __LINE__);
    }
    std::printf("points=%zu cells=%zu\n", geometry->pointCount(), geometry->cellCount());

    const xq::PreservedVtpArrays& arrays = result.model.preservedArrays();
    std::printf("preservedArrays GlobalNodeID=%d GlobalElementID=%d ModelFaceID=%d CapID=%d\n",
                arrays.hasGlobalNodeID ? 1 : 0,
                arrays.hasGlobalElementID ? 1 : 0,
                arrays.hasModelFaceID ? 1 : 0,
                arrays.hasCapID ? 1 : 0);
    if (!(arrays.hasModelFaceID || arrays.hasGlobalNodeID)) {
        return fail("mdl preserved arrays", __LINE__);
    }

    if (result.model.source() != xq::ModelSource::Loaded) {
        return fail("mdl model source", __LINE__);
    }
    if (result.modelName != "0090_0001") {
        return fail("mdl model name", __LINE__);
    }
    if (result.sourceRelativePath != "Models/0090_0001.mdl") {
        return fail("mdl source relative path", __LINE__);
    }

    xq::MDLReadResult missing = {};
    const xq::MDLModelReader::Status missingStatus =
        xq::MDLModelReader::read(mdl_path("missing_file_does_not_exist"), &missing);
    if (missingStatus != xq::MDLModelReader::Status::MdlNotFound) {
        return fail("mdl missing file status", __LINE__);
    }

    std::fflush(stdout);
    return 0;
}
