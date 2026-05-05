#include "xq_ResultImport.h"

#include <xq_PipelineDataUtils.h>

#include <mitkIOUtil.h>
#include <mitkSurface.h>

#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkDataSetAttributes.h>
#include <vtkSmartPointer.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkGeometryFilter.h>

namespace
{

std::string Basename(const std::string& path)
{
    auto pos = path.rfind('/');
#ifdef _WIN32
    auto pos2 = path.rfind('\\');
    if (pos2 != std::string::npos && (pos == std::string::npos || pos2 > pos))
        pos = pos2;
#endif
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

std::string StripExt(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos != std::string::npos)
        return filename.substr(0, pos);
    return filename;
}

void CollectFieldNames(vtkDataSetAttributes* dsa, const std::string& prefix,
                       std::vector<std::string>& out)
{
    if (!dsa) return;
    for (int i = 0; i < dsa->GetNumberOfArrays(); ++i)
    {
        const char* name = dsa->GetArrayName(i);
        if (name)
            out.push_back(prefix + name);
    }
}

} // namespace

xq_ResultImportOutcome xq_ResultImport::Import(
    mitk::DataStorage* dataStorage,
    const xq_ResultImportEntry& entry)
{
    xq_ResultImportOutcome outcome;

    if (!dataStorage)
    {
        outcome.diagnostics.push_back("DataStorage is null.");
        return outcome;
    }

    if (entry.filePath.empty())
    {
        outcome.diagnostics.push_back("File path is empty.");
        return outcome;
    }

    // Load via MITK IOUtil — produces Surface (vtp).
    // For VTU files MITK may not have a native reader, so we fall back to
    // direct VTK loading and surface extraction.
    std::vector<mitk::BaseData::Pointer> loaded;
    bool loadedAsSurface = false;
    try
    {
        loaded = mitk::IOUtil::Load(entry.filePath);
    }
    catch (const mitk::Exception& e)
    {
        // Non-fatal: we try VTU fallback below.
        outcome.diagnostics.push_back(
            std::string("IOUtil::Load note: ") + e.GetDescription());
    }

    // If MITK did not produce a Surface, try loading as VTU (volume mesh
    // result with scalar fields on an unstructured grid).
    if (!loaded.empty())
    {
        auto* surface = dynamic_cast<mitk::Surface*>(loaded[0].GetPointer());
        if (surface)
            loadedAsSurface = true;
    }

    if (!loadedAsSurface)
    {
        // Direct VTU loading via VTK, then extract surface geometry for
        // rendering while preserving point/cell data arrays.
        auto vtuReader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
        vtuReader->SetFileName(entry.filePath.c_str());
        vtuReader->Update();
        auto ug = vtuReader->GetOutput();
        if (!ug || ug->GetNumberOfPoints() == 0)
        {
            if (loaded.empty())
            {
                outcome.diagnostics.push_back(
                    "Failed to load file as VTP or VTU: " + entry.filePath);
                return outcome;
            }
            // MITK loaded something non-Surface; proceed with it anyway.
        }
        else
        {
            auto geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
            geomFilter->SetInputData(ug);
            geomFilter->Update();
            auto surf = mitk::Surface::New();
            surf->SetVtkPolyData(geomFilter->GetOutput());
            loaded.clear();
            loaded.push_back(surf.GetPointer());
            loadedAsSurface = true;
        }
    }

    if (loaded.empty())
    {
        outcome.diagnostics.push_back("No data loaded from file.");
        return outcome;
    }

    std::string nodeName = entry.nodeName;
    if (nodeName.empty())
        nodeName = StripExt(Basename(entry.filePath));

    auto node = mitk::DataNode::New();
    node->SetData(loaded[0]);
    node->SetName(nodeName);

    // Tag as result
    node->SetStringProperty("xq.type", "result");
    node->SetStringProperty("xq.pipeline.stage", "result");

    // Record source simulation if known
    if (!entry.simulationName.empty())
    {
        xq::pipeline::SetStringProperty(
            node, "xq.source.simulation", entry.simulationName);
    }

    // Discover field names (point + cell)
    vtkDataSet* ds = nullptr;
    auto* surface = dynamic_cast<mitk::Surface*>(loaded[0].GetPointer());
    if (surface)
        ds = surface->GetVtkPolyData();

    std::vector<std::string> fields;
    if (ds)
    {
        CollectFieldNames(ds->GetPointData(), "point:", fields);
        CollectFieldNames(ds->GetCellData(), "cell:", fields);
    }
    outcome.fieldNames = fields;

    // Store field names as CSV on the node for quick lookup
    if (!fields.empty())
    {
        std::string csv;
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) csv += ",";
            csv += fields[i];
        }
        xq::pipeline::SetStringProperty(node, "xq.result.field_names", csv);
    }

    // Add to DataStorage under the Simulation folder (or fallback to root)
    auto simFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::SimulationPrep, nullptr);
    if (simFolder.IsNotNull())
        dataStorage->Add(node, simFolder);
    else
        dataStorage->Add(node);


    outcome.ok = true;
    outcome.node = node;
    return outcome;
}

std::vector<std::string> xq_ResultImport::GetFieldNames(const mitk::DataNode* node)
{
    std::vector<std::string> names;

    if (!node || !node->GetData())
        return names;

    vtkDataSet* ds = nullptr;
    auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
    if (surface)
        ds = surface->GetVtkPolyData();

    // Fallback: try dynamic_cast to vtkDataSet for nodes that store
    // raw VTK data without a MITK Surface wrapper.
    if (!ds)
        ds = dynamic_cast<vtkDataSet*>(node->GetData());

    if (ds)
    {
        CollectFieldNames(ds->GetPointData(), "point:", names);
        CollectFieldNames(ds->GetCellData(), "cell:", names);
    }

    return names;
}

bool xq_ResultImport::SetActiveScalar(mitk::DataNode* node, const std::string& name)
{
    if (!node || !node->GetData() || name.empty())
        return false;

    auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
    if (!surface)
        return false;

    auto* pd = surface->GetVtkPolyData();
    if (!pd || !pd->GetPointData())
        return false;

    // Strip "point:" or "cell:" prefix if present
    std::string fieldName = name;
    if (fieldName.rfind("point:", 0) == 0)
        fieldName = fieldName.substr(6);
    else if (fieldName.rfind("cell:", 0) == 0)
        fieldName = fieldName.substr(5);

    vtkDataArray* arr = pd->GetPointData()->GetArray(fieldName.c_str());
    if (!arr)
        arr = pd->GetCellData()->GetArray(fieldName.c_str());
    if (!arr)
        return false;

    pd->GetPointData()->SetActiveScalars(fieldName.c_str());
    pd->Modified();
    node->Modified();
    return true;
}
