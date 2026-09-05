#include "xq_MultiPhysicsXmlWriter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <tinyxml2.h>

// Deterministic field order helpers.
namespace
{

void WriteTextElement(tinyxml2::XMLDocument& doc,
                      tinyxml2::XMLElement* parent,
                      const char* name, const std::string& value)
{
    auto* el = doc.NewElement(name);
    el->SetText(value.c_str());
    parent->InsertEndChild(el);
}

void WriteTextElement(tinyxml2::XMLDocument& doc,
                      tinyxml2::XMLElement* parent,
                      const char* name, double value)
{
    WriteTextElement(doc, parent, name, std::to_string(value));
}

void WriteTextElement(tinyxml2::XMLDocument& doc,
                      tinyxml2::XMLElement* parent,
                      const char* name, int value)
{
    WriteTextElement(doc, parent, name, std::to_string(value));
}

void WriteKeyValueElement(tinyxml2::XMLDocument& doc,
                          tinyxml2::XMLElement* parent,
                          const char* name,
                          const std::string& key,
                          const std::string& value)
{
    auto* el = doc.NewElement(name);
    el->SetAttribute("key", key.c_str());
    el->SetAttribute("value", value.c_str());
    parent->InsertEndChild(el);
}

std::string ToLowerCopy(std::string_view value)
{
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

bool TryParseDomainType(std::string_view text, xq_MultiPhysicsDomainType& out)
{
    const auto lower = ToLowerCopy(text);
    if (lower == "fluid")
    {
        out = xq_MultiPhysicsDomainType::Fluid;
        return true;
    }
    if (lower == "solid")
    {
        out = xq_MultiPhysicsDomainType::Solid;
        return true;
    }
    if (lower == "mesh")
    {
        out = xq_MultiPhysicsDomainType::Mesh;
        return true;
    }
    return false;
}

bool TryParseEquationType(std::string_view text, xq_MultiPhysicsEquationType& out)
{
    const auto lower = ToLowerCopy(text);
    if (lower == "fluid")
    {
        out = xq_MultiPhysicsEquationType::Fluid;
        return true;
    }
    if (lower == "struct")
    {
        out = xq_MultiPhysicsEquationType::Structure;
        return true;
    }
    if (lower == "fsi")
    {
        out = xq_MultiPhysicsEquationType::FSI;
        return true;
    }
    return false;
}

bool TryParseBcType(std::string_view text, xq_MultiPhysicsBCType& out)
{
    const auto lower = ToLowerCopy(text);
    if (lower == "dirichlet")
    {
        out = xq_MultiPhysicsBCType::Dirichlet;
        return true;
    }
    if (lower == "neumann")
    {
        out = xq_MultiPhysicsBCType::Neumann;
        return true;
    }
    if (lower == "traction")
    {
        out = xq_MultiPhysicsBCType::Traction;
        return true;
    }
    if (lower == "resistance")
    {
        out = xq_MultiPhysicsBCType::Resistance;
        return true;
    }
    return false;
}

} // namespace

xq_MultiPhysicsXmlWriter::Result
xq_MultiPhysicsXmlWriter::Write(const xq_MultiPhysicsJob& job)
{
    Result r;

    std::string validationMsg = job.Validate();
    if (!validationMsg.empty())
    {
        r.diagnostic = "Validation failed: " + validationMsg;
        return r;
    }

    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    auto* root = doc.NewElement("multiphysics_job");
    doc.InsertEndChild(root);

    // -- Job identity --
    WriteTextElement(doc, root, "job_name", job.GetJobName());

    // -- Time stepping --
    auto* ts = doc.NewElement("time_stepping");
    WriteTextElement(doc, ts, "time_step_size", job.GetTimeStepSize());
    WriteTextElement(doc, ts, "num_time_steps", job.GetNumTimeSteps());
    root->InsertEndChild(ts);

    // -- Domains --
    auto* domainsEl = doc.NewElement("domains");
    for (const auto& domain : job.GetDomains())
    {
        auto* dEl = doc.NewElement("domain");
        dEl->SetAttribute("name", domain.name.c_str());
        WriteTextElement(doc, dEl, "type", std::string(ToString(domain.type)));

        auto* matEl = doc.NewElement("material");
        WriteTextElement(doc, matEl, "density", domain.material.density);
        WriteTextElement(doc, matEl, "viscosity", domain.material.viscosity);
        WriteTextElement(doc, matEl, "elastic_modulus", domain.material.elasticModulus);
        WriteTextElement(doc, matEl, "poisson_ratio", domain.material.poissonRatio);
        dEl->InsertEndChild(matEl);

        auto* propsEl = doc.NewElement("properties");
        for (const auto& [k, v] : domain.properties)
        {
            auto* pEl = doc.NewElement("property");
            pEl->SetAttribute("key", k.c_str());
            pEl->SetText(v.c_str());
            propsEl->InsertEndChild(pEl);
        }
        dEl->InsertEndChild(propsEl);

        domainsEl->InsertEndChild(dEl);
    }
    root->InsertEndChild(domainsEl);

    // -- Equations --
    auto* eqEl = doc.NewElement("equations");
    for (const auto& eq : job.GetEquations())
    {
        auto* eEl = doc.NewElement("equation");
        eEl->SetAttribute("name", eq.name.c_str());
        WriteTextElement(doc, eEl, "type", std::string(ToString(eq.type)));

        auto* domsEl = doc.NewElement("domain_names");
        for (const auto& dn : eq.domainNames)
            WriteTextElement(doc, domsEl, "domain", dn);
        eEl->InsertEndChild(domsEl);

        auto* solEl = doc.NewElement("solver_settings");
        WriteTextElement(doc, solEl, "linear_solver", eq.solverSettings.linearSolver);
        WriteTextElement(doc, solEl, "tolerance", eq.solverSettings.tolerance);
        WriteTextElement(doc, solEl, "max_iterations", eq.solverSettings.maxIterations);
        eEl->InsertEndChild(solEl);

        auto* propsEl = doc.NewElement("properties");
        for (const auto& [k, v] : eq.properties)
        {
            auto* pEl = doc.NewElement("property");
            pEl->SetAttribute("key", k.c_str());
            pEl->SetText(v.c_str());
            propsEl->InsertEndChild(pEl);
        }
        eEl->InsertEndChild(propsEl);

        eqEl->InsertEndChild(eEl);
    }
    root->InsertEndChild(eqEl);

    // -- Boundary conditions --
    auto* bcsEl = doc.NewElement("boundary_conditions");
    for (const auto& bc : job.GetBoundaryConditions())
    {
        auto* bEl = doc.NewElement("bc");
        bEl->SetAttribute("face", bc.faceName.c_str());
        WriteTextElement(doc, bEl, "domain", bc.domainName);
        WriteTextElement(doc, bEl, "type", std::string(ToString(bc.bcType)));

        for (const auto& [k, v] : bc.parameters)
            WriteKeyValueElement(doc, bEl, "param", k, v);

        bcsEl->InsertEndChild(bEl);
    }
    root->InsertEndChild(bcsEl);

    // -- Properties --
    auto* propsEl = doc.NewElement("properties");
    for (const auto& [k, v] : job.GetProperties())
    {
        auto* pEl = doc.NewElement("property");
        pEl->SetAttribute("key", k.c_str());
        pEl->SetText(v.c_str());
        propsEl->InsertEndChild(pEl);
    }
    root->InsertEndChild(propsEl);

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    r.xml = printer.CStr();
    r.ok = true;
    return r;
}

xq_MultiPhysicsXmlWriter::Result
xq_MultiPhysicsXmlWriter::Read(const std::string& xml, xq_MultiPhysicsJob& outJob)
{
    Result r;
    xq_MultiPhysicsJob parsedJob;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    {
        r.diagnostic = "Failed to parse XML.";
        return r;
    }

    auto* root = doc.FirstChildElement("multiphysics_job");
    if (!root)
    {
        r.diagnostic = "Missing <multiphysics_job> root element.";
        return r;
    }

    // Job name
    if (auto* el = root->FirstChildElement("job_name"))
    {
        const char* txt = el->GetText();
        if (txt)
            parsedJob.SetJobName(txt);
    }

    // Time stepping
    if (auto* ts = root->FirstChildElement("time_stepping"))
    {
        if (auto* el = ts->FirstChildElement("time_step_size"))
        {
            const char* txt = el->GetText();
            if (txt)
            {
                try
                {
                    parsedJob.SetTimeStepSize(std::stod(txt));
                }
                catch (...)
                {
                    r.diagnostic = "Invalid multi-physics time_step_size.";
                    return r;
                }
            }
        }
        if (auto* el = ts->FirstChildElement("num_time_steps"))
        {
            const char* txt = el->GetText();
            if (txt)
            {
                try
                {
                    parsedJob.SetNumTimeSteps(std::stoi(txt));
                }
                catch (...)
                {
                    r.diagnostic = "Invalid multi-physics num_time_steps.";
                    return r;
                }
            }
        }
    }

    // Domains
    if (auto* domainsEl = root->FirstChildElement("domains"))
    {
        for (auto* dEl = domainsEl->FirstChildElement("domain"); dEl;
             dEl = dEl->NextSiblingElement("domain"))
        {
            xq_MultiPhysicsDomain domain;
            const char* nameAttr = dEl->Attribute("name");
            if (nameAttr) domain.name = nameAttr;

            auto* typeEl = dEl->FirstChildElement("type");
            if (!typeEl || !typeEl->GetText() ||
                !TryParseDomainType(typeEl->GetText(), domain.type))
            {
                r.diagnostic = "Unknown or missing multi-physics domain type.";
                return r;
            }
            if (auto* matEl = dEl->FirstChildElement("material"))
            {
                auto readDbl = [&](const char* name) -> double {
                    auto* el = matEl->FirstChildElement(name);
                    if (el && el->GetText())
                    { try { return std::stod(el->GetText()); } catch (...) {} }
                        return 0.0;
                };
                domain.material.density = readDbl("density");
                domain.material.viscosity = readDbl("viscosity");
                domain.material.elasticModulus = readDbl("elastic_modulus");
                domain.material.poissonRatio = readDbl("poisson_ratio");
            }
            if (auto* propsEl = dEl->FirstChildElement("properties"))
            {
                for (auto* pEl = propsEl->FirstChildElement("property"); pEl;
                     pEl = pEl->NextSiblingElement("property"))
                {
                    const char* key = pEl->Attribute("key");
                    const char* txt = pEl->GetText();
                    if (key && txt)
                        domain.properties[key] = txt;
                }
            }
            parsedJob.AddDomain(domain);
        }
    }

    // Equations
    if (auto* eqsEl = root->FirstChildElement("equations"))
    {
        for (auto* eEl = eqsEl->FirstChildElement("equation"); eEl;
             eEl = eEl->NextSiblingElement("equation"))
        {
            xq_MultiPhysicsEquation eq;
            const char* nameAttr = eEl->Attribute("name");
            if (nameAttr) eq.name = nameAttr;

            auto* typeEl = eEl->FirstChildElement("type");
            if (!typeEl || !typeEl->GetText() ||
                !TryParseEquationType(typeEl->GetText(), eq.type))
            {
                r.diagnostic = "Unknown or missing multi-physics equation type.";
                return r;
            }
            if (auto* domsEl = eEl->FirstChildElement("domain_names"))
            {
                for (auto* dnEl = domsEl->FirstChildElement("domain"); dnEl;
                     dnEl = dnEl->NextSiblingElement("domain"))
                {
                    const char* txt = dnEl->GetText();
                    if (txt) eq.domainNames.push_back(txt);
                }
            }
            if (auto* solEl = eEl->FirstChildElement("solver_settings"))
            {
                if (auto* el = solEl->FirstChildElement("linear_solver"))
                { const char* txt = el->GetText(); if (txt) eq.solverSettings.linearSolver = txt; }
                if (auto* el = solEl->FirstChildElement("tolerance"))
                { const char* txt = el->GetText(); if (txt)
                  { try { eq.solverSettings.tolerance = std::stod(txt); } catch(...) {} } }
                if (auto* el = solEl->FirstChildElement("max_iterations"))
                { const char* txt = el->GetText(); if (txt)
                  { try { eq.solverSettings.maxIterations = std::stoi(txt); } catch(...) {} } }
            }
            if (auto* propsEl = eEl->FirstChildElement("properties"))
            {
                for (auto* pEl = propsEl->FirstChildElement("property"); pEl;
                     pEl = pEl->NextSiblingElement("property"))
                {
                    const char* key = pEl->Attribute("key");
                    const char* txt = pEl->GetText();
                    if (key && txt)
                        eq.properties[key] = txt;
                }
            }
            parsedJob.AddEquation(eq);
        }
    }

    // Boundary conditions
    if (auto* bcsEl = root->FirstChildElement("boundary_conditions"))
    {
        for (auto* bEl = bcsEl->FirstChildElement("bc"); bEl;
             bEl = bEl->NextSiblingElement("bc"))
        {
            xq_MultiPhysicsBoundaryCondition bc;
            const char* faceAttr = bEl->Attribute("face");
            if (faceAttr) bc.faceName = faceAttr;

            if (auto* domEl = bEl->FirstChildElement("domain"))
            { const char* txt = domEl->GetText(); if (txt) bc.domainName = txt; }
            auto* typeEl = bEl->FirstChildElement("type");
            if (!typeEl || !typeEl->GetText() ||
                !TryParseBcType(typeEl->GetText(), bc.bcType))
            {
                r.diagnostic = "Unknown or missing multi-physics boundary-condition type.";
                return r;
            }

            for (auto* pEl = bEl->FirstChildElement("param"); pEl;
                 pEl = pEl->NextSiblingElement("param"))
            {
                const char* keyAttr = pEl->Attribute("key");
                const char* valueAttr = pEl->Attribute("value");
                if (keyAttr && valueAttr)
                {
                    bc.parameters[keyAttr] = valueAttr;
                    continue;
                }

                const char* txt = pEl->GetText();
                if (!txt)
                    continue;

                std::string paramStr(txt);
                auto eqPos = paramStr.find('=');
                if (eqPos == std::string::npos || eqPos == 0)
                {
                    r.diagnostic = "Malformed multi-physics BC parameter.";
                    return r;
                }
                bc.parameters[paramStr.substr(0, eqPos)] = paramStr.substr(eqPos + 1);
            }
            parsedJob.AddBoundaryCondition(bc);
        }
    }

    // Properties
    if (auto* propsEl = root->FirstChildElement("properties"))
    {
        for (auto* pEl = propsEl->FirstChildElement("property"); pEl;
             pEl = pEl->NextSiblingElement("property"))
        {
            const char* key = pEl->Attribute("key");
            const char* txt = pEl->GetText();
            if (key && txt) parsedJob.SetProperty(key, txt);
        }
    }

    const auto validation = parsedJob.Validate();
    if (!validation.empty())
    {
        r.diagnostic = "Validation failed: " + validation;
        return r;
    }

    outJob = std::move(parsedJob);
    r.ok = true;
    return r;
}
