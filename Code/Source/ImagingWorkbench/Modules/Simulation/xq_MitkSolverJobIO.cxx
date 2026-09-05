#include "xq_MitkSolverJobIO.h"
#include "xq_MitkSolverJob.h"
#include "xq_SolverJob.h"
#include "xq_XmlIOUtil.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>
#include <tinyxml2.h>

#include <filesystem>
#include <fstream>

namespace {

constexpr auto kMimeName  = "application/vnd.xq.simjob";
constexpr auto kExtension = "xqsjb";

void serializePropertyMap(tinyxml2::XMLDocument& doc,
                          tinyxml2::XMLElement* parent,
                          const char* sectionName,
                          const XqSimPropertyMap& props)
{
    auto* section = doc.NewElement(sectionName);
    parent->InsertEndChild(section);
    for (const auto& [key, value] : props)
    {
        auto* propElem = doc.NewElement("prop");
        propElem->SetAttribute("key", key.c_str());
        propElem->SetAttribute("value", value.c_str());
        section->InsertEndChild(propElem);
    }
}

[[nodiscard]] XqSimPropertyMap deserializePropertyMap(tinyxml2::XMLElement* section)
{
    XqSimPropertyMap props;
    if (!section)
        return props;

    for (auto* elem = section->FirstChildElement("prop");
         elem;
         elem = elem->NextSiblingElement("prop"))
    {
        auto key   = xq_XmlIOUtil::ReadStringAttribute(elem, "key");
        auto value = xq_XmlIOUtil::ReadStringAttribute(elem, "value");
        if (!key.empty())
            props[std::move(key)] = std::move(value);
    }
    return props;
}

void serializeCapPropertyMap(tinyxml2::XMLDocument& doc,
                             tinyxml2::XMLElement* parent,
                             const char* sectionName,
                             const XqSimCapPropertyMap& capProps)
{
    auto* section = doc.NewElement(sectionName);
    parent->InsertEndChild(section);
    for (const auto& [capName, props] : capProps)
    {
        auto* capElem = doc.NewElement("cap");
        capElem->SetAttribute("name", capName.c_str());
        section->InsertEndChild(capElem);
        for (const auto& [key, value] : props)
        {
            auto* propElem = doc.NewElement("prop");
            propElem->SetAttribute("key", key.c_str());
            propElem->SetAttribute("value", value.c_str());
            capElem->InsertEndChild(propElem);
        }
    }
}

[[nodiscard]] XqSimCapPropertyMap deserializeCapPropertyMap(tinyxml2::XMLElement* section)
{
    XqSimCapPropertyMap capProps;
    if (!section)
        return capProps;

    for (auto* capElem = section->FirstChildElement("cap");
         capElem;
         capElem = capElem->NextSiblingElement("cap"))
    {
        auto capName = xq_XmlIOUtil::ReadStringAttribute(capElem, "name");
        if (capName.empty())
            continue;

        for (auto* propElem = capElem->FirstChildElement("prop");
             propElem;
             propElem = propElem->NextSiblingElement("prop"))
        {
            auto key   = xq_XmlIOUtil::ReadStringAttribute(propElem, "key");
            auto value = xq_XmlIOUtil::ReadStringAttribute(propElem, "value");
            if (!key.empty())
                capProps[capName][std::move(key)] = std::move(value);
        }
    }
    return capProps;
}

// Safely extract an optional string attribute from an XML element
[[nodiscard]] std::string safeAttribute(const tinyxml2::XMLElement* elem, const char* name)
{
    const auto* attr = elem->Attribute(name);
    return attr ? std::string(attr) : std::string{};
}

} // anonymous namespace

xq_MitkSolverJobIO::xq_MitkSolverJobIO()
    : mitk::AbstractFileIO(xq_MitkSolverJob::GetStaticNameOfClass(),
                           mitk::CustomMimeType(kMimeName),
                           "XQ Simulation Job")
{
    mitk::CustomMimeType mimeType(kMimeName);
    mimeType.SetCategory("XQ Files");
    mimeType.SetComment("XQ Simulation Job");
    mimeType.AddExtension(kExtension);
    this->SetMimeType(mimeType);

    this->SetReaderDescription("XQ Simulation Job Reader");
    this->SetWriterDescription("XQ Simulation Job Writer");

    RegisterService();
}

