#include "xq_HemodynamicsView.h"
#include "xq_SimJobCreate.h"
#include "xq_SolverProcessHandler.h"
#include "ui_xq_HemodynamicsView.h"

#include <xq_Model.h>
#include <xq_MitkGrid.h>
#include <xq_Grid.h>
#include <xq_MitkSolverJob.h>
#include <xq_FlowSolverRegistry.h>
#include <xq_SimulationPrepPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>
#include <mitkDataNodeSelection.h>

#include <mitkLookupTable.h>
#include <mitkLookupTableProperty.h>

#include <berryISelectionService.h>

#include <vtkPolyData.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkLookupTable.h>
#include <vtkSmartPointer.h>

#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>

#include <cmath>

const QString xq_HemodynamicsView::VIEW_ID = "org.xq.views.simulation";

namespace
{
struct SolverSettings
{
  QString solverPath;
  QString mpiPath = "mpiexec";
  int numProcs = 1;
};

SolverSettings ReadSolverSettings(int defaultNumProcs)
{
  SolverSettings settings;
  settings.numProcs = defaultNumProcs;

  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  if (!prefService)
    return settings;

  mitk::IPreferences* prefs =
    prefService->GetSystemPreferences()->Node("/org.xq.views.simulation");
  mitk::IPreferences* legacyPrefs =
    prefService->GetSystemPreferences()->Node("/org.xq.preferences");

  settings.solverPath = QString::fromStdString(
    prefs->Get("solverPath", legacyPrefs ? legacyPrefs->Get("sim.solverPath", "") : ""));
  settings.mpiPath = QString::fromStdString(
    prefs->Get("mpiPath", legacyPrefs ? legacyPrefs->Get("sim.mpiPath", "mpiexec") : "mpiexec"));
  settings.numProcs = prefs->GetInt(
    "numProcessors",
    legacyPrefs ? legacyPrefs->GetInt("sim.numProcessors", defaultNumProcs)
                : defaultNumProcs);

  return settings;
}

bool IsExecutableCommand(const QString& path)
{
  if (path.trimmed().isEmpty())
    return false;

  const QFileInfo info(path);
  if (info.isAbsolute() || path.contains('/'))
    return info.exists() && info.isFile() && info.isExecutable();

  return !QStandardPaths::findExecutable(path).isEmpty();
}

QString JoinFiles(const std::vector<std::string>& files)
{
  QStringList list;
  for (const auto& file : files)
    list << QString::fromStdString(file);
  return list.join('\n');
}

QString EncodeMetadataField(const QString& value)
{
  return QString::fromUtf8(
    value.toUtf8().toPercentEncoding(QByteArray(), QByteArray("|;")));
}

QString DecodeMetadataField(const QString& value)
{
  return QString::fromUtf8(QByteArray::fromPercentEncoding(value.toUtf8()));
}

struct ResultFieldChoice
{
  QString label;
  QString token;
  QString arrayName;
  bool isPointData = true;

