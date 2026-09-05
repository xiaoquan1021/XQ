#pragma once

#include <xqModuleROMSimulationExports.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

// 0D/1D reduced-order model (ROM) simulation job.
// Phase 1: 0D Windkessel / lumped-parameter model only.
class XQMODULEROMSIMULATION_EXPORT xq_ROMJob
{
public:
    xq_ROMJob() = default;
    ~xq_ROMJob() = default;

    // --- Job identity ---
    void SetJobName(std::string_view name);
    [[nodiscard]] const std::string& GetJobName() const;

    // --- Model properties ---
    void SetModelType(std::string_view type); // "Windkessel", "Lumped", "1D"
    [[nodiscard]] const std::string& GetModelType() const;

    // --- Inlet / outlet cap properties ---
    void SetCapProp(std::string_view capName, std::string_view key,
                    std::string_view value);
    [[nodiscard]] std::string GetCapProp(std::string_view capName,
                                         std::string_view key) const;
    [[nodiscard]] const std::map<std::string, std::map<std::string, std::string>>&
        GetCapProps() const;
    void SetCapProps(const std::map<std::string, std::map<std::string, std::string>>& caps);

    // Convenience: RCR parameters on a cap.
    void SetRCR(std::string_view capName, double Rp, double C, double Rd);
    [[nodiscard]] bool GetRCR(std::string_view capName,
                              double& Rp, double& C, double& Rd) const;

    // --- Wall properties ---
    void SetWallThickness(double t);
    [[nodiscard]] double GetWallThickness() const;
    void SetWallElasticModulus(double e);
    [[nodiscard]] double GetWallElasticModulus() const;
    void SetWallPoissonRatio(double v);
    [[nodiscard]] double GetWallPoissonRatio() const;

    // --- Solver run properties ---
    void SetTimeStepSize(double dt);
    [[nodiscard]] double GetTimeStepSize() const;
    void SetNumTimeSteps(int n);
    [[nodiscard]] int GetNumTimeSteps() const;
    void SetSolverTolerance(double tol);
    [[nodiscard]] double GetSolverTolerance() const;
    void SetMaxSolverIterations(int n);
    [[nodiscard]] int GetMaxSolverIterations() const;

    // --- Result conversion properties ---
    void SetOutputFormat(std::string_view fmt); // "vtp", "csv"
    [[nodiscard]] const std::string& GetOutputFormat() const;
    void AddOutputField(std::string_view fieldName);
    [[nodiscard]] const std::vector<std::string>& GetOutputFields() const;

    // --- Extensible properties ---
    void SetProperty(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetProperty(std::string_view key) const;
    [[nodiscard]] const std::map<std::string, std::string>& GetProperties() const;
    void SetProperties(const std::map<std::string, std::string>& props);

    // --- Validation ---
    // Returns empty string if the job is valid.
    [[nodiscard]] std::string Validate() const;

private:
    std::string m_JobName;
    std::string m_ModelType = "Windkessel";

    std::map<std::string, std::map<std::string, std::string>> m_CapProps;

    double m_WallThickness = 0.5;
    double m_WallElasticModulus = 4.0e6;
    double m_WallPoissonRatio = 0.45;

    double m_TimeStepSize = 0.001;
    int m_NumTimeSteps = 200;
    double m_SolverTolerance = 1e-6;
    int m_MaxSolverIterations = 100;

    std::string m_OutputFormat = "vtp";
    std::vector<std::string> m_OutputFields;

    std::map<std::string, std::string> m_Properties;
};
