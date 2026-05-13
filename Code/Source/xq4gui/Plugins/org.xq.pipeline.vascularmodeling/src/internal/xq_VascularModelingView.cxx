#include "xq_VascularModelingView.h"
#include "xq_ModelCreate.h"
#include "ui_xq_VascularModelingView.h"

#include <xq_Model.h>
#include <xq_GeometryOp.h>
#include <xq_GeometryUtils.h>
#include <xq_VascularGeometry.h>
#include <xq_PolyGeometry.h>
#include <xq_ModelPipeline.h>
#include <xq_ModelQuality.h>
#include <xq_ProfileGroup.h>
#include <xq_LumenProfile.h>
#include <xq_ContourGroup.h>
#include <xq_ContourGroupMigration.h>
#include <xq_SegmentationUtils.h>
#include <xq_VesselCenterline.h>
#include <xq_UndoHelper.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>
#include <mitkLogMacros.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>

#include <vtkCellData.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkStringArray.h>
#include <vtkPolyData.h>
#include <vtkSTLWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkAppendPolyData.h>
#include <vtkBooleanOperationPolyDataFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkTriangleFilter.h>
#include <vtkCell.h>
#include <vtkDecimatePro.h>
#include <vtkPolyDataNormals.h>
#include <vtkFillHolesFilter.h>
#include <vtkFeatureEdges.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkPointLocator.h>
#include <vtkIdList.h>
#include <vtkPoints.h>

#include <QComboBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QHeaderView>

#include <cmath>
#include <set>

#include <vtkMath.h>

#include "xq_ExtractCenterlinesAction.h"

namespace {

const int OpRESTORE_SURFACE = 60100;

vtkPolyData* GetModelPolyData(mitk::DataNode::Pointer node)
{
  if (node.IsNull() || node->GetData() == nullptr)
    return nullptr;

  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  if (surface != nullptr)
    return surface->GetVtkPolyData();

  auto* model = dynamic_cast<xq_Model*>(node->GetData());
  auto* element = model ? model->GetModelElement(0) : nullptr;
  auto polyData = element ? element->GetWholeVtkPolyData() : nullptr;
  return polyData;
}

bool SetModelPolyData(mitk::DataNode::Pointer node, vtkPolyData* polyData)
{
  if (node.IsNull() || node->GetData() == nullptr || polyData == nullptr)
    return false;

  auto* surface = dynamic_cast<mitk::Surface*>(node->GetData());
  if (surface != nullptr)
  {
    surface->SetVtkPolyData(polyData);
    surface->Modified();
    node->SetData(surface);
    node->Modified();
    return true;
  }

  auto* model = dynamic_cast<xq_Model*>(node->GetData());
  if (model != nullptr)
  {
    auto geometry = std::make_unique<xq_PolyGeometry>();
    auto copy = vtkSmartPointer<vtkPolyData>::New();
    copy->DeepCopy(polyData);
    geometry->SetWholeVtkPolyData(copy);
    model->SetModelElement(std::move(geometry), 0);
    model->Modified();
    node->Modified();
    return true;
  }

  return false;
}

class RestoreSurfaceOp : public mitk::Operation
{
public:
    RestoreSurfaceOp(vtkSmartPointer<vtkPolyData> state)
        : mitk::Operation(OpRESTORE_SURFACE), m_State(state) {}
    vtkSmartPointer<vtkPolyData> GetState() const { return m_State; }
private:
    vtkSmartPointer<vtkPolyData> m_State;
};

// Lightweight undo actor for restoring surface polydata
class SurfaceUndoActor : public mitk::OperationActor
{
public:
    static SurfaceUndoActor* GetInstance()
    {
        static SurfaceUndoActor instance;
        return &instance;
    }

    void SetTargetSurface(mitk::Surface* surf) { m_TargetSurface = surf; m_TargetNode = nullptr; }
    void SetTargetNode(mitk::DataNode::Pointer node) { m_TargetNode = node; m_TargetSurface = nullptr; }

    void ExecuteOperation(mitk::Operation* op) override
    {
        if (!op || op->GetOperationType() != OpRESTORE_SURFACE)
            return;
        auto* restoreOp = dynamic_cast<RestoreSurfaceOp*>(op);
        if (!restoreOp || !restoreOp->GetState())
            return;

        if (m_TargetNode.IsNotNull())
        {
            SetModelPolyData(m_TargetNode, restoreOp->GetState());
            return;
        }

        if (m_TargetSurface)
        {
            m_TargetSurface->SetVtkPolyData(restoreOp->GetState());
            m_TargetSurface->Modified();
        }
    }

private:
    SurfaceUndoActor() = default;
    mitk::Surface* m_TargetSurface = nullptr;
    mitk::DataNode::Pointer m_TargetNode;
};

std::vector<xq_ProfilePlacementFrame> BuildPlacementFrames(xq_VesselCenterline* centerline)
{
  std::vector<xq_ProfilePlacementFrame> frames;
  if (!centerline)
    return frames;

  auto* segment = centerline->GetSegment();
  if (!segment)
    return frames;

  if (segment->GetTraceVertexCount() == 0 && segment->GetAnchorCount() >= 2)
    segment->Interpolate();

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

xq_VesselCenterline* FindPathByName(
  mitk::DataStorage* dataStorage, const std::string& pathName)
{
  if (!dataStorage || pathName.empty())
    return nullptr;

  auto allNodes = dataStorage->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    bool isPath = false;
    it->Value()->GetBoolProperty("xq.pathplanning.path", isPath);
    if (!isPath || it->Value()->GetName() != pathName)
      continue;

    return dynamic_cast<xq_VesselCenterline*>(it->Value()->GetData());
  }

  return nullptr;
}

std::vector<xq_ProfilePlacementFrame> ResolvePlacementFramesForPath(
  mitk::DataStorage* dataStorage, const std::string& pathName)
{
  if (!dataStorage || pathName.empty())
    return {};

  auto* centerline = FindPathByName(dataStorage, pathName);
  return BuildPlacementFrames(centerline);
}

} // anonymous namespace

