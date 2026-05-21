#include "xq_GridGenerationView.h"
#include "xq_MeshCreate.h"
#include "ui_xq_GridGenerationView.h"

#include <xq_Model.h>
#include <xq_Grid.h>
#include <xq_MitkGrid.h>
#include <xq_MeshPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>
#include <mitkUnstructuredGrid.h>
#include <mitkDataNodeSelection.h>

#include <berryISelectionService.h>

#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkUnstructuredGridWriter.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkMeshQuality.h>
#include <vtkTriangleFilter.h>
#include <vtkLinearSubdivisionFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkCellTypes.h>
#include <vtkSmartPointer.h>
#include <vtkAppendFilter.h>
#include <vtkGeometryFilter.h>
#include <vtkCell.h>
#include <vtkPolyDataNormals.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkSphereSource.h>
#include <vtkCylinderSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

#include <QComboBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSignalBlocker>

#include <mitkIOUtil.h>

#include <cmath>

const QString xq_GridGenerationView::VIEW_ID = "org.xq.views.meshing";

namespace {

vtkPolyData* GetModelPolyData(mitk::DataNode::Pointer node)
{
  if (node.IsNull() || node->GetData() == nullptr)
    return nullptr;

  auto* model = dynamic_cast<xq_Model*>(node->GetData());
  auto* element = model ? model->GetModelElement(0) : nullptr;
  auto polyData = element ? element->GetWholeVtkPolyData() : nullptr;
  if (polyData != nullptr)
    return polyData;

  std::string type;
  node->GetStringProperty("xq.type", type);
  std::string stage;
  node->GetStringProperty(xq::pipeline::kStageProperty, stage);
  if (type != "model" && stage != std::string(xq::pipeline::ToStageName(xq::pipeline::Stage::Model)))
    return nullptr;

  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  return surface ? surface->GetVtkPolyData() : nullptr;
}

bool ParseFaceId(const QString& faceName, int& faceId)
{
  QString text = faceName.trimmed();
  if (text.startsWith("face_", Qt::CaseInsensitive))
    text = text.mid(5);
  else if (text.startsWith("face", Qt::CaseInsensitive))
    text = text.mid(4);

  bool ok = false;
  const int parsed = text.toInt(&ok);
  if (!ok)
    return false;

  faceId = parsed;
  return true;
}

QString SerializeLocalSizeTable(QTableWidget* table)
{
  if (!table)
    return {};

  QStringList entries;
  for (int row = 0; row < table->rowCount(); ++row)
  {
    QTableWidgetItem* nameItem = table->item(row, 0);
    QTableWidgetItem* sizeItem = table->item(row, 2);
    if (!nameItem || !sizeItem)
      continue;

    int faceId = 0;
    bool sizeOk = false;
    const double edgeSize = sizeItem->text().toDouble(&sizeOk);
    if (!ParseFaceId(nameItem->text(), faceId) || !sizeOk || edgeSize <= 0.0)
      continue;

    entries << QString("%1:%2").arg(faceId).arg(edgeSize, 0, 'g', 12);
  }
  return entries.join(';');
}

QString SerializeRefinementRegionTable(QTableWidget* table)
{
  if (!table)
    return {};

  QStringList entries;
  for (int row = 0; row < table->rowCount(); ++row)
  {
    QTableWidgetItem* typeItem = table->item(row, 1);
    QTableWidgetItem* cxItem = table->item(row, 2);
    QTableWidgetItem* cyItem = table->item(row, 3);
    QTableWidgetItem* czItem = table->item(row, 4);
    QTableWidgetItem* sizeItem = table->item(row, 5);
    QTableWidgetItem* edgeItem = table->item(row, 6);
    if (!typeItem || !cxItem || !cyItem || !czItem || !sizeItem || !edgeItem)
      continue;

    bool ok = false;
    const double cx = cxItem->text().toDouble(&ok); if (!ok) continue;
    const double cy = cyItem->text().toDouble(&ok); if (!ok) continue;
    const double cz = czItem->text().toDouble(&ok); if (!ok) continue;
    const double edgeSize = edgeItem->text().toDouble(&ok); if (!ok || edgeSize <= 0.0) continue;

    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;
    const QString type = typeItem->text();
    if (type == "Cylinder")
    {
      QStringList parts = sizeItem->text().split(' ', Qt::SkipEmptyParts);
      for (const QString& part : parts)
      {
        if (part.startsWith("R="))
          sx = part.mid(2).toDouble();
        else if (part.startsWith("L="))
          sy = part.mid(2).toDouble();
        else if (part.startsWith("Axis="))
        {
          const QString axis = part.mid(5);
          sz = axis == "X" ? 0.0 : (axis == "Y" ? 1.0 : 2.0);
        }
      }
    }
    else
    {
      const QStringList sizes = sizeItem->text().split(' ', Qt::SkipEmptyParts);
      sx = sizes.value(0, sizeItem->text()).toDouble(&ok); if (!ok) continue;
      sy = sizes.size() > 1 ? sizes[1].toDouble(&ok) : sx; if (!ok) continue;
      sz = sizes.size() > 2 ? sizes[2].toDouble(&ok) : sx; if (!ok) continue;
    }

    entries << QString("%1,%2,%3,%4,%5,%6,%7,%8")
      .arg(type)
      .arg(cx, 0, 'g', 12)
      .arg(cy, 0, 'g', 12)
      .arg(cz, 0, 'g', 12)
      .arg(sx, 0, 'g', 12)
      .arg(sy, 0, 'g', 12)
      .arg(sz, 0, 'g', 12)
      .arg(edgeSize, 0, 'g', 12);
  }
  return entries.join(';');
}

} // namespace

xq_GridGenerationView::xq_GridGenerationView()
  : m_Ui(nullptr)
  , m_ContextLabel(nullptr)
  , m_CurrentModelNode(nullptr)
  , m_CurrentMeshNode(nullptr)
{
}

xq_GridGenerationView::~xq_GridGenerationView()
{
  delete m_Ui;
}

