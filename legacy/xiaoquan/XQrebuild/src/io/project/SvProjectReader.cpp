#include "io/project/SvProjectReader.h"

#include <tinyxml2.h>

#include "core/XQDataNode.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace xq {
namespace {

struct PendingNode {
    std::string domain_type;
    std::string display_name;
};

struct InsertedNode {
    NodeId id;
    std::string domain_type;
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

bool read_image_nodes(const tinyxml2::XMLElement* root, std::vector<PendingNode>* nodes)
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

        PendingNode node = {};
        node.domain_type = "image";
        node.display_name = image_name;
        nodes->push_back(node);
    }

    return true;
}

bool read_folder_nodes(const std::filesystem::path& project_dir,
                       const std::string& folder,
                       const char* extension,
                       const char* domain_type,
                       std::vector<PendingNode>* nodes)
{
    if (extension == 0 || domain_type == 0 || nodes == 0) {
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
        PendingNode node = {};
        node.domain_type = domain_type;
        node.display_name = stem_string(*file);
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

bool insert_nodes(const std::vector<PendingNode>& pending_nodes,
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
        if (scene->insert(XQDataNode(node_id, it->domain_type, it->display_name))
            != XQScene::InsertResult::Inserted) {
            return false;
        }

        InsertedNode inserted = {};
        inserted.id = node_id;
        inserted.domain_type = it->domain_type;
        inserted.display_name = it->display_name;
        inserted_nodes->push_back(inserted);
        ++next_id;
    }

    return true;
}

bool link_path_contours(const std::vector<InsertedNode>& nodes, XQScene* scene)
{
    if (scene == 0) {
        return false;
    }

    std::map<std::string, NodeId> path_by_name;
    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain_type == "path") {
            path_by_name[it->display_name] = it->id;
        }
    }

    for (std::vector<InsertedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (it->domain_type != "contour_group") {
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

    std::string paths_folder;
    std::string segmentations_folder;
    if (!folder_name(root, "paths", &paths_folder)
        || !folder_name(root, "segmentations", &segmentations_folder)) {
        return false;
    }

    std::vector<PendingNode> pending_nodes;
    if (!read_image_nodes(root, &pending_nodes)
        || !read_folder_nodes(project_dir, paths_folder, ".pth", "path", &pending_nodes)
        || !read_folder_nodes(project_dir,
                              segmentations_folder,
                              ".ctgr",
                              "contour_group",
                              &pending_nodes)) {
        return false;
    }

    XQProject parsed;
    std::vector<InsertedNode> inserted_nodes;
    if (!insert_nodes(pending_nodes, &parsed.scene(), &inserted_nodes)
        || !link_path_contours(inserted_nodes, &parsed.scene())
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
