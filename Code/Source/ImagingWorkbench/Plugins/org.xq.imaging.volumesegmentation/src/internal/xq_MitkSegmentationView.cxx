#include "xq_MitkSegmentationView.h"
#include "ui_xq_MitkSegmentationView.h"

#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkLabelSetImage.h>
#include <mitkBaseGeometry.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkProperties.h>
#include <mitkToolManagerProvider.h>
#include <mitkDataNodeSelection.h>

#include <berryISelectionService.h>

#include <xq_ProfileGroup.h>
#include <xq_MitkSeg3D.h>
#include <xq_PipelineDataUtils.h>
#include <xq_Seg3DUtils.h>
#include <xq_SegmentationUtils.h>
#include <xq_SegUndoActor.h>
#include <xq_UndoHelper.h>

#include <mitkSurface.h>

#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QToolBar>

const QString xq_MitkSegmentationView::VIEW_ID = "org.xq.views.mitksegmentation";

xq_MitkSegmentationView::xq_MitkSegmentationView()
  : m_Ui(nullptr)
  , m_ImageSelector(nullptr)
  , m_ContextLabel(nullptr)
  , m_ToolBar(nullptr)
  , m_ToolParameterStack(nullptr)
  , m_ReferenceNode(nullptr)
  , m_WorkingNode(nullptr)
  , m_ActiveProfileGroupNode(nullptr)
  , m_ActiveToolId(-1)
{
}

xq_MitkSegmentationView::~xq_MitkSegmentationView()
{
  delete m_Ui;
}

void xq_MitkSegmentationView::CreateQtPartControl(QWidget* parent)
{
  m_Ui = new Ui::xq_MitkSegmentationView;
  m_Ui->setupUi(parent);

  m_ContextLabel = new QLabel(parent);
  m_ContextLabel->setWordWrap(true);
  m_ContextLabel->setStyleSheet(
    "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
    "border-radius: 4px; padding: 6px; }");
  m_Ui->mainLayout->insertWidget(0, m_ContextLabel);

  m_ImageSelector = m_Ui->imageComboBox;
  m_ToolParameterStack = m_Ui->toolParameterStack;

  // Set stacked widget to empty page initially
  m_ToolParameterStack->setCurrentIndex(0);

  // Populate image list from data storage
  UpdateImageList();

  // Connect image selector
  connect(m_ImageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_MitkSegmentationView::OnImageSelectionChanged);

  // Connect refresh button
  connect(m_Ui->refreshButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::UpdateImageList);

  // Connect tool buttons
  connect(m_Ui->thresholdButton, &QPushButton::clicked, this, [this]() {
    OnToolSelected(1);
  });
  connect(m_Ui->regionGrowButton, &QPushButton::clicked, this, [this]() {
    OnToolSelected(2);
  });
  connect(m_Ui->levelSetButton, &QPushButton::clicked, this, [this]() {
    OnToolSelected(3);
  });

  // Connect apply buttons
  connect(m_Ui->applyThresholdButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnThresholdApply);
  connect(m_Ui->applyRegionGrowButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnRegionGrowApply);
  connect(m_Ui->applyLevelSetButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnLevelSetApply);

  // Connect create segmentation button
  connect(m_Ui->createSegmentationButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::CreateNewSegmentation);

  // Phase 16a: Morphological operations
  connect(m_Ui->erodeButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnErodeApply);
  connect(m_Ui->dilateButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnDilateApply);
  connect(m_Ui->invertButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnInvertSegmentation);
  connect(m_Ui->maskImageButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::OnMaskImageApply);

  EnableSegmentationControls(false);
  UpdateContextStatus("Missing input image");
  SyncToolManager();
  BindCurrentDataManagerSelection();
}

void xq_MitkSegmentationView::SetFocus()
{
  if (m_ImageSelector)
  {
    m_ImageSelector->setFocus();
  }
  BindCurrentDataManagerSelection();
}

void xq_MitkSegmentationView::Activated()
{
  BindCurrentDataManagerSelection();
}

void xq_MitkSegmentationView::Deactivated()
{
  DeactivateToolState();
}

void xq_MitkSegmentationView::Visible()
{
}

void xq_MitkSegmentationView::Hidden()
{
  DeactivateToolState();
}

void xq_MitkSegmentationView::DeactivateToolState()
{
  auto* toolManager = mitk::ToolManagerProvider::GetInstance()->GetToolManager();
  if (!toolManager)
    return;

  toolManager->ActivateTool(-1);
  toolManager->SetReferenceData(nullptr);
  toolManager->SetWorkingData(nullptr);
  m_ActiveToolId = -1;
  if (m_ToolParameterStack)
    m_ToolParameterStack->setCurrentIndex(0);
  EnableSegmentationControls(false);
}

void xq_MitkSegmentationView::BindCurrentDataManagerSelection()
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

