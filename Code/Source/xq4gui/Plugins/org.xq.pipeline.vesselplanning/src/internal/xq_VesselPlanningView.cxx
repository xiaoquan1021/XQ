#include "xq_VesselPlanningView.h"
#include "ui_xq_VesselPlanningView.h"
#include "xq_PathCreate.h"
#include "xq_CenterlineSmoother.h"
#include "xq_CenterlineInteractor.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"
#include "xq_CenterlineOp.h"
#include "xq_UndoHelper.h"
#include <xq_PipelineDataUtils.h>
#include <xq_SegmentationUtils.h>
#include <xq_LegacyNodeMigration.h>

#include <mitkIPreferences.h>
#include <mitkIPreferencesService.h>
#include <mitkCoreServices.h>

#include <mitkDataStorage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkSliceNavigationController.h>
#include <usModuleRegistry.h>

#include <QmitkRenderWindow.h>

#include <QMessageBox>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QShortcut>
#include <cmath>
#include <algorithm>

const QString xq_VesselPlanningView::VIEW_ID = "org.xq.views.pathplanning";

namespace
{

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

} // namespace

xq_VesselPlanningView::xq_VesselPlanningView()
  : m_Ui(nullptr)
  , m_PathTableView(nullptr)
  , m_PointTableView(nullptr)
  , m_ResliceSlider(nullptr)
  , m_ResliceLabel(nullptr)
  , m_ResliceSizeSpin(nullptr)
  , m_CurrentPathNode(nullptr)
{
}

xq_VesselPlanningView::~xq_VesselPlanningView()
{
  delete m_Ui;
}

void xq_VesselPlanningView::CreateQtPartControl(QWidget* parent)
{
  xq_LegacyNodeMigration::UpgradeImportedLegacyNodes(GetDataStorage());
  xq_LegacyNodeMigration::ReparentIntoCategoryFolders(GetDataStorage());

  m_Ui = new Ui::xq_VesselPlanningView;
  m_Ui->setupUi(parent);

  m_PathTableView = m_Ui->pathTableView;
  m_PointTableView = m_Ui->pointTableView;

  // Initialize path table model
  auto* pathModel = new QStandardItemModel(0, 2, this);
  pathModel->setHorizontalHeaderLabels({"Name", "Points"});
  m_PathTableView->setModel(pathModel);
  m_PathTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_PathTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_PathTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Initialize point table model
  auto* pointModel = new QStandardItemModel(0, 4, this);
  pointModel->setHorizontalHeaderLabels({"Index", "X", "Y", "Z"});
  m_PointTableView->setModel(pointModel);
  m_PointTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_PointTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_PointTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

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
  m_Ui->pathToolsLayout->insertLayout(0, resliceLayout);

  SetupConnections();

  // Keyboard shortcuts for vessel path editing
  auto* deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), parent);
  deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(deleteShortcut, &QShortcut::activated, this, &xq_VesselPlanningView::DeletePoint);

  auto* ctrlAShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_A), parent);
  ctrlAShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(ctrlAShortcut, &QShortcut::activated, this, &xq_VesselPlanningView::AddPoint);

  auto* ctrlDShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), parent);
  ctrlDShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(ctrlDShortcut, &QShortcut::activated, this, &xq_VesselPlanningView::DeletePoint);

  connect(m_ResliceSlider, &QSlider::valueChanged,
          this, &xq_VesselPlanningView::OnReslicePositionChanged);
  connect(m_ResliceSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { UpdatePathReslice(); });
}

void xq_VesselPlanningView::SetupConnections()
{
  connect(m_Ui->addPathButton, &QPushButton::clicked, this, &xq_VesselPlanningView::AddPath);
  connect(m_Ui->deletePathButton, &QPushButton::clicked, this, &xq_VesselPlanningView::DeletePath);
  connect(m_Ui->addPointButton, &QPushButton::clicked, this, &xq_VesselPlanningView::AddPoint);
  connect(m_Ui->deletePointButton, &QPushButton::clicked, this, &xq_VesselPlanningView::DeletePoint);
  connect(m_Ui->smartPointButton, &QPushButton::clicked, this, &xq_VesselPlanningView::AddSmartPoint);
  connect(m_Ui->interpolateButton, &QPushButton::clicked, this, &xq_VesselPlanningView::InterpolatePath);
  connect(m_Ui->statsButton, &QPushButton::clicked, this, &xq_VesselPlanningView::ShowPathStatistics);
  connect(m_Ui->exportButton, &QPushButton::clicked, this, &xq_VesselPlanningView::ExportPath);
  connect(m_Ui->btnResamplePath, &QPushButton::clicked, this, &xq_VesselPlanningView::ResamplePath);
  connect(m_Ui->btnReversePath, &QPushButton::clicked, this, &xq_VesselPlanningView::ReversePath);
  connect(m_Ui->btnMergePaths, &QPushButton::clicked, this, &xq_VesselPlanningView::MergePaths);
  connect(m_Ui->btnCrossSection, &QPushButton::clicked, this, &xq_VesselPlanningView::ShowCrossSectionInfo);
  connect(m_Ui->btnSmoothPath, &QPushButton::clicked, this, &xq_VesselPlanningView::SmoothPath);

  connect(m_PathTableView->selectionModel(),
          &QItemSelectionModel::selectionChanged,
          this,
          &xq_VesselPlanningView::OnPathTableSelectionChanged);

  connect(m_PointTableView->selectionModel(),
          &QItemSelectionModel::selectionChanged,
          this,
          &xq_VesselPlanningView::OnPointTableSelectionChanged);
}

