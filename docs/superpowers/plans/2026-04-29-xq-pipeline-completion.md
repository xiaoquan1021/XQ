# XQ Pipeline Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the XQ pipeline so that custom XQ types survive project save/load round-trips, right-click context menu actions complete the full pipeline, and QA/readiness/checker are integrated.

**Architecture:** Fix the ProjectDataReader to restore XQ custom types (xq_Model, xq_MitkGrid, xq_VesselCenterline, xq_ProfileGroup, xq_MitkSolverJob) instead of generic MITK types. Give ProfileGroup and MitkSolverJob real file writers instead of metadata-only. Add missing context menu actions. Wire QA reports into pipeline creation and views.

**Tech Stack:** C++17, MITK, VTK, tinyxml2, Qt5, CMake

---

### Task A1: Fix ProjectDataWriter — ProfileGroup and MitkSolverJob real file writing

**Files:**
- Modify: `Code/Source/xq4gui/Modules/ProjectManagement/xq_ProjectDataWriter.cxx`

- [ ] **Step 1: Read the ProfileGroup and SolverJob headers to understand their API**

Read these files to understand the data structures:
- `Code/Source/xq4gui/Modules/Segmentation/xq_ProfileGroup.h`
- `Code/Source/xq4gui/Modules/Simulation/xq_SolverJob.h`
- `Code/Source/xq4gui/Modules/Simulation/xq_MitkSolverJob.h`

- [ ] **Step 2: Rewrite ProfileGroup writer to produce .xqprofiles.xml**

Replace the metadata-only block (lines 221-230) in `xq_ProjectDataWriter.cxx` with:

```cpp
    // xq_ProfileGroup: serialize to .xqprofiles.xml
    if (className == "xq_ProfileGroup")
    {
        auto* group = dynamic_cast<const xq_ProfileGroup*>(node->GetData());
        if (!group)
        {
            result.diagnostics.push_back("dynamic_cast<xq_ProfileGroup> failed");
            return result;
        }

        std::string xmlPath = savePath + ".xqprofiles.xml";

        tinyxml2::XMLDocument doc;
        auto* decl = doc.NewDeclaration();
        doc.InsertFirstChild(decl);

        auto* root = doc.NewElement("xq_profile_group");
        root->SetAttribute("version", "1");
        doc.InsertEndChild(root);

        // name
        auto* nameElem = doc.NewElement("name");
        nameElem->SetText(node->GetName().c_str());
        root->InsertEndChild(nameElem);

        // attributes (path_name, image_name, etc.)
        auto* attrsElem = doc.NewElement("attributes");
        auto attrKeys = group->GetAttributeKeys();
        for (const auto& key : attrKeys)
        {
            std::string val = group->GetAttribute(key);
            if (!val.empty())
            {
                auto* attrElem = doc.NewElement("attribute");
                attrElem->SetAttribute("key", key.c_str());
                attrElem->SetAttribute("value", val.c_str());
                attrsElem->InsertEndChild(attrElem);
            }
        }
        root->InsertEndChild(attrsElem);

        // profiles — iterate and save position + type info
        auto* profilesElem = doc.NewElement("profiles");
        int numProfiles = group->GetNumberOfProfiles();
        for (int i = 0; i < numProfiles; ++i)
        {
            auto* profile = group->GetProfile(i);
            if (!profile) continue;

            auto* profElem = doc.NewElement("profile");
            profElem->SetAttribute("path_pos_index", profile->GetPathPosIndex());

            // Save profile points
            auto* ptsElem = doc.NewElement("points");
            int numPts = profile->GetNumberOfPoints();
            for (int j = 0; j < numPts; ++j)
            {
                auto pt = profile->GetPoint(j);
                auto* ptElem = doc.NewElement("point");
                ptElem->SetAttribute("x", pt[0]);
                ptElem->SetAttribute("y", pt[1]);
                ptElem->SetAttribute("z", pt[2]);
                ptsElem->InsertEndChild(ptElem);
            }
            profElem->InsertEndChild(ptsElem);
            profilesElem->InsertEndChild(profElem);
        }
        root->InsertEndChild(profilesElem);

        if (doc.SaveFile(xmlPath.c_str()) != tinyxml2::XML_SUCCESS)
        {
            result.diagnostics.push_back("Failed to write xqprofiles.xml");
            return result;
        }

        result.ok = true;
        result.savedPath = xmlPath;
        result.relPath = subdir + "/" + xq_WorkspaceManager::GetFileName(xmlPath);
        return result;
    }
```