void xq_MitkSegmentationView::OnSelectionChanged(
    berry::IWorkbenchPart::Pointer /*source*/,
    const QList<mitk::DataNode::Pointer>& nodes)
{
  bool foundCompatibleNode = false;

  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      // Recognize a canonical ProfileGroup target node.
      if (dynamic_cast<xq_ProfileGroup*>(node->GetData()) != nullptr)
      {
        m_ActiveProfileGroupNode = node;
        auto referenceNode = ResolveReferenceImageForNode(node);
        if (referenceNode.IsNotNull())
          SetReferenceNode(referenceNode);
        SyncToolManager();
        foundCompatibleNode = true;
        QString geometryReason;
        EnableSegmentationControls(m_ReferenceNode.IsNotNull() && HasCompatibleWorkingGeometry(&geometryReason));
        UpdateContextStatus(geometryReason);
        return;
      }

      if (dynamic_cast<xq_MitkSeg3D*>(node->GetData()) != nullptr)
      {
        m_WorkingNode = node;
        auto referenceNode = ResolveReferenceImageForNode(node);
        if (referenceNode.IsNotNull())
          SetReferenceNode(referenceNode);
        RestoreSegmentationToolMetadata(node);
        foundCompatibleNode = true;
        SyncToolManager();
        QString geometryReason;
        EnableSegmentationControls(m_ReferenceNode.IsNotNull() && HasCompatibleWorkingGeometry(&geometryReason));
        UpdateContextStatus(geometryReason);
        return;
      }

      if (dynamic_cast<mitk::LabelSetImage*>(node->GetData()) != nullptr)
      {
        m_WorkingNode = node;
        foundCompatibleNode = true;

        auto referenceNode = ResolveReferenceImageForNode(node);
        if (referenceNode.IsNotNull())
          SetReferenceNode(referenceNode);

        RestoreSegmentationToolMetadata(node);
        SyncToolManager();
        QString geometryReason;
        EnableSegmentationControls(m_ReferenceNode.IsNotNull() && HasCompatibleWorkingGeometry(&geometryReason));
        UpdateContextStatus(geometryReason);
        return;
      }

      mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(node->GetData());
      if (image.IsNotNull())
      {
        SetReferenceNode(node);
        foundCompatibleNode = true;
        SyncToolManager();
        EnableSegmentationControls(HasCompatibleWorkingGeometry());
        UpdateContextStatus("Ready");
        return;
      }
    }
  }

  if (!foundCompatibleNode)
    ClearSegmentationState();
}

void xq_MitkSegmentationView::ClearSegmentationState()
{
  m_ReferenceNode = nullptr;
  m_WorkingNode = nullptr;
  m_ActiveProfileGroupNode = nullptr;
  m_ActiveToolId = -1;
  SyncToolManager();
  if (m_ToolParameterStack)
    m_ToolParameterStack->setCurrentIndex(0);
  EnableSegmentationControls(false);
  UpdateContextStatus("Missing input image");
}

void xq_MitkSegmentationView::EnableSegmentationControls(bool enabled)
{
  if (!m_Ui)
    return;

  const bool compatibleWorkingGeometry = HasCompatibleWorkingGeometry();
  m_Ui->thresholdButton->setEnabled(enabled);
  m_Ui->regionGrowButton->setEnabled(enabled);
  m_Ui->levelSetButton->setEnabled(enabled);
  m_Ui->applyThresholdButton->setEnabled(enabled);
  m_Ui->applyRegionGrowButton->setEnabled(enabled);
  m_Ui->applyLevelSetButton->setEnabled(enabled);
  m_Ui->createSegmentationButton->setEnabled(enabled);

  const bool hasWorkingNode = m_WorkingNode.IsNotNull();
  m_Ui->erodeButton->setEnabled(hasWorkingNode && compatibleWorkingGeometry);
  m_Ui->dilateButton->setEnabled(hasWorkingNode && compatibleWorkingGeometry);
  m_Ui->invertButton->setEnabled(hasWorkingNode && compatibleWorkingGeometry);
  m_Ui->maskImageButton->setEnabled(enabled && hasWorkingNode && compatibleWorkingGeometry);
}

void xq_MitkSegmentationView::UpdateContextStatus(const QString& status)
{
  if (!m_ContextLabel)
    return;

  const QString referenceName = m_ReferenceNode.IsNotNull()
    ? QString::fromStdString(m_ReferenceNode->GetName())
    : QStringLiteral("<none>");
  const QString workingName = m_WorkingNode.IsNotNull()
    ? QString::fromStdString(m_WorkingNode->GetName())
    : QStringLiteral("<none>");
  const QString targetName = m_ActiveProfileGroupNode.IsNotNull()
    ? QString::fromStdString(m_ActiveProfileGroupNode->GetName())
    : QStringLiteral("<none>");
  const QString resolvedStatus = status.isEmpty()
    ? (m_ReferenceNode.IsNotNull() ? QStringLiteral("Ready") : QStringLiteral("Missing input image"))
    : status;
  m_ContextLabel->setText(
    QString("Reference image: %1\nWorking segmentation: %2\nPreprocessing target: %3\nStatus: %4\nNext: create or select a working segmentation before morphology/mask operations.")
      .arg(referenceName, workingName, targetName, resolvedStatus));
}

void xq_MitkSegmentationView::OnImageSelectionChanged(int index)
{
  if (index < 0)
  {
    m_ReferenceNode = nullptr;
    SyncToolManager();
    EnableSegmentationControls(false);
    UpdateContextStatus("Missing input image");
    return;
  }

  QString nodeName = m_ImageSelector->itemText(index);
  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNull())
    return;

  mitk::DataStorage::SetOfObjects::ConstPointer allImages =
      storage->GetSubset(mitk::NodePredicateDataType::New("Image"));

  for (auto it = allImages->Begin(); it != allImages->End(); ++it)
  {
    if (QString::fromStdString(it->Value()->GetName()) == nodeName)
    {
      SetReferenceNode(it->Value());
      SyncToolManager();
      QString geometryReason;
      EnableSegmentationControls(HasCompatibleWorkingGeometry(&geometryReason));
      UpdateContextStatus(geometryReason);
      return;
    }
  }

  m_ReferenceNode = nullptr;
  SyncToolManager();
  EnableSegmentationControls(false);
  UpdateContextStatus("Missing input image");
}

