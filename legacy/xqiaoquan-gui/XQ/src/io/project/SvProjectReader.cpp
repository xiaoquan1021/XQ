#include "io/project/SvProjectReader.h"

#include <tinyxml2.h>

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQPathPayload.h"
#include "core/XQSourcePayload.h"
#include "io/project/PTHPathReader.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace xq {
namespace {

struct PendingNode {
    XQDomainType domain;
    std::string display_name;
    std::string source_path; // path relative to the project dir
};

struct InsertedNode {
    NodeId id;
    XQDomainType domain;
    std::string display_name;
};

void add_diagnostic(std::vector<Diagnostic>* diagnostics,
                    DiagnosticSeverity severity,
                    const std::string& code,
                    const std::string& message)
{
    if (diagnostics != 0) {
        diagnostics->push_back(Diagnostic(severity, code, message));
    }
}

bool non_empty_attribute(const tinyxml2::XMLElement* element,
                         const char* name,
                         std::string* out)
{
    if (element == 0 || name == 0 || out == 0) {
        return false;
    }

    const char* value = element->Attribute(name);
    if (value == 0 || value[0] == '\0') {
        return false;
    }

    *out = value;
    return true;
}

bool folder_name(const tinyxml2::XMLElement* root,
                 const char* element_name,
                 std::string* out)
{
    if (root == 0 || element_name == 0 || out == 0) {
        return false;
    }

    return non_empty_attribute(root->FirstChildElement(element_name), "folder_name", out);
}

bool has_extension(const std::filesystem::path& path, const char* extension)
{
    return path.extension().string() == extension;
}

std::string stem_string(const std::filesystem::path& path)
{
    return path.stem().string();
}

bool read_image_nodes(const tinyxml2::XMLElement* root,
                      const std::string& images_folder,
                      std::vector<PendingNode>* nodes)
{
    if (root == 0 || nodes == 0) {
        return false;
    }

    const tinyxml2::XMLElement* images_element = root->FirstChildElement("images");
    if (images_element == 0) {
        return false;
    }

    std::string ignored_folder;
    if (!non_empty_attribute(images_element, "folder_name", &ignored_folder)) {
        return false;
    }

    for (const tinyxml2::XMLElement* image = images_element->FirstChildElement("image");
         image != 0;
         image = image->NextSiblingElement("image")) {
        std::string image_name;
        std::string image_path;
        if (!non_empty_attribute(image, "name", &image_name)
            || !non_empty_attribute(image, "path", &image_path)) {
            return false;
        }

        PendingNode node;
        node.domain = XQDomainType::Image;
        node.display_name = image_name;
        node.source_path = images_folder + "/" + image_path;
        nodes->push_back(node);
    }

    return true;
}

bool read_folder_nodes(const std::filesystem::path& project_dir,
                       const std::string& folder,
                       const char* extension,
                       XQDomainType domain,
                       std::vector<PendingNode>* nodes)
{
    if (extension == 0 || nodes == 0) {
        return false;
    }

    const std::filesystem::path folder_path = project_dir / folder;
    std::error_code error;
    if (!std::filesystem::is_directory(folder_path, error) || error) {
        return false;
    }

    std::vector<std::filesystem::path> files;
    std::filesystem::directory_iterator it(folder_path, error);
    if (error) {
        return false;
    }
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(error)) {
        if (error) {
            return false;
        }
        const bool is_regular = it->is_regular_file(error);
        if (error) {
            return false;
        }
        if (!is_regular) {
            continue;
        }
        if (has_extension(it->path(), extension)) {
            files.push_back(it->path());
        }
    }

    std::sort(files.begin(), files.end());
    for (std::vector<std::filesystem::path>::const_iterator file = files.begin();
         file != files.end();
         ++file) {
        PendingNode node;
        node.domain = domain;
        node.display_name = stem_string(*file);
        node.source_path = folder + "/" + file->filename().string();
        nodes->push_back(node);
    }

    return true;
}

