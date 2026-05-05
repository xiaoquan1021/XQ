#include "xq_ProjectDataReader.h"

#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_ProfileGroup.h>
#include <xq_PolygonalProfile.h>
#include <xq_Model.h>
#include <xq_VascularGeometry.h>
#include <xq_PolyGeometry.h>
#include <xq_MitkGrid.h>
#include <xq_TetGenGrid.h>
#include <xq_MitkSolverJob.h>
#include <xq_SolverJob.h>

#include <mitkIOUtil.h>

#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <tinyxml2.h>

#include <mitkPlaneGeometry.h>

#include <functional>

namespace {

bool EndsWith(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

mitk::DataNode::Pointer LoadXqPath(const std::string& fullPath)
{
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(fullPath.c_str());
    reader->Update();
    vtkPolyData* pd = reader->GetOutput();
    if (!pd || pd->GetNumberOfPoints() == 0)
        return nullptr;

    vtkPoints* vtkPts = pd->GetPoints();
    std::vector<mitk::Point3D> anchors;
    for (vtkIdType i = 0; i < vtkPts->GetNumberOfPoints(); ++i)
    {
        double pt[3];
        vtkPts->GetPoint(i, pt);
        anchors.emplace_back();
        anchors.back()[0] = pt[0];
        anchors.back()[1] = pt[1];
        anchors.back()[2] = pt[2];
    }

    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors(anchors);

    auto cl = xq_VesselCenterline::New();
    cl->SetSegment(segment, 0);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(cl);
    return node;
}

mitk::DataNode::Pointer LoadXqModel(const std::string& fullPath)
{
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(fullPath.c_str());
    reader->Update();
    vtkPolyData* pd = reader->GetOutput();
    if (!pd || pd->GetNumberOfPoints() == 0)
        return nullptr;

    auto* geom = new xq_PolyGeometry();

    vtkNew<vtkPolyData> pdCopy;
    pdCopy->DeepCopy(pd);
    geom->SetWholeVtkPolyData(pdCopy);
    geom->RestoreFaceInfoFromPolyData(pdCopy);

    auto model = xq_Model::New();
    model->SetModelElement(std::unique_ptr<xq_VascularGeometry>(geom), 0);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(model);
    return node;
}

mitk::DataNode::Pointer LoadXqMesh(const std::string& fullPath)
{
    vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(fullPath.c_str());
    reader->Update();
    vtkUnstructuredGrid* ug = reader->GetOutput();
    if (!ug || ug->GetNumberOfPoints() == 0)
        return nullptr;

    auto* tetGenGrid = new xq_TetGenGrid();

    vtkSmartPointer<vtkUnstructuredGrid> ugCopy = vtkSmartPointer<vtkUnstructuredGrid>::New();
    ugCopy->DeepCopy(ug);
    tetGenGrid->SetVolumeMesh(ugCopy);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGenGrid, 0);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(mitkGrid);
    return node;
}

mitk::DataNode::Pointer LoadXqProfileGroup(const std::string& fullPath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(fullPath.c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    auto* root = doc.FirstChildElement("xq_profile_group");
    if (!root) return nullptr;

    auto group = xq_ProfileGroup::New();

    // Restore attributes
    auto* attrsElem = root->FirstChildElement("attributes");
    if (attrsElem)
    {
        for (auto* attrElem = attrsElem->FirstChildElement("attribute");
             attrElem; attrElem = attrElem->NextSiblingElement("attribute"))
        {
            const char* key = attrElem->Attribute("key");
            const char* val = attrElem->Attribute("value");
            if (key && val)
                group->SetAttribute(key, val);
        }
    }

    // Restore profiles
    auto* profilesElem = root->FirstChildElement("profiles");
    if (profilesElem)
    {
        for (auto* profElem = profilesElem->FirstChildElement("profile");
             profElem; profElem = profElem->NextSiblingElement("profile"))
        {
            int pathPosIndex = profElem->IntAttribute("path_pos_index", 0);

            auto* profile = new xq_PolygonalProfile();
            profile->SetPathPosIndex(pathPosIndex);

            const char* method = profElem->Attribute("method");
            if (method) profile->SetMethod(method);

            auto* ptsElem = profElem->FirstChildElement("points");
            if (ptsElem)
            {
                std::vector<mitk::Point3D> anchorPts;
                for (auto* ptElem = ptsElem->FirstChildElement("point");
                     ptElem; ptElem = ptElem->NextSiblingElement("point"))
                {
                    double x = ptElem->DoubleAttribute("x", 0.0);
                    double y = ptElem->DoubleAttribute("y", 0.0);
                    double z = ptElem->DoubleAttribute("z", 0.0);
                    mitk::Point3D pt;
                    pt[0] = x; pt[1] = y; pt[2] = z;
                    anchorPts.push_back(pt);
                }
                profile->SetAnchorPoints(anchorPts);
                profile->GenerateProfilePoints();
            }

            // Restore placement frame (position + slice plane)
            auto* placementElem = profElem->FirstChildElement("placement");
            if (placementElem)
            {
                mitk::Point3D center;
                center[0] = placementElem->DoubleAttribute("cx", 0.0);
                center[1] = placementElem->DoubleAttribute("cy", 0.0);
                center[2] = placementElem->DoubleAttribute("cz", 0.0);
                profile->SetProfileCenter(center);

                if (placementElem->Attribute("nx") != nullptr)
                {
                    mitk::Vector3D normal;
                    normal[0] = placementElem->DoubleAttribute("nx", 0.0);
                    normal[1] = placementElem->DoubleAttribute("ny", 0.0);
                    normal[2] = placementElem->DoubleAttribute("nz", 0.0);

                    mitk::Point3D origin;
                    origin[0] = placementElem->DoubleAttribute("ox", 0.0);
                    origin[1] = placementElem->DoubleAttribute("oy", 0.0);
                    origin[2] = placementElem->DoubleAttribute("oz", 0.0);

                    auto plane = mitk::PlaneGeometry::New();
                    plane->InitializePlane(origin, normal);
                    profile->SetSlicePlane(plane);
                    profile->PlaceProfile(center);
                }
            }

            group->AppendProfile(profile, pathPosIndex, 0);
        }
    }

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(group);

    auto* nameElem = root->FirstChildElement("name");
    if (nameElem && nameElem->GetText())
        node->SetName(nameElem->GetText());

    return node;
}

mitk::DataNode::Pointer LoadXqSolverJob(const std::string& fullPath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(fullPath.c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    auto* root = doc.FirstChildElement("xq_simulation_job");
    if (!root) return nullptr;

    // Read basic parameters
    auto* basicElem = root->FirstChildElement("basic");
    int numTimesteps = basicElem ? basicElem->IntAttribute("num_timesteps", 200) : 200;
    double timeStepSize = basicElem ? basicElem->DoubleAttribute("time_step_size", 0.001) : 0.001;
    int numCycles = basicElem ? basicElem->IntAttribute("num_cycles", 2) : 2;

    auto* nameElem = root->FirstChildElement("name");
    std::string jobName = (nameElem && nameElem->GetText()) ? nameElem->GetText() : "SolverJob";

    auto job = xq_SolverJob::Builder()
        .withName(jobName)
        .withTimesteps(numTimesteps, timeStepSize)
        .withCycles(numCycles)
        .build();

    // Fluid properties
    auto* fluidElem = root->FirstChildElement("fluid");
    if (fluidElem)
    {
        job->SetFluidDensity(fluidElem->DoubleAttribute("density", 1.06));
        job->SetFluidViscosity(fluidElem->DoubleAttribute("viscosity", 0.04));
        job->SetInitialPressure(fluidElem->DoubleAttribute("initial_pressure", 0.0));
        job->SetInitialVelocity(fluidElem->DoubleAttribute("initial_velocity", 0.0));
    }

    // Wall properties
    auto* wallElem = root->FirstChildElement("wall");
    if (wallElem)
    {
        job->SetDeformable(wallElem->BoolAttribute("deformable", false));
        job->SetWallThickness(wallElem->DoubleAttribute("thickness", 0.5));
        job->SetWallElasticModulus(wallElem->DoubleAttribute("elastic_modulus", 4.0e6));
        job->SetWallPoissonRatio(wallElem->DoubleAttribute("poisson_ratio", 0.45));
        job->SetWallDensity(wallElem->DoubleAttribute("density", 1.0));
    }

    // Solver properties
    auto* solverElem = root->FirstChildElement("solver");
    if (solverElem)
    {
        const char* solverType = solverElem->Attribute("type");
        if (solverType)
            job->SetSolverType(solverType);
        job->SetNumLinearIterations(solverElem->IntAttribute("linear_iterations", 10));
        job->SetNumNonlinearIterations(solverElem->IntAttribute("nonlinear_iterations", 2));
    }

    // Boundary conditions
    auto* bcsElem = root->FirstChildElement("boundary_conditions");
    if (bcsElem)
    {
        for (auto* bcElem = bcsElem->FirstChildElement("bc");
             bcElem; bcElem = bcElem->NextSiblingElement("bc"))
        {
            xq_BoundaryCondition bc;
            const char* face = bcElem->Attribute("face");
            const char* role = bcElem->Attribute("role");
            const char* type = bcElem->Attribute("type");
            if (face) bc.faceName = face;
            if (role) bc.faceRole = role;
            if (type) bc.bcType = type;

            for (auto* paramElem = bcElem->FirstChildElement("param");
                 paramElem; paramElem = paramElem->NextSiblingElement("param"))
            {
                const char* key = paramElem->Attribute("key");
                const char* val = paramElem->Attribute("value");
                if (key && val)
                    bc.parameters[key] = val;
            }
            // Read waveform if present
            auto* wfElem = bcElem->FirstChildElement("waveform");
            if (wfElem)
            {
                for (auto* wpElem = wfElem->FirstChildElement("point");
                     wpElem; wpElem = wpElem->NextSiblingElement("point"))
                {
                    double t = wpElem->DoubleAttribute("t", 0.0);
                    double q = wpElem->DoubleAttribute("q", 0.0);
                    bc.waveform.push_back({t, q});
                }
            }
            job->AddBoundaryCondition(bc);
        }
    }

    // Generic property maps
    auto readPropMap = [&](const char* elemName,
                           std::function<void(std::string_view, std::string_view)> setter)
    {
        auto* elem = root->FirstChildElement(elemName);
        if (!elem) return;
        for (auto* propElem = elem->FirstChildElement("prop");
             propElem; propElem = propElem->NextSiblingElement("prop"))
        {
            const char* key = propElem->Attribute("key");
            const char* val = propElem->Attribute("value");
            if (key && val)
                setter(key, val);
        }
    };
    readPropMap("basic_props", [&](auto k, auto v) { job->SetBasicProp(k, v); });
    readPropMap("wall_props", [&](auto k, auto v) { job->SetWallProp(k, v); });
    readPropMap("solver_props", [&](auto k, auto v) { job->SetSolverProp(k, v); });
    readPropMap("run_props", [&](auto k, auto v) { job->SetRunProp(k, v); });

    // Cap and IC property maps
    auto readCapPropMap = [&](const char* elemName,
                              std::function<void(std::string_view, std::string_view, std::string_view)> setter)
    {
        auto* elem = root->FirstChildElement(elemName);
        if (!elem) return;
        for (auto* capElem = elem->FirstChildElement("cap");
             capElem; capElem = capElem->NextSiblingElement("cap"))
        {
            const char* capName = capElem->Attribute("name");
            if (!capName) continue;
            for (auto* propElem = capElem->FirstChildElement("prop");
                 propElem; propElem = propElem->NextSiblingElement("prop"))
            {
                const char* key = propElem->Attribute("key");
                const char* val = propElem->Attribute("value");
                if (key && val)
                    setter(capName, key, val);
            }
        }
    };
    readCapPropMap("cap_props", [&](auto cap, auto k, auto v) { job->SetCapProp(cap, k, v); });
    readCapPropMap("ic_props", [&](auto cap, auto k, auto v) { job->SetIcProp(cap, k, v); });

    auto mitkJob = xq_MitkSolverJob::New();

    // model_name and mesh_name
    auto* modelElem = root->FirstChildElement("model_name");
    if (modelElem && modelElem->GetText())
        mitkJob->SetModelName(modelElem->GetText());

    auto* meshElem = root->FirstChildElement("mesh_name");
    if (meshElem && meshElem->GetText())
        mitkJob->SetMeshName(meshElem->GetText());

    auto* statusElem = root->FirstChildElement("status");
    if (statusElem && statusElem->GetText())
        mitkJob->SetStatus(statusElem->GetText());

    mitkJob->SetSimJob(std::move(job), 0);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(mitkJob);
    node->SetName(jobName);
    return node;
}

} // anonymous namespace

xq_ProjectDataReadResult xq_ProjectDataReader::LoadNodeData(
    const std::string& fullPath)
{
    xq_ProjectDataReadResult result;

    // Dispatch by XQ-specific extensions first
    if (EndsWith(fullPath, ".xqpath.vtp"))
    {
        auto node = LoadXqPath(fullPath);
        if (node)
        {
            result.nodes.push_back(node);
            result.ok = true;
        }
        else
            result.diagnostics.push_back("Failed to load xqpath: " + fullPath);
        return result;
    }

    if (EndsWith(fullPath, ".xqprofiles.xml"))
    {
        auto node = LoadXqProfileGroup(fullPath);
        if (node)
        {
            result.nodes.push_back(node);
            result.ok = true;
        }
        else
            result.diagnostics.push_back("Failed to load xqprofiles: " + fullPath);
        return result;
    }

    if (EndsWith(fullPath, ".xqmodel.vtp"))
    {
        auto node = LoadXqModel(fullPath);
        if (node)
        {
            result.nodes.push_back(node);
            result.ok = true;
        }
        else
            result.diagnostics.push_back("Failed to load xqmodel: " + fullPath);
        return result;
    }

    if (EndsWith(fullPath, ".xqmesh.vtu"))
    {
        auto node = LoadXqMesh(fullPath);
        if (node)
        {
            result.nodes.push_back(node);
            result.ok = true;
        }
        else
            result.diagnostics.push_back("Failed to load xqmesh: " + fullPath);
        return result;
    }

    if (EndsWith(fullPath, ".xqsim.xml"))
    {
        auto node = LoadXqSolverJob(fullPath);
        if (node)
        {
            result.nodes.push_back(node);
            result.ok = true;
        }
        else
            result.diagnostics.push_back("Failed to load xqsim: " + fullPath);
        return result;
    }

    // Fallback: MITK native types (.nrrd, .vti, .vtp, .vtu, etc.)
    try
    {
        std::vector<mitk::BaseData::Pointer> loadedData =
            mitk::IOUtil::Load(fullPath);
        for (auto& data : loadedData)
        {
            mitk::DataNode::Pointer dataNode = mitk::DataNode::New();
            dataNode->SetData(data);
            result.nodes.push_back(dataNode);
        }
        result.ok = !result.nodes.empty();
        if (!result.ok)
            result.diagnostics.push_back("IOUtil::Load returned no data");
    }
    catch (const mitk::Exception& e)
    {
        result.diagnostics.push_back(
            std::string("IOUtil::Load failed: ") + e.GetDescription());
    }

    return result;
}
