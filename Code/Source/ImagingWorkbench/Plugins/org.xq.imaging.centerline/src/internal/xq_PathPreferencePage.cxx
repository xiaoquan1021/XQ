#include "xq_PathPreferencePage.h"

#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <QColorDialog>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

xq_PathPreferencePage::xq_PathPreferencePage()
  : m_MainControl(nullptr)
  , m_Point2DSizeSpinBox(nullptr)
  , m_Point3DSizeSpinBox(nullptr)
  , m_PathColorButton(nullptr)
  , m_PathColor(Qt::yellow)
{
}

xq_PathPreferencePage::~xq_PathPreferencePage()
{
}

void xq_PathPreferencePage::Init(berry::IWorkbench::Pointer /*workbench*/)
{
}

void xq_PathPreferencePage::CreateQtControl(QWidget* parent)
{
  m_MainControl = new QWidget(parent);
  auto* layout = new QVBoxLayout(m_MainControl);

  auto* formLayout = new QFormLayout;

  // 2D point size
  m_Point2DSizeSpinBox = new QSpinBox(m_MainControl);
  m_Point2DSizeSpinBox->setRange(1, 20);
  m_Point2DSizeSpinBox->setValue(6);
  formLayout->addRow("Default 2D Point Size:", m_Point2DSizeSpinBox);

  // 3D point size
  m_Point3DSizeSpinBox = new QSpinBox(m_MainControl);
  m_Point3DSizeSpinBox->setRange(1, 30);
  m_Point3DSizeSpinBox->setValue(10);
  formLayout->addRow("Default 3D Point Size:", m_Point3DSizeSpinBox);

  // Path color
  m_PathColorButton = new QPushButton(m_MainControl);
  m_PathColorButton->setFixedSize(40, 20);
  formLayout->addRow("Default Path Color:", m_PathColorButton);

  connect(m_PathColorButton, &QPushButton::clicked,
          this, &xq_PathPreferencePage::OnColorButtonClicked);

  layout->addLayout(formLayout);
  layout->addStretch();

  Update();
}

QWidget* xq_PathPreferencePage::GetQtControl() const
{
  return m_MainControl;
}

bool xq_PathPreferencePage::PerformOk()
{
  auto* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService != nullptr)
  {
    m_Preferences = prefService->GetSystemPreferences()->Node("/org.xq.views.pathplanning");
    if (m_Preferences != nullptr)
    {
      m_Preferences->PutInt("point.2d.size", m_Point2DSizeSpinBox->value());
      m_Preferences->PutInt("point.3d.size", m_Point3DSizeSpinBox->value());
      m_Preferences->Put("path.color", m_PathColor.name().toStdString());
    }
  }
  return true;
}

void xq_PathPreferencePage::PerformCancel()
{
}

void xq_PathPreferencePage::Update()
{
  auto* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService != nullptr)
  {
    m_Preferences = prefService->GetSystemPreferences()->Node("/org.xq.views.pathplanning");
    if (m_Preferences != nullptr)
    {
      m_Point2DSizeSpinBox->setValue(m_Preferences->GetInt("point.2d.size", 6));
      m_Point3DSizeSpinBox->setValue(m_Preferences->GetInt("point.3d.size", 10));

      QString colorName = QString::fromStdString(
          m_Preferences->Get("path.color", "#ffff00"));
      m_PathColor = QColor(colorName);
    }
  }

  // Update color button appearance
  if (m_PathColorButton != nullptr)
  {
    QString style = QString("background-color: %1;").arg(m_PathColor.name());
    m_PathColorButton->setStyleSheet(style);
  }
}

void xq_PathPreferencePage::OnColorButtonClicked()
{
  QColor color = QColorDialog::getColor(m_PathColor, m_MainControl, "Select Path Color");
  if (color.isValid())
  {
    m_PathColor = color;
    QString style = QString("background-color: %1;").arg(m_PathColor.name());
    m_PathColorButton->setStyleSheet(style);
  }
}
