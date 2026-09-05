#ifndef XQ_CORE_XQ_FLOW_SMOKE_H
#define XQ_CORE_XQ_FLOW_SMOKE_H

#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQVesselProfile.h"

#include <cstddef>
#include <string>
#include <vector>

namespace xq {

enum class FlowLengthUnit {
    Millimeter,
    Centimeter
};

enum class FlowAreaUnit {
    SquareMillimeter,
    SquareCentimeter
};

inline const char* flowLengthUnitToToken(FlowLengthUnit unit)
{
    switch (unit) {
    case FlowLengthUnit::Millimeter:
        return "mm";
    case FlowLengthUnit::Centimeter:
        return "cm";
    }
    return "";
}

inline bool flowLengthUnitFromToken(const std::string& token, FlowLengthUnit* out)
{
    if (out == nullptr) {
        return false;
    }
    if (token == "mm") {
        *out = FlowLengthUnit::Millimeter;
        return true;
    }
    if (token == "cm") {
        *out = FlowLengthUnit::Centimeter;
        return true;
    }
    return false;
}

inline const char* flowAreaUnitToToken(FlowAreaUnit unit)
{
    switch (unit) {
    case FlowAreaUnit::SquareMillimeter:
        return "mm2";
    case FlowAreaUnit::SquareCentimeter:
        return "cm2";
    }
    return "";
}

inline bool flowAreaUnitFromToken(const std::string& token, FlowAreaUnit* out)
{
    if (out == nullptr) {
        return false;
    }
    if (token == "mm2") {
        *out = FlowAreaUnit::SquareMillimeter;
        return true;
    }
    if (token == "cm2") {
        *out = FlowAreaUnit::SquareCentimeter;
        return true;
    }
    return false;
}

struct FlowSmokeProtocolStamp {
    std::string id;
    unsigned int version = 0;
    std::string label;
    std::size_t stationCount = 0;
    double dtSeconds = 0.0;
    int numTimeSteps = 0;
    int numCycles = 0;
};

struct FlowUnitConversionRecord {
    FlowLengthUnit sourceLengthUnit = FlowLengthUnit::Millimeter;
    FlowAreaUnit sourceAreaUnit = FlowAreaUnit::SquareMillimeter;
    FlowLengthUnit targetLengthUnit = FlowLengthUnit::Centimeter;
    FlowAreaUnit targetAreaUnit = FlowAreaUnit::SquareCentimeter;
    double lengthScale = 0.0;
    double areaScale = 0.0;
};

struct FlowStationSourceMapping {
    std::size_t solverStationIndex = 0;
    double solverArcLengthCm = 0.0;
    VesselSampleId leftSampleId;
    VesselSampleId rightSampleId;
    double leftWeight = 0.0;
    double rightWeight = 0.0;
};

struct FlowSmokeCaseProvenance {
    FlowSmokeProtocolStamp protocol;
    std::string assemblerId;
    std::string assemblerVersion;
    NodeId sourceVesselProfileNode;
    ContentRevision sourceVesselProfileRevision = 0;
    FlowUnitConversionRecord conversion;
    std::vector<FlowStationSourceMapping> stationMap;
};

struct FlowSmokeResultProvenance {
    FlowSmokeProtocolStamp protocol;
    std::string solverId;
    std::string solverVersion;
    NodeId sourceVesselProfileNode;
    ContentRevision sourceVesselProfileRevision = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_FLOW_SMOKE_H