bool has_suffix(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string path_name_for_contour(const std::string& contour_name)
{
    const std::string suffix = "_final";
    if (has_suffix(contour_name, suffix)) {
        return contour_name.substr(0, contour_name.size() - suffix.size());
    }

    return contour_name;
}

// Builds the payload for a pending node. Path nodes carry their real geometry
// (an XQPathPayload filled by PTHPathReader from the .pth file); every other
// domain carries an XQSourcePayload binding its relative source path. Returns
// nullptr if a path file fails to read, which fails the whole load.
std::shared_ptr<XQPayload> make_payload(const std::filesystem::path& project_dir,
                                        const NodeId& node_id,
                                        const PendingNode& pending)
{
    if (pending.domain != XQDomainType::Path) {
        return std::make_shared<XQSourcePayload>(pending.domain, pending.source_path);
    }

    const std::filesystem::path pth_path = project_dir / pending.source_path;
    PTHReadResult read_result;
    if (PTHPathReader::read(pth_path.string(), &read_result) != PTHPathReader::Status::Ok) {
        return std::shared_ptr<XQPayload>();
    }

    XQPath path = read_result.path;
    path.setId(node_id);
    return std::make_shared<XQPathPayload>(path);
}

bool insert_nodes(const std::filesystem::path& project_dir,
                  const std::vector<PendingNode>& pending_nodes,
                  XQScene* scene,
                  std::vector<InsertedNode>* inserted_nodes)
{
    if (scene == 0 || inserted_nodes == 0) {
        return false;
    }

    inserted_nodes->clear();
    inserted_nodes->reserve(pending_nodes.size());
    NodeId::ValueType next_id = 1;
    for (std::vector<PendingNode>::const_iterator it = pending_nodes.begin();
         it != pending_nodes.end();
         ++it) {
        const NodeId node_id(next_id);
        std::shared_ptr<XQPayload> payload = make_payload(project_dir, node_id, *it);
        if (payload == nullptr) {
            return false;
        }
        if (scene->insert(XQDataNode(node_id, it->domain, it->display_name, payload))
            != XQScene::InsertResult::Inserted) {
            return false;
        }

        InsertedNode inserted;
        inserted.id = node_id;
        inserted.domain = it->domain;
        inserted.display_name = it->display_name;
        inserted_nodes->push_back(inserted);
        ++next_id;
    }

    return true;
}

std::vector<NodeId> ids_for_domain(const std::vector<InsertedNode>& nodes, XQDomainType domain)
{
    std::vector<NodeId> ids;
    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain == domain) {
            ids.push_back(it->id);
        }
    }
    return ids;
}

std::map<std::string, NodeId> ids_by_name_for_domain(const std::vector<InsertedNode>& nodes,
                                                     XQDomainType domain)
{
    std::map<std::string, NodeId> by_name;
    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain == domain) {
            by_name[it->display_name] = it->id;
        }
    }
    return by_name;
}

// path --source--> contour group, matched by the path name embedded in the
// contour group's "<path>_final" display name.
bool link_path_contours(const std::vector<InsertedNode>& nodes, XQScene* scene)
{
    if (scene == 0) {
        return false;
    }

    const std::map<std::string, NodeId> path_by_name =
        ids_by_name_for_domain(nodes, XQDomainType::Path);

    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain != XQDomainType::ContourGroup) {
            continue;
        }

        const std::string path_name = path_name_for_contour(it->display_name);
        const std::map<std::string, NodeId>::const_iterator source = path_by_name.find(path_name);
        if (source == path_by_name.end()) {
            continue;
        }
        if (scene->link_derived(source->second, it->id) != XQScene::RelationResult::Linked) {
            return false;
        }
    }

    return true;
}

// contour group --derived--> surface model. The .svproj does not record which
// contour groups feed which model, so every contour group is linked as a source
// of every surface model (single model for the 0007 fixture).
bool link_contours_models(const std::vector<InsertedNode>& nodes, XQScene* scene)
{
    if (scene == 0) {
        return false;
    }

    const std::vector<NodeId> contour_ids = ids_for_domain(nodes, XQDomainType::ContourGroup);
    const std::vector<NodeId> model_ids = ids_for_domain(nodes, XQDomainType::SurfaceModel);

    for (std::vector<NodeId>::const_iterator model = model_ids.begin();
         model != model_ids.end();
         ++model) {
        for (std::vector<NodeId>::const_iterator contour = contour_ids.begin();
             contour != contour_ids.end();
             ++contour) {
            if (scene->link_derived(*contour, *model) != XQScene::RelationResult::Linked) {
                return false;
            }
        }
    }

    return true;
}

// Links nodes of two domains by matching display name (model -> mesh, mesh ->
// simulation case all share the case stem for the 0007 fixture).
bool link_by_name(const std::vector<InsertedNode>& nodes,
                  XQDomainType source_domain,
                  XQDomainType derived_domain,
                  XQScene* scene)
{
    if (scene == 0) {
        return false;
    }

    const std::map<std::string, NodeId> source_by_name =
        ids_by_name_for_domain(nodes, source_domain);

    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain != derived_domain) {
            continue;
        }
        const std::map<std::string, NodeId>::const_iterator source =
            source_by_name.find(it->display_name);
        if (source == source_by_name.end()) {
            continue;
        }
        if (scene->link_derived(source->second, it->id) != XQScene::RelationResult::Linked) {
            return false;
        }
    }

    return true;
}

