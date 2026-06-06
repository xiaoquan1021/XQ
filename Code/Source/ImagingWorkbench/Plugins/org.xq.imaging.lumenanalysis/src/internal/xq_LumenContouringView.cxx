#include "xq_LumenContouringView.h"
#include "xq_ProfileGroupCreate.h"
#include "xq_LoftParamWidget.h"
#include "ui_xq_LumenContouringView.h"

#include <xq_ProfileGroupInteractor.h>
#include <xq_Seg3DUtils.h>
#include <xq_SegmentationUtils.h>
#include <xq_ContourGroup.h>
#include <xq_ProfileGroup.h>
#include <xq_PolygonalProfile.h>
#include <xq_ThresholdContour.h>
#include <xq_VesselCenterline.h>
#include <xq_SegmentationPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <xq_RenderWindowStateUtils.h>
#include <xq_LegacyNodeMigration.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>
#include <mitkPointSet.h>
#include <mitkImage.h>
#include <mitkImageCast.h>
#include <mitkDataNodeSelection.h>
#include <mitkBaseRenderer.h>
#include <mitkPlaneGeometry.h>
#include <mitkSliceNavigationController.h>

#include <usModuleRegistry.h>

#include <berryISelectionService.h>

#include <QmitkRenderWindow.h>

#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyData.h>
#include <vtkTriangleFilter.h>
#include <vtkStripper.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkSplineFilter.h>

#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>

namespace
{

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTwoPi = 2.0 * kPi;

xq_ProfileGroup* GetProfileGroup(mitk::DataNode* node)
{
  return node ? dynamic_cast<xq_ProfileGroup*>(node->GetData()) : nullptr;
}

xq_ContourGroup* GetContourGroup(mitk::DataNode* node)
{
  return node ? dynamic_cast<xq_ContourGroup*>(node->GetData()) : nullptr;
}

int GetProfileListRow(const xq_ProfileGroup* group, int pathPosIndex)
{
  if (!group)
    return -1;

  const auto indices = group->GetProfilePathIndices();
  const auto it = std::find(indices.cbegin(), indices.cend(), pathPosIndex);
  return it == indices.cend() ? -1 : static_cast<int>(std::distance(indices.cbegin(), it));
}

mitk::BaseRenderer* GetCurrentSliceRenderer()
{
  auto* renderer = mitk::BaseRenderer::GetInstance(
    mitk::BaseRenderer::GetRenderWindowByName("stdmulti.widget0"));
  if (!renderer)
  {
    renderer = mitk::BaseRenderer::GetInstance(
      mitk::BaseRenderer::GetRenderWindowByName("stdmulti.widget1"));
  }
  return renderer;
}

const mitk::PlaneGeometry* GetCurrentSlicePlaneGeometry()
{
  auto* renderer = GetCurrentSliceRenderer();
  return renderer ? renderer->GetCurrentWorldPlaneGeometry() : nullptr;
}

std::vector<xq_ProfilePlacementFrame> BuildPlacementFrames(xq_VesselCenterline* centerline)
{
  std::vector<xq_ProfilePlacementFrame> frames;
  if (!centerline)
    return frames;

  auto* segment = centerline->GetSegment();
  if (!segment)
    return frames;

  if (segment->GetTraceVertexCount() == 0 && segment->GetAnchorCount() >= 2)
  {
    segment->Interpolate();
  }

  const auto traceVertices = segment->GetTraceVertices();
  frames.reserve(traceVertices.size());
  for (const auto& traceVertex : traceVertices)
  {
    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = traceVertex.id;
    frame.position = traceVertex.pos;
    frame.tangent = traceVertex.tangent;
    frame.rotation = traceVertex.rotation;
    frames.push_back(frame);
  }

  return frames;
}

xq_VesselCenterline* FindPathForProfileGroup(
  mitk::DataStorage* dataStorage, const xq_ProfileGroup* profileGroup)
{
  if (!dataStorage || !profileGroup)
    return nullptr;

  const auto pathName = profileGroup->GetAttribute("path_name");
  if (pathName.empty())
    return nullptr;

  auto allNodes = dataStorage->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    const auto& node = it->Value();
    const bool isPath = xq::pipeline::IsPathNode(node);
    if (!isPath || node->GetName() != pathName)
      continue;

    return dynamic_cast<xq_VesselCenterline*>(node->GetData());
  }

  return nullptr;
}

mitk::DataNode::Pointer FindPathNodeByName(
  mitk::DataStorage* dataStorage, const std::string& pathName)
{
  if (!dataStorage || pathName.empty())
    return nullptr;

  auto allNodes = dataStorage->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    const auto& node = it->Value();
    if (xq::pipeline::IsPathNode(node) && node->GetName() == pathName)
      return node;
  }

  return nullptr;
}

bool ResolvePlacementFrame(
  mitk::DataStorage* dataStorage,
  const xq_ProfileGroup* profileGroup,
  const mitk::PlaneGeometry* referencePlane,
  xq_ProfilePlacementFrame& outFrame)
{
  if (!dataStorage || !profileGroup || !referencePlane)
    return false;

  auto* centerline = FindPathForProfileGroup(dataStorage, profileGroup);
  const auto frames = BuildPlacementFrames(centerline);
  return xq_SegmentationUtils::FindNearestPlacementFrame(
    frames, referencePlane->GetOrigin(), outFrame);
}

bool InsertCanonicalProfile(
  mitk::DataStorage* dataStorage,
  xq_ProfileGroup* profileGroup,
  QListWidget* contourListWidget,
  xq_ProfileGroupInteractor* profileInteractor,
  const std::vector<mitk::Point3D>& contourPoints,
  const mitk::PlaneGeometry* referencePlane,
  std::string_view method,
  int& outPathPosIndex)
{
  if (!dataStorage || !profileGroup || !referencePlane || contourPoints.size() < 3)
    return false;

  auto profile = xq_SegmentationUtils::CreateProfileFromContourPoints(
    contourPoints, method, "Polygon");
  if (!profile)
    return false;

  xq_ProfilePlacementFrame placementFrame;
  const bool hasPlacementFrame = ResolvePlacementFrame(
    dataStorage, profileGroup, referencePlane, placementFrame);
  if (!xq_SegmentationUtils::ResolveCanonicalPathPosIndex(
        profileGroup, hasPlacementFrame ? &placementFrame : nullptr, outPathPosIndex))
  {
    QMessageBox::warning(nullptr, "Canonical Placement",
      "Cannot resolve the current centerline placement. Select a valid vessel path slice before writing back a canonical contour.");
    return false;
  }

  if (hasPlacementFrame)
  {
    xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), placementFrame, referencePlane);
  }
  else
  {
    profile->SetPathPosIndex(outPathPosIndex);
    profile->SetSlicePlane(referencePlane->Clone());
  }

  if (profileGroup->HasProfile(outPathPosIndex))
  {
    profileGroup->ReplaceProfile(profile.release(), outPathPosIndex);
  }
  else
  {
    profileGroup->AppendProfile(profile.release(), outPathPosIndex);
  }

  if (contourListWidget)
  {
    contourListWidget->clearSelection();
  }

  if (profileInteractor)
  {
    profileInteractor->SetCurrentPathPosIndex(outPathPosIndex);
  }

  return true;
}

void UpdateContourGroupReadiness(mitk::DataNode* node, xq_ProfileGroup* group)
{
    if (!node || !group)
        return;
    auto report = xq_SegmentationUtils::BuildReadinessReport(group);
    node->SetBoolProperty("xq.contour.ready", report.loftReady && report.modelingReady);
    node->SetIntProperty("xq.contour.profile_count", report.profileCount);
    node->SetIntProperty("xq.contour.missing_count", report.missingCount);
    node->SetIntProperty("xq.contour.warning_count", static_cast<int>(report.warnings.size()));
    node->SetIntProperty("xq.contour.error_count", static_cast<int>(report.errors.size()));
}

} // namespace

const QString xq_LumenContouringView::VIEW_ID = "org.xq.views.segmentation";

xq_LumenContouringView::xq_LumenContouringView()
  : m_Ui(nullptr)
  , m_PathComboBox(nullptr)
  , m_ContourGroupListWidget(nullptr)
  , m_ContourListWidget(nullptr)
  , m_ContextLabel(nullptr)
  , m_ResliceSlider(nullptr)
  , m_ResliceLabel(nullptr)
  , m_ResliceSizeSpin(nullptr)
  , m_CurrentContourGroupNode(nullptr)
  , m_CurrentContourIndex(-1)
{
}

xq_LumenContouringView::~xq_LumenContouringView()
{
  DetachProfileInteractor();
  delete m_Ui;
}

void xq_LumenContouringView::EnsureProfileInteractor(int pathPosIndex)
{
  if (m_CurrentContourGroupNode.IsNull() || GetProfileGroup(m_CurrentContourGroupNode) == nullptr)
    return;

  if (m_ProfileInteractor.IsNull())
  {
    m_ProfileInteractor = xq_ProfileGroupInteractor::New();
    m_ProfileInteractor->LoadStateMachine("xq_ProfileGroupInteraction.xml",
      us::ModuleRegistry::GetModule("xqModuleSegmentation"));
    m_ProfileInteractor->SetEventConfig("xq_ProfileGroupConfig.xml",
      us::ModuleRegistry::GetModule("xqModuleSegmentation"));
  }

  m_ProfileInteractor->SetCurrentPathPosIndex(pathPosIndex);
  m_ProfileInteractor->SetDataNode(m_CurrentContourGroupNode);
}

void xq_LumenContouringView::DetachProfileInteractor()
{
  if (m_ProfileInteractor.IsNotNull() && m_ProfileInteractor->GetDataNode())
  {
    m_ProfileInteractor->GetDataNode()->SetDataInteractor(nullptr);
  }
}

