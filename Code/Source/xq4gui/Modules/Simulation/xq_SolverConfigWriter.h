#pragma once

#include <xqModuleSimulationExports.h>
#include "xq_SolverJob.h"

#include <tinyxml2.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Fluent XML document builder for constructing solver configuration files
class XQMODULESIMULATION_EXPORT XmlDocumentBuilder
{
public:
    explicit XmlDocumentBuilder(std::string_view rootName);

    XmlDocumentBuilder& addAttribute(std::string_view name, std::string_view value);
    XmlDocumentBuilder& addAttribute(std::string_view name, int value);
    XmlDocumentBuilder& addAttribute(std::string_view name, double value);
    XmlDocumentBuilder& beginElement(std::string_view name);
    XmlDocumentBuilder& endElement();
    XmlDocumentBuilder& addTextElement(std::string_view name, std::string_view text);
    XmlDocumentBuilder& addTextElement(std::string_view name, int value);
    XmlDocumentBuilder& addTextElement(std::string_view name, double value);

    void saveToFile(const std::filesystem::path& filePath);

private:
    [[nodiscard]] tinyxml2::XMLElement* currentElement() const;

    tinyxml2::XMLDocument m_Doc;
    std::vector<tinyxml2::XMLElement*> m_ElementStack;
};

class XQMODULESIMULATION_EXPORT xq_SolverConfigWriter
{
public:
    xq_SolverConfigWriter() = default;
    ~xq_SolverConfigWriter() = default;

    void CreateDocument(const xq_SolverJob& job,
                        const std::map<std::string, std::string>& faceTypes,
                        const std::filesystem::path& outputDir,
                        std::string_view fileName);

private:
    void BuildGeneralSection(XmlDocumentBuilder& builder, const xq_SolverJob& job) const;
    void BuildMeshSection(XmlDocumentBuilder& builder, const xq_SolverJob& job) const;
    void BuildEquationSection(XmlDocumentBuilder& builder, const xq_SolverJob& job) const;
    void BuildBoundaryConditions(XmlDocumentBuilder& builder,
                                 const xq_SolverJob& job,
                                 const std::map<std::string, std::string>& faceTypes) const;
};
