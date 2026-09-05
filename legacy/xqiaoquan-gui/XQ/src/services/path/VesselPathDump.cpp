#include "services/path/VesselPathDump.h"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace xq {
namespace {

std::string escaped_field(const std::string& value)
{
    static const char* digits = "0123456789ABCDEF";
    std::string escaped;
    for (const unsigned char character : value) {
        if (character >= 0x20 && character <= 0x7e && character != '%') {
            escaped.push_back(static_cast<char>(character));
        } else {
            escaped.push_back('%');
            escaped.push_back(digits[(character >> 4) & 0x0f]);
            escaped.push_back(digits[character & 0x0f]);
        }
    }
    return escaped;
}

} // namespace

VesselPathDump::Result VesselPathDump::format(const VesselPathV1& path)
{
    Result result;
    result.validation = VesselPathValidator::validate(path);
    if (!result.validation.ok()) {
        return result;
    }

    const DerivationInputStamp& input = path.derivationStamp.inputs.front();
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    stream << "vessel_path_v1\n";
    stream << "contract_version=" << path.contractVersion << '\n';
    stream << "coordinate_system=LPS\n";
    stream << "length_unit=mm\n";
    stream << "radius_definition="
           << vesselPathRadiusDefinitionToken(path.radiusDefinition) << '\n';
    stream << "frame_of_reference_id="
           << escaped_field(path.frameOfReferenceId) << '\n';
    stream << "source=" << vesselPathSourceToken(path.source.kind) << '\n';
    stream << "source_profile_node=" << input.nodeId.value() << '\n';
    stream << "source_profile_revision=" << input.contentRevision << '\n';
    stream << "source_profile_asset=";
    if (input.assetId.has_value()) {
        stream << input.assetId->value();
    } else {
        stream << "none";
    }
    stream << '\n';
    stream << "source_profile_fingerprint="
           << escaped_field(input.assetFingerprint) << '\n';
    stream << "station_count=" << path.stations.size() << '\n';
    for (std::size_t i = 0; i < path.stations.size(); ++i) {
        const VesselPathStationV1& station = path.stations[i];
        stream << "station[" << i << "]"
               << " id=" << station.stationId.value()
               << " arc_length_mm=" << station.arcLengthMm
               << " position_lps_mm=" << station.positionMm.x << ','
               << station.positionMm.y << ',' << station.positionMm.z
               << " radius_mm=" << station.radiusMm << '\n';
    }

    result.status = Status::Ok;
    result.text = stream.str();
    return result;
}

} // namespace xq
