#include "xq_MitkSegmentationView.h"
#include "ui_xq_MitkSegmentationView.h"

#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkLabelSetImage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkProperties.h>
#include <mitkToolManagerProvider.h>

#include <xq_ProfileGroup.h>
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
#include <QMessageBox>
#include <QStackedWidget>
#include <QToolBar>

const QString xq_MitkSegmentationView::VIEW_ID = "org.xq.views.mitksegmentation";

xq_MitkSegmentationView::xq_MitkSegmentationView()
  : m_Ui(nullptr)
  , m_ImageSelector(nullptr)
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

  // P2-C: 3D Seg → Model conversion
  connect(m_Ui->convertToModelButton, &QPushButton::clicked,
          this, &xq_MitkSegmentationView::ConvertToModel);
}

void xq_MitkSegmentationView::SetFocus()
{
  if (m_ImageSelector)
  {
    m_ImageSelector->setFocus();
  }
}

void xq_MitkSegmentationView::OnSelectionChanged(
    berry::IWorkbenchPart::Pointer /*source*/,
    const QList<mitk::DataNode::Pointer>& nodes)
{
  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      // Recognize a canonical ProfileGroup target node.
      if (dynamic_cast<xq_ProfileGroup*>(node->GetData()) != nullptr)
      {
        m_ActiveProfileGroupNode = node;
        return;
      }

      mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(node->GetData());
      if (image.IsNotNull())
      {
        m_ReferenceNode = node;

        // Update combo box selection to match
        int index = m_ImageSelector->findText(
            QString::fromStdString(node->GetName()));
        if (index >= 0)
        {
          m_ImageSelector->blockSignals(true);
          m_ImageSelector->setCurrentIndex(index);
          m_ImageSelector->blockSignals(false);
        }
        return;
      }
    }
  }
}

void xq_MitkSegmentationView::OnImageSelectionChanged(int index)
{
  if (index < 0)
  {
    m_ReferenceNode = nullptr;
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
      m_ReferenceNode = it->Value();
      return;
    }
  }

  m_ReferenceNode = nullptr;
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

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;

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

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;

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

  GetDataStorage()->Add(resultNode, GetSegmentationParentNode());
  m_WorkingNode = resultNode;

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

  GetDataStorage()->Add(segNode, GetSegmentationParentNode());
  m_WorkingNode = segNode;

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

  if (refImage->GetDimension(0) != segImage->GetDimension(0) ||
      refImage->GetDimension(1) != segImage->GetDimension(1) ||
      refImage->GetDimension(2) != segImage->GetDimension(2))
  {
    QMessageBox::warning(nullptr, "Mask", "Reference image and segmentation dimensions do not match.");
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
// P2-C: 3D Seg → Model conversion
// ---------------------------------------------------------------------------

void xq_MitkSegmentationView::ConvertToModel()
{
  if (m_WorkingNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Convert to Model",
                         "No working segmentation. Create or generate one first.");
    return;
  }

  mitk::Image::Pointer segImage =
      dynamic_cast<mitk::Image*>(m_WorkingNode->GetData());
  if (segImage.IsNull())
  {
    QMessageBox::warning(nullptr, "Convert to Model",
                         "Working node is not an image.");
    return;
  }

  vtkImageData* vtkImg = segImage->GetVtkImageData();
  if (!vtkImg)
  {
    QMessageBox::warning(nullptr, "Convert to Model",
                         "Failed to obtain VTK image data.");
    return;
  }

  // Step 1: Marching cubes at iso-value 0.5 (binary boundary)
  auto surface = xq_Seg3DUtils::MarchingCubes(vtkImg, 0.5);
  if (!surface || surface->GetNumberOfCells() == 0)
  {
    QMessageBox::warning(nullptr, "Convert to Model",
                         "Marching cubes produced no surface. "
                         "Ensure the segmentation contains foreground voxels.");
    return;
  }

  // Step 2: Optional smoothing
  int smoothIter = m_Ui->smoothIterSpinBox->value();
  if (smoothIter > 0)
  {
    surface = xq_Seg3DUtils::SmoothSurface(surface, smoothIter, 0.1);
  }

  // Step 3: Optional decimation
  int decimatePct = m_Ui->decimateSpinBox->value();
  if (decimatePct > 0)
  {
    surface = xq_Seg3DUtils::DecimateSurface(
        surface, decimatePct / 100.0);
  }

  // Step 4: Compute normals
  surface = xq_Seg3DUtils::ComputeNormals(surface);

  // Create a mitk::Surface node
  mitk::Surface::Pointer mitkSurface = mitk::Surface::New();
  mitkSurface->SetVtkPolyData(surface);

  mitk::DataNode::Pointer modelNode = mitk::DataNode::New();
  modelNode->SetData(mitkSurface);
  modelNode->SetName(m_WorkingNode->GetName() + "_model");
  modelNode->SetColor(0.8, 0.8, 0.2);
  modelNode->SetStringProperty("xq.pipeline.stage", "model");
  modelNode->SetStringProperty("xq.model.source", "seg3d_conversion");

  GetDataStorage()->Add(modelNode, m_WorkingNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Convert to Model",
      QString("Surface model '%1' created (%2 points, %3 triangles).")
          .arg(QString::fromStdString(modelNode->GetName()))
          .arg(surface->GetNumberOfPoints())
          .arg(surface->GetNumberOfCells()));
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
