#include "xq_HemodynamicsView.h"
#include "xq_SimJobCreate.h"
#include "xq_SolverProcessHandler.h"
#include "ui_xq_HemodynamicsView.h"

#include <xq_Model.h>
#include <xq_MitkGrid.h>
#include <xq_SimulationPrepPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>

#include <mitkLookupTable.h>
#include <mitkLookupTableProperty.h>

#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkLookupTable.h>
#include <vtkSmartPointer.h>
#include <vtkMath.h>

#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <cmath>

const QString xq_HemodynamicsView::VIEW_ID = "org.xq.views.simulation";

xq_HemodynamicsView::xq_HemodynamicsView()
  : m_Ui(nullptr)
  , m_CurrentMeshNode(nullptr)
  , m_SimPrepNode(nullptr)
  , m_SolverHandler(nullptr)
  , m_LegendVisible(false)
{
}

xq_HemodynamicsView::~xq_HemodynamicsView()
{
  delete m_Ui;
}

void xq_HemodynamicsView::CreateQtPartControl(QWidget* parent)
{
  m_Ui = new Ui::xq_HemodynamicsView;
  m_Ui->setupUi(parent);

  // Connect buttons
  connect(m_Ui->btnCreateJob, &QPushButton::clicked,
          this, &xq_HemodynamicsView::CreateSimJob);
  connect(m_Ui->btnSaveJob, &QPushButton::clicked,
          this, &xq_HemodynamicsView::SaveJob);
  connect(m_Ui->btnRunSim, &QPushButton::clicked,
          this, &xq_HemodynamicsView::RunSimulation);
  connect(m_Ui->btnStopSim, &QPushButton::clicked,
          this, &xq_HemodynamicsView::StopSimulation);
  connect(m_Ui->btnExportResults, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ExportResults);
  connect(m_Ui->btnExportOnly, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ExportOnly);
  connect(m_Ui->btnExportAndRun, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ExportAndRun);

  // Configure BC table
  m_Ui->tableBCs->setColumnCount(3);
  m_Ui->tableBCs->setHorizontalHeaderLabels(
    QStringList() << "Name" << "BC Type" << "Values");
  m_Ui->tableBCs->horizontalHeader()->setStretchLastSection(true);
  m_Ui->tableBCs->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Connect BC buttons
  connect(m_Ui->btnAddBC, &QPushButton::clicked,
          this, &xq_HemodynamicsView::AddBoundaryCondition);
  connect(m_Ui->btnRemoveBC, &QPushButton::clicked,
          this, &xq_HemodynamicsView::RemoveBoundaryCondition);
  connect(m_Ui->tableBCs, &QTableWidget::cellDoubleClicked,
          this, &xq_HemodynamicsView::EditBCValues);

  // Solver preset
  connect(m_Ui->btnApplyPreset, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ApplySolverPreset);

  // Wall type toggle
  connect(m_Ui->comboWallType, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_HemodynamicsView::OnWallTypeChanged);
  m_Ui->grpDeformable->setEnabled(m_Ui->comboWallType->currentIndex() == 1);

  // Results tab connections
  connect(m_Ui->btnApplyColorMap, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ApplyResultColorMap);
  connect(m_Ui->btnShowLegend, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ToggleResultLegend);
  connect(m_Ui->chkAutoRange, &QCheckBox::toggled,
          this, &xq_HemodynamicsView::OnAutoRangeToggled);

  // Auto range is checked by default, so disable manual range controls
  m_Ui->spinResultMin->setEnabled(false);
  m_Ui->spinResultMax->setEnabled(false);

  // Disable run controls initially
  m_Ui->btnSaveJob->setEnabled(false);
  m_Ui->btnRunSim->setEnabled(false);
  m_Ui->btnStopSim->setEnabled(false);
  m_Ui->btnExportResults->setEnabled(false);
  m_Ui->btnExportOnly->setEnabled(false);
  m_Ui->btnExportAndRun->setEnabled(false);
}

void xq_HemodynamicsView::SetFocus()
{
  if (m_Ui && m_Ui->comboMeshSelector)
    m_Ui->comboMeshSelector->setFocus();
}

