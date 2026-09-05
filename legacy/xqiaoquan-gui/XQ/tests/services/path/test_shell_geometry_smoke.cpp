#include <services/path/ShellGeometrySmokeService.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                     \
    do {                                      \
        if (!(expression)) {                  \
            return fail(#expression, __LINE__); \
        }                                     \
    } while (0)

bool near(double left, double right)
{
    return std::abs(left - right) < 1.0e-12;
}

xq::VesselPathV1 path()
{
    xq::VesselPathV1 value;
    value.coordinateSystem = xq::VesselPathCoordinateSystem::LPS;
    value.lengthUnit = xq::VesselPathLengthUnit::Millimeter;
    value.radiusDefinition =
        xq::VesselPathRadiusDefinition::EquivalentCircularArea;
    value.frameOfReferenceId = "1.2.840.smoke";
    value.source.kind = xq::VesselPathSourceKind::GoldFile;
    value.derivationStamp.algorithmId = "snapshot";
    value.derivationStamp.algorithmVersion = "1";
    value.derivationStamp.inputs.push_back(
        {xq::NodeId(5), 2, std::nullopt, std::string()});
    value.stations = {
        {xq::VesselPathStationId(1), {0.0, 0.0, 0.0}, 1.0, 0.0},
        {xq::VesselPathStationId(2), {3.0, 4.0, 0.0}, 2.0, 5.0},
        {xq::VesselPathStationId(3), {3.0, 4.0, 12.0}, 3.0, 17.0},
    };
    return value;
}

} // namespace

int main()
{
    const xq::ShellGeometrySmokeService::Result first =
        xq::ShellGeometrySmokeService::run(path());
    const xq::ShellGeometrySmokeService::Result second =
        xq::ShellGeometrySmokeService::run(path());
    CHECK(first.ok());
    CHECK(second.ok());
    CHECK(first.summary.stationCount == 3);
    CHECK(first.summary.segmentCount == 2);
    CHECK(near(first.summary.arcSpanMm, 17.0));
    CHECK(near(first.summary.polylineLengthMm, 17.0));
    CHECK(near(first.summary.minimumRadiusMm, 1.0));
    CHECK(near(first.summary.maximumRadiusMm, 3.0));
    CHECK(near(first.summary.meanRadiusMm, 2.0));
    CHECK(first.summary.canonicalDump == second.summary.canonicalDump);

    xq::VesselPathV1 invalid = path();
    invalid.stations[1].radiusMm = 0.0;
    const xq::ShellGeometrySmokeService::Result rejected =
        xq::ShellGeometrySmokeService::run(invalid);
    CHECK(!rejected.ok());
    CHECK(rejected.status
          == xq::ShellGeometrySmokeService::Status::InvalidPath);
    CHECK(rejected.summary.stationCount == 0);
    CHECK(rejected.summary.canonicalDump.empty());

    xq::VesselPathV1 largeRadii = path();
    for (xq::VesselPathStationV1& station : largeRadii.stations) {
        station.radiusMm = (std::numeric_limits<double>::max)();
    }
    const xq::ShellGeometrySmokeService::Result largeSummary =
        xq::ShellGeometrySmokeService::run(largeRadii);
    CHECK(largeSummary.ok());
    CHECK(std::isfinite(largeSummary.summary.meanRadiusMm));
    CHECK(largeSummary.summary.meanRadiusMm
          == (std::numeric_limits<double>::max)());

    std::printf("Path-only geometry smoke checks passed\n");
    return 0;
}