void xq_MitkSegmentationView::OnToolSelected(int toolId)
{
  m_ActiveToolId = toolId;

  if (toolId >= 0 && toolId < m_ToolParameterStack->count())
  {
    m_ToolParameterStack->setCurrentIndex(toolId);
  }
}

void xq_MitkSegmentationView::OnThresholdApply()
{
  if (m_ReferenceNode.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Please select a reference image first.");
    return;
  }

  mitk::Image::Pointer image =
      dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  if (image.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Selected node does not contain a valid image.");
    return;
  }

  QString targetBlockReason;
  if (!CheckPreprocessingTarget(targetBlockReason))
  {
    QMessageBox::warning(nullptr, "3D Segmentation", targetBlockReason);
    return;
  }

  double minThreshold = m_Ui->minThresholdSpinBox->value();
  double maxThreshold = m_Ui->maxThresholdSpinBox->value();

  if (minThreshold > maxThreshold)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Minimum threshold must be less than or equal to maximum threshold.");
    return;
  }

  // Create result segmentation using VTK for type-safe pixel access
  vtkImageData* vtkImg = image->GetVtkImageData();
  if (!vtkImg)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Failed to obtain VTK image data.");
    return;
  }

  if (vtkImg->GetNumberOfScalarComponents() < 1)
    return;

  mitk::Image::Pointer segResult = mitk::Image::New();
  segResult->Initialize(image);

  vtkImageData* vtkSeg = segResult->GetVtkImageData();
  if (!vtkSeg)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Failed to initialize segmentation result.");
    return;
  }

  // Ensure scalars are allocated for the segmentation result
  if (!vtkSeg->GetPointData() || !vtkSeg->GetPointData()->GetScalars())
  {
    vtkSeg->AllocateScalars(VTK_DOUBLE, 1);
  }

  int* dims = vtkImg->GetDimensions();
  for (int z = 0; z < dims[2]; ++z)
  {
    for (int y = 0; y < dims[1]; ++y)
    {
      for (int x = 0; x < dims[0]; ++x)
      {
        double val = vtkImg->GetScalarComponentAsDouble(x, y, z, 0);
        vtkSeg->SetScalarComponentFromDouble(x, y, z, 0,
            (val >= minThreshold && val <= maxThreshold) ? 1.0 : 0.0);
      }
    }
  }

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetData(segResult);
  resultNode->SetName(m_ReferenceNode->GetName() + "_threshold");
  resultNode->SetBoolProperty("binary", true);
  resultNode->SetColor(0.0, 1.0, 0.0);
  resultNode->SetOpacity(0.5);
  StampSegmentationNode(resultNode, "threshold");
  resultNode->SetDoubleProperty("xq.segmentation.threshold.min", minThreshold);
  resultNode->SetDoubleProperty("xq.segmentation.threshold.max", maxThreshold);

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;
  SyncToolManager();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSegmentationView::OnRegionGrowApply()
{
  if (m_ReferenceNode.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Please select a reference image first.");
    return;
  }

  mitk::Image::Pointer image =
      dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  if (image.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Selected node does not contain a valid image.");
    return;
  }

  QString targetBlockReason;
  if (!CheckPreprocessingTarget(targetBlockReason))
  {
    QMessageBox::warning(nullptr, "3D Segmentation", targetBlockReason);
    return;
  }

  double tolerance = m_Ui->toleranceSpinBox->value();

  // Create result segmentation using VTK for type-safe pixel access
  vtkImageData* vtkImg = image->GetVtkImageData();
  if (!vtkImg)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Failed to obtain VTK image data.");
    return;
  }

  mitk::Image::Pointer segResult = mitk::Image::New();
  segResult->Initialize(image);

  vtkImageData* vtkSeg = segResult->GetVtkImageData();
  if (!vtkSeg)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Failed to initialize segmentation result.");
    return;
  }

  // Use center voxel as seed point
  int* dims = vtkImg->GetDimensions();
  int sx = dims[0] / 2;
  int sy = dims[1] / 2;
  int sz = dims[2] / 2;
  double seedVal = vtkImg->GetScalarComponentAsDouble(sx, sy, sz, 0);

  for (int z = 0; z < dims[2]; ++z)
  {
    for (int y = 0; y < dims[1]; ++y)
    {
      for (int x = 0; x < dims[0]; ++x)
      {
        double val = vtkImg->GetScalarComponentAsDouble(x, y, z, 0);
        vtkSeg->SetScalarComponentFromDouble(x, y, z, 0,
            (std::abs(val - seedVal) <= tolerance) ? 1.0 : 0.0);
      }
    }
  }

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetData(segResult);
  resultNode->SetName(m_ReferenceNode->GetName() + "_regiongrow");
  resultNode->SetBoolProperty("binary", true);
  resultNode->SetColor(1.0, 1.0, 0.0);
  resultNode->SetOpacity(0.5);
  StampSegmentationNode(resultNode, "region_growing");
  resultNode->SetDoubleProperty("xq.segmentation.region_growing.tolerance", tolerance);
  resultNode->SetStringProperty("xq.segmentation.seed_points",
    QString("%1,%2,%3").arg(sx).arg(sy).arg(sz).toStdString().c_str());

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;
  SyncToolManager();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSegmentationView::OnLevelSetApply()
{
  if (m_ReferenceNode.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Please select a reference image first.");
    return;
  }

  mitk::Image::Pointer image =
      dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  if (image.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Selected node does not contain a valid image.");
    return;
  }

  QString targetBlockReason;
  if (!CheckPreprocessingTarget(targetBlockReason))
  {
    QMessageBox::warning(nullptr, "3D Segmentation", targetBlockReason);
    return;
  }

  int iterations = m_Ui->iterationsSpinBox->value();
  double propagation = m_Ui->propagationSpinBox->value();
  double curvature = m_Ui->curvatureSpinBox->value();

  // Create result segmentation using threshold-based initialization
  mitk::Image::Pointer segResult = mitk::Image::New();
  segResult->Initialize(image);

  try
  {
    auto* imgVtk = image->GetVtkImageData();
    auto* segVtk = segResult->GetVtkImageData();

    // Compute image statistics for automatic threshold
    double sum = 0.0;
    unsigned long count = 0;
    int dims[3];
    imgVtk->GetDimensions(dims);

    for (int z = 0; z < dims[2]; ++z)
      for (int y = 0; y < dims[1]; ++y)
        for (int x = 0; x < dims[0]; ++x)
        {
          sum += imgVtk->GetScalarComponentAsDouble(x, y, z, 0);
          ++count;
        }

    double mean = (count > 0) ? sum / count : 0.0;
    double threshold = mean * propagation;

    for (int z = 0; z < dims[2]; ++z)
      for (int y = 0; y < dims[1]; ++y)
        for (int x = 0; x < dims[0]; ++x)
        {
          double val = imgVtk->GetScalarComponentAsDouble(x, y, z, 0);
          segVtk->SetScalarComponentFromDouble(x, y, z, 0, (val > threshold) ? 1.0 : 0.0);
        }

    // Iterative morphological smoothing pass to simulate curvature evolution
    for (int iter = 0; iter < iterations; ++iter)
    {
      // Create a temporary copy for neighborhood reads
      mitk::Image::Pointer tempCopy = mitk::Image::New();
      tempCopy->Initialize(segResult);

      {
        auto* srcVtk = segResult->GetVtkImageData();
        auto* dstVtk = tempCopy->GetVtkImageData();

        for (int zz = 1; zz + 1 < dims[2]; ++zz)
        {
          for (int yy = 1; yy + 1 < dims[1]; ++yy)
          {
            for (int xx = 1; xx + 1 < dims[0]; ++xx)
            {
              // Count 6-connected neighbors that are foreground
              int neighbors = 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx-1, yy, zz, 0) > 0) ? 1 : 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx+1, yy, zz, 0) > 0) ? 1 : 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx, yy-1, zz, 0) > 0) ? 1 : 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx, yy+1, zz, 0) > 0) ? 1 : 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx, yy, zz-1, 0) > 0) ? 1 : 0;
              neighbors += (srcVtk->GetScalarComponentAsDouble(xx, yy, zz+1, 0) > 0) ? 1 : 0;

              // Curvature-weighted majority vote smoothing
              int curVal = (srcVtk->GetScalarComponentAsDouble(xx, yy, zz, 0) > 0) ? 1 : 0;
              int smoothThreshold = static_cast<int>(3.0 + curvature);
              if (smoothThreshold < 1) smoothThreshold = 1;
              if (smoothThreshold > 5) smoothThreshold = 5;

              if (neighbors >= smoothThreshold)
                dstVtk->SetScalarComponentFromDouble(xx, yy, zz, 0, 1.0);
              else if (neighbors <= (6 - smoothThreshold))
                dstVtk->SetScalarComponentFromDouble(xx, yy, zz, 0, 0.0);
              else
                dstVtk->SetScalarComponentFromDouble(xx, yy, zz, 0, static_cast<double>(curVal));
            }
          }
        }
      }

      // Copy back
      {
        auto* srcVtk = tempCopy->GetVtkImageData();
        auto* dstVtk = segResult->GetVtkImageData();
        for (int zz = 0; zz < dims[2]; ++zz)
          for (int yy = 0; yy < dims[1]; ++yy)
            for (int xx = 0; xx < dims[0]; ++xx)
              dstVtk->SetScalarComponentFromDouble(xx, yy, zz, 0,
                  srcVtk->GetScalarComponentAsDouble(xx, yy, zz, 0));
      }
    }
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         QString("Level set segmentation failed: %1").arg(e.what()));
    return;
  }

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetData(segResult);
  resultNode->SetName(m_ReferenceNode->GetName() + "_levelset");
  resultNode->SetBoolProperty("binary", true);
  resultNode->SetColor(0.0, 0.0, 1.0);
  resultNode->SetOpacity(0.5);
  StampSegmentationNode(resultNode, "level_set");
  resultNode->SetIntProperty("xq.segmentation.levelset.iterations", iterations);
  resultNode->SetDoubleProperty("xq.segmentation.levelset.propagation", propagation);
  resultNode->SetDoubleProperty("xq.segmentation.levelset.curvature", curvature);

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;
  SyncToolManager();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSegmentationView::CreateNewSegmentation()
{
  if (m_ReferenceNode.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Please select a reference image first.");
    return;
  }

  mitk::Image::Pointer refImage =
      dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  if (refImage.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation",
                         "Selected node does not contain a valid image.");
    return;
  }

  QString targetBlockReason;
  if (!CheckPreprocessingTarget(targetBlockReason))
  {
    QMessageBox::warning(nullptr, "3D Segmentation", targetBlockReason);
    return;
  }

  mitk::LabelSetImage::Pointer segImage = mitk::LabelSetImage::New();
  segImage->Initialize(refImage);

  mitk::DataNode::Pointer segNode = mitk::DataNode::New();
  segNode->SetData(segImage);
  segNode->SetName(m_ReferenceNode->GetName() + "_segmentation");
  segNode->SetBoolProperty("binary", true);
  segNode->SetColor(1.0, 0.0, 0.0);
  segNode->SetOpacity(0.5);
  StampSegmentationNode(segNode, "manual");

  GetDataStorage()->Add(segNode, GetSegmentationParentNode());
  m_WorkingNode = segNode;
  SyncToolManager();

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_MitkSegmentationView::UpdateImageList()
{
  if (m_ImageSelector == nullptr)
    return;

  m_ImageSelector->blockSignals(true);
  m_ImageSelector->clear();

  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNotNull())
  {
    mitk::NodePredicateDataType::Pointer isImage =
        mitk::NodePredicateDataType::New("Image");
    mitk::NodePredicateProperty::Pointer isHelper =
        mitk::NodePredicateProperty::New("helper object", mitk::BoolProperty::New(true));
    mitk::NodePredicateNot::Pointer isNotHelper =
        mitk::NodePredicateNot::New(isHelper);

    mitk::DataStorage::SetOfObjects::ConstPointer images =
        storage->GetSubset(isImage);

  for (auto it = images->Begin(); it != images->End(); ++it)
  {
    bool isHelperObj = false;
    it->Value()->GetBoolProperty("helper object", isHelperObj);
    if (!isHelperObj)
      {
        m_ImageSelector->addItem(
            QString::fromStdString(it->Value()->GetName()));
      }
    }
  }

  m_ImageSelector->blockSignals(false);

  if (m_ImageSelector->count() > 0)
  {
    OnImageSelectionChanged(0);
  }
  else
  {
    m_ReferenceNode = nullptr;
    SyncToolManager();
    EnableSegmentationControls(false);
    UpdateContextStatus("Missing input image");
  }
}

