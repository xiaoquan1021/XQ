#include "xq_TensionProfile.h"

#include <vtkKochanekSpline.h>
#include <vtkSmartPointer.h>
#include <cmath>
#include <numeric>

// Shared helper to evaluate closed spline and populate output points
static void evaluateClosedSpline(
    vtkSpline* splineX, vtkSpline* splineY, vtkSpline* splineZ,
    const std::vector<mitk::Point3D>& controlPoints,
    int subdivisionCount,
    std::vector<mitk::Point3D>& outPoints)
{
    const int numIn = static_cast<int>(controlPoints.size());
    for (int i = 0; i < numIn; ++i)
    {
        const double t = static_cast<double>(i);
        splineX->AddPoint(t, controlPoints[i][0]);
        splineY->AddPoint(t, controlPoints[i][1]);
        splineZ->AddPoint(t, controlPoints[i][2]);
    }

    const int numOut = subdivisionCount * numIn;
    const double tMax = static_cast<double>(numIn);

    outPoints.resize(numOut);
    int idx = 0;
    std::generate(outPoints.begin(), outPoints.end(),
        [&]() {
            const double t = tMax * static_cast<double>(idx++)
                             / static_cast<double>(numOut);
            mitk::Point3D pt;
            pt[0] = splineX->Evaluate(t);
            pt[1] = splineY->Evaluate(t);
            pt[2] = splineZ->Evaluate(t);
            return pt;
        });
}

void xq_TensionProfile::GenerateProfilePoints()
{
    m_ProfilePoints.clear();

    if (m_AnchorPoints.size() < 2)
    {
        return;
    }

    auto splineX = vtkSmartPointer<vtkKochanekSpline>::New();
    auto splineY = vtkSmartPointer<vtkKochanekSpline>::New();
    auto splineZ = vtkSmartPointer<vtkKochanekSpline>::New();

    splineX->SetClosed(true);
    splineY->SetClosed(true);
    splineZ->SetClosed(true);

    // Apply tension parameter to Kochanek splines
    splineX->SetDefaultTension(m_Tension);
    splineY->SetDefaultTension(m_Tension);
    splineZ->SetDefaultTension(m_Tension);

    evaluateClosedSpline(splineX, splineY, splineZ,
                         m_AnchorPoints, m_SubdivisionCount, m_ProfilePoints);

    // Recompute center point as centroid of control points using std::accumulate
    mitk::Point3D zero;
    zero.Fill(0.0);
    const auto numIn = static_cast<double>(m_AnchorPoints.size());

    m_ProfileCenter = std::accumulate(m_AnchorPoints.cbegin(), m_AnchorPoints.cend(), zero,
        [](mitk::Point3D acc, const mitk::Point3D& p) {
            acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2];
            return acc;
        });
    m_ProfileCenter[0] /= numIn;
    m_ProfileCenter[1] /= numIn;
    m_ProfileCenter[2] /= numIn;
}

std::string xq_TensionProfile::GetProfileKind() const
{
    return "TensionPolygon";
}

void xq_TensionProfile::SetTension(double t)
{
    m_Tension = t;
}

double xq_TensionProfile::GetTension() const
{
    return m_Tension;
}

// XQ: unique_ptr ownership transfer pattern for profile cloning
std::unique_ptr<xq_LumenProfile> xq_TensionProfile::Duplicate() const
{
    auto clone = std::make_unique<xq_TensionProfile>();
    transferBaseState(*clone);
    clone->m_Tension = m_Tension;
    return clone;
}