  bool operator==(const ResultFieldChoice& other) const
  {
    return token == other.token;
  }
};

QList<ResultFieldChoice> ResultFieldChoicesFromMetadata(const mitk::DataNode* node)
{
  QList<ResultFieldChoice> fields;
  if (!node)
    return fields;

  std::string csv;
  if (!node->GetStringProperty("xq.result.field_names", csv) || csv.empty())
  {
    std::string singleField;
    if (node->GetStringProperty("xq.result.field_name", singleField) && !singleField.empty())
    {
      const QString token = QString::fromStdString(singleField);
      ResultFieldChoice choice;
      choice.token = token;
      choice.arrayName = token;
      choice.label = token;
      fields << choice;
    }
    return fields;
  }

  const auto serialized = QString::fromStdString(csv).split(',', Qt::SkipEmptyParts);
  for (const auto& field : serialized)
  {
    const QString token = field.trimmed();
    if (token.isEmpty())
      continue;

    ResultFieldChoice choice;
    choice.token = token;
    choice.isPointData = !token.startsWith("cell:");
    choice.arrayName = token;
    if (choice.arrayName.startsWith("point:"))
      choice.arrayName = choice.arrayName.mid(6);
    else if (choice.arrayName.startsWith("cell:"))
      choice.arrayName = choice.arrayName.mid(5);

    choice.label = choice.arrayName;
    if (token.startsWith("point:"))
      choice.label += " [point]";
    else if (token.startsWith("cell:"))
      choice.label += " [cell]";
    else
      choice.label += " [data]";

    if (!fields.contains(choice))
      fields << choice;
  }
  return fields;
}

QString ResultFieldUnitsFromToken(const mitk::DataNode* node, const QString& token)
{
  if (!node)
    return QString();

  const QString lower = token.toLower();
  std::string units;
  if (lower.contains("pressure") &&
      node->GetStringProperty("xq.result.units.pressure", units) &&
      !units.empty())
    return QString::fromStdString(units);
  if ((lower.contains("velocity") || lower.contains("flow")) &&
      node->GetStringProperty("xq.result.units.velocity", units) &&
      !units.empty())
    return QString::fromStdString(units);
  if ((lower.contains("shear") || lower.contains("wss")) &&
      node->GetStringProperty("xq.result.units.wall_shear", units) &&
      !units.empty())
    return QString::fromStdString(units);
  if (node->GetStringProperty("xq.result.units", units) && !units.empty())
    return QString::fromStdString(units);
  return QString();
}

QString ResultFieldLocationFromToken(const QString& token)
{
  if (token.startsWith("point:"))
    return QStringLiteral("point data");
  if (token.startsWith("cell:"))
    return QStringLiteral("cell data");
  return QStringLiteral("data");
}

QString ResultFieldArrayNameFromToken(const QString& token)
{
  if (token.startsWith("point:"))
    return token.mid(6);
  if (token.startsWith("cell:"))
    return token.mid(5);
  return token;
}

void PopulateResultFieldCombo(QComboBox* combo, const mitk::DataNode* node)
{
  if (!combo)
    return;

  combo->blockSignals(true);
  combo->clear();

  const auto choices = ResultFieldChoicesFromMetadata(node);
  for (const auto& choice : choices)
    combo->addItem(choice.label, choice.token);

  QString activeScalar;
  if (node)
  {
    std::string activeScalarStd;
    if (node->GetStringProperty("xq.result.active_scalar", activeScalarStd) &&
        !activeScalarStd.empty())
      activeScalar = QString::fromStdString(activeScalarStd);
  }

  int selectedIndex = -1;
  if (!activeScalar.isEmpty())
  {
    selectedIndex = combo->findData(activeScalar);
    if (selectedIndex < 0)
    {
      const QString activeArrayName = ResultFieldArrayNameFromToken(activeScalar);
      for (int i = 0; i < combo->count(); ++i)
      {
        const QString token = combo->itemData(i).toString();
        if (ResultFieldArrayNameFromToken(token) == activeArrayName)
        {
          selectedIndex = i;
          break;
        }
      }
    }
  }

  if (selectedIndex < 0 && combo->count() > 0)
    selectedIndex = 0;
  if (selectedIndex >= 0)
    combo->setCurrentIndex(selectedIndex);

  combo->blockSignals(false);
}

bool ResolveResultArray(vtkDataSet* dataSet, const QString& token, vtkDataArray*& outArray,
                        bool& usePointData)
{
  outArray = nullptr;
  if (!dataSet || token.isEmpty())
    return false;

  const QString arrayName = ResultFieldArrayNameFromToken(token);
  const bool explicitPoint = token.startsWith("point:");
  const bool explicitCell = token.startsWith("cell:");

  if (explicitPoint)
  {
    outArray = dataSet->GetPointData()->GetArray(arrayName.toStdString().c_str());
    usePointData = true;
    return outArray != nullptr;
  }

  if (explicitCell)
  {
    outArray = dataSet->GetCellData()->GetArray(arrayName.toStdString().c_str());
    usePointData = false;
    return outArray != nullptr;
  }

  outArray = dataSet->GetPointData()->GetArray(arrayName.toStdString().c_str());
  usePointData = true;
  if (outArray)
    return true;

  outArray = dataSet->GetCellData()->GetArray(arrayName.toStdString().c_str());
  usePointData = false;
  return outArray != nullptr;
}

QString ResultFieldSummaryText(const mitk::DataNode* node, const QString& token,
                               const QString& fieldLabel,
                               const QString& colorMap, const double range[2],
                               double mean)
{
  QStringList lines;
  lines << QString("Field: %1").arg(fieldLabel);
  lines << QString("Array: %1").arg(ResultFieldArrayNameFromToken(token));
  lines << QString("Location: %1").arg(ResultFieldLocationFromToken(token));

  std::string sourceSimulation;
  if (node && node->GetStringProperty("xq.result.source_simulation", sourceSimulation) &&
      !sourceSimulation.empty())
  {
    lines << QString("Source simulation: %1").arg(QString::fromStdString(sourceSimulation));
  }
  else if (node && node->GetStringProperty("xq.result.source_simulation_job", sourceSimulation) &&
           !sourceSimulation.empty())
  {
    lines << QString("Source job: %1").arg(QString::fromStdString(sourceSimulation));
  }

  int timeStepIndex = -1;
  int timeStepCount = -1;
  double timeValue = 0.0;
  const bool hasStepIndex = node && node->GetIntProperty("xq.result.time_step_index", timeStepIndex);
  const bool hasStepCount = node && node->GetIntProperty("xq.result.time_step_count", timeStepCount);
  const bool hasTimeValue = node && node->GetDoubleProperty("xq.result.time_value", timeValue);
  if (hasStepIndex || hasStepCount || hasTimeValue)
  {
    QString stepText;
    if (hasStepIndex && hasStepCount && timeStepIndex >= 0 && timeStepCount > 0)
      stepText = QString("%1 / %2").arg(timeStepIndex + 1).arg(timeStepCount);
    else if (hasStepIndex && timeStepIndex >= 0)
      stepText = QString::number(timeStepIndex);
    if (hasTimeValue)
    {
      if (!stepText.isEmpty())
        stepText += QString(" (t=%1)").arg(timeValue, 0, 'f', 4);
      else
        stepText = QString("t=%1").arg(timeValue, 0, 'f', 4);
    }
    if (!stepText.isEmpty())
      lines << QString("Time step: %1").arg(stepText);
  }

  const QString units = ResultFieldUnitsFromToken(node, token);
  if (!units.isEmpty())
    lines << QString("Units: %1").arg(units);

  lines << QString("Range: [%1, %2]").arg(range[0], 0, 'f', 4).arg(range[1], 0, 'f', 4);
  lines << QString("Mean: %1").arg(mean, 0, 'f', 4);
  lines << QString("Color map: %1").arg(colorMap);
  return lines.join('\n');
}

QString DiagnosticsToText(const std::vector<xq::pipeline::Diagnostic>& diagnostics)
{
  QString text;
  for (const auto& diagnostic : diagnostics)
  {
    if (!text.isEmpty())
      text += "\n";
    text += "- " + QString::fromStdString(diagnostic.message);
  }
  return text;
}

vtkDataSet* ResultDataSetFromNode(mitk::BaseData* data)
{
  if (!data)
    return nullptr;

  if (auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(data))
  {
    auto* grid = mitkGrid->GetMesh(0);
    return grid ? grid->GetVolumeMesh() : nullptr;
  }

  if (auto* surface = dynamic_cast<mitk::Surface*>(data))
    return surface->GetVtkPolyData();

  return dynamic_cast<vtkDataSet*>(data);
}

QString SafeCaseName(const mitk::DataNode* node)
{
  QString name = node ? QString::fromStdString(node->GetName()) : QStringLiteral("simulation");
  if (name.trimmed().isEmpty())
    name = QStringLiteral("simulation");

  for (auto& ch : name)
  {
    if (!ch.isLetterOrNumber() && ch != '_' && ch != '-')
      ch = '_';
  }
  return name;
}

QString DefaultCaseDir(const mitk::DataNode* node)
{
  if (node)
  {
    std::string existing;
    if (node->GetStringProperty("xq.solver.case_dir", existing) && !existing.empty())
      return QString::fromStdString(existing);
    if (node->GetStringProperty("xq.sim.export_dir", existing) && !existing.empty())
      return QString::fromStdString(existing);
  }

  QString root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  if (root.isEmpty())
    root = QDir::tempPath();
  return QDir(root).filePath("xq_flow_cases/" + SafeCaseName(node));
}

bool BackendCanRunAndImport(const mitk::DataNode* simPrepNode)
{
  if (!simPrepNode)
    return false;

  auto* mitkJob = dynamic_cast<xq_MitkSolverJob*>(simPrepNode->GetData());
  auto* job = mitkJob ? mitkJob->GetSimJob(0) : nullptr;
  if (!job)
    return false;

  auto* backend = xq_FlowSolverRegistry::Instance().FindBackend(job->GetSolverType());
  return backend && backend->GetCapabilities().supports_result_import;
}

QString SolverJobDiagnostic(const mitk::DataNode* simPrepNode)
{
  if (!simPrepNode)
    return QStringLiteral("Missing simulation job");

  auto* mitkJob = dynamic_cast<xq_MitkSolverJob*>(simPrepNode->GetData());
  auto* job = mitkJob ? mitkJob->GetSimJob(0) : nullptr;
  if (!job)
    return QStringLiteral("Invalid simulation: missing solver job");

  const auto diagnostic = job->Validate();
  if (!diagnostic.empty())
    return QStringLiteral("Invalid simulation: %1").arg(QString::fromStdString(diagnostic));

  return QString();
}
}