void xq_MitkSegmentationView::OnErodeApply()
{
  if (m_WorkingNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Erode", "No working segmentation. Create or generate one first.");
    return;
  }

  mitk::Image::Pointer segImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (segImage.IsNull())
  {
    QMessageBox::warning(nullptr, "Erode", "Working node is not an image.");
    return;
  }
  if (!HasCompatibleWorkingGeometry())
  {
    QMessageBox::warning(nullptr, "Erode", "Reference image and working segmentation geometry do not match.");
    return;
  }

  int radius = m_Ui->morphRadiusSpinBox->value();

  try
  {
    // Capture state before modification for undo
    auto* actor = xq_SegUndoActor::GetInstance();
    actor->SetTargetImage(segImage.GetPointer());
    auto beforeState = vtkSmartPointer<vtkImageData>::New();
    beforeState->DeepCopy(segImage->GetVtkImageData());

    unsigned int dimX = segImage->GetDimension(0);
    unsigned int dimY = segImage->GetDimension(1);
    unsigned int dimZ = segImage->GetDimension(2);

    mitk::Image::Pointer result = mitk::Image::New();
    result->Initialize(segImage);

    auto* srcVtk = segImage->GetVtkImageData();
    auto* dstVtk = result->GetVtkImageData();

    for (unsigned int z = 0; z < dimZ; ++z)
    {
      for (unsigned int y = 0; y < dimY; ++y)
      {
        for (unsigned int x = 0; x < dimX; ++x)
        {
          double val = srcVtk->GetScalarComponentAsDouble(x, y, z, 0);
          if (val <= 0.0)
          {
            dstVtk->SetScalarComponentFromDouble(x, y, z, 0, 0.0);
            continue;
          }
          bool eroded = false;
          for (int dz = -radius; dz <= radius && !eroded; ++dz)
          {
            for (int dy = -radius; dy <= radius && !eroded; ++dy)
            {
              for (int dx = -radius; dx <= radius && !eroded; ++dx)
              {
                if (dx*dx + dy*dy + dz*dz > radius*radius) continue;
                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;
                int nz = static_cast<int>(z) + dz;
                if (nx < 0 || nx >= (int)dimX || ny < 0 || ny >= (int)dimY || nz < 0 || nz >= (int)dimZ)
                {
                  eroded = true;
                }
                else if (srcVtk->GetScalarComponentAsDouble(nx, ny, nz, 0) <= 0.0)
                {
                  eroded = true;
                }
              }
            }
          }
          dstVtk->SetScalarComponentFromDouble(x, y, z, 0, eroded ? 0.0 : 1.0);
        }
      }
    }

    m_WorkingNode->SetData(result);
    m_WorkingNode->SetStringProperty("xq.segmentation.utility", "erosion");
    m_WorkingNode->SetIntProperty("xq.segmentation.morph.radius", radius);

    // Register undo: capture after-state from the new result image
    mitk::Image::Pointer newImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
    actor->SetTargetImage(newImage.GetPointer());
    auto afterState = vtkSmartPointer<vtkImageData>::New();
    afterState->DeepCopy(newImage->GetVtkImageData());

    auto* doOp = new RestoreImageOp(afterState);
    auto* undoOp = new RestoreImageOp(beforeState);
    xq_UndoHelper::RegisterUndoableOperation(actor, doOp, undoOp, "Erode Segmentation");

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Erode",
        QString("Erosion applied (radius=%1) to '%2'.")
            .arg(radius).arg(QString::fromStdString(m_WorkingNode->GetName())));
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "Erode", QString("Erosion failed: %1").arg(e.what()));
  }
}

