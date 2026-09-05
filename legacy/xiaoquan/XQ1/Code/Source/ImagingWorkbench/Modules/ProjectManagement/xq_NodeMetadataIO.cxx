#include "xq_NodeMetadataIO.h"

#include <mitkProperties.h>
#include <tinyxml2.h>

#include <sstream>

// Typed property whitelists for round-trip pipeline metadata.
namespace {
    static constexpr const char* kStringKeys[] = {
        "xq.pipeline.stage",
        "xq.pipeline.version",
        "xq.pipeline.algorithm",
        "xq.pipeline.algorithm_version",
        "xq.pipeline.created_by_tool",
        "xq.pipeline.created_at",
        "xq.pipeline.diagnostic",
        "xq.pipeline.limitations",
        "xq.status",
        "xq.dirty_reason",
        "xq.source.image",
        "xq.source.preprocessed_image",
        "xq.source.centerline",
        "xq.source.path",
        "xq.source.paths",
        "xq.source.contour_group",
        "xq.source.contour_groups",
        "xq.source.model",
        "xq.source.mesh",
        "xq.source.simulation",
        "xq.source.simulation_job",
        "xq.source.solver_case",
        "xq.source.segmentation",
        "xq.params.image_processing.operation",
        "xq.params.image_processing.parameters",
        "xq.params.image_processing.seed",
        "xq.params.image_processing.crop",
        "xq.params.image_processing.spacing",
        "xq.params.simulation.face_roles",
        "xq.params.simulation.boundary_conditions",
        "xq.params.path.method",
        "xq.params.path.smoothing",
        "xq.params.segmentation2d.method",
        "xq.params.segmentation2d.level_set.method",
        "xq.params.segmentation3d.method",
        "xq.params.segmentation3d.toolmanager.mode",
        "xq.params.modeling.model_type",
        "xq.params.modeling.loft_method",
        "xq.params.modeling.smoothing",
        "xq.params.modeling.qa.summary",
        "xq.params.meshing.backend",
        "xq.params.meshing.command_history",
        "xq.params.meshing.local_size.values",
        "xq.params.meshing.boundary_layer.mode",
        "xq.params.meshing.quality.summary",
        "xq.params.solver.backend_id",
        "xq.params.solver.backend_name",
        "xq.params.solver.backend_version",
        "xq.params.solver.case_format_version",
        "xq.params.solver.case_dir",
        "xq.params.solver.status",
        "xq.params.solver.started_at",
        "xq.params.solver.finished_at",
        "xq.params.solver.diagnostic",
        "xq.params.solver.command_line",
        "xq.params.solver.result_import_status",
        "xq.params.results.field_name",
        "xq.params.results.field_type",
        "xq.params.results.units",
        "xq.params.results.backend_id",
        "xq.params.results.importer_version",
        "xq.params.simulation.wall_model",
        "xq.params.simulation.export_format",
        "xq.units.length",
        "xq.units.time",
        "xq.units.mass",
        "xq.units.pressure",
        "xq.units.velocity",
        "xq.units.flow",
        "xq.units.viscosity",
        "xq.units.density",
        "xq.image.processing.operation",
        "xq.image.processing.source_node",
        "xq.image.processing.parameters",
        "xq.image.processing.seed_points",
        "xq.path.method",
        "xq.pathplanning.algorithm.requested",
        "xq.pathplanning.algorithm.actual",
        "xq.pathplanning.algorithm.diagnostic",
        "xq.centerline.capability.diagnostic",
        "xq.sim.status",
        "xq.sim.export_dir",
        "xq.sim.run_dir",
        "xq.sim.solver_path",
        "xq.sim.solver_type",
        "xq.sim.solver_preset",
        "xq.sim.step_construction",
        "xq.sim.pressure_coupling",
        "xq.sim.bc_table",
        "xq.sim.files_written",
        "xq.sim.export_warnings",
        "xq.sim.last_error",
        "xq.rom.model_order",
        "xq.rom.status",
        "xq.rom.bc",
        "xq.rom.solver_params",
        "xq.rom.run_params",
        "xq.rom.result_conversion",
        "xq.rom.capability.diagnostic",
        "xq.multiphysics.domains",
        "xq.multiphysics.equations",
        "xq.multiphysics.bc",
        "xq.multiphysics.params",
        "xq.multiphysics.solver_paths",
        "xq.multiphysics.status",
        "xq.multiphysics.capability.diagnostic",
        "xq.type",
        "xq.result.field_names",
        "xq.result.field_name",
        "xq.result.field_type",
        "xq.result.active_scalar",
        "xq.result.dataset_type",
        "xq.result.file_path",
        "xq.result.source_simulation",
        "xq.result.source_simulation_job",
        "xq.result.backend_id",
        "xq.result.backend_name",
        "xq.result.backend_version",
        "xq.result.importer_version",
        "xq.result.units.pressure",
        "xq.result.units.velocity",
        "xq.result.units.wall_shear",
        "xq.result.units",
        "xq.solver.backend_id",
        "xq.solver.backend_name",
        "xq.solver.backend_version",
        "xq.solver.case_format_version",
        "xq.solver.case_dir",
        "xq.solver.command_line",
        "xq.solver.status",
        "xq.solver.started_at",
        "xq.solver.finished_at",
        "xq.solver.diagnostic",
        "xq.solver.result_import_status",
        "xq.model.type",
        "xq.model.sampling",
        "xq.model.operation",
        "xq.model.cap_info",
        "xq.model.loft.parameters",
        "xq.model.trim.path",
        "xq.model.requested_engine",
        "xq.model.actual_engine",
        "xq.model.fillet.type",
        "xq.mesh.type",
        "xq.mesh.requested_backend",
        "xq.mesh.actual_backend",
        "xq.mesh.command_history",
        "xq.mesh.capability.diagnostic",
        "xq.mesh.local_face_sizes.values",
        "xq.mesh.refinement_regions.values",
        "xq.mesh.adapt.source_mesh",
        "xq.mesh.adapt.source_result",
        "xq.mesh.adapt.diagnostic",
        "xq.segmentation.activetype",
        "xq.segmentation.contourtype",
        "xq.segmentation.current_path_name",
        "xq.segmentation.method",
        "xq.segmentation.ml.model",
        "xq.segmentation.seed_points",
        "xq.segmentation.toolmanager.mode",
        "xq.segmentation.utility"
    };