xq_HemodynamicsView::xq_HemodynamicsView()
  : m_Ui(nullptr)
  , m_ContextLabel(nullptr)
  , m_CurrentMeshNode(nullptr)
  , m_CurrentResultNode(nullptr)
  , m_SimPrepNode(nullptr)
  , m_SolverHandler(nullptr)
  , m_StopRequested(false)
  , m_LegendVisible(false)
  , m_RestoringSimulationMetadata(false)
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

  m_ContextLabel = new QLabel(parent);
  m_ContextLabel->setWordWrap(true);
  m_ContextLabel->setStyleSheet(
    "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
    "border-radius: 4px; padding: 6px; }");
  m_Ui->mainLayout->insertWidget(0, m_ContextLabel);

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
  connect(m_Ui->tableBCs, &QTableWidget::itemChanged,
          this, [this]() { PersistSimulationParametersToMetadata(); });
  connect(m_Ui->tableBCs, &QTableWidget::currentCellChanged,
          this, [this](int, int, int, int) { PersistSimulationParametersToMetadata(); });

  // Solver preset
  connect(m_Ui->btnApplyPreset, &QPushButton::clicked,
          this, &xq_HemodynamicsView::ApplySolverPreset);

  // Wall type toggle
  connect(m_Ui->comboWallType, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_HemodynamicsView::OnWallTypeChanged);
  m_Ui->grpDeformable->setEnabled(m_Ui->comboWallType->currentIndex() == 1);

  auto persistParameters = [this]() { PersistSimulationParametersToMetadata(); };
  connect(m_Ui->spinStartTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinEndTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinTimeStepSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinNumTimeSteps, QOverload<int>::of(&QSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinWallThickness, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinElasticModulus, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->spinPoissonRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->comboSolverPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, persistParameters);
  connect(m_Ui->spinResidualTol, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->comboStepConstruction, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, persistParameters);
  connect(m_Ui->comboPressureCoupling, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, persistParameters);
  connect(m_Ui->spinMaxIterations, QOverload<int>::of(&QSpinBox::valueChanged),
          this, persistParameters);
  connect(m_Ui->chkStabilization, &QCheckBox::toggled,
          this, persistParameters);
  connect(m_Ui->spinNumProcs, QOverload<int>::of(&QSpinBox::valueChanged),
          this, persistParameters);

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
  UpdateContextStatus("Missing mesh");
  BindCurrentDataManagerSelection();
}

void xq_HemodynamicsView::SetFocus()
{
  if (m_Ui && m_Ui->comboMeshSelector)
    m_Ui->comboMeshSelector->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_HemodynamicsView::BindCurrentDataManagerSelection()
{
  if (!GetSite() || GetSite()->GetWorkbenchWindow().IsNull())
    return;

  auto* selectionService = GetSite()->GetWorkbenchWindow()->GetSelectionService();
  if (!selectionService)
    return;

  berry::ISelection::ConstPointer selection =
      selectionService->GetSelection("org.xq.views.datamanager");
  mitk::DataNodeSelection::ConstPointer nodeSelection =
      selection.Cast<const mitk::DataNodeSelection>();
  if (nodeSelection.IsNull())
  {
    selection = selectionService->GetSelection();
    nodeSelection = selection.Cast<const mitk::DataNodeSelection>();
  }
  if (nodeSelection.IsNull())
    return;

  QList<mitk::DataNode::Pointer> nodes;
  const auto selectedNodes = nodeSelection->GetSelectedDataNodes();
  for (const auto& node : selectedNodes)
  {
    if (node.IsNotNull())
      nodes.push_back(node);
  }

  if (!nodes.empty())
    OnSelectionChanged(berry::IWorkbenchPart::Pointer(), nodes);
}

void xq_HemodynamicsView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer /*source*/,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  mitk::DataNode::Pointer selectedMeshNode = nullptr;
  mitk::DataNode::Pointer selectedJobNode = nullptr;
  mitk::DataNode::Pointer selectedResultNode = nullptr;

  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      if (xq::pipeline::HasStage(node, xq::pipeline::Stage::Result) &&
          selectedResultNode.IsNull())
      {
        selectedResultNode = node;
        continue;
      }

      auto* job = dynamic_cast<xq_MitkSolverJob*>(node->GetData());
      if (job != nullptr && selectedJobNode.IsNull())
      {
        selectedJobNode = node;
        continue;
      }

      auto* grid = dynamic_cast<xq_MitkGrid*>(node->GetData());
      if (grid != nullptr && selectedMeshNode.IsNull())
      {
        selectedMeshNode = node;
        continue;
      }

    }
  }

  if (selectedJobNode.IsNotNull())
  {
    m_SimPrepNode = selectedJobNode;
    m_CurrentMeshNode = selectedMeshNode.IsNotNull()
      ? selectedMeshNode
      : ResolveMeshForSimulationJob(selectedJobNode.GetPointer());
    m_CurrentResultNode = selectedResultNode;
  }
  else if (selectedResultNode.IsNotNull())
  {
    m_CurrentResultNode = selectedResultNode;
    m_SimPrepNode = xq::pipeline::ResolveUpstreamNode(
      GetDataStorage(),
      selectedResultNode.GetPointer(),
      xq::pipeline::kSourceSimulationJobProperty,
      xq::pipeline::Stage::SimulationPrep);
    m_CurrentMeshNode = m_SimPrepNode.IsNotNull()
      ? ResolveMeshForSimulationJob(m_SimPrepNode.GetPointer())
      : nullptr;
    if (m_CurrentMeshNode.IsNull())
      m_CurrentMeshNode = selectedMeshNode;
  }
  else if (selectedMeshNode.IsNotNull())
  {
    m_CurrentMeshNode = selectedMeshNode;
    m_SimPrepNode = nullptr;
    m_CurrentResultNode = nullptr;
  }
  else
  {
    ClearSimulationState();
    return;
  }

  const bool hasMesh = m_CurrentMeshNode.IsNotNull();
  const bool hasJob = m_SimPrepNode.IsNotNull();
  const bool hasResult = m_CurrentResultNode.IsNotNull();
  EnableSimulationControls(hasMesh, hasJob, hasResult);
  UpdateContextStatus();
  RestoreSimulationParametersFromMetadata();
  if (m_Ui && m_Ui->comboResultField && m_CurrentResultNode.IsNotNull())
    PopulateResultFieldCombo(m_Ui->comboResultField, m_CurrentResultNode.GetPointer());

  if (hasMesh && m_Ui && m_Ui->comboMeshSelector)
  {
    const QString meshName = QString::fromStdString(m_CurrentMeshNode->GetName());
    m_Ui->comboMeshSelector->clear();
    m_Ui->comboMeshSelector->addItem(meshName);
    m_Ui->comboMeshSelector->setCurrentText(meshName);
  }
}

void xq_HemodynamicsView::ClearSimulationState()
{
  m_CurrentMeshNode = nullptr;
  m_CurrentResultNode = nullptr;
  m_SimPrepNode = nullptr;
  if (m_Ui && m_Ui->comboMeshSelector)
    m_Ui->comboMeshSelector->clear();
  if (m_Ui && m_Ui->comboResultField)
    m_Ui->comboResultField->clear();
  EnableSimulationControls(false, false, false);
  UpdateContextStatus("Missing mesh");
}

