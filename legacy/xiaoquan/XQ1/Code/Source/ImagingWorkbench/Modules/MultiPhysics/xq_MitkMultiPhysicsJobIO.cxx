#include "xq_MitkMultiPhysicsJobIO.h"
#include "xq_MitkMultiPhysicsJob.h"
#include "xq_MultiPhysicsXmlWriter.h"

#include <fstream>
#include <sstream>

xq_MitkMultiPhysicsJobIO::Result
xq_MitkMultiPhysicsJobIO::Write(const mitk::DataNode* node, const std::string& filePath)
{
    Result r;

    if (!node)
    {
        r.diagnostic = "Node is null.";
        return r;
    }

    auto* mitkJob = dynamic_cast<const xq_MitkMultiPhysicsJob*>(node->GetData());
    if (!mitkJob)
    {
        r.diagnostic = "Node data is not xq_MitkMultiPhysicsJob.";
        return r;
    }

    auto* job = mitkJob->GetJob(0);
    if (!job)
    {
        r.diagnostic = "Job slot 0 is empty.";
        return r;
    }

    auto xmlResult = xq_MultiPhysicsXmlWriter::Write(*job);
    if (!xmlResult.ok)
    {
        r.diagnostic = xmlResult.diagnostic;
        return r;
    }

    std::ofstream ofs(filePath);
    if (!ofs)
    {
        r.diagnostic = "Cannot open file for writing: " + filePath;
        return r;
    }
    ofs << xmlResult.xml;
    r.ok = true;
    return r;
}

xq_MitkMultiPhysicsJobIO::Result
xq_MitkMultiPhysicsJobIO::Read(mitk::DataNode* node, const std::string& filePath)
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

    auto mitkJob = xq_MitkMultiPhysicsJob::New();
    auto job = std::make_unique<xq_MultiPhysicsJob>();
    auto xmlResult = xq_MultiPhysicsXmlWriter::Read(xml, *job);
    if (!xmlResult.ok)
    {
        r.diagnostic = xmlResult.diagnostic;
        return r;
    }
    mitkJob->SetJob(std::move(job), 0);
    node->SetData(mitkJob);
    r.ok = true;
    return r;
}