void xq_VesselPlanningView::SetFocus()
{
  m_PathTableView->setFocus();
}

void xq_VesselPlanningView::OnSelectionChanged(
    berry::IWorkbenchPart::Pointer /*source*/,
    const QList<mitk::DataNode::Pointer>& nodes)
{
  for (const auto& node : nodes)
  {
    if (node.IsNotNull())
    {
      auto* centerline = dynamic_cast<xq_VesselCenterline*>(node->GetData());
      if (centerline != nullptr)
      {
        m_CurrentPathNode = node;

        // Detach interactor from old node before switching
        if (m_PathInteractor.IsNotNull() && m_PathInteractor->GetDataNode())
        {
          m_PathInteractor->GetDataNode()->SetDataInteractor(nullptr);
        }

        // Attach the CenterlineInteractor to the selected path node
        if (m_PathInteractor.IsNull())
        {
          m_PathInteractor = xq_CenterlineInteractor::New();
          m_PathInteractor->LoadStateMachine("xq_PathInteraction.xml",
                                             us::ModuleRegistry::GetModule("xqModulePath"));
          m_PathInteractor->SetEventConfig("xq_PathConfig.xml",
                                           us::ModuleRegistry::GetModule("xqModulePath"));
        }
        m_PathInteractor->SetDataNode(node);

        UpdatePointTable();
        UpdatePathReslice();
        return;
      }
    }
  }
}

