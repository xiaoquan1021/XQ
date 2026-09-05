#include "xq_LocalMeshSizeWidget.h"

#include <xq_Model.h>
#include <xq_VascularGeometry.h>

#include <mitkSurface.h>

#include <vtkPolyData.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>

#include <set>

namespace {

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

} // namespace

xq_LocalMeshSizeWidget::xq_LocalMeshSizeWidget(QWidget* parent)
  : QWidget(parent)
  , m_Table(new QTableWidget(this))
  , m_AddButton(new QPushButton("Add", this))
  , m_RemoveButton(new QPushButton("Remove", this))
  , m_ModelNode(nullptr)
{
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  m_Table->setColumnCount(COL_COUNT);
  m_Table->setHorizontalHeaderLabels(
    QStringList() << "Face Name" << "Type" << "Edge Size");
  m_Table->horizontalHeader()->setStretchLastSection(true);
  m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_Table->setSelectionMode(QAbstractItemView::ExtendedSelection);

  mainLayout->addWidget(m_Table);

  QHBoxLayout* btnLayout = new QHBoxLayout();
  btnLayout->addWidget(m_AddButton);
  btnLayout->addWidget(m_RemoveButton);
  btnLayout->addStretch();
  mainLayout->addLayout(btnLayout);

  connect(m_AddButton, &QPushButton::clicked,
          this, &xq_LocalMeshSizeWidget::AddEntry);
  connect(m_RemoveButton, &QPushButton::clicked,
          this, &xq_LocalMeshSizeWidget::RemoveSelectedEntries);
}

xq_LocalMeshSizeWidget::~xq_LocalMeshSizeWidget()
{
}

void xq_LocalMeshSizeWidget::SetModelNode(mitk::DataNode::Pointer node)
{
  m_ModelNode = node;
  Clear();
  if (node.IsNotNull())
    PopulateFromModel(node);
}

void xq_LocalMeshSizeWidget::Clear()
{
  m_Table->setRowCount(0);
}

void xq_LocalMeshSizeWidget::PopulateFromModel(mitk::DataNode::Pointer node)
{
  vtkPolyData* poly = GetModelPolyData(node);
  if (!poly)
    return;

  vtkCellData* cellData = poly->GetCellData();

  std::set<int> faceIdSet;
  vtkDataArray* faceIds = cellData ? cellData->GetArray("FaceIds") : nullptr;

  if (faceIds)
  {
    for (vtkIdType i = 0; i < faceIds->GetNumberOfTuples(); ++i)
      faceIdSet.insert(static_cast<int>(faceIds->GetTuple1(i)));
  }
  else
  {
    faceIdSet.insert(0);
  }

  for (int fid : faceIdSet)
  {
    int row = m_Table->rowCount();
    m_Table->insertRow(row);

    QString faceName = (fid == 0) ? QString::fromStdString(node->GetName())
                                  : QString("face_%1").arg(fid);
    m_Table->setItem(row, COL_FACE_NAME, new QTableWidgetItem(faceName));

    QString faceType = (fid == 0) ? "wall" : "cap";
    m_Table->setItem(row, COL_TYPE, new QTableWidgetItem(faceType));

    // Default edge size (empty = use global)
    m_Table->setItem(row, COL_EDGE_SIZE, new QTableWidgetItem(""));
  }
}

QList<xq_LocalMeshSizeWidget::LocalSizeEntry>
xq_LocalMeshSizeWidget::GetAllEntries() const
{
  QList<LocalSizeEntry> entries;
  for (int row = 0; row < m_Table->rowCount(); ++row)
  {
    auto* sizeItem = m_Table->item(row, COL_EDGE_SIZE);
    if (!sizeItem) continue;
    QString sizeText = sizeItem->text().trimmed();
    if (sizeText.isEmpty())
      continue;

    bool ok = false;
    double sz = sizeText.toDouble(&ok);
    if (!ok || sz <= 0.0)
      continue;

    LocalSizeEntry entry;
    auto* nameItem = m_Table->item(row, COL_FACE_NAME);
    auto* typeItem = m_Table->item(row, COL_TYPE);
    entry.faceName = nameItem ? nameItem->text() : QString();
    entry.faceType = typeItem ? typeItem->text() : QString();
    entry.edgeSize = sz;
    entries.append(entry);
  }
  return entries;
}

void xq_LocalMeshSizeWidget::AddEntry()
{
  bool ok = false;
  QString faceName = QInputDialog::getText(
    this, "Add Local Size", "Face name:", QLineEdit::Normal, "", &ok);
  if (!ok || faceName.isEmpty())
    return;

  double edgeSize = QInputDialog::getDouble(
    this, "Add Local Size",
    "Edge size for face '" + faceName + "':",
    1.0, 0.001, 1000.0, 3, &ok);
  if (!ok)
    return;

  int row = m_Table->rowCount();
  m_Table->insertRow(row);
  m_Table->setItem(row, COL_FACE_NAME, new QTableWidgetItem(faceName));
  m_Table->setItem(row, COL_TYPE, new QTableWidgetItem("wall"));
  m_Table->setItem(row, COL_EDGE_SIZE,
    new QTableWidgetItem(QString::number(edgeSize, 'f', 3)));

  emit EntriesChanged();
}

void xq_LocalMeshSizeWidget::RemoveSelectedEntries()
{
  QList<int> rowsToRemove;
  for (const QModelIndex& idx : m_Table->selectionModel()->selectedRows())
    rowsToRemove.append(idx.row());

  // Remove from bottom to top to keep indices valid
  std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

  for (int row : rowsToRemove)
    m_Table->removeRow(row);

  emit EntriesChanged();
}