void xq_HemodynamicsView::EnableSimulationControls(bool hasMesh, bool hasJob, bool hasResult)
{
  if (!m_Ui)
    return;

  const bool solverRunning = m_SolverHandler && !m_StopRequested && m_Ui->btnStopSim->isEnabled();
  const bool canExport = hasJob && IsSimulationExportReady();
  const bool canRun = hasJob && IsSimulationRunReady();
  m_Ui->btnCreateJob->setEnabled(hasMesh);
  m_Ui->btnSaveJob->setEnabled(hasMesh);
  m_Ui->btnRunSim->setEnabled(canRun);
  m_Ui->btnExportResults->setEnabled(hasResult);
  m_Ui->btnExportOnly->setEnabled(canExport);
  m_Ui->btnExportAndRun->setEnabled(canRun);
  if (!solverRunning)
    m_Ui->btnStopSim->setEnabled(false);
}

bool xq_HemodynamicsView::IsSimulationExportReady(QString* reason)
{
  auto setReason = [reason](const QString& text) {
    if (reason)
      *reason = text;
    return false;
  };

  if (m_CurrentMeshNode.IsNull())
    return setReason(QStringLiteral("Missing mesh"));
  if (m_SimPrepNode.IsNull())
    return setReason(QStringLiteral("Missing simulation job"));

  if (auto jobDiagnostic = SolverJobDiagnostic(m_SimPrepNode.GetPointer());
      !jobDiagnostic.isEmpty())
  {
    return setReason(jobDiagnostic);
  }

  auto* mitkJob = dynamic_cast<xq_MitkSolverJob*>(m_SimPrepNode->GetData());
  auto* job = mitkJob ? mitkJob->GetSimJob(0) : nullptr;
  auto* backend = job
    ? xq_FlowSolverRegistry::Instance().FindBackend(job->GetSolverType())
    : nullptr;
  if (!backend)
  {
    const QString solverType = job
      ? QString::fromStdString(job->GetSolverType())
      : QStringLiteral("<unknown>");
    return setReason(QStringLiteral("Unsupported solver backend: %1").arg(solverType));
  }

  if (reason)
    *reason = QStringLiteral("Ready to export");
  return true;
}

bool xq_HemodynamicsView::IsSimulationRunReady(QString* reason)
{
  QString exportReason;
  if (!IsSimulationExportReady(&exportReason))
    return reason ? (*reason = exportReason, false) : false;

  if (!BackendCanRunAndImport(m_SimPrepNode.GetPointer()))
  {
    if (reason)
      *reason = QStringLiteral("Selected backend cannot run and import results in this build");
    return false;
  }

  if (reason)
    *reason = QStringLiteral("Ready to run");
  return true;
}

QString xq_HemodynamicsView::BuildSimulationReadinessStatus(bool requireRunnable)
{
  QString reason;
  const bool ready = requireRunnable
    ? IsSimulationRunReady(&reason)
    : IsSimulationExportReady(&reason);
  if (!ready)
    return reason;
  return reason.isEmpty() ? QStringLiteral("Ready") : reason;
}

void xq_HemodynamicsView::UpdateContextStatus(const QString& status)
{
  if (!m_ContextLabel)
    return;

  const QString meshName = m_CurrentMeshNode.IsNotNull()
    ? QString::fromStdString(m_CurrentMeshNode->GetName())
    : QStringLiteral("<none>");
  const QString jobName = m_SimPrepNode.IsNotNull()
    ? QString::fromStdString(m_SimPrepNode->GetName())
    : QStringLiteral("<none>");
  const QString resultName = m_CurrentResultNode.IsNotNull()
    ? QString::fromStdString(m_CurrentResultNode->GetName())
    : QStringLiteral("<none>");
  const int bcCount = (m_Ui && m_Ui->tableBCs) ? m_Ui->tableBCs->rowCount() : 0;
  const QString readiness = BuildSimulationReadinessStatus(true);
  const QString resolvedStatus = status.isEmpty() ? readiness : status;
  m_ContextLabel->setText(
    QString("Input mesh: %1\nSimulation job: %2\nResult node: %3\nBoundary conditions: %4\nStatus: %5\nRun readiness: %6\nNext: configure BCs/solver settings before export or run.")
      .arg(meshName, jobName, resultName)
      .arg(bcCount)
      .arg(resolvedStatus, readiness));
}

QComboBox* xq_HemodynamicsView::CreateBoundaryConditionTypeCombo(
  const QString& currentText)
{
  auto* typeCombo = new QComboBox(m_Ui->tableBCs);
  typeCombo->addItems(QStringList()
    << "Prescribed Velocities" << "Resistance" << "RCR"
    << "Impedance" << "Coronary" << "No-slip");
  if (!currentText.isEmpty())
    typeCombo->setCurrentText(currentText);
  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() { PersistSimulationParametersToMetadata(); });
  return typeCombo;
}

QString xq_HemodynamicsView::SerializeBoundaryConditionRows() const
{
  if (!m_Ui || !m_Ui->tableBCs)
    return QString();

  QStringList rows;
  const int rowCount = m_Ui->tableBCs->rowCount();
  for (int row = 0; row < rowCount; ++row)
  {
    const QString faceName = m_Ui->tableBCs->item(row, 0)
      ? m_Ui->tableBCs->item(row, 0)->text()
      : QString("face_%1").arg(row);
    const auto* typeCombo =
      qobject_cast<const QComboBox*>(m_Ui->tableBCs->cellWidget(row, 1));
    const QString bcType = typeCombo
      ? typeCombo->currentText()
      : QStringLiteral("No-slip");
    const QString values = m_Ui->tableBCs->item(row, 2)
      ? m_Ui->tableBCs->item(row, 2)->text()
      : QString();
    rows << QString("%1|%2|%3")
              .arg(EncodeMetadataField(faceName),
                   EncodeMetadataField(bcType),
                   EncodeMetadataField(values));
  }
  return rows.join(';');
}

void xq_HemodynamicsView::RestoreBoundaryConditionRows(
  const QString& serializedRows)
{
  if (!m_Ui || serializedRows.isEmpty())
    return;

  m_Ui->tableBCs->setRowCount(0);
  const QStringList rows = serializedRows.split(';', Qt::SkipEmptyParts);
  for (const QString& serializedRow : rows)
  {
    const QStringList fields = serializedRow.split('|');
    if (fields.size() < 3)
      continue;

    const int row = m_Ui->tableBCs->rowCount();
    m_Ui->tableBCs->insertRow(row);
    m_Ui->tableBCs->setItem(
      row, 0, new QTableWidgetItem(DecodeMetadataField(fields[0])));
    m_Ui->tableBCs->setCellWidget(
      row, 1, CreateBoundaryConditionTypeCombo(DecodeMetadataField(fields[1])));
    m_Ui->tableBCs->setItem(
      row, 2, new QTableWidgetItem(DecodeMetadataField(fields[2])));
  }
}

