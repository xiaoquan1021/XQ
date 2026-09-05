#pragma once

#include <xqModuleSimulationExports.h>

#include <mitkAbstractFileIO.h>

class XQMODULESIMULATION_EXPORT xq_MitkSolverJobIO : public mitk::AbstractFileIO
{
public:
    xq_MitkSolverJobIO();

    using mitk::AbstractFileReader::Read;
    std::vector<mitk::BaseData::Pointer> DoRead() override;
    [[nodiscard]] mitk::IFileIO::ConfidenceLevel GetReaderConfidenceLevel() const override;

    void Write() override;
    [[nodiscard]] mitk::IFileIO::ConfidenceLevel GetWriterConfidenceLevel() const override;

private:
    xq_MitkSolverJobIO(const xq_MitkSolverJobIO& other);
    xq_MitkSolverJobIO* IOClone() const override;
};