void xq_VesselPlanningView::AddPath()
{
  xq_PathCreate dialog(m_PathTableView);
  if (dialog.exec() == QDialog::Accepted)
  {
    QString pathName = dialog.GetPathName();
    if (pathName.isEmpty())
    {
      pathName = "Path";
    }

    auto pathData = xq_VesselCenterline::New();
    auto* segment = new xq_CenterlineSegment();
    segment->SetSampleDensity(dialog.GetCalculationNumber());
    pathData->SetSegment(segment);
    auto node = mitk::DataNode::New();
    node->SetData(pathData);
    node->SetName(pathName.toStdString());
    node->SetProperty("subdivision.number",
                       mitk::IntProperty::New(dialog.GetSubdivisionNumber()));
    node->SetProperty("calculation.number",
                       mitk::IntProperty::New(dialog.GetCalculationNumber()));
    node->SetBoolProperty("xq.pathplanning.path", true);
    node->SetStringProperty(xq::pipeline::kAlgorithmProperty, "manual");
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
    EnsureSourceImageProperty(node);

    AddPathNodeToFolder(node);
    UpdatePathTable();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_VesselPlanningView::DeletePath()
{
  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  auto result = QMessageBox::question(m_PathTableView,
                                      "Delete Path",
                                      "Are you sure you want to delete the selected path?",
                                      QMessageBox::Yes | QMessageBox::No);
  if (result == QMessageBox::Yes)
  {
    GetDataStorage()->Remove(m_CurrentPathNode);
    m_CurrentPathNode = nullptr;
    UpdatePathTable();
    UpdatePointTable();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_VesselPlanningView::AddPoint()
{
  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (centerline == nullptr)
  {
    return;
  }

  auto* segment = centerline->GetSegment();
  if (segment == nullptr)
  {
    return;
  }

  mitk::Point3D newPoint;
  newPoint.Fill(0.0);
  int insertIdx = segment->GetAnchorCount();
  auto* doOp = new xq_CenterlineOp(xq_CenterlineOp::ActInsertAnchor, 0, newPoint, insertIdx);
  auto* undoOp = new xq_CenterlineOp(xq_CenterlineOp::ActRemoveAnchor, 0, newPoint, insertIdx);
  xq_UndoHelper::SubmitUndoableOperation(centerline, doOp, undoOp, "Add Path Point");

  UpdatePointTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();
}

void xq_VesselPlanningView::DeletePoint()
{
  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (centerline == nullptr)
  {
    return;
  }

  auto* segment = centerline->GetSegment();
  if (segment == nullptr)
  {
    return;
  }

  QModelIndexList selected = m_PointTableView->selectionModel()->selectedRows();
  if (selected.isEmpty())
  {
    return;
  }

  int row = selected.first().row();
  if (row >= 0 && row < segment->GetAnchorCount())
  {
    segment->RemoveAnchor(row);
    centerline->Modified();
  }

  UpdatePointTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VesselPlanningView::AddSmartPoint()
{
  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (centerline == nullptr)
  {
    return;
  }

  auto* segment = centerline->GetSegment();
  if (segment == nullptr || segment->GetAnchorCount() < 2)
  {
    return;
  }

  QModelIndexList selected = m_PointTableView->selectionModel()->selectedRows();
  if (selected.isEmpty())
  {
    return;
  }

  int row = selected.first().row();
  if (row >= segment->GetAnchorCount() - 1)
  {
    return;
  }

  // Insert midpoint between selected point and next point
  mitk::Point3D p1 = segment->GetAnchorPosition(row);
  mitk::Point3D p2 = segment->GetAnchorPosition(row + 1);
  mitk::Point3D midPoint;
  midPoint[0] = (p1[0] + p2[0]) / 2.0;
  midPoint[1] = (p1[1] + p2[1]) / 2.0;
  midPoint[2] = (p1[2] + p2[2]) / 2.0;

  segment->InsertAnchor(row + 1, midPoint);
  centerline->Modified();

  UpdatePointTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_VesselPlanningView::SelectPoint()
{
  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  QModelIndexList selected = m_PointTableView->selectionModel()->selectedRows();
  if (selected.isEmpty())
  {
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (centerline == nullptr)
  {
    return;
  }

  auto* segment = centerline->GetSegment();
  if (segment == nullptr)
  {
    return;
  }

  int row = selected.first().row();
  if (row >= 0 && row < segment->GetAnchorCount())
  {
    mitk::Point3D point = segment->GetAnchorPosition(row);
    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(
        GetDataStorage());
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_VesselPlanningView::OnPathTableSelectionChanged(
    const QItemSelection& selected,
    const QItemSelection& /*deselected*/)
{
  QModelIndexList indexes = selected.indexes();
  if (indexes.isEmpty())
  {
    m_CurrentPathNode = nullptr;
    UpdatePointTable();
    return;
  }

  int row = indexes.first().row();

  // Find the corresponding data node
  auto allNodes = GetDataStorage()->GetAll();
  int pathIndex = 0;
  for (auto nodeIt = allNodes->Begin(); nodeIt != allNodes->End(); ++nodeIt)
  {
    auto node = nodeIt->Value();
    if (dynamic_cast<xq_VesselCenterline*>(node->GetData()) != nullptr)
    {
      if (pathIndex == row)
      {
        m_CurrentPathNode = node;
        UpdatePointTable();
        UpdatePathReslice();
        return;
      }
      ++pathIndex;
    }
  }
}

void xq_VesselPlanningView::OnPointTableSelectionChanged(
    const QItemSelection& selected,
    const QItemSelection& /*deselected*/)
{
  bool hasSelection = !selected.indexes().isEmpty();
  m_Ui->deletePointButton->setEnabled(hasSelection);
  m_Ui->smartPointButton->setEnabled(hasSelection);
}

void xq_VesselPlanningView::UpdatePathTable()
{
  auto* model = qobject_cast<QStandardItemModel*>(m_PathTableView->model());
  if (model == nullptr)
  {
    return;
  }

  model->removeRows(0, model->rowCount());

  auto allNodes = GetDataStorage()->GetAll();
  for (auto nodeIt = allNodes->Begin(); nodeIt != allNodes->End(); ++nodeIt)
  {
    auto node = nodeIt->Value();
    auto* centerline = dynamic_cast<xq_VesselCenterline*>(node->GetData());
    if (centerline != nullptr)
    {
      int row = model->rowCount();
      model->insertRow(row);
      model->setItem(row, 0, new QStandardItem(
          QString::fromStdString(node->GetName())));
      auto* seg = centerline->GetSegment();
      model->setItem(row, 1, new QStandardItem(
          QString::number(seg ? seg->GetAnchorCount() : 0)));
    }
  }
}

void xq_VesselPlanningView::UpdatePointTable()
{
  auto* model = qobject_cast<QStandardItemModel*>(m_PointTableView->model());
  if (model == nullptr)
  {
    return;
  }

  model->removeRows(0, model->rowCount());

  if (m_CurrentPathNode.IsNull())
  {
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (centerline == nullptr)
  {
    return;
  }

  auto* segment = centerline->GetSegment();
  if (segment == nullptr)
  {
    return;
  }

  std::vector<mitk::Point3D> anchors = segment->GetAnchorPositions();
  for (int index = 0; index < static_cast<int>(anchors.size()); ++index)
  {
    const mitk::Point3D& point = anchors[index];
    int row = model->rowCount();
    model->insertRow(row);
    model->setItem(row, 0, new QStandardItem(QString::number(index)));
    model->setItem(row, 1, new QStandardItem(QString::number(point[0], 'f', 2)));
    model->setItem(row, 2, new QStandardItem(QString::number(point[1], 'f', 2)));
    model->setItem(row, 3, new QStandardItem(QString::number(point[2], 'f', 2)));
  }
}

void xq_VesselPlanningView::InterpolatePath()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Interpolate", "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::information(nullptr, "Interpolate",
      "Path must have at least 2 control points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 2)
  {
    QMessageBox::information(nullptr, "Interpolate",
      "Path must have at least 2 control points.");
    return;
  }

  bool ok = false;
  int numSamples = QInputDialog::getInt(nullptr, "Interpolate Path",
    "Number of interpolated points:", 50, 3, 1000, 1, &ok);
  if (!ok) return;

  // Collect control points
  std::vector<mitk::Point3D> controls = segment->GetAnchorPositions();

  // Compute cumulative arc-length
  std::vector<double> arcLen(controls.size(), 0.0);
  for (size_t i = 1; i < controls.size(); ++i)
  {
    double dx = controls[i][0] - controls[i-1][0];
    double dy = controls[i][1] - controls[i-1][1];
    double dz = controls[i][2] - controls[i-1][2];
    arcLen[i] = arcLen[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
  }

  double totalLen = arcLen.back();
  if (totalLen < 1e-12)
  {
    QMessageBox::warning(nullptr, "Interpolate", "Path has zero length.");
    return;
  }

  // Create interpolated path using piecewise linear interpolation
  auto interpCenterline = xq_VesselCenterline::New();
  auto* interpSegment = new xq_CenterlineSegment();
  std::vector<mitk::Point3D> interpPoints;
  for (int s = 0; s < numSamples; ++s)
  {
    double t = (s * totalLen) / (numSamples - 1);
    // Find segment
    size_t seg = 0;
    for (size_t j = 1; j < arcLen.size(); ++j)
    {
      if (arcLen[j] >= t) { seg = j - 1; break; }
    }
    if (seg >= controls.size() - 1) seg = controls.size() - 2;

    double segLen = arcLen[seg+1] - arcLen[seg];
    double frac = (segLen > 1e-12) ? (t - arcLen[seg]) / segLen : 0.0;

    mitk::Point3D pt;
    pt[0] = controls[seg][0] + frac * (controls[seg+1][0] - controls[seg][0]);
    pt[1] = controls[seg][1] + frac * (controls[seg+1][1] - controls[seg][1]);
    pt[2] = controls[seg][2] + frac * (controls[seg+1][2] - controls[seg][2]);
    interpPoints.push_back(pt);
  }

  interpSegment->ReplaceAnchors(interpPoints);
  interpCenterline->SetSegment(interpSegment);

  // Store as a new node
  mitk::DataNode::Pointer interpNode = mitk::DataNode::New();
  interpNode->SetData(interpCenterline);
  interpNode->SetName(m_CurrentPathNode->GetName() + "_interpolated");
  interpNode->SetColor(0.0f, 0.8f, 1.0f);
  interpNode->SetFloatProperty("pointsize", 1.5f);
  interpNode->SetBoolProperty("show contour", true);
  interpNode->SetBoolProperty("xq.pathplanning.path", true);
  xq::pipeline::MarkNode(interpNode, xq::pipeline::Stage::Path);
  interpNode->SetStringProperty(xq::pipeline::kAlgorithmProperty, "interpolated");
  EnsureSourceImageProperty(interpNode, m_CurrentPathNode);

  GetDataStorage()->Add(interpNode, m_CurrentPathNode);

  UpdatePathTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();

  QMessageBox::information(nullptr, "Interpolate",
    QString("Created interpolated path with %1 points\nTotal length: %2 mm")
      .arg(numSamples).arg(totalLen, 0, 'f', 2));
}

void xq_VesselPlanningView::ShowPathStatistics()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Statistics", "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::information(nullptr, "Statistics", "Path has no points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 1)
  {
    QMessageBox::information(nullptr, "Statistics", "Path has no points.");
    return;
  }

  std::vector<mitk::Point3D> points = segment->GetAnchorPositions();

  int numPoints = static_cast<int>(points.size());
  double totalLength = 0.0;
  double minSegLen = 1e30;
  double maxSegLen = 0.0;

  for (size_t i = 1; i < points.size(); ++i)
  {
    double dx = points[i][0] - points[i-1][0];
    double dy = points[i][1] - points[i-1][1];
    double dz = points[i][2] - points[i-1][2];
    double segLen = std::sqrt(dx*dx + dy*dy + dz*dz);
    totalLength += segLen;
    if (segLen < minSegLen) minSegLen = segLen;
    if (segLen > maxSegLen) maxSegLen = segLen;
  }

  // Compute bounding box
  double minX = 1e30, minY = 1e30, minZ = 1e30;
  double maxX = -1e30, maxY = -1e30, maxZ = -1e30;
  for (const auto& p : points)
  {
    if (p[0] < minX) minX = p[0]; if (p[0] > maxX) maxX = p[0];
    if (p[1] < minY) minY = p[1]; if (p[1] > maxY) maxY = p[1];
    if (p[2] < minZ) minZ = p[2]; if (p[2] > maxZ) maxZ = p[2];
  }

  double avgSegLen = (numPoints > 1) ? totalLength / (numPoints - 1) : 0.0;

  QString stats = QString(
    "Path: %1\n\n"
    "Points: %2\n"
    "Segments: %3\n\n"
    "Total Length: %4 mm\n"
    "Average Segment: %5 mm\n"
    "Shortest Segment: %6 mm\n"
    "Longest Segment: %7 mm\n\n"
    "Bounding Box:\n"
    "  X: [%8, %9]\n"
    "  Y: [%10, %11]\n"
    "  Z: [%12, %13]")
    .arg(QString::fromStdString(m_CurrentPathNode->GetName()))
    .arg(numPoints)
    .arg(numPoints > 1 ? numPoints - 1 : 0)
    .arg(totalLength, 0, 'f', 2)
    .arg(avgSegLen, 0, 'f', 2)
    .arg(numPoints > 1 ? minSegLen : 0.0, 0, 'f', 2)
    .arg(maxSegLen, 0, 'f', 2)
    .arg(minX, 0, 'f', 2).arg(maxX, 0, 'f', 2)
    .arg(minY, 0, 'f', 2).arg(maxY, 0, 'f', 2)
    .arg(minZ, 0, 'f', 2).arg(maxZ, 0, 'f', 2);

  QMessageBox::information(nullptr, "Path Statistics", stats);
}

void xq_VesselPlanningView::ExportPath()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Export Path", "No path selected.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::warning(nullptr, "Export Path", "Selected path has no points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() == 0)
  {
    QMessageBox::warning(nullptr, "Export Path", "Selected path has no points.");
    return;
  }

  QString defaultName = QString::fromStdString(m_CurrentPathNode->GetName()) + ".csv";
  QString filePath = QFileDialog::getSaveFileName(nullptr, "Export Path",
    defaultName, "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");
  if (filePath.isEmpty()) return;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::critical(nullptr, "Export Path",
      QString("Cannot write to file:\n%1").arg(filePath));
    return;
  }

  QTextStream out(&file);
  out << "Index,X,Y,Z\n";

  std::vector<mitk::Point3D> anchors = segment->GetAnchorPositions();
  for (int i = 0; i < static_cast<int>(anchors.size()); ++i)
  {
    const mitk::Point3D& pt = anchors[i];
    out << i << ","
        << QString::number(pt[0], 'f', 6) << ","
        << QString::number(pt[1], 'f', 6) << ","
        << QString::number(pt[2], 'f', 6) << "\n";
  }

  file.close();
  QMessageBox::information(nullptr, "Export Path",
    QString("Exported %1 points to:\n%2").arg(anchors.size()).arg(filePath));
}

void xq_VesselPlanningView::ResamplePath()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Resample", "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::information(nullptr, "Resample",
      "Path must have at least 2 points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 2)
  {
    QMessageBox::information(nullptr, "Resample",
      "Path must have at least 2 points.");
    return;
  }

  double spacing = static_cast<double>(m_Ui->spinResampleSpacing->value());

  // Collect original points
  std::vector<mitk::Point3D> origPoints = segment->GetAnchorPositions();

  int origCount = static_cast<int>(origPoints.size());

  // Compute cumulative arc-length
  std::vector<double> arcLen(origPoints.size(), 0.0);
  for (size_t i = 1; i < origPoints.size(); ++i)
  {
    double dx = origPoints[i][0] - origPoints[i-1][0];
    double dy = origPoints[i][1] - origPoints[i-1][1];
    double dz = origPoints[i][2] - origPoints[i-1][2];
    arcLen[i] = arcLen[i-1] + std::sqrt(dx*dx + dy*dy + dz*dz);
  }

  double totalLen = arcLen.back();
  if (totalLen < 1e-12)
  {
    QMessageBox::warning(nullptr, "Resample", "Path has zero length.");
    return;
  }

  // Generate resampled points at uniform spacing
  std::vector<mitk::Point3D> resampled;
  resampled.push_back(origPoints.front());

  double currentDist = spacing;
  while (currentDist < totalLen)
  {
    // Find segment containing currentDist
    size_t seg = 0;
    for (size_t j = 1; j < arcLen.size(); ++j)
    {
      if (arcLen[j] >= currentDist) { seg = j - 1; break; }
    }

    double segLen = arcLen[seg+1] - arcLen[seg];
    double frac = (segLen > 1e-12) ? (currentDist - arcLen[seg]) / segLen : 0.0;

    mitk::Point3D pt;
    pt[0] = origPoints[seg][0] + frac * (origPoints[seg+1][0] - origPoints[seg][0]);
    pt[1] = origPoints[seg][1] + frac * (origPoints[seg+1][1] - origPoints[seg][1]);
    pt[2] = origPoints[seg][2] + frac * (origPoints[seg+1][2] - origPoints[seg][2]);
    resampled.push_back(pt);

    currentDist += spacing;
  }

  // Always include the last point
  resampled.push_back(origPoints.back());

  // Replace node data with resampled points
  segment->ReplaceAnchors(resampled);
  centerline->Modified();
  UpdatePointTable();
  UpdatePathTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();

  QMessageBox::information(nullptr, "Resample",
    QString("Path resampled: %1 \u2192 %2 points, spacing = %3")
      .arg(origCount)
      .arg(resampled.size())
      .arg(spacing, 0, 'f', 1));
}

void xq_VesselPlanningView::ReversePath()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Reverse", "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::information(nullptr, "Reverse", "Path has no points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 1)
  {
    QMessageBox::information(nullptr, "Reverse", "Path has no points.");
    return;
  }

  // Collect points
  std::vector<mitk::Point3D> points = segment->GetAnchorPositions();

  int numPoints = static_cast<int>(points.size());

  // Reverse and rebuild
  std::reverse(points.begin(), points.end());

  segment->ReplaceAnchors(points);
  centerline->Modified();
  UpdatePointTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();

  QMessageBox::information(nullptr, "Reverse",
    QString("Path direction reversed (%1 points)").arg(numPoints));
}

void xq_VesselPlanningView::MergePaths()
{
  // Collect all VesselCenterline nodes from data storage
  auto allNodes = GetDataStorage()->GetAll();
  std::vector<mitk::DataNode::Pointer> pathNodes;

  for (auto nodeIt = allNodes->Begin(); nodeIt != allNodes->End(); ++nodeIt)
  {
    auto node = nodeIt->Value();
    if (dynamic_cast<xq_VesselCenterline*>(node->GetData()) != nullptr)
    {
      pathNodes.push_back(node);
    }
  }

  if (pathNodes.size() < 2)
  {
    QMessageBox::warning(nullptr, "Merge Paths",
      "At least 2 paths are required for merging.");
    return;
  }

  // Combine all anchor points from all path nodes in order
  std::vector<mitk::Point3D> mergedPoints;
  int mergedPathCount = 0;

  for (const auto& node : pathNodes)
  {
    auto* cl = dynamic_cast<xq_VesselCenterline*>(node->GetData());
    if (cl != nullptr)
    {
      auto* seg = cl->GetSegment();
      if (seg != nullptr)
      {
        std::vector<mitk::Point3D> anchors = seg->GetAnchorPositions();
        mergedPoints.insert(mergedPoints.end(), anchors.begin(), anchors.end());
      }
      ++mergedPathCount;
    }
  }

  // Create new node
  auto mergedCenterline = xq_VesselCenterline::New();
  auto* mergedSegment = new xq_CenterlineSegment();
  mergedSegment->ReplaceAnchors(mergedPoints);
  mergedCenterline->SetSegment(mergedSegment);

  auto mergedNode = mitk::DataNode::New();
  mergedNode->SetData(mergedCenterline);
  mergedNode->SetName("merged_path");
  mergedNode->SetBoolProperty("xq.pathplanning.path", true);
  xq::pipeline::MarkNode(mergedNode, xq::pipeline::Stage::Path);
  mergedNode->SetStringProperty(xq::pipeline::kAlgorithmProperty, "merged");
  EnsureSourceImageProperty(mergedNode, m_CurrentPathNode);
  AddPathNodeToFolder(mergedNode);

  UpdatePathTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();

  QMessageBox::information(nullptr, "Merge Paths",
    QString("Merged %1 paths (total %2 points)")
      .arg(mergedPathCount).arg(mergedPoints.size()));
}

void xq_VesselPlanningView::ShowCrossSectionInfo()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Cross-Section Info",
      "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::information(nullptr, "Cross-Section Info",
      "Path must have at least 2 points.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 2)
  {
    QMessageBox::information(nullptr, "Cross-Section Info",
      "Path must have at least 2 points.");
    return;
  }

  std::vector<mitk::Point3D> points = segment->GetAnchorPositions();

  int numPoints = static_cast<int>(points.size());
  double totalLength = 0.0;
  double minCurvature = 1e30;
  double maxCurvature = 0.0;
  double sumCurvature = 0.0;
  int curvatureCount = 0;

  // Compute segment lengths and curvatures
  for (size_t i = 1; i < points.size(); ++i)
  {
    double dx = points[i][0] - points[i-1][0];
    double dy = points[i][1] - points[i-1][1];
    double dz = points[i][2] - points[i-1][2];
    double segLen = std::sqrt(dx*dx + dy*dy + dz*dz);
    totalLength += segLen;
  }

  // Curvature using 3 consecutive points
  for (size_t i = 1; i + 1 < points.size(); ++i)
  {
    double ax = points[i][0] - points[i-1][0];
    double ay = points[i][1] - points[i-1][1];
    double az = points[i][2] - points[i-1][2];

    double bx = points[i+1][0] - points[i][0];
    double by = points[i+1][1] - points[i][1];
    double bz = points[i+1][2] - points[i][2];

    // Cross product a x b
    double cx = ay * bz - az * by;
    double cy = az * bx - ax * bz;
    double cz = ax * by - ay * bx;
    double crossMag = std::sqrt(cx*cx + cy*cy + cz*cz);

    double segA = std::sqrt(ax*ax + ay*ay + az*az);
    double segB = std::sqrt(bx*bx + by*by + bz*bz);
    double avgSeg = (segA + segB) / 2.0;

    double curvature = 0.0;
    if (avgSeg > 1e-12)
    {
      curvature = crossMag / (avgSeg * avgSeg * avgSeg);
    }

    if (curvature < minCurvature) minCurvature = curvature;
    if (curvature > maxCurvature) maxCurvature = curvature;
    sumCurvature += curvature;
    ++curvatureCount;
  }

  double avgSegLen = (numPoints > 1) ? totalLength / (numPoints - 1) : 0.0;
  double avgCurvature = (curvatureCount > 0) ? sumCurvature / curvatureCount : 0.0;

  if (curvatureCount == 0)
  {
    minCurvature = 0.0;
    maxCurvature = 0.0;
  }

  QString info = QString(
    "Cross-Section Info: %1\n\n"
    "Total Path Length: %2 mm\n"
    "Number of Points: %3\n"
    "Average Segment Length: %4 mm\n\n"
    "Min Curvature: %5\n"
    "Max Curvature: %6\n"
    "Average Curvature: %7")
    .arg(QString::fromStdString(m_CurrentPathNode->GetName()))
    .arg(totalLength, 0, 'f', 4)
    .arg(numPoints)
    .arg(avgSegLen, 0, 'f', 4)
    .arg(minCurvature, 0, 'f', 6)
    .arg(maxCurvature, 0, 'f', 6)
    .arg(avgCurvature, 0, 'f', 6);

  QMessageBox::information(nullptr, "Cross-Section Info", info);
}

// --- PathPointSizeAction (consolidated from xq_PathPoint2D/3DSizeAction) ---

PathPointSizeAction::PathPointSizeAction(
  const QString& label, const QString& prefKey,
  int minVal, int maxVal, int defaultVal, int tickInterval,
  QWidget* parent)
  : QWidgetAction(parent)
  , m_Label(label)
  , m_PrefKey(prefKey)
  , m_MinVal(minVal)
  , m_MaxVal(maxVal)
  , m_DefaultVal(defaultVal)
  , m_TickInterval(tickInterval)
  , m_Slider(nullptr)
{
  setText(label);
}

PathPointSizeAction::~PathPointSizeAction()
{
}

QWidget* PathPointSizeAction::createWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QHBoxLayout(widget);
  layout->setContentsMargins(4, 2, 4, 2);

  auto* label = new QLabel(m_Label + ":", widget);
  layout->addWidget(label);

  m_Slider = new QSlider(Qt::Horizontal, widget);
  m_Slider->setRange(m_MinVal, m_MaxVal);
  m_Slider->setValue(m_DefaultVal);
  m_Slider->setTickPosition(QSlider::TicksBelow);
  m_Slider->setTickInterval(m_TickInterval);
  layout->addWidget(m_Slider);

  connect(m_Slider, &QSlider::valueChanged,
          this, &PathPointSizeAction::OnSizeChanged);

  auto* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService != nullptr)
  {
    auto prefs = prefService->GetSystemPreferences()->Node("/org.xq.views.pathplanning");
    if (prefs != nullptr)
    {
      m_Slider->setValue(prefs->GetInt(m_PrefKey.toStdString(), m_DefaultVal));
    }
  }

  return widget;
}

void PathPointSizeAction::OnSizeChanged(int value)
{
  auto* prefService = mitk::CoreServices::GetPreferencesService();
  if (prefService != nullptr)
  {
    auto prefs = prefService->GetSystemPreferences()->Node("/org.xq.views.pathplanning");
    if (prefs != nullptr)
    {
      prefs->PutInt(m_PrefKey.toStdString(), value);
    }
  }
}

void xq_VesselPlanningView::SmoothPath()
{
  if (m_CurrentPathNode.IsNull())
  {
    QMessageBox::information(nullptr, "Smooth Path", "Select a path first.");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
  if (!centerline)
  {
    QMessageBox::warning(nullptr, "Smooth Path", "Invalid path data.");
    return;
  }

  auto* segment = centerline->GetSegment();
  if (!segment || segment->GetAnchorCount() < 3)
  {
    QMessageBox::information(nullptr, "Smooth Path",
      "Path must have at least 3 control points for smoothing.");
    return;
  }

  // Show smoother dialog
  xq_CenterlineSmoother dialog;
  if (dialog.exec() != QDialog::Accepted)
    return;

  QString method = dialog.GetSmoothingMethod();
  int numOutput = dialog.GetOutputNumber();
  int numModes = dialog.GetSamplingNumber();

  std::vector<mitk::Point3D> controls = segment->GetAnchorPositions();
  const int N = static_cast<int>(controls.size());
  std::vector<mitk::Point3D> smoothed;

  if (method == "Fourier")
  {
    // XQ Fourier smoothing: decompose each coordinate into frequency domain,
    // truncate high-frequency modes, reconstruct
    int keepModes = std::min(numModes, N / 2);
    if (keepModes < 1) keepModes = 1;

    // Separate x, y, z coordinates
    std::vector<double> xCoords(N), yCoords(N), zCoords(N);
    for (int i = 0; i < N; ++i)
    {
      xCoords[i] = controls[i][0];
      yCoords[i] = controls[i][1];
      zCoords[i] = controls[i][2];
    }

    // DFT-based smoothing for each coordinate
    auto fourierSmooth = [&](const std::vector<double>& input, int outputCount) -> std::vector<double>
    {
      const int n = static_cast<int>(input.size());
      // Compute DFT coefficients (real and imaginary)
      std::vector<double> realCoeffs(n, 0.0), imagCoeffs(n, 0.0);
      for (int k = 0; k < n; ++k)
      {
        for (int j = 0; j < n; ++j)
        {
          double angle = -2.0 * M_PI * k * j / n;
          realCoeffs[k] += input[j] * std::cos(angle);
          imagCoeffs[k] += input[j] * std::sin(angle);
        }
      }

      // Reconstruct with only keepModes lowest frequencies
      std::vector<double> result(outputCount);
      for (int i = 0; i < outputCount; ++i)
      {
        double t = static_cast<double>(i) * n / outputCount;
        double val = realCoeffs[0] / n;
        for (int k = 1; k <= keepModes; ++k)
        {
          double angle = 2.0 * M_PI * k * t / n;
          val += 2.0 * (realCoeffs[k] * std::cos(angle)
                      - imagCoeffs[k] * std::sin(angle)) / n;
        }
        result[i] = val;
      }
      return result;
    };

    auto sX = fourierSmooth(xCoords, numOutput);
    auto sY = fourierSmooth(yCoords, numOutput);
    auto sZ = fourierSmooth(zCoords, numOutput);

    smoothed.resize(numOutput);
    for (int i = 0; i < numOutput; ++i)
    {
      smoothed[i][0] = sX[i];
      smoothed[i][1] = sY[i];
      smoothed[i][2] = sZ[i];
    }
  }
  else // Moving Average
  {
    int windowSize = std::max(3, numModes);
    if (windowSize % 2 == 0) ++windowSize;
    int halfW = windowSize / 2;

    smoothed.resize(N);
    for (int i = 0; i < N; ++i)
    {
      mitk::Point3D avg;
      avg.Fill(0.0);
      int count = 0;
      for (int j = i - halfW; j <= i + halfW; ++j)
      {
        int idx = std::clamp(j, 0, N - 1);
        avg[0] += controls[idx][0];
        avg[1] += controls[idx][1];
        avg[2] += controls[idx][2];
        ++count;
      }
      avg[0] /= count; avg[1] /= count; avg[2] /= count;
      smoothed[i] = avg;
    }

    // Keep first and last points fixed
    smoothed.front() = controls.front();
    smoothed.back() = controls.back();
  }

  // Create smoothed path as child node
  auto smoothCenterline = xq_VesselCenterline::New();
  auto* smoothSegment = new xq_CenterlineSegment();
  smoothSegment->ReplaceAnchors(smoothed);
  smoothCenterline->SetSegment(smoothSegment);

  mitk::DataNode::Pointer smoothNode = mitk::DataNode::New();
  smoothNode->SetData(smoothCenterline);
  smoothNode->SetName(m_CurrentPathNode->GetName() + "_smoothed");
  smoothNode->SetColor(0.2f, 1.0f, 0.6f);
  smoothNode->SetFloatProperty("pointsize", 1.5f);
  smoothNode->SetBoolProperty("show contour", true);
  smoothNode->SetBoolProperty("xq.pathplanning.path", true);
  xq::pipeline::MarkNode(smoothNode, xq::pipeline::Stage::Path);
  smoothNode->SetStringProperty(xq::pipeline::kAlgorithmProperty, "smoothed");
  EnsureSourceImageProperty(smoothNode, m_CurrentPathNode);

  GetDataStorage()->Add(smoothNode, m_CurrentPathNode);
  UpdatePathTable();
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  UpdatePathReslice();

  QMessageBox::information(nullptr, "Smooth Path",
    QString("Created smoothed path (%1) with %2 points.\nMethod: %3, Modes/Window: %4")
      .arg(smoothNode->GetName().c_str())
      .arg(smoothed.size())
      .arg(method)
      .arg(numModes));
}

mitk::Image::Pointer xq_VesselPlanningView::FindSelectedImage() const
{
  auto allNodes = GetDataStorage()->GetAll();
  for (auto nodeIt = allNodes->Begin(); nodeIt != allNodes->End(); ++nodeIt)
  {
    if (auto image = dynamic_cast<mitk::Image*>(nodeIt->Value()->GetData()))
      return image;
  }

  return nullptr;
}

mitk::DataNode::Pointer xq_VesselPlanningView::FindSelectedImageNode() const
{
  auto allNodes = GetDataStorage()->GetAll();
  for (auto nodeIt = allNodes->Begin(); nodeIt != allNodes->End(); ++nodeIt)
  {
    if (auto image = dynamic_cast<mitk::Image*>(nodeIt->Value()->GetData()))
      return nodeIt->Value();
  }
  return nullptr;
}

void xq_VesselPlanningView::EnsureSourceImageProperty(
    const mitk::DataNode::Pointer& pathNode,
    const mitk::DataNode* sourcePathNode)
{
  if (pathNode.IsNull())
    return;

  // If we have a source path node, copy its xq.source.image
  if (sourcePathNode)
  {
    std::string sourceImage;
    if (sourcePathNode->GetStringProperty(
            xq::pipeline::kSourceImageProperty, sourceImage) && !sourceImage.empty())
    {
      pathNode->SetStringProperty(
          xq::pipeline::kSourceImageProperty, sourceImage.c_str());
      return;
    }
  }

  // Otherwise, try to find the image node in DataStorage
  auto imageNode = FindSelectedImageNode();
  if (imageNode.IsNotNull())
  {
    pathNode->SetStringProperty(
        xq::pipeline::kSourceImageProperty, imageNode->GetName().c_str());
  }
}

void xq_VesselPlanningView::AddPathNodeToFolder(
    const mitk::DataNode::Pointer& pathNode)
{
  if (pathNode.IsNull())
    return;

  auto pathFolder = xq::pipeline::FindCategoryFolder(
      GetDataStorage(), xq::pipeline::Stage::Path);
  if (pathFolder.IsNotNull())
    GetDataStorage()->Add(pathNode, pathFolder);
}

void xq_VesselPlanningView::OnReslicePositionChanged(int index)
{
  if (m_CurrentSlicedGeometry.IsNull() || !m_ResliceSlider || !m_ResliceLabel)
    return;

  const int maxIndex = m_ResliceSlider->maximum();
  index = std::max(0, std::min(index, maxIndex));
  m_ResliceLabel->setText(QString("%1 / %2").arg(index + 1).arg(maxIndex + 1));

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

void xq_VesselPlanningView::UpdatePathReslice()
{
  if (!m_ResliceSlider || !m_ResliceLabel || !m_ResliceSizeSpin)
    return;

  if (m_CurrentPathNode.IsNull())
  {
    m_CurrentSlicedGeometry = nullptr;
    m_ResliceSlider->setEnabled(false);
    m_ResliceSlider->setMinimum(0);
    m_ResliceSlider->setMaximum(0);
    m_ResliceLabel->setText("-- / --");
    return;
  }

  auto* centerline = dynamic_cast<xq_VesselCenterline*>(m_CurrentPathNode->GetData());
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
  if (!baseData)
    baseData = m_CurrentPathNode->GetData();

  m_CurrentSlicedGeometry = xq_SegmentationUtils::CreateSlicedGeometry(
    frames, baseData, m_ResliceSizeSpin->value(), true);

  m_ResliceSlider->setEnabled(true);
  m_ResliceSlider->setMinimum(0);
  m_ResliceSlider->setMaximum(static_cast<int>(frames.size()) - 1);
  if (m_ResliceSlider->value() > m_ResliceSlider->maximum())
    m_ResliceSlider->setValue(0);

  OnReslicePositionChanged(m_ResliceSlider->value());
}
