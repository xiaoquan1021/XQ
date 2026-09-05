#include <adapters/vtk/MSHMeshReader.h>

#include <cstdio>
#include <memory>
#include <string>

#ifndef XQ_MESHES_DIR
#error "XQ_MESHES_DIR must be defined"
#endif

namespace {

std::string msh_path(const std::string& name)
{
    return std::string(XQ_MESHES_DIR) + "/" + name + ".msh";
}

} // namespace

int main()
{
    xq::MSHReadResult result = {};
    const xq::MSHMeshReader::Status st =
        xq::MSHMeshReader::read(msh_path("0090_0001"), &result);
    if (st != xq::MSHMeshReader::Status::Ok) {
        std::fprintf(stderr, "FAIL: status=%d\n", static_cast<int>(st));
        return 1;
    }

    const std::shared_ptr<xq::VolumeMeshHandle> volume = result.mesh.volumeGrid();
    if (!volume) {
        std::fprintf(stderr, "FAIL: volumeGrid is null\n");
        return 1;
    }
    if (!volume->is_valid()) {
        std::fprintf(stderr, "FAIL: volumeGrid is invalid\n");
        return 1;
    }
    if (volume->pointCount() == 0) {
        std::fprintf(stderr, "FAIL: volumeGrid pointCount is zero\n");
        return 1;
    }
    if (volume->cellCount() == 0) {
        std::fprintf(stderr, "FAIL: volumeGrid cellCount is zero\n");
        return 1;
    }

    const std::shared_ptr<xq::SurfaceMeshHandle> surface = result.mesh.surfaceMesh();
    if (!surface) {
        std::fprintf(stderr, "FAIL: surfaceMesh is null\n");
        return 1;
    }
    if (!surface->is_valid()) {
        std::fprintf(stderr, "FAIL: surfaceMesh is invalid\n");
        return 1;
    }
    if (surface->pointCount() == 0) {
        std::fprintf(stderr, "FAIL: surfaceMesh pointCount is zero\n");
        return 1;
    }
    if (surface->cellCount() == 0) {
        std::fprintf(stderr, "FAIL: surfaceMesh cellCount is zero\n");
        return 1;
    }

    const xq::PreservedMeshArrays& arrays = result.mesh.preservedArrays();
    if (!arrays.hasGlobalNodeID) {
        std::fprintf(stderr, "FAIL: GlobalNodeID array was not preserved\n");
        return 1;
    }
    if (!arrays.hasGlobalElementID) {
        std::fprintf(stderr, "FAIL: GlobalElementID array was not preserved\n");
        return 1;
    }
    if (!arrays.hasModelFaceID) {
        std::fprintf(stderr, "FAIL: ModelFaceID array was not preserved\n");
        return 1;
    }
    if (!arrays.hasCapID) {
        std::fprintf(stderr, "FAIL: CapID array was not preserved\n");
        return 1;
    }

    if (result.mesh.boundaryFaces().empty()) {
        std::fprintf(stderr, "FAIL: boundaryFaces is empty\n");
        return 1;
    }

    std::printf("vtu points=%zu cells=%zu\n", volume->pointCount(), volume->cellCount());
    std::printf("vtp points=%zu cells=%zu\n", surface->pointCount(), surface->cellCount());
    std::printf("boundaryFaces=%zu\n", result.mesh.boundaryFaces().size());
    std::printf("preservedArrays GlobalNodeID=%s GlobalElementID=%s ModelFaceID=%s CapID=%s\n",
                arrays.hasGlobalNodeID ? "true" : "false",
                arrays.hasGlobalElementID ? "true" : "false",
                arrays.hasModelFaceID ? "true" : "false",
                arrays.hasCapID ? "true" : "false");
    std::fflush(stdout);

    return 0;
}