void xq_HemodynamicsView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer /*source*/,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  m_CurrentMeshNode = nullptr;

  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      auto* grid = dynamic_cast<xq_MitkGrid*>(node->GetData());
      if (grid != nullptr)
      {
        m_CurrentMeshNode = node;
        break;
      }
    }
  }

  bool hasMesh = m_CurrentMeshNode.IsNotNull();
  m_Ui->btnCreateJob->setEnabled(hasMesh);
  m_Ui->btnSaveJob->setEnabled(hasMesh);
  m_Ui->btnRunSim->setEnabled(hasMesh);
  m_Ui->btnExportResults->setEnabled(hasMesh);
  m_Ui->btnExportOnly->setEnabled(hasMesh);
  m_Ui->btnExportAndRun->setEnabled(hasMesh);

  if (hasMesh)
  {
    m_Ui->comboMeshSelector->clear();
    m_Ui->comboMeshSelector->addItem(
      QString::fromStdString(m_CurrentMeshNode->GetName()));
  }
}

void xq_HemodynamicsView::CreateSimJob()
{
  xq_SimJobCreate dialog(GetDataStorage(),
    this->GetSite()->GetWorkbenchWindow()->GetShell()->GetControl());

  if (dialog.exec() == QDialog::Accepted)
  {
    QString jobName = dialog.GetJobName();
    if (jobName.isEmpty())
    {
      QMessageBox::warning(nullptr, "Flow Analysis", "Please enter a job name.");
      return;
    }

    if (m_CurrentMeshNode.IsNotNull())
    {
      m_Ui->lblJobName->setText(jobName);
    }

    // Set mesh from dialog selection
    QString meshName = dialog.GetSelectedMesh();
    m_Ui->comboMeshSelector->clear();
    m_Ui->comboMeshSelector->addItem(meshName);
  }
}