void xq_HemodynamicsView::PersistSimulationParametersToMetadata()
{
  if (!m_Ui || m_SimPrepNode.IsNull() || m_RestoringSimulationMetadata)
    return;

  m_SimPrepNode->SetDoubleProperty("xq.sim.start_time", m_Ui->spinStartTime->value());
  m_SimPrepNode->SetDoubleProperty("xq.sim.end_time", m_Ui->spinEndTime->value());
  m_SimPrepNode->SetDoubleProperty("xq.sim.time_step_size", m_Ui->spinTimeStepSize->value());
  m_SimPrepNode->SetIntProperty("xq.sim.num_timesteps", m_Ui->spinNumTimeSteps->value());
  m_SimPrepNode->SetIntProperty("xq.sim.num_cycles", std::max(1, static_cast<int>(
    std::round((m_Ui->spinEndTime->value() - m_Ui->spinStartTime->value()) /
               std::max(1e-9, m_Ui->spinTimeStepSize->value())))));
  m_SimPrepNode->SetBoolProperty(
    "xq.sim.deformable_wall", m_Ui->comboWallType->currentIndex() == 1);
  m_SimPrepNode->SetDoubleProperty("xq.sim.wall_thickness", m_Ui->spinWallThickness->value());
  m_SimPrepNode->SetDoubleProperty(
    "xq.sim.wall_elastic_modulus", m_Ui->spinElasticModulus->value());
  m_SimPrepNode->SetDoubleProperty(
    "xq.sim.wall_poisson_ratio", m_Ui->spinPoissonRatio->value());
  m_SimPrepNode->SetStringProperty(
    "xq.sim.solver_preset", m_Ui->comboSolverPreset->currentText().toStdString().c_str());
  m_SimPrepNode->SetDoubleProperty("xq.sim.residual_tolerance", m_Ui->spinResidualTol->value());
  m_SimPrepNode->SetStringProperty(
    "xq.sim.step_construction", m_Ui->comboStepConstruction->currentText().toStdString().c_str());
  m_SimPrepNode->SetStringProperty(
    "xq.sim.pressure_coupling", m_Ui->comboPressureCoupling->currentText().toStdString().c_str());
  m_SimPrepNode->SetIntProperty(
    "xq.sim.num_nonlinear_iterations", m_Ui->spinMaxIterations->value());
  m_SimPrepNode->SetBoolProperty("xq.sim.stabilization", m_Ui->chkStabilization->isChecked());
  m_SimPrepNode->SetIntProperty("xq.sim.num_processors", m_Ui->spinNumProcs->value());
  m_SimPrepNode->SetIntProperty("xq.sim.bc_count", m_Ui->tableBCs->rowCount());
  m_SimPrepNode->SetIntProperty("xq.sim.current_bc_index", m_Ui->tableBCs->currentRow());
  m_SimPrepNode->SetStringProperty(
    "xq.sim.bc_table", SerializeBoundaryConditionRows().toStdString().c_str());
  m_SimPrepNode->Modified();
}

void xq_HemodynamicsView::RestoreSimulationParametersFromMetadata()
{
  if (!m_Ui || m_SimPrepNode.IsNull())
    return;

  m_RestoringSimulationMetadata = true;

  if (auto* solverJobNode = dynamic_cast<xq_MitkSolverJob*>(m_SimPrepNode->GetData()))
  {
    auto* job = solverJobNode->GetSimJob(0);
    if (job)
    {
      m_Ui->lblJobName->setText(QString::fromStdString(job->GetJobName()));
      m_Ui->spinNumTimeSteps->setValue(job->GetNumTimesteps());
      m_Ui->spinTimeStepSize->setValue(job->GetTimeStepSize());
      m_Ui->spinMaxIterations->setValue(job->GetNumNonlinearIterations());
      m_Ui->comboWallType->setCurrentIndex(job->GetDeformable() ? 1 : 0);
      m_Ui->grpDeformable->setEnabled(job->GetDeformable());
      m_Ui->spinWallThickness->setValue(job->GetWallThickness());
      m_Ui->spinElasticModulus->setValue(job->GetWallElasticModulus());
      m_Ui->spinPoissonRatio->setValue(job->GetWallPoissonRatio());

      m_Ui->tableBCs->setRowCount(0);
      for (const auto& bc : job->GetBoundaryConditions())
      {
        const int row = m_Ui->tableBCs->rowCount();
        m_Ui->tableBCs->insertRow(row);
        m_Ui->tableBCs->setItem(
          row, 0, new QTableWidgetItem(QString::fromStdString(bc.faceName)));

        QString currentType;
        if (bc.bcType == "prescribed_velocity")
          currentType = "Prescribed Velocities";
        else if (bc.bcType == "resistance")
          currentType = "Resistance";
        else if (bc.bcType == "rcr")
          currentType = "RCR";
        else if (bc.bcType == "impedance")
          currentType = "Impedance";
        else if (bc.bcType == "coronary")
          currentType = "Coronary";
        else
          currentType = "No-slip";
        m_Ui->tableBCs->setCellWidget(row, 1, CreateBoundaryConditionTypeCombo(currentType));

        QStringList values;
        for (const auto& kv : bc.parameters)
          values << QString("%1=%2")
                      .arg(QString::fromStdString(kv.first))
                      .arg(QString::fromStdString(kv.second));
        m_Ui->tableBCs->setItem(row, 2, new QTableWidgetItem(values.join(' ')));
      }
    }
  }

  int numTimesteps = 0;
  if (m_SimPrepNode->GetIntProperty("xq.sim.num_timesteps", numTimesteps))
    m_Ui->spinNumTimeSteps->setValue(numTimesteps);

  double timeStep = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.time_step_size", timeStep) && timeStep > 0.0)
    m_Ui->spinTimeStepSize->setValue(timeStep);

  double startTime = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.start_time", startTime))
    m_Ui->spinStartTime->setValue(startTime);

  double endTime = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.end_time", endTime))
    m_Ui->spinEndTime->setValue(endTime);

  int maxIterations = 0;
  if (m_SimPrepNode->GetIntProperty("xq.sim.num_nonlinear_iterations", maxIterations) &&
      maxIterations > 0)
    m_Ui->spinMaxIterations->setValue(maxIterations);

  bool deformableWall = false;
  if (m_SimPrepNode->GetBoolProperty("xq.sim.deformable_wall", deformableWall))
  {
    m_Ui->comboWallType->setCurrentIndex(deformableWall ? 1 : 0);
    m_Ui->grpDeformable->setEnabled(deformableWall);
  }

  double wallThickness = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.wall_thickness", wallThickness) &&
      wallThickness > 0.0)
    m_Ui->spinWallThickness->setValue(wallThickness);

  double wallElasticModulus = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.wall_elastic_modulus", wallElasticModulus) &&
      wallElasticModulus > 0.0)
    m_Ui->spinElasticModulus->setValue(wallElasticModulus);

  double wallPoissonRatio = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.wall_poisson_ratio", wallPoissonRatio) &&
      wallPoissonRatio > 0.0)
    m_Ui->spinPoissonRatio->setValue(wallPoissonRatio);

  std::string solverPreset;
  if (m_SimPrepNode->GetStringProperty("xq.sim.solver_preset", solverPreset))
    m_Ui->comboSolverPreset->setCurrentText(QString::fromStdString(solverPreset));

  double residualTolerance = 0.0;
  if (m_SimPrepNode->GetDoubleProperty("xq.sim.residual_tolerance", residualTolerance) &&
      residualTolerance > 0.0)
    m_Ui->spinResidualTol->setValue(residualTolerance);

  std::string stepConstruction;
  if (m_SimPrepNode->GetStringProperty("xq.sim.step_construction", stepConstruction))
    m_Ui->comboStepConstruction->setCurrentText(QString::fromStdString(stepConstruction));

  std::string pressureCoupling;
  if (m_SimPrepNode->GetStringProperty("xq.sim.pressure_coupling", pressureCoupling))
    m_Ui->comboPressureCoupling->setCurrentText(QString::fromStdString(pressureCoupling));

  bool stabilization = false;
  if (m_SimPrepNode->GetBoolProperty("xq.sim.stabilization", stabilization))
    m_Ui->chkStabilization->setChecked(stabilization);

  int numProcessors = 0;
  if (m_SimPrepNode->GetIntProperty("xq.sim.num_processors", numProcessors) &&
      numProcessors > 0)
    m_Ui->spinNumProcs->setValue(numProcessors);

  std::string bcTable;
  if (m_SimPrepNode->GetStringProperty("xq.sim.bc_table", bcTable) &&
      !bcTable.empty())
    RestoreBoundaryConditionRows(QString::fromStdString(bcTable));

  int currentBcIndex = -1;
  if (m_SimPrepNode->GetIntProperty("xq.sim.current_bc_index", currentBcIndex) &&
      currentBcIndex >= 0 && currentBcIndex < m_Ui->tableBCs->rowCount())
    m_Ui->tableBCs->setCurrentCell(currentBcIndex, 0);

  m_RestoringSimulationMetadata = false;
}