const QString xq_VascularModelingView::VIEW_ID = "org.xq.views.modeling";

xq_VascularModelingView::xq_VascularModelingView()
  : m_Ui(nullptr)
  , m_CurrentModelNode(nullptr)
{
}

xq_VascularModelingView::~xq_VascularModelingView()
{
  delete m_Ui;
}

void xq_VascularModelingView::CreateQtPartControl(QWidget* parent)
{
  m_Ui = new Ui::xq_VascularModelingView;
  m_Ui->setupUi(parent);

  // Connect create tab buttons
  connect(m_Ui->btnCreateModel, &QPushButton::clicked,
          this, &xq_VascularModelingView::CreateModel);
  connect(m_Ui->btnDeleteModel, &QPushButton::clicked,
          this, &xq_VascularModelingView::DeleteModel);

  // Connect edit tab buttons
  connect(m_Ui->btnChangeFaceColor, &QPushButton::clicked,
          this, &xq_VascularModelingView::ChangeFaceColor);
  connect(m_Ui->btnToggleFaceVisibility, &QPushButton::clicked,
          this, &xq_VascularModelingView::ToggleFaceVisibility);
  connect(m_Ui->btnBooleanUnion, &QPushButton::clicked,
          this, &xq_VascularModelingView::BooleanUnion);
  connect(m_Ui->btnBooleanSubtract, &QPushButton::clicked,
          this, &xq_VascularModelingView::BooleanSubtract);
  connect(m_Ui->btnApplyFillet, &QPushButton::clicked,
          this, &xq_VascularModelingView::ApplyFillet);

  // Connect export tab buttons
  connect(m_Ui->btnExportModel, &QPushButton::clicked,
          this, &xq_VascularModelingView::ExportModel);
  connect(m_Ui->btnModelStats, &QPushButton::clicked,
          this, &xq_VascularModelingView::ShowModelStatistics);
  connect(m_Ui->btnExtractCenterlines, &QPushButton::clicked,
          this, &xq_VascularModelingView::ExtractCenterlines);

  // Connect decimation and surface ops buttons
  connect(m_Ui->btnDecimate, &QPushButton::clicked,
          this, &xq_VascularModelingView::DecimateSurface);
  connect(m_Ui->btnComputeNormals, &QPushButton::clicked,
          this, &xq_VascularModelingView::ComputeNormals);
  connect(m_Ui->btnCleanSurface, &QPushButton::clicked,
          this, &xq_VascularModelingView::CleanSurface);
  connect(m_Ui->btnFillHoles, &QPushButton::clicked,
          this, &xq_VascularModelingView::FillHoles);

  // Configure face table
  m_Ui->tableFaces->setColumnCount(4);
  m_Ui->tableFaces->setHorizontalHeaderLabels(
    QStringList() << "Visible" << "Name" << "Type" << "Color");
  m_Ui->tableFaces->horizontalHeader()->setStretchLastSection(true);
  m_Ui->tableFaces->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Allow editing the type column (col 2) so users can assign BC roles
  connect(m_Ui->tableFaces, &QTableWidget::cellChanged,
          this, [this](int row, int col) {
    if (col != 2 || !m_CurrentModelNode)
      return;
    QTableWidgetItem* typeItem = m_Ui->tableFaces->item(row, 2);
    QTableWidgetItem* nameItem = m_Ui->tableFaces->item(row, 1);
    if (!typeItem || !nameItem)
      return;
    QString newType = typeItem->text();
    int faceId = -1;
    // Try to find face id from geometry
    auto* model = dynamic_cast<xq_Model*>(m_CurrentModelNode->GetData());
    auto* geom = model ? model->GetModelElement(0) : nullptr;
    if (geom) {
      faceId = geom->GetFaceIdByName(nameItem->text().toStdString());
      if (faceId < 0) {
        // Try extracting ID from "face_N" format
        QString name = nameItem->text();
        if (name.startsWith("face_"))
          faceId = name.mid(5).toInt();
      }
      if (faceId >= 0) {
        const FaceInfo* existing = geom->GetFaceInfo(faceId);
        FaceInfo updated;
        if (existing) {
          updated = *existing;
        } else {
          updated.id = faceId;
          updated.name = nameItem->text().toStdString();
        }
        updated.type = newType.toStdString();
        geom->SetFaceInfo(faceId, updated);
        m_CurrentModelNode->Modified();
      }
    }
  });
}

void xq_VascularModelingView::SetFocus()
{
  if (m_Ui && m_Ui->comboModelSelector)
    m_Ui->comboModelSelector->setFocus();
}

void xq_VascularModelingView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer /*source*/,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  ClearFaceTable();
  m_CurrentModelNode = nullptr;

  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      if (GetModelPolyData(node) != nullptr)
      {
        m_CurrentModelNode = node;
        PopulateFaceTable(node);
        break;
      }
    }
  }

  bool hasModel = m_CurrentModelNode.IsNotNull();
  m_Ui->btnDeleteModel->setEnabled(hasModel);
  m_Ui->btnExportModel->setEnabled(hasModel);
  m_Ui->btnModelStats->setEnabled(hasModel);
  m_Ui->btnExtractCenterlines->setEnabled(hasModel);
  m_Ui->btnBooleanUnion->setEnabled(hasModel);
  m_Ui->btnBooleanSubtract->setEnabled(hasModel);
  m_Ui->btnApplyFillet->setEnabled(hasModel);
  m_Ui->btnChangeFaceColor->setEnabled(hasModel);
  m_Ui->btnToggleFaceVisibility->setEnabled(hasModel);
  m_Ui->btnDecimate->setEnabled(hasModel);
  m_Ui->btnComputeNormals->setEnabled(hasModel);
  m_Ui->btnCleanSurface->setEnabled(hasModel);
  m_Ui->btnFillHoles->setEnabled(hasModel);
}