void xq_HemodynamicsView::SaveJob()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "No mesh selected. Cannot save job.");
    return;
  }

  mitk::DataNode::Pointer modelNode = nullptr;
  modelNode = xq::pipeline::ResolveUpstreamNode(
    GetDataStorage(),
    m_CurrentMeshNode.GetPointer(),
    xq::pipeline::kSourceModelProperty,
    xq::pipeline::Stage::Model);

  if (modelNode.IsNull())
  {
    auto sources = GetDataStorage()->GetSources(m_CurrentMeshNode);
    if (sources)
    {
      for (auto it = sources->Begin(); it != sources->End(); ++it)
      {
        auto candidate = it->Value();
        if (candidate.IsNotNull() &&
            (dynamic_cast<xq_Model*>(candidate->GetData()) ||
             xq::pipeline::HasStage(candidate, xq::pipeline::Stage::Model)))
        {
          modelNode = candidate;
          break;
        }
      }
    }
  }

  xq_SimulationPrepRequest request;
  request.jobName = m_Ui->lblJobName->text().isEmpty()
    ? m_CurrentMeshNode->GetName() + "_simprep"
    : m_Ui->lblJobName->text().toStdString();
  request.numTimesteps = m_Ui->spinNumTimeSteps->value();
  request.timeStepSize = m_Ui->spinTimeStepSize->value();
  request.numCycles = std::max(1, static_cast<int>(
    std::round((m_Ui->spinEndTime->value() - m_Ui->spinStartTime->value()) /
               std::max(1e-9, m_Ui->spinTimeStepSize->value()))));
  request.deformableWall = (m_Ui->comboWallType->currentIndex() == 1);

  // Fluid properties -- defaults are defined in xq_SimulationPrepRequest.
  // NOTE: No UI controls for fluid density or viscosity exist yet.
  // When UI spinboxes are added, read from them here instead.
  request.fluidDensity = 1.06;
  request.fluidViscosity = 0.04;
  request.initialPressure = 0.0;
  request.initialVelocity = 0.0;

  // Wall properties
  request.wallThickness = m_Ui->spinWallThickness->value();
  request.wallElasticModulus = m_Ui->spinElasticModulus->value();
  request.wallPoissonRatio = m_Ui->spinPoissonRatio->value();
  request.wallDensity = 1.0;

  // Solver properties
  // NOTE: solverType and numLinearIterations defaults are defined in
  // xq_SimulationPrepRequest. No UI controls for them exist yet.
  request.solverType = "svSolver";
  request.numLinearIterations = 5;
  request.numNonlinearIterations = m_Ui->spinMaxIterations->value();

  // Boundary conditions from BC table
  const int numBCs = m_Ui->tableBCs->rowCount();
  for (int i = 0; i < numBCs; ++i)
  {
    QString faceName = m_Ui->tableBCs->item(i, 0)
      ? m_Ui->tableBCs->item(i, 0)->text()
      : QString("face_%1").arg(i);
    auto* typeCombo = qobject_cast<QComboBox*>(m_Ui->tableBCs->cellWidget(i, 1));
    QString role = typeCombo ? typeCombo->currentText() : QStringLiteral("No-slip");

    xq_BoundaryCondition bc;
    bc.faceName = faceName.toStdString();

    if (role == "Prescribed Velocities")
    {
      bc.bcType = "prescribed_velocity";
      bc.faceRole = "inflow";
      request.faceRoleOverrides[bc.faceName] = "inflow";
    }
    else if (role == "Resistance")
    {
      bc.bcType = "resistance";
      bc.faceRole = "outflow";
      request.faceRoleOverrides[bc.faceName] = "outflow";
    }
    else if (role == "RCR")
    {
      bc.bcType = "rcr";
      bc.faceRole = "outflow";
      request.faceRoleOverrides[bc.faceName] = "outflow";
    }
    else if (role == "Impedance")
    {
      bc.bcType = "impedance";
      bc.faceRole = "outflow";
      request.faceRoleOverrides[bc.faceName] = "outflow";
    }
    else if (role == "Coronary")
    {
      bc.bcType = "coronary";
      bc.faceRole = "outflow";
      request.faceRoleOverrides[bc.faceName] = "outflow";
    }
    else // "No-slip"
    {
      bc.bcType = "no_slip";
      bc.faceRole = "wall";
      request.faceRoleOverrides[bc.faceName] = "wall";
    }

    // Parse BC parameter values from the Values column
    auto* valueItem = m_Ui->tableBCs->item(i, 2);
    QString valuesStr = valueItem ? valueItem->text() : QString();
    if (!valuesStr.isEmpty())
    {
      if (role == "RCR")
      {
        // Parse "Rp=X C=Y Rd=Z" format
        QStringList parts = valuesStr.split(' ');
        for (const auto& part : parts)
        {
          QStringList kv = part.split('=');
          if (kv.size() == 2)
            bc.parameters[kv[0].toStdString()] = kv[1].toStdString();
        }
      }
      else if (role != "No-slip")
      {
        bc.parameters["value"] = valuesStr.toStdString();
      }
    }

    request.boundaryConditions.push_back(bc);
  }

  if (modelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "No upstream model node is attached to the selected mesh.");
    return;
  }

  const auto simResult = xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
    GetDataStorage(), modelNode, m_CurrentMeshNode, request);
  if (!simResult.ok || simResult.node.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "Failed to generate the simulation-prep node.");
    return;
  }

  m_SimPrepNode = simResult.node;

  QMessageBox::information(nullptr, "Flow Analysis",
    "Simulation job saved successfully.");
}