mitk::DataNode::Pointer xq_HemodynamicsView::ResolveMeshForSimulationJob(
  mitk::DataNode* jobNode) const
{
  auto storage = GetDataStorage();
  if (storage.IsNull() || !jobNode)
    return nullptr;

  auto* job = dynamic_cast<xq_MitkSolverJob*>(jobNode->GetData());
  if (job && !job->GetMeshName().empty())
  {
    auto meshByName = xq::pipeline::FindNodeByNameAndStage(
      storage,
      job->GetMeshName(),
      xq::pipeline::Stage::VolumeMesh);
    if (meshByName.IsNotNull())
      return meshByName;
  }

  auto upstreamMesh = xq::pipeline::ResolveUpstreamNode(
    storage,
    jobNode,
    xq::pipeline::kSourceMeshProperty,
    xq::pipeline::Stage::VolumeMesh);
  return upstreamMesh;
}

void xq_HemodynamicsView::CreateSimJob()
{
  xq_SimJobCreate dialog(GetDataStorage(),
    this->GetSite()->GetWorkbenchWindow()->GetShell()->GetControl());
  if (m_CurrentMeshNode.IsNotNull())
    dialog.SelectMesh(QString::fromStdString(m_CurrentMeshNode->GetName()));

  if (dialog.exec() == QDialog::Accepted)
  {
    QString jobName = dialog.GetJobName();
    if (jobName.isEmpty())
    {
      QMessageBox::warning(nullptr, "Flow Simulation", "Please enter a job name.");
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
    QMessageBox::warning(nullptr, "Flow Simulation",
      "No mesh selected. Cannot save job.");
    return;
  }

  mitk::DataNode::Pointer modelNode = nullptr;
  modelNode = xq::pipeline::ResolveUpstreamNode(
    GetDataStorage(),
    m_CurrentMeshNode.GetPointer(),
    xq::pipeline::kSourceModelProperty,
    xq::pipeline::Stage::Model);

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

  // Use XQ's registered native backend by default. Unsupported cases fail
  // through backend validation rather than silently falling back.
  request.solverType = "xq_simple_flow";
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
    QMessageBox::warning(nullptr, "Flow Simulation",
      "No upstream model node is attached to the selected mesh.");
    return;
  }

  const auto simResult = xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
    GetDataStorage(), modelNode, m_CurrentMeshNode, request);
  if (!simResult.ok || simResult.node.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      "Failed to generate the simulation-prep node.");
    return;
  }

  m_SimPrepNode = simResult.node;
  PersistSimulationParametersToMetadata();
  EnableSimulationControls(
    m_CurrentMeshNode.IsNotNull(),
    m_SimPrepNode.IsNotNull(),
    m_CurrentResultNode.IsNotNull());

  QMessageBox::information(nullptr, "Flow Simulation",
    "Simulation job saved successfully.");
}

void xq_HemodynamicsView::SetSimulationStatus(const char* status)
{
  if (m_SimPrepNode.IsNull() || status == nullptr)
    return;

  xq::pipeline::SetStringProperty(m_SimPrepNode, "xq.sim.status", status);
  if (auto* simJob = dynamic_cast<xq_MitkSolverJob*>(m_SimPrepNode->GetData()))
  {
    simJob->SetStatus(status);
    simJob->Modified();
  }
  m_SimPrepNode->Modified();
}

bool xq_HemodynamicsView::EnsureSolverHandler()
{
  if (m_SolverHandler)
    return true;

  m_SolverHandler = new xq_SolverProcessHandler(this);
  connect(m_SolverHandler, &xq_SolverProcessHandler::outputReceived,
          this, &xq_HemodynamicsView::OnSolverOutput);
  connect(m_SolverHandler, &xq_SolverProcessHandler::solverFinished,
          this, &xq_HemodynamicsView::OnSolverFinished);
  connect(m_SolverHandler, &xq_SolverProcessHandler::solverStarted,
          this, [this]() {
            SetSimulationStatus("running");
          });
  connect(m_SolverHandler, &xq_SolverProcessHandler::solverError,
          this, [this](const QString& err) {
            m_Ui->textLog->append("ERROR: " + err);
            SetSimulationStatus("failed");
            if (m_SimPrepNode.IsNotNull())
              xq::pipeline::SetStringProperty(
                m_SimPrepNode, "xq.sim.last_error", err.toStdString());
            m_Ui->btnRunSim->setEnabled(false);
            m_Ui->btnStopSim->setEnabled(false);
          });
  connect(m_SolverHandler, &xq_SolverProcessHandler::progressUpdate,
          this, [this](int pct) {
            m_Ui->progressBar->setValue(pct);
          });

  return true;
}