    static constexpr const char* kBoolKeys[] = {
        "xq.pipeline.valid",
        "xq.model.qa.ok",
        "xq.model.qa.has_face_ids",
        "xq.model.algorithm.fallback",
        "xq.mesh.qa.ok",
        "xq.mesh.optimize",
        "xq.mesh.preserve_surface",
        "xq.mesh.backend.fallback",
        "xq.mesh.adaptPending",
        "xq.mesh.bl.directionInward",
        "xq.mesh.local_face_sizes.applied",
        "xq.mesh.refinement_regions.applied",
        "xq.contour.ready",
        "xq.image.processing.output_is_image",
        "xq.image.processing.output_is_surface",
        "xq.loft.linearSample",
        "xq.loft.useFFT",
        "xq.path.editable",
        "xq.pathplanning.algorithm.fallback",
        "xq.pathplanning.path",
        "xq.python.metadata_only",
        "xq.params.modeling.qa.ok",
        "xq.params.meshing.quality.ok",
        "xq.params.solver.supports_transient",
        "xq.params.solver.supports_steady",
        "xq.params.solver.supports_result_import",
        "xq.segmentation.3d",
        "xq.segmentation.contour",
        "xq.segmentation.profile_group",
        "xq.sim.legendVisible",
        "xq.sim.deformable_wall",
        "xq.sim.stabilization",
        "xq.result.volume_grid_preserved",
        "xq.result.visual_surface_derived",
        "binary",
        "scalar visibility",
        "show contour",
        "visible"
    };