- [ ] **Step 3: Rewrite MitkSolverJob writer to produce .xqsim.xml**

Replace the metadata-only block (lines 210-219) in `xq_ProjectDataWriter.cxx` with:

```cpp
    // xq_MitkSolverJob: serialize to .xqsim.xml
    if (className == "xq_MitkSolverJob")
    {
        auto* mitkJob = dynamic_cast<const xq_MitkSolverJob*>(node->GetData());
        if (!mitkJob)
        {
            result.diagnostics.push_back("dynamic_cast<xq_MitkSolverJob> failed");
            return result;
        }

        const xq_SolverJob* job = mitkJob->GetSolverJob();
        if (!job)
        {
            result.diagnostics.push_back("xq_MitkSolverJob has no xq_SolverJob");
            return result;
        }

        std::string xmlPath = savePath + ".xqsim.xml";

        tinyxml2::XMLDocument doc;
        auto* decl = doc.NewDeclaration();
        doc.InsertFirstChild(decl);

        auto* root = doc.NewElement("xq_simulation_job");
        root->SetAttribute("version", "1");
        doc.InsertEndChild(root);

        auto* nameElem = doc.NewElement("name");
        nameElem->SetText(node->GetName().c_str());
        root->InsertEndChild(nameElem);

        auto* modelElem = doc.NewElement("model_name");
        modelElem->SetText(job->GetModelName().c_str());
        root->InsertEndChild(modelElem);

        auto* meshElem = doc.NewElement("mesh_name");
        meshElem->SetText(job->GetMeshName().c_str());
        root->InsertEndChild(meshElem);

        auto* statusElem = doc.NewElement("status");
        statusElem->SetText(job->GetStatus().c_str());
        root->InsertEndChild(statusElem);

        auto* basicElem = doc.NewElement("basic");
        basicElem->SetAttribute("num_timesteps", job->GetNumTimesteps());
        basicElem->SetAttribute("time_step_size", job->GetTimeStepSize());
        basicElem->SetAttribute("num_cycles", job->GetNumCycles());
        basicElem->SetAttribute("save_frequency", job->GetSaveFrequency());
        root->InsertEndChild(basicElem);

        auto* fluidElem = doc.NewElement("fluid");
        fluidElem->SetAttribute("density", job->GetFluidDensity());
        fluidElem->SetAttribute("viscosity", job->GetFluidViscosity());
        fluidElem->SetAttribute("initial_pressure", job->GetInitialPressure());
        fluidElem->SetAttribute("initial_velocity", job->GetInitialVelocity());
        root->InsertEndChild(fluidElem);

        auto* wallElem = doc.NewElement("wall");
        wallElem->SetAttribute("deformable", job->GetWallDeformable() ? "true" : "false");
        wallElem->SetAttribute("thickness", job->GetWallThickness());
        wallElem->SetAttribute("elastic_modulus", job->GetWallElasticModulus());
        wallElem->SetAttribute("poisson_ratio", job->GetWallPoissonRatio());
        wallElem->SetAttribute("density", job->GetWallDensity());
        root->InsertEndChild(wallElem);

        // boundary conditions
        auto* bcsElem = doc.NewElement("boundary_conditions");
        const auto& bcs = job->GetBoundaryConditions();
        for (const auto& bc : bcs)
        {
            auto* bcElem = doc.NewElement("bc");
            bcElem->SetAttribute("face", bc.faceName.c_str());
            bcElem->SetAttribute("role", bc.faceRole.c_str());
            bcElem->SetAttribute("type", bc.bcType.c_str());
            for (const auto& param : bc.parameters)
            {
                auto* paramElem = doc.NewElement("param");
                paramElem->SetAttribute("key", param.first.c_str());
                paramElem->SetAttribute("value", param.second.c_str());
                bcElem->InsertEndChild(paramElem);
            }
            bcsElem->InsertEndChild(bcElem);
        }
        root->InsertEndChild(bcsElem);

        if (doc.SaveFile(xmlPath.c_str()) != tinyxml2::XML_SUCCESS)
        {
            result.diagnostics.push_back("Failed to write xqsim.xml");
            return result;
        }

        result.ok = true;
        result.savedPath = xmlPath;
        result.relPath = subdir + "/" + xq_WorkspaceManager::GetFileName(xmlPath);
        return result;
    }
```