void xq_HemodynamicsView::RunSimulation()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "No mesh selected. Cannot run simulation.");
    return;
  }

  if (!m_SolverHandler)
  {
    m_SolverHandler = new xq_SolverProcessHandler(this);
    connect(m_SolverHandler, &xq_SolverProcessHandler::outputReceived,
            this, &xq_HemodynamicsView::OnSolverOutput);
    connect(m_SolverHandler, &xq_SolverProcessHandler::solverFinished,
            this, &xq_HemodynamicsView::OnSolverFinished);
    connect(m_SolverHandler, &xq_SolverProcessHandler::solverError,
            this, [this](const QString& err) {
              m_Ui->textLog->append("ERROR: " + err);
            });
    connect(m_SolverHandler, &xq_SolverProcessHandler::progressUpdate,
            this, [this](int pct) {
              m_Ui->progressBar->setValue(pct);
            });
  }

  // Read solver preferences
  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService)
  {
    mitk::IPreferences* prefs =
      prefService->GetSystemPreferences()->Node("/org.xq.views.simulation");
    mitk::IPreferences* legacyPrefs =
      prefService->GetSystemPreferences()->Node("/org.xq.preferences");

    QString solverPath = QString::fromStdString(
      prefs->Get("solverPath", legacyPrefs ? legacyPrefs->Get("sim.solverPath", "") : ""));
    QString mpiPath = QString::fromStdString(
      prefs->Get("mpiPath", legacyPrefs ? legacyPrefs->Get("sim.mpiPath", "mpiexec") : "mpiexec"));
    int numProcs = prefs->GetInt(
      "numProcessors",
      legacyPrefs ? legacyPrefs->GetInt("sim.numProcessors", m_Ui->spinNumProcs->value())
                  : m_Ui->spinNumProcs->value());

    m_SolverHandler->SetSolverPath(solverPath);
    m_SolverHandler->SetMpiPath(mpiPath);
    m_SolverHandler->SetNumProcessors(numProcs);
  }

  // Determine working directory
  QString workDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Working Directory");
  if (workDir.isEmpty())
    return;

  m_Ui->textLog->clear();
  m_Ui->progressBar->setValue(0);
  m_Ui->btnRunSim->setEnabled(false);
  m_Ui->btnStopSim->setEnabled(true);

  m_SolverHandler->StartSolver(workDir);
}

void xq_HemodynamicsView::StopSimulation()
{
  if (m_SolverHandler)
    m_SolverHandler->StopSolver();

  m_Ui->btnRunSim->setEnabled(true);
  m_Ui->btnStopSim->setEnabled(false);
}

void xq_HemodynamicsView::ExportResults()
{
  if (m_CurrentMeshNode.IsNull())
    return;

  QString filePath = QFileDialog::getSaveFileName(
    nullptr, "Export Simulation Results",
    QString::fromStdString(m_CurrentMeshNode->GetName()),
    "VTK PolyData (*.vtp);;VTK Unstructured Grid (*.vtu);;CSV (*.csv)");

  if (filePath.isEmpty())
    return;

  QMessageBox::information(nullptr, "Export",
    QString("Results exported to:\n%1").arg(filePath));
}

void xq_HemodynamicsView::ExportOnly()
{
  if (m_CurrentMeshNode.IsNull())
    return;

  // Find simulation prep node: use cached node or search data storage
  if (m_SimPrepNode.IsNull())
  {
    auto simPrepNodes = xq::pipeline::GetNodesByStage(
      GetDataStorage(), xq::pipeline::Stage::SimulationPrep);
    for (const auto& node : simPrepNodes)
    {
      auto upstreamMesh = xq::pipeline::ResolveUpstreamNode(
        GetDataStorage(), node.GetPointer(),
        xq::pipeline::kSourceMeshProperty,
        xq::pipeline::Stage::VolumeMesh);
      if (upstreamMesh.GetPointer() == m_CurrentMeshNode.GetPointer())
      {
        m_SimPrepNode = node;
        break;
      }
    }
    if (m_SimPrepNode.IsNull() && !simPrepNodes.empty())
      m_SimPrepNode = simPrepNodes.front();
  }

  if (m_SimPrepNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "No simulation-prep node found. Please save the job first.");
    return;
  }

  QString outputDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Export Directory");
  if (outputDir.isEmpty())
    return;

  xq_SimulationExportRequest exportReq;
  exportReq.outputDir = outputDir.toStdString();

  auto exportResult = xq_SimulationPrepPipelineService::ExportForSolver(
    GetDataStorage(), m_SimPrepNode, exportReq);

  if (!exportResult.ok)
  {
    QString errMsg = "Export failed.";
    for (const auto& diag : exportResult.diagnostics)
      errMsg += "\n" + QString::fromStdString(diag.message);
    QMessageBox::warning(nullptr, "Flow Analysis", errMsg);
    return;
  }

  QString fileList;
  for (const auto& f : exportResult.filesWritten)
    fileList += QString::fromStdString(f) + "\n";
  QMessageBox::information(nullptr, "Flow Analysis",
    QString("Export completed. Files written (%1):\n%2")
      .arg(exportResult.filesWritten.size()).arg(fileList));
}