void xq_LumenContouringView::CreateQtPartControl(QWidget* parent)
{
  xq_LegacyNodeMigration::UpgradeImportedLegacyNodes(GetDataStorage());
  xq_LegacyNodeMigration::ReparentIntoCategoryFolders(GetDataStorage());

  m_Ui = new Ui::xq_LumenContouringView;
  m_Ui->setupUi(parent);

  m_PathComboBox = m_Ui->pathComboBox;
  m_ContourGroupListWidget = m_Ui->contourGroupListWidget;
  m_ContourListWidget = m_Ui->contourListWidget;

  m_ContextLabel = new QLabel(parent);
  m_ContextLabel->setWordWrap(true);
  m_ContextLabel->setStyleSheet(
    "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
    "border-radius: 4px; padding: 6px; }");
  m_Ui->mainLayout->insertWidget(0, m_ContextLabel);

  auto* resliceLayout = new QHBoxLayout();
  resliceLayout->addWidget(new QLabel("Path Slice", parent));
  m_ResliceSlider = new QSlider(Qt::Horizontal, parent);
  m_ResliceSlider->setEnabled(false);
  m_ResliceSlider->setMinimum(0);
  m_ResliceSlider->setMaximum(0);
  resliceLayout->addWidget(m_ResliceSlider, 1);
  m_ResliceLabel = new QLabel("-- / --", parent);
  resliceLayout->addWidget(m_ResliceLabel);
  resliceLayout->addWidget(new QLabel("Size", parent));
  m_ResliceSizeSpin = new QDoubleSpinBox(parent);
  m_ResliceSizeSpin->setRange(1.0, 200.0);
  m_ResliceSizeSpin->setDecimals(1);
  m_ResliceSizeSpin->setSingleStep(1.0);
  m_ResliceSizeSpin->setValue(12.0);
  m_ResliceSizeSpin->setSuffix(" mm");
  resliceLayout->addWidget(m_ResliceSizeSpin);
  m_Ui->pathLayout->addLayout(resliceLayout);

  // Path selection
  connect(m_PathComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_LumenContouringView::OnPathSelectionChanged);
  connect(m_ResliceSlider, &QSlider::valueChanged,
          this, &xq_LumenContouringView::OnReslicePositionChanged);
  connect(m_ResliceSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double value) {
            if (m_CurrentContourGroupNode.IsNotNull())
              m_CurrentContourGroupNode->SetDoubleProperty(
                "xq.segmentation.reslice_size", value);
            PersistContourToolMetadata();
            UpdatePathReslice();
          });
  connect(m_Ui->spinLowerThresh, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_LumenContouringView::PersistContourToolMetadata);
  connect(m_Ui->spinUpperThresh, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_LumenContouringView::PersistContourToolMetadata);
  connect(m_Ui->spinThreshContourValue, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_LumenContouringView::PersistContourToolMetadata);

  // Contour group buttons
  connect(m_Ui->btnCreateGroup, &QPushButton::clicked,
          this, &xq_LumenContouringView::CreateContourGroup);
  connect(m_Ui->btnDeleteGroup, &QPushButton::clicked,
          this, &xq_LumenContouringView::DeleteContourGroup);
  connect(m_Ui->btnLoft, &QPushButton::clicked,
          this, &xq_LumenContouringView::LoftContourGroup);

  // Contour buttons
  connect(m_Ui->btnAddContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::AddContour);
  connect(m_Ui->btnDeleteContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::DeleteContour);
  connect(m_Ui->btnCopyContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::CopyContour);
  connect(m_Ui->btnPasteContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::PasteContour);
  connect(m_Ui->btnScaleContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::ScaleContour);

  // Contour editing buttons
  connect(m_Ui->btnSmoothContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::SmoothContour);
  connect(m_Ui->btnResampleContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::ResampleContour);
  connect(m_Ui->btnContourStats, &QPushButton::clicked,
          this, &xq_LumenContouringView::ShowContourStatistics);

  // Auto-segmentation button
  connect(m_Ui->btnPerformSegmentation, &QPushButton::clicked,
          this, &xq_LumenContouringView::PerformAutoSegmentation);

  // Threshold contour button
  connect(m_Ui->btnApplyThresholdContour, &QPushButton::clicked,
          this, &xq_LumenContouringView::OnApplyThresholdContour);

  // Contour type toolbox
  connect(m_Ui->contourToolBox, &QToolBox::currentChanged,
          this, &xq_LumenContouringView::OnContourTypeChanged);

  // Contour group selection
  connect(m_ContourGroupListWidget, &QListWidget::currentRowChanged,
          this, [this](int /*row*/) { UpdateContourList(); });
  connect(m_ContourListWidget, &QListWidget::currentRowChanged,
          this, [this](int row) {
            m_CurrentContourIndex = row;
            if (m_CurrentContourGroupNode.IsNotNull() && row >= 0)
              m_CurrentContourGroupNode->SetIntProperty(
                "xq.segmentation.current_contour_index", row);
            PersistContourToolMetadata();

            auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode);
            if (!profileGroup || row < 0)
              return;

            const auto indices = profileGroup->GetProfilePathIndices();
            if (row >= static_cast<int>(indices.size()))
              return;

            EnsureProfileInteractor(indices[static_cast<size_t>(row)]);
          });

  // Populate available paths
  PopulatePathComboBox();
  UpdatePathReslice();
  EnableContourControls(false);
  UpdateContextStatus("Missing contour group");
  BindCurrentDataManagerSelection();
}

void xq_LumenContouringView::SetFocus()
{
  if (m_ContourGroupListWidget)
    m_ContourGroupListWidget->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_LumenContouringView::Activated()
{
  BindCurrentDataManagerSelection();
}

void xq_LumenContouringView::Deactivated()
{
  DeactivateToolState();
}

void xq_LumenContouringView::Visible()
{
}

void xq_LumenContouringView::Hidden()
{
  DeactivateToolState();
}

void xq_LumenContouringView::RenderWindowPartActivated(mitk::IRenderWindowPart* /*renderWindowPart*/)
{
}

void xq_LumenContouringView::RenderWindowPartDeactivated(mitk::IRenderWindowPart* /*renderWindowPart*/)
{
  DeactivateToolState();
}

void xq_LumenContouringView::DeactivateToolState()
{
  DetachProfileInteractor();
  m_CurrentSlicedGeometry = nullptr;
  EnableContourControls(false);
  xq::rendering::RestoreOrthogonalSliceViews(GetDataStorage());
}

void xq_LumenContouringView::BindCurrentDataManagerSelection()
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

void xq_LumenContouringView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer /*source*/,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      if (GetProfileGroup(node) != nullptr || GetContourGroup(node) != nullptr)
      {
        if (m_CurrentContourGroupNode != node)
          DetachProfileInteractor();
        m_CurrentContourGroupNode = node;
        if (auto* profileGroup = GetProfileGroup(node))
        {
          QString pathName = QString::fromStdString(profileGroup->GetAttribute("path_name"));
          if (pathName.isEmpty())
          {
            std::string sourcePath;
            if (node->GetStringProperty(xq::pipeline::kSourcePathProperty, sourcePath))
              pathName = QString::fromStdString(sourcePath);
          }
          if (pathName.isEmpty())
          {
            std::string currentPath;
            if (node->GetStringProperty("xq.segmentation.current_path_name", currentPath))
              pathName = QString::fromStdString(currentPath);
          }
          if (!pathName.isEmpty() && m_PathComboBox)
          {
            const int comboIndex = m_PathComboBox->findText(pathName);
            if (comboIndex >= 0)
              m_PathComboBox->setCurrentIndex(comboIndex);
          }
        }

        double resliceSize = 0.0;
        if (m_ResliceSizeSpin &&
            node->GetDoubleProperty("xq.segmentation.reslice_size", resliceSize) &&
            resliceSize > 0.0)
        {
          m_ResliceSizeSpin->setValue(resliceSize);
        }

        double thresholdMin = 0.0;
        double thresholdMax = 0.0;
        if (m_Ui && node->GetDoubleProperty("xq.segmentation.threshold.min", thresholdMin))
          m_Ui->spinLowerThresh->setValue(thresholdMin);
        if (m_Ui && node->GetDoubleProperty("xq.segmentation.threshold.max", thresholdMax))
        {
          m_Ui->spinUpperThresh->setValue(thresholdMax);
          m_Ui->spinThreshContourValue->setValue(thresholdMax);
        }

        std::string activeType;
        if (m_Ui && m_Ui->contourToolBox &&
            node->GetStringProperty("xq.segmentation.activetype", activeType))
        {
          const QString active = QString::fromStdString(activeType);
          if (active == "Circle")
            m_Ui->contourToolBox->setCurrentIndex(0);
          else if (active == "Ellipse")
            m_Ui->contourToolBox->setCurrentIndex(1);
          else if (active == "SplinePolygon")
            m_Ui->contourToolBox->setCurrentIndex(2);
          else if (active == "Manual")
            m_Ui->contourToolBox->setCurrentIndex(3);
        }

        UpdateContourList();

        int currentContourIndex = -1;
        if (m_ContourListWidget &&
            node->GetIntProperty("xq.segmentation.current_contour_index", currentContourIndex) &&
            currentContourIndex >= 0 &&
            currentContourIndex < m_ContourListWidget->count())
        {
          m_ContourListWidget->setCurrentRow(currentContourIndex);
        }

        if (auto* profileGroup = GetProfileGroup(node))
        {
          const auto indices = profileGroup->GetProfilePathIndices();
          if (!indices.empty())
            EnsureProfileInteractor(indices.front());
        }
        EnableContourControls(true);
        UpdatePathReslice();
        int currentResliceIndex = -1;
        if (m_ResliceSlider &&
            node->GetIntProperty("xq.segmentation.current_reslice_index", currentResliceIndex) &&
            currentResliceIndex >= m_ResliceSlider->minimum() &&
            currentResliceIndex <= m_ResliceSlider->maximum())
        {
          m_ResliceSlider->setValue(currentResliceIndex);
        }
        PersistContourToolMetadata();
        UpdateContextStatus("Ready");
        return;
      }
    }
  }

  ClearContourState();
}

void xq_LumenContouringView::ClearContourState()
{
  DetachProfileInteractor();
  m_CurrentContourGroupNode = nullptr;
  m_CurrentContourIndex = -1;
  if (m_ContourListWidget)
    m_ContourListWidget->clear();
  UpdatePathReslice();
  EnableContourControls(false);
  UpdateContextStatus("Missing contour group");
}

void xq_LumenContouringView::EnableContourControls(bool enabled)
{
  if (!m_Ui)
    return;

  m_Ui->btnDeleteGroup->setEnabled(enabled);
  m_Ui->btnLoft->setEnabled(enabled);
  m_Ui->btnAddContour->setEnabled(enabled);
  m_Ui->btnDeleteContour->setEnabled(enabled);
  m_Ui->btnCopyContour->setEnabled(enabled);
  m_Ui->btnPasteContour->setEnabled(enabled);
  m_Ui->btnScaleContour->setEnabled(enabled);
  m_Ui->btnSmoothContour->setEnabled(enabled);
  m_Ui->btnResampleContour->setEnabled(enabled);
  m_Ui->btnContourStats->setEnabled(enabled);
  m_Ui->btnPerformSegmentation->setEnabled(enabled);
  m_Ui->btnApplyThresholdContour->setEnabled(enabled);
}

void xq_LumenContouringView::UpdateContextStatus(const QString& status)
{
  if (!m_ContextLabel)
    return;

  const QString groupName = m_CurrentContourGroupNode.IsNotNull()
    ? QString::fromStdString(m_CurrentContourGroupNode->GetName())
    : QStringLiteral("<none>");
  const QString pathName = m_PathComboBox && !m_PathComboBox->currentText().isEmpty()
    ? m_PathComboBox->currentText()
    : QStringLiteral("<none>");
  QString imageName = QStringLiteral("<unresolved>");
  if (m_CurrentContourGroupNode.IsNotNull())
  {
    std::string sourceImage;
    if (m_CurrentContourGroupNode->GetStringProperty(xq::pipeline::kSourceImageProperty, sourceImage) &&
        !sourceImage.empty())
    {
      imageName = QString::fromStdString(sourceImage);
    }
    else
    {
      auto imageNode = FindSelectedImageNode();
      if (imageNode.IsNotNull())
        imageName = QString::fromStdString(imageNode->GetName());
    }
  }

  const QString resolvedStatus = status.isEmpty()
    ? (m_CurrentContourGroupNode.IsNotNull() ? QStringLiteral("Ready") : QStringLiteral("Missing contour group"))
    : status;
  m_ContextLabel->setText(
    QString("Current contour group: %1\nInput path: %2\nInput image: %3\nStatus: %4\nNext: create/refine contours, then build an anatomy model.")
      .arg(groupName, pathName, imageName, resolvedStatus));
}

