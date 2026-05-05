#include "xq_SolverConfigWriter.h"

#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------- XmlDocumentBuilder implementation ----------

XmlDocumentBuilder::XmlDocumentBuilder(std::string_view rootName)
{
    auto* decl = m_Doc.NewDeclaration();
    m_Doc.InsertFirstChild(decl);

    auto* root = m_Doc.NewElement(std::string(rootName).c_str());
    m_Doc.InsertEndChild(root);
    m_ElementStack.push_back(root);
}

tinyxml2::XMLElement* XmlDocumentBuilder::currentElement() const
{
    return m_ElementStack.back();
}

XmlDocumentBuilder& XmlDocumentBuilder::addAttribute(std::string_view name, std::string_view value)
{
    currentElement()->SetAttribute(std::string(name).c_str(), std::string(value).c_str());
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::addAttribute(std::string_view name, int value)
{
    currentElement()->SetAttribute(std::string(name).c_str(), value);
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::addAttribute(std::string_view name, double value)
{
    currentElement()->SetAttribute(std::string(name).c_str(), value);
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::beginElement(std::string_view name)
{
    auto* elem = m_Doc.NewElement(std::string(name).c_str());
    currentElement()->InsertEndChild(elem);
    m_ElementStack.push_back(elem);
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::endElement()
{
    if (m_ElementStack.size() > 1)
        m_ElementStack.pop_back();
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::addTextElement(std::string_view name, std::string_view text)
{
    auto* elem = m_Doc.NewElement(std::string(name).c_str());
    elem->SetText(std::string(text).c_str());
    currentElement()->InsertEndChild(elem);
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::addTextElement(std::string_view name, int value)
{
    auto* elem = m_Doc.NewElement(std::string(name).c_str());
    elem->SetText(value);
    currentElement()->InsertEndChild(elem);
    return *this;
}

XmlDocumentBuilder& XmlDocumentBuilder::addTextElement(std::string_view name, double value)
{
    auto* elem = m_Doc.NewElement(std::string(name).c_str());
    elem->SetText(value);
    currentElement()->InsertEndChild(elem);
    return *this;
}

void XmlDocumentBuilder::saveToFile(const fs::path& filePath)
{
    m_Doc.SaveFile(filePath.string().c_str());
}

// ---------- xq_SolverConfigWriter implementation ----------

void xq_SolverConfigWriter::CreateDocument(const xq_SolverJob& job,
                                            const std::map<std::string, std::string>& faceTypes,
                                            const fs::path& outputDir,
                                            std::string_view fileName)
{
    XmlDocumentBuilder builder("xq_solver_input");
    builder.addAttribute("version", "1.0");

    BuildGeneralSection(builder, job);
    BuildMeshSection(builder, job);
    BuildEquationSection(builder, job);
    BuildBoundaryConditions(builder, job, faceTypes);

    builder.saveToFile(outputDir / fileName);
}

void xq_SolverConfigWriter::BuildGeneralSection(XmlDocumentBuilder& builder,
                                                  const xq_SolverJob& job) const
{
    builder.beginElement("general_simulation_parameters")
           .addTextElement("solver_type", job.GetSolverType())
           .addTextElement("number_of_time_steps", job.GetNumTimesteps())
           .addTextElement("time_step_size", job.GetTimeStepSize())
           .addTextElement("number_of_cycles", job.GetNumCycles());

    for (const auto& [key, value] : job.GetRunProps())
        builder.addTextElement(key, value);

    builder.endElement();
}

void xq_SolverConfigWriter::BuildMeshSection(XmlDocumentBuilder& builder,
                                              const xq_SolverJob& job) const
{
    builder.beginElement("mesh")
           .addTextElement("mesh_directory", "mesh-complete")
           .addTextElement("mesh_file", "mesh-complete.mesh.vtu")
           .addTextElement("faces_directory", "mesh-complete");

    if (job.GetDeformable())
    {
        builder.beginElement("wall_properties")
               .addTextElement("deformable", "true")
               .addTextElement("thickness", job.GetWallThickness())
               .addTextElement("elastic_modulus", job.GetWallElasticModulus())
               .addTextElement("density", job.GetWallDensity())
               .endElement();
    }

    if (const auto& wallProps = job.GetWallProps(); !wallProps.empty())
    {
        builder.beginElement("wall_extra_properties");
        for (const auto& [key, value] : wallProps)
            builder.addTextElement(key, value);
        builder.endElement();
    }

    builder.endElement();
}

void xq_SolverConfigWriter::BuildEquationSection(XmlDocumentBuilder& builder,
                                                   const xq_SolverJob& job) const
{
    builder.beginElement("equation")
           .addTextElement("type", "navier_stokes")
           .beginElement("linear_solver")
               .addTextElement("type", job.GetSolverType())
               .addTextElement("max_iterations", job.GetNumLinearIterations())
           .endElement()
           .beginElement("nonlinear_solver")
               .addTextElement("max_iterations", job.GetNumNonlinearIterations())
           .endElement();

    if (const auto& solverProps = job.GetSolverProps(); !solverProps.empty())
    {
        builder.beginElement("solver_extra_properties");
        for (const auto& [key, value] : solverProps)
            builder.addTextElement(key, value);
        builder.endElement();
    }

    builder.endElement();
}

void xq_SolverConfigWriter::BuildBoundaryConditions(
    XmlDocumentBuilder& builder,
    const xq_SolverJob& job,
    const std::map<std::string, std::string>& faceTypes) const
{
    builder.beginElement("boundary_conditions");

    int faceId = 2; // wall is face 1
    for (const auto& [capName, props] : job.GetCapProps())
    {
        auto faceTypeIt = faceTypes.find(capName);
        const auto& faceType = (faceTypeIt != faceTypes.end())
                                   ? faceTypeIt->second
                                   : std::string("prescribed_velocities");

        builder.beginElement("boundary_condition")
               .addTextElement("name", capName)
               .addTextElement("face_id", faceId)
               .addTextElement("type", faceType);

        for (const auto& [key, value] : props)
            builder.addTextElement(key, value);

        builder.endElement();
        ++faceId;
    }

    builder.endElement();
}