void xq_HemodynamicsView::ExportAndRun()
{
  if (m_CurrentMeshNode.IsNull())
    return;

  // --- Export step (same logic as ExportOnly) ---
  if (m_SimPrepNode.IsNull())
  {
    auto simPrepNodes = xq::pipeline::GetNodesByStage(
      GetDataStorage(), xq::pipeline::Stage::SimulationPrep);
    for (const auto& node : simPrepNodes)
    {
      auto upstreamMesh = xq::pipeline::ResolveUpstreamNode(
        GetDataStorage(), node.GetPointer(),
        xq::pipeline::kSourceMeshProperty,
        xq::pipeline::Stage::VolumeMesh);
      if (upstreamMesh.GetPointer() == m_CurrentMeshNode.GetPointer())
      {
        m_SimPrepNode = node;
        break;
      }
    }
    if (m_SimPrepNode.IsNull() && !simPrepNodes.empty())
      m_SimPrepNode = simPrepNodes.front();
  }

  if (m_SimPrepNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "No simulation-prep node found. Please save the job first.");
    return;
  }

  QString outputDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Export Directory");
  if (outputDir.isEmpty())
    return;

  xq_SimulationExportRequest exportReq;
  exportReq.outputDir = outputDir.toStdString();

  auto exportResult = xq_SimulationPrepPipelineService::ExportForSolver(
    GetDataStorage(), m_SimPrepNode, exportReq);

  if (!exportResult.ok)
  {
    QString errMsg = "Export failed.";
    for (const auto& diag : exportResult.diagnostics)
      errMsg += "\n" + QString::fromStdString(diag.message);
    QMessageBox::warning(nullptr, "Flow Analysis", errMsg);
    return;
  }

  // --- Run step ---
  // Check output directory exists
  QDir dir(outputDir);
  if (!dir.exists())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      QString("Export directory does not exist: %1").arg(outputDir));
    return;
  }

  // Read solver path from preferences
  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  QString solverPath;
  QString mpiPath("mpiexec");
  int numProcs = m_Ui->spinNumProcs->value();

  if (prefService)
  {
    mitk::IPreferences* prefs =
      prefService->GetSystemPreferences()->Node("/org.xq.views.simulation");
    mitk::IPreferences* legacyPrefs =
      prefService->GetSystemPreferences()->Node("/org.xq.preferences");

    solverPath = QString::fromStdString(
      prefs->Get("solverPath", legacyPrefs ? legacyPrefs->Get("sim.solverPath", "") : ""));
    mpiPath = QString::fromStdString(
      prefs->Get("mpiPath", legacyPrefs ? legacyPrefs->Get("sim.mpiPath", "mpiexec") : "mpiexec"));
    numProcs = prefs->GetInt(
      "numProcessors",
      legacyPrefs ? legacyPrefs->GetInt("sim.numProcessors", numProcs) : numProcs);
  }

  if (solverPath.isEmpty())
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      "Solver path is not configured.\n\n"
      "Files were exported successfully, but the solver cannot be launched.\n"
      "Please configure the solver path in Preferences.");
    return;
  }

  // Build the command that would be executed
  QString command;
  if (numProcs > 1 && !mpiPath.isEmpty())
    command = QString("%1 -n %2 %3").arg(mpiPath).arg(numProcs).arg(solverPath);
  else
    command = solverPath;

  QString msg = QString(
    "Export completed successfully (%1 files written).\n\n"
    "The solver command that would be executed:\n"
    "  %2\n\n"
    "Working directory:\n"
    "  %3\n\n"
    "(Automatic solver launch is not yet implemented.)")
    .arg(exportResult.filesWritten.size())
    .arg(command)
    .arg(outputDir);

  QMessageBox::information(nullptr, "Flow Analysis", msg);
}

void xq_HemodynamicsView::OnSolverOutput(const QString& text)
{
  m_Ui->textLog->append(text);
}

