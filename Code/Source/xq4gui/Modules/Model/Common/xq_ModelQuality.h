#ifndef XQ_MODELQUALITY_H
#define XQ_MODELQUALITY_H

#include <xqModelCommonExports.h>
#include <string>
#include <vector>

class vtkPolyData;

struct XQMODELCOMMON_EXPORT xq_ModelQualityReport
{
    bool ok = false;
    int numberOfPoints = 0;
    int numberOfCells = 0;
    int boundaryEdges = 0;
    int nonManifoldEdges = 0;
    int degenerateCellCount = 0;
    int connectedComponents = 0;
    bool hasFaceIds = false;
    int faceCount = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // Convenience: true if no errors
    bool IsClosed() const { return boundaryEdges == 0; }
    bool IsManifold() const { return nonManifoldEdges == 0; }
};

class XQMODELCOMMON_EXPORT xq_ModelQuality
{
public:
    static xq_ModelQualityReport Evaluate(vtkPolyData* polyData);
};

#endif
