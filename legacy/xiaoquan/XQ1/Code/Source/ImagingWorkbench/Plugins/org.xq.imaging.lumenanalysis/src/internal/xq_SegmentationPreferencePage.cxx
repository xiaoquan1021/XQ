#include "xq_SegmentationPreferencePage.h"

#include <mitkCoreServices.h>
#include <mitkIPreferencesService.h>
#include <mitkIPreferences.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>

xq_SegmentationPreferencePage::xq_SegmentationPreferencePage()
  : m_MainControl(nullptr)
  , m_DefaultContourType(nullptr)
  , m_LoftSections(nullptr)
  , m_LoftSplineDegree(nullptr)
  , m_LoftSamplePerSection(nullptr)
  , m_LoftLinearSample(nullptr)
  , m_LoftUseFFT(nullptr)
  , m_ContourColorButton(nullptr)
  , m_SurfaceColorButton(nullptr)
  , m_ContourColor(Qt::yellow)
  , m_SurfaceColor(Qt::red)
  , m_Preferences(nullptr)
{
}

xq_SegmentationPreferencePage::~xq_SegmentationPreferencePage()
{
}

void xq_SegmentationPreferencePage::Init(berry::IWorkbench::Pointer /*workbench*/)
{
  auto* preferencesService = mitk::CoreServices::GetPreferencesService();
  if (preferencesService)
  {
    m_Preferences = preferencesService->GetSystemPreferences()->Node(
      "/org.xq.views.segmentation");
  }
}

void xq_SegmentationPreferencePage::CreateQtControl(QWidget* parent)
{
  m_MainControl = new QWidget(parent);
  QVBoxLayout* mainLayout = new QVBoxLayout(m_MainControl);

  // Default contour type
  QGroupBox* contourGroup = new QGroupBox("Contour Defaults", m_MainControl);
  QFormLayout* contourLayout = new QFormLayout(contourGroup);

  m_DefaultContourType = new QComboBox(contourGroup);
  m_DefaultContourType->addItem("Circle");
  m_DefaultContourType->addItem("Ellipse");
  m_DefaultContourType->addItem("SplinePolygon");
  m_DefaultContourType->addItem("Manual");
  contourLayout->addRow("Default Contour Type:", m_DefaultContourType);

  mainLayout->addWidget(contourGroup);

  // Lofting defaults
  QGroupBox* loftGroup = new QGroupBox("Lofting Defaults", m_MainControl);
  QFormLayout* loftLayout = new QFormLayout(loftGroup);

  m_LoftSections = new QSpinBox(loftGroup);
  m_LoftSections->setRange(2, 500);
  m_LoftSections->setValue(12);
  loftLayout->addRow("Number of Sections:", m_LoftSections);

  m_LoftSplineDegree = new QSpinBox(loftGroup);
  m_LoftSplineDegree->setRange(2, 5);
  m_LoftSplineDegree->setValue(3);
  loftLayout->addRow("Spline Degree:", m_LoftSplineDegree);

  m_LoftSamplePerSection = new QSpinBox(loftGroup);
  m_LoftSamplePerSection->setRange(4, 500);
  m_LoftSamplePerSection->setValue(60);
  loftLayout->addRow("Samples Per Section:", m_LoftSamplePerSection);

  m_LoftLinearSample = new QCheckBox(loftGroup);
  loftLayout->addRow("Use Linear Sample:", m_LoftLinearSample);

  m_LoftUseFFT = new QCheckBox(loftGroup);
  loftLayout->addRow("Use FFT:", m_LoftUseFFT);

  mainLayout->addWidget(loftGroup);

  // Display colors
  QGroupBox* colorGroup = new QGroupBox("Display Colors", m_MainControl);
  QFormLayout* colorLayout = new QFormLayout(colorGroup);

  m_ContourColorButton = new QPushButton(colorGroup);
  m_ContourColorButton->setAutoFillBackground(true);
  connect(m_ContourColorButton, &QPushButton::clicked,
          this, &xq_SegmentationPreferencePage::OnContourColorClicked);
  colorLayout->addRow("Contour Color:", m_ContourColorButton);

  m_SurfaceColorButton = new QPushButton(colorGroup);
  m_SurfaceColorButton->setAutoFillBackground(true);
  connect(m_SurfaceColorButton, &QPushButton::clicked,
          this, &xq_SegmentationPreferencePage::OnSurfaceColorClicked);
  colorLayout->addRow("Surface Color:", m_SurfaceColorButton);

  mainLayout->addWidget(colorGroup);

  // Spacer
  mainLayout->addStretch();

  Update();
}

