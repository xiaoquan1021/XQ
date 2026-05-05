#include "xq_MeshPreferencePage.h"

#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>

static const std::string PREF_NODE = "/org.xq.views.meshing";
static const std::string KEY_DEFAULT_MESH_TYPE = "defaultMeshType";
static const std::string KEY_DEFAULT_GLOBAL_EDGE = "defaultGlobalEdgeSize";
static const std::string KEY_DEFAULT_BL_LAYERS = "defaultBLLayers";
static const std::string KEY_DEFAULT_BL_THICKNESS = "defaultBLThickness";
static const std::string KEY_DEFAULT_BL_GROWTH = "defaultBLGrowthRate";
static const std::string KEY_AUTO_RUN = "autoRunMeshing";

xq_GridPreferencePage::xq_GridPreferencePage()
  : m_Control(nullptr)
  , m_DefaultMeshType(nullptr)
  , m_DefaultGlobalEdgeSize(nullptr)
  , m_DefaultBLLayers(nullptr)
  , m_DefaultBLThickness(nullptr)
  , m_DefaultBLGrowthRate(nullptr)
  , m_AutoRunMeshing(nullptr)
{
}

xq_GridPreferencePage::~xq_GridPreferencePage()
{
}

void xq_GridPreferencePage::Init(berry::IWorkbench::Pointer /*workbench*/)
{
  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService)
    m_Preferences = prefService->GetSystemPreferences()->Node(PREF_NODE);
}

void xq_GridPreferencePage::CreateQtControl(QWidget* parent)
{
  m_Control = new QWidget(parent);
  QFormLayout* formLayout = new QFormLayout(m_Control);

  m_DefaultMeshType = new QComboBox(m_Control);
  m_DefaultMeshType->addItem("TetGen");
  formLayout->addRow("Default Mesh Type:", m_DefaultMeshType);

  m_DefaultGlobalEdgeSize = new QDoubleSpinBox(m_Control);
  m_DefaultGlobalEdgeSize->setRange(0.001, 1000.0);
  m_DefaultGlobalEdgeSize->setValue(1.0);
  m_DefaultGlobalEdgeSize->setDecimals(3);
  m_DefaultGlobalEdgeSize->setSingleStep(0.1);
  formLayout->addRow("Default Global Edge Size:", m_DefaultGlobalEdgeSize);

  m_DefaultBLLayers = new QSpinBox(m_Control);
  m_DefaultBLLayers->setRange(0, 20);
  m_DefaultBLLayers->setValue(2);
  formLayout->addRow("Default BL Layers:", m_DefaultBLLayers);

  m_DefaultBLThickness = new QDoubleSpinBox(m_Control);
  m_DefaultBLThickness->setRange(0.001, 100.0);
  m_DefaultBLThickness->setValue(0.5);
  m_DefaultBLThickness->setDecimals(3);
  m_DefaultBLThickness->setSingleStep(0.05);
  formLayout->addRow("Default BL Thickness:", m_DefaultBLThickness);

  m_DefaultBLGrowthRate = new QDoubleSpinBox(m_Control);
  m_DefaultBLGrowthRate->setRange(1.0, 5.0);
  m_DefaultBLGrowthRate->setValue(1.2);
  m_DefaultBLGrowthRate->setDecimals(2);
  m_DefaultBLGrowthRate->setSingleStep(0.1);
  formLayout->addRow("Default BL Growth Rate:", m_DefaultBLGrowthRate);

  m_AutoRunMeshing = new QCheckBox(m_Control);
  formLayout->addRow("Auto-Run Meshing:", m_AutoRunMeshing);

  Update();
}

QWidget* xq_GridPreferencePage::GetQtControl() const
{
  return m_Control;
}

bool xq_GridPreferencePage::PerformOk()
{
  if (m_Preferences != nullptr)
  {
    m_Preferences->Put(KEY_DEFAULT_MESH_TYPE,
      m_DefaultMeshType->currentText().toStdString());
    m_Preferences->PutDouble(KEY_DEFAULT_GLOBAL_EDGE,
      m_DefaultGlobalEdgeSize->value());
    m_Preferences->PutInt(KEY_DEFAULT_BL_LAYERS,
      m_DefaultBLLayers->value());
    m_Preferences->PutDouble(KEY_DEFAULT_BL_THICKNESS,
      m_DefaultBLThickness->value());
    m_Preferences->PutDouble(KEY_DEFAULT_BL_GROWTH,
      m_DefaultBLGrowthRate->value());
    m_Preferences->PutBool(KEY_AUTO_RUN,
      m_AutoRunMeshing->isChecked());
  }
  return true;
}

void xq_GridPreferencePage::PerformCancel()
{
}

void xq_GridPreferencePage::Update()
{
  if (m_Preferences == nullptr)
    return;

  std::string meshType = m_Preferences->Get(KEY_DEFAULT_MESH_TYPE, "TetGen");
  int idx = m_DefaultMeshType->findText(QString::fromStdString(meshType));
  if (idx >= 0)
    m_DefaultMeshType->setCurrentIndex(idx);

  m_DefaultGlobalEdgeSize->setValue(
    m_Preferences->GetDouble(KEY_DEFAULT_GLOBAL_EDGE, 1.0));

  m_DefaultBLLayers->setValue(
    m_Preferences->GetInt(KEY_DEFAULT_BL_LAYERS, 2));

  m_DefaultBLThickness->setValue(
    m_Preferences->GetDouble(KEY_DEFAULT_BL_THICKNESS, 0.5));

  m_DefaultBLGrowthRate->setValue(
    m_Preferences->GetDouble(KEY_DEFAULT_BL_GROWTH, 1.2));

  m_AutoRunMeshing->setChecked(
    m_Preferences->GetBool(KEY_AUTO_RUN, false));
}