bool build_project(const std::filesystem::path& project_dir,
                   const tinyxml2::XMLDocument& document,
                   XQProject* project)
{
    if (project == 0) {
        return false;
    }

    const tinyxml2::XMLElement* root = document.FirstChildElement("projectDescription");
    if (root == 0) {
        return false;
    }

    std::string images_folder;
    std::string paths_folder;
    std::string segmentations_folder;
    if (!folder_name(root, "images", &images_folder)
        || !folder_name(root, "paths", &paths_folder)
        || !folder_name(root, "segmentations", &segmentations_folder)) {
        return false;
    }

    // models / meshes / simulations folders are optional in a project; default
    // to the conventional folder name when the element omits folder_name.
    std::string models_folder;
    std::string meshes_folder;
    std::string simulations_folder;
    if (!folder_name(root, "models", &models_folder)) {
        models_folder = "Models";
    }
    if (!folder_name(root, "meshes", &meshes_folder)) {
        meshes_folder = "Meshes";
    }
    if (!folder_name(root, "simulations", &simulations_folder)) {
        simulations_folder = "Simulations";
    }

    std::vector<PendingNode> pending_nodes;
    if (!read_image_nodes(root, images_folder, &pending_nodes)
        || !read_folder_nodes(project_dir, paths_folder, ".pth", XQDomainType::Path, &pending_nodes)
        || !read_folder_nodes(project_dir,
                              segmentations_folder,
                              ".ctgr",
                              XQDomainType::ContourGroup,
                              &pending_nodes)) {
        return false;
    }

    // Models / meshes / simulations are present in mature projects but may be
    // absent in early-stage ones; tolerate missing folders (read returns false).
    read_folder_nodes(project_dir, models_folder, ".mdl", XQDomainType::SurfaceModel, &pending_nodes);
    read_folder_nodes(project_dir, meshes_folder, ".msh", XQDomainType::Mesh, &pending_nodes);
    read_folder_nodes(project_dir,
                      simulations_folder,
                      ".sjb",
                      XQDomainType::SimulationCase,
                      &pending_nodes);

    XQProject parsed;
    std::vector<InsertedNode> inserted_nodes;
    if (!insert_nodes(project_dir, pending_nodes, &parsed.scene(), &inserted_nodes)
        || !link_path_contours(inserted_nodes, &parsed.scene())
        || !link_contours_models(inserted_nodes, &parsed.scene())
        || !link_by_name(inserted_nodes,
                         XQDomainType::SurfaceModel,
                         XQDomainType::Mesh,
                         &parsed.scene())
        || !link_by_name(inserted_nodes,
                         XQDomainType::Mesh,
                         XQDomainType::SimulationCase,
                         &parsed.scene())
        || parsed.open() != XQProject::LifecycleResult::Ok) {
        return false;
    }

    *project = parsed;
    return true;
}

} // namespace

SvProjectReader::Status SvProjectReader::load(const std::string& projectDir,
                                              XQProjectReadResult* out)
{
    if (out == 0) {
        return Status::ParseError;
    }

    XQProjectReadResult result = {};
    const std::filesystem::path project_dir = std::filesystem::path(projectDir);
    const std::filesystem::path project_file = project_dir / ".svproj";

    std::error_code error;
    if (!std::filesystem::is_regular_file(project_file, error) || error) {
        add_diagnostic(&result.diagnostics,
                       DiagnosticSeverity::Error,
                       "SVPROJECT_FILE_NOT_FOUND",
                       "svproject file could not be opened");
        *out = result;
        return Status::ProjectFileNotFound;
    }

    tinyxml2::XMLDocument document;
    if (document.LoadFile(project_file.string().c_str()) != tinyxml2::XML_SUCCESS) {
        add_diagnostic(&result.diagnostics,
                       DiagnosticSeverity::Error,
                       "SVPROJECT_PARSE_ERROR",
                       "svproject XML could not be parsed");
        *out = result;
        return Status::ParseError;
    }

    if (!build_project(project_dir, document, &result.project)) {
        add_diagnostic(&result.diagnostics,
                       DiagnosticSeverity::Error,
                       "SVPROJECT_PARSE_ERROR",
                       "svproject did not match the expected project structure");
        *out = result;
        return Status::ParseError;
    }

    *out = result;
    return Status::Ok;
}

} // namespace xq