- [ ] **Step 4: Add tinyxml2 include to ProjectDataWriter.cxx**

Add at top of includes:
```cpp
#include <tinyxml2.h>
```

- [ ] **Step 5: Change model writer extension to .xqmodel.vtp**

In the model writer block, change:
```cpp
std::string vtpPath = savePath + ".vtp";
```
to:
```cpp
std::string vtpPath = savePath + ".xqmodel.vtp";
```

- [ ] **Step 6: Change mesh writer extension to .xqmesh.vtu**

In the mesh writer block, change:
```cpp
std::string vtuPath = savePath + ".vtu";
```
to:
```cpp
std::string vtuPath = savePath + ".xqmesh.vtu";
```

- [ ] **Step 7: Change path writer extension to .xqpath.vtp**

In the path writer block, change:
```cpp
std::string vtpPath = savePath + ".vtp";
```
to:
```cpp
std::string vtpPath = savePath + ".xqpath.vtp";
```

- [ ] **Step 8: Build and verify**

```bash
cmake --build /home/xiaoquan/XQ/build --target xqModuleProjectManagement XQ -j2
```

---

### Task A2: Fix ProjectDataReader to restore XQ custom types

**Files:**
- Modify: `Code/Source/xq4gui/Modules/ProjectManagement/xq_ProjectDataReader.cxx`

- [ ] **Step 1: Rewrite LoadNodeData to dispatch by extension**

Replace the entire body of `xq_ProjectDataReader.cxx` with:

```cpp
#include "xq_ProjectDataReader.h"

#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_ProfileGroup.h>
#include <xq_Model.h>
#include <xq_VascularGeometry.h>
#include <xq_PolyGeometry.h>
#include <xq_MitkGrid.h>
#include <xq_Grid.h>
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
    if (!reader->CanReadFile(fullPath.c_str()))
        return nullptr;
    reader->Update();
    vtkPolyData* pd = reader->GetOutput();
    if (!pd || pd->GetNumberOfPoints() == 0)
        return nullptr;

    // Reconstruct xq_VesselCenterline from polyline points
    auto* cl = xq_VesselCenterline::New();
    // For now, reconstruct as a single segment with the points as anchors
    auto* segment = xq_CenterlineSegment::New();
    vtkPoints* vtkPts = pd->GetPoints();
    for (vtkIdType i = 0; i < vtkPts->GetNumberOfPoints(); ++i)
    {
        double pt[3];
        vtkPts->GetPoint(i, pt);
        segment->AddAnchorPosition(pt[0], pt[1], pt[2]);
    }
    cl->AddSegment(segment);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(cl);
    return node;
}

mitk::DataNode::Pointer LoadXqModel(const std::string& fullPath)
{
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(fullPath.c_str());
    if (!reader->CanReadFile(fullPath.c_str()))
        return nullptr;
    reader->Update();
    vtkPolyData* pd = reader->GetOutput();
    if (!pd || pd->GetNumberOfPoints() == 0)
        return nullptr;

    // Create xq_Model wrapping the polydata
    auto* model = xq_Model::New();
    auto* geom = xq_VascularGeometry::New();

    // Make a deep copy so the geometry owns its polydata
    vtkNew<vtkPolyData> pdCopy;
    pdCopy->DeepCopy(pd);
    geom->SetWholeVtkPolyData(pdCopy);

    // Restore face info that was embedded during save
    geom->RestoreFaceInfoFromPolyData(pdCopy);

    model->AddModelElement(geom);

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(model);
    return node;
}

mitk::DataNode::Pointer LoadXqMesh(const std::string& fullPath)
{
    vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(fullPath.c_str());
    if (!reader->CanReadFile(fullPath.c_str()))
        return nullptr;
    reader->Update();
    vtkUnstructuredGrid* ug = reader->GetOutput();
    if (!ug || ug->GetNumberOfPoints() == 0)
        return nullptr;

    // Create xq_MitkGrid
    auto* mitkGrid = xq_MitkGrid::New();
    auto* meshObj = xq_Grid::New();

    vtkSmartPointer<vtkUnstructuredGrid> ugCopy = vtkSmartPointer<vtkUnstructuredGrid>::New();
    ugCopy->DeepCopy(ug);
    meshObj->SetVolumeMesh(ugCopy);
    mitkGrid->SetMesh(meshObj, 0);

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

    auto* group = xq_ProfileGroup::New();

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
            auto* ptsElem = profElem->FirstChildElement("points");
            if (ptsElem)
            {
                auto* profile = group->CreateProfile(pathPosIndex);
                if (profile)
                {
                    for (auto* ptElem = ptsElem->FirstChildElement("point");
                         ptElem; ptElem = ptElem->NextSiblingElement("point"))
                    {
                        double x = ptElem->DoubleAttribute("x", 0.0);
                        double y = ptElem->DoubleAttribute("y", 0.0);
                        double z = ptElem->DoubleAttribute("z", 0.0);
                        profile->AddPoint(x, y, z);
                    }
                }
            }
        }
    }

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(group);

    // Set name from XML
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

    auto* job = xq_SolverJob::New();
    auto* mitkJob = xq_MitkSolverJob::New();

    // name
    auto* nameElem = root->FirstChildElement("name");
    std::string jobName = nameElem && nameElem->GetText() ? nameElem->GetText() : "SolverJob";

    // model_name
    auto* modelElem = root->FirstChildElement("model_name");
    if (modelElem && modelElem->GetText())
        job->SetModelName(modelElem->GetText());

    // mesh_name
    auto* meshElem = root->FirstChildElement("mesh_name");
    if (meshElem && meshElem->GetText())
        job->SetMeshName(meshElem->GetText());

    // status
    auto* statusElem = root->FirstChildElement("status");
    if (statusElem && statusElem->GetText())
        job->SetStatus(statusElem->GetText());

    // basic
    auto* basicElem = root->FirstChildElement("basic");
    if (basicElem)
    {
        job->SetNumTimesteps(basicElem->IntAttribute("num_timesteps", 100));
        job->SetTimeStepSize(basicElem->DoubleAttribute("time_step_size", 0.001));
        job->SetNumCycles(basicElem->IntAttribute("num_cycles", 1));
        job->SetSaveFrequency(basicElem->IntAttribute("save_frequency", 1));
    }

    // fluid
    auto* fluidElem = root->FirstChildElement("fluid");
    if (fluidElem)
    {
        job->SetFluidDensity(fluidElem->DoubleAttribute("density", 1.06));
        job->SetFluidViscosity(fluidElem->DoubleAttribute("viscosity", 0.04));
        job->SetInitialPressure(fluidElem->DoubleAttribute("initial_pressure", 0.0));
        job->SetInitialVelocity(fluidElem->DoubleAttribute("initial_velocity", 0.0));
    }

    // wall
    auto* wallElem = root->FirstChildElement("wall");
    if (wallElem)
    {
        job->SetWallDeformable(wallElem->BoolAttribute("deformable", false));
        job->SetWallThickness(wallElem->DoubleAttribute("thickness", 0.0));
        job->SetWallElasticModulus(wallElem->DoubleAttribute("elastic_modulus", 0.0));
        job->SetWallPoissonRatio(wallElem->DoubleAttribute("poisson_ratio", 0.0));
        job->SetWallDensity(wallElem->DoubleAttribute("density", 0.0));
    }

    // boundary conditions
    auto* bcsElem = root->FirstChildElement("boundary_conditions");
    if (bcsElem)
    {
        std::vector<xq_BoundaryCondition> bcs;
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
            bcs.push_back(bc);
        }
        job->SetBoundaryConditions(bcs);
    }

    mitkJob->SetSolverJob(job);

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
```

