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
        "xq.status",
        "xq.dirty_reason",
        "xq.source.image",
        "xq.source.path",
        "xq.source.contour_groups",
        "xq.source.model",
        "xq.source.mesh",
        "xq.source.simulation",
        "xq.sim.status",
        "xq.sim.export_dir",
        "xq.sim.files_written",
        "xq.type",
        "xq.result.field_names",
        "xq.model.type",
        "xq.model.sampling",
        "xq.mesh.type",
        "xq.mesh.capability.diagnostic"
    };

    static constexpr const char* kBoolKeys[] = {
        "xq.model.qa.ok",
        "xq.model.qa.has_face_ids",
        "xq.mesh.qa.ok",
        "xq.mesh.optimize",
        "xq.mesh.local_face_sizes.applied",
        "xq.mesh.refinement_regions.applied",
        "xq.contour.ready",
        "visible"
    };

    static constexpr const char* kIntKeys[] = {
        "xq.model.qa.boundary_edges",
        "xq.model.qa.non_manifold_edges",
        "xq.model.qa.degenerate_cells",
        "xq.model.qa.connected_components",
        "xq.model.face_count",
        "xq.mesh.cells",
        "xq.mesh.points",
        "xq.mesh.negative_volume_count",
        "xq.mesh.bl.layers",
        "xq.mesh.local_face_sizes",
        "xq.mesh.refinement_regions",
        "xq.contour.profile_count",
        "xq.contour.missing_count",
        "xq.contour.warning_count",
        "xq.contour.error_count",
        "xq.sim.num_timesteps",
        "xq.sim.bc_count",
        "xq.sim.export_file_count",
        "xq.sim.files_written_count"
    };

    static constexpr const char* kDoubleKeys[] = {
        "xq.mesh.min_volume",
        "xq.mesh.max_volume",
        "xq.mesh.globalEdgeSize",
        "xq.mesh.bl.firstHeight",
        "xq.mesh.bl.growthRate",
        "xq.sim.fluid_density",
        "xq.sim.fluid_viscosity",
        "xq.sim.time_step_size"
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