void xq_GridGenerationView::CreateQtPartControl(QWidget* parent)
{
  m_Ui = new Ui::xq_GridGenerationView;
  m_Ui->setupUi(parent);

  m_ContextLabel = new QLabel(parent);
  m_ContextLabel->setWordWrap(true);
  m_ContextLabel->setStyleSheet(
    "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
    "border-radius: 4px; padding: 6px; }");
  m_Ui->mainLayout->insertWidget(0, m_ContextLabel);

  // Connections
  connect(m_Ui->btnCreateMesh, &QPushButton::clicked,
          this, &xq_GridGenerationView::CreateMesh);
  connect(m_Ui->btnRunMeshing, &QPushButton::clicked,
          this, &xq_GridGenerationView::RunMeshing);
  connect(m_Ui->btnAdaptMesh, &QPushButton::clicked,
          this, &xq_GridGenerationView::AdaptMesh);
  connect(m_Ui->spinGlobalEdgeSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_GridGenerationView::SetGlobalSize);
  connect(m_Ui->spinBLFirstHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_GridGenerationView::PersistMeshParametersToMetadata);
  connect(m_Ui->spinBLLayers, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &xq_GridGenerationView::PersistMeshParametersToMetadata);
  connect(m_Ui->spinBLGrowthRate, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &xq_GridGenerationView::PersistMeshParametersToMetadata);
  connect(m_Ui->btnAddLocalSize, &QPushButton::clicked,
          this, &xq_GridGenerationView::AddLocalSize);
  connect(m_Ui->btnQualityReport, &QPushButton::clicked,
          this, &xq_GridGenerationView::ShowMeshQualityReport);
  connect(m_Ui->btnExportMesh, &QPushButton::clicked,
          this, &xq_GridGenerationView::ExportMesh);
  connect(m_Ui->chkBoundaryLayer, &QCheckBox::toggled,
          this, &xq_GridGenerationView::OnBoundaryLayerToggled);
  connect(m_Ui->btnPreviewBL, &QPushButton::clicked,
          this, &xq_GridGenerationView::PreviewBoundaryLayer);

  // Boundary layer controls initially disabled
  m_Ui->grpBLParams->setEnabled(false);
  m_Ui->btnPreviewBL->setEnabled(false);

  // Refinement region connections
  connect(m_Ui->btnAddSphereRegion, &QPushButton::clicked,
          this, &xq_GridGenerationView::AddSphereRefinementRegion);
  connect(m_Ui->btnAddCylinderRegion, &QPushButton::clicked,
          this, &xq_GridGenerationView::AddCylinderRefinementRegion);
  connect(m_Ui->btnRemoveRegion, &QPushButton::clicked,
          this, &xq_GridGenerationView::RemoveRefinementRegion);
  connect(m_Ui->btnVisualizeRegions, &QPushButton::clicked,
          this, &xq_GridGenerationView::VisualizeRefinementRegions);

  // Configure refinement regions table
  m_Ui->tableRefinementRegions->setColumnCount(7);
  m_Ui->tableRefinementRegions->setHorizontalHeaderLabels(
    QStringList() << "Name" << "Type" << "Center X" << "Center Y"
                  << "Center Z" << "Radius/Size" << "Target Size");
  m_Ui->tableRefinementRegions->horizontalHeader()->setStretchLastSection(true);
  m_Ui->tableRefinementRegions->setSelectionBehavior(QAbstractItemView::SelectRows);
  connect(m_Ui->tableRefinementRegions, &QTableWidget::cellChanged,
          this, &xq_GridGenerationView::PersistMeshParametersToMetadata);

  // Configure local size table
  m_Ui->tableLocalSize->setColumnCount(3);
  m_Ui->tableLocalSize->setHorizontalHeaderLabels(
    QStringList() << "Face Name" << "Type" << "Edge Size");
  m_Ui->tableLocalSize->horizontalHeader()->setStretchLastSection(true);
  m_Ui->tableLocalSize->setSelectionBehavior(QAbstractItemView::SelectRows);
  connect(m_Ui->tableLocalSize, &QTableWidget::cellChanged,
          this, &xq_GridGenerationView::PersistMeshParametersToMetadata);

  // Initialize statistics labels
  ClearMeshStatistics();
  EnableMeshControls(false, false);
  BindCurrentDataManagerSelection();
}

void xq_GridGenerationView::SetFocus()
{
  if (m_Ui && m_Ui->comboModelSelector)
    m_Ui->comboModelSelector->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_GridGenerationView::BindCurrentDataManagerSelection()
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

void xq_GridGenerationView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer /*source*/,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  mitk::DataNode::Pointer selectedModelNode = nullptr;
  mitk::DataNode::Pointer selectedMeshNode = nullptr;

  for (const auto& node : nodes)
  {
    if (node.IsNull())
      continue;

    std::string stage;
    const bool hasStage = node->GetStringProperty("xq.pipeline.stage", stage);
    if (selectedModelNode.IsNull() &&
        GetModelPolyData(node) != nullptr &&
        (!hasStage || stage == "model"))
    {
      selectedModelNode = node;
      continue;
    }

    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(node->GetData());
    if (mitkGrid != nullptr && selectedMeshNode.IsNull())
    {
      selectedMeshNode = node;
      continue;
    }
  }

  if (selectedMeshNode.IsNotNull())
  {
    m_CurrentMeshNode = selectedMeshNode;
    m_CurrentModelNode = selectedModelNode.IsNotNull()
      ? selectedModelNode
      : ResolveModelForMesh(selectedMeshNode.GetPointer());
  }
  else if (selectedModelNode.IsNotNull())
  {
    m_CurrentModelNode = selectedModelNode;
    m_CurrentMeshNode = nullptr;
  }
  else
  {
    ClearMeshState();
    return;
  }

  const bool hasModel = m_CurrentModelNode.IsNotNull();
  const bool hasMesh = m_CurrentMeshNode.IsNotNull();
  EnableMeshControls(hasModel, hasMesh);
  UpdateContextStatus(hasModel ? QStringLiteral("Ready") : QStringLiteral("Missing model"));

  if (hasModel && m_Ui && m_Ui->comboModelSelector)
  {
    const QString modelName = QString::fromStdString(m_CurrentModelNode->GetName());
    if (m_Ui->comboModelSelector->findText(modelName) < 0)
      m_Ui->comboModelSelector->addItem(modelName);
    m_Ui->comboModelSelector->setCurrentText(modelName);
  }

  if (hasModel)
    PopulateLocalSizeTable();

  if (hasMesh)
  {
    RestoreMeshParametersFromMetadata();
    UpdateMeshStatistics();
  }
  else
  {
    RestoreMeshParametersFromMetadata();
    ClearMeshStatistics();
  }
}

