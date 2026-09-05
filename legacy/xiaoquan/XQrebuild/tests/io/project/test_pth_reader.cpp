#include <io/project/PTHPathReader.h>

#include <core/GeometryTypes.h>

#include <cassert>
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

void validate_raw_samples(const std::vector<xq::PathSamplePoint>& samples)
{
    assert(!samples.empty());
    assert(close(samples.front().arcLength, 0.0, 1e-9));

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const xq::PathSamplePoint& sample = samples[i];
        assert(unit_vector(sample.tangent));
        assert(unit_vector(sample.normal));
        assert(unit_vector(sample.binormal));
        assert(close(xq::dot(sample.tangent, sample.normal), 0.0, 1e-6));

        if (i > 0) {
            assert(sample.arcLength >= samples[i - 1].arcLength);
        }
    }
}

} // namespace

int main()
{
    xq::PTHReadResult aorta = {};
    assert(xq::PTHPathReader::read(pth_path("aorta"), &aorta) == xq::PTHPathReader::Status::Ok);
    assert(aorta.path.controlPoints().size() == 16);
    assert(!aorta.path.samplePoints().empty());
    assert(aorta.pathName == "aorta");
    assert(aorta.sourceRelativePath == "Paths/aorta.pth");
    assert(!aorta.rawSamplePoints.empty());
    validate_raw_samples(aorta.rawSamplePoints);
    assert(close(aorta.path.controlPoints()[0].position.x, -1.91891, 1e-6));

    const std::vector<std::string> names = {
        "aorta",
        "btrunk",
        "carotid",
        "rt_carotid",
        "subclavian",
    };
    for (const std::string& name : names) {
        xq::PTHReadResult result = {};
        assert(xq::PTHPathReader::read(pth_path(name), &result) == xq::PTHPathReader::Status::Ok);
        assert(!result.path.controlPoints().empty());
        assert(!result.rawSamplePoints.empty());
        std::printf("%s control_points=%zu raw_sample_points=%zu\n",
                    name.c_str(),
                    result.path.controlPoints().size(),
                    result.rawSamplePoints.size());
    }

    xq::PTHReadResult missing = {};
    assert(xq::PTHPathReader::read(pth_path("missing_file_does_not_exist"), &missing)
           == xq::PTHPathReader::Status::FileNotFound);

    return 0;
}
