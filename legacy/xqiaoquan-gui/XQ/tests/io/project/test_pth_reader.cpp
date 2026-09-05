#include <io/project/PTHPathReader.h>

#include <core/GeometryTypes.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool close(double a, double b, double epsilon)
{
    return std::abs(a - b) <= epsilon;
}

bool unit_vector(const xq::Vec3& value)
{
    return close(xq::norm(value), 1.0, 1e-6);
}

std::string pth_path(const std::string& name)
{
    return std::string(XQ_PTH_DIR) + "/" + name + ".pth";
}

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survives Release /DNDEBUG. Side-effecting
// reads are evaluated to a variable first (see main), never inside CHECK.
#define CHECK(cond)                            \
    do {                                       \
        if (!(cond)) {                         \
            return fail(#cond, __LINE__);      \
        }                                      \
    } while (0)

namespace {

// Returns 0 on success, non-zero (with a printed FAIL line) on the first
// failing invariant. Operates on already-read sample data (no side effects).
int validate_raw_samples(const std::vector<xq::PathSamplePoint>& samples)
{
    CHECK(!samples.empty());
    CHECK(close(samples.front().arcLength, 0.0, 1e-9));

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const xq::PathSamplePoint& sample = samples[i];
        CHECK(unit_vector(sample.tangent));
        CHECK(unit_vector(sample.normal));
        CHECK(unit_vector(sample.binormal));
        CHECK(close(xq::dot(sample.tangent, sample.normal), 0.0, 1e-6));

        if (i > 0) {
            CHECK(sample.arcLength >= samples[i - 1].arcLength);
        }
    }

    return 0;
}

} // namespace

int main()
{
    xq::PTHReadResult aorta = {};
    const xq::PTHPathReader::Status aorta_status = xq::PTHPathReader::read(pth_path("aorta"), &aorta);
    CHECK(aorta_status == xq::PTHPathReader::Status::Ok);
    CHECK(aorta.path.controlPoints().size() == 16);
    CHECK(!aorta.path.samplePoints().empty());
    CHECK(aorta.pathName == "aorta");
    CHECK(aorta.sourceRelativePath == "Paths/aorta.pth");
    CHECK(!aorta.rawSamplePoints.empty());
    if (const int rc = validate_raw_samples(aorta.rawSamplePoints)) {
        return rc;
    }
    CHECK(close(aorta.path.controlPoints()[0].position.x, -1.91891, 1e-6));

    const std::vector<std::string> names = {
        "aorta",
        "btrunk",
        "carotid",
        "rt_carotid",
        "subclavian",
    };
    for (const std::string& name : names) {
        xq::PTHReadResult result = {};
        const xq::PTHPathReader::Status status = xq::PTHPathReader::read(pth_path(name), &result);
        CHECK(status == xq::PTHPathReader::Status::Ok);
        CHECK(!result.path.controlPoints().empty());
        CHECK(!result.rawSamplePoints.empty());
        std::printf("%s control_points=%zu raw_sample_points=%zu\n",
                    name.c_str(),
                    result.path.controlPoints().size(),
                    result.rawSamplePoints.size());
    }

    xq::PTHReadResult missing = {};
    const xq::PTHPathReader::Status missing_status =
        xq::PTHPathReader::read(pth_path("missing_file_does_not_exist"), &missing);
    CHECK(missing_status == xq::PTHPathReader::Status::FileNotFound);

    return 0;
}