void xq_GridGenerationView::CreateMesh()
{
  PersistMeshParametersToMetadata();

  xq_GridCreate dialog(GetDataStorage(),
    this->GetSite()->GetWorkbenchWindow()->GetShell()->GetControl());
  if (m_CurrentModelNode.IsNotNull())
    dialog.SelectModel(QString::fromStdString(m_CurrentModelNode->GetName()));

  if (dialog.exec() == QDialog::Accepted)
  {
    QString meshName = dialog.GetMeshName();
    QString modelName = dialog.GetSelectedModelName();

    if (meshName.isEmpty())
    {
      QMessageBox::warning(nullptr, "Mesh Generation", "Please enter a mesh name.");
      return;
    }

    mitk::DataNode::Pointer parentNode = nullptr;
    if (!modelName.isEmpty())
    {
      mitk::DataStorage::SetOfObjects::ConstPointer allNodes =
        GetDataStorage()->GetAll();
      for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
      {
        if (it->Value()->GetName() == modelName.toStdString())
        {
          parentNode = it->Value();
          break;
        }
      }
    }

    if (parentNode.IsNull())
    {
      QMessageBox::warning(nullptr, "Mesh Generation", "Selected model node was not found.");
      return;
    }

    xq_MeshGenerationRequest request;
    request.meshName = meshName.toStdString();
    request.globalEdgeSize = m_Ui->spinGlobalEdgeSize->value();
    request.boundaryLayerFirstHeight = m_Ui->spinBLFirstHeight->value();
    request.boundaryLayerLayers = m_Ui->spinBLLayers->value();
    request.boundaryLayerGrowthRate = m_Ui->spinBLGrowthRate->value();
    request.preserveSurface = true;
    request.optimize = true;
    request.minDihedral = 10.0;
    request.maxEdgeSize = 0.0;

    const auto meshResult =
      xq_MeshPipelineService::CreateVolumeMesh(GetDataStorage(), parentNode, request);
    if (!meshResult.ok || meshResult.node.IsNull())
    {
      QMessageBox::warning(nullptr, "Mesh Generation", "Failed to create mesh node.");
      return;
    }

    m_CurrentModelNode = parentNode;
    m_CurrentMeshNode = meshResult.node;
    UpdateMeshStatistics();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_GridGenerationView::RunMeshing()
{
  PersistMeshParametersToMetadata();

  QString readiness;
  if (!IsModelReadyForMeshing(&readiness))
  {
    QMessageBox::warning(nullptr, "Mesh Generation",
      readiness.isEmpty() ? QStringLiteral("Model is not ready for meshing.") : readiness);
    return;
  }

  auto* model = dynamic_cast<xq_Model*>(m_CurrentModelNode->GetData());
  auto* element = model ? model->GetModelElement(0) : nullptr;
  auto surface = element ? element->GetWholeVtkPolyData() : nullptr;
  if (!surface || surface->GetNumberOfPoints() == 0)
  {
    QMessageBox::warning(nullptr, "Mesh Generation", "Selected model has no usable geometry.");
    return;
  }

  double globalSize = m_Ui->spinGlobalEdgeSize->value();
  if (globalSize <= 0.0)
  {
    QMessageBox::warning(nullptr, "Mesh Generation",
      "Global edge size must be positive.");
    return;
  }

  // Collect meshing parameters
  double blFirstHeight = m_Ui->spinBLFirstHeight->value();
  int blLayers = m_Ui->spinBLLayers->value();
  double blGrowthRate = m_Ui->spinBLGrowthRate->value();

  xq_MeshGenerationRequest request;
  request.meshName = m_CurrentMeshNode.IsNotNull()
    ? m_CurrentMeshNode->GetName()
    : m_CurrentModelNode->GetName() + "-mesh";
  request.globalEdgeSize = globalSize;
  request.boundaryLayerFirstHeight = blFirstHeight;
  request.boundaryLayerLayers = blLayers;
  request.boundaryLayerGrowthRate = blGrowthRate;

  for (int row = 0; row < m_Ui->tableLocalSize->rowCount(); ++row)
  {
    QTableWidgetItem* nameItem = m_Ui->tableLocalSize->item(row, 0);
    QTableWidgetItem* sizeItem = m_Ui->tableLocalSize->item(row, 2);
    if (nameItem && sizeItem)
    {
      int faceId = 0;
      if (ParseFaceId(nameItem->text(), faceId))
        request.localFaceSizes[faceId] = sizeItem->text().toDouble();
    }
  }

  // Populate refinement regions from the table
  for (int row = 0; row < m_Ui->tableRefinementRegions->rowCount(); ++row)
  {
    QTableWidgetItem* typeItem = m_Ui->tableRefinementRegions->item(row, 1);
    if (!typeItem)
      continue;

    xq_RefinementRegion region;
    QString typeStr = typeItem->text();

    if (m_Ui->tableRefinementRegions->item(row, 2))
      region.center[0] = m_Ui->tableRefinementRegions->item(row, 2)->text().toDouble();
    if (m_Ui->tableRefinementRegions->item(row, 3))
      region.center[1] = m_Ui->tableRefinementRegions->item(row, 3)->text().toDouble();
    if (m_Ui->tableRefinementRegions->item(row, 4))
      region.center[2] = m_Ui->tableRefinementRegions->item(row, 4)->text().toDouble();

    if (typeStr == "Sphere")
    {
      region.type = xq_RefinementRegion::Type::Sphere;
      if (m_Ui->tableRefinementRegions->item(row, 5))
      {
        region.radiusOrSize[0] = m_Ui->tableRefinementRegions->item(row, 5)->text().toDouble();
        region.radiusOrSize[1] = region.radiusOrSize[0];
        region.radiusOrSize[2] = region.radiusOrSize[0];
      }
    }
    else if (typeStr == "Cylinder")
    {
      region.type = xq_RefinementRegion::Type::Cylinder;
      if (m_Ui->tableRefinementRegions->item(row, 5))
      {
        QString sizeStr = m_Ui->tableRefinementRegions->item(row, 5)->text();
        QStringList parts = sizeStr.split(' ', Qt::SkipEmptyParts);
        for (const QString& part : parts)
        {
          if (part.startsWith("R="))
            region.radiusOrSize[0] = part.mid(2).toDouble();
          else if (part.startsWith("L="))
            region.radiusOrSize[1] = part.mid(2).toDouble();
          else if (part.startsWith("Axis="))
          {
            QString axis = part.mid(5);
            if (axis == "X") region.radiusOrSize[2] = 0.0;
            else if (axis == "Y") region.radiusOrSize[2] = 1.0;
            else region.radiusOrSize[2] = 2.0;
          }
        }
      }
    }
    else if (typeStr == "Box")
    {
      region.type = xq_RefinementRegion::Type::Box;
    }

    if (m_Ui->tableRefinementRegions->item(row, 6))
      region.edgeSize = m_Ui->tableRefinementRegions->item(row, 6)->text().toDouble();

    request.refinementRegions.push_back(region);
  }

  request.preserveSurface = true;
  request.optimize = true;
  request.minDihedral = 10.0;
  request.maxEdgeSize = 0.0;

  const auto meshResult =
    xq_MeshPipelineService::CreateVolumeMesh(GetDataStorage(), m_CurrentModelNode, request);
  if (!meshResult.ok || meshResult.node.IsNull())
  {
    QMessageBox::warning(nullptr, "Mesh Generation", "Mesh generation failed.");
    return;
  }

  m_CurrentMeshNode = meshResult.node;

  UpdateMeshStatistics();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Mesh Generation",
    QString("Surface mesh generated:\n"
            "Points: %1\nCells: %2\nGlobal edge size: %3")
      .arg(surface->GetNumberOfPoints())
      .arg(surface->GetNumberOfCells())
      .arg(globalSize));
}

void xq_GridGenerationView::AdaptMesh()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Mesh Generation", "No mesh selected for adaptation.");
    return;
  }

  m_CurrentMeshNode->SetBoolProperty("xq.mesh.adaptPending", true);
  m_CurrentMeshNode->SetStringProperty("xq.mesh.adapt.source_mesh",
    m_CurrentMeshNode->GetName().c_str());
  m_CurrentMeshNode->SetStringProperty("xq.mesh.adapt.diagnostic",
    "Mesh adaptation requires a real error metric/result array. Current UI marks the mesh pending but does not create a new adapted mesh.");

  QMessageBox::information(nullptr, "Mesh Adaptation",
    "Mesh adaptation queued. The mesher will refine elements "
    "based on error estimation.");
}

