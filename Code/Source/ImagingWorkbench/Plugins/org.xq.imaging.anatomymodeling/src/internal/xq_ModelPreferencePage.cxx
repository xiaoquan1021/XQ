#include "xq_ModelPreferencePage.h"

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

static const std::string PREF_NODE = "/org.xq.views.modeling";
static const std::string KEY_DEFAULT_MODEL_TYPE = "defaultModelType";
static const std::string KEY_DEFAULT_SAMPLING = "defaultSamplingPoints";
static const std::string KEY_DEFAULT_FILLET_RADIUS = "defaultFilletRadius";
static const std::string KEY_AUTO_UPDATE = "autoUpdateModel";

xq_ModelPreferencePage::xq_ModelPreferencePage()
  : m_Control(nullptr)
  , m_DefaultModelType(nullptr)
  , m_DefaultSamplingPoints(nullptr)
  , m_DefaultFilletRadius(nullptr)
  , m_AutoUpdateModel(nullptr)
{
}

xq_ModelPreferencePage::~xq_ModelPreferencePage()
{
}

void xq_ModelPreferencePage::Init(berry::IWorkbench::Pointer /*workbench*/)
{
  mitk::IPreferencesService* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService)
    m_Preferences = prefService->GetSystemPreferences()->Node(PREF_NODE);
}

void xq_ModelPreferencePage::CreateQtControl(QWidget* parent)
{
  m_Control = new QWidget(parent);
  QFormLayout* formLayout = new QFormLayout(m_Control);

  m_DefaultModelType = new QComboBox(m_Control);
  m_DefaultModelType->addItem("PolyData");
  m_DefaultModelType->addItem("OCCT");
  formLayout->addRow("Default Model Type:", m_DefaultModelType);

  m_DefaultSamplingPoints = new QSpinBox(m_Control);
  m_DefaultSamplingPoints->setRange(10, 1000);
  m_DefaultSamplingPoints->setValue(60);
  formLayout->addRow("Default Sampling Points:", m_DefaultSamplingPoints);

  m_DefaultFilletRadius = new QDoubleSpinBox(m_Control);
  m_DefaultFilletRadius->setRange(0.001, 100.0);
  m_DefaultFilletRadius->setValue(0.5);
  m_DefaultFilletRadius->setDecimals(3);
  m_DefaultFilletRadius->setSingleStep(0.1);
  formLayout->addRow("Default Fillet Radius:", m_DefaultFilletRadius);

  m_AutoUpdateModel = new QCheckBox(m_Control);
  formLayout->addRow("Auto-Update Model:", m_AutoUpdateModel);

  Update();
}

QWidget* xq_ModelPreferencePage::GetQtControl() const
{
  return m_Control;
}

bool xq_ModelPreferencePage::PerformOk()
{
  if (m_Preferences != nullptr)
  {
    m_Preferences->Put(KEY_DEFAULT_MODEL_TYPE,
      m_DefaultModelType->currentText().toStdString());
    m_Preferences->PutInt(KEY_DEFAULT_SAMPLING,
      m_DefaultSamplingPoints->value());
    m_Preferences->PutDouble(KEY_DEFAULT_FILLET_RADIUS,
      m_DefaultFilletRadius->value());
    m_Preferences->PutBool(KEY_AUTO_UPDATE,
      m_AutoUpdateModel->isChecked());
  }
  return true;
}

void xq_ModelPreferencePage::PerformCancel()
{
}

void xq_ModelPreferencePage::Update()
{
  if (m_Preferences == nullptr)
    return;

  std::string modelType = m_Preferences->Get(KEY_DEFAULT_MODEL_TYPE, "PolyData");
  int idx = m_DefaultModelType->findText(QString::fromStdString(modelType));
  if (idx >= 0)
    m_DefaultModelType->setCurrentIndex(idx);

  m_DefaultSamplingPoints->setValue(
    m_Preferences->GetInt(KEY_DEFAULT_SAMPLING, 60));

  m_DefaultFilletRadius->setValue(
    m_Preferences->GetDouble(KEY_DEFAULT_FILLET_RADIUS, 0.5));

  m_AutoUpdateModel->setChecked(
    m_Preferences->GetBool(KEY_AUTO_UPDATE, false));
}
