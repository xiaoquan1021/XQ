#include "xq_SimulationPreferencePage.h"
#include "ui_xq_SimulationPreferencePage.h"

#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <QFileDialog>

static const std::string PREF_NODE = "/org.xq.views.simulation";
static const std::string KEY_SOLVER_PATH = "solverPath";
static const std::string KEY_MPI_PATH = "mpiPath";
static const std::string KEY_NUM_PROCS = "numProcessors";
static const std::string KEY_USE_CUSTOM_MPI = "useCustomMpi";

xq_SimulationPreferencePage::xq_SimulationPreferencePage()
  : m_Ui(nullptr)
  , m_MainControl(nullptr)
{
}

xq_SimulationPreferencePage::~xq_SimulationPreferencePage()
{
  delete m_Ui;
}

void xq_SimulationPreferencePage::Init(berry::IWorkbench::Pointer /*workbench*/)
{
  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService)
    m_Prefs = prefService->GetSystemPreferences()->Node(PREF_NODE);
}

void xq_SimulationPreferencePage::CreateQtControl(QWidget* parent)
{
  m_MainControl = new QWidget(parent);
  m_Ui = new Ui::xq_SimulationPreferencePage;
  m_Ui->setupUi(m_MainControl);

  connect(m_Ui->btnBrowseSolver, &QPushButton::clicked,
          this, &xq_SimulationPreferencePage::BrowseSolverPath);
  connect(m_Ui->btnBrowseMpi, &QPushButton::clicked,
          this, &xq_SimulationPreferencePage::BrowseMpiPath);

  Update();
}

QWidget* xq_SimulationPreferencePage::GetQtControl() const
{
  return m_MainControl;
}

bool xq_SimulationPreferencePage::PerformOk()
{
  if (m_Prefs != nullptr)
  {
    m_Prefs->Put(KEY_SOLVER_PATH,
      m_Ui->editSolverPath->text().toStdString());
    m_Prefs->Put(KEY_MPI_PATH,
      m_Ui->editMpiPath->text().toStdString());
    m_Prefs->PutInt(KEY_NUM_PROCS,
      m_Ui->spinNumProcs->value());
    m_Prefs->PutBool(KEY_USE_CUSTOM_MPI,
      m_Ui->chkUseCustomMpi->isChecked());
  }
  return true;
}

void xq_SimulationPreferencePage::PerformCancel()
{
}

void xq_SimulationPreferencePage::Update()
{
  if (m_Prefs == nullptr || m_Ui == nullptr)
    return;

  m_Ui->editSolverPath->setText(
    QString::fromStdString(m_Prefs->Get(KEY_SOLVER_PATH, "")));
  m_Ui->editMpiPath->setText(
    QString::fromStdString(m_Prefs->Get(KEY_MPI_PATH, "mpiexec")));
  m_Ui->spinNumProcs->setValue(
    m_Prefs->GetInt(KEY_NUM_PROCS, 1));
  m_Ui->chkUseCustomMpi->setChecked(
    m_Prefs->GetBool(KEY_USE_CUSTOM_MPI, false));
}

void xq_SimulationPreferencePage::BrowseSolverPath()
{
  QString filePath = QFileDialog::getOpenFileName(
    m_MainControl, "Select Solver Executable");
  if (!filePath.isEmpty())
    m_Ui->editSolverPath->setText(filePath);
}

void xq_SimulationPreferencePage::BrowseMpiPath()
{
  QString filePath = QFileDialog::getOpenFileName(
    m_MainControl, "Select MPI Executable");
  if (!filePath.isEmpty())
    m_Ui->editMpiPath->setText(filePath);
}
