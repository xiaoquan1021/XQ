#include "xq_ProjectDataWriter.h"

#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_ProfileGroup.h>
#include <xq_LumenProfile.h>
#include <xq_Model.h>
#include <xq_VascularGeometry.h>
#include <xq_MitkGrid.h>
#include <xq_Grid.h>
#include <xq_MitkSolverJob.h>
#include <xq_WorkspaceManager.h>
#include <xq_MitkROMJob.h>
#include <xq_MitkROMJobIO.h>
#include <xq_MitkMultiPhysicsJob.h>
#include <xq_MitkMultiPhysicsJobIO.h>
#include <xq_MitkSeg3D.h>

#include <mitkIOUtil.h>

#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <tinyxml2.h>

#include <functional>

namespace {

bool IsMitkNativeType(const std::string& className)
{
    return (className == "Image" ||
            className == "Surface" ||
            className == "PointSet" ||
            className == "LabelSetImage");
}

bool HasFileExtension(const std::string& path)
{
    const auto slashPos = path.find_last_of("/\\");
    const auto dotPos = path.find_last_of('.');
    return dotPos != std::string::npos &&
           (slashPos == std::string::npos || dotPos > slashPos);
}

std::string DefaultMitkNativeExtension(const std::string& className)
{
    if (className == "Image" || className == "LabelSetImage")
        return ".nrrd";
    if (className == "Surface")
        return ".vtp";
    if (className == "PointSet")
        return ".mps";
    return "";
}

std::string EnsureMitkNativeExtension(
    const std::string& savePath,
    const std::string& className)
{
    if (HasFileExtension(savePath))
        return savePath;

    return savePath + DefaultMitkNativeExtension(className);
}

} // anonymous namespace