void xq_GridGenerationView::SetGlobalSize(double size)
{
  if (m_CurrentMeshNode.IsNotNull())
    m_CurrentMeshNode->SetDoubleProperty("xq.mesh.globalEdgeSize", size);
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::AddLocalSize()
{
  if (m_CurrentModelNode.IsNull())
    return;

  if (!GetModelPolyData(m_CurrentModelNode))
    return;

  // Ask user for face name and desired size
  bool ok = false;
  QString faceName = QInputDialog::getText(
    nullptr, "Add Local Size", "Face name:", QLineEdit::Normal, "", &ok);
  if (!ok || faceName.isEmpty())
    return;

  double localSize = QInputDialog::getDouble(
    nullptr, "Add Local Size",
    "Edge size for face '" + faceName + "':",
    m_Ui->spinGlobalEdgeSize->value(), 0.001, 1000.0, 3, &ok);
  if (!ok)
    return;

  int row = m_Ui->tableLocalSize->rowCount();
  m_Ui->tableLocalSize->insertRow(row);
  m_Ui->tableLocalSize->setItem(row, 0, new QTableWidgetItem(faceName));
  m_Ui->tableLocalSize->setItem(row, 1, new QTableWidgetItem("wall"));
  m_Ui->tableLocalSize->setItem(row, 2,
    new QTableWidgetItem(QString::number(localSize, 'f', 3)));
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::UpdateModelSelector()
{
  m_Ui->comboModelSelector->clear();

  mitk::NodePredicateDataType::Pointer predicate =
    mitk::NodePredicateDataType::New("xq_Model");
  mitk::DataStorage::SetOfObjects::ConstPointer surfaces =
    GetDataStorage()->GetSubset(predicate);

  for (auto it = surfaces->Begin(); it != surfaces->End(); ++it)
  {
    m_Ui->comboModelSelector->addItem(
      QString::fromStdString(it->Value()->GetName()));
  }
}

void xq_GridGenerationView::PopulateLocalSizeTable()
{
  // Preserve existing entries; just validate face names still exist in model
  if (m_CurrentModelNode.IsNull())
    return;

  // The table is manually managed via AddLocalSize
}

void xq_GridGenerationView::UpdateMeshStatistics()
{
  if (m_CurrentMeshNode.IsNull())
  {
    ClearMeshStatistics();
    return;
  }

  auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(m_CurrentMeshNode->GetData());
  auto* gridData = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;
  if (!gridData || !gridData->GetVolumeMesh())
  {
    ClearMeshStatistics();
    return;
  }

  vtkUnstructuredGrid* grid = gridData->GetVolumeMesh();

  vtkIdType numCells = grid->GetNumberOfCells();
  vtkIdType numPoints = grid->GetNumberOfPoints();

  m_Ui->lblNumElements->setText(QString("Elements: %1").arg(numCells));
  m_Ui->lblNumNodes->setText(QString("Nodes: %1").arg(numPoints));

  // Compute mesh quality if cells exist
  if (numCells > 0)
  {
    vtkSmartPointer<vtkMeshQuality> quality =
      vtkSmartPointer<vtkMeshQuality>::New();
    quality->SetInputData(grid);
    quality->SetTetQualityMeasureToAspectRatio();
    quality->Update();

    vtkDataArray* qualityArray =
      quality->GetOutput()->GetCellData()->GetArray("Quality");

    if (qualityArray && qualityArray->GetNumberOfTuples() > 0)
    {
      double range[2];
      qualityArray->GetRange(range);
      double sum = 0.0;
      for (vtkIdType i = 0; i < qualityArray->GetNumberOfTuples(); ++i)
        sum += qualityArray->GetTuple1(i);
      double avg = sum / qualityArray->GetNumberOfTuples();

      m_Ui->lblQuality->setText(
        QString("Quality (aspect ratio): min=%1 max=%2 avg=%3")
          .arg(range[0], 0, 'f', 2)
          .arg(range[1], 0, 'f', 2)
          .arg(avg, 0, 'f', 2));
    }
    else
    {
      m_Ui->lblQuality->setText("Quality: N/A");
    }
  }
  else
  {
    m_Ui->lblQuality->setText("Quality: (no elements)");
  }
  UpdateContextStatus(numCells > 0 ? QStringLiteral("Ready") : QStringLiteral("Invalid mesh: no cells"));
}

void xq_GridGenerationView::ClearMeshStatistics()
{
  if (!m_Ui)
    return;

  m_Ui->lblNumElements->setText("Elements: --");
  m_Ui->lblNumNodes->setText("Nodes: --");
  m_Ui->lblQuality->setText("Quality: --");
  UpdateContextStatus(m_CurrentModelNode.IsNotNull() ? QStringLiteral("Missing mesh")
                                                     : QStringLiteral("Missing model"));
}

void xq_GridGenerationView::ClearMeshState()
{
  m_CurrentModelNode = nullptr;
  m_CurrentMeshNode = nullptr;
  ClearMeshStatistics();
  if (m_Ui && m_Ui->tableLocalSize)
    m_Ui->tableLocalSize->setRowCount(0);
  EnableMeshControls(false, false);
  UpdateContextStatus("Missing model");
}

void xq_GridGenerationView::EnableMeshControls(bool hasModel, bool hasMesh)
{
  if (!m_Ui)
    return;

  const bool modelReady = hasModel && IsModelReadyForMeshing();
  const bool meshReady = hasMesh && IsMeshReadyForDownstream();
  m_Ui->btnCreateMesh->setEnabled(modelReady);
  m_Ui->btnRunMeshing->setEnabled(modelReady);
  m_Ui->btnAdaptMesh->setEnabled(meshReady);
  m_Ui->btnAddLocalSize->setEnabled(modelReady);
  m_Ui->spinGlobalEdgeSize->setEnabled(modelReady);
  m_Ui->btnQualityReport->setEnabled(hasMesh);
  m_Ui->btnExportMesh->setEnabled(meshReady);
  m_Ui->btnPreviewBL->setEnabled(modelReady && m_Ui->chkBoundaryLayer->isChecked());
  m_Ui->btnAddSphereRegion->setEnabled(modelReady);
  m_Ui->btnAddCylinderRegion->setEnabled(modelReady);
  m_Ui->btnRemoveRegion->setEnabled(modelReady);
  m_Ui->btnVisualizeRegions->setEnabled(modelReady);
}

bool xq_GridGenerationView::IsModelReadyForMeshing(QString* reason) const
{
  auto setReason = [reason](const QString& text) {
    if (reason)
      *reason = text;
    return false;
  };

  if (m_CurrentModelNode.IsNull())
    return setReason(QStringLiteral("Missing model"));

  auto* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData || polyData->GetNumberOfPoints() == 0 || polyData->GetNumberOfCells() == 0)
    return setReason(QStringLiteral("Invalid model: no usable geometry"));

  bool modelQaOk = true;
  if (m_CurrentModelNode->GetBoolProperty("xq.model.qa.ok", modelQaOk) && !modelQaOk)
    return setReason(QStringLiteral("Invalid model: QA failed"));

  if (reason)
    *reason = QStringLiteral("Ready");
  return true;
}

bool xq_GridGenerationView::IsMeshReadyForDownstream(QString* reason) const
{
  auto setReason = [reason](const QString& text) {
    if (reason)
      *reason = text;
    return false;
  };

  if (m_CurrentMeshNode.IsNull())
    return setReason(QStringLiteral("Missing mesh"));

  bool meshQaOk = true;
  if (m_CurrentMeshNode->GetBoolProperty("xq.mesh.qa.ok", meshQaOk) && !meshQaOk)
    return setReason(QStringLiteral("Invalid mesh: QA failed"));

  auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(m_CurrentMeshNode->GetData());
  auto* gridData = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;
  auto* volume = gridData ? gridData->GetVolumeMesh() : nullptr;
  if (!volume || volume->GetNumberOfPoints() == 0 || volume->GetNumberOfCells() == 0)
    return setReason(QStringLiteral("Invalid mesh: no cells"));

  if (reason)
    *reason = QStringLiteral("Ready");
  return true;
}

void xq_GridGenerationView::UpdateContextStatus(const QString& status)
{
  if (!m_ContextLabel)
    return;

  const QString modelName = m_CurrentModelNode.IsNotNull()
    ? QString::fromStdString(m_CurrentModelNode->GetName())
    : QStringLiteral("<none>");
  const QString meshName = m_CurrentMeshNode.IsNotNull()
    ? QString::fromStdString(m_CurrentMeshNode->GetName())
    : QStringLiteral("<new output>");
  const QString resolvedStatus = status.isEmpty()
    ? (m_CurrentModelNode.IsNotNull() ? QStringLiteral("Ready") : QStringLiteral("Missing model"))
    : status;
  QString quality = QStringLiteral("--");
  if (m_Ui && m_Ui->lblQuality)
    quality = m_Ui->lblQuality->text();
  QString modelReadiness;
  IsModelReadyForMeshing(&modelReadiness);
  QString meshReadiness;
  IsMeshReadyForDownstream(&meshReadiness);
  m_ContextLabel->setText(
    QString("Input model: %1\nCurrent mesh: %2\nQA: %3\nStatus: %4\nModel readiness: %5\nMesh readiness: %6\nNext: generate/inspect mesh, then configure Flow Analysis.")
      .arg(modelName, meshName, quality, resolvedStatus, modelReadiness, meshReadiness));
}

mitk::DataNode::Pointer xq_GridGenerationView::ResolveModelForMesh(
  mitk::DataNode* meshNode) const
{
  auto storage = GetDataStorage();
  if (storage.IsNull() || !meshNode)
    return nullptr;

  auto upstreamModel = xq::pipeline::ResolveUpstreamNode(
    storage,
    meshNode,
    xq::pipeline::kSourceModelProperty,
    xq::pipeline::Stage::Model);
  return upstreamModel;
}

void xq_GridGenerationView::RestoreMeshParametersFromMetadata()
{
  if (!m_Ui)
    return;

  mitk::DataNode::Pointer sourceNode =
    m_CurrentMeshNode.IsNotNull() ? m_CurrentMeshNode : m_CurrentModelNode;
  if (sourceNode.IsNull())
    return;

  m_RestoringMeshMetadata = true;
  QSignalBlocker globalSizeBlocker(m_Ui->spinGlobalEdgeSize);
  QSignalBlocker firstHeightBlocker(m_Ui->spinBLFirstHeight);
  QSignalBlocker layersBlocker(m_Ui->spinBLLayers);
  QSignalBlocker growthBlocker(m_Ui->spinBLGrowthRate);
  QSignalBlocker localTableBlocker(m_Ui->tableLocalSize);
  QSignalBlocker regionTableBlocker(m_Ui->tableRefinementRegions);

  double globalSize = 0.0;
  if (sourceNode->GetDoubleProperty("xq.mesh.globalEdgeSize", globalSize) &&
      globalSize > 0.0)
    m_Ui->spinGlobalEdgeSize->setValue(globalSize);

  double blFirstHeight = 0.0;
  if (sourceNode->GetDoubleProperty("xq.mesh.bl.firstHeight", blFirstHeight) &&
      blFirstHeight > 0.0)
    m_Ui->spinBLFirstHeight->setValue(blFirstHeight);

  int blLayers = 0;
  if (sourceNode->GetIntProperty("xq.mesh.bl.layers", blLayers) ||
      sourceNode->GetIntProperty("xq.mesh.bl.numLayers", blLayers))
  {
    m_Ui->spinBLLayers->setValue(blLayers);
    m_Ui->chkBoundaryLayer->setChecked(blLayers > 0);
  }

  double blGrowthRate = 0.0;
  if (sourceNode->GetDoubleProperty("xq.mesh.bl.growthRate", blGrowthRate) &&
      blGrowthRate > 0.0)
    m_Ui->spinBLGrowthRate->setValue(blGrowthRate);

  std::string localFaceSizeValues;
  if (sourceNode->GetStringProperty("xq.mesh.local_face_sizes.values", localFaceSizeValues) &&
      m_Ui->tableLocalSize)
  {
    m_Ui->tableLocalSize->setRowCount(0);
    const QStringList entries = QString::fromStdString(localFaceSizeValues)
      .split(';', Qt::SkipEmptyParts);
    for (const QString& entry : entries)
    {
      const QStringList parts = entry.split(':', Qt::SkipEmptyParts);
      if (parts.size() != 2)
        continue;
      bool faceOk = false;
      bool sizeOk = false;
      const int faceId = parts[0].toInt(&faceOk);
      const double edgeSize = parts[1].toDouble(&sizeOk);
      if (!faceOk || !sizeOk)
        continue;

      const int row = m_Ui->tableLocalSize->rowCount();
      m_Ui->tableLocalSize->insertRow(row);
      m_Ui->tableLocalSize->setItem(
        row, 0, new QTableWidgetItem(QString("face_%1").arg(faceId)));
      m_Ui->tableLocalSize->setItem(row, 1, new QTableWidgetItem("Local"));
      m_Ui->tableLocalSize->setItem(
        row, 2, new QTableWidgetItem(QString::number(edgeSize)));
    }
  }

  std::string refinementRegionValues;
  if (sourceNode->GetStringProperty("xq.mesh.refinement_regions.values", refinementRegionValues) &&
      m_Ui->tableRefinementRegions)
  {
    m_Ui->tableRefinementRegions->setRowCount(0);
    const QStringList entries = QString::fromStdString(refinementRegionValues)
      .split(';', Qt::SkipEmptyParts);
    int regionIndex = 1;
    for (const QString& entry : entries)
    {
      const QStringList parts = entry.split(',', Qt::SkipEmptyParts);
      if (parts.size() != 8)
        continue;

      const QString type = parts[0];
      bool ok = true;
      const double cx = parts[1].toDouble(&ok); if (!ok) continue;
      const double cy = parts[2].toDouble(&ok); if (!ok) continue;
      const double cz = parts[3].toDouble(&ok); if (!ok) continue;
      const double sx = parts[4].toDouble(&ok); if (!ok) continue;
      const double sy = parts[5].toDouble(&ok); if (!ok) continue;
      const double sz = parts[6].toDouble(&ok); if (!ok) continue;
      const double edgeSize = parts[7].toDouble(&ok); if (!ok) continue;

      const int row = m_Ui->tableRefinementRegions->rowCount();
      m_Ui->tableRefinementRegions->insertRow(row);
      m_Ui->tableRefinementRegions->setItem(
        row, 0, new QTableWidgetItem(QString("Region %1").arg(regionIndex++)));
      m_Ui->tableRefinementRegions->setItem(row, 1, new QTableWidgetItem(type));
      m_Ui->tableRefinementRegions->setItem(row, 2, new QTableWidgetItem(QString::number(cx)));
      m_Ui->tableRefinementRegions->setItem(row, 3, new QTableWidgetItem(QString::number(cy)));
      m_Ui->tableRefinementRegions->setItem(row, 4, new QTableWidgetItem(QString::number(cz)));
      m_Ui->tableRefinementRegions->setItem(
        row, 5, new QTableWidgetItem(QString("%1 %2 %3").arg(sx).arg(sy).arg(sz)));
      m_Ui->tableRefinementRegions->setItem(
        row, 6, new QTableWidgetItem(QString::number(edgeSize)));
    }

    m_Ui->lblRegionCount->setText(
      QString("%1 refinement regions defined").arg(m_Ui->tableRefinementRegions->rowCount()));
  }

  m_RestoringMeshMetadata = false;
}

void xq_GridGenerationView::PersistMeshParametersToMetadata()
{
  if (!m_Ui || m_RestoringMeshMetadata)
    return;

  mitk::DataNode::Pointer targetNode =
    m_CurrentMeshNode.IsNotNull() ? m_CurrentMeshNode : m_CurrentModelNode;
  if (targetNode.IsNull())
    return;

  targetNode->SetDoubleProperty(
    "xq.mesh.globalEdgeSize", m_Ui->spinGlobalEdgeSize->value());
  targetNode->SetDoubleProperty(
    "xq.mesh.bl.firstHeight", m_Ui->spinBLFirstHeight->value());
  targetNode->SetIntProperty(
    "xq.mesh.bl.layers", m_Ui->spinBLLayers->value());
  targetNode->SetIntProperty(
    "xq.mesh.bl.numLayers", m_Ui->spinBLLayers->value());
  targetNode->SetDoubleProperty(
    "xq.mesh.bl.growthRate", m_Ui->spinBLGrowthRate->value());
  targetNode->SetBoolProperty(
    "xq.mesh.bl.directionInward", m_Ui->chkBLDirectionInward->isChecked());

  const QString localSizes = SerializeLocalSizeTable(m_Ui->tableLocalSize);
  const int localSizeCount = localSizes.isEmpty()
    ? 0
    : localSizes.split(';', Qt::SkipEmptyParts).size();
  targetNode->SetIntProperty("xq.mesh.local_face_sizes", localSizeCount);
  targetNode->SetStringProperty(
    "xq.mesh.local_face_sizes.values", localSizes.toStdString().c_str());

  const QString regions = SerializeRefinementRegionTable(m_Ui->tableRefinementRegions);
  const int regionCount = regions.isEmpty()
    ? 0
    : regions.split(';', Qt::SkipEmptyParts).size();
  targetNode->SetIntProperty("xq.mesh.refinement_regions", regionCount);
  targetNode->SetStringProperty(
    "xq.mesh.refinement_regions.values", regions.toStdString().c_str());
  targetNode->Modified();
}

void xq_GridGenerationView::ShowMeshQualityReport()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::information(nullptr, "Quality Report",
      "No mesh selected. Please run meshing first.");
    return;
  }

  // Try UnstructuredGrid first, then Surface
  vtkSmartPointer<vtkPolyData> polyData;
  int numPoints = 0;
  int numCells = 0;

  auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(m_CurrentMeshNode->GetData());
  auto* volumeGrid = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;
  if (volumeGrid && volumeGrid->GetVolumeMesh())
  {
    vtkUnstructuredGrid* vtkUg = volumeGrid->GetVolumeMesh();
    numPoints = static_cast<int>(vtkUg->GetNumberOfPoints());
    numCells = static_cast<int>(vtkUg->GetNumberOfCells());

    // Convert to polydata for quality analysis
    vtkSmartPointer<vtkGeometryFilter> geoFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geoFilter->SetInputData(vtkUg);
    geoFilter->Update();
    polyData = geoFilter->GetOutput();
  }
  else
  {
    auto* surface = dynamic_cast<mitk::Surface*>(m_CurrentMeshNode->GetData());
    if (surface && surface->GetVtkPolyData())
    {
      polyData = surface->GetVtkPolyData();
      numPoints = static_cast<int>(polyData->GetNumberOfPoints());
      numCells = static_cast<int>(polyData->GetNumberOfCells());
    }
  }

  if (!polyData || numCells == 0)
  {
    QMessageBox::information(nullptr, "Quality Report", "Mesh has no cells.");
    return;
  }

  // Triangulate for quality analysis
  vtkSmartPointer<vtkTriangleFilter> triFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  triFilter->SetInputData(polyData);
  triFilter->Update();
  vtkPolyData* triData = triFilter->GetOutput();

  // Compute edge length statistics
  double minEdge = 1e30, maxEdge = 0.0, sumEdge = 0.0;
  int edgeCount = 0;
  for (vtkIdType c = 0; c < triData->GetNumberOfCells(); ++c)
  {
    vtkCell* cell = triData->GetCell(c);
    if (!cell) continue;
    int nEdges = cell->GetNumberOfEdges();
    for (int e = 0; e < nEdges; ++e)
    {
      vtkCell* edge = cell->GetEdge(e);
      if (!edge || edge->GetNumberOfPoints() < 2) continue;
      double p1[3], p2[3];
      triData->GetPoint(edge->GetPointId(0), p1);
      triData->GetPoint(edge->GetPointId(1), p2);
      double len = std::sqrt(
        (p2[0]-p1[0])*(p2[0]-p1[0]) +
        (p2[1]-p1[1])*(p2[1]-p1[1]) +
        (p2[2]-p1[2])*(p2[2]-p1[2]));
      sumEdge += len;
      edgeCount++;
      if (len < minEdge) minEdge = len;
      if (len > maxEdge) maxEdge = len;
    }
  }

  double avgEdge = (edgeCount > 0) ? sumEdge / edgeCount : 0.0;

  // Compute area statistics
  double totalArea = 0.0;
  double minArea = 1e30, maxArea = 0.0;
  for (vtkIdType c = 0; c < triData->GetNumberOfCells(); ++c)
  {
    vtkCell* cell = triData->GetCell(c);
    if (!cell || cell->GetNumberOfPoints() < 3) continue;
    double p0[3], p1[3], p2[3];
    triData->GetPoint(cell->GetPointId(0), p0);
    triData->GetPoint(cell->GetPointId(1), p1);
    triData->GetPoint(cell->GetPointId(2), p2);
    // Cross product for triangle area
    double v1[3] = {p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2]};
    double v2[3] = {p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2]};
    double cx = v1[1]*v2[2] - v1[2]*v2[1];
    double cy = v1[2]*v2[0] - v1[0]*v2[2];
    double cz = v1[0]*v2[1] - v1[1]*v2[0];
    double area = 0.5 * std::sqrt(cx*cx + cy*cy + cz*cz);
    totalArea += area;
    if (area < minArea) minArea = area;
    if (area > maxArea) maxArea = area;
  }

  double avgArea = (triData->GetNumberOfCells() > 0)
    ? totalArea / triData->GetNumberOfCells() : 0.0;

  // Compute bounding box diagonal
  double bounds[6];
  polyData->GetBounds(bounds);
  double diagonal = std::sqrt(
    (bounds[1]-bounds[0])*(bounds[1]-bounds[0]) +
    (bounds[3]-bounds[2])*(bounds[3]-bounds[2]) +
    (bounds[5]-bounds[4])*(bounds[5]-bounds[4]));

  QString report = QString(
    "=== Mesh Quality Report ===\n\n"
    "Mesh: %1\n\n"
    "--- Topology ---\n"
    "  Points: %2\n"
    "  Cells: %3\n"
    "  Triangles: %4\n"
    "  Edges (approx): %5\n\n"
    "--- Edge Lengths ---\n"
    "  Min: %6\n"
    "  Max: %7\n"
    "  Average: %8\n"
    "  Ratio (max/min): %9\n\n"
    "--- Cell Areas ---\n"
    "  Total: %10\n"
    "  Min: %11\n"
    "  Max: %12\n"
    "  Average: %13\n\n"
    "--- Bounding Box ---\n"
    "  X: [%14, %15]\n"
    "  Y: [%16, %17]\n"
    "  Z: [%18, %19]\n"
    "  Diagonal: %20")
    .arg(QString::fromStdString(m_CurrentMeshNode->GetName()))
    .arg(numPoints)
    .arg(numCells)
    .arg(triData->GetNumberOfCells())
    .arg(edgeCount)
    .arg(minEdge, 0, 'f', 4).arg(maxEdge, 0, 'f', 4).arg(avgEdge, 0, 'f', 4)
    .arg(minEdge > 1e-12 ? maxEdge / minEdge : 0.0, 0, 'f', 2)
    .arg(totalArea, 0, 'f', 4)
    .arg(minArea, 0, 'f', 6).arg(maxArea, 0, 'f', 6).arg(avgArea, 0, 'f', 6)
    .arg(bounds[0], 0, 'f', 2).arg(bounds[1], 0, 'f', 2)
    .arg(bounds[2], 0, 'f', 2).arg(bounds[3], 0, 'f', 2)
    .arg(bounds[4], 0, 'f', 2).arg(bounds[5], 0, 'f', 2)
    .arg(diagonal, 0, 'f', 2);

  QMessageBox::information(nullptr, "Mesh Quality Report", report);
}