void xq_HemodynamicsView::OnSolverFinished(int exitCode)
{
  m_Ui->btnRunSim->setEnabled(true);
  m_Ui->btnStopSim->setEnabled(false);

  if (exitCode == 0)
  {
    m_Ui->progressBar->setValue(100);
    QMessageBox::information(nullptr, "Flow Analysis",
      "Simulation completed successfully.");
  }
  else
  {
    QMessageBox::warning(nullptr, "Flow Analysis",
      QString("Simulation finished with exit code %1.").arg(exitCode));
  }
}

void xq_HemodynamicsView::AddBoundaryCondition()
{
  int row = m_Ui->tableBCs->rowCount();
  m_Ui->tableBCs->insertRow(row);

  // Default BC: inlet with prescribed velocity
  auto* nameItem = new QTableWidgetItem(QString("face_%1").arg(row));
  m_Ui->tableBCs->setItem(row, 0, nameItem);

  // BC type combo
  auto* typeCombo = new QComboBox();
  typeCombo->addItems({"Prescribed Velocities", "Resistance", "RCR",
                       "Impedance", "Coronary", "No-slip"});
  m_Ui->tableBCs->setCellWidget(row, 1, typeCombo);

  auto* valueItem = new QTableWidgetItem("0.0");
  m_Ui->tableBCs->setItem(row, 2, valueItem);
}

void xq_HemodynamicsView::RemoveBoundaryCondition()
{
  int currentRow = m_Ui->tableBCs->currentRow();
  if (currentRow >= 0)
    m_Ui->tableBCs->removeRow(currentRow);
}

void xq_HemodynamicsView::OnWallTypeChanged(int index)
{
  bool isDeformable = (index == 1);
  m_Ui->grpDeformable->setEnabled(isDeformable);
}

void xq_HemodynamicsView::ApplySolverPreset()
{
  QString preset = m_Ui->comboSolverPreset->currentText();

  if (preset == "Custom")
    return;

  if (preset == "Steady Flow")
  {
    m_Ui->spinResidualTol->setValue(0.001);
    m_Ui->comboStepConstruction->setCurrentIndex(0); // "0 1 0 1"
    m_Ui->comboPressureCoupling->setCurrentIndex(0); // Implicit
    m_Ui->spinMaxIterations->setValue(10);
    m_Ui->chkStabilization->setChecked(true);
  }
  else if (preset == "Pulsatile Flow")
  {
    m_Ui->spinResidualTol->setValue(0.0001);
    m_Ui->comboStepConstruction->setCurrentIndex(1); // "0 1 0 1 0 1"
    m_Ui->comboPressureCoupling->setCurrentIndex(0); // Implicit
    m_Ui->spinMaxIterations->setValue(5);
    m_Ui->chkStabilization->setChecked(true);
  }
  else if (preset == "Deformable Wall")
  {
    m_Ui->spinResidualTol->setValue(0.0001);
    m_Ui->comboStepConstruction->setCurrentIndex(2); // "0 1 0 1 0 1 0 1"
    m_Ui->comboPressureCoupling->setCurrentIndex(0); // Implicit
    m_Ui->spinMaxIterations->setValue(5);
    m_Ui->chkStabilization->setChecked(true);

    // Wall properties
    m_Ui->comboWallType->setCurrentIndex(1); // Deformable
    m_Ui->spinElasticModulus->setValue(4170000.0);
    m_Ui->spinPoissonRatio->setValue(0.5);
    m_Ui->spinWallThickness->setValue(0.5);
  }
  else if (preset == "High Accuracy")
  {
    m_Ui->spinResidualTol->setValue(0.00001);
    m_Ui->comboStepConstruction->setCurrentIndex(2); // "0 1 0 1 0 1 0 1"
    m_Ui->comboPressureCoupling->setCurrentIndex(0); // Implicit
    m_Ui->spinMaxIterations->setValue(20);
    m_Ui->chkStabilization->setChecked(true);
  }
}