xq_ProjectDataWriteResult xq_ProjectDataWriter::SaveNodeData(
    const mitk::DataNode* node,
    const std::string& savePath,
    const std::string& subdir)
{
    xq_ProjectDataWriteResult result;

    if (!node || !node->GetData())
    {
        result.diagnostics.push_back("null node or data");
        return result;
    }

    std::string className = node->GetData()->GetNameOfClass();

    // MITK-native types: delegate to IOUtil
    if (IsMitkNativeType(className))
    {
        const std::string nativeSavePath =
            EnsureMitkNativeExtension(savePath, className);
        try
        {
            mitk::IOUtil::Save(node->GetData(), nativeSavePath);
            result.ok = true;
            result.savedPath = nativeSavePath;
            result.relPath = subdir + "/" +
                xq_WorkspaceManager::GetFileName(nativeSavePath);
            return result;
        }
        catch (const mitk::Exception& e)
        {
            result.diagnostics.push_back(
                std::string("mitk::IOUtil::Save failed: ") + e.GetDescription());
            return result;
        }
    }

    // xq_VesselCenterline: export anchor points as .vtp polyline
    if (className == "xq_VesselCenterline")
    {
        auto* cl = dynamic_cast<const xq_VesselCenterline*>(node->GetData());
        if (!cl)
        {
            result.diagnostics.push_back("dynamic_cast<xq_VesselCenterline> failed");
            return result;
        }

        auto* segment = cl->GetSegment(0);
        if (!segment)
        {
            result.diagnostics.push_back("xq_VesselCenterline has no segment at t=0");
            return result;
        }

        auto anchors = segment->GetAnchorPositions();
        if (anchors.empty())
        {
            result.diagnostics.push_back("xq_VesselCenterline segment has no anchor points");
            return result;
        }

        vtkNew<vtkPolyData> pd;
        vtkNew<vtkPoints> vtkPts;
        vtkNew<vtkCellArray> lines;
        lines->InsertNextCell(static_cast<vtkIdType>(anchors.size()));
        for (size_t i = 0; i < anchors.size(); ++i)
        {
            vtkPts->InsertNextPoint(anchors[i][0], anchors[i][1], anchors[i][2]);
            lines->InsertCellPoint(static_cast<vtkIdType>(i));
        }
        pd->SetPoints(vtkPts);
        pd->SetLines(lines);

        std::string vtpPath = savePath + ".xqpath.vtp";
        vtkNew<vtkXMLPolyDataWriter> writer;
        writer->SetFileName(vtpPath.c_str());
        writer->SetInputData(pd);
        if (!writer->Write())
        {
            result.diagnostics.push_back("vtkXMLPolyDataWriter::Write() returned false");
            return result;
        }

        result.ok = true;
        result.savedPath = vtpPath;
        result.relPath = subdir + "/" +
            xq_WorkspaceManager::GetFileName(vtpPath);
        return result;
    }

    // xq_Model: export underlying vtkPolyData as .xqmodel.vtp
    if (className == "xq_Model")
    {
        auto* model = dynamic_cast<const xq_Model*>(node->GetData());
        if (!model)
        {
            result.diagnostics.push_back("dynamic_cast<xq_Model> failed");
            return result;
        }

        xq_VascularGeometry* geom = model->GetModelElement(0);
        if (!geom)
        {
            result.diagnostics.push_back("xq_Model has no geometry at t=0");
            return result;
        }

        vtkSmartPointer<vtkPolyData> pd = geom->GetWholeVtkPolyData();
        if (!pd || pd->GetNumberOfPoints() == 0)
        {
            result.diagnostics.push_back("xq_Model geometry has no polydata");
            return result;
        }

        // Embed face metadata (types, names, colors) into the VTP so
        // face roles assigned by user survive save/load round-trips.
        geom->EmbedFaceInfoToPolyData(pd);

        std::string vtpPath = savePath + ".xqmodel.vtp";
        vtkNew<vtkXMLPolyDataWriter> writer;
        writer->SetFileName(vtpPath.c_str());
        writer->SetInputData(pd);
        if (!writer->Write())
        {
            result.diagnostics.push_back("vtkXMLPolyDataWriter::Write() returned false");
            return result;
        }

        result.ok = true;
        result.savedPath = vtpPath;
        result.relPath = subdir + "/" +
            xq_WorkspaceManager::GetFileName(vtpPath);
        return result;
    }

    // xq_MitkGrid: export underlying vtkUnstructuredGrid as .xqmesh.vtu
    if (className == "xq_MitkGrid")
    {
        auto* mitkGrid = dynamic_cast<const xq_MitkGrid*>(node->GetData());
        if (!mitkGrid)
        {
            result.diagnostics.push_back("dynamic_cast<xq_MitkGrid> failed");
            return result;
        }

        xq_Grid* meshObj = mitkGrid->GetMesh(0);
        if (!meshObj)
        {
            result.diagnostics.push_back("xq_MitkGrid has no mesh at t=0");
            return result;
        }

        vtkUnstructuredGrid* ug = meshObj->GetVolumeMesh();
        if (!ug || ug->GetNumberOfPoints() == 0)
        {
            result.diagnostics.push_back("xq_MitkGrid mesh has no volume mesh data");
            return result;
        }

        std::string vtuPath = savePath + ".xqmesh.vtu";
        vtkNew<vtkXMLUnstructuredGridWriter> writer;
        writer->SetFileName(vtuPath.c_str());
        writer->SetInputData(ug);
        if (!writer->Write())
        {
            result.diagnostics.push_back("vtkXMLUnstructuredGridWriter::Write() returned false");
            return result;
        }

        result.ok = true;
        result.savedPath = vtuPath;
        result.relPath = subdir + "/" +
            xq_WorkspaceManager::GetFileName(vtuPath);
        return result;
    }

    // xq_MitkSolverJob: serialize to .xqsim.xml
    if (className == "xq_MitkSolverJob")
    {
        auto* mitkJob = dynamic_cast<const xq_MitkSolverJob*>(node->GetData());
        if (!mitkJob)
        {
            result.diagnostics.push_back("dynamic_cast<xq_MitkSolverJob> failed");
            return result;
        }

        const xq_SolverJob* job = mitkJob->GetSimJob(0);
        if (!job)
        {
            result.diagnostics.push_back("xq_MitkSolverJob has no xq_SolverJob at t=0");
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

        const std::string& modelName = mitkJob->GetModelName();
        if (!modelName.empty())
        {
            auto* modelElem = doc.NewElement("model_name");
            modelElem->SetText(modelName.c_str());
            root->InsertEndChild(modelElem);
        }

        const std::string& meshName = mitkJob->GetMeshName();
        if (!meshName.empty())
        {
            auto* meshElem = doc.NewElement("mesh_name");
            meshElem->SetText(meshName.c_str());
            root->InsertEndChild(meshElem);
        }

        const std::string& status = mitkJob->GetStatus();
        if (!status.empty())
        {
            auto* statusElem = doc.NewElement("status");
            statusElem->SetText(status.c_str());
            root->InsertEndChild(statusElem);
        }

        auto* basicElem = doc.NewElement("basic");
        basicElem->SetAttribute("num_timesteps", job->GetNumTimesteps());
        basicElem->SetAttribute("time_step_size", job->GetTimeStepSize());
        basicElem->SetAttribute("num_cycles", job->GetNumCycles());
        root->InsertEndChild(basicElem);

        auto* fluidElem = doc.NewElement("fluid");
        fluidElem->SetAttribute("density", job->GetFluidDensity());
        fluidElem->SetAttribute("viscosity", job->GetFluidViscosity());
        fluidElem->SetAttribute("initial_pressure", job->GetInitialPressure());
        fluidElem->SetAttribute("initial_velocity", job->GetInitialVelocity());
        root->InsertEndChild(fluidElem);

        auto* wallElem = doc.NewElement("wall");
        wallElem->SetAttribute("deformable", job->GetDeformable() ? "true" : "false");
        wallElem->SetAttribute("thickness", job->GetWallThickness());
        wallElem->SetAttribute("elastic_modulus", job->GetWallElasticModulus());
        wallElem->SetAttribute("poisson_ratio", job->GetWallPoissonRatio());
        wallElem->SetAttribute("density", job->GetWallDensity());
        root->InsertEndChild(wallElem);

        auto* solverElem = doc.NewElement("solver");
        solverElem->SetAttribute("type", job->GetSolverType().c_str());
        solverElem->SetAttribute("linear_iterations", job->GetNumLinearIterations());
        solverElem->SetAttribute("nonlinear_iterations", job->GetNumNonlinearIterations());
        root->InsertEndChild(solverElem);

        const auto& bcs = job->GetBoundaryConditions();
        if (!bcs.empty())
        {
            auto* bcsElem = doc.NewElement("boundary_conditions");
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
                if (!bc.waveform.empty())
                {
                    auto* wfElem = doc.NewElement("waveform");
                    for (const auto& wp : bc.waveform)
                    {
                        auto* wpElem = doc.NewElement("point");
                        wpElem->SetAttribute("t", wp.first);
                        wpElem->SetAttribute("q", wp.second);
                        wfElem->InsertEndChild(wpElem);
                    }
                    bcElem->InsertEndChild(wfElem);
                }
                bcsElem->InsertEndChild(bcElem);
            }
            root->InsertEndChild(bcsElem);
        }

        // Generic property maps
        auto writePropMap = [&](const char* elemName, const XqSimPropertyMap& props)
        {
            if (props.empty()) return;
            auto* elem = doc.NewElement(elemName);
            for (const auto& pair : props)
            {
                auto* propElem = doc.NewElement("prop");
                propElem->SetAttribute("key", pair.first.c_str());
                propElem->SetAttribute("value", pair.second.c_str());
                elem->InsertEndChild(propElem);
            }
            root->InsertEndChild(elem);
        };
        writePropMap("basic_props", job->GetBasicProps());
        writePropMap("wall_props", job->GetWallProps());
        writePropMap("solver_props", job->GetSolverProps());
        writePropMap("run_props", job->GetRunProps());

        // Cap and IC property maps
        auto writeCapPropMap = [&](const char* elemName, const XqSimCapPropertyMap& props)
        {
            if (props.empty()) return;
            auto* elem = doc.NewElement(elemName);
            for (const auto& capPair : props)
            {
                auto* capElem = doc.NewElement("cap");
                capElem->SetAttribute("name", capPair.first.c_str());
                for (const auto& pair : capPair.second)
                {
                    auto* propElem = doc.NewElement("prop");
                    propElem->SetAttribute("key", pair.first.c_str());
                    propElem->SetAttribute("value", pair.second.c_str());
                    capElem->InsertEndChild(propElem);
                }
                elem->InsertEndChild(capElem);
            }
            root->InsertEndChild(elem);
        };
        writeCapPropMap("cap_props", job->GetCapProps());
        writeCapPropMap("ic_props", job->GetIcProps());

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

    // xq_MitkROMJob: serialize to .xqrom.xml
    if (className == "xq_MitkROMJob")
    {
        std::string xmlPath = savePath + ".xqrom.xml";
        auto ioResult = xq_MitkROMJobIO::Write(node, xmlPath);
        if (!ioResult.ok)
        {
            result.diagnostics.push_back(ioResult.diagnostic);
            return result;
        }

        result.ok = true;
        result.savedPath = xmlPath;
        result.relPath = subdir + "/" + xq_WorkspaceManager::GetFileName(xmlPath);
        return result;
    }

    // xq_MitkMultiPhysicsJob: serialize to .xqmp.xml
    if (className == "xq_MitkMultiPhysicsJob")
    {
        std::string xmlPath = savePath + ".xqmp.xml";
        auto ioResult = xq_MitkMultiPhysicsJobIO::Write(node, xmlPath);
        if (!ioResult.ok)
        {
            result.diagnostics.push_back(ioResult.diagnostic);
            return result;
        }

        result.ok = true;
        result.savedPath = xmlPath;
        result.relPath = subdir + "/" + xq_WorkspaceManager::GetFileName(xmlPath);
        return result;
    }

    // xq_MitkSeg3D: serialize native 3D segmentation state to .xqseg3d
    if (className == "xq_MitkSeg3D")
    {
        auto* seg3d = dynamic_cast<const xq_MitkSeg3D*>(node->GetData());
        if (!seg3d)
        {
            result.diagnostics.push_back("dynamic_cast<xq_MitkSeg3D> failed");
            return result;
        }

        std::string xmlPath = savePath + ".xqseg3d";

        tinyxml2::XMLDocument doc;
        auto* decl = doc.NewDeclaration();
        doc.InsertFirstChild(decl);

        auto* root = doc.NewElement("xq_seg3d");
        root->SetAttribute("version", "1");
        root->SetAttribute("method", std::string(seg3d->GetMethodString()).c_str());
        doc.InsertEndChild(root);

        auto* nameElem = doc.NewElement("name");
        nameElem->SetText(node->GetName().c_str());
        root->InsertEndChild(nameElem);

        auto* thresholdsElem = doc.NewElement("thresholds");
        thresholdsElem->SetAttribute("lower", seg3d->GetLowerThreshold());
        thresholdsElem->SetAttribute("upper", seg3d->GetUpperThreshold());
        root->InsertEndChild(thresholdsElem);

        const auto seeds = seg3d->GetSeedPoints();
        if (!seeds.empty())
        {
            auto* seedsElem = doc.NewElement("seed_points");
            seedsElem->SetAttribute("count", static_cast<int>(seeds.size()));
            for (const auto& seed : seeds)
            {
                auto* pointElem = doc.NewElement("point");
                pointElem->SetAttribute("x", seed[0]);
                pointElem->SetAttribute("y", seed[1]);
                pointElem->SetAttribute("z", seed[2]);
                seedsElem->InsertEndChild(pointElem);
            }
            root->InsertEndChild(seedsElem);
        }

        auto polyData = seg3d->GetSurfaceMesh();
        if (polyData && polyData->GetNumberOfPoints() > 0)
        {
            std::string vtpPath = xmlPath + ".vtp";
            vtkNew<vtkXMLPolyDataWriter> writer;
            writer->SetFileName(vtpPath.c_str());
            writer->SetInputData(polyData);
            if (!writer->Write())
            {
                result.diagnostics.push_back("vtkXMLPolyDataWriter::Write() returned false for xq_MitkSeg3D surface");
                return result;
            }

            auto* vtpElem = doc.NewElement("vtp_file");
            vtpElem->SetAttribute("path", xq_WorkspaceManager::GetFileName(vtpPath).c_str());
            root->InsertEndChild(vtpElem);
        }

        if (doc.SaveFile(xmlPath.c_str()) != tinyxml2::XML_SUCCESS)
        {
            result.diagnostics.push_back("Failed to write xqseg3d");
            return result;
        }

        result.ok = true;
        result.savedPath = xmlPath;
        result.relPath = subdir + "/" + xq_WorkspaceManager::GetFileName(xmlPath);
        return result;
    }

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

        auto* nameElem = doc.NewElement("name");
        nameElem->SetText(node->GetName().c_str());
        root->InsertEndChild(nameElem);

        const auto& attrs = group->GetAttributes();
        if (!attrs.empty())
        {
            auto* attrsElem = doc.NewElement("attributes");
            for (const auto& pair : attrs)
            {
                auto* attrElem = doc.NewElement("attribute");
                attrElem->SetAttribute("key", pair.first.c_str());
                attrElem->SetAttribute("value", pair.second.c_str());
                attrsElem->InsertEndChild(attrElem);
            }
            root->InsertEndChild(attrsElem);
        }

        int numProfiles = group->GetProfileCount(0);
        if (numProfiles > 0)
        {
            auto* profilesElem = doc.NewElement("profiles");
            auto indices = group->GetProfilePathIndices(0);
            for (int pathPosIndex : indices)
            {
                auto* profile = group->GetProfileAtPathPos(pathPosIndex, 0);
                if (!profile) continue;

                auto* profElem = doc.NewElement("profile");
                profElem->SetAttribute("path_pos_index", pathPosIndex);
                profElem->SetAttribute("method", profile->GetMethod().c_str());

                auto pts = profile->GetProfilePoints();
                if (!pts.empty())
                {
                    auto* ptsElem = doc.NewElement("points");
                    for (const auto& pt : pts)
                    {
                        auto* ptElem = doc.NewElement("point");
                        ptElem->SetAttribute("x", pt[0]);
                        ptElem->SetAttribute("y", pt[1]);
                        ptElem->SetAttribute("z", pt[2]);
                        ptsElem->InsertEndChild(ptElem);
                    }
                    profElem->InsertEndChild(ptsElem);
                }

                // Save placement frame data (position + slice plane normal/origin)
                {
                    auto* placementElem = doc.NewElement("placement");
                    auto center = profile->GetProfileCenter();
                    placementElem->SetAttribute("cx", center[0]);
                    placementElem->SetAttribute("cy", center[1]);
                    placementElem->SetAttribute("cz", center[2]);

                    auto plane = profile->GetSlicePlane();
                    if (plane.IsNotNull())
                    {
                        auto normal = plane->GetNormal();
                        placementElem->SetAttribute("nx", normal[0]);
                        placementElem->SetAttribute("ny", normal[1]);
                        placementElem->SetAttribute("nz", normal[2]);
                        auto origin = plane->GetOrigin();
                        placementElem->SetAttribute("ox", origin[0]);
                        placementElem->SetAttribute("oy", origin[1]);
                        placementElem->SetAttribute("oz", origin[2]);
                    }
                    profElem->InsertEndChild(placementElem);
                }
                profilesElem->InsertEndChild(profElem);
            }
            root->InsertEndChild(profilesElem);
        }

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

    // Unrecognized type — report but don't silently skip
    result.diagnostics.push_back(
        std::string("Unrecognized data type: ") + className +
        " — no writer registered");
    return result;
}