void xq_LumenContouringView::PersistContourToolMetadata()
{
  if (m_CurrentContourGroupNode.IsNull() || !m_Ui)
    return;

  if (m_PathComboBox)
  {
    const QString pathName = m_PathComboBox->currentText();
    if (!pathName.isEmpty())
    {
      m_CurrentContourGroupNode->SetStringProperty(
        xq::pipeline::kSourcePathProperty, pathName.toStdString().c_str());
      m_CurrentContourGroupNode->SetStringProperty(
        "xq.segmentation.current_path_name", pathName.toStdString().c_str());
    }
  }

  if (m_ResliceSlider)
    m_CurrentContourGroupNode->SetIntProperty(
      "xq.segmentation.current_reslice_index", m_ResliceSlider->value());
  if (m_ResliceSizeSpin)
    m_CurrentContourGroupNode->SetDoubleProperty(
      "xq.segmentation.reslice_size", m_ResliceSizeSpin->value());

  m_CurrentContourGroupNode->SetDoubleProperty(
    "xq.segmentation.threshold.min", m_Ui->spinLowerThresh->value());
  m_CurrentContourGroupNode->SetDoubleProperty(
    "xq.segmentation.threshold.max", m_Ui->spinUpperThresh->value());
  m_CurrentContourGroupNode->SetDoubleProperty(
    "xq.segmentation.threshold.current", m_Ui->spinThreshContourValue->value());

  if (m_ContourListWidget && m_ContourListWidget->currentRow() >= 0)
    m_CurrentContourGroupNode->SetIntProperty(
      "xq.segmentation.current_contour_index", m_ContourListWidget->currentRow());

  if (auto imageNode = FindSelectedImageNode(); imageNode.IsNotNull())
    m_CurrentContourGroupNode->SetStringProperty(
      xq::pipeline::kSourceImageProperty, imageNode->GetName().c_str());

  m_CurrentContourGroupNode->Modified();
}

void xq_LumenContouringView::PopulatePathComboBox()
{
  if (!m_PathComboBox)
    return;

  const QSignalBlocker blocker(m_PathComboBox);
  m_PathComboBox->setUpdatesEnabled(false);
  m_PathComboBox->clear();

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull())
  {
    m_PathComboBox->setUpdatesEnabled(true);
    return;
  }

  mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    const auto& node = it->Value();
    const bool isPath = xq::pipeline::IsPathNode(node);
    if (isPath)
    {
      m_PathComboBox->addItem(
        QString::fromStdString(node->GetName()));
    }
  }
  m_PathComboBox->setUpdatesEnabled(true);
}