void xq_MitkSegmentationView::OnDilateApply()
{
  if (m_WorkingNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Dilate", "No working segmentation. Create or generate one first.");
    return;
  }

  mitk::Image::Pointer segImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (segImage.IsNull())
  {
    QMessageBox::warning(nullptr, "Dilate", "Working node is not an image.");
    return;
  }
  if (!HasCompatibleWorkingGeometry())
  {
    QMessageBox::warning(nullptr, "Dilate", "Reference image and working segmentation geometry do not match.");
    return;
  }

  int radius = m_Ui->morphRadiusSpinBox->value();

  try
  {
    // Capture state before modification for undo
    auto* actor = xq_SegUndoActor::GetInstance();
    actor->SetTargetImage(segImage.GetPointer());
    auto beforeState = vtkSmartPointer<vtkImageData>::New();
    beforeState->DeepCopy(segImage->GetVtkImageData());

    unsigned int dimX = segImage->GetDimension(0);
    unsigned int dimY = segImage->GetDimension(1);
    unsigned int dimZ = segImage->GetDimension(2);

    mitk::Image::Pointer result = mitk::Image::New();
    result->Initialize(segImage);

    auto* srcVtk = segImage->GetVtkImageData();
    auto* dstVtk = result->GetVtkImageData();

    for (unsigned int z = 0; z < dimZ; ++z)
    {
      for (unsigned int y = 0; y < dimY; ++y)
      {
        for (unsigned int x = 0; x < dimX; ++x)
        {
          double val = srcVtk->GetScalarComponentAsDouble(x, y, z, 0);
          if (val > 0.0)
          {
            dstVtk->SetScalarComponentFromDouble(x, y, z, 0, 1.0);
            continue;
          }
          // Check if any neighbor within radius is foreground
          bool dilated = false;
          for (int dz = -radius; dz <= radius && !dilated; ++dz)
          {
            for (int dy = -radius; dy <= radius && !dilated; ++dy)
            {
              for (int dx = -radius; dx <= radius && !dilated; ++dx)
              {
                if (dx*dx + dy*dy + dz*dz > radius*radius) continue;
                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;
                int nz = static_cast<int>(z) + dz;
                if (nx >= 0 && nx < (int)dimX && ny >= 0 && ny < (int)dimY && nz >= 0 && nz < (int)dimZ)
                {
                  if (srcVtk->GetScalarComponentAsDouble(nx, ny, nz, 0) > 0.0)
                    dilated = true;
                }
              }
            }
          }
          dstVtk->SetScalarComponentFromDouble(x, y, z, 0, dilated ? 1.0 : 0.0);
        }
      }
    }

    m_WorkingNode->SetData(result);
    m_WorkingNode->SetStringProperty("xq.segmentation.utility", "dilatation");
    m_WorkingNode->SetIntProperty("xq.segmentation.morph.radius", radius);

    // Register undo: capture after-state from the new result image
    mitk::Image::Pointer newImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
    actor->SetTargetImage(newImage.GetPointer());
    auto afterState = vtkSmartPointer<vtkImageData>::New();
    afterState->DeepCopy(newImage->GetVtkImageData());

    auto* doOp = new RestoreImageOp(afterState);
    auto* undoOp = new RestoreImageOp(beforeState);
    xq_UndoHelper::RegisterUndoableOperation(actor, doOp, undoOp, "Dilate Segmentation");

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Dilate",
        QString("Dilation applied (radius=%1) to '%2'.")
            .arg(radius).arg(QString::fromStdString(m_WorkingNode->GetName())));
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "Dilate", QString("Dilation failed: %1").arg(e.what()));
  }
}