    static constexpr const char* kIntKeys[] = {
        "xq.model.qa.boundary_edges",
        "xq.model.qa.non_manifold_edges",
        "xq.model.qa.degenerate_cells",
        "xq.model.qa.connected_components",
        "xq.model.face_count",
        "xq.model.current_face_index",
        "xq.model.sampling",
        "xq.model.fillet.iterations",
        "xq.path.calculation_number",
        "xq.path.current_point_index",
        "xq.path.point_count",
        "xq.path.smoothing_modes",
        "xq.path.smoothing_output_count",
        "xq.path.subdivision_number",
        "xq.mesh.cells",
        "xq.mesh.points",
        "xq.mesh.negative_volume_count",
        "xq.mesh.bl.layers",
        "xq.mesh.bl.numLayers",
        "xq.mesh.local_face_sizes",
        "xq.mesh.refinement_regions",
        "xq.contour.profile_count",
        "xq.contour.missing_count",
        "xq.contour.warning_count",
        "xq.contour.error_count",
        "xq.loft.numOutputPoints",
        "xq.loft.numSections",
        "xq.loft.samplePerSection",
        "xq.loft.splineDegree",
        "xq.result.time_step_count",
        "xq.result.time_step",
        "xq.result.time_step_index",
        "xq.result.volume_points",
        "xq.result.volume_cells",
        "xq.result.field_count",
        "xq.solver.exit_code",
        "xq.solver.num_processors",
        "xq.solver.result_file_count",
        "xq.segmentation.current_contour_index",
        "xq.segmentation.current_reslice_index",
        "xq.segmentation.contourindex",
        "xq.segmentation.active_tool_id",
        "xq.segmentation.levelset.iterations",
        "xq.segmentation.interval",
        "xq.segmentation.morph.radius",
        "xq.sim.num_timesteps",
        "xq.sim.num_cycles",
        "xq.params.simulation.num_steps",
        "xq.params.simulation.num_cycles",
        "xq.params.simulation.boundary_condition_count",
        "xq.params.simulation.face_role_count",
        "xq.params.modeling.face_count",
        "xq.params.modeling.cap_count",
        "xq.params.meshing.boundary_layer.layers",
        "xq.params.meshing.quality.cell_count",
        "xq.params.meshing.quality.point_count",
        "xq.params.solver.exit_code",
        "xq.params.solver.num_processors",
        "xq.params.solver.result_file_count",
        "xq.params.results.time_step",
        "xq.sim.face_role_count",
        "xq.sim.num_linear_iterations",
        "xq.sim.num_nonlinear_iterations",
        "xq.sim.num_processors",
        "xq.sim.bc_count",
        "xq.sim.current_bc_index",
        "xq.simprep.face_role_count",
        "xq.sim.exit_code",
        "xq.sim.export_file_count",
        "xq.sim.files_written_count",
        "xq.multiphysics.process_count",
        "selectedFaceId"
    };