void xq_LumenContouringView::CreateContourGroup()
{
  xq_ProfileGroupCreate dialog(GetDataStorage(),
    this->GetSite()->GetWorkbenchWindow()->GetShell()->GetControl());
  if (GetSite() && GetSite()->GetWorkbenchWindow().IsNotNull())
  {
    auto* selectionService = GetSite()->GetWorkbenchWindow()->GetSelectionService();
    if (selectionService)
    {
      berry::ISelection::ConstPointer selection =
        selectionService->GetSelection("org.xq.views.datamanager");
      mitk::DataNodeSelection::ConstPointer nodeSelection =
        selection.Cast<const mitk::DataNodeSelection>();
      if (nodeSelection.IsNull())
      {
        selection = selectionService->GetSelection();
        nodeSelection = selection.Cast<const mitk::DataNodeSelection>();
      }
      if (nodeSelection.IsNotNull())
      {
        const auto selectedNodes = nodeSelection->GetSelectedDataNodes();
        for (const auto& node : selectedNodes)
        {
          if (xq::pipeline::IsPathNode(node))
          {
            dialog.SelectPath(QString::fromStdString(node->GetName()));
            break;
          }
        }
      }
    }
  }

  if (dialog.exec() == QDialog::Accepted)
  {
    QString groupName = dialog.GetGroupName();
    if (groupName.isEmpty())
    {
      QMessageBox::warning(nullptr, "2D Segmentation", "Please enter a group name.");
      return;
    }

    QString pathName = dialog.GetSelectedPathName();
    const auto createResult = xq_SegmentationPipelineService::CreateContourGroup(
      GetDataStorage(),
      {groupName.toStdString(), pathName.toStdString()});
    if (!createResult.ok || createResult.node.IsNull())
    {
      QMessageBox::warning(nullptr, "2D Segmentation",
        "Failed to create contour group.");
      return;
    }

    m_ContourGroupListWidget->addItem(groupName);
    m_CurrentContourGroupNode = createResult.node;

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_LumenContouringView::DeleteContourGroup()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  QMessageBox::StandardButton reply = QMessageBox::question(
    nullptr, "Delete Contour Group",
    QString("Delete group '%1' and all its contours?")
      .arg(m_CurrentContourGroupNode->GetName().c_str()),
    QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes)
  {
    // Remove child contour nodes
    mitk::DataStorage::SetOfObjects::ConstPointer children =
      GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);
    for (auto it = children->Begin(); it != children->End(); ++it)
      GetDataStorage()->Remove(it->Value());

    GetDataStorage()->Remove(m_CurrentContourGroupNode);
    m_CurrentContourGroupNode = nullptr;
    m_CurrentContourIndex = -1;

    // Refresh list
    int row = m_ContourGroupListWidget->currentRow();
    if (row >= 0)
      delete m_ContourGroupListWidget->takeItem(row);

    m_ContourListWidget->clear();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_LumenContouringView::AddContour()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::information(nullptr, "2D Segmentation",
      "Select a contour group first.");
    return;
  }

  int contourType = m_Ui->contourToolBox->currentIndex();
  QString typeName;
  switch (contourType)
  {
    case 0: typeName = "Circle"; break;
    case 1: typeName = "Ellipse"; break;
    case 2: typeName = "SplinePolygon"; break;
    case 3: typeName = "Manual"; break;
    default: typeName = "Circle"; break;
  }

  auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode);
  auto* contourGroup = GetContourGroup(m_CurrentContourGroupNode);
  const mitk::PlaneGeometry* currentPlane = GetCurrentSlicePlaneGeometry();

  xq_ProfilePlacementFrame placementFrame;
  const bool hasPlacementFrame = profileGroup != nullptr &&
    ResolvePlacementFrame(GetDataStorage(), profileGroup, currentPlane, placementFrame);
  const bool requiresPathPlacement = xq_SegmentationUtils::RequiresPathPlacement(profileGroup);

  if (profileGroup != nullptr && requiresPathPlacement && !hasPlacementFrame)
  {
    QMessageBox::warning(nullptr, "Add Contour",
      "Cannot resolve the current centerline placement. Select a valid vessel path slice before adding a contour.");
    return;
  }

  int contourIdx = 0;
  if (profileGroup != nullptr)
  {
    if (!xq_SegmentationUtils::ResolveCanonicalPathPosIndex(
          profileGroup, hasPlacementFrame ? &placementFrame : nullptr, contourIdx))
    {
      QMessageBox::warning(nullptr, "Add Contour",
        "Cannot resolve the current centerline placement. Select a valid vessel path slice before adding a contour.");
      return;
    }
  }
  else
  {
    mitk::DataStorage::SetOfObjects::ConstPointer children =
      GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);
    contourIdx = static_cast<int>(children->size());
  }

  // Parameter dialog for circle/ellipse
  double radius = 5.0;
  double radiusX = 5.0, radiusY = 3.0;
  double centerZ = hasPlacementFrame ? placementFrame.position[2] : contourIdx * 2.0;
  int numPoints = 36;

  if (contourType == 0) // Circle
  {
    QDialog paramDlg(nullptr);
    paramDlg.setWindowTitle("Circle Contour Parameters");
    auto* fl = new QFormLayout(&paramDlg);
    auto* radiusSpin = new QDoubleSpinBox();
    radiusSpin->setRange(0.1, 100.0);
    radiusSpin->setValue(5.0);
    radiusSpin->setSuffix(" mm");
    fl->addRow("Radius:", radiusSpin);
    auto* zPosSpin = new QDoubleSpinBox();
    zPosSpin->setRange(-500.0, 500.0);
    zPosSpin->setValue(centerZ);
    fl->addRow("Z Position:", zPosSpin);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &paramDlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &paramDlg, &QDialog::reject);
    if (paramDlg.exec() != QDialog::Accepted) return;
    radius = radiusSpin->value();
    centerZ = zPosSpin->value();
  }
  else if (contourType == 1) // Ellipse
  {
    QDialog paramDlg(nullptr);
    paramDlg.setWindowTitle("Ellipse Contour Parameters");
    auto* fl = new QFormLayout(&paramDlg);
    auto* rxSpin = new QDoubleSpinBox();
    rxSpin->setRange(0.1, 100.0);
    rxSpin->setValue(5.0);
    rxSpin->setSuffix(" mm");
    fl->addRow("Semi-axis X:", rxSpin);
    auto* rySpin = new QDoubleSpinBox();
    rySpin->setRange(0.1, 100.0);
    rySpin->setValue(3.0);
    rySpin->setSuffix(" mm");
    fl->addRow("Semi-axis Y:", rySpin);
    auto* zPosSpin = new QDoubleSpinBox();
    zPosSpin->setRange(-500.0, 500.0);
    zPosSpin->setValue(centerZ);
    fl->addRow("Z Position:", zPosSpin);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &paramDlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &paramDlg, &QDialog::reject);
    if (paramDlg.exec() != QDialog::Accepted) return;
    radiusX = rxSpin->value();
    radiusY = rySpin->value();
    centerZ = zPosSpin->value();
  }

  if (profileGroup != nullptr)
  {
    mitk::Point3D center;
    if (hasPlacementFrame)
    {
      center = placementFrame.position;
    }
    else
    {
      center[0] = 0.0;
      center[1] = 0.0;
      center[2] = centerZ;
    }

    std::unique_ptr<xq_LumenProfile> profile;
    if (contourType == 0)
    {
      profile = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", center, radius, 0.0, numPoints);
    }
    else if (contourType == 1)
    {
      profile = xq_SegmentationUtils::CreatePresetProfile(
        "Ellipse", center, radiusX, radiusY, numPoints);
    }
    else if (contourType == 2)
    {
      profile = xq_SegmentationUtils::CreateEditableProfile("SplinePolygon", center);
    }
    else if (contourType == 3)
    {
      profile = xq_SegmentationUtils::CreateEditableProfile("Manual", center);
    }
    else
    {
      QMessageBox::warning(nullptr, "2D Segmentation",
        "Unsupported contour type for canonical profile creation.");
      return;
    }

    if (!profile)
    {
      QMessageBox::warning(nullptr, "2D Segmentation",
        "Failed to create a canonical lumen profile.");
      return;
    }

    const double primarySize = contourType == 1 ? radiusX : radius;
    const double secondarySize = contourType == 1 ? radiusY : 0.0;

    if (hasPlacementFrame)
    {
      xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), placementFrame, currentPlane);
      xq_SegmentationUtils::OrientPresetProfileToPlacement(
        profile.get(), primarySize, secondarySize);
    }
    else if (currentPlane != nullptr)
    {
      profile->SetPathPosIndex(contourIdx);
      profile->SetSlicePlane(currentPlane->Clone());
      xq_SegmentationUtils::OrientPresetProfileToPlacement(
        profile.get(), primarySize, secondarySize);
    }

    if (profileGroup->HasProfile(contourIdx))
    {
      profileGroup->ReplaceProfile(profile.release(), contourIdx);
    }
    else
    {
      profileGroup->AppendProfile(profile.release(), contourIdx);
    }

    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    EnsureProfileInteractor(contourIdx);
    UpdateContourList();
    const int row = GetProfileListRow(profileGroup, contourIdx);
    if (row >= 0)
      m_ContourListWidget->setCurrentRow(row);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    if (contourType == 2 || contourType == 3)
    {
      QMessageBox::information(nullptr, "2D Segmentation",
        "Editable profile created. Use Ctrl + Left Click to add points, Enter to finish, and Escape to cancel.");
    }
    return;
  }

  if (contourGroup == nullptr)
  {
    QMessageBox::warning(nullptr, "2D Segmentation",
      "Selected node is not a supported contour container.");
    return;
  }

  // Create PointSet with actual geometry
  mitk::PointSet::Pointer contourPoints = mitk::PointSet::New();

  if (contourType == 0) // Circle
  {
    for (int i = 0; i < numPoints; ++i)
    {
      double angle = kTwoPi * i / numPoints;
      mitk::Point3D pt;
      pt[0] = radius * std::cos(angle);
      pt[1] = radius * std::sin(angle);
      pt[2] = centerZ;
      contourPoints->InsertPoint(i, pt);
    }
  }
  else if (contourType == 1) // Ellipse
  {
    for (int i = 0; i < numPoints; ++i)
    {
      double angle = kTwoPi * i / numPoints;
      mitk::Point3D pt;
      pt[0] = radiusX * std::cos(angle);
      pt[1] = radiusY * std::sin(angle);
      pt[2] = centerZ;
      contourPoints->InsertPoint(i, pt);
    }
  }
  // SplinePolygon and Manual: start with empty PointSet for user to populate

  // Create contour data node
  mitk::DataNode::Pointer contourNode = mitk::DataNode::New();
  contourNode->SetData(contourPoints);
  contourNode->SetName(
    (m_CurrentContourGroupNode->GetName() + "_contour_" + std::to_string(contourIdx)));
  contourNode->SetBoolProperty("xq.segmentation.contour", true);
  contourNode->SetStringProperty("xq.segmentation.contourtype",
    typeName.toStdString().c_str());
  contourNode->SetIntProperty("xq.segmentation.contourindex", contourIdx);
  contourNode->SetColor(1.0f, 1.0f, 0.0f);
  contourNode->SetFloatProperty("pointsize", 3.0f);

  GetDataStorage()->Add(contourNode, m_CurrentContourGroupNode);
  UpdateContourList();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_LumenContouringView::DeleteContour()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::information(nullptr, "2D Segmentation", "Select a contour first.");
      return;
    }

    profileGroup->RemoveProfile(indices[static_cast<size_t>(row)]);
    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    return;
  }

  int row = m_ContourListWidget->currentRow();
  if (row < 0)
  {
    QMessageBox::information(nullptr, "2D Segmentation", "Select a contour first.");
    return;
  }

  mitk::DataStorage::SetOfObjects::ConstPointer children =
    GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);

  if (row < static_cast<int>(children->size()))
  {
    GetDataStorage()->Remove(children->GetElement(row));
    UpdateContourList();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_LumenContouringView::LoftContourGroup()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::information(nullptr, "2D Segmentation",
      "Select a contour group to loft.");
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    auto readinessReport = xq_SegmentationUtils::BuildReadinessReport(profileGroup);
    if (!readinessReport.loftReady)
    {
        QString errorMsg = QString("Cannot loft: group is not loft-ready.\n\nIssues:");
        for (const auto& err : readinessReport.errors)
            errorMsg += "\n- " + QString::fromStdString(err);
        QMessageBox::warning(nullptr, "Lofting", errorMsg);
        return;
    }
    if (!readinessReport.modelingReady)
    {
        QString warnMsg = QString("Lofting with warnings:");
        for (const auto& warn : readinessReport.warnings)
            warnMsg += "\n- " + QString::fromStdString(warn);
        QMessageBox::warning(nullptr, "Lofting", warnMsg);
    }

    auto loftedMesh = xq_SegmentationUtils::LoftProfileGroup(profileGroup);
    if (!loftedMesh || loftedMesh->GetNumberOfPoints() == 0)
    {
      QMessageBox::warning(nullptr, "Lofting",
        "At least two canonical profiles with valid contour points are required for lofting.");
      return;
    }

    profileGroup->SetLoftedMesh(loftedMesh);
    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Lofting",
      QString("Lofted mesh stored on profile group '%1' with %2 points.")
        .arg(QString::fromStdString(m_CurrentContourGroupNode->GetName()))
        .arg(loftedMesh->GetNumberOfPoints()));
    return;
  }

  mitk::DataStorage::SetOfObjects::ConstPointer children =
    GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);

  // Collect contour point sets (excluding any existing lofted surfaces)
  std::vector<mitk::PointSet::Pointer> contourSets;
  for (auto it = children->Begin(); it != children->End(); ++it)
  {
    bool isContour = false;
    it->Value()->GetBoolProperty("xq.segmentation.contour", isContour);
    if (!isContour) continue;

    auto* ps = dynamic_cast<mitk::PointSet*>(it->Value()->GetData());
    if (ps && ps->GetSize() >= 3)
      contourSets.push_back(ps);
  }

  if (contourSets.size() < 2)
  {
    QMessageBox::warning(nullptr, "Lofting",
      "At least two contours with 3+ points each are required for lofting.");
    return;
  }

  // Show loft parameter dialog
  QDialog paramDialog(nullptr);
  paramDialog.setWindowTitle("Lofting Parameters");
  QVBoxLayout* layout = new QVBoxLayout(&paramDialog);

  xq_LoftParamWidget* paramWidget = new xq_LoftParamWidget(&paramDialog);
  paramWidget->SetDefaults();
  layout->addWidget(paramWidget);

  QDialogButtonBox* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &paramDialog);
  layout->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, &paramDialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &paramDialog, &QDialog::reject);

  if (paramDialog.exec() != QDialog::Accepted)
    return;

  // Build VTK surface by connecting contour rings
  vtkSmartPointer<vtkPoints> allPoints = vtkSmartPointer<vtkPoints>::New();
  vtkSmartPointer<vtkCellArray> triangles = vtkSmartPointer<vtkCellArray>::New();

  int pointOffset = 0;
  std::vector<int> ringStartIndices;
  std::vector<int> ringSizes;

  // Resample each contour to same number of points for consistent lofting
  int samplesPerContour = paramWidget->GetSamplePerSection();
  if (samplesPerContour < 8) samplesPerContour = 36;

  for (size_t c = 0; c < contourSets.size(); ++c)
  {
    mitk::PointSet::Pointer ps = contourSets[c];
    int origSize = ps->GetSize();

    ringStartIndices.push_back(pointOffset);
    ringSizes.push_back(samplesPerContour);

    // Sample contour points uniformly
    for (int s = 0; s < samplesPerContour; ++s)
    {
      double t = static_cast<double>(s) / samplesPerContour * origSize;
      int idx0 = static_cast<int>(std::floor(t)) % origSize;
      int idx1 = (idx0 + 1) % origSize;
      double frac = t - std::floor(t);

      auto it0 = ps->Begin();
      std::advance(it0, idx0);
      auto it1 = ps->Begin();
      std::advance(it1, idx1);

      mitk::Point3D p0 = it0->Value();
      mitk::Point3D p1 = it1->Value();

      double x = p0[0] * (1.0 - frac) + p1[0] * frac;
      double y = p0[1] * (1.0 - frac) + p1[1] * frac;
      double z = p0[2] * (1.0 - frac) + p1[2] * frac;

      allPoints->InsertNextPoint(x, y, z);
      ++pointOffset;
    }
  }

  // Create triangle strips between consecutive contour rings
  for (size_t c = 0; c + 1 < contourSets.size(); ++c)
  {
    int start0 = ringStartIndices[c];
    int start1 = ringStartIndices[c + 1];
    int n = samplesPerContour;

    for (int i = 0; i < n; ++i)
    {
      int i0 = start0 + i;
      int i1 = start0 + (i + 1) % n;
      int i2 = start1 + i;
      int i3 = start1 + (i + 1) % n;

      vtkIdType tri1[3] = { i0, i3, i1 };
      triangles->InsertNextCell(3, tri1);

      vtkIdType tri2[3] = { i0, i2, i3 };
      triangles->InsertNextCell(3, tri2);
    }
  }

  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
  polyData->SetPoints(allPoints);
  polyData->SetPolys(triangles);

  // Create MITK surface and node
  mitk::Surface::Pointer surface = mitk::Surface::New();
  surface->SetVtkPolyData(polyData);

  mitk::DataNode::Pointer surfaceNode = mitk::DataNode::New();
  surfaceNode->SetData(surface);
  surfaceNode->SetName(m_CurrentContourGroupNode->GetName() + "_lofted");
  surfaceNode->SetColor(0.8f, 0.2f, 0.2f);
  surfaceNode->SetFloatProperty("opacity", 0.7f);

  surfaceNode->SetIntProperty("xq.loft.numSections", paramWidget->GetNumSections());
  surfaceNode->SetIntProperty("xq.loft.splineDegree", paramWidget->GetSplineDegree());
  surfaceNode->SetIntProperty("xq.loft.samplePerSection", paramWidget->GetSamplePerSection());
  surfaceNode->SetBoolProperty("xq.loft.linearSample", paramWidget->GetUseLinearSample());
  surfaceNode->SetBoolProperty("xq.loft.useFFT", paramWidget->GetUseFFT());
  surfaceNode->SetIntProperty("xq.loft.numOutputPoints", paramWidget->GetNumOutputPoints());

  GetDataStorage()->Add(surfaceNode, m_CurrentContourGroupNode);
  mitk::RenderingManager::GetInstance()->InitializeViews(surface->GetTimeGeometry());
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Lofting",
    QString("Lofted surface created with %1 triangles from %2 contours.")
      .arg(triangles->GetNumberOfCells())
      .arg(contourSets.size()));
}

