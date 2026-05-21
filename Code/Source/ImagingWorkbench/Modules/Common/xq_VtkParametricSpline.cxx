#include "xq_VtkParametricSpline.h"
#include "xq_SpatialMath.h"

#include <vtkCardinalSpline.h>
#include <vtkSmartPointer.h>

#include <cmath>

xq_VtkParametricSpline::xq_VtkParametricSpline()
{
}

xq_VtkParametricSpline::~xq_VtkParametricSpline()
{
}

std::vector<mitk::Point3D> xq_VtkParametricSpline::EvaluateCurve()
{
    std::vector<mitk::Point3D> result;
    const int numIn = static_cast<int>(m_ControlVertices.size());
    if (numIn < 2 || m_Resolution < 1)
    {
        return m_ControlVertices;
    }

    auto splineX = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineY = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineZ = vtkSmartPointer<vtkCardinalSpline>::New();

    splineX->SetClosed(m_Periodic);
    splineY->SetClosed(m_Periodic);
    splineZ->SetClosed(m_Periodic);

    for (int i = 0; i < numIn; ++i)
    {
        double t = static_cast<double>(i);
        splineX->AddPoint(t, m_ControlVertices[i][0]);
        splineY->AddPoint(t, m_ControlVertices[i][1]);
        splineZ->AddPoint(t, m_ControlVertices[i][2]);
    }

    double tMax = m_Periodic ? static_cast<double>(numIn)
                           : static_cast<double>(numIn - 1);

    result.reserve(m_Resolution);

    for (int i = 0; i < m_Resolution; ++i)
    {
        double t = tMax * static_cast<double>(i)
                   / static_cast<double>(m_Resolution);
        mitk::Point3D pt;
        pt[0] = splineX->Evaluate(t);
        pt[1] = splineY->Evaluate(t);
        pt[2] = splineZ->Evaluate(t);
        result.push_back(pt);
    }

    return result;
}

std::vector<xq_VtkParametricSpline::SplinePoint>
xq_VtkParametricSpline::GetSplineFramePoints()
{
    std::vector<SplinePoint> result;
    const int numIn = static_cast<int>(m_ControlVertices.size());
    if (numIn < 2 || m_Resolution < 1)
    {
        return result;
    }

    auto splineX = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineY = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineZ = vtkSmartPointer<vtkCardinalSpline>::New();

    splineX->SetClosed(m_Periodic);
    splineY->SetClosed(m_Periodic);
    splineZ->SetClosed(m_Periodic);

    for (int i = 0; i < numIn; ++i)
    {
        double t = static_cast<double>(i);
        splineX->AddPoint(t, m_ControlVertices[i][0]);
        splineY->AddPoint(t, m_ControlVertices[i][1]);
        splineZ->AddPoint(t, m_ControlVertices[i][2]);
    }

    double tMax = m_Periodic ? static_cast<double>(numIn)
                           : static_cast<double>(numIn - 1);

    result.reserve(m_Resolution);

    // Small step for finite differences
    const double dt = tMax / static_cast<double>(m_Resolution) * 0.01;

    // Track the previous in-plane x-axis for stable slice placement.
    mitk::Vector3D prevXAxis;
    prevXAxis[0] = 0.0; prevXAxis[1] = 0.0; prevXAxis[2] = 0.0;
    bool firstFrame = true;

    for (int i = 0; i < m_Resolution; ++i)
    {
        double t = tMax * static_cast<double>(i)
                   / static_cast<double>(m_Resolution);

        SplinePoint sp;

        // Position
        sp.position[0] = splineX->Evaluate(t);
        sp.position[1] = splineY->Evaluate(t);
        sp.position[2] = splineZ->Evaluate(t);

        // Tangent via central finite differences
        double tFwd = std::min(t + dt, tMax - 1e-9);
        double tBwd = std::max(t - dt, 0.0);

        mitk::Point3D pFwd, pBwd;
        pFwd[0] = splineX->Evaluate(tFwd);
        pFwd[1] = splineY->Evaluate(tFwd);
        pFwd[2] = splineZ->Evaluate(tFwd);
        pBwd[0] = splineX->Evaluate(tBwd);
        pBwd[1] = splineY->Evaluate(tBwd);
        pBwd[2] = splineZ->Evaluate(tBwd);

        mitk::Vector3D tangent;
        tangent[0] = pFwd[0] - pBwd[0];
        tangent[1] = pFwd[1] - pBwd[1];
        tangent[2] = pFwd[2] - pBwd[2];
        sp.tangent = xq_SpatialMath::UnitVector(tangent);

        // The rotation field stores the in-plane xhat vector, not the binormal.
        // Downstream reslice and contour placement assume this exact semantic.
        mitk::Vector3D frameXAxis;
        if (firstFrame)
        {
            frameXAxis = xq_SpatialMath::ComputeOrthogonalVector(sp.tangent);
            firstFrame = false;
        }
        else
        {
            // Project the previous xhat onto the plane perpendicular to the
            // current tangent to keep frame rotation continuous.
            double proj = xq_SpatialMath::DotProduct3D(prevXAxis, sp.tangent);
            mitk::Vector3D n;
            n[0] = prevXAxis[0] - proj * sp.tangent[0];
            n[1] = prevXAxis[1] - proj * sp.tangent[1];
            n[2] = prevXAxis[2] - proj * sp.tangent[2];
            double nLen = std::sqrt(xq_SpatialMath::DotProduct3D(n, n));
            if (nLen > 1e-12)
            {
                frameXAxis = xq_SpatialMath::UnitVector(n);
            }
            else
            {
                frameXAxis = xq_SpatialMath::ComputeOrthogonalVector(sp.tangent);
            }
        }

        const auto frameBinormal = xq_SpatialMath::CrossProduct3D(sp.tangent, frameXAxis);
        sp.rotation = frameXAxis;
        sp.normal = xq_SpatialMath::UnitVector(frameBinormal);

        prevXAxis = sp.rotation;
        result.push_back(sp);
    }

    return result;
}