// Copy constructor required by IOClone() for MITK's AbstractFileIO registration.
// The declaration lives in the header but the definition was missing — that
// caused the XQ executable to fail at startup with
// "undefined symbol: _ZN18xq_MitkSolverJobIOC1ERKS_".
xq_MitkSolverJobIO::xq_MitkSolverJobIO(const xq_MitkSolverJobIO& other)
    : mitk::AbstractFileIO(other)
{
}

std::vector<mitk::BaseData::Pointer> xq_MitkSolverJobIO::DoRead()
{
    const auto filename = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_MitkSolverJobIO: Failed to load file: " << filename;

    auto* root = doc.FirstChildElement("xq_job");
    if (!root)
        mitkThrow() << "xq_MitkSolverJobIO: Missing <xq_job> root element in " << filename;

    auto job = std::make_unique<xq_SolverJob>();

    // Named attributes on root
    if (const auto* name = root->Attribute("name"))
        job->SetJobName(name);

    int intVal = 0;
    double dblVal = 0.0;

    if (root->QueryIntAttribute("num_timesteps", &intVal) == tinyxml2::XML_SUCCESS)
        job->SetNumTimesteps(intVal);
    if (root->QueryDoubleAttribute("timestep_size", &dblVal) == tinyxml2::XML_SUCCESS)
        job->SetTimeStepSize(dblVal);
    if (root->QueryIntAttribute("num_cycles", &intVal) == tinyxml2::XML_SUCCESS)
        job->SetNumCycles(intVal);

    // Wall section
    if (auto* wallElem = root->FirstChildElement("wall"))
    {
        bool deformable = false;
        if (wallElem->QueryBoolAttribute("deformable", &deformable) == tinyxml2::XML_SUCCESS)
            job->SetDeformable(deformable);
        if (wallElem->QueryDoubleAttribute("thickness", &dblVal) == tinyxml2::XML_SUCCESS)
            job->SetWallThickness(dblVal);
        if (wallElem->QueryDoubleAttribute("elastic_modulus", &dblVal) == tinyxml2::XML_SUCCESS)
            job->SetWallElasticModulus(dblVal);
        if (wallElem->QueryDoubleAttribute("density", &dblVal) == tinyxml2::XML_SUCCESS)
            job->SetWallDensity(dblVal);
    }

    // Solver section
    if (auto* solverElem = root->FirstChildElement("solver"))
    {
        if (const auto* solverType = solverElem->Attribute("type"))
            job->SetSolverType(solverType);
        if (solverElem->QueryIntAttribute("linear_iterations", &intVal) == tinyxml2::XML_SUCCESS)
            job->SetNumLinearIterations(intVal);
        if (solverElem->QueryIntAttribute("nonlinear_iterations", &intVal) == tinyxml2::XML_SUCCESS)
            job->SetNumNonlinearIterations(intVal);
    }

    // Property maps
    job->SetBasicProps(deserializePropertyMap(root->FirstChildElement("basic_props")));
    job->SetWallProps(deserializePropertyMap(root->FirstChildElement("wall_props")));
    job->SetSolverProps(deserializePropertyMap(root->FirstChildElement("solver_props")));
    job->SetRunProps(deserializePropertyMap(root->FirstChildElement("run_props")));

    // Cap property maps
    job->SetCapProps(deserializeCapPropertyMap(root->FirstChildElement("cap_props")));
    job->SetIcProps(deserializeCapPropertyMap(root->FirstChildElement("ic_props")));

    // Metadata
    auto meshName  = safeAttribute(root, "mesh_name");
    auto modelName = safeAttribute(root, "model_name");
    auto status    = safeAttribute(root, "status");

    auto mitkJob = xq_MitkSolverJob::New();
    mitkJob->SetSimJob(std::move(job), 0);
    mitkJob->SetMeshName(meshName);
    mitkJob->SetModelName(modelName);
    mitkJob->SetStatus(status);
    mitkJob->SetDataModified(false);

    return { mitkJob.GetPointer() };
}