void xq_LumenContouringView::OnContourTypeChanged(int index)
{
  switch (index)
  {
    case 0: SetCircleContour(); break;
    case 1: SetEllipseContour(); break;
    case 2: SetSplinePolygonContour(); break;
    case 3: SetManualContour(); break;
    default: break;
  }
}

void xq_LumenContouringView::OnPathSelectionChanged(int index)
{
  Q_UNUSED(index)
  PersistContourToolMetadata();
  UpdatePathReslice();
}

void xq_LumenContouringView::OnReslicePositionChanged(int index)
{
  if (m_CurrentSlicedGeometry.IsNull() || !m_ResliceSlider || !m_ResliceLabel)
    return;

  const int maxIndex = m_ResliceSlider->maximum();
  index = std::max(0, std::min(index, maxIndex));
  m_ResliceLabel->setText(QString("%1 / %2").arg(index + 1).arg(maxIndex + 1));
  if (m_CurrentContourGroupNode.IsNotNull())
    m_CurrentContourGroupNode->SetIntProperty(
      "xq.segmentation.current_reslice_index", index);

  auto* renderWindowPart = GetRenderWindowPart(mitk::WorkbenchUtil::OPEN);
  if (!renderWindowPart)
    return;

  for (const auto& windowName : {QString("axial"), QString("sagittal")})
  {
    auto* renderWindow = renderWindowPart->GetQmitkRenderWindow(windowName);
    if (!renderWindow)
      continue;

    auto* controller = renderWindow->GetSliceNavigationController();
    if (!controller)
      continue;

    controller->SetInputWorldTimeGeometry(m_CurrentSlicedGeometry);
    controller->SetViewDirection(mitk::AnatomicalPlane::Original);
    controller->Update();
    controller->GetStepper()->SetPos(index);
    controller->SendSlice();
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_LumenContouringView::UpdatePathReslice()
{
  if (!m_PathComboBox || !m_ResliceSlider || !m_ResliceLabel || !m_ResliceSizeSpin)
    return;

  const auto pathName = m_PathComboBox->currentText().toStdString();
  auto pathNode = FindPathNodeByName(GetDataStorage(), pathName);
  auto* centerline = pathNode.IsNotNull()
    ? dynamic_cast<xq_VesselCenterline*>(pathNode->GetData())
    : nullptr;
  const auto frames = BuildPlacementFrames(centerline);

  if (frames.empty())
  {
    m_CurrentSlicedGeometry = nullptr;
    m_ResliceSlider->setEnabled(false);
    m_ResliceSlider->setMinimum(0);
    m_ResliceSlider->setMaximum(0);
    m_ResliceLabel->setText("-- / --");
    return;
  }

  mitk::BaseData* baseData = FindSelectedImage().GetPointer();
  if (!baseData && pathNode.IsNotNull())
    baseData = pathNode->GetData();

  m_CurrentSlicedGeometry = xq_SegmentationUtils::CreateSlicedGeometry(
    frames, baseData, m_ResliceSizeSpin->value(), true);

  m_ResliceSlider->setEnabled(true);
  m_ResliceSlider->setMinimum(0);
  m_ResliceSlider->setMaximum(static_cast<int>(frames.size()) - 1);
  if (m_ResliceSlider->value() > m_ResliceSlider->maximum())
    m_ResliceSlider->setValue(0);

  OnReslicePositionChanged(m_ResliceSlider->value());
}

void xq_LumenContouringView::UpdateContourList()
{
  const QSignalBlocker blocker(m_ContourListWidget);
  m_ContourListWidget->setUpdatesEnabled(false);
  m_ContourListWidget->clear();

  if (m_CurrentContourGroupNode.IsNull())
  {
    m_ContourListWidget->setUpdatesEnabled(true);
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const auto indices = profileGroup->GetProfilePathIndices();
    const auto unresolvedPathIndices = profileGroup->GetUnresolvedProfilePathIndices();
    const std::set<int> unresolvedIndices(
      unresolvedPathIndices.begin(),
      unresolvedPathIndices.end());
    for (const auto pathPosIndex : indices)
    {
      auto* profile = profileGroup->GetProfileAtPathPos(pathPosIndex);
      if (!profile)
        continue;

      QString label = QString("Profile %1").arg(pathPosIndex);
      const auto method = QString::fromStdString(profile->GetMethod());
      if (!method.isEmpty())
        label += QString(" [%1]").arg(method);
      if (unresolvedIndices.count(pathPosIndex) != 0)
        label += " [needs-placement]";

      m_ContourListWidget->addItem(label);
    }
    m_ContourListWidget->setUpdatesEnabled(true);
    return;
  }

  mitk::DataStorage::SetOfObjects::ConstPointer children =
    GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);

  for (auto it = children->Begin(); it != children->End(); ++it)
  {
    std::string contourType;
    it->Value()->GetStringProperty("xq.segmentation.contourtype", contourType);

    QString label = QString::fromStdString(it->Value()->GetName());
    if (!contourType.empty())
      label += QString(" [%1]").arg(QString::fromStdString(contourType));

    m_ContourListWidget->addItem(label);
  }
  m_ContourListWidget->setUpdatesEnabled(true);
}

void xq_LumenContouringView::SetCircleContour()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  m_CurrentContourGroupNode->SetStringProperty(
    "xq.segmentation.activetype", "Circle");
}

void xq_LumenContouringView::SetEllipseContour()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  m_CurrentContourGroupNode->SetStringProperty(
    "xq.segmentation.activetype", "Ellipse");
}

void xq_LumenContouringView::SetSplinePolygonContour()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  m_CurrentContourGroupNode->SetStringProperty(
    "xq.segmentation.activetype", "SplinePolygon");
}

void xq_LumenContouringView::SetManualContour()
{
  if (m_CurrentContourGroupNode.IsNull())
    return;

  m_CurrentContourGroupNode->SetStringProperty(
    "xq.segmentation.activetype", "Manual");
}

void xq_LumenContouringView::CopyContour()
{
  if (m_CurrentContourGroupNode.IsNull()) return;

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::warning(nullptr, "Copy Contour", "No contour selected to copy.");
      return;
    }

    auto* sourceProfile = profileGroup->GetProfileAtPathPos(indices[static_cast<size_t>(row)]);
    if (!sourceProfile)
    {
      QMessageBox::warning(nullptr, "Copy Contour", "Selected profile is unavailable.");
      return;
    }

    auto clipboardGroup = xq_ProfileGroup::New();
    clipboardGroup->AppendProfile(sourceProfile->Duplicate().release());

    mitk::DataNode::Pointer copyNode = mitk::DataNode::New();
    copyNode->SetData(clipboardGroup);
    copyNode->SetName(("profile_copy_" + std::to_string(indices[static_cast<size_t>(row)])).c_str());
    m_CopiedContourNode = copyNode;

    QMessageBox::information(nullptr, "Copy Contour",
      QString("Copied profile at path position %1").arg(indices[static_cast<size_t>(row)]));
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  // Find the contour node at current index
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);
  if (!children || m_CurrentContourIndex < 0 ||
      static_cast<unsigned int>(m_CurrentContourIndex) >= children->Size())
  {
    QMessageBox::warning(nullptr, "Copy Contour", "No contour selected to copy.");
    return;
  }

  mitk::DataNode::Pointer sourceNode = children->GetElement(m_CurrentContourIndex);
  auto* sourcePS = dynamic_cast<mitk::PointSet*>(sourceNode->GetData());
  if (!sourcePS || sourcePS->GetSize() == 0)
  {
    QMessageBox::warning(nullptr, "Copy Contour", "Selected contour has no points.");
    return;
  }

  // Deep copy the pointset
  mitk::PointSet::Pointer copy = mitk::PointSet::New();
  for (auto it = sourcePS->Begin(); it != sourcePS->End(); ++it)
    copy->InsertPoint(it->Index(), it->Value());

  mitk::DataNode::Pointer copyNode = mitk::DataNode::New();
  copyNode->SetData(copy);
  copyNode->SetName(sourceNode->GetName() + "_copy");
  m_CopiedContourNode = copyNode;

  QMessageBox::information(nullptr, "Copy Contour",
    QString("Copied contour \"%1\" (%2 points)")
      .arg(QString::fromStdString(sourceNode->GetName()))
      .arg(sourcePS->GetSize()));
}

void xq_LumenContouringView::PasteContour()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Paste Contour", "No contour group selected.");
    return;
  }
  if (m_CopiedContourNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Paste Contour", "No contour copied. Use Copy first.");
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    auto* clipboardGroup = GetProfileGroup(m_CopiedContourNode);
    if (!clipboardGroup || clipboardGroup->GetProfileCount() == 0)
    {
      QMessageBox::warning(nullptr, "Paste Contour",
        "Clipboard does not contain a canonical profile copy.");
      return;
    }

    const auto clipboardIndices = clipboardGroup->GetProfilePathIndices();
    auto* sourceProfile = clipboardIndices.empty()
      ? nullptr
      : clipboardGroup->GetProfileAtPathPos(clipboardIndices.front());
    if (!sourceProfile)
    {
      QMessageBox::warning(nullptr, "Paste Contour",
        "Clipboard profile is unavailable.");
      return;
    }

    const mitk::PlaneGeometry* currentPlane = GetCurrentSlicePlaneGeometry();
    xq_ProfilePlacementFrame placementFrame;
    const bool hasPlacementFrame = ResolvePlacementFrame(
      GetDataStorage(), profileGroup, currentPlane, placementFrame);
    const bool requiresPathPlacement = xq_SegmentationUtils::RequiresPathPlacement(profileGroup);

    if (requiresPathPlacement && !hasPlacementFrame)
    {
      QMessageBox::warning(nullptr, "Paste Contour",
        "Cannot resolve the current centerline placement. Select a valid vessel path slice before pasting a contour.");
      return;
    }

    double zOffset = 0.0;
    if (!hasPlacementFrame)
    {
      bool ok;
      zOffset = QInputDialog::getDouble(nullptr, "Paste Contour",
        "Z offset for pasted contour:", 1.0, -1000.0, 1000.0, 2, &ok);
      if (!ok) return;
    }

    auto pastedProfile = xq_SegmentationUtils::CloneProfileWithOffset(sourceProfile, zOffset);
    if (!pastedProfile)
    {
      QMessageBox::warning(nullptr, "Paste Contour", "Failed to clone pasted profile.");
      return;
    }

    int targetIndex = -1;
    if (!xq_SegmentationUtils::ResolveCanonicalPathPosIndex(
          profileGroup, hasPlacementFrame ? &placementFrame : nullptr, targetIndex))
    {
      QMessageBox::warning(nullptr, "Paste Contour",
        "Cannot resolve the current centerline placement. Select a valid vessel path slice before pasting a contour.");
      return;
    }

    if (hasPlacementFrame)
    {
      xq_SegmentationUtils::ApplyPlacementFrame(
        pastedProfile.get(), placementFrame, currentPlane);
    }
    else
    {
      pastedProfile->SetPathPosIndex(targetIndex);
    }

    if (profileGroup->HasProfile(targetIndex))
    {
      profileGroup->ReplaceProfile(pastedProfile.release(), targetIndex);
    }
    else
    {
      profileGroup->AppendProfile(pastedProfile.release(), targetIndex);
    }

    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    const int row = GetProfileListRow(profileGroup, targetIndex);
    if (row >= 0)
      m_ContourListWidget->setCurrentRow(row);
    EnsureProfileInteractor(targetIndex);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  // Ask for Z offset
  bool ok;
  double zOffset = QInputDialog::getDouble(nullptr, "Paste Contour",
    "Z offset for pasted contour:", 1.0, -1000.0, 1000.0, 2, &ok);
  if (!ok) return;

  auto* sourcePS = dynamic_cast<mitk::PointSet*>(m_CopiedContourNode->GetData());
  if (!sourcePS) return;

  // Create new pointset with offset
  mitk::PointSet::Pointer newPS = mitk::PointSet::New();
  for (auto it = sourcePS->Begin(); it != sourcePS->End(); ++it)
  {
    mitk::PointSet::PointType pt = it->Value();
    pt[2] += zOffset;
    newPS->InsertPoint(it->Index(), pt);
  }

  mitk::DataNode::Pointer newNode = mitk::DataNode::New();
  newNode->SetData(newPS);

  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);
  int idx = children ? children->Size() : 0;
  newNode->SetName("Contour_" + std::to_string(idx));
  newNode->SetColor(0.0f, 1.0f, 0.0f);

  ds->Add(newNode, m_CurrentContourGroupNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdateContourList();
}