QWidget* xq_SegmentationPreferencePage::GetQtControl() const
{
  return m_MainControl;
}

bool xq_SegmentationPreferencePage::PerformOk()
{
  if (m_Preferences != nullptr)
  {
    m_Preferences->PutInt("defaultContourType",
      m_DefaultContourType->currentIndex());
    m_Preferences->PutInt("loftSections", m_LoftSections->value());
    m_Preferences->PutInt("loftSplineDegree", m_LoftSplineDegree->value());
    m_Preferences->PutInt("loftSamplePerSection", m_LoftSamplePerSection->value());
    m_Preferences->PutBool("loftLinearSample", m_LoftLinearSample->isChecked());
    m_Preferences->PutBool("loftUseFFT", m_LoftUseFFT->isChecked());
    m_Preferences->Put("contourColor", m_ContourColor.name().toStdString());
    m_Preferences->Put("surfaceColor", m_SurfaceColor.name().toStdString());
  }
  return true;
}

void xq_SegmentationPreferencePage::PerformCancel()
{
}

void xq_SegmentationPreferencePage::Update()
{
  if (m_Preferences != nullptr)
  {
    m_DefaultContourType->setCurrentIndex(
      m_Preferences->GetInt("defaultContourType", 0));
    m_LoftSections->setValue(m_Preferences->GetInt("loftSections", 12));
    m_LoftSplineDegree->setValue(m_Preferences->GetInt("loftSplineDegree", 3));
    m_LoftSamplePerSection->setValue(
      m_Preferences->GetInt("loftSamplePerSection", 60));
    m_LoftLinearSample->setChecked(
      m_Preferences->GetBool("loftLinearSample", false));
    m_LoftUseFFT->setChecked(m_Preferences->GetBool("loftUseFFT", false));

    QString contourColorStr = QString::fromStdString(
      m_Preferences->Get("contourColor", "#ffff00"));
    m_ContourColor = QColor(contourColorStr);

    QString surfaceColorStr = QString::fromStdString(
      m_Preferences->Get("surfaceColor", "#ff0000"));
    m_SurfaceColor = QColor(surfaceColorStr);
  }

  // Update button colors
  if (m_ContourColorButton)
  {
    QPalette pal = m_ContourColorButton->palette();
    pal.setColor(QPalette::Button, m_ContourColor);
    m_ContourColorButton->setPalette(pal);
    m_ContourColorButton->setText(m_ContourColor.name());
  }

  if (m_SurfaceColorButton)
  {
    QPalette pal = m_SurfaceColorButton->palette();
    pal.setColor(QPalette::Button, m_SurfaceColor);
    m_SurfaceColorButton->setPalette(pal);
    m_SurfaceColorButton->setText(m_SurfaceColor.name());
  }
}

void xq_SegmentationPreferencePage::OnContourColorClicked()
{
  QColor chosen = QColorDialog::getColor(m_ContourColor, m_MainControl,
    "Select Contour Color");
  if (chosen.isValid())
  {
    m_ContourColor = chosen;
    QPalette pal = m_ContourColorButton->palette();
    pal.setColor(QPalette::Button, m_ContourColor);
    m_ContourColorButton->setPalette(pal);
    m_ContourColorButton->setText(m_ContourColor.name());
  }
}

void xq_SegmentationPreferencePage::OnSurfaceColorClicked()
{
  QColor chosen = QColorDialog::getColor(m_SurfaceColor, m_MainControl,
    "Select Surface Color");
  if (chosen.isValid())
  {
    m_SurfaceColor = chosen;
    QPalette pal = m_SurfaceColorButton->palette();
    pal.setColor(QPalette::Button, m_SurfaceColor);
    m_SurfaceColorButton->setPalette(pal);
    m_SurfaceColorButton->setText(m_SurfaceColor.name());
  }
}