    static constexpr const char* kDoubleKeys[] = {
        "xq.mesh.min_volume",
        "xq.mesh.max_volume",
        "xq.mesh.globalEdgeSize",
        "xq.mesh.min_dihedral",
        "xq.mesh.max_edge_size",
        "xq.mesh.bl.firstHeight",
        "xq.mesh.bl.growthRate",
        "xq.model.fillet.radius",
        "xq.path.point_size",
        "xq.path.reslice_size",
        "xq.path.spacing",
        "xq.image.processing.threshold.max",
        "xq.image.processing.threshold.min",
        "xq.params.image_processing.threshold.max",
        "xq.params.image_processing.threshold.min",
        "xq.params.image_processing.threshold.inside",
        "xq.params.image_processing.threshold.outside",
        "xq.params.image_processing.isovalue",
        "xq.params.path.spacing",
        "xq.params.path.point_size_2d",
        "xq.params.path.point_size_3d",
        "xq.params.segmentation2d.slice_spacing",
        "xq.params.segmentation2d.pixel_spacing",
        "xq.params.segmentation2d.threshold",
        "xq.params.segmentation2d.level_set.propagation",
        "xq.params.segmentation2d.level_set.curvature",
        "xq.params.segmentation2d.point_size_2d",
        "xq.params.segmentation2d.point_size_3d",
        "xq.params.modeling.qa.min_area",
        "xq.params.modeling.qa.max_area",
        "xq.params.meshing.global_size",
        "xq.params.meshing.local_size.default",
        "xq.params.meshing.boundary_layer.first_height",
        "xq.params.meshing.boundary_layer.growth_rate",
        "xq.params.meshing.quality.min_volume",
        "xq.params.meshing.quality.min_dihedral",
        "xq.params.simulation.fluid_density",
        "xq.params.simulation.fluid_viscosity",
        "xq.params.simulation.time_step",
        "xq.params.results.time_value",
        "xq.segmentation.reslice_size",
        "xq.segmentation.region_growing.tolerance",
        "xq.segmentation.threshold.current",
        "xq.segmentation.threshold.max",
        "xq.segmentation.threshold.min",
        "xq.segmentation.levelset.propagation",
        "xq.segmentation.levelset.curvature",
        "xq.sim.fluid_density",
        "xq.sim.fluid_viscosity",
        "xq.sim.initial_pressure",
        "xq.sim.initial_velocity",
        "xq.sim.wall_thickness",
        "xq.sim.wall_elastic_modulus",
        "xq.sim.wall_poisson_ratio",
        "xq.sim.wall_density",
        "xq.sim.start_time",
        "xq.sim.end_time",
        "xq.sim.residual_tolerance",
        "xq.sim.time_step_size",
        "xq.result.time_value"
    };

    static constexpr const char* kFloatKeys[] = {
        "opacity",
        "point size"
    };
}

bool xq_NodeMetadataIO::WriteNodeMetadata(
    const mitk::DataNode* node,
    const std::string& metaPath)
{
    if (!node)
        return false;

    tinyxml2::XMLDocument doc;

    auto* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    auto* root = doc.NewElement("xq_node_metadata");
    root->SetAttribute("version", "1");
    doc.InsertEndChild(root);

    // Node name
    auto* nameElem = doc.NewElement("name");
    nameElem->SetText(node->GetName().c_str());
    root->InsertEndChild(nameElem);

    // Data class name
    if (node->GetData())
    {
        auto* classElem = doc.NewElement("class");
        classElem->SetText(node->GetData()->GetNameOfClass());
        root->InsertEndChild(classElem);
    }

    // Properties
    auto* propsElem = doc.NewElement("properties");
    root->InsertEndChild(propsElem);

    // ---- String properties ----
    for (const auto& key : kStringKeys)
    {
        std::string value;
        if (node->GetStringProperty(key, value) && !value.empty())
        {
            auto* prop = doc.NewElement("property");
            prop->SetAttribute("key", key);
            prop->SetAttribute("type", "string");
            prop->SetAttribute("value", value.c_str());
            propsElem->InsertEndChild(prop);
        }
    }

    // ---- Bool properties ----
    for (const auto& key : kBoolKeys)
    {
        bool value = false;
        if (node->GetBoolProperty(key, value))
        {
            auto* prop = doc.NewElement("property");
            prop->SetAttribute("key", key);
            prop->SetAttribute("type", "bool");
            prop->SetAttribute("value", value ? "true" : "false");
            propsElem->InsertEndChild(prop);
        }
    }

    // ---- Int properties ----
    for (const auto& key : kIntKeys)
    {
        int value = 0;
        if (node->GetIntProperty(key, value))
        {
            auto* prop = doc.NewElement("property");
            prop->SetAttribute("key", key);
            prop->SetAttribute("type", "int");
            prop->SetAttribute("value", std::to_string(value).c_str());
            propsElem->InsertEndChild(prop);
        }
    }

    // ---- Double properties ----
    for (const auto& key : kDoubleKeys)
    {
        double value = 0.0;
        if (node->GetDoubleProperty(key, value))
        {
            auto* prop = doc.NewElement("property");
            prop->SetAttribute("key", key);
            prop->SetAttribute("type", "double");
            prop->SetAttribute("value", std::to_string(value).c_str());
            propsElem->InsertEndChild(prop);
        }
    }

    // ---- Float properties ----
    for (const auto& key : kFloatKeys)
    {
        float value = 0.0f;
        if (node->GetFloatProperty(key, value))
        {
            auto* prop = doc.NewElement("property");
            prop->SetAttribute("key", key);
            prop->SetAttribute("type", "float");
            prop->SetAttribute("value", std::to_string(value).c_str());
            propsElem->InsertEndChild(prop);
        }
    }

    // ---- Color property ----
    float color[3] = {0.0f, 0.0f, 0.0f};
    if (node->GetColor(color))
    {
        auto* prop = doc.NewElement("property");
        prop->SetAttribute("key", "color");
        prop->SetAttribute("type", "color");
        std::ostringstream oss;
        oss << color[0] << "," << color[1] << "," << color[2];
        prop->SetAttribute("value", oss.str().c_str());
        propsElem->InsertEndChild(prop);
    }

    return doc.SaveFile(metaPath.c_str()) == tinyxml2::XML_SUCCESS;
}