void xq_GridGenerationView::ExportMesh()
{
  if (m_CurrentMeshNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Export Mesh", "No mesh selected.");
    return;
  }

  // Get the unstructured grid from the mesh node
  auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(m_CurrentMeshNode->GetData());
  auto* gridData = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;
  vtkUnstructuredGrid* grid = gridData ? gridData->GetVolumeMesh() : nullptr;

  if (!grid || grid->GetNumberOfCells() == 0)
  {
    QMessageBox::warning(nullptr, "Export Mesh", "No volume mesh data to export.");
    return;
  }

  QString defaultName = QString::fromStdString(m_CurrentMeshNode->GetName());
  QString filter = "VTU (*.vtu);;VTK Legacy (*.vtk)";
  if (!defaultName.endsWith(".vtu") && !defaultName.endsWith(".vtk"))
    defaultName += ".vtu";

  QString filePath = QFileDialog::getSaveFileName(
    nullptr, "Export Mesh", defaultName, filter);
  if (filePath.isEmpty()) return;

  bool ok = false;
  QString errorMsg;

  if (filePath.endsWith(".vtu", Qt::CaseInsensitive))
  {
    vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
      vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputData(grid);
    ok = writer->Write() != 0;
    if (!ok) errorMsg = "Failed to write VTU file.";
  }
  else if (filePath.endsWith(".vtk", Qt::CaseInsensitive))
  {
    vtkSmartPointer<vtkUnstructuredGridWriter> writer =
      vtkSmartPointer<vtkUnstructuredGridWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputData(grid);
    ok = writer->Write() != 0;
    if (!ok) errorMsg = "Failed to write VTK legacy file.";
  }
  else
  {
    QMessageBox::warning(nullptr, "Export Mesh",
      "Unsupported file extension. Use .vtu or .vtk.");
    return;
  }

  if (ok)
  {
    QMessageBox::information(nullptr, "Export Mesh",
      QString("Mesh exported to:\n%1").arg(filePath));
  }
  else
  {
    QMessageBox::critical(nullptr, "Export Mesh", errorMsg);
  }
}

