#include "xq_ResultImport.h"

#include <xq_PipelineDataUtils.h>
#include <xq_MitkGrid.h>
#include <xq_TetGenGrid.h>

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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

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

std::string InferFieldType(const std::string& fieldName)
{
    std::string lower;
    lower.reserve(fieldName.size());
    for (char c : fieldName)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (lower.find("pressure") != std::string::npos)
        return "pressure";
    if (lower.find("velocity") != std::string::npos)
        return "velocity";
    if (lower.find("shear") != std::string::npos || lower.find("wss") != std::string::npos)
        return "wall_shear_stress";
    if (lower.find("flow") != std::string::npos)
        return "flow_rate";
    if (lower.find("displacement") != std::string::npos)
        return "displacement";
    return "scalar";
}

int ExtractTrailingNumber(const std::string& path)
{
    std::string name = StripExt(Basename(path));
    int end = static_cast<int>(name.size()) - 1;
    while (end >= 0 && !std::isdigit(static_cast<unsigned char>(name[end])))
        --end;
    if (end < 0)
        return -1;

    int begin = end;
    while (begin >= 0 && std::isdigit(static_cast<unsigned char>(name[begin])))
        --begin;

    return std::atoi(name.substr(static_cast<size_t>(begin + 1),
                                 static_cast<size_t>(end - begin)).c_str());
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

    // Load via MITK IOUtil - produces Surface (vtp).
    // For VTU files, preserve the original unstructured grid in xq_MitkGrid
    // instead of silently reducing it to a visualization-only surface.
    std::vector<mitk::BaseData::Pointer> loaded;
    bool loadedAsSurface = false;
    vtkSmartPointer<vtkUnstructuredGrid> loadedVolumeGrid;
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
        // Direct VTU loading via VTK. Keep the unstructured grid as the
        // primary node data so downstream analysis does not lose volume cells.
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
            loadedVolumeGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
            loadedVolumeGrid->DeepCopy(ug);

            auto geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
            geomFilter->SetInputData(loadedVolumeGrid);
            geomFilter->Update();

            auto grid = std::make_unique<xq_TetGenGrid>();
            grid->SetVolumeMesh(loadedVolumeGrid);
            grid->SetSurfaceMesh(geomFilter->GetOutput());

            auto mitkGrid = xq_MitkGrid::New();
            mitkGrid->SetMesh(grid.release(), 0);

            loaded.clear();
            loaded.push_back(mitkGrid.GetPointer());
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

    // Tag as a generated result node with explicit provenance metadata.
    xq::pipeline::MarkGeneratedNode(
        node,
        xq::pipeline::Stage::Result,
        "result_import",
        "org.xq.imaging.flowanalysis");
    node->SetStringProperty("xq.type", "result");
    xq::pipeline::SetStringProperty(node, "xq.result.backend_id", "result_import");
    xq::pipeline::SetStringProperty(node, "xq.result.importer_version", "1");
    node->SetStringProperty(
        "xq.result.dataset_type",
        loadedVolumeGrid ? "unstructured_grid" : (loadedAsSurface ? "surface" : "unknown"));
    node->SetBoolProperty("xq.result.volume_grid_preserved", loadedVolumeGrid != nullptr);
    node->SetBoolProperty("xq.result.visual_surface_derived", loadedVolumeGrid != nullptr);
    if (loadedVolumeGrid)
    {
        node->SetIntProperty(
            "xq.result.volume_points",
            static_cast<int>(loadedVolumeGrid->GetNumberOfPoints()));
        node->SetIntProperty(
            "xq.result.volume_cells",
            static_cast<int>(loadedVolumeGrid->GetNumberOfCells()));
    }

    // Record source simulation if known
    if (!entry.simulationName.empty())
    {
        xq::pipeline::SetStringProperty(
            node, "xq.source.simulation", entry.simulationName);
        xq::pipeline::SetStringProperty(
            node, xq::pipeline::kSourceSimulationJobProperty, entry.simulationName);
        xq::pipeline::SetStringProperty(
            node, "xq.result.source_simulation", entry.simulationName);
        xq::pipeline::SetStringProperty(
            node, "xq.result.source_simulation_job", entry.simulationName);
    }
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceSolverCaseProperty, entry.filePath);
    xq::pipeline::SetStringProperty(node, "xq.result.file_path", entry.filePath);

    // Discover field names (point + cell)
    vtkDataSet* ds = nullptr;
    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(loaded[0].GetPointer());
    if (mitkGrid)
    {
        auto* grid = mitkGrid->GetMesh(0);
        ds = grid ? grid->GetVolumeMesh() : nullptr;
    }
    auto* surface = dynamic_cast<mitk::Surface*>(loaded[0].GetPointer());
    if (!ds && surface)
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
        xq::pipeline::SetStringProperty(
            node, "xq.result.field_name", fields.front());
        xq::pipeline::SetStringProperty(
            node, "xq.result.field_type", InferFieldType(fields.front()));
        node->SetIntProperty(
            "xq.result.field_count", static_cast<int>(fields.size()));
    }
    else
    {
        node->SetIntProperty("xq.result.field_count", 0);
        xq::pipeline::SetStringProperty(
            node, "xq.pipeline.limitations",
            "No named point or cell result arrays were discovered in the imported file.");
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
    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(node->GetData());
    if (mitkGrid)
    {
        auto* grid = mitkGrid->GetMesh(0);
        ds = grid ? grid->GetVolumeMesh() : nullptr;
    }

    auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
    if (!ds && surface)
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

    vtkDataSet* ds = nullptr;
    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(node->GetData());
    if (mitkGrid)
    {
        auto* grid = mitkGrid->GetMesh(0);
        ds = grid ? grid->GetVolumeMesh() : nullptr;
    }

    auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
    if (!ds && surface)
        ds = surface->GetVtkPolyData();
    if (!ds || !ds->GetPointData())
        return false;

    // Strip "point:" or "cell:" prefix if present
    std::string fieldName = name;
    if (fieldName.rfind("point:", 0) == 0)
        fieldName = fieldName.substr(6);
    else if (fieldName.rfind("cell:", 0) == 0)
        fieldName = fieldName.substr(5);

    vtkDataArray* arr = ds->GetPointData()->GetArray(fieldName.c_str());
    if (arr)
    {
        ds->GetPointData()->SetActiveScalars(fieldName.c_str());
    }
    else
    {
        arr = ds->GetCellData()->GetArray(fieldName.c_str());
        if (arr)
            ds->GetCellData()->SetActiveScalars(fieldName.c_str());
    }
    if (!arr)
        return false;

    xq::pipeline::SetStringProperty(node, "xq.result.active_scalar", fieldName);
    ds->Modified();
    node->Modified();
    return true;
}