bool xq_NodeMetadataIO::ReadNodeMetadata(
    mitk::DataNode* node,
    const std::string& metaPath)
{
    if (!node)
        return false;

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(metaPath.c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    auto* root = doc.FirstChildElement("xq_node_metadata");
    if (!root)
        return false;

    // Restore name only if the node doesn't already have one
    if (node->GetName().empty())
    {
        auto* nameElem = root->FirstChildElement("name");
        if (nameElem && nameElem->GetText())
            node->SetName(nameElem->GetText());
    }

    auto* propsElem = root->FirstChildElement("properties");
    if (!propsElem)
        return true; // No properties is OK — name restored, metadata may be empty

    for (auto* prop = propsElem->FirstChildElement("property");
         prop;
         prop = prop->NextSiblingElement("property"))
    {
        const char* key = prop->Attribute("key");
        const char* value = prop->Attribute("value");
        if (!key || !value)
            continue;

        // Backward compatibility: missing type attribute defaults to "string"
        const char* type = prop->Attribute("type");
        std::string typeStr = type ? std::string(type) : std::string("string");

        if (typeStr == "string")
        {
            node->SetStringProperty(key, value);
        }
        else if (typeStr == "bool")
        {
            bool b = (std::string(value) == "true");
            node->SetBoolProperty(key, b);
        }
        else if (typeStr == "int")
        {
            try
            {
                int i = std::stoi(value);
                node->SetIntProperty(key, i);
            }
            catch (const std::exception&)
            {
                // Skip invalid int values — don't crash on malformed files
            }
        }
        else if (typeStr == "double")
        {
            try
            {
                double d = std::stod(value);
                node->SetDoubleProperty(key, d);
            }
            catch (const std::exception&)
            {
                // Skip invalid double values — don't crash on malformed files
            }
        }
        else if (typeStr == "float")
        {
            try
            {
                float f = std::stof(value);
                node->SetFloatProperty(key, f);
            }
            catch (const std::exception&)
            {
                // Skip invalid float values — don't crash on malformed files
            }
        }
        else if (typeStr == "color")
        {
            std::istringstream iss(value);
            float r = 0.0f, g = 0.0f, b = 0.0f;
            char sep1, sep2;
            if (iss >> r >> sep1 >> g >> sep2 >> b && sep1 == ',' && sep2 == ',')
            {
                node->SetColor(r, g, b);
            }
        }
    }

    return true;
}
