#include "xq_MitkROMJobIO.h"
#include "xq_MitkROMJob.h"

#include <fstream>
#include <sstream>
#include <tinyxml2.h>

xq_MitkROMJobIO::Result
xq_MitkROMJobIO::Write(const mitk::DataNode* node, const std::string& filePath)
{
    Result r;

    if (!node)
    {
        r.diagnostic = "Node is null.";
        return r;
    }

    auto* mitkJob = dynamic_cast<const xq_MitkROMJob*>(node->GetData());
    if (!mitkJob)
    {
        r.diagnostic = "Node data is not xq_MitkROMJob.";
        return r;
    }

    auto* job = mitkJob->GetROMJob(0);
    if (!job)
    {
        r.diagnostic = "Job slot 0 is empty.";
        return r;
    }

    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    auto* root = doc.NewElement("rom_job");
    doc.InsertEndChild(root);

    auto addEl = [&](const char* name, const std::string& val) {
        auto* e = doc.NewElement(name);
        e->SetText(val.c_str());
        root->InsertEndChild(e);
    };

    addEl("job_name", job->GetJobName());
    addEl("model_type", job->GetModelType());
    addEl("time_step_size", std::to_string(job->GetTimeStepSize()));
    addEl("num_time_steps", std::to_string(job->GetNumTimeSteps()));
    addEl("solver_tolerance", std::to_string(job->GetSolverTolerance()));
    addEl("max_solver_iterations", std::to_string(job->GetMaxSolverIterations()));
    addEl("wall_thickness", std::to_string(job->GetWallThickness()));
    addEl("wall_elastic_modulus", std::to_string(job->GetWallElasticModulus()));
    addEl("wall_poisson_ratio", std::to_string(job->GetWallPoissonRatio()));
    addEl("output_format", job->GetOutputFormat());

    auto* capsEl = doc.NewElement("caps");
    for (const auto& [capName, props] : job->GetCapProps())
    {
        auto* cEl = doc.NewElement("cap");
        cEl->SetAttribute("name", capName.c_str());
        for (const auto& [k, v] : props)
        {
            auto* pEl = doc.NewElement(k.c_str());
            pEl->SetText(v.c_str());
            cEl->InsertEndChild(pEl);
        }
        capsEl->InsertEndChild(cEl);
    }
    root->InsertEndChild(capsEl);

    auto* fieldsEl = doc.NewElement("output_fields");
    for (const auto& f : job->GetOutputFields())
    {
        auto* fEl = doc.NewElement("field");
        fEl->SetText(f.c_str());
        fieldsEl->InsertEndChild(fEl);
    }
    root->InsertEndChild(fieldsEl);

    auto* propsEl = doc.NewElement("properties");
    for (const auto& [key, value] : job->GetProperties())
    {
        auto* propEl = doc.NewElement("property");
        propEl->SetAttribute("key", key.c_str());
        propEl->SetText(value.c_str());
        propsEl->InsertEndChild(propEl);
    }
    root->InsertEndChild(propsEl);

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);

    std::ofstream ofs(filePath);
    if (!ofs)
    {
        r.diagnostic = "Cannot open file for writing: " + filePath;
        return r;
    }
    ofs << printer.CStr();
    r.ok = true;
    return r;
}

xq_MitkROMJobIO::Result
xq_MitkROMJobIO::Read(mitk::DataNode* node, const std::string& filePath)
{
    Result r;

    if (!node)
    {
        r.diagnostic = "Node is null.";
        return r;
    }

    std::ifstream ifs(filePath);
    if (!ifs)
    {
        r.diagnostic = "Cannot open file for reading: " + filePath;
        return r;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string xml = oss.str();

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    {
        r.diagnostic = "Failed to parse XML.";
        return r;
    }

    auto* root = doc.FirstChildElement("rom_job");
    if (!root)
    {
        r.diagnostic = "Missing <rom_job> root element.";
        return r;
    }

    auto mitkJob = xq_MitkROMJob::New();
    auto job = std::make_unique<xq_ROMJob>();

    auto readText = [&](const char* name) -> std::string {
        auto* el = root->FirstChildElement(name);
        if (el && el->GetText()) return el->GetText();
        return "";
    };

    job->SetJobName(readText("job_name"));
    job->SetModelType(readText("model_type"));
    try { job->SetTimeStepSize(std::stod(readText("time_step_size"))); } catch (...) {}
    try { job->SetNumTimeSteps(std::stoi(readText("num_time_steps"))); } catch (...) {}
    try { job->SetSolverTolerance(std::stod(readText("solver_tolerance"))); } catch (...) {}
    try { job->SetMaxSolverIterations(std::stoi(readText("max_solver_iterations"))); } catch (...) {}
    try { job->SetWallThickness(std::stod(readText("wall_thickness"))); } catch (...) {}
    try { job->SetWallElasticModulus(std::stod(readText("wall_elastic_modulus"))); } catch (...) {}
    try { job->SetWallPoissonRatio(std::stod(readText("wall_poisson_ratio"))); } catch (...) {}
    job->SetOutputFormat(readText("output_format"));

    // Read caps
    if (auto* capsEl = root->FirstChildElement("caps"))
    {
        for (auto* cEl = capsEl->FirstChildElement("cap"); cEl;
             cEl = cEl->NextSiblingElement("cap"))
        {
            const char* capName = cEl->Attribute("name");
            if (!capName) continue;
            for (auto* pEl = cEl->FirstChildElement(); pEl;
                 pEl = pEl->NextSiblingElement())
            {
                const char* txt = pEl->GetText();
                if (txt) job->SetCapProp(capName, pEl->Name(), txt);
            }
        }
    }

    if (auto* fieldsEl = root->FirstChildElement("output_fields"))
    {
        for (auto* fEl = fieldsEl->FirstChildElement("field"); fEl;
             fEl = fEl->NextSiblingElement("field"))
        {
            if (const char* txt = fEl->GetText())
                job->AddOutputField(txt);
        }
    }

    if (auto* propsEl = root->FirstChildElement("properties"))
    {
        for (auto* propEl = propsEl->FirstChildElement("property"); propEl;
             propEl = propEl->NextSiblingElement("property"))
        {
            const char* key = propEl->Attribute("key");
            const char* value = propEl->GetText();
            if (key && value)
                job->SetProperty(key, value);
        }
    }

    mitkJob->SetROMJob(std::move(job), 0);
    node->SetData(mitkJob);
    r.ok = true;
    return r;
}