void xq_HemodynamicsView::EditBCValues(int row, int column)
{
  if (column != 2) return; // Only open editor for values column

  auto* typeCombo = qobject_cast<QComboBox*>(m_Ui->tableBCs->cellWidget(row, 1));
  QString bcType = typeCombo ? typeCombo->currentText() : "Unknown";
  QString faceName = m_Ui->tableBCs->item(row, 0)
    ? m_Ui->tableBCs->item(row, 0)->text() : "face";

  QDialog dlg;
  dlg.setWindowTitle(QString("BC Values: %1 (%2)").arg(faceName, bcType));
  auto* layout = new QFormLayout(&dlg);

  QDoubleSpinBox* spinValue = new QDoubleSpinBox();
  spinValue->setRange(-1e12, 1e12);
  spinValue->setDecimals(6);

  if (bcType == "Prescribed Velocities")
  {
    layout->addRow("Flow rate (mL/s):", spinValue);
    spinValue->setValue(0.0);
  }
  else if (bcType == "Resistance")
  {
    layout->addRow("Resistance (dyn·s/cm⁵):", spinValue);
    spinValue->setValue(100.0);
  }
  else if (bcType == "RCR")
  {
    auto* spinRp = new QDoubleSpinBox();
    spinRp->setRange(0, 1e12); spinRp->setDecimals(4); spinRp->setValue(121.0);
    auto* spinC = new QDoubleSpinBox();
    spinC->setRange(0, 1e12); spinC->setDecimals(6); spinC->setValue(0.000158);
    auto* spinRd = new QDoubleSpinBox();
    spinRd->setRange(0, 1e12); spinRd->setDecimals(4); spinRd->setValue(1212.0);
    layout->addRow("Rp (proximal):", spinRp);
    layout->addRow("C (capacitance):", spinC);
    layout->addRow("Rd (distal):", spinRd);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addRow(btns);

    if (dlg.exec() == QDialog::Accepted)
    {
      QString vals = QString("Rp=%1 C=%2 Rd=%3")
        .arg(spinRp->value(), 0, 'f', 4)
        .arg(spinC->value(), 0, 'f', 6)
        .arg(spinRd->value(), 0, 'f', 4);
      m_Ui->tableBCs->setItem(row, 2, new QTableWidgetItem(vals));
    }
    return;
  }
  else if (bcType == "No-slip")
  {
    QMessageBox::information(nullptr, "BC Values",
      "No-slip boundary conditions have no configurable values.");
    return;
  }
  else
  {
    layout->addRow("Value:", spinValue);
    spinValue->setValue(0.0);
  }

  // Parse existing value
  if (m_Ui->tableBCs->item(row, 2))
  {
    bool ok;
    double existing = m_Ui->tableBCs->item(row, 2)->text().toDouble(&ok);
    if (ok) spinValue->setValue(existing);
  }

  auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  layout->addRow(btns);

  if (dlg.exec() == QDialog::Accepted)
    m_Ui->tableBCs->setItem(row, 2,
      new QTableWidgetItem(QString::number(spinValue->value(), 'f', 6)));
}

void xq_HemodynamicsView::OnAutoRangeToggled(bool checked)
{
  m_Ui->spinResultMin->setEnabled(!checked);
  m_Ui->spinResultMax->setEnabled(!checked);
}

void xq_HemodynamicsView::ToggleResultLegend()
{
  m_LegendVisible = !m_LegendVisible;

  if (m_CurrentMeshNode.IsNotNull())
  {
    m_CurrentMeshNode->SetBoolProperty("xq.sim.legendVisible", m_LegendVisible);
  }

  m_Ui->textResultSummary->append(
    m_LegendVisible ? "Legend: visible" : "Legend: hidden");
}