void xq_MitkSegmentationView::OnInvertSegmentation()
{
  if (m_WorkingNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Invert", "No working segmentation.");
    return;
  }

  mitk::Image::Pointer segImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (segImage.IsNull())
  {
    QMessageBox::warning(nullptr, "Invert", "Working node is not an image.");
    return;
  }
  if (!HasCompatibleWorkingGeometry())
  {
    QMessageBox::warning(nullptr, "Invert", "Reference image and working segmentation geometry do not match.");
    return;
  }

  try
  {
    // Capture state before modification for undo
    auto* actor = xq_SegUndoActor::GetInstance();
    actor->SetTargetImage(segImage.GetPointer());
    auto beforeState = vtkSmartPointer<vtkImageData>::New();
    beforeState->DeepCopy(segImage->GetVtkImageData());

    auto* vtk = segImage->GetVtkImageData();
    int dims[3];
    vtk->GetDimensions(dims);
    for (int z = 0; z < dims[2]; ++z)
    {
      for (int y = 0; y < dims[1]; ++y)
      {
        for (int x = 0; x < dims[0]; ++x)
        {
          double val = vtk->GetScalarComponentAsDouble(x, y, z, 0);
          vtk->SetScalarComponentFromDouble(x, y, z, 0, (val > 0.0) ? 0.0 : 1.0);
        }
      }
    }

    // Register undo with after-state
    auto afterState = vtkSmartPointer<vtkImageData>::New();
    afterState->DeepCopy(segImage->GetVtkImageData());

    auto* doOp = new RestoreImageOp(afterState);
    auto* undoOp = new RestoreImageOp(beforeState);
    xq_UndoHelper::RegisterUndoableOperation(actor, doOp, undoOp, "Invert Segmentation");

    m_WorkingNode->Modified();
    m_WorkingNode->SetStringProperty("xq.segmentation.utility", "invert");
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    QMessageBox::information(nullptr, "Invert",
        QString("Segmentation '%1' inverted.").arg(QString::fromStdString(m_WorkingNode->GetName())));
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "Invert", QString("Invert failed: %1").arg(e.what()));
  }
}

