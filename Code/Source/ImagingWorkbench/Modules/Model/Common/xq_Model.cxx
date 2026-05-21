#include "xq_Model.h"
#include "xq_GeometryOp.h"

#include <mitkGeometry3D.h>
#include <mitkLogMacros.h>
#include <mitkSlicedGeometry3D.h>
#include <mitkTimeGeometry.h>
#include <mitkProportionalTimeGeometry.h>

#include <vtkPolyData.h>

xq_Model::xq_Model()
    : m_DataModified(false)
    , m_CalculateBoundingBox(true)
{
    m_ModelElements.push_back(nullptr);
    Superclass::InitializeTimeGeometry(1);
}

xq_Model::xq_Model(const xq_Model& other)
    : mitk::BaseData(other)
    , m_Type(other.m_Type)
    , m_Properties(other.m_Properties)
    , m_DataModified(other.m_DataModified)
    , m_CalculateBoundingBox(true)
{
    for (const auto& elem : other.m_ModelElements)
    {
        if (elem)
        {
            auto cloned = elem->Clone();
            if (!cloned)
            {
                MITK_WARN << "xq_Model: Clone() returned nullptr for a non-null model element.";
            }
            m_ModelElements.push_back(std::move(cloned));
        }
        else
        {
            m_ModelElements.push_back(nullptr);
        }
    }
}

xq_Model::~xq_Model()
{
    ClearData();
}

void xq_Model::UpdateOutputInformation()
{
    if (this->GetSource())
    {
        this->GetSource()->UpdateOutputInformation();
    }

    auto timeGeometry = mitk::ProportionalTimeGeometry::New();
    timeGeometry->Initialize(mitk::Geometry3D::New(),
                             static_cast<unsigned int>(m_ModelElements.size()));

    for (size_t t = 0; t < m_ModelElements.size(); ++t)
    {
        if (!m_ModelElements[t])
        {
            MITK_WARN << "xq_Model::UpdateOutputInformation: null element at timestep " << t;
            continue;
        }

        auto polyData = m_ModelElements[t]->GetWholeVtkPolyData();
        if (!polyData || polyData->GetNumberOfPoints() == 0)
            continue;

        double bounds[6];
        polyData->GetBounds(bounds);

        auto geometry = mitk::Geometry3D::New();
        mitk::Point3D origin;
        origin[0] = bounds[0];
        origin[1] = bounds[2];
        origin[2] = bounds[4];

        mitk::Vector3D spacing;
        spacing.Fill(1.0);

        mitk::Geometry3D::BoundsArrayType geoBounds;
        geoBounds[0] = 0;
        geoBounds[1] = bounds[1] - bounds[0];
        geoBounds[2] = 0;
        geoBounds[3] = bounds[3] - bounds[2];
        geoBounds[4] = 0;
        geoBounds[5] = bounds[5] - bounds[4];

        geometry->SetBounds(geoBounds);
        geometry->SetOrigin(origin);
        geometry->SetSpacing(spacing);

        timeGeometry->SetTimeStepGeometry(geometry, static_cast<unsigned int>(t));
    }

    SetTimeGeometry(timeGeometry);
}

void xq_Model::SetRequestedRegionToLargestPossibleRegion()
{
}

bool xq_Model::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_Model::VerifyRequestedRegion()
{
    return true;
}

void xq_Model::SetRequestedRegion(const itk::DataObject* /*data*/)
{
}

void xq_Model::Expand(unsigned int timeSteps)
{
    while (m_ModelElements.size() < timeSteps)
    {
        m_ModelElements.push_back(nullptr);
    }
}

void xq_Model::ExecuteOperation(mitk::Operation* operation)
{
    auto* modelOp = dynamic_cast<xq_GeometryOp*>(operation);
    if (!modelOp)
        return;

    switch (operation->GetOperationType())
    {
    case OpSETMODELELEMENT:
    {
        const unsigned int t = modelOp->GetTimeStep();
        if (t >= m_ModelElements.size())
        {
            Expand(t + 1);
        }
        // Transfer raw pointer from operation (non-owning in op, Model takes ownership)
        m_ModelElements[t].reset(modelOp->GetModelElement());
        m_DataModified = true;
        m_CalculateBoundingBox = true;
        this->Modified();
        break;
    }
    case OpSETFACESELECTED:
    {
        this->Modified();
        break;
    }
    default:
        MITK_WARN << "xq_Model::ExecuteOperation: unhandled operation type "
                   << operation->GetOperationType();
        break;
    }
}

xq_VascularGeometry* xq_Model::GetModelElement(unsigned int t) const
{
    if (t < m_ModelElements.size())
    {
        return m_ModelElements[t].get();
    }
    return nullptr;
}

void xq_Model::SetModelElement(std::unique_ptr<xq_VascularGeometry> element, unsigned int t)
{
    if (t >= m_ModelElements.size())
    {
        Expand(t + 1);
    }
    m_ModelElements[t] = std::move(element);
    m_CalculateBoundingBox = true;
    this->Modified();
}

void xq_Model::SetModelElement(xq_VascularGeometry* element, unsigned int t)
{
    SetModelElement(std::unique_ptr<xq_VascularGeometry>(element), t);
}

std::string xq_Model::GetType() const
{
    return m_Type;
}

void xq_Model::SetType(std::string_view type)
{
    m_Type = std::string(type);
}

void xq_Model::SetProperty(std::string_view key, std::string_view value)
{
    m_Properties[std::string(key)] = std::string(value);
}

std::string xq_Model::GetProperty(const std::string& key) const
{
    if (auto it = m_Properties.find(key); it != m_Properties.end())
    {
        return it->second;
    }
    return {};
}

bool xq_Model::IsEmptyTimeStep(unsigned int t) const
{
    if (t >= m_ModelElements.size() || !m_ModelElements[t])
        return true;

    auto pd = m_ModelElements[t]->GetWholeVtkPolyData();
    return !pd || pd->GetNumberOfPoints() == 0;
}

void xq_Model::ClearData()
{
    m_ModelElements.clear();
}

void xq_Model::InitializeEmpty()
{
    m_ModelElements.clear();
    m_ModelElements.push_back(nullptr);
}