void xq_VascularModelingView::PopulateFaceTable(mitk::DataNode::Pointer modelNode)
{
  if (modelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(modelNode);
  if (polyData == nullptr)
    return;

  // Try to get FaceInfo from the xq_VascularGeometry if available
  // (preferred over raw cell data — carries user-assigned face types)
  xq_VascularGeometry* geometry = nullptr;
  auto* model = dynamic_cast<xq_Model*>(modelNode->GetData());
  if (model)
    geometry = model->GetModelElement(0);

  m_Ui->tableFaces->setRowCount(0);

  // If geometry has face infos, use those directly (restores user-assigned types)
  if (geometry && geometry->GetFaceNumber() > 0)
  {
    const auto& faceInfos = geometry->GetAllFaceInfos();
    for (const auto& fi : faceInfos)
    {
      int row = m_Ui->tableFaces->rowCount();
      m_Ui->tableFaces->insertRow(row);

      QTableWidgetItem* visItem = new QTableWidgetItem();
      visItem->setCheckState(fi.visible ? Qt::Checked : Qt::Unchecked);
      m_Ui->tableFaces->setItem(row, 0, visItem);

      m_Ui->tableFaces->setItem(row, 1,
        new QTableWidgetItem(QString::fromStdString(fi.name)));

      // Type column is user-editable so users can assign BC roles
      // (wall / cap / inlet / outlet)
      QTableWidgetItem* typeItem = new QTableWidgetItem(QString::fromStdString(fi.type));
      typeItem->setFlags(typeItem->flags() | Qt::ItemIsEditable);
      m_Ui->tableFaces->setItem(row, 2, typeItem);

      QTableWidgetItem* colorItem = new QTableWidgetItem();
      colorItem->setBackground(QBrush(
        QColor::fromRgbF(fi.color[0], fi.color[1], fi.color[2])));
      m_Ui->tableFaces->setItem(row, 3, colorItem);
    }
    return;
  }

  // Read embedded face metadata from VTP field data (set by EmbedFaceInfoToPolyData)
  // This allows face types/names to survive save/load round-trips even for
  // mitk::Surface nodes that don't carry xq_VascularGeometry.
  auto readFieldStr = [](vtkFieldData* fd, const char* name, vtkIdType idx) -> std::string {
    if (!fd) return {};
    auto* arr = vtkStringArray::SafeDownCast(fd->GetArray(name));
    if (arr && idx < arr->GetNumberOfTuples())
      return arr->GetValue(idx);
    return {};
  };
  auto readFieldInt = [](vtkFieldData* fd, const char* name, vtkIdType idx) -> int {
    if (!fd) return -1;
    auto* arr = vtkIntArray::SafeDownCast(fd->GetArray(name));
    if (arr && idx < arr->GetNumberOfTuples())
      return arr->GetValue(idx);
    return -1;
  };

  // Fallback: enumerate faces from cell data FaceIds array
  vtkCellData* cellData = polyData->GetCellData();
  vtkFieldData* fieldData = polyData->GetFieldData();
  if (cellData == nullptr)
    return;

  vtkDataArray* faceIdsArr = cellData->GetArray("FaceIds");
  if (faceIdsArr == nullptr)
  {
    MITK_WARN << "PopulateFaceTable: 'FaceIds' array not found in cell data, treating as single-face model.";
    int row = m_Ui->tableFaces->rowCount();
    m_Ui->tableFaces->insertRow(row);

    QTableWidgetItem* visItem = new QTableWidgetItem();
    visItem->setCheckState(Qt::Checked);
    m_Ui->tableFaces->setItem(row, 0, visItem);

    m_Ui->tableFaces->setItem(row, 1,
      new QTableWidgetItem(modelNode->GetName().c_str()));
    m_Ui->tableFaces->setItem(row, 2,
      new QTableWidgetItem("wall"));

    float rgb[3] = {1.0f, 1.0f, 1.0f};
    modelNode->GetColor(rgb);
    QTableWidgetItem* colorItem = new QTableWidgetItem();
    colorItem->setBackground(QBrush(
      QColor::fromRgbF(rgb[0], rgb[1], rgb[2])));
    m_Ui->tableFaces->setItem(row, 3, colorItem);
    return;
  }

  // Collect unique face IDs
  std::set<int> uniqueIds;
  for (vtkIdType i = 0; i < faceIdsArr->GetNumberOfTuples(); ++i)
    uniqueIds.insert(static_cast<int>(faceIdsArr->GetTuple1(i)));

  // Try to read embedded face metadata from field data (VTP round-trip)
  vtkIntArray* embFaceIds = fieldData ? vtkIntArray::SafeDownCast(fieldData->GetArray("XQ_FaceIds")) : nullptr;

  for (int faceId : uniqueIds)
  {
    int row = m_Ui->tableFaces->rowCount();
    m_Ui->tableFaces->insertRow(row);

    // Look up face info: prefer geometry, then embedded field data, then defaults
    const FaceInfo* fi = geometry ? geometry->GetFaceInfo(faceId) : nullptr;

    // Search embedded metadata for this faceId
    int embIdx = -1;
    if (!fi && embFaceIds)
    {
      for (vtkIdType j = 0; j < embFaceIds->GetNumberOfTuples(); ++j)
      {
        if (embFaceIds->GetValue(j) == faceId)
        {
          embIdx = static_cast<int>(j);
          break;
        }
      }
    }

    QString faceName = fi ? QString::fromStdString(fi->name) : QString("face_%1").arg(faceId);
    if (!fi && embIdx >= 0)
    {
      auto embName = readFieldStr(fieldData, "XQ_FaceNames", embIdx);
      if (!embName.empty())
        faceName = QString::fromStdString(embName);
    }

    QString faceType = fi ? QString::fromStdString(fi->type) : ((faceId == 0) ? "wall" : "cap");
    if (!fi && embIdx >= 0)
    {
      auto embType = readFieldStr(fieldData, "XQ_FaceTypes", embIdx);
      if (!embType.empty())
        faceType = QString::fromStdString(embType);
    }

    bool visible = fi ? fi->visible : true;
    if (!fi && embIdx >= 0)
    {
      int embVis = readFieldInt(fieldData, "XQ_FaceVisible", embIdx);
      if (embVis >= 0)
        visible = embVis != 0;
    }

    QTableWidgetItem* visItem = new QTableWidgetItem();
    visItem->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
    m_Ui->tableFaces->setItem(row, 0, visItem);

    m_Ui->tableFaces->setItem(row, 1, new QTableWidgetItem(faceName));

    m_Ui->tableFaces->setItem(row, 2, new QTableWidgetItem(faceType));

    QTableWidgetItem* colorItem = new QTableWidgetItem();
    if (fi)
      colorItem->setBackground(QBrush(
        QColor::fromRgbF(fi->color[0], fi->color[1], fi->color[2])));
    else if (embIdx >= 0)
    {
      auto* colorArr = fieldData ? vtkIntArray::SafeDownCast(fieldData->GetArray("XQ_FaceColor")) : nullptr;
      if (colorArr && embIdx < colorArr->GetNumberOfTuples())
      {
        colorItem->setBackground(QBrush(QColor(
          colorArr->GetComponent(embIdx, 0),
          colorArr->GetComponent(embIdx, 1),
          colorArr->GetComponent(embIdx, 2))));
      }
      else
        colorItem->setBackground(QBrush(QColor(200, 200, 200)));
    }
    else
      colorItem->setBackground(QBrush(QColor(200, 200, 200)));
    m_Ui->tableFaces->setItem(row, 3, colorItem);
  }
}

void xq_VascularModelingView::ClearFaceTable()
{
  if (m_Ui && m_Ui->tableFaces)
    m_Ui->tableFaces->setRowCount(0);
}

void xq_VascularModelingView::CreateModel()
{
  xq_ModelCreate dialog(GetDataStorage(), this->GetSite()->GetWorkbenchWindow()->GetShell()->GetControl());
  if (dialog.exec() == QDialog::Accepted)
  {
    QString modelName = dialog.GetModelName();
    QString modelType = dialog.GetModelType();
    int numSampling = dialog.GetNumSamplingPoints();

    if (modelName.isEmpty())
    {
      QMessageBox::warning(nullptr, "Solid Modeling", "Please enter a model name.");
      return;
    }

    const auto createResult = xq_ModelPipelineService::CreateModel(
      GetDataStorage(),
      {modelName.toStdString(), modelType.toStdString(), numSampling});
    if (!createResult.ok || createResult.node.IsNull())
    {
      QStringList messages;
      for (const auto& diagnostic : createResult.diagnostics)
        messages << QString::fromStdString(diagnostic.message);
      QMessageBox::warning(nullptr, "Solid Modeling",
        messages.isEmpty() ? "Failed to create model." : messages.join("\n"));
      return;
    }

    m_CurrentModelNode = createResult.node;
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_VascularModelingView::DeleteModel()
{
  if (m_CurrentModelNode.IsNull())
    return;

  QMessageBox::StandardButton reply = QMessageBox::question(
    nullptr, "Delete Model",
    QString("Delete model '%1'?").arg(m_CurrentModelNode->GetName().c_str()),
    QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes)
  {
    GetDataStorage()->Remove(m_CurrentModelNode);
    m_CurrentModelNode = nullptr;
    ClearFaceTable();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_VascularModelingView::ChangeFaceColor()
{
  if (m_CurrentModelNode.IsNull())
    return;

  int row = m_Ui->tableFaces->currentRow();
  if (row < 0)
  {
    QMessageBox::information(nullptr, "Solid Modeling", "Select a face first.");
    return;
  }

  QTableWidgetItem* colorItem = m_Ui->tableFaces->item(row, 3);
  QTableWidgetItem* nameItem = m_Ui->tableFaces->item(row, 1);
  QColor current = colorItem ? colorItem->background().color() : Qt::white;
  QColor chosen = QColorDialog::getColor(current, nullptr, "Select Face Color");
  if (chosen.isValid() && colorItem)
  {
    colorItem->setBackground(QBrush(chosen));

    // Update FaceInfo in geometry so color persists across saves
    auto* model = dynamic_cast<xq_Model*>(m_CurrentModelNode->GetData());
    auto* geom = model ? model->GetModelElement(0) : nullptr;
    if (geom && nameItem)
    {
      int faceId = geom->GetFaceIdByName(nameItem->text().toStdString());
      if (faceId >= 0)
      {
        const FaceInfo* existing = geom->GetFaceInfo(faceId);
        FaceInfo updated;
        if (existing)
          updated = *existing;
        else
        {
          updated.id = faceId;
          updated.name = nameItem->text().toStdString();
        }
        updated.color = {static_cast<float>(chosen.redF()),
                         static_cast<float>(chosen.greenF()),
                         static_cast<float>(chosen.blueF())};
        geom->SetFaceInfo(faceId, updated);
      }
    }

    // If single-face model, also set node color
    if (m_Ui->tableFaces->rowCount() == 1)
    {
      m_CurrentModelNode->SetColor(chosen.redF(), chosen.greenF(), chosen.blueF());
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }

    m_CurrentModelNode->Modified();
  }
}

void xq_VascularModelingView::ToggleFaceVisibility()
{
  if (m_CurrentModelNode.IsNull())
    return;

  int row = m_Ui->tableFaces->currentRow();
  if (row < 0)
    return;

  QTableWidgetItem* visItem = m_Ui->tableFaces->item(row, 0);
  QTableWidgetItem* nameItem = m_Ui->tableFaces->item(row, 1);
  bool newVisible = true;
  if (visItem)
  {
    Qt::CheckState state = visItem->checkState();
    newVisible = (state == Qt::Unchecked);
    visItem->setCheckState(newVisible ? Qt::Checked : Qt::Unchecked);
  }

  // Update FaceInfo in geometry
  auto* model = dynamic_cast<xq_Model*>(m_CurrentModelNode->GetData());
  auto* geom = model ? model->GetModelElement(0) : nullptr;
  if (geom && nameItem)
  {
    int faceId = geom->GetFaceIdByName(nameItem->text().toStdString());
    if (faceId >= 0)
    {
      const FaceInfo* existing = geom->GetFaceInfo(faceId);
      FaceInfo updated;
      if (existing)
        updated = *existing;
      else
      {
        updated.id = faceId;
        updated.name = nameItem->text().toStdString();
      }
      updated.visible = newVisible;
      geom->SetFaceInfo(faceId, updated);
    }
  }

  // For single-face model, toggle the whole node
  if (m_Ui->tableFaces->rowCount() == 1)
  {
    bool isVisible = true;
    m_CurrentModelNode->GetBoolProperty("visible", isVisible);
    m_CurrentModelNode->SetBoolProperty("visible", !isVisible);
  }

  m_CurrentModelNode->Modified();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::ExportModel()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (polyData == nullptr)
    return;

  QString selectedFilter;
  QString filePath = QFileDialog::getSaveFileName(
    nullptr, "Export Model",
    QString::fromStdString(m_CurrentModelNode->GetName()),
    "VTK PolyData (*.vtp);;STL (*.stl)",
    &selectedFilter);

  if (filePath.isEmpty())
    return;

  if (selectedFilter.contains("stl", Qt::CaseInsensitive))
  {
    vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputData(polyData);
    writer->Write();
  }
  else
  {
    vtkSmartPointer<vtkXMLPolyDataWriter> writer =
      vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputData(polyData);
    writer->Write();
  }

  QMessageBox::information(nullptr, "Export", "Model exported successfully.");
}

void xq_VascularModelingView::ExtractCenterlines()
{
  if (m_CurrentModelNode.IsNull())
    return;

  auto* action = new xq_ExtractCenterlinesAction(this);
  action->SetDataStorage(GetDataStorage());
  action->SetModelNode(m_CurrentModelNode);
  connect(action, &xq_ExtractCenterlinesAction::ExtractionFinished,
          action, &QObject::deleteLater);
  action->Execute();
}

void xq_VascularModelingView::BooleanUnion()
{
  if (m_CurrentModelNode.IsNull())
    return;

  mitk::DataStorage::Pointer ds = GetDataStorage();
  mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();

  mitk::DataNode::Pointer otherNode = nullptr;
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    if (it->Value() != m_CurrentModelNode && GetModelPolyData(it->Value()) != nullptr)
    {
      otherNode = it->Value();
      break;
    }
  }

  if (otherNode.IsNull())
  {
    QMessageBox::information(nullptr, "Boolean Union",
      "At least two surface models are required for union.");
    return;
  }

  vtkPolyData* surfA = GetModelPolyData(m_CurrentModelNode);
  vtkPolyData* surfB = GetModelPolyData(otherNode);

  if (!surfA || !surfB)
    return;

  vtkSmartPointer<vtkBooleanOperationPolyDataFilter> boolFilter =
    vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
  boolFilter->SetOperationToUnion();
  boolFilter->SetInputData(0, surfA);
  boolFilter->SetInputData(1, surfB);
  boolFilter->Update();

  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  cleaner->SetInputConnection(boolFilter->GetOutputPort());
  cleaner->Update();

  mitk::Surface::Pointer result = mitk::Surface::New();
  result->SetVtkPolyData(cleaner->GetOutput());

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetData(result);
  resultNode->SetName(m_CurrentModelNode->GetName() + "_union_" + otherNode->GetName());
  resultNode->SetColor(0.2f, 0.7f, 0.3f);

  // Copy pipeline metadata from source model so the result stays in the pipeline
  xq::pipeline::MarkNode(resultNode, xq::pipeline::Stage::Model);
  std::string sourcePath;
  if (m_CurrentModelNode->GetStringProperty(
        xq::pipeline::kSourcePathProperty, sourcePath) && !sourcePath.empty())
    xq::pipeline::SetStringProperty(resultNode, xq::pipeline::kSourcePathProperty, sourcePath);
  resultNode->SetStringProperty("xq.status", "modified");
  resultNode->SetStringProperty("xq.model.operation", "boolean_union");

  ds->Add(resultNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::BooleanSubtract()
{
  if (m_CurrentModelNode.IsNull())
    return;

  mitk::DataStorage::Pointer ds = GetDataStorage();
  mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();

  mitk::DataNode::Pointer otherNode = nullptr;
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    if (it->Value() != m_CurrentModelNode && GetModelPolyData(it->Value()) != nullptr)
    {
      otherNode = it->Value();
      break;
    }
  }

  if (otherNode.IsNull())
  {
    QMessageBox::information(nullptr, "Boolean Subtract",
      "At least two surface models are required for subtraction.");
    return;
  }

  vtkPolyData* surfA = GetModelPolyData(m_CurrentModelNode);
  vtkPolyData* surfB = GetModelPolyData(otherNode);

  if (!surfA || !surfB)
    return;

  vtkSmartPointer<vtkBooleanOperationPolyDataFilter> boolFilter =
    vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
  boolFilter->SetOperationToDifference();
  boolFilter->SetInputData(0, surfA);
  boolFilter->SetInputData(1, surfB);
  boolFilter->Update();

  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  cleaner->SetInputConnection(boolFilter->GetOutputPort());
  cleaner->Update();

  mitk::Surface::Pointer result = mitk::Surface::New();
  result->SetVtkPolyData(cleaner->GetOutput());

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetData(result);
  resultNode->SetName(m_CurrentModelNode->GetName() + "_subtract_" + otherNode->GetName());
  resultNode->SetColor(0.7f, 0.3f, 0.2f);

  // Copy pipeline metadata from source model
  xq::pipeline::MarkNode(resultNode, xq::pipeline::Stage::Model);
  std::string sourcePath;
  if (m_CurrentModelNode->GetStringProperty(
        xq::pipeline::kSourcePathProperty, sourcePath) && !sourcePath.empty())
    xq::pipeline::SetStringProperty(resultNode, xq::pipeline::kSourcePathProperty, sourcePath);
  resultNode->SetStringProperty("xq.status", "modified");
  resultNode->SetStringProperty("xq.model.operation", "boolean_subtract");

  ds->Add(resultNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::ApplyFillet()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData)
    return;

  double radius = m_Ui->spinFilletRadius->value();
  if (radius <= 0.0)
  {
    QMessageBox::warning(nullptr, "Fillet", "Fillet radius must be positive.");
    return;
  }

  int iterations = m_Ui->spinFilletIterations->value();
  QString blendType = m_Ui->comboBlendType->currentText();

  // Capture state before modification for undo
  auto* undoActor = SurfaceUndoActor::GetInstance();
  undoActor->SetTargetNode(m_CurrentModelNode);
  auto beforeState = vtkSmartPointer<vtkPolyData>::New();
  beforeState->DeepCopy(polyData);

  // XQ iterative edge-aware blending: smooth only the neighborhood of
  // feature edges within the user-specified radius instead of applying a
  // single global VMTK blend with a fixed radius to the whole model.
  // Detect feature / junction edges
  vtkSmartPointer<vtkFeatureEdges> featureEdges =
    vtkSmartPointer<vtkFeatureEdges>::New();
  featureEdges->SetInputData(polyData);
  featureEdges->BoundaryEdgesOn();
  featureEdges->FeatureEdgesOn();
  featureEdges->NonManifoldEdgesOn();
  featureEdges->ManifoldEdgesOff();
  featureEdges->SetFeatureAngle(30.0);
  featureEdges->Update();

  vtkPolyData* edges = featureEdges->GetOutput();
  if (!edges || edges->GetNumberOfPoints() == 0)
  {
    QMessageBox::information(nullptr, "Fillet",
      "No feature edges detected — nothing to blend.");
    return;
  }

  // Build a locator over the edge points so we can identify which
  // points in the original mesh lie within `radius` of an edge.
  vtkSmartPointer<vtkPointLocator> edgeLocator =
    vtkSmartPointer<vtkPointLocator>::New();
  edgeLocator->SetDataSet(edges);
  edgeLocator->BuildLocator();

  // Mark mesh points near feature edges
  vtkPoints* meshPoints = polyData->GetPoints();
  vtkIdType numPts = meshPoints->GetNumberOfPoints();
  std::vector<bool> nearEdge(numPts, false);

  for (vtkIdType i = 0; i < numPts; ++i)
  {
    double pt[3];
    meshPoints->GetPoint(i, pt);
    vtkIdType closestId = edgeLocator->FindClosestPoint(pt);
    if (closestId >= 0)
    {
      double edgePt[3];
      edges->GetPoint(closestId, edgePt);
      double dist2 = vtkMath::Distance2BetweenPoints(pt, edgePt);
      if (dist2 <= radius * radius)
        nearEdge[i] = true;
    }
  }

  // Iterative constrained Laplacian smooth on the near-edge points only.
  // Chamfer mode uses a single pass with heavier relaxation.
  double relaxation = (blendType == "Chamfer") ? 0.8 : 0.5;
  int nPasses = (blendType == "Chamfer") ? 1 : iterations;

  vtkSmartPointer<vtkPolyData> working =
    vtkSmartPointer<vtkPolyData>::New();
  working->DeepCopy(polyData);

  polyData->BuildLinks();

  for (int pass = 0; pass < nPasses; ++pass)
  {
    vtkSmartPointer<vtkPoints> newPoints =
      vtkSmartPointer<vtkPoints>::New();
    newPoints->DeepCopy(working->GetPoints());

    for (vtkIdType i = 0; i < numPts; ++i)
    {
      if (!nearEdge[i])
        continue;

      // Gather 1-ring neighbors
      vtkSmartPointer<vtkIdList> cellIds =
        vtkSmartPointer<vtkIdList>::New();
      polyData->GetPointCells(i, cellIds);

      std::set<vtkIdType> neighborSet;
      for (vtkIdType c = 0; c < cellIds->GetNumberOfIds(); ++c)
      {
        vtkCell* cell = polyData->GetCell(cellIds->GetId(c));
        for (vtkIdType e = 0; e < cell->GetNumberOfPoints(); ++e)
        {
          vtkIdType nid = cell->GetPointId(e);
          if (nid != i)
            neighborSet.insert(nid);
        }
      }
      if (neighborSet.empty())
        continue;

      double avg[3] = {0, 0, 0};
      for (vtkIdType nid : neighborSet)
      {
        double np[3];
        working->GetPoint(nid, np);
        avg[0] += np[0]; avg[1] += np[1]; avg[2] += np[2];
      }
      double invN = 1.0 / static_cast<double>(neighborSet.size());
      avg[0] *= invN; avg[1] *= invN; avg[2] *= invN;

      double orig[3];
      working->GetPoint(i, orig);
      double smoothed[3];
      smoothed[0] = orig[0] + relaxation * (avg[0] - orig[0]);
      smoothed[1] = orig[1] + relaxation * (avg[1] - orig[1]);
      smoothed[2] = orig[2] + relaxation * (avg[2] - orig[2]);
      newPoints->SetPoint(i, smoothed);
    }
    working->SetPoints(newPoints);
  }

  // Store parameters on the node for provenance / undo
  m_CurrentModelNode->SetDoubleProperty("xq.model.fillet.radius", radius);
  m_CurrentModelNode->SetIntProperty("xq.model.fillet.iterations", nPasses);
  m_CurrentModelNode->SetStringProperty("xq.model.fillet.type",
    blendType.toStdString().c_str());

  // Apply the smoothed result
  SetModelPolyData(m_CurrentModelNode, working);

  // Register undo for fillet operation
  auto afterState = vtkSmartPointer<vtkPolyData>::New();
  afterState->DeepCopy(GetModelPolyData(m_CurrentModelNode));
  auto* doOp = new RestoreSurfaceOp(afterState);
  auto* undoOp = new RestoreSurfaceOp(beforeState);
  xq_UndoHelper::RegisterUndoableOperation(undoActor, doOp, undoOp, "Apply Fillet");

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "Fillet",
    QString("Applied %1 (r=%2, %3 iterations) to junction edges.")
      .arg(blendType).arg(radius).arg(nPasses));
}

void xq_VascularModelingView::ShowModelStatistics()
{
  if (m_CurrentModelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Model Statistics", "No model selected.");
    return;
  }

  vtkPolyData* poly = GetModelPolyData(m_CurrentModelNode);
  if (!poly)
  {
    QMessageBox::warning(nullptr, "Model Statistics", "Selected node has no surface data.");
    return;
  }

  // Run model quality assurance
  auto qa = xq_ModelQuality::Evaluate(poly);
  QString qaHtml;
  qaHtml += "<tr><td colspan='2'><hr></td></tr>";
  qaHtml += "<tr><td colspan='2'><b>QA Summary:</b></td></tr>";
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Closed:</td><td>%1</td></tr>")
      .arg(qa.IsClosed() ? "yes" : "no");
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Manifold:</td><td>%1</td></tr>")
      .arg(qa.IsManifold() ? "yes" : "no");
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Boundary edges:</td><td>%1</td></tr>")
      .arg(qa.boundaryEdges);
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Non-manifold edges:</td><td>%1</td></tr>")
      .arg(qa.nonManifoldEdges);
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Connected components:</td><td>%1</td></tr>")
      .arg(qa.connectedComponents);
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Has FaceIds:</td><td>%1</td></tr>")
      .arg(qa.hasFaceIds ? "yes" : "no");
  qaHtml += QString("<tr><td>&nbsp;&nbsp;Face count:</td><td>%1</td></tr>")
      .arg(qa.faceCount);
  if (!qa.ok)
  {
      qaHtml += "<tr><td colspan='2'><b><font color='red'>WARNING: Model has non-manifold edges</font></b></td></tr>";
  }

  // Triangulate for consistent analysis
  auto triFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  triFilter->SetInputData(poly);
  triFilter->Update();
  vtkPolyData* triPoly = triFilter->GetOutput();

  int numPoints = triPoly->GetNumberOfPoints();
  int numCells = triPoly->GetNumberOfCells();

  // Count cell types
  int numTris = 0, numQuads = 0, numOther = 0;
  for (vtkIdType i = 0; i < triPoly->GetNumberOfCells(); ++i)
  {
    int cellType = triPoly->GetCellType(i);
    if (cellType == VTK_TRIANGLE) numTris++;
    else if (cellType == VTK_QUAD) numQuads++;
    else numOther++;
  }

  // Surface area
  double totalArea = 0.0;
  double minArea = 1e30, maxArea = 0.0;
  for (vtkIdType i = 0; i < triPoly->GetNumberOfCells(); ++i)
  {
    vtkCell* cell = triPoly->GetCell(i);
    if (!cell || cell->GetNumberOfPoints() < 3) continue;
    double p0[3], p1[3], p2[3];
    triPoly->GetPoint(cell->GetPointId(0), p0);
    triPoly->GetPoint(cell->GetPointId(1), p1);
    triPoly->GetPoint(cell->GetPointId(2), p2);
    double v1[3] = {p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2]};
    double v2[3] = {p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2]};
    double cross[3] = {
      v1[1]*v2[2] - v1[2]*v2[1],
      v1[2]*v2[0] - v1[0]*v2[2],
      v1[0]*v2[1] - v1[1]*v2[0]
    };
    double area = 0.5 * std::sqrt(cross[0]*cross[0] + cross[1]*cross[1] + cross[2]*cross[2]);
    totalArea += area;
    if (area < minArea) minArea = area;
    if (area > maxArea) maxArea = area;
  }

  // Bounding box
  double bounds[6];
  triPoly->GetBounds(bounds);
  double dx = bounds[1] - bounds[0];
  double dy = bounds[3] - bounds[2];
  double dz = bounds[5] - bounds[4];
  double diagonal = std::sqrt(dx*dx + dy*dy + dz*dz);

  QString report = QString(
    "<h3>Model Statistics: %1</h3>"
    "<table cellspacing='4'>"
    "<tr><td><b>Points:</b></td><td>%2</td></tr>"
    "<tr><td><b>Cells:</b></td><td>%3</td></tr>"
    "<tr><td><b>Triangles:</b></td><td>%4</td></tr>"
    "<tr><td><b>Quads:</b></td><td>%5</td></tr>"
    "<tr><td><b>Other:</b></td><td>%6</td></tr>"
    "<tr><td colspan='2'><hr></td></tr>"
    "<tr><td><b>Total Area:</b></td><td>%7 mm²</td></tr>"
    "<tr><td><b>Min Cell Area:</b></td><td>%8 mm²</td></tr>"
    "<tr><td><b>Max Cell Area:</b></td><td>%9 mm²</td></tr>"
    "<tr><td colspan='2'><hr></td></tr>"
    "<tr><td><b>Bounding Box:</b></td><td>%10 x %11 x %12 mm</td></tr>"
    "<tr><td><b>Diagonal:</b></td><td>%13 mm</td></tr>"
    "")
    .arg(QString::fromStdString(m_CurrentModelNode->GetName()))
    .arg(numPoints).arg(numCells).arg(numTris).arg(numQuads).arg(numOther)
    .arg(totalArea, 0, 'f', 2)
    .arg(numCells > 0 ? minArea : 0.0, 0, 'f', 6)
    .arg(maxArea, 0, 'f', 6)
    .arg(dx, 0, 'f', 2).arg(dy, 0, 'f', 2).arg(dz, 0, 'f', 2)
    .arg(diagonal, 0, 'f', 2);

  report += qaHtml;
  report += "</table>";

  QMessageBox dlg;
  dlg.setWindowTitle("Model Statistics");
  dlg.setTextFormat(Qt::RichText);
  dlg.setText(report);
  dlg.exec();
}

void xq_VascularModelingView::DecimateSurface()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData)
    return;

  vtkIdType originalCells = polyData->GetNumberOfCells();

  if (originalCells == 0)
  {
    m_Ui->lblDecimationResult->setText("No cells to decimate.");
    return;
  }

  double ratio = m_Ui->spinDecimationRatio->value();
  bool preserveTopology = m_Ui->chkPreserveTopology->isChecked();

  // Capture state before modification for undo
  auto* undoActor = SurfaceUndoActor::GetInstance();
  undoActor->SetTargetNode(m_CurrentModelNode);
  auto beforeState = vtkSmartPointer<vtkPolyData>::New();
  beforeState->DeepCopy(polyData);

  vtkSmartPointer<vtkTriangleFilter> triFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  triFilter->SetInputData(polyData);
  triFilter->Update();

  vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
  decimator->SetInputConnection(triFilter->GetOutputPort());
  decimator->SetTargetReduction(ratio);
  decimator->SetPreserveTopology(preserveTopology ? 1 : 0);
  decimator->Update();

  vtkPolyData* result = decimator->GetOutput();
  vtkIdType newCells = result->GetNumberOfCells();

  SetModelPolyData(m_CurrentModelNode, result);

  m_CurrentModelNode->SetStringProperty("xq.status", "modified");
  m_CurrentModelNode->SetStringProperty("xq.model.operation", "decimate");

  // Register undo for decimation
  auto afterState = vtkSmartPointer<vtkPolyData>::New();
  afterState->DeepCopy(GetModelPolyData(m_CurrentModelNode));
  auto* doOp = new RestoreSurfaceOp(afterState);
  auto* undoOp = new RestoreSurfaceOp(beforeState);
  xq_UndoHelper::RegisterUndoableOperation(undoActor, doOp, undoOp, "Decimate Surface");

  double actualReduction = 100.0 * (1.0 - static_cast<double>(newCells) / static_cast<double>(originalCells));
  m_Ui->lblDecimationResult->setText(
    QString("Reduced from %1 to %2 triangles (%3% reduction)")
      .arg(originalCells).arg(newCells).arg(actualReduction, 0, 'f', 1));

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::ComputeNormals()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData)
    return;

  vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  normals->SetInputData(polyData);
  normals->AutoOrientNormalsOn();
  normals->ConsistencyOn();
  normals->SplittingOff();
  normals->Update();

  vtkPolyData* result = normals->GetOutput();
  vtkIdType numCells = result->GetNumberOfCells();

  SetModelPolyData(m_CurrentModelNode, result);

  m_CurrentModelNode->SetStringProperty("xq.status", "modified");
  m_CurrentModelNode->SetStringProperty("xq.model.operation", "compute_normals");

  m_Ui->lblSurfaceOpsResult->setText(
    QString("Normals computed for %1 cells").arg(numCells));

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::CleanSurface()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData)
    return;

  vtkIdType origPoints = polyData->GetNumberOfPoints();
  vtkIdType origCells = polyData->GetNumberOfCells();

  vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
  cleaner->SetInputData(polyData);
  cleaner->Update();

  vtkPolyData* result = cleaner->GetOutput();
  vtkIdType newPoints = result->GetNumberOfPoints();
  vtkIdType newCells = result->GetNumberOfCells();

  SetModelPolyData(m_CurrentModelNode, result);

  m_CurrentModelNode->SetStringProperty("xq.status", "modified");
  m_CurrentModelNode->SetStringProperty("xq.model.operation", "clean_surface");

  m_Ui->lblSurfaceOpsResult->setText(
    QString::fromUtf8("Cleaned: %1 points \u2192 %2 points, %3 cells \u2192 %4 cells")
      .arg(origPoints).arg(newPoints).arg(origCells).arg(newCells));

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VascularModelingView::FillHoles()
{
  if (m_CurrentModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_CurrentModelNode);
  if (!polyData)
    return;

  vtkIdType origCells = polyData->GetNumberOfCells();

  // Capture state before modification for undo
  auto* undoActor = SurfaceUndoActor::GetInstance();
  undoActor->SetTargetNode(m_CurrentModelNode);
  auto beforeState = vtkSmartPointer<vtkPolyData>::New();
  beforeState->DeepCopy(polyData);

  vtkSmartPointer<vtkFillHolesFilter> filler = vtkSmartPointer<vtkFillHolesFilter>::New();
  filler->SetInputData(polyData);
  filler->SetHoleSize(1000.0);
  filler->Update();

  vtkPolyData* result = filler->GetOutput();
  vtkIdType newCells = result->GetNumberOfCells();

  SetModelPolyData(m_CurrentModelNode, result);

  m_CurrentModelNode->SetStringProperty("xq.status", "modified");
  m_CurrentModelNode->SetStringProperty("xq.model.operation", "fill_holes");

  // Register undo for fill holes
  auto afterState = vtkSmartPointer<vtkPolyData>::New();
  afterState->DeepCopy(GetModelPolyData(m_CurrentModelNode));
  auto* doOp = new RestoreSurfaceOp(afterState);
  auto* undoOp = new RestoreSurfaceOp(beforeState);
  xq_UndoHelper::RegisterUndoableOperation(undoActor, doOp, undoOp, "Fill Holes");

  m_Ui->lblSurfaceOpsResult->setText(
    QString::fromUtf8("Filled holes: %1 cells \u2192 %2 cells")
      .arg(origCells).arg(newCells));

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