bool xq_HemodynamicsView::StartConfiguredSolver(const QString& workDir)
{
  if (m_SimPrepNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      "No simulation-prep node is selected. Please save or select a job first.");
    return false;
  }

  QString caseDir = workDir.trimmed();
  if (caseDir.isEmpty())
    caseDir = DefaultCaseDir(m_SimPrepNode.GetPointer());

  if (caseDir.isEmpty())
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      "No solver case directory is available.");
    return false;
  }

  xq_SimulationRunRequest request;
  request.caseDir = caseDir.toStdString();
  request.numProcessors = m_Ui ? m_Ui->spinNumProcs->value() : 1;
  request.mpiPath = ReadSolverSettings(request.numProcessors).mpiPath.toStdString();

  if (m_Ui)
  {
    m_Ui->textLog->append(
      QString("Running XQ native solver backend in %1").arg(caseDir));
    m_Ui->btnRunSim->setEnabled(false);
    m_Ui->btnExportAndRun->setEnabled(false);
    m_Ui->btnExportResults->setEnabled(false);
    m_Ui->btnStopSim->setEnabled(false);
    m_Ui->progressBar->setValue(5);
  }

  m_CurrentResultNode = nullptr;
  if (m_Ui && m_Ui->comboResultField)
    m_Ui->comboResultField->clear();

  const auto runResult =
    xq_SimulationPrepPipelineService::RunSolverAndImportResults(
      GetDataStorage(), m_SimPrepNode, request);

  if (m_Ui)
  {
    for (const auto& diagnostic : runResult.diagnostics)
      m_Ui->textLog->append(QString::fromStdString(diagnostic.message));
  }

  if (!runResult.ok)
  {
    const QString diagnostics = DiagnosticsToText(runResult.diagnostics);
    QMessageBox::warning(
      nullptr, "Flow Simulation",
      QString("Solver run failed.%1")
        .arg(diagnostics.isEmpty() ? QString() : "\n" + diagnostics));
    if (m_Ui)
      m_Ui->progressBar->setValue(0);
    EnableSimulationControls(
      m_CurrentMeshNode.IsNotNull(),
      m_SimPrepNode.IsNotNull(),
      m_CurrentResultNode.IsNotNull());
    return false;
  }

  if (!runResult.resultNodes.empty())
  {
    m_CurrentResultNode = runResult.resultNodes.front();
    if (m_Ui)
      PopulateResultFieldCombo(m_Ui->comboResultField, m_CurrentResultNode.GetPointer());
  }
  else
  {
    m_CurrentResultNode = nullptr;
    if (m_Ui && m_Ui->comboResultField)
      m_Ui->comboResultField->clear();
  }

  if (m_Ui)
  {
    m_Ui->progressBar->setValue(100);
    m_Ui->textLog->append(
      QString("Solver finished. Imported result nodes: %1")
        .arg(runResult.resultNodes.size()));
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  EnableSimulationControls(
    m_CurrentMeshNode.IsNotNull(),
    m_SimPrepNode.IsNotNull(),
    m_CurrentResultNode.IsNotNull());
  QMessageBox::information(
    nullptr, "Flow Simulation",
    QString("Solver completed and imported %1 result node(s).")
      .arg(runResult.resultNodes.size()));
  return true;
}

void xq_HemodynamicsView::RunSimulation()
{
  QString reason;
  if (!IsSimulationRunReady(&reason))
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      reason.isEmpty()
        ? QStringLiteral("Simulation is not ready to run.")
        : reason);
    return;
  }

  StartConfiguredSolver(QString());
}

void xq_HemodynamicsView::StopSimulation()
{
  if (m_SolverHandler)
  {
    m_StopRequested = true;
    m_SolverHandler->StopSolver();
  }
  SetSimulationStatus("stopped");

  m_Ui->btnRunSim->setEnabled(false);
  m_Ui->btnStopSim->setEnabled(false);
}

void xq_HemodynamicsView::ExportResults()
{
  mitk::DataNode::Pointer resultNode = m_CurrentResultNode;
  if (resultNode.IsNull() &&
      m_CurrentMeshNode.IsNotNull() &&
      xq::pipeline::HasStage(m_CurrentMeshNode, xq::pipeline::Stage::Result))
  {
    resultNode = m_CurrentMeshNode;
  }

  if (resultNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Export Simulation Results",
      "No imported simulation result node is selected.");
    return;
  }

  std::string sourceFile;
  if (!resultNode->GetStringProperty("xq.result.file_path", sourceFile) ||
      sourceFile.empty())
  {
    QMessageBox::warning(nullptr, "Export Simulation Results",
      "The selected result node does not record an original file path.");
    return;
  }

  const QFileInfo sourceInfo(QString::fromStdString(sourceFile));
  if (!sourceInfo.exists())
  {
    QMessageBox::warning(nullptr, "Export Simulation Results",
      "The original result file no longer exists on disk.");
    return;
  }

  QString outputDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Export Directory");
  if (outputDir.isEmpty())
    return;

  const QString targetPath = QDir(outputDir).filePath(sourceInfo.fileName());
  if (QFile::exists(targetPath))
    QFile::remove(targetPath);

  if (!QFile::copy(QString::fromStdString(sourceFile), targetPath))
  {
    QMessageBox::warning(nullptr, "Export Simulation Results",
      QString("Failed to copy result file to %1.").arg(targetPath));
    return;
  }

  if (m_SimPrepNode.IsNotNull())
  {
    xq::pipeline::SetStringProperty(
      m_SimPrepNode, "xq.sim.files_written", targetPath.toStdString());
    m_SimPrepNode->Modified();
  }

  QMessageBox::information(nullptr, "Export Simulation Results",
    QString("Copied result file to:\n%1").arg(targetPath));
}

void xq_HemodynamicsView::ExportOnly()
{
  if (m_CurrentMeshNode.IsNull())
    return;

  // Find simulation prep node: use cached node or an explicit upstream mesh match.
  // Do not fall back to an unrelated first job; exporting the wrong case is
  // worse than asking the user to select/save the intended job.
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
  }

  if (m_SimPrepNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      "No simulation-prep node is linked to the selected mesh. Please select "
      "the intended simulation job or save a new job for this mesh first.");
    return;
  }

  QString reason;
  if (!IsSimulationExportReady(&reason))
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      reason.isEmpty()
        ? QStringLiteral("Simulation is not ready to export.")
        : reason);
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
    QMessageBox::warning(nullptr, "Flow Simulation", errMsg);
    return;
  }

  xq::pipeline::SetStringProperty(m_SimPrepNode, "xq.sim.files_written",
    JoinFiles(exportResult.filesWritten).toStdString());

  QString fileList;
  for (const auto& f : exportResult.filesWritten)
    fileList += QString::fromStdString(f) + "\n";

  QString warnings;
  for (const auto& diag : exportResult.diagnostics)
  {
    if (diag.severity == xq::pipeline::Severity::Warning)
      warnings += "\n- " + QString::fromStdString(diag.message);
  }

  const QString warningBlock = warnings.isEmpty()
    ? QString()
    : QString("\nWarnings:%1\n\nDo not run these files until the warnings "
              "are resolved.").arg(warnings);
  QMessageBox::information(nullptr, "Flow Simulation",
    QString("Export completed%1. Files written (%2):\n%3%4")
      .arg(warnings.isEmpty() ? QString() : QString(" with warnings"))
      .arg(exportResult.filesWritten.size()).arg(fileList).arg(warningBlock));
}