void xq_MitkSegmentationView::OnMaskImageApply()
{
  if (m_ReferenceNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Mask", "No reference image selected.");
    return;
  }
  if (m_WorkingNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Mask", "No working segmentation.");
    return;
  }

  mitk::Image::Pointer refImage = dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  mitk::Image::Pointer segImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (refImage.IsNull() || segImage.IsNull())
  {
    QMessageBox::warning(nullptr, "Mask", "Reference or segmentation is not a valid image.");
    return;
  }

  if (!HasCompatibleWorkingGeometry())
  {
    QMessageBox::warning(nullptr, "Mask", "Reference image and working segmentation geometry do not match.");
    return;
  }

  try
  {
    mitk::Image::Pointer maskedImage = mitk::Image::New();
    maskedImage->Initialize(refImage);

    auto* refVtk = refImage->GetVtkImageData();
    auto* segVtk = segImage->GetVtkImageData();
    auto* outVtk = maskedImage->GetVtkImageData();

    for (unsigned int z = 0; z < refImage->GetDimension(2); ++z)
    {
      for (unsigned int y = 0; y < refImage->GetDimension(1); ++y)
      {
        for (unsigned int x = 0; x < refImage->GetDimension(0); ++x)
        {
          double segVal = segVtk->GetScalarComponentAsDouble(x, y, z, 0);
          double refVal = refVtk->GetScalarComponentAsDouble(x, y, z, 0);
          outVtk->SetScalarComponentFromDouble(x, y, z, 0, (segVal > 0.0) ? refVal : 0.0);
        }
      }
    }

    mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
    resultNode->SetData(maskedImage);
    resultNode->SetName(m_ReferenceNode->GetName() + "_masked");
    resultNode->SetStringProperty("xq.pipeline.stage", "image_processing");
    resultNode->SetStringProperty("xq.pipeline.version", "1");
    resultNode->SetStringProperty("xq.pipeline.algorithm", "segmentation_masking");
    resultNode->SetStringProperty("xq.image.processing.operation", "mask_image");
    resultNode->SetStringProperty("xq.source.image", m_ReferenceNode->GetName().c_str());
    resultNode->SetStringProperty("xq.source.segmentation", m_WorkingNode->GetName().c_str());
    resultNode->SetStringProperty("xq.segmentation.utility", "image_masking");

    GetDataStorage()->Add(resultNode, m_ReferenceNode);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    QMessageBox::information(nullptr, "Mask",
        QString("Masked image created as '%1'.").arg(QString::fromStdString(resultNode->GetName())));
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "Mask", QString("Masking failed: %1").arg(e.what()));
  }
}

// ---------------------------------------------------------------------------
// Preprocessing-target helpers
// ---------------------------------------------------------------------------

xq_PreprocTargetResolution xq_MitkSegmentationView::ResolveTargetContext() const
{
  const xq_ProfileGroup* group = nullptr;
  if (m_ActiveProfileGroupNode.IsNotNull())
    group = dynamic_cast<const xq_ProfileGroup*>(m_ActiveProfileGroupNode->GetData());
  return xq_SegmentationUtils::ResolvePreprocessingTarget(group);
}

mitk::DataNode::Pointer xq_MitkSegmentationView::GetSegmentationParentNode() const
{
  if (ResolveTargetContext().IsResolved() && m_ActiveProfileGroupNode.IsNotNull())
    return m_ActiveProfileGroupNode;
  return m_ReferenceNode;
}

bool xq_MitkSegmentationView::CheckPreprocessingTarget(QString& outReason) const
{
  const auto resolution = ResolveTargetContext();
  if (resolution.state == xq_PreprocTargetState::TargetIneligible)
  {
    outReason = QString("Preprocessing target is not eligible: %1")
                    .arg(QString::fromStdString(resolution.blockingReason));
    return false;
  }
  return true;
}

bool xq_MitkSegmentationView::HasCompatibleWorkingGeometry(QString* outReason) const
{
  if (m_WorkingNode.IsNull())
  {
    if (outReason)
      outReason->clear();
    return true;
  }

  if (m_ReferenceNode.IsNull())
  {
    if (outReason)
      *outReason = "Missing reference image";
    return false;
  }

  auto* refImage = dynamic_cast<mitk::Image*>(m_ReferenceNode->GetData());
  auto* workingImage = dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (!refImage || !workingImage || !refImage->GetGeometry() || !workingImage->GetGeometry())
  {
    if (outReason)
      *outReason = "Invalid image geometry";
    return false;
  }

  const bool compatible = mitk::Equal(*refImage->GetGeometry(), *workingImage->GetGeometry());
  if (outReason)
    *outReason = compatible ? QString() : "Reference image and working segmentation geometry do not match";
  return compatible;
}

