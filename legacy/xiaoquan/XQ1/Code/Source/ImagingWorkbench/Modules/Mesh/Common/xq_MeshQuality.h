#ifndef XQ_MESHQUALITY_H
#define XQ_MESHQUALITY_H

#include <xqMeshCommonExports.h>
#include <string>
#include <vector>

class vtkUnstructuredGrid;

struct XQMESHCOMMON_EXPORT xq_MeshQualityReport
{
    bool ok = false;
    int numberOfPoints = 0;
    int numberOfCells = 0;
    double minVolume = 0.0;
    double maxVolume = 0.0;
    int negativeVolumeCount = 0;
    double minEdgeLength = 0.0;
    double maxEdgeLength = 0.0;
    double meanEdgeLength = 0.0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class XQMESHCOMMON_EXPORT xq_MeshQuality
{
public:
    static xq_MeshQualityReport Evaluate(vtkUnstructuredGrid* grid);
};

#endif
