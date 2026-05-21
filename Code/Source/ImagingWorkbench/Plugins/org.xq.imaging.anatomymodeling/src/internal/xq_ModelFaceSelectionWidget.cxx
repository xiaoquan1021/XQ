#include "xq_ModelFaceSelectionWidget.h"

#include <xq_Model.h>
#include <xq_VascularGeometry.h>

#include <mitkSurface.h>

#include <vtkPolyData.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>

#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QColorDialog>
#include <QInputDialog>

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

xq_ModelFaceSelectionWidget::xq_ModelFaceSelectionWidget(QWidget* parent)
  : QWidget(parent)
  , m_Table(new QTableWidget(this))
  , m_ContextMenu(new QMenu(this))
  , m_ModelNode(nullptr)
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_Table);

  m_Table->setColumnCount(COL_COUNT);
  m_Table->setHorizontalHeaderLabels(
    QStringList() << "Visible" << "Name" << "Type" << "Color");
  m_Table->horizontalHeader()->setStretchLastSection(true);
  m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_Table->setSelectionMode(QAbstractItemView::ExtendedSelection);

  m_Table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_Table, &QTableWidget::customContextMenuRequested,
          this, &xq_ModelFaceSelectionWidget::ShowContextMenu);
  connect(m_Table, &QTableWidget::cellDoubleClicked,
          this, &xq_ModelFaceSelectionWidget::OnCellDoubleClicked);
  connect(m_Table, &QTableWidget::cellClicked,
          this, &xq_ModelFaceSelectionWidget::OnCellClicked);

  // Context menu actions
  m_RenameAction = m_ContextMenu->addAction("Rename Face");
  connect(m_RenameAction, &QAction::triggered,
          this, &xq_ModelFaceSelectionWidget::RenameSelectedFace);

  m_ContextMenu->addSeparator();

  m_SelectAllAction = m_ContextMenu->addAction("Select All");
  connect(m_SelectAllAction, &QAction::triggered,
          this, &xq_ModelFaceSelectionWidget::SelectAllFaces);

  m_DeselectAllAction = m_ContextMenu->addAction("Deselect All");
  connect(m_DeselectAllAction, &QAction::triggered,
          this, &xq_ModelFaceSelectionWidget::DeselectAllFaces);
}

xq_ModelFaceSelectionWidget::~xq_ModelFaceSelectionWidget()
{
}

void xq_ModelFaceSelectionWidget::SetModelNode(mitk::DataNode::Pointer node)
{
  m_ModelNode = node;
  Clear();
  if (node.IsNotNull())
    PopulateTable(node);
}

void xq_ModelFaceSelectionWidget::Clear()
{
  m_Table->setRowCount(0);
}

void xq_ModelFaceSelectionWidget::PopulateTable(mitk::DataNode::Pointer node)
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

  float nodeColor[3] = {0.8f, 0.8f, 0.8f};
  node->GetColor(nodeColor);
  QColor baseColor = QColor::fromRgbF(nodeColor[0], nodeColor[1], nodeColor[2]);

  for (int fid : faceIdSet)
  {
    int row = m_Table->rowCount();
    m_Table->insertRow(row);

    // Visible column (checkbox)
    QTableWidgetItem* visItem = new QTableWidgetItem();
    visItem->setCheckState(Qt::Checked);
    visItem->setData(Qt::UserRole, fid);
    m_Table->setItem(row, COL_VISIBLE, visItem);

    // Name column
    QString faceName = (fid == 0) ? QString::fromStdString(node->GetName())
                                  : QString("face_%1").arg(fid);
    m_Table->setItem(row, COL_NAME, new QTableWidgetItem(faceName));

    // Type column
    QString faceType = (fid == 0) ? "wall" : "cap";
    m_Table->setItem(row, COL_TYPE, new QTableWidgetItem(faceType));

    // Color column (color swatch)
    QTableWidgetItem* colorItem = new QTableWidgetItem();
    colorItem->setBackground(QBrush(baseColor));
    colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
    m_Table->setItem(row, COL_COLOR, colorItem);
  }
}