void xq_MitkSegmentationView::SyncToolManager()
{
  auto* toolManager = mitk::ToolManagerProvider::GetInstance()->GetToolManager();
  if (!toolManager)
    return;

  auto storage = GetDataStorage();
  if (storage.IsNotNull())
    toolManager->SetDataStorage(*storage);

  toolManager->SetReferenceData(m_ReferenceNode.GetPointer());
  toolManager->SetWorkingData(m_WorkingNode.GetPointer());
}

mitk::DataNode::Pointer xq_MitkSegmentationView::ResolveReferenceImageForNode(
  const mitk::DataNode::Pointer& node) const
{
  auto storage = GetDataStorage();
  if (storage.IsNull() || node.IsNull())
    return nullptr;

  if (dynamic_cast<mitk::Image*>(node->GetData()) != nullptr)
    return node;

  auto imageNode = xq::pipeline::ResolveUpstreamNode(
    storage,
    node.GetPointer(),
    xq::pipeline::kSourceImageProperty,
    xq::pipeline::Stage::Unknown);
  if (imageNode.IsNotNull())
    return imageNode;

  auto pathNode = xq::pipeline::ResolveUpstreamNode(
    storage,
    node.GetPointer(),
    xq::pipeline::kSourcePathProperty,
    xq::pipeline::Stage::Path);
  if (pathNode.IsNotNull())
  {
    imageNode = xq::pipeline::ResolveUpstreamNode(
      storage,
      pathNode.GetPointer(),
      xq::pipeline::kSourceImageProperty,
      xq::pipeline::Stage::Unknown);
    if (imageNode.IsNotNull())
      return imageNode;
  }

  return nullptr;
}

void xq_MitkSegmentationView::SetReferenceNode(
  const mitk::DataNode::Pointer& node)
{
  if (node.IsNull() || dynamic_cast<mitk::Image*>(node->GetData()) == nullptr)
    return;

  m_ReferenceNode = node;
  if (!m_ImageSelector)
    return;

  const int index = m_ImageSelector->findText(
    QString::fromStdString(node->GetName()));
  if (index >= 0)
  {
    m_ImageSelector->blockSignals(true);
    m_ImageSelector->setCurrentIndex(index);
    m_ImageSelector->blockSignals(false);
  }
}

void xq_MitkSegmentationView::StampSegmentationNode(
  const mitk::DataNode::Pointer& node,
  const char* method) const
{
  if (node.IsNull())
    return;

  node->SetStringProperty("xq.pipeline.stage", "segmentation_3d");
  node->SetStringProperty("xq.pipeline.version", "1");
  node->SetStringProperty("xq.pipeline.algorithm", method ? method : "manual");
  node->SetBoolProperty("xq.segmentation.3d", true);
  node->SetStringProperty("xq.segmentation.method", method ? method : "manual");
  node->SetStringProperty("xq.segmentation.toolmanager.mode", "reference_working_data");
  if (m_ReferenceNode.IsNotNull())
  {
    node->SetStringProperty("xq.source.image", m_ReferenceNode->GetName().c_str());
  }
  if (m_WorkingNode.IsNotNull() && m_WorkingNode != node)
  {
    node->SetStringProperty("xq.source.segmentation", m_WorkingNode->GetName().c_str());
  }

  int toolId = 0;
  const std::string methodString = method ? method : "manual";
  if (methodString == "threshold")
    toolId = 1;
  else if (methodString == "region_growing")
    toolId = 2;
  else if (methodString == "level_set")
    toolId = 3;
  node->SetIntProperty("xq.segmentation.active_tool_id", toolId);
}

void xq_MitkSegmentationView::RestoreSegmentationToolMetadata(
  const mitk::DataNode::Pointer& node)
{
  if (node.IsNull() || !m_Ui)
    return;

  int toolId = -1;
  if (!node->GetIntProperty("xq.segmentation.active_tool_id", toolId))
  {
    std::string method;
    node->GetStringProperty("xq.segmentation.method", method);
    if (method == "threshold")
      toolId = 1;
    else if (method == "region_growing")
      toolId = 2;
    else if (method == "level_set")
      toolId = 3;
  }
  if (toolId >= 0)
    OnToolSelected(toolId);

  double minThreshold = 0.0;
  if (node->GetDoubleProperty("xq.segmentation.threshold.min", minThreshold))
    m_Ui->minThresholdSpinBox->setValue(minThreshold);

  double maxThreshold = 0.0;
  if (node->GetDoubleProperty("xq.segmentation.threshold.max", maxThreshold))
    m_Ui->maxThresholdSpinBox->setValue(maxThreshold);

  double tolerance = 0.0;
  if (node->GetDoubleProperty("xq.segmentation.region_growing.tolerance", tolerance))
    m_Ui->toleranceSpinBox->setValue(tolerance);

  int iterations = 0;
  if (node->GetIntProperty("xq.segmentation.levelset.iterations", iterations) &&
      iterations > 0)
  {
    m_Ui->iterationsSpinBox->setValue(iterations);
  }

  double propagation = 0.0;
  if (node->GetDoubleProperty("xq.segmentation.levelset.propagation", propagation))
    m_Ui->propagationSpinBox->setValue(propagation);

  double curvature = 0.0;
  if (node->GetDoubleProperty("xq.segmentation.levelset.curvature", curvature))
    m_Ui->curvatureSpinBox->setValue(curvature);
}