- [ ] **Step 2: Build and verify**

```bash
cmake --build /home/xiaoquan/XQ/build --target xqModuleProjectManagement XQ -j2
```

---

### Task A3: Update OpenProject to use ProjectDataReader

**Files:**
- Modify: `Code/Source/xq4gui/Modules/ProjectManagement/xq_WorkspaceManager.cxx`

- [ ] **Step 1: Include ProjectDataReader header**

Add at top of `xq_WorkspaceManager.cxx`:
```cpp
#include "xq_ProjectDataReader.h"
```

- [ ] **Step 2: Replace mitk::IOUtil::Load with xq_ProjectDataReader::LoadNodeData**

In `OpenProject()`, replace lines 303-349 (the data loading loop) with:

```cpp
        // Load data using XQ custom reader (handles XQ types first, falls back to MITK IOUtil)
        const auto readResult = xq_ProjectDataReader::LoadNodeData(fullPath);
        for (auto& dataNode : readResult.nodes)
        {
            // Only set name from filename if reader didn't provide one
            if (dataNode->GetName().empty())
                dataNode->SetName(GetFileNameWithoutExtension(relPath));

            // Initialize display properties based on data category
            // AND stamp the pipeline stage so downstream services and
            // the folder-based migration can resolve this node.
            if (normalizedType == "Images") {
                dataNode->SetVisibility(true);
                dataNode->SetOpacity(1.0);
            } else if (normalizedType == "Paths") {
                dataNode->SetVisibility(false);
                dataNode->SetFloatProperty("point size", 1.0f);
                if (!xq::pipeline::HasStage(dataNode))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::Path);
            } else if (normalizedType == "Segmentations") {
                dataNode->SetVisibility(false);
                if (!xq::pipeline::HasStage(dataNode))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::ContourGroup);
            } else if (normalizedType == "Models") {
                dataNode->SetVisibility(true);
                if (!xq::pipeline::HasStage(dataNode))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::Model);
            } else if (normalizedType == "Meshes") {
                dataNode->SetVisibility(true);
                dataNode->SetColor(0.0f, 0.8f, 0.2f);
                dataNode->SetProperty("opacity", mitk::FloatProperty::New(0.6f));
                if (!xq::pipeline::HasStage(dataNode))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::VolumeMesh);
            } else if (normalizedType == "Simulations") {
                dataNode->SetVisibility(true);
                if (!xq::pipeline::HasStage(dataNode))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::SimulationPrep);
            } else {
                dataNode->SetVisibility(false);
            }

            // Restore pipeline metadata from sidecar file if present
            std::string metaPath = fullPath + ".xqmeta.xml";
            xq_NodeMetadataIO::ReadNodeMetadata(dataNode, metaPath);

            if (parentFolderNode.IsNotNull())
                dataStorage->Add(dataNode, parentFolderNode);
            else
                dataStorage->Add(dataNode, projectNode);
        }

        // Log any reader diagnostics
        for (const auto& diag : readResult.diagnostics)
        {
            MITK_WARN << "OpenProject: " << diag;
        }
```

- [ ] **Step 3: Build and verify**

```bash
cmake --build /home/xiaoquan/XQ/build --target xqModuleProjectManagement XQ -j2
```

---

### Task C: Add missing context menu actions

**Files:**
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.vascularmodeling/plugin.xml`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.vascularmodeling/src/internal/xq_ModelCreateAction.h`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.vascularmodeling/src/internal/xq_ModelCreateAction.cxx`
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/plugin.xml`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_SimJobCreateAction.h`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_SimJobCreateAction.cxx`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_SolverExportAction.h`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_SolverExportAction.cxx`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_ResultImportAction.h`
- Create: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_ResultImportAction.cxx`

These need to be created following the pattern of existing actions like `xq_MeshCreateAction` in gridgeneration plugin. Each action implements `xqmitk::IContextMenuAction`. After creating the files, register in plugin.xml and add to the plugin's CMakeLists/fils.cmake.

---

### Task D: Wire Model QA into pipeline and view