QList<xq_ModelFaceSelectionWidget::FaceEntry>
xq_ModelFaceSelectionWidget::GetSelectedFaces() const
{
  QList<FaceEntry> selected;
  for (int row = 0; row < m_Table->rowCount(); ++row)
  {
    if (m_Table->item(row, COL_VISIBLE)->isSelected())
    {
      FaceEntry entry;
      entry.id = m_Table->item(row, COL_VISIBLE)->data(Qt::UserRole).toInt();
      entry.name = m_Table->item(row, COL_NAME)->text();
      entry.type = m_Table->item(row, COL_TYPE)->text();
      entry.color = m_Table->item(row, COL_COLOR)->background().color();
      entry.visible = (m_Table->item(row, COL_VISIBLE)->checkState() == Qt::Checked);
      selected.append(entry);
    }
  }
  return selected;
}

QList<xq_ModelFaceSelectionWidget::FaceEntry>
xq_ModelFaceSelectionWidget::GetAllFaces() const
{
  QList<FaceEntry> all;
  for (int row = 0; row < m_Table->rowCount(); ++row)
  {
    FaceEntry entry;
    entry.id = m_Table->item(row, COL_VISIBLE)->data(Qt::UserRole).toInt();
    entry.name = m_Table->item(row, COL_NAME)->text();
    entry.type = m_Table->item(row, COL_TYPE)->text();
    entry.color = m_Table->item(row, COL_COLOR)->background().color();
    entry.visible = (m_Table->item(row, COL_VISIBLE)->checkState() == Qt::Checked);
    all.append(entry);
  }
  return all;
}

void xq_ModelFaceSelectionWidget::OnCellDoubleClicked(int row, int column)
{
  if (column == COL_NAME)
  {
    QTableWidgetItem* item = m_Table->item(row, COL_NAME);
    if (!item)
      return;

    bool ok = false;
    QString newName = QInputDialog::getText(
      this, "Rename Face", "New name:", QLineEdit::Normal, item->text(), &ok);

    if (ok && !newName.isEmpty())
    {
      int faceId = m_Table->item(row, COL_VISIBLE)->data(Qt::UserRole).toInt();
      item->setText(newName);
      emit FaceRenamed(faceId, newName);
    }
  }
}

void xq_ModelFaceSelectionWidget::OnCellClicked(int row, int column)
{
  if (column == COL_VISIBLE)
  {
    QTableWidgetItem* item = m_Table->item(row, COL_VISIBLE);
    if (!item)
      return;

    int faceId = item->data(Qt::UserRole).toInt();
    bool visible = (item->checkState() == Qt::Checked);
    emit FaceVisibilityToggled(faceId, visible);
  }
  else if (column == COL_COLOR)
  {
    QTableWidgetItem* colorItem = m_Table->item(row, COL_COLOR);
    if (!colorItem)
      return;

    QColor current = colorItem->background().color();
    QColor chosen = QColorDialog::getColor(current, this, "Select Face Color");
    if (chosen.isValid())
    {
      colorItem->setBackground(QBrush(chosen));
      int faceId = m_Table->item(row, COL_VISIBLE)->data(Qt::UserRole).toInt();
      emit FaceColorChanged(faceId, chosen);
    }
  }

  emit FaceSelectionChanged();
}

void xq_ModelFaceSelectionWidget::ShowContextMenu(const QPoint& pos)
{
  m_ContextMenu->popup(m_Table->viewport()->mapToGlobal(pos));
}

void xq_ModelFaceSelectionWidget::RenameSelectedFace()
{
  int row = m_Table->currentRow();
  if (row < 0)
    return;

  OnCellDoubleClicked(row, COL_NAME);
}

void xq_ModelFaceSelectionWidget::SelectAllFaces()
{
  m_Table->selectAll();
  emit FaceSelectionChanged();
}

void xq_ModelFaceSelectionWidget::DeselectAllFaces()
{
  m_Table->clearSelection();
  emit FaceSelectionChanged();
}