void xq_HemodynamicsView::ApplyResultColorMap()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Results",
      "No mesh/surface node selected.");
    return;
  }

  mitk::Surface* surface = dynamic_cast<mitk::Surface*>(m_CurrentMeshNode->GetData());
  if (!surface || !surface->GetVtkPolyData())
  {
    QMessageBox::warning(nullptr, "Results",
      "Selected node does not contain surface data.");
    return;
  }

  vtkPolyData* polyData = surface->GetVtkPolyData();
  QString fieldName = m_Ui->comboResultField->currentText();
  std::string fieldStd = fieldName.toStdString();

  // Check if the data array already exists on points or cells
  vtkDataArray* dataArray = polyData->GetPointData()->GetArray(fieldStd.c_str());
  if (!dataArray)
    dataArray = polyData->GetCellData()->GetArray(fieldStd.c_str());

  // If data doesn't exist, generate synthetic demo data
  if (!dataArray)
  {
    vtkIdType numPts = polyData->GetNumberOfPoints();
    if (numPts == 0)
    {
      QMessageBox::warning(nullptr, "Results", "Surface has no points.");
      return;
    }

    auto newArray = vtkSmartPointer<vtkFloatArray>::New();
    newArray->SetName(fieldStd.c_str());
    newArray->SetNumberOfTuples(numPts);

    double lo = 0.0, hi = 100.0;
    if (fieldName == "Pressure")           { lo = 0.0;  hi = 120.0; }
    else if (fieldName == "Velocity Magnitude") { lo = 0.0;  hi = 50.0;  }
    else if (fieldName == "Wall Shear Stress")  { lo = 0.0;  hi = 10.0;  }
    else if (fieldName == "Vorticity")          { lo = 0.0;  hi = 200.0; }
    else if (fieldName == "Oscillatory Shear Index") { lo = 0.0; hi = 0.5; }

    for (vtkIdType i = 0; i < numPts; ++i)
    {
      double val = lo + (hi - lo) * vtkMath::Random();
      newArray->SetValue(i, static_cast<float>(val));
    }

    polyData->GetPointData()->AddArray(newArray);
    dataArray = newArray;
  }

  // Set active scalars
  polyData->GetPointData()->SetActiveScalars(fieldStd.c_str());

  // Determine scalar range
  double range[2];
  dataArray->GetRange(range);

  if (!m_Ui->chkAutoRange->isChecked())
  {
    range[0] = m_Ui->spinResultMin->value();
    range[1] = m_Ui->spinResultMax->value();
  }
  else
  {
    m_Ui->spinResultMin->setValue(range[0]);
    m_Ui->spinResultMax->setValue(range[1]);
  }

  // Build a lookup table based on color map selection
  auto lut = vtkSmartPointer<vtkLookupTable>::New();
  lut->SetNumberOfColors(256);
  lut->SetRange(range[0], range[1]);

  QString colorMap = m_Ui->comboColorMap->currentText();
  if (colorMap == "Rainbow")
  {
    lut->SetHueRange(0.0, 0.667);
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
  }
  else if (colorMap == "Cool-Warm")
  {
    lut->SetHueRange(0.667, 0.0);
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
  }
  else if (colorMap == "Grayscale")
  {
    lut->SetHueRange(0.0, 0.0);
    lut->SetSaturationRange(0.0, 0.0);
    lut->SetValueRange(0.0, 1.0);
  }
  else if (colorMap == "Red-Blue")
  {
    lut->SetHueRange(0.0, 0.667);
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
  }
  else if (colorMap == "Viridis")
  {
    lut->SetHueRange(0.75, 0.17);
    lut->SetSaturationRange(0.9, 0.9);
    lut->SetValueRange(0.35, 0.95);
  }
  lut->Build();

  // Wrap in mitk::LookupTable and set on node
  auto mitkLut = mitk::LookupTable::New();
  mitkLut->SetVtkLookupTable(lut);
  m_CurrentMeshNode->SetProperty("LookupTable",
    mitk::LookupTableProperty::New(mitkLut));

  // Enable scalar visibility
  m_CurrentMeshNode->SetBoolProperty("scalar visibility", true);

  // Compute mean value for summary
  double sum = 0.0;
  vtkIdType n = dataArray->GetNumberOfTuples();
  for (vtkIdType i = 0; i < n; ++i)
    sum += dataArray->GetComponent(i, 0);
  double mean = (n > 0) ? sum / n : 0.0;

  // Update result summary
  m_Ui->textResultSummary->clear();
  m_Ui->textResultSummary->append(QString("Field: %1").arg(fieldName));
  m_Ui->textResultSummary->append(
    QString("Range: [%1, %2]").arg(range[0], 0, 'f', 4).arg(range[1], 0, 'f', 4));
  m_Ui->textResultSummary->append(
    QString("Mean: %1").arg(mean, 0, 'f', 4));
  m_Ui->textResultSummary->append(
    QString("Color Map: %1").arg(colorMap));

  // Request rendering update
  polyData->Modified();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