**Files:**
- Modify: `Code/Source/xq4gui/Modules/Model/Common/xq_ModelPipeline.cxx`
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.vascularmodeling/src/internal/xq_VascularModelingView.cxx`

After creating a model node in `CreateModel()`, call `xq_ModelQuality::Evaluate()` and write properties:
```cpp
auto qa = xq_ModelQuality::Evaluate(modelElement->GetWholeVtkPolyData());
modelNode->SetBoolProperty("xq.model.qa.ok", qa.ok);
modelNode->SetIntProperty("xq.model.qa.boundary_edges", qa.boundaryEdges);
modelNode->SetIntProperty("xq.model.qa.non_manifold_edges", qa.nonManifoldEdges);
modelNode->SetIntProperty("xq.model.qa.connected_components", qa.connectedComponents);
modelNode->SetBoolProperty("xq.model.qa.has_face_ids", qa.hasFaceIds);
modelNode->SetIntProperty("xq.model.face_count", qa.faceCount);
```

In `ShowModelStatistics()`, use `xq_ModelQuality::Evaluate()` to display QA summary alongside existing stats.

---

### Task E: Wire Contour readiness report into view and node properties

**Files:**
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.lumencontouring/src/internal/xq_LumenContouringView.cxx`
- Modify: `Code/Source/xq4gui/Modules/Segmentation/xq_SegmentationPipeline.cxx`

After contour group creation/modification, call `xq_SegmentationUtils::BuildReadinessReport()` and write properties:
```cpp
auto report = xq_SegmentationUtils::BuildReadinessReport(group);
node->SetBoolProperty("xq.contour.ready", report.loftReady && report.modelingReady);
node->SetIntProperty("xq.contour.profile_count", report.profileCount);
node->SetIntProperty("xq.contour.missing_count", report.missingCount);
node->SetIntProperty("xq.contour.warning_count", static_cast<int>(report.warnings.size()));
node->SetIntProperty("xq.contour.error_count", static_cast<int>(report.errors.size()));
```

---

### Task F: Simulation job save UI parameters completely

**Files:**
- Modify: `Code/Source/xq4gui/Modules/Simulation/xq_SimulationPrepPipeline.h`
- Modify: `Code/Source/xq4gui/Modules/Simulation/xq_SimulationPrepPipeline.cxx`
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/src/internal/xq_HemodynamicsView.cxx`

Extend `xq_SimulationPrepRequest` to include all fields (fluidDensity, fluidViscosity, wallThickness, etc.) and boundary conditions. In `SaveJob()`, read all UI values and fill the request. In `CreateOrUpdateSimulationPrep()`, write all fields to the solver job.

---

### Task G: Solver export correctness improvements

**Files:**
- Modify: `Code/Source/xq4gui/Modules/Simulation/xq_SvPreWriter.cxx`

Ensure `filesWritten` only lists files actually created. Add diagnostic warnings for placeholder files. Verify `solver.inp` contains actual density/viscosity/timestep values from the job.

---

### Task H: Result import Data Manager action

**Files:**
- Modify: `Code/Source/xq4gui/Plugins/org.xq.pipeline.hemodynamics/plugin.xml`
- Modify: `Code/Source/xq4gui/Modules/Simulation/xq_ResultImport.cxx`

Add `ResultImportAction` implementing `IContextMenuAction`. Support both `vtkPolyData` and `vtkUnstructuredGrid` in result import. Register in plugin.xml on `xq_SimulationFolder`.

---

### Task I: Pipeline consistency checker enhancements

**Files:**
- Modify: `Code/Source/xq4gui/Modules/Common/xq_PipelineConsistency.cxx`

Add checks for: stage vs folder mismatch, QA property failures, missing data type for stage, metadata-only orphans.

---

### Build Verification

After all tasks, run full build:
```bash
cmake --build /home/xiaoquan/XQ/build --target org_xq_core_datamanager org_xq_core_workspace org_xq_pipeline_vesselplanning org_xq_pipeline_lumencontouring org_xq_pipeline_vascularmodeling org_xq_pipeline_gridgeneration org_xq_pipeline_hemodynamics XQ -j2
```
