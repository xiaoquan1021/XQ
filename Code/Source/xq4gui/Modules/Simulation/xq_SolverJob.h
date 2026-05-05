#pragma once

#include <xqModuleSimulationExports.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using XqSimPropertyMap = std::map<std::string, std::string>;
using XqSimCapPropertyMap = std::map<std::string, std::map<std::string, std::string>>;

struct XQMODULESIMULATION_EXPORT xq_BoundaryCondition
{
    std::string faceName;
    std::string faceRole; // wall / inflow / outflow
    std::string bcType;   // prescribed_velocity / resistance / rcr / pressure / no_slip
    std::map<std::string, std::string> parameters;
    std::vector<std::pair<double, double>> waveform; // t (s) -> Q (cm^3/s)
};

class XQMODULESIMULATION_EXPORT xq_SolverJob
{
public:
    xq_SolverJob() = default;
    xq_SolverJob(const xq_SolverJob& other) = default;
    xq_SolverJob& operator=(const xq_SolverJob& other) = default;
    xq_SolverJob(xq_SolverJob&&) noexcept = default;
    xq_SolverJob& operator=(xq_SolverJob&&) noexcept = default;
    virtual ~xq_SolverJob() = default;

    [[nodiscard]] std::unique_ptr<xq_SolverJob> Clone() const;

    // --- Basic properties ---
    void SetJobName(std::string_view name);
    [[nodiscard]] const std::string& GetJobName() const;

    void SetNumTimesteps(int n);
    [[nodiscard]] int GetNumTimesteps() const;

    void SetTimeStepSize(double dt);
    [[nodiscard]] double GetTimeStepSize() const;

    void SetNumCycles(int n);
    [[nodiscard]] int GetNumCycles() const;

    // --- Wall properties ---
    void SetDeformable(bool flag);
    [[nodiscard]] bool GetDeformable() const;

    // --- Fluid properties ---
    void SetFluidDensity(double v);
    [[nodiscard]] double GetFluidDensity() const;

    void SetFluidViscosity(double v);
    [[nodiscard]] double GetFluidViscosity() const;

    void SetInitialPressure(double v);
    [[nodiscard]] double GetInitialPressure() const;

    void SetInitialVelocity(double v);
    [[nodiscard]] double GetInitialVelocity() const;

    // --- Wall properties ---
    void SetWallThickness(double t);
    [[nodiscard]] double GetWallThickness() const;

    void SetWallElasticModulus(double e);
    [[nodiscard]] double GetWallElasticModulus() const;

    void SetWallDensity(double d);
    [[nodiscard]] double GetWallDensity() const;

    void SetWallPoissonRatio(double v);
    [[nodiscard]] double GetWallPoissonRatio() const;

    // --- Solver properties ---
    void SetSolverType(std::string_view type);
    [[nodiscard]] const std::string& GetSolverType() const;

    void SetNumLinearIterations(int n);
    [[nodiscard]] int GetNumLinearIterations() const;

    void SetNumNonlinearIterations(int n);
    [[nodiscard]] int GetNumNonlinearIterations() const;

    // --- Generic property map accessors ---
    void SetBasicProp(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetBasicProp(std::string_view key) const;
    [[nodiscard]] const XqSimPropertyMap& GetBasicProps() const;
    void SetBasicProps(const XqSimPropertyMap& props);

    void SetWallProp(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetWallProp(std::string_view key) const;
    [[nodiscard]] const XqSimPropertyMap& GetWallProps() const;
    void SetWallProps(const XqSimPropertyMap& props);

    void SetSolverProp(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetSolverProp(std::string_view key) const;
    [[nodiscard]] const XqSimPropertyMap& GetSolverProps() const;
    void SetSolverProps(const XqSimPropertyMap& props);

    void SetRunProp(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetRunProp(std::string_view key) const;
    [[nodiscard]] const XqSimPropertyMap& GetRunProps() const;
    void SetRunProps(const XqSimPropertyMap& props);

    // --- Cap property accessors (per-face BCs and ICs) ---
    void SetCapProp(std::string_view capName, std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetCapProp(std::string_view capName, std::string_view key) const;
    [[nodiscard]] const XqSimCapPropertyMap& GetCapProps() const;
    void SetCapProps(const XqSimCapPropertyMap& props);

    void SetIcProp(std::string_view capName, std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetIcProp(std::string_view capName, std::string_view key) const;
    [[nodiscard]] const XqSimCapPropertyMap& GetIcProps() const;
    void SetIcProps(const XqSimCapPropertyMap& props);

    // --- Boundary condition list ---
    void SetBoundaryConditions(const std::vector<xq_BoundaryCondition>& bcs);
    [[nodiscard]] const std::vector<xq_BoundaryCondition>& GetBoundaryConditions() const;
    void AddBoundaryCondition(const xq_BoundaryCondition& bc);

    // --- Builder pattern for fluent construction ---
    class XQMODULESIMULATION_EXPORT Builder;

protected:
    std::string m_JobName = "simulation";
    int m_NumTimesteps = 200;
    double m_TimeStepSize = 0.001;
    int m_NumCycles = 2;

    bool m_Deformable = false;
    double m_WallThickness = 0.5;
    double m_WallElasticModulus = 4.0e6;
    double m_WallDensity = 1.0;
    double m_WallPoissonRatio = 0.45;

    double m_FluidDensity = 1.06;
    double m_FluidViscosity = 0.04;
    double m_InitialPressure = 0.0;
    double m_InitialVelocity = 0.0;

    std::string m_SolverType = "svLS";
    int m_NumLinearIterations = 10;
    int m_NumNonlinearIterations = 2;

    XqSimPropertyMap m_BasicProps;
    XqSimPropertyMap m_WallProps;
    XqSimPropertyMap m_SolverProps;
    XqSimPropertyMap m_RunProps;

    XqSimCapPropertyMap m_CapProps;
    XqSimCapPropertyMap m_IcProps;

    std::vector<xq_BoundaryCondition> m_BoundaryConditions;

private:
    [[nodiscard]] static std::string LookupMapValue(const XqSimPropertyMap& m, std::string_view key);
    [[nodiscard]] static std::string LookupCapMapValue(const XqSimCapPropertyMap& m,
                                                       std::string_view capName,
                                                       std::string_view key);
};

class XQMODULESIMULATION_EXPORT xq_SolverJob::Builder
{
public:
    Builder& withName(std::string_view name);
    Builder& withTimesteps(int count, double stepSize);
    Builder& withCycles(int n);
    Builder& withDeformableWall(double thickness, double elasticModulus, double density);
    Builder& withRigidWall();
    Builder& withSolver(std::string_view type, int linearIter, int nonlinearIter);
    Builder& withBasicProp(std::string_view key, std::string_view value);
    Builder& withWallProp(std::string_view key, std::string_view value);
    Builder& withSolverProp(std::string_view key, std::string_view value);
    Builder& withRunProp(std::string_view key, std::string_view value);
    Builder& withCapBoundaryCondition(std::string_view capName, std::string_view key, std::string_view value);
    Builder& withInitialCondition(std::string_view capName, std::string_view key, std::string_view value);
    [[nodiscard]] std::unique_ptr<xq_SolverJob> build() const;

private:
    xq_SolverJob m_Job;
};