void xq_LumenContouringView::ScaleContour()
{
  if (m_CurrentContourGroupNode.IsNull()) return;

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::warning(nullptr, "Scale Contour", "No contour selected.");
      return;
    }

    bool ok;
    double factor = QInputDialog::getDouble(nullptr, "Scale Contour",
      "Scale factor:", 1.0, 0.01, 100.0, 3, &ok);
    if (!ok || factor <= 0.0) return;

    const int pathPosIndex = indices[static_cast<size_t>(row)];
    auto* sourceProfile = profileGroup->GetProfileAtPathPos(pathPosIndex);
    auto scaledProfile = xq_SegmentationUtils::ScaleProfile(sourceProfile, factor);
    if (!scaledProfile)
    {
      QMessageBox::warning(nullptr, "Scale Contour", "Failed to scale selected profile.");
      return;
    }

    profileGroup->ReplaceProfile(scaledProfile.release(), pathPosIndex);
    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    m_ContourListWidget->setCurrentRow(row);
    EnsureProfileInteractor(pathPosIndex);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    QMessageBox::information(nullptr, "Scale Contour",
      QString("Scaled profile by factor %1").arg(factor, 0, 'f', 3));
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);
  if (!children || m_CurrentContourIndex < 0 ||
      static_cast<unsigned int>(m_CurrentContourIndex) >= children->Size())
  {
    QMessageBox::warning(nullptr, "Scale Contour", "No contour selected.");
    return;
  }

  bool ok;
  double factor = QInputDialog::getDouble(nullptr, "Scale Contour",
    "Scale factor:", 1.0, 0.01, 100.0, 3, &ok);
  if (!ok || factor <= 0.0) return;

  mitk::DataNode::Pointer node = children->GetElement(m_CurrentContourIndex);
  auto* ps = dynamic_cast<mitk::PointSet*>(node->GetData());
  if (!ps || ps->GetSize() == 0) return;

  // Compute centroid
  mitk::PointSet::PointType centroid;
  centroid.Fill(0.0);
  int count = 0;
  for (auto it = ps->Begin(); it != ps->End(); ++it)
  {
    mitk::PointSet::PointType pt = it->Value();
    centroid[0] += pt[0];
    centroid[1] += pt[1];
    centroid[2] += pt[2];
    count++;
  }
  if (count > 0)
  {
    centroid[0] /= count;
    centroid[1] /= count;
    centroid[2] /= count;
  }

  // Scale around centroid
  mitk::PointSet::Pointer scaled = mitk::PointSet::New();
  for (auto it = ps->Begin(); it != ps->End(); ++it)
  {
    mitk::PointSet::PointType pt = it->Value();
    pt[0] = centroid[0] + (pt[0] - centroid[0]) * factor;
    pt[1] = centroid[1] + (pt[1] - centroid[1]) * factor;
    pt[2] = centroid[2] + (pt[2] - centroid[2]) * factor;
    scaled->InsertPoint(it->Index(), pt);
  }

  node->SetData(scaled);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Scale Contour",
    QString("Scaled contour by factor %1").arg(factor, 0, 'f', 3));
}

void xq_LumenContouringView::SmoothContour()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Smooth Contour", "No contour group selected.");
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::warning(nullptr, "Smooth Contour", "No contour selected.");
      return;
    }

    const int pathPosIndex = indices[static_cast<size_t>(row)];
    auto* sourceProfile = profileGroup->GetProfileAtPathPos(pathPosIndex);
    if (!sourceProfile)
    {
      QMessageBox::warning(nullptr, "Smooth Contour", "Selected profile is unavailable.");
      return;
    }

    const auto kind = sourceProfile->GetProfileKind();
    if (kind == "Circle" || kind == "Ellipse")
    {
      QMessageBox::information(nullptr, "Smooth Contour",
        "Canonical smoothing is currently intended for manual/spline profiles. "
        "Use resample or edit the primitive parameters for circle/ellipse profiles.");
      return;
    }

    int iterations = m_Ui->spinSmoothIterations->value();
    double relaxation = m_Ui->spinSmoothFactor->value();
    auto smoothedProfile = xq_SegmentationUtils::SmoothProfile(sourceProfile, iterations, relaxation);
    if (!smoothedProfile)
    {
      QMessageBox::warning(nullptr, "Smooth Contour", "Failed to smooth selected profile.");
      return;
    }

    profileGroup->ReplaceProfile(smoothedProfile.release(), pathPosIndex);
    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    m_ContourListWidget->setCurrentRow(row);
    EnsureProfileInteractor(pathPosIndex);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Smooth Contour",
      QString("Profile smoothed: %1 iterations, factor=%2")
        .arg(iterations).arg(relaxation, 0, 'f', 2));
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  int row = m_ContourListWidget->currentRow();
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);

  if (row < 0 || !children || static_cast<unsigned int>(row) >= children->Size())
  {
    QMessageBox::warning(nullptr, "Smooth Contour", "No contour selected.");
    return;
  }

  int iterations = m_Ui->spinSmoothIterations->value();
  double relaxation = m_Ui->spinSmoothFactor->value();

  mitk::DataNode::Pointer node = children->GetElement(row);

  // Check if the node holds a Surface (vtkPolyData)
  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  if (surface && surface->GetVtkPolyData())
  {
    vtkSmartPointer<vtkSmoothPolyDataFilter> smoother =
      vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    smoother->SetInputData(surface->GetVtkPolyData());
    smoother->SetNumberOfIterations(iterations);
    smoother->SetRelaxationFactor(relaxation);
    smoother->Update();

    mitk::Surface::Pointer result = mitk::Surface::New();
    result->SetVtkPolyData(smoother->GetOutput());
    node->SetData(result);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Smooth Contour",
      QString("Contour smoothed: %1 iterations, factor=%2")
        .arg(iterations).arg(relaxation, 0, 'f', 2));
    return;
  }

  // Handle PointSet contour: convert to polyline, smooth, convert back
  auto* ps = dynamic_cast<mitk::PointSet*>(node->GetData());
  if (!ps || ps->GetSize() < 3)
  {
    QMessageBox::warning(nullptr, "Smooth Contour",
      "Selected contour has insufficient points (need >= 3).");
    return;
  }

  int numPts = ps->GetSize();

  // Build a closed polyline vtkPolyData from the PointSet
  vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
  for (auto it = ps->Begin(); it != ps->End(); ++it)
  {
    mitk::Point3D p = it->Value();
    pts->InsertNextPoint(p[0], p[1], p[2]);
  }

  vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
  vtkSmartPointer<vtkPolyData> polyLine = vtkSmartPointer<vtkPolyData>::New();

  // Create line segments connecting consecutive points (open polyline;
  // closing segment omitted to prevent vtkSmoothPolyDataFilter from
  // producing a duplicate vertex at the wrap-around)
  for (int i = 0; i < numPts - 1; ++i)
  {
    vtkIdType seg[2] = { i, i + 1 };
    lines->InsertNextCell(2, seg);
  }

  polyLine->SetPoints(pts);
  polyLine->SetLines(lines);

  vtkSmartPointer<vtkSmoothPolyDataFilter> smoother =
    vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
  smoother->SetInputData(polyLine);
  smoother->SetNumberOfIterations(iterations);
  smoother->SetRelaxationFactor(relaxation);
  smoother->Update();

  // Extract smoothed points back to a PointSet
  vtkPolyData* smoothed = smoother->GetOutput();
  mitk::PointSet::Pointer result = mitk::PointSet::New();
  for (vtkIdType i = 0; i < smoothed->GetNumberOfPoints(); ++i)
  {
    double coords[3];
    smoothed->GetPoint(i, coords);
    mitk::Point3D mp;
    mp[0] = coords[0];
    mp[1] = coords[1];
    mp[2] = coords[2];
    result->InsertPoint(static_cast<int>(i), mp);
  }

  node->SetData(result);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Smooth Contour",
    QString("Contour smoothed: %1 iterations, factor=%2")
      .arg(iterations).arg(relaxation, 0, 'f', 2));
}