mitk::IFileIO::ConfidenceLevel xq_MitkSolverJobIO::GetReaderConfidenceLevel() const
{
    if (mitk::AbstractFileIO::GetReaderConfidenceLevel() == mitk::IFileIO::Unsupported)
        return mitk::IFileIO::Unsupported;

    const auto inputPath = std::filesystem::path(this->GetInputLocation());
    if (inputPath.extension() == std::string(".") + kExtension)
        return mitk::IFileIO::Supported;

    return mitk::IFileIO::Unsupported;
}

void xq_MitkSolverJobIO::Write()
{
    ValidateOutputLocation();

    const auto* mitkJob = dynamic_cast<const xq_MitkSolverJob*>(this->GetInput());
    if (!mitkJob)
        mitkThrow() << "xq_MitkSolverJobIO: Input is not an xq_MitkSolverJob";

    const auto* job = mitkJob->GetSimJob(0);
    if (!job)
        mitkThrow() << "xq_MitkSolverJobIO: No simulation job data at time step 0";

    tinyxml2::XMLDocument doc;
    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("xq_job");
    root->SetAttribute("version", "1.0");
    root->SetAttribute("name", job->GetJobName().c_str());
    root->SetAttribute("num_timesteps", job->GetNumTimesteps());
    root->SetAttribute("timestep_size", job->GetTimeStepSize());
    root->SetAttribute("num_cycles", job->GetNumCycles());
    root->SetAttribute("mesh_name", mitkJob->GetMeshName().c_str());
    root->SetAttribute("model_name", mitkJob->GetModelName().c_str());
    root->SetAttribute("status", mitkJob->GetStatus().c_str());
    doc.InsertEndChild(root);

    // Wall
    auto* wallElem = doc.NewElement("wall");
    wallElem->SetAttribute("deformable", job->GetDeformable());
    wallElem->SetAttribute("thickness", job->GetWallThickness());
    wallElem->SetAttribute("elastic_modulus", job->GetWallElasticModulus());
    wallElem->SetAttribute("density", job->GetWallDensity());
    root->InsertEndChild(wallElem);

    // Solver
    auto* solverElem = doc.NewElement("solver");
    solverElem->SetAttribute("type", job->GetSolverType().c_str());
    solverElem->SetAttribute("linear_iterations", job->GetNumLinearIterations());
    solverElem->SetAttribute("nonlinear_iterations", job->GetNumNonlinearIterations());
    root->InsertEndChild(solverElem);

    // Property maps
    serializePropertyMap(doc, root, "basic_props",  job->GetBasicProps());
    serializePropertyMap(doc, root, "wall_props",   job->GetWallProps());
    serializePropertyMap(doc, root, "solver_props", job->GetSolverProps());
    serializePropertyMap(doc, root, "run_props",    job->GetRunProps());

    // Cap property maps
    serializeCapPropertyMap(doc, root, "cap_props", job->GetCapProps());
    serializeCapPropertyMap(doc, root, "ic_props",  job->GetIcProps());

    const auto outputFile = this->GetOutputLocation();
    if (doc.SaveFile(outputFile.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_MitkSolverJobIO: Failed to save file: " << outputFile;
}

mitk::IFileIO::ConfidenceLevel xq_MitkSolverJobIO::GetWriterConfidenceLevel() const
{
    const auto* mitkJob = dynamic_cast<const xq_MitkSolverJob*>(this->GetInput());
    return mitkJob ? mitk::IFileIO::Supported : mitk::IFileIO::Unsupported;
}

xq_MitkSolverJobIO* xq_MitkSolverJobIO::IOClone() const
{
    return new xq_MitkSolverJobIO(*this);
}