void xq_HemodynamicsView::ExportAndRun()
{
  QString reason;
  if (!IsSimulationRunReady(&reason))
  {
    QMessageBox::warning(nullptr, "Flow Simulation",
      reason.isEmpty()
        ? QStringLiteral("Simulation is not ready to run.")
        : reason);
    return;
  }

  QString outputDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Solver Case Directory", DefaultCaseDir(m_SimPrepNode.GetPointer()));
  if (outputDir.isEmpty())
    return;

  StartConfiguredSolver(outputDir);
}

void xq_HemodynamicsView::OnSolverOutput(const QString& text)
{
  m_Ui->textLog->append(text);
}

void xq_HemodynamicsView::OnSolverFinished(int exitCode)
{
  m_Ui->btnRunSim->setEnabled(false);
  m_Ui->btnStopSim->setEnabled(false);

  if (m_SimPrepNode.IsNotNull())
  {
    m_SimPrepNode->SetIntProperty("xq.sim.exit_code", exitCode);
    xq::pipeline::SetStringProperty(
      m_SimPrepNode, "xq.sim.last_error",
      "Solver process callbacks are disabled in the XQ-native build.");
  }

  if (m_StopRequested)
  {
    SetSimulationStatus("stopped");
    m_StopRequested = false;
    return;
  }

  SetSimulationStatus("solver_unavailable");
  QMessageBox::warning(nullptr, "Flow Simulation",
    "Solver process completion was ignored because XQ-native solver execution "
    "is disabled in this build.");
}

void xq_HemodynamicsView::AddBoundaryCondition()
{
  int row = m_Ui->tableBCs->rowCount();
  m_Ui->tableBCs->insertRow(row);

  // Default BC: inlet with prescribed velocity
  auto* nameItem = new QTableWidgetItem(QString("face_%1").arg(row));
  m_Ui->tableBCs->setItem(row, 0, nameItem);

  // BC type combo
  auto* typeCombo = CreateBoundaryConditionTypeCombo("Prescribed Velocities");
  m_Ui->tableBCs->setCellWidget(row, 1, typeCombo);

  auto* valueItem = new QTableWidgetItem("0.0");
  m_Ui->tableBCs->setItem(row, 2, valueItem);
  PersistSimulationParametersToMetadata();
}

void xq_HemodynamicsView::RemoveBoundaryCondition()
{
  int currentRow = m_Ui->tableBCs->currentRow();
  if (currentRow >= 0)
    m_Ui->tableBCs->removeRow(currentRow);
  PersistSimulationParametersToMetadata();
}

void xq_HemodynamicsView::OnWallTypeChanged(int index)
{
  bool isDeformable = (index == 1);
  m_Ui->grpDeformable->setEnabled(isDeformable);
  PersistSimulationParametersToMetadata();
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
  PersistSimulationParametersToMetadata();
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
      PersistSimulationParametersToMetadata();
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
  {
    m_Ui->tableBCs->setItem(row, 2,
      new QTableWidgetItem(QString::number(spinValue->value(), 'f', 6)));
    PersistSimulationParametersToMetadata();
  }
}

void xq_HemodynamicsView::OnAutoRangeToggled(bool checked)
{
  m_Ui->spinResultMin->setEnabled(!checked);
  m_Ui->spinResultMax->setEnabled(!checked);
}

void xq_HemodynamicsView::ToggleResultLegend()
{
  m_LegendVisible = !m_LegendVisible;

  mitk::DataNode::Pointer resultNode = m_CurrentResultNode;
  if (resultNode.IsNull() &&
      m_CurrentMeshNode.IsNotNull() &&
      xq::pipeline::HasStage(m_CurrentMeshNode, xq::pipeline::Stage::Result))
  {
    resultNode = m_CurrentMeshNode;
  }

  if (resultNode.IsNotNull())
  {
    resultNode->SetBoolProperty("xq.sim.legendVisible", m_LegendVisible);
  }

  m_Ui->textResultSummary->append(
    m_LegendVisible ? "Legend: visible" : "Legend: hidden");
}

void xq_HemodynamicsView::ApplyResultColorMap()
{
  mitk::DataNode::Pointer resultNode = m_CurrentResultNode;
  if (resultNode.IsNull() &&
      m_CurrentMeshNode.IsNotNull() &&
      xq::pipeline::HasStage(m_CurrentMeshNode, xq::pipeline::Stage::Result))
  {
    resultNode = m_CurrentMeshNode;
  }

  if (resultNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Results",
      "No result node selected.");
    return;
  }

  vtkDataSet* dataSet = ResultDataSetFromNode(resultNode->GetData());
  if (!dataSet)
  {
    QMessageBox::warning(nullptr, "Results",
      "Selected result node does not contain VTK data.");
    return;
  }

  const QString fieldToken = m_Ui->comboResultField->currentData().toString().isEmpty()
    ? m_Ui->comboResultField->currentText()
    : m_Ui->comboResultField->currentData().toString();
  const QString fieldLabel = m_Ui->comboResultField->currentText().isEmpty()
    ? fieldToken
    : m_Ui->comboResultField->currentText();
  std::string fieldStd = ResultFieldArrayNameFromToken(fieldToken).toStdString();

  // Check if the data array already exists on points or cells
  vtkDataArray* dataArray = nullptr;
  bool usePointData = true;
  if (!ResolveResultArray(dataSet, fieldToken, dataArray, usePointData))
  {
    dataArray = dataSet->GetPointData()->GetArray(fieldStd.c_str());
    usePointData = true;
    if (!dataArray)
    {
      dataArray = dataSet->GetCellData()->GetArray(fieldStd.c_str());
      usePointData = false;
    }
  }

  if (!dataArray)
  {
    QMessageBox::warning(nullptr, "Results",
      "The selected node does not contain the requested real result field. "
      "Import solver result files first; XQ will not generate synthetic demo "
      "result data.");
    return;
  }

  // Set active scalars
  if (usePointData)
    dataSet->GetPointData()->SetActiveScalars(fieldStd.c_str());
  else
    dataSet->GetCellData()->SetActiveScalars(fieldStd.c_str());

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
  resultNode->SetProperty("LookupTable",
    mitk::LookupTableProperty::New(mitkLut));

  // Enable scalar visibility
  resultNode->SetBoolProperty("scalar visibility", true);

  // Compute mean value for summary
  double sum = 0.0;
  vtkIdType n = dataArray->GetNumberOfTuples();
  for (vtkIdType i = 0; i < n; ++i)
    sum += dataArray->GetComponent(i, 0);
  double mean = (n > 0) ? sum / n : 0.0;

  // Update result summary
  m_CurrentResultNode = resultNode;
  m_Ui->textResultSummary->setPlainText(
    ResultFieldSummaryText(resultNode.GetPointer(), fieldToken, fieldLabel, colorMap, range, mean));

  // Request rendering update
  dataSet->Modified();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