void xq_LumenContouringView::ResampleContour()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Resample Contour", "No contour group selected.");
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::warning(nullptr, "Resample Contour", "No contour selected.");
      return;
    }

    int targetPoints = m_Ui->spinResamplePoints->value();
    const int pathPosIndex = indices[static_cast<size_t>(row)];
    auto* sourceProfile = profileGroup->GetProfileAtPathPos(pathPosIndex);
    auto resampledProfile = xq_SegmentationUtils::ResampleProfile(sourceProfile, targetPoints);
    if (!resampledProfile)
    {
      QMessageBox::warning(nullptr, "Resample Contour", "Failed to resample selected profile.");
      return;
    }

    const int actualPoints = static_cast<int>(resampledProfile->GetProfilePoints().size());
    profileGroup->ReplaceProfile(resampledProfile.release(), pathPosIndex);
    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    m_ContourListWidget->setCurrentRow(row);
    EnsureProfileInteractor(pathPosIndex);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Resample Contour",
      QString("Profile resampled to %1 points").arg(actualPoints));
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  int row = m_ContourListWidget->currentRow();
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);

  if (row < 0 || !children || static_cast<unsigned int>(row) >= children->Size())
  {
    QMessageBox::warning(nullptr, "Resample Contour", "No contour selected.");
    return;
  }

  int targetPoints = m_Ui->spinResamplePoints->value();
  mitk::DataNode::Pointer node = children->GetElement(row);

  // Check if the node holds a Surface
  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  vtkSmartPointer<vtkPolyData> inputPoly;
  bool isSurface = false;

  if (surface && surface->GetVtkPolyData())
  {
    inputPoly = surface->GetVtkPolyData();
    isSurface = true;
  }
  else
  {
    // Convert PointSet to polyline
    auto* ps = dynamic_cast<mitk::PointSet*>(node->GetData());
    if (!ps || ps->GetSize() < 2)
    {
      QMessageBox::warning(nullptr, "Resample Contour",
        "Selected contour has insufficient points (need >= 2).");
      return;
    }

    int numPts = ps->GetSize();
    vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
    for (auto it = ps->Begin(); it != ps->End(); ++it)
    {
      mitk::Point3D p = it->Value();
      pts->InsertNextPoint(p[0], p[1], p[2]);
    }

    // Build an open polyline (vtkSplineFilter needs a single polyline cell)
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    lines->InsertNextCell(numPts + 1);
    for (int i = 0; i < numPts; ++i)
      lines->InsertCellPoint(i);
    lines->InsertCellPoint(0); // close the loop

    inputPoly = vtkSmartPointer<vtkPolyData>::New();
    inputPoly->SetPoints(pts);
    inputPoly->SetLines(lines);
  }

  // Compute subdivisions to achieve approximate target point count
  int numInputPts = static_cast<int>(inputPoly->GetNumberOfPoints());
  int numCells = static_cast<int>(inputPoly->GetNumberOfLines());
  int subdivs = 1;
  if (numCells > 0)
    subdivs = std::max(1, targetPoints / numCells);

  vtkSmartPointer<vtkSplineFilter> spline =
    vtkSmartPointer<vtkSplineFilter>::New();
  spline->SetInputData(inputPoly);
  spline->SetSubdivideToSpecified();
  spline->SetNumberOfSubdivisions(subdivs);
  spline->Update();

  vtkPolyData* resampled = spline->GetOutput();
  int actualPoints = static_cast<int>(resampled->GetNumberOfPoints());

  if (isSurface)
  {
    mitk::Surface::Pointer result = mitk::Surface::New();
    result->SetVtkPolyData(resampled);
    node->SetData(result);
  }
  else
  {
    mitk::PointSet::Pointer result = mitk::PointSet::New();
    for (vtkIdType i = 0; i < resampled->GetNumberOfPoints(); ++i)
    {
      double coords[3];
      resampled->GetPoint(i, coords);
      mitk::Point3D mp;
      mp[0] = coords[0];
      mp[1] = coords[1];
      mp[2] = coords[2];
      result->InsertPoint(static_cast<int>(i), mp);
    }
    node->SetData(result);
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Resample Contour",
    QString("Contour resampled to %1 points").arg(actualPoints));
}

void xq_LumenContouringView::ShowContourStatistics()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Contour Statistics", "No contour group selected.");
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const int row = m_ContourListWidget->currentRow();
    const auto indices = profileGroup->GetProfilePathIndices();
    if (row < 0 || row >= static_cast<int>(indices.size()))
    {
      QMessageBox::warning(nullptr, "Contour Statistics", "No contour selected.");
      return;
    }

    const int pathPosIndex = indices[static_cast<size_t>(row)];
    auto* profile = profileGroup->GetProfileAtPathPos(pathPosIndex);
    const auto stats = xq_SegmentationUtils::ComputeProfileStatistics(profile);
    if (stats.pointCount == 0)
    {
      QMessageBox::warning(nullptr, "Contour Statistics", "Profile has no contour points.");
      return;
    }

    QString msg = QString(
      "Profile: %1\n\n"
      "Kind: %2\n"
      "Method: %3\n"
      "Number of Points: %4\n"
      "Perimeter: %5 mm\n"
      "Enclosed Area: %6 mm²\n\n"
      "Bounding Box:\n"
      "  X: [%7, %8] (width: %9 mm)\n"
      "  Y: [%10, %11] (height: %12 mm)\n"
      "  Z: [%13, %14] (depth: %15 mm)")
      .arg(pathPosIndex)
      .arg(QString::fromStdString(profile->GetProfileKind()))
      .arg(QString::fromStdString(profile->GetMethod()))
      .arg(stats.pointCount)
      .arg(stats.perimeter, 0, 'f', 3)
      .arg(stats.area, 0, 'f', 3)
      .arg(stats.boundingBox[0], 0, 'f', 3).arg(stats.boundingBox[1], 0, 'f', 3)
      .arg(stats.boundingBox[1] - stats.boundingBox[0], 0, 'f', 3)
      .arg(stats.boundingBox[2], 0, 'f', 3).arg(stats.boundingBox[3], 0, 'f', 3)
      .arg(stats.boundingBox[3] - stats.boundingBox[2], 0, 'f', 3)
      .arg(stats.boundingBox[4], 0, 'f', 3).arg(stats.boundingBox[5], 0, 'f', 3)
      .arg(stats.boundingBox[5] - stats.boundingBox[4], 0, 'f', 3);

    QMessageBox::information(nullptr, "Contour Statistics", msg);
    return;
  }

  mitk::DataStorage::Pointer ds = GetDataStorage();
  if (ds.IsNull()) return;

  int row = m_ContourListWidget->currentRow();
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    ds->GetDerivations(m_CurrentContourGroupNode);

  if (row < 0 || !children || static_cast<unsigned int>(row) >= children->Size())
  {
    QMessageBox::warning(nullptr, "Contour Statistics", "No contour selected.");
    return;
  }

  mitk::DataNode::Pointer node = children->GetElement(row);

  // Collect 3D points from either Surface or PointSet
  std::vector<mitk::Point3D> points;

  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  if (surface && surface->GetVtkPolyData())
  {
    vtkPolyData* pd = surface->GetVtkPolyData();
    for (vtkIdType i = 0; i < pd->GetNumberOfPoints(); ++i)
    {
      double coords[3];
      pd->GetPoint(i, coords);
      mitk::Point3D mp;
      mp[0] = coords[0];
      mp[1] = coords[1];
      mp[2] = coords[2];
      points.push_back(mp);
    }
  }
  else
  {
    auto* ps = dynamic_cast<mitk::PointSet*>(node->GetData());
    if (ps)
    {
      for (auto it = ps->Begin(); it != ps->End(); ++it)
        points.push_back(it->Value());
    }
  }

  if (points.empty())
  {
    QMessageBox::warning(nullptr, "Contour Statistics", "Contour has no points.");
    return;
  }

  int numPoints = static_cast<int>(points.size());

  // Perimeter (total edge length of closed contour)
  double perimeter = 0.0;
  for (int i = 0; i < numPoints; ++i)
  {
    const mitk::Point3D& p0 = points[i];
    const mitk::Point3D& p1 = points[(i + 1) % numPoints];
    double dx = p1[0] - p0[0];
    double dy = p1[1] - p0[1];
    double dz = p1[2] - p0[2];
    perimeter += std::sqrt(dx * dx + dy * dy + dz * dz);
  }

  // Enclosed area using Shoelace formula on 2D projection (X-Y plane)
  double area = 0.0;
  for (int i = 0; i < numPoints; ++i)
  {
    const mitk::Point3D& p0 = points[i];
    const mitk::Point3D& p1 = points[(i + 1) % numPoints];
    area += p0[0] * p1[1] - p1[0] * p0[1];
  }
  area = std::fabs(area) * 0.5;

  // Bounding box
  double bbMin[3] = { points[0][0], points[0][1], points[0][2] };
  double bbMax[3] = { points[0][0], points[0][1], points[0][2] };
  for (int i = 1; i < numPoints; ++i)
  {
    for (int d = 0; d < 3; ++d)
    {
      if (points[i][d] < bbMin[d]) bbMin[d] = points[i][d];
      if (points[i][d] > bbMax[d]) bbMax[d] = points[i][d];
    }
  }

  QString msg = QString(
    "Contour: %1\n\n"
    "Number of Points: %2\n"
    "Perimeter: %3 mm\n"
    "Enclosed Area (XY projection): %4 mm²\n\n"
    "Bounding Box:\n"
    "  X: [%5, %6] (width: %7 mm)\n"
    "  Y: [%8, %9] (height: %10 mm)\n"
    "  Z: [%11, %12] (depth: %13 mm)")
    .arg(QString::fromStdString(node->GetName()))
    .arg(numPoints)
    .arg(perimeter, 0, 'f', 3)
    .arg(area, 0, 'f', 3)
    .arg(bbMin[0], 0, 'f', 3).arg(bbMax[0], 0, 'f', 3).arg(bbMax[0] - bbMin[0], 0, 'f', 3)
    .arg(bbMin[1], 0, 'f', 3).arg(bbMax[1], 0, 'f', 3).arg(bbMax[1] - bbMin[1], 0, 'f', 3)
    .arg(bbMin[2], 0, 'f', 3).arg(bbMax[2], 0, 'f', 3).arg(bbMax[2] - bbMin[2], 0, 'f', 3);

  QMessageBox::information(nullptr, "Contour Statistics", msg);
}

// ---------------------------------------------------------------------------
// FindSelectedImage — locate the first mitk::Image in the data storage
// ---------------------------------------------------------------------------

mitk::Image::Pointer xq_LumenContouringView::FindSelectedImage()
{
  auto node = FindSelectedImageNode();
  if (node.IsNull())
    return nullptr;
  return dynamic_cast<mitk::Image*>(node->GetData());
}

mitk::DataNode::Pointer xq_LumenContouringView::FindSelectedImageNode()
{
  auto ds = GetDataStorage();
  if (ds.IsNull()) return nullptr;

  std::string sourceImageName;
  if (m_CurrentContourGroupNode.IsNotNull() &&
      m_CurrentContourGroupNode->GetStringProperty(
        xq::pipeline::kSourceImageProperty, sourceImageName) &&
      !sourceImageName.empty())
  {
    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      const auto& node = it->Value();
      if (node.IsNotNull() &&
          node->GetName() == sourceImageName &&
          dynamic_cast<mitk::Image*>(node->GetData()) != nullptr)
      {
        return node;
      }
    }
    return nullptr;
  }

  auto predicate = mitk::TNodePredicateDataType<mitk::Image>::New();
  auto nodes = ds->GetSubset(predicate);
  mitk::DataNode::Pointer visibleImageNode = nullptr;
  for (auto it = nodes->Begin(); it != nodes->End(); ++it)
  {
    const auto& node = it->Value();
    if (node.IsNotNull() && node->IsVisible(nullptr) &&
        dynamic_cast<mitk::Image*>(node->GetData()) != nullptr)
    {
      if (visibleImageNode.IsNotNull())
        return nullptr;
      visibleImageNode = node;
    }
  }
  return visibleImageNode;
}

// ---------------------------------------------------------------------------
// PerformAutoSegmentation — threshold or region-growing on the current image
// ---------------------------------------------------------------------------