std::vector<std::string> xq_ResultImport::SortTimeStepFiles(
    const std::vector<std::string>& filePaths)
{
    std::vector<std::pair<std::string, int>> keyed;
    keyed.reserve(filePaths.size());
    for (const auto& path : filePaths)
        keyed.emplace_back(path, ExtractTrailingNumber(path));

    std::stable_sort(keyed.begin(), keyed.end(),
        [](const auto& a, const auto& b) {
            if (a.second >= 0 && b.second >= 0 && a.second != b.second)
                return a.second < b.second;
            if (a.second >= 0 && b.second < 0)
                return true;
            if (a.second < 0 && b.second >= 0)
                return false;
            return a.first < b.first;
        });

    std::vector<std::string> sorted;
    sorted.reserve(keyed.size());
    for (const auto& item : keyed)
        sorted.push_back(item.first);
    return sorted;
}

std::vector<xq_ResultImportOutcome> xq_ResultImport::ImportTimeSeries(
    mitk::DataStorage* dataStorage,
    const std::vector<std::string>& filePaths,
    const std::string& simulationName)
{
    auto sorted = SortTimeStepFiles(filePaths);
    std::vector<xq_ResultImportOutcome> outcomes;
    outcomes.reserve(sorted.size());

    for (size_t i = 0; i < sorted.size(); ++i)
    {
        xq_ResultImportEntry entry;
        entry.filePath = sorted[i];
        entry.simulationName = simulationName;
        entry.nodeName = StripExt(Basename(sorted[i]));

        auto outcome = Import(dataStorage, entry);
        if (outcome.node.IsNotNull())
        {
            outcome.node->SetIntProperty(
                "xq.result.time_step", static_cast<int>(i));
            outcome.node->SetIntProperty(
                "xq.result.time_step_index", static_cast<int>(i));
            outcome.node->SetIntProperty(
                "xq.result.time_step_count", static_cast<int>(sorted.size()));
            outcome.node->SetDoubleProperty(
                "xq.result.time_value", static_cast<double>(i));
        }
        outcomes.push_back(outcome);
    }

    return outcomes;
}