void xq_GridGenerationView::OnBoundaryLayerToggled(bool enabled)
{
  m_Ui->grpBLParams->setEnabled(enabled);
  m_Ui->btnPreviewBL->setEnabled(enabled);
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::PreviewBoundaryLayer()
{
  if (m_CurrentModelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Boundary Layer", "No model selected.");
    return;
  }

  vtkPolyData* inputPoly = GetModelPolyData(m_CurrentModelNode);
  if (!inputPoly)
  {
    QMessageBox::warning(nullptr, "Boundary Layer",
      "Selected model has no surface data.");
    return;
  }

  int numLayers = m_Ui->spinBLLayers->value();
  double firstHeight = m_Ui->spinBLFirstHeight->value();
  double growthRate = m_Ui->spinBLGrowthRate->value();
  bool directionInward = m_Ui->chkBLDirectionInward->isChecked();

  // Compute total height for status display
  double totalHeight = 0.0;
  for (int i = 0; i < numLayers; ++i)
    totalHeight += firstHeight * std::pow(growthRate, i);

  // Compute surface normals
  vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  normals->SetInputData(inputPoly);
  normals->ComputePointNormalsOn();
  normals->ComputeCellNormalsOff();
  normals->SplittingOff();
  normals->ConsistencyOn();
  normals->Update();

  vtkPolyData* normalsPoly = normals->GetOutput();
  vtkDataArray* pointNormals = normalsPoly->GetPointData()->GetNormals();

  if (!pointNormals)
  {
    QMessageBox::warning(nullptr, "Boundary Layer",
      "Failed to compute surface normals.");
    return;
  }

  // Create the innermost boundary layer surface by offsetting along normals
  vtkSmartPointer<vtkPoints> newPoints = vtkSmartPointer<vtkPoints>::New();
  newPoints->SetNumberOfPoints(normalsPoly->GetNumberOfPoints());

  double sign = directionInward ? -1.0 : 1.0;

  for (vtkIdType i = 0; i < normalsPoly->GetNumberOfPoints(); ++i)
  {
    double pt[3];
    normalsPoly->GetPoint(i, pt);
    double n[3];
    pointNormals->GetTuple(i, n);

    pt[0] += sign * totalHeight * n[0];
    pt[1] += sign * totalHeight * n[1];
    pt[2] += sign * totalHeight * n[2];

    newPoints->SetPoint(i, pt);
  }

  vtkSmartPointer<vtkPolyData> blPoly = vtkSmartPointer<vtkPolyData>::New();
  blPoly->DeepCopy(normalsPoly);
  blPoly->SetPoints(newPoints);

  // Create or update the preview node
  std::string previewName = m_CurrentModelNode->GetName() + "_boundary_layer_preview";

  // Remove existing preview node if present
  mitk::DataStorage::SetOfObjects::ConstPointer children =
    GetDataStorage()->GetDerivations(m_CurrentModelNode);
  for (auto it = children->Begin(); it != children->End(); ++it)
  {
    if (it->Value()->GetName() == previewName)
    {
      GetDataStorage()->Remove(it->Value());
      break;
    }
  }

  mitk::Surface::Pointer blSurface = mitk::Surface::New();
  blSurface->SetVtkPolyData(blPoly);

  mitk::DataNode::Pointer blNode = mitk::DataNode::New();
  blNode->SetData(blSurface);
  blNode->SetName(previewName);
  blNode->SetColor(0.0f, 0.8f, 0.0f);
  blNode->SetFloatProperty("opacity", 0.4f);

  GetDataStorage()->Add(blNode, m_CurrentModelNode);

  // Store boundary layer parameters on the model node
  m_CurrentModelNode->SetIntProperty("xq.mesh.bl.numLayers", numLayers);
  m_CurrentModelNode->SetDoubleProperty("xq.mesh.bl.firstHeight", firstHeight);
  m_CurrentModelNode->SetDoubleProperty("xq.mesh.bl.growthRate", growthRate);
  m_CurrentModelNode->SetBoolProperty("xq.mesh.bl.directionInward", directionInward);
  PersistMeshParametersToMetadata();

  // Update status label
  m_Ui->lblBLStatus->setText(
    QString("Boundary layer: %1 layers, total height = %2")
      .arg(numLayers)
      .arg(totalHeight, 0, 'f', 4));

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_GridGenerationView::AddSphereRefinementRegion()
{
  QDialog dialog(nullptr);
  dialog.setWindowTitle("Add Sphere Refinement Region");

  QFormLayout* formLayout = new QFormLayout(&dialog);

  QLineEdit* nameEdit = new QLineEdit("SphereRegion", &dialog);
  formLayout->addRow("Name:", nameEdit);

  double defaultCx = 0.0, defaultCy = 0.0, defaultCz = 0.0;
  double defaultRadius = 1.0;
  double defaultTargetSize = m_Ui->spinGlobalEdgeSize->value() * 0.5;

  if (m_CurrentModelNode.IsNotNull())
  {
    vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
    if (polyData)
    {
      double bounds[6];
      polyData->GetBounds(bounds);
      defaultCx = (bounds[0] + bounds[1]) * 0.5;
      defaultCy = (bounds[2] + bounds[3]) * 0.5;
      defaultCz = (bounds[4] + bounds[5]) * 0.5;
      double extent = std::sqrt(
        std::pow(bounds[1] - bounds[0], 2) +
        std::pow(bounds[3] - bounds[2], 2) +
        std::pow(bounds[5] - bounds[4], 2));
      defaultRadius = extent * 0.1;
    }
  }

  QDoubleSpinBox* spinCx = new QDoubleSpinBox(&dialog);
  spinCx->setRange(-1e6, 1e6); spinCx->setDecimals(4); spinCx->setValue(defaultCx);
  formLayout->addRow("Center X:", spinCx);

  QDoubleSpinBox* spinCy = new QDoubleSpinBox(&dialog);
  spinCy->setRange(-1e6, 1e6); spinCy->setDecimals(4); spinCy->setValue(defaultCy);
  formLayout->addRow("Center Y:", spinCy);

  QDoubleSpinBox* spinCz = new QDoubleSpinBox(&dialog);
  spinCz->setRange(-1e6, 1e6); spinCz->setDecimals(4); spinCz->setValue(defaultCz);
  formLayout->addRow("Center Z:", spinCz);

  QDoubleSpinBox* spinRadius = new QDoubleSpinBox(&dialog);
  spinRadius->setRange(0.001, 1e6); spinRadius->setDecimals(4); spinRadius->setValue(defaultRadius);
  formLayout->addRow("Radius:", spinRadius);

  QDoubleSpinBox* spinTarget = new QDoubleSpinBox(&dialog);
  spinTarget->setRange(0.001, 1e6); spinTarget->setDecimals(4); spinTarget->setValue(defaultTargetSize);
  formLayout->addRow("Target Mesh Size:", spinTarget);

  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  formLayout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return;

  QString name = nameEdit->text();
  if (name.isEmpty())
    name = "SphereRegion";

  int row = m_Ui->tableRefinementRegions->rowCount();
  m_Ui->tableRefinementRegions->insertRow(row);
  m_Ui->tableRefinementRegions->setItem(row, 0, new QTableWidgetItem(name));
  m_Ui->tableRefinementRegions->setItem(row, 1, new QTableWidgetItem("Sphere"));
  m_Ui->tableRefinementRegions->setItem(row, 2, new QTableWidgetItem(QString::number(spinCx->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 3, new QTableWidgetItem(QString::number(spinCy->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 4, new QTableWidgetItem(QString::number(spinCz->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 5, new QTableWidgetItem(QString::number(spinRadius->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 6, new QTableWidgetItem(QString::number(spinTarget->value(), 'f', 4)));

  m_Ui->lblRegionCount->setText(
    QString("%1 refinement regions defined").arg(m_Ui->tableRefinementRegions->rowCount()));
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::AddCylinderRefinementRegion()
{
  QDialog dialog(nullptr);
  dialog.setWindowTitle("Add Cylinder Refinement Region");

  QFormLayout* formLayout = new QFormLayout(&dialog);

  QLineEdit* nameEdit = new QLineEdit("CylinderRegion", &dialog);
  formLayout->addRow("Name:", nameEdit);

  double defaultCx = 0.0, defaultCy = 0.0, defaultCz = 0.0;
  double defaultRadius = 1.0;
  double defaultLength = 2.0;
  double defaultTargetSize = m_Ui->spinGlobalEdgeSize->value() * 0.5;

  if (m_CurrentModelNode.IsNotNull())
  {
    vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
    if (polyData)
    {
      double bounds[6];
      polyData->GetBounds(bounds);
      defaultCx = (bounds[0] + bounds[1]) * 0.5;
      defaultCy = (bounds[2] + bounds[3]) * 0.5;
      defaultCz = (bounds[4] + bounds[5]) * 0.5;
      double extent = std::sqrt(
        std::pow(bounds[1] - bounds[0], 2) +
        std::pow(bounds[3] - bounds[2], 2) +
        std::pow(bounds[5] - bounds[4], 2));
      defaultRadius = extent * 0.1;
      defaultLength = extent * 0.2;
    }
  }

  QDoubleSpinBox* spinCx = new QDoubleSpinBox(&dialog);
  spinCx->setRange(-1e6, 1e6); spinCx->setDecimals(4); spinCx->setValue(defaultCx);
  formLayout->addRow("Center X:", spinCx);

  QDoubleSpinBox* spinCy = new QDoubleSpinBox(&dialog);
  spinCy->setRange(-1e6, 1e6); spinCy->setDecimals(4); spinCy->setValue(defaultCy);
  formLayout->addRow("Center Y:", spinCy);

  QDoubleSpinBox* spinCz = new QDoubleSpinBox(&dialog);
  spinCz->setRange(-1e6, 1e6); spinCz->setDecimals(4); spinCz->setValue(defaultCz);
  formLayout->addRow("Center Z:", spinCz);

  QDoubleSpinBox* spinRadius = new QDoubleSpinBox(&dialog);
  spinRadius->setRange(0.001, 1e6); spinRadius->setDecimals(4); spinRadius->setValue(defaultRadius);
  formLayout->addRow("Radius:", spinRadius);

  QDoubleSpinBox* spinLength = new QDoubleSpinBox(&dialog);
  spinLength->setRange(0.001, 1e6); spinLength->setDecimals(4); spinLength->setValue(defaultLength);
  formLayout->addRow("Length:", spinLength);

  QComboBox* comboAxis = new QComboBox(&dialog);
  comboAxis->addItems(QStringList() << "X" << "Y" << "Z");
  comboAxis->setCurrentIndex(2);
  formLayout->addRow("Axis Direction:", comboAxis);

  QDoubleSpinBox* spinTarget = new QDoubleSpinBox(&dialog);
  spinTarget->setRange(0.001, 1e6); spinTarget->setDecimals(4); spinTarget->setValue(defaultTargetSize);
  formLayout->addRow("Target Mesh Size:", spinTarget);

  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  formLayout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return;

  QString name = nameEdit->text();
  if (name.isEmpty())
    name = "CylinderRegion";

  // Encode radius and length together in the Radius/Size column
  QString sizeStr = QString("R=%1 L=%2 Axis=%3")
    .arg(spinRadius->value(), 0, 'f', 4)
    .arg(spinLength->value(), 0, 'f', 4)
    .arg(comboAxis->currentText());

  int row = m_Ui->tableRefinementRegions->rowCount();
  m_Ui->tableRefinementRegions->insertRow(row);
  m_Ui->tableRefinementRegions->setItem(row, 0, new QTableWidgetItem(name));
  m_Ui->tableRefinementRegions->setItem(row, 1, new QTableWidgetItem("Cylinder"));
  m_Ui->tableRefinementRegions->setItem(row, 2, new QTableWidgetItem(QString::number(spinCx->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 3, new QTableWidgetItem(QString::number(spinCy->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 4, new QTableWidgetItem(QString::number(spinCz->value(), 'f', 4)));
  m_Ui->tableRefinementRegions->setItem(row, 5, new QTableWidgetItem(sizeStr));
  m_Ui->tableRefinementRegions->setItem(row, 6, new QTableWidgetItem(QString::number(spinTarget->value(), 'f', 4)));

  m_Ui->lblRegionCount->setText(
    QString("%1 refinement regions defined").arg(m_Ui->tableRefinementRegions->rowCount()));
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::RemoveRefinementRegion()
{
  int row = m_Ui->tableRefinementRegions->currentRow();
  if (row < 0)
  {
    QMessageBox::information(nullptr, "Remove Region",
      "Please select a region to remove.");
    return;
  }

  QTableWidgetItem* nameItem = m_Ui->tableRefinementRegions->item(row, 0);
  if (nameItem)
  {
    std::string vizName = "RefRegion_" + nameItem->text().toStdString();
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = GetDataStorage()->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      if (it->Value()->GetName() == vizName)
      {
        GetDataStorage()->Remove(it->Value());
        break;
      }
    }
  }

  m_Ui->tableRefinementRegions->removeRow(row);
  m_Ui->lblRegionCount->setText(
    QString("%1 refinement regions defined").arg(m_Ui->tableRefinementRegions->rowCount()));
  PersistMeshParametersToMetadata();
}

void xq_GridGenerationView::VisualizeRefinementRegions()
{
  int numRegions = m_Ui->tableRefinementRegions->rowCount();
  if (numRegions == 0)
  {
    QMessageBox::information(nullptr, "Visualize Regions",
      "No refinement regions defined.");
    return;
  }

  mitk::DataNode::Pointer parentNode = m_CurrentModelNode;

  for (int row = 0; row < numRegions; ++row)
  {
    QTableWidgetItem* nameItem = m_Ui->tableRefinementRegions->item(row, 0);
    QTableWidgetItem* typeItem = m_Ui->tableRefinementRegions->item(row, 1);
    if (!nameItem || !typeItem)
      continue;

    QString name = nameItem->text();
    QString type = typeItem->text();
    std::string nodeName = "RefRegion_" + name.toStdString();

    // Remove existing visualization node with this name
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = GetDataStorage()->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      if (it->Value()->GetName() == nodeName)
      {
        GetDataStorage()->Remove(it->Value());
        break;
      }
    }

    double cx = m_Ui->tableRefinementRegions->item(row, 2)->text().toDouble();
    double cy = m_Ui->tableRefinementRegions->item(row, 3)->text().toDouble();
    double cz = m_Ui->tableRefinementRegions->item(row, 4)->text().toDouble();

    vtkSmartPointer<vtkPolyData> regionPoly;

    if (type == "Sphere")
    {
      double radius = m_Ui->tableRefinementRegions->item(row, 5)->text().toDouble();

      vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
      sphere->SetCenter(cx, cy, cz);
      sphere->SetRadius(radius);
      sphere->SetPhiResolution(24);
      sphere->SetThetaResolution(24);
      sphere->Update();
      regionPoly = sphere->GetOutput();
    }
    else if (type == "Cylinder")
    {
      QString sizeStr = m_Ui->tableRefinementRegions->item(row, 5)->text();
      double radius = 1.0, length = 2.0;
      QString axis = "Z";

      // Parse "R=... L=... Axis=..."
      QStringList parts = sizeStr.split(' ', Qt::SkipEmptyParts);
      for (const QString& part : parts)
      {
        if (part.startsWith("R="))
          radius = part.mid(2).toDouble();
        else if (part.startsWith("L="))
          length = part.mid(2).toDouble();
        else if (part.startsWith("Axis="))
          axis = part.mid(5);
      }

      vtkSmartPointer<vtkCylinderSource> cylinder = vtkSmartPointer<vtkCylinderSource>::New();
      cylinder->SetRadius(radius);
      cylinder->SetHeight(length);
      cylinder->SetResolution(24);
      cylinder->SetCenter(0.0, 0.0, 0.0);
      cylinder->Update();

      // vtkCylinderSource is aligned along Y by default; rotate to desired axis
      vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
      transform->Translate(cx, cy, cz);
      if (axis == "X")
        transform->RotateZ(90.0);
      else if (axis == "Z")
        transform->RotateX(90.0);
      // Y axis needs no rotation (default)

      vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter =
        vtkSmartPointer<vtkTransformPolyDataFilter>::New();
      transformFilter->SetInputConnection(cylinder->GetOutputPort());
      transformFilter->SetTransform(transform);
      transformFilter->Update();
      regionPoly = transformFilter->GetOutput();
    }
    else
    {
      continue;
    }

    mitk::Surface::Pointer regionSurface = mitk::Surface::New();
    regionSurface->SetVtkPolyData(regionPoly);

    mitk::DataNode::Pointer regionNode = mitk::DataNode::New();
    regionNode->SetData(regionSurface);
    regionNode->SetName(nodeName);
    regionNode->SetColor(1.0f, 1.0f, 0.0f);
    regionNode->SetFloatProperty("opacity", 0.25f);

    if (parentNode.IsNotNull())
      GetDataStorage()->Add(regionNode, parentNode);
    else
      GetDataStorage()->Add(regionNode);
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Visualize Regions",
    QString("Visualized %1 refinement region(s).").arg(numRegions));
}