void xq_LumenContouringView::PerformAutoSegmentation()
{
  mitk::Image::Pointer image = FindSelectedImage();
  if (image.IsNull())
  {
    QMessageBox::warning(nullptr, "Auto Segmentation",
                         "No unambiguous image is bound to this contour group. "
                         "Select a contour group with xq.source.image metadata "
                         "or keep exactly one image visible.");
    return;
  }

  vtkImageData* vtk = image->GetVtkImageData();
  if (!vtk)
  {
    QMessageBox::warning(nullptr, "Auto Segmentation",
                         "Cannot obtain VTK image data from the selected image.");
    return;
  }

  const double lo = m_Ui->spinLowerThresh->value();
  const double hi = m_Ui->spinUpperThresh->value();

  vtkSmartPointer<vtkPolyData> result;

  if (m_Ui->rbRegionGrow->isChecked())
  {
    // For region growing we use the image center as a default seed
    double bounds[6];
    vtk->GetBounds(bounds);
    mitk::Point3D seed;
    seed[0] = (bounds[0] + bounds[1]) * 0.5;
    seed[1] = (bounds[2] + bounds[3]) * 0.5;
    seed[2] = (bounds[4] + bounds[5]) * 0.5;

    std::vector<mitk::Point3D> seeds{seed};
    result = xq_Seg3DUtils::RegionGrowingSegmentation(vtk, seeds, lo, hi);
  }
  else
  {
    result = xq_Seg3DUtils::ThresholdSegmentation(vtk, lo, hi);
  }

  if (!result || result->GetNumberOfPoints() == 0)
  {
    QMessageBox::information(nullptr, "Auto Segmentation",
                             "Segmentation produced no surface.  Try adjusting thresholds.");
    return;
  }

  // Optional post-processing
  if (m_Ui->cbAutoSmooth->isChecked())
    result = xq_Seg3DUtils::SmoothSurface(result, 15, 0.12);

  if (m_Ui->cbAutoDecimate->isChecked())
    result = xq_Seg3DUtils::DecimateSurface(result, 0.4);

  result = xq_Seg3DUtils::ComputeNormals(result);

  if (m_CurrentContourGroupNode.IsNotNull())
  {
    m_CurrentContourGroupNode->SetDoubleProperty("xq.segmentation.threshold.min", lo);
    m_CurrentContourGroupNode->SetDoubleProperty("xq.segmentation.threshold.max", hi);
    if (auto imageNode = FindSelectedImageNode(); imageNode.IsNotNull())
    {
      m_CurrentContourGroupNode->SetStringProperty(
        xq::pipeline::kSourceImageProperty, imageNode->GetName().c_str());
    }
  }

  const mitk::PlaneGeometry* planeGeo = GetCurrentSlicePlaneGeometry();
  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    const auto placementFrames = BuildPlacementFrames(
      FindPathForProfileGroup(GetDataStorage(), profileGroup));
    xq_ProfilePlacementFrame placementFrame;
    const auto preWritebackDecision =
      xq_SegmentationUtils::ResolveAutoSegmentationWritebackDecisionForPlacementFrames(
        profileGroup, planeGeo, placementFrames, &placementFrame);
    const bool hasPlacementFrame = placementFrame.pathPosIndex >= 0;
    if (preWritebackDecision.ShouldWarn())
    {
      QMessageBox::warning(nullptr, "Auto Segmentation",
        QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
          xq_CanonicalWritebackWarningContext::AutoSegmentation,
          preWritebackDecision.warningReason).data()));
    }

    if (preWritebackDecision.ShouldAttemptCanonicalWriteback() && planeGeo != nullptr)
    {
      const auto contourPoints = xq_SegmentationUtils::ExtractSurfaceContourOnPlane(
        result, planeGeo);
      if (!contourPoints.empty())
      {
        int contourIdx = -1;
        const auto method = m_Ui->rbRegionGrow->isChecked()
          ? std::string_view("region-grow")
          : std::string_view("threshold-3d");

        if (InsertCanonicalProfile(
              GetDataStorage(), profileGroup, m_ContourListWidget, m_ProfileInteractor,
              contourPoints, planeGeo, method, contourIdx))
        {
          UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
          UpdateContourList();
          const int row = GetProfileListRow(profileGroup, contourIdx);
          if (row >= 0)
            m_ContourListWidget->setCurrentRow(row);
          EnsureProfileInteractor(contourIdx);
        }
        else
        {
          const auto failedWritebackDecision =
            xq_SegmentationUtils::ResolveAutoSegmentationWritebackDecisionForPlacementFrames(
              profileGroup, planeGeo, placementFrames, &placementFrame);
          if (failedWritebackDecision.ShouldWarn())
          {
            QMessageBox::warning(nullptr, "Auto Segmentation",
              QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
                xq_CanonicalWritebackWarningContext::AutoSegmentation,
                failedWritebackDecision.warningReason).data()));
          }
          else
          {
            QMessageBox::warning(nullptr, "Auto Segmentation",
              "Failed to write the segmentation contour back into the canonical profile group.");
          }
        }
      }
      else
      {
        const auto extractionDecision =
          xq_SegmentationUtils::GetExtractionFailedWritebackDecision();
        QMessageBox::warning(nullptr, "Auto Segmentation",
          QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
            xq_CanonicalWritebackWarningContext::AutoSegmentation,
            extractionDecision.warningReason).data()));
      }
    }
  }

  // Create a MITK surface and add to data storage
  mitk::Surface::Pointer surface = mitk::Surface::New();
  surface->SetVtkPolyData(result);

  mitk::DataNode::Pointer node = mitk::DataNode::New();
  node->SetData(surface);
  node->SetName("AutoSeg_Result");
  node->SetProperty("color", mitk::ColorProperty::New(0.8f, 0.2f, 0.2f));
  node->SetProperty("opacity", mitk::FloatProperty::New(0.6f));
  node->SetStringProperty("xq.segmentation.method",
    m_Ui->rbRegionGrow->isChecked() ? "RegionGrowing" : "Threshold3D");

  if (m_CurrentContourGroupNode.IsNotNull())
    GetDataStorage()->Add(node, m_CurrentContourGroupNode);
  else
    GetDataStorage()->Add(node);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

// ---------------------------------------------------------------------------
// OnApplyThresholdContour — generate a 2D contour on the current slice using
// the xq_ThresholdContour module
// ---------------------------------------------------------------------------

void xq_LumenContouringView::OnApplyThresholdContour()
{
  if (m_CurrentContourGroupNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Threshold Contour",
                         "Select a contour group first.");
    return;
  }

  mitk::Image::Pointer image = FindSelectedImage();
  if (image.IsNull())
  {
    QMessageBox::warning(nullptr, "Threshold Contour",
                         "No unambiguous image is bound to this contour group. "
                         "Select a contour group with xq.source.image metadata "
                         "or keep exactly one image visible.");
    return;
  }

  const mitk::PlaneGeometry* planeGeo = GetCurrentSlicePlaneGeometry();
  if (!planeGeo)
  {
    QMessageBox::warning(nullptr, "Threshold Contour",
                         "Cannot determine the current slice plane geometry.");
    return;
  }

  const double thresholdVal = m_Ui->spinThreshContourValue->value();
  m_CurrentContourGroupNode->SetDoubleProperty("xq.segmentation.threshold.min", thresholdVal);
  m_CurrentContourGroupNode->SetDoubleProperty("xq.segmentation.threshold.max", thresholdVal);
  if (auto imageNode = FindSelectedImageNode(); imageNode.IsNotNull())
  {
    m_CurrentContourGroupNode->SetStringProperty(
      xq::pipeline::kSourceImageProperty, imageNode->GetName().c_str());
  }

  // Create and configure the threshold contour
  xq_ThresholdContour::Pointer threshContour = xq_ThresholdContour::New();
  threshContour->SetImageData(image);
  threshContour->SetPlaneGeometry(planeGeo);
  threshContour->SetThresholdValue(thresholdVal);
  threshContour->GenerateContourPoints();

  const auto& contourPoints = threshContour->GetContourPoints();
  if (contourPoints.empty())
  {
    const auto extractionDecision =
      xq_SegmentationUtils::GetExtractionFailedWritebackDecision();
    QMessageBox::warning(nullptr, "Threshold Contour",
      QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
        xq_CanonicalWritebackWarningContext::ThresholdContour,
        extractionDecision.warningReason).data()));
    return;
  }

  if (auto* profileGroup = GetProfileGroup(m_CurrentContourGroupNode))
  {
    xq_ProfilePlacementFrame placementFrame;
    const bool hasPlacementFrame =
      !xq_SegmentationUtils::RequiresPathPlacement(profileGroup) ||
      ResolvePlacementFrame(GetDataStorage(), profileGroup, planeGeo, placementFrame);
    const auto writebackDecision =
      xq_SegmentationUtils::GetThresholdContourWritebackDecision(
        profileGroup, true, hasPlacementFrame);
    if (!writebackDecision.ShouldAttemptCanonicalWriteback())
    {
      QMessageBox::warning(nullptr, "Threshold Contour",
                            QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
                              xq_CanonicalWritebackWarningContext::ThresholdContour,
                              writebackDecision.warningReason).data()));
      if (writebackDecision.ShouldReturnEarly())
        return;
    }

    int contourIdx = -1;
    if (!InsertCanonicalProfile(
          GetDataStorage(), profileGroup, m_ContourListWidget, m_ProfileInteractor,
          contourPoints, planeGeo, "threshold", contourIdx))
    {
      const auto failedWritebackDecision =
        xq_SegmentationUtils::GetThresholdContourWritebackDecision(
          profileGroup, true, false);
      if (failedWritebackDecision.ShouldWarn())
      {
        QMessageBox::warning(nullptr, "Threshold Contour",
          QString::fromUtf8(xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
            xq_CanonicalWritebackWarningContext::ThresholdContour,
            failedWritebackDecision.warningReason).data()));
      }
      else
      {
        QMessageBox::warning(nullptr, "Threshold Contour",
                             "Failed to convert threshold contour into a canonical profile.");
      }
      return;
    }

    UpdateContourGroupReadiness(m_CurrentContourGroupNode, profileGroup);
    UpdateContourList();
    const int row = GetProfileListRow(profileGroup, contourIdx);
    if (row >= 0)
      m_ContourListWidget->setCurrentRow(row);
    EnsureProfileInteractor(contourIdx);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    return;
  }

  // Determine contour index within the group
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    GetDataStorage()->GetDerivations(m_CurrentContourGroupNode);
  int contourIdx = children ? static_cast<int>(children->size()) : 0;

  // Create a data node for the threshold contour
  mitk::DataNode::Pointer contourNode =
    xq_SegmentationUtils::CreateLegacyThresholdContourNode(
      contourPoints, m_CurrentContourGroupNode->GetName(), contourIdx);
  if (contourNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Threshold Contour",
                         "Failed to create a legacy contour node from the threshold result.");
    return;
  }

  GetDataStorage()->Add(contourNode, m_CurrentContourGroupNode);
  UpdateContourList();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
