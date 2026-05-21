#include "xq_DataExplorerView.h"
#include "ui_xq_DataExplorerView.h"
#include "xq_mitkIContextMenuAction.h"
#include <xq_PipelineDataUtils.h>

// Forward-declare xq_DataFolder — full header lives in ProjectManagement
// where this plugin can't depend on it.

#include <QmitkDataStorageTreeModel.h>
#include <QmitkIOUtil.h>

#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkRenderingManager.h>
#include <mitkBaseRenderer.h>
#include <mitkProperties.h>
#include <mitkStatusBar.h>

#include <berryPlatform.h>
#include <berryIExtensionRegistry.h>
#include <berryIConfigurationElement.h>

#include <QMenu>
#include <QAction>
#include <QSlider>
#include <QTreeView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QColorDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QItemSelectionModel>

#include <QClipboard>
#include <QApplication>
#include <QTimer>

#include <mitkIOUtil.h>
#include <mitkSurface.h>
#include <mitkImage.h>
#include <mitkPointSet.h>

#include <berryIWorkbenchPage.h>
#include <berryIWorkbenchPartSite.h>

namespace
{

// XQ fix: MITK allows "visible" to be overridden per-renderer. If a
// renderer-specific override exists, toggling only the generic property has
// no visible effect — the standard display does not refresh. Apply the new
// visibility to the generic property AND to every registered renderer so
// the mapper actually picks it up. Finally mark the node Modified to force
// the rendering pipeline to re-evaluate.
inline void xqSetVisibilityEverywhere(mitk::DataNode* node, bool visible)
{
    if (!node)
        return;
    node->SetVisibility(visible);  // generic
    const auto& rwMap =
        mitk::RenderingManager::GetInstance()->GetAllRegisteredRenderWindows();
    for (auto* rw : rwMap)
    {
        if (!rw)
            continue;
        if (auto* renderer = mitk::BaseRenderer::GetInstance(rw))
            node->SetVisibility(visible, renderer);
    }
    node->Modified();
}

void AddDataNotesNodeNameCandidates(QStringList& candidates, const QString& text)
{
    for (const QString& part : text.split(';', Qt::SkipEmptyParts))
    {
        const QString candidate = part.trimmed();
        if (!candidate.isEmpty() && !candidates.contains(candidate))
            candidates << candidate;
    }
}

} // namespace

// Safe wrapper that guards against null root node in headerData.
// MITK's QmitkDataStorageTreeModel stores root DataNode as WeakPointer;
// the node dies immediately after SetDataStorage, causing headerData to crash.
// Also provides colored type indicators via DecorationRole.
class SafeDataStorageTreeModel : public QmitkDataStorageTreeModel
{
  Q_OBJECT

public:
  using QmitkDataStorageTreeModel::QmitkDataStorageTreeModel;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if (orientation == Qt::Horizontal)
    {
      if (role == Qt::DisplayRole)
        return QStringLiteral("Data Nodes");
      if (role == Qt::CheckStateRole && section == 0)
        return m_RootChecked ? Qt::Checked : Qt::Unchecked;
    }
    return QVariant();
  }

  bool setHeaderData(int section, Qt::Orientation orientation,
                     const QVariant& value, int role = Qt::EditRole) override
  {
    Q_UNUSED(value);
    if (orientation == Qt::Horizontal && section == 0 &&
        (role == Qt::CheckStateRole || role == Qt::EditRole))
    {
      m_RootChecked = !m_RootChecked;
      setAllChildrenVisibility(m_RootChecked);
      emit headerDataChanged(orientation, section, section);
      return true;
    }
    return false;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (!index.isValid())
      return QVariant();

    auto node = GetNode(index);
    if (node.IsNull())
      return QVariant();

    // Fix: MITK returns bool for CheckStateRole, but Qt6 needs Qt::CheckState.
    // bool true → 1 → Qt::PartiallyChecked (wrong); we need Qt::Checked (2).
    if (role == Qt::CheckStateRole)
    {
      return node->IsVisible(nullptr) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DecorationRole)
    {
      if (node->GetData())
      {
        QPixmap px(14, 14);
        px.fill(Qt::transparent);
        QPainter painter(&px);
        painter.setRenderHint(QPainter::Antialiasing);
        QString className = QString::fromStdString(node->GetData()->GetNameOfClass());

        if (className.contains("Image"))
        {
          painter.setBrush(QColor(70, 130, 220));
          painter.setPen(Qt::NoPen);
          painter.drawRoundedRect(1, 1, 12, 12, 2, 2);
        }
        else if (className.contains("Surface"))
        {
          painter.setBrush(QColor(60, 180, 90));
          painter.setPen(Qt::NoPen);
          QPolygon tri;
          tri << QPoint(7, 1) << QPoint(13, 13) << QPoint(1, 13);
          painter.drawPolygon(tri);
        }
        else if (className.contains("PointSet"))
        {
          painter.setBrush(QColor(220, 80, 60));
          painter.setPen(Qt::NoPen);
          painter.drawEllipse(1, 1, 12, 12);
        }
        else
        {
          painter.setBrush(QColor(150, 150, 150));
          painter.setPen(Qt::NoPen);
          painter.drawRoundedRect(1, 1, 12, 12, 6, 6);
        }
        painter.end();
        return QIcon(px);
      }
    }

    return QmitkDataStorageTreeModel::data(index, role);
  }

  bool setData(const QModelIndex& index, const QVariant& value, int role) override
  {
    if (!index.isValid())
      return false;

    auto node = GetNode(index);
    if (node.IsNull())
      return false;

    if (role == Qt::CheckStateRole)
    {
      bool isVisible = node->IsVisible(nullptr);
      bool newVisibility = !isVisible;
      xqSetVisibilityEverywhere(node.GetPointer(), newVisibility);
      // Recursively toggle children visibility
      setChildrenVisibility(index, newVisibility);
      emit dataChanged(index, index);
      emit nodeVisibilityChanged();
      updateRootCheckState();
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
      return true;
    }

    return QmitkDataStorageTreeModel::setData(index, value, role);
  }

private:
  bool m_RootChecked = true;  // default ON

  void setChildrenVisibility(const QModelIndex& parent, bool visible)
  {
    int childCount = rowCount(parent);
    for (int i = 0; i < childCount; ++i)
    {
      QModelIndex child = index(i, 0, parent);
      auto childNode = GetNode(child);
      if (!childNode.IsNull())
      {
        xqSetVisibilityEverywhere(childNode.GetPointer(), visible);
        emit dataChanged(child, child);
        setChildrenVisibility(child, visible);
      }
    }
  }

  void setAllChildrenVisibility(bool visible)
  {
    QModelIndex rootIdx; // invalid = root
    int topCount = rowCount(rootIdx);
    for (int i = 0; i < topCount; ++i)
    {
      QModelIndex child = index(i, 0, rootIdx);
      auto node = GetNode(child);
      if (!node.IsNull())
      {
        xqSetVisibilityEverywhere(node.GetPointer(), visible);
        emit dataChanged(child, child);
        setChildrenVisibility(child, visible);
      }
    }
    emit nodeVisibilityChanged();
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }

  void updateRootCheckState()
  {
    QModelIndex rootIdx;
    int topCount = rowCount(rootIdx);
    if (topCount == 0)
    {
      m_RootChecked = true;
      return;
    }
    bool anyVisible = false;
    for (int i = 0; i < topCount; ++i)
    {
      auto node = GetNode(index(i, 0, rootIdx));
      if (!node.IsNull() && node->IsVisible(nullptr))
      {
        anyVisible = true;
        break;
      }
    }
    m_RootChecked = anyVisible;
    emit headerDataChanged(Qt::Horizontal, 0, 0);
  }
};

const QString xq_DataExplorerView::VIEW_ID = "org.xq.views.datamanager";

xq_DataExplorerView::xq_DataExplorerView()
  : m_NodeTreeView(nullptr)
  , m_NodeTreeModel(nullptr)
  , m_OpacitySlider(nullptr)
  , m_ColorButton(nullptr)
  , m_OpacityValueLabel(nullptr)
  , m_SearchBox(nullptr)
  , m_ContextMenu(nullptr)
  , m_Ui(nullptr)
  , m_InternalSliderUpdate(false)
  , m_PropertiesTable(nullptr)
  , m_PropertiesToggle(nullptr)
  , m_RenderDebounceTimer(nullptr)
{
}

xq_DataExplorerView::~xq_DataExplorerView()
{
  delete m_NodeTreeModel;
  delete m_Ui;
}

QTreeView* xq_DataExplorerView::GetTreeView()
{
  return m_NodeTreeView;
}

QItemSelectionModel* xq_DataExplorerView::GetDataNodeSelectionModel() const
{
  return m_NodeTreeView ? m_NodeTreeView->selectionModel() : nullptr;
}

void xq_DataExplorerView::CreateQtPartControl(QWidget* parent)
{
  m_Ui = new Ui::xq_DataExplorerView;
  m_Ui->setupUi(parent);

  m_NodeTreeView = m_Ui->treeView;
  m_OpacitySlider = m_Ui->opacitySlider;
  m_ColorButton = m_Ui->colorButton;
  m_OpacityValueLabel = m_Ui->opacityValueLabel;
  m_SearchBox = m_Ui->searchBox;

  m_NodeTreeModel = new SafeDataStorageTreeModel(GetDataStorage(), false, parent);
  m_NodeTreeModel->SetAllowHierarchyChange(true);
  m_NodeTreeView->setModel(m_NodeTreeModel);
  m_NodeTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Enable header checkbox click to toggle all nodes
  m_NodeTreeView->header()->setSectionsClickable(true);
  connect(m_NodeTreeView->header(), &QHeaderView::sectionClicked,
          this, [this](int section) {
            m_NodeTreeModel->setHeaderData(section, Qt::Horizontal, QVariant(), Qt::CheckStateRole);
          });

  // Enable drag-and-drop for node reparenting
  m_NodeTreeView->setDragEnabled(true);
  m_NodeTreeView->setAcceptDrops(true);
  m_NodeTreeView->setDropIndicatorShown(true);
  m_NodeTreeView->setDragDropMode(QAbstractItemView::InternalMove);

  connect(m_OpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(OpacityChanged(int)));
  connect(m_ColorButton, SIGNAL(clicked()), this, SLOT(ColorChanged()));

  // Selection tracking — update opacity slider when selection changes
  connect(m_NodeTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &xq_DataExplorerView::OnSelectionChanged);
  connect(m_NodeTreeView, &QTreeView::doubleClicked,
          this, &xq_DataExplorerView::OnDataTreeDoubleClicked);

  // Search filtering
  connect(m_SearchBox, &QLineEdit::textChanged,
          this, &xq_DataExplorerView::OnSearchTextChanged);

  // ========== Context Menu ==========
  m_ContextMenu = new QMenu(m_NodeTreeView);

  auto* renameAction = new QAction(QIcon::fromTheme("edit-rename"), "Rename...", m_ContextMenu);
  renameAction->setShortcut(QKeySequence(Qt::Key_F2));
  connect(renameAction, &QAction::triggered, this, &xq_DataExplorerView::RenameSelectedNode);
  m_ContextMenu->addAction(renameAction);

  auto* toggleVisAction = new QAction("Toggle Visibility", m_ContextMenu);
  toggleVisAction->setShortcut(QKeySequence(Qt::Key_Space));
  connect(toggleVisAction, &QAction::triggered, this, &xq_DataExplorerView::ToggleVisibility);
  m_ContextMenu->addAction(toggleVisAction);

  auto* removeAction = new QAction(QIcon::fromTheme("edit-delete"), "Remove", m_ContextMenu);
  removeAction->setShortcut(QKeySequence::Delete);
  connect(removeAction, &QAction::triggered, this, &xq_DataExplorerView::RemoveSelectedNodes);
  m_ContextMenu->addAction(removeAction);

  m_ContextMenu->addSeparator();

  auto* showOnlyAction = new QAction("Show Only Selected", m_ContextMenu);
  connect(showOnlyAction, &QAction::triggered, this, &xq_DataExplorerView::ShowOnlySelected);
  m_ContextMenu->addAction(showOnlyAction);

  auto* makeAllVisibleAction = new QAction("Make All Visible", m_ContextMenu);
  connect(makeAllVisibleAction, &QAction::triggered, this, &xq_DataExplorerView::MakeAllVisible);
  m_ContextMenu->addAction(makeAllVisibleAction);

  auto* makeAllInvisibleAction = new QAction("Make All Invisible", m_ContextMenu);
  connect(makeAllInvisibleAction, &QAction::triggered, this, &xq_DataExplorerView::MakeAllInvisible);
  m_ContextMenu->addAction(makeAllInvisibleAction);

  m_ContextMenu->addSeparator();

  auto* reinitAction = new QAction("Reinitialize Node", m_ContextMenu);
  connect(reinitAction, &QAction::triggered, this, &xq_DataExplorerView::ReinitializeSelectedNode);
  m_ContextMenu->addAction(reinitAction);

  auto* globalReinitAction = new QAction("Global Reinit", m_ContextMenu);
  connect(globalReinitAction, &QAction::triggered, this, &xq_DataExplorerView::GlobalReinit);
  m_ContextMenu->addAction(globalReinitAction);

  m_ContextMenu->addSeparator();

  auto* nodeInfoAction = new QAction("Node Information...", m_ContextMenu);
  connect(nodeInfoAction, &QAction::triggered, this, &xq_DataExplorerView::ShowNodeInfo);
  m_ContextMenu->addAction(nodeInfoAction);

  auto* dicomInfoAction = new QAction("DICOM Information...", m_ContextMenu);
  connect(dicomInfoAction, &QAction::triggered, this, &xq_DataExplorerView::ShowDicomInfo);
  m_ContextMenu->addAction(dicomInfoAction);

  m_ContextMenu->addSeparator();

  // Surface representation submenu
  auto* reprMenu = new QMenu("Representation", m_ContextMenu);
  auto* surfaceReprAction = new QAction("Surface", reprMenu);
  connect(surfaceReprAction, &QAction::triggered, this, &xq_DataExplorerView::SetRepresentationSurface);
  reprMenu->addAction(surfaceReprAction);

  auto* wireframeReprAction = new QAction("Wireframe", reprMenu);
  connect(wireframeReprAction, &QAction::triggered, this, &xq_DataExplorerView::SetRepresentationWireframe);
  reprMenu->addAction(wireframeReprAction);

  auto* pointsReprAction = new QAction("Points", reprMenu);
  connect(pointsReprAction, &QAction::triggered, this, &xq_DataExplorerView::SetRepresentationPoints);
  reprMenu->addAction(pointsReprAction);

  m_ContextMenu->addMenu(reprMenu);

  auto* addImageAction = new QAction("Add/Replace Image...", m_ContextMenu);
  connect(addImageAction, &QAction::triggered, this, &xq_DataExplorerView::AddImageToSelectedFolder);
  m_ContextMenu->addAction(addImageAction);

  auto* exportAction = new QAction("Export Selected...", m_ContextMenu);
  connect(exportAction, &QAction::triggered, this, &xq_DataExplorerView::ExportSelectedNode);
  m_ContextMenu->addAction(exportAction);

  // Duplicate node action
  auto* duplicateAction = new QAction("Duplicate Node", m_ContextMenu);
  connect(duplicateAction, &QAction::triggered, this, [this]() {
    mitk::DataNode* node = GetSelectedNode();
    if (!node || !node->GetData()) return;
    mitk::DataNode::Pointer newNode = mitk::DataNode::New();
    newNode->SetData(node->GetData());
    newNode->SetName(node->GetName() + "_copy");
    // Copy visual properties
    float opacity = 1.0f;
    node->GetFloatProperty("opacity", opacity);
    newNode->SetFloatProperty("opacity", opacity);
    float rgb[3] = {1.0f, 1.0f, 1.0f};
    node->GetColor(rgb);
    newNode->SetColor(rgb[0], rgb[1], rgb[2]);
    bool visible = true;
    node->GetBoolProperty("visible", visible);
    newNode->SetBoolProperty("visible", visible);
    GetDataStorage()->Add(newNode);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  });
  m_ContextMenu->addAction(duplicateAction);

  m_NodeTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_NodeTreeView, &QTreeView::customContextMenuRequested,
          this, [this](const QPoint& pos) {
            // Dynamically add extension-contributed actions before showing the menu.
            // Remove any previously-added extension actions (tracked by object name prefix).
            QList<QAction*> actions = m_ContextMenu->actions();
            for (QAction* a : actions)
            {
              if (a->objectName().startsWith("xq_ext_"))
              {
                m_ContextMenu->removeAction(a);
                a->deleteLater();
              }
            }

            // Restore any hardcoded actions hidden by a previous dedup pass.
            actions = m_ContextMenu->actions();
            for (QAction* a : actions)
            {
              if (!a->objectName().startsWith("xq_ext_"))
                a->setVisible(true);
            }

            mitk::DataNode* selectedNode = GetSelectedNode();
            if (selectedNode && selectedNode->GetData())
            {
              std::string dataClassName = selectedNode->GetData()->GetNameOfClass();

              // Build candidate names for matching: data class name,
              // plus folder type string derived from the class name.
              std::vector<std::string> candidateNames;
              candidateNames.push_back(dataClassName);
              // If the data class looks like a folder (e.g. "xq_ImageFolder"),
              // also match on the short folder-type string (e.g. "ImageFolder").
              if (dataClassName.size() > 6 &&
                  dataClassName.compare(dataClassName.size() - 6, 6, "Folder") == 0)
              {
                // Strip "xq_" prefix to get "ImageFolder", "SimulationFolder", etc.
                if (dataClassName.compare(0, 3, "xq_") == 0)
                  candidateNames.push_back(dataClassName.substr(3));
                else
                  candidateNames.push_back(dataClassName);
              }

              berry::IExtensionRegistry* registry =
                  berry::Platform::GetExtensionRegistry();
              if (registry)
              {
                QList<berry::IConfigurationElement::Pointer> configs =
                    registry->GetConfigurationElementsFor(
                        "org.xq.core.datamanager.contextMenuActions");

                bool firstExtAction = true;
                for (const auto& config : configs)
                {
                  QString nodeDesc = config->GetAttribute("nodeDescriptorName");
                  // Match by data class name or folder type
                  // (the nodeDescriptorName in plugin.xml maps to the MITK
                  // data class name, e.g. "xq_ImageFolder", or a folder type
                  // string like "ImageFolder")
                  bool matchesAny = false;
                  for (const auto& name : candidateNames)
                  {
                    if (nodeDesc.toStdString() == name)
                    {
                      matchesAny = true;
                      break;
                    }
                  }
                  if (!matchesAny)
                    continue;

                  if (firstExtAction)
                  {
                    m_ContextMenu->addSeparator();
                    firstExtAction = false;
                  }

                  QString label = config->GetAttribute("label");
                  QAction* extAction = new QAction(label, m_ContextMenu);
                  extAction->setObjectName(QString("xq_ext_") + label);

                  // Store config element for lazy instantiation
                  QString className = config->GetAttribute("class");
                  connect(extAction, &QAction::triggered, this,
                          [this, className, config]() {
                    auto* actionObj =
                        config->CreateExecutableExtension<xqmitk::IContextMenuAction>("class");
                    if (!actionObj)
                    {
                      MITK_WARN << "Failed to create context menu action: "
                                << className.toStdString();
                      return;
                    }
                    actionObj->SetDataStorage(GetDataStorage());
                    QList<mitk::DataNode::Pointer> nodes;
                    mitk::DataNode* selNode = GetSelectedNode();
                    if (selNode)
                      nodes << selNode;
                    actionObj->Run(nodes);
                    delete actionObj;
                  });

                  m_ContextMenu->addAction(extAction);
                }
              }
            }

            // Suppress hardcoded actions whose label duplicates an
            // extension-provided action (e.g. "Add/Replace Image...").
            // Keep the extension action — it is more flexible.
            {
              QSet<QString> extLabels;
              QList<QAction*> allActions = m_ContextMenu->actions();
              for (QAction* a : allActions)
              {
                if (a->objectName().startsWith("xq_ext_"))
                  extLabels.insert(a->text());
              }
              for (QAction* a : allActions)
              {
                if (!a->objectName().startsWith("xq_ext_") && extLabels.contains(a->text()))
                  a->setVisible(false);
              }
            }

            m_ContextMenu->exec(m_NodeTreeView->viewport()->mapToGlobal(pos));
          });

  // Add shortcut actions to tree view for keyboard access
  m_NodeTreeView->addAction(renameAction);
  m_NodeTreeView->addAction(toggleVisAction);
  m_NodeTreeView->addAction(removeAction);
  m_NodeTreeView->addAction(exportAction);

  // Properties panel
  m_PropertiesToggle = m_Ui->propertiesToggle;
  m_PropertiesTable = m_Ui->propertiesTable;
  m_PropertiesTable->horizontalHeader()->setStretchLastSection(true);
  m_PropertiesTable->verticalHeader()->setVisible(false);
  m_PropertiesTable->setVisible(false);

  connect(m_PropertiesTable, &QTableWidget::cellDoubleClicked,
          this, &xq_DataExplorerView::OnDataNotesDoubleClicked);

  // Prevent double-click from starting cell edit instead of firing cellDoubleClicked
  m_PropertiesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Style the toggle button for clear visual feedback
  m_PropertiesToggle->setFlat(false);
  m_PropertiesToggle->setStyleSheet(
    "QPushButton { text-align: left; padding: 4px 8px; border: 1px solid #CBD5E1; "
    "             border-radius: 3px; background: #F8FAFC; color: #1E293B; }"
    "QPushButton:checked { background: #EFF6FF; border-color: #2563EB; color: #1E40AF; }"
    "QPushButton:hover { background: #EFF6FF; }");

  connect(m_PropertiesToggle, &QPushButton::clicked,
          this, &xq_DataExplorerView::TogglePropertiesPanel);

  // Debounce timer for rendering (coalesces rapid property changes)
  m_RenderDebounceTimer = new QTimer(this);
  m_RenderDebounceTimer->setSingleShot(true);
  m_RenderDebounceTimer->setInterval(80);
  connect(m_RenderDebounceTimer, &QTimer::timeout, this, &xq_DataExplorerView::DeferredRenderUpdate);
}

void xq_DataExplorerView::SetFocus()
{
  m_NodeTreeView->setFocus();
}

mitk::DataNode* xq_DataExplorerView::GetSelectedNode()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return nullptr;

  QModelIndexList indices = m_NodeTreeView->selectionModel()->selectedIndexes();
  if (indices.isEmpty())
    return nullptr;

  return m_NodeTreeModel->GetNode(indices.first());
}

void xq_DataExplorerView::UpdateOpacitySliderForNode(mitk::DataNode* node)
{
  m_InternalSliderUpdate = true;
  if (node)
  {
    float opacity = 1.0f;
    node->GetFloatProperty("opacity", opacity);
    int sliderVal = static_cast<int>(opacity * 100.0f);
    m_OpacitySlider->setValue(sliderVal);
    m_OpacityValueLabel->setText(QString("%1%").arg(sliderVal));
  }
  else
  {
    m_OpacitySlider->setValue(100);
    m_OpacityValueLabel->setText("100%");
  }
  m_InternalSliderUpdate = false;
}

void xq_DataExplorerView::OnSelectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
  mitk::DataNode* node = GetSelectedNode();
  UpdateOpacitySliderForNode(node);

  // Update color button to reflect selected node's color
  if (node)
  {
    float rgb[3] = {1.0f, 1.0f, 1.0f};
    node->GetColor(rgb);
    QColor c;
    c.setRgbF(rgb[0], rgb[1], rgb[2]);
    QString style = QString("QPushButton { background-color: %1; border: 1px solid #555; }").arg(c.name());
    m_ColorButton->setStyleSheet(style);
  }
  else
  {
    m_ColorButton->setStyleSheet("");
  }

  // Update properties panel if visible
  if (m_PropertiesTable && m_PropertiesTable->isVisible())
  {
    UpdatePropertiesTable(node);
  }
}

void xq_DataExplorerView::OnSearchTextChanged(const QString& text)
{
  if (!m_NodeTreeModel)
    return;

  if (text.isEmpty())
  {
    m_NodeTreeView->collapseAll();
    return;
  }

  // Find matching node without expanding entire tree first
  std::function<QModelIndex(const QModelIndex&)> findMatch;
  findMatch = [&](const QModelIndex& parent) -> QModelIndex {
    int rows = m_NodeTreeModel->rowCount(parent);
    for (int r = 0; r < rows; ++r)
    {
      QModelIndex idx = m_NodeTreeModel->index(r, 0, parent);
      QString name = m_NodeTreeModel->data(idx, Qt::DisplayRole).toString();
      if (name.contains(text, Qt::CaseInsensitive))
        return idx;
      QModelIndex child = findMatch(idx);
      if (child.isValid())
        return child;
    }
    return QModelIndex();
  };

  QModelIndex match = findMatch(QModelIndex());
  if (match.isValid())
  {
    // Expand only the path to the matched node
    QModelIndex parent = match.parent();
    while (parent.isValid())
    {
      m_NodeTreeView->expand(parent);
      parent = parent.parent();
    }
    m_NodeTreeView->selectionModel()->select(match, QItemSelectionModel::ClearAndSelect);
    m_NodeTreeView->scrollTo(match);
  }
}

void xq_DataExplorerView::OpacityChanged(int value)
{
  if (m_InternalSliderUpdate)
    return;
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  m_OpacityValueLabel->setText(QString("%1%").arg(value));

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();

  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      node->SetFloatProperty("opacity", static_cast<float>(value) / 100.0f);
    }
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::ColorChanged()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();

  if (selectedIndices.isEmpty())
    return;

  mitk::DataNode* node = m_NodeTreeModel->GetNode(selectedIndices.first());
  if (!node)
    return;

  float rgb[3] = {1.0f, 1.0f, 1.0f};
  node->GetColor(rgb);

  QColor currentColor;
  currentColor.setRgbF(rgb[0], rgb[1], rgb[2]);

  QColor newColor = QColorDialog::getColor(currentColor, m_NodeTreeView, "Select Color");
  if (newColor.isValid())
  {
    // Apply to all selected nodes
    for (const QModelIndex& index : selectedIndices)
    {
      mitk::DataNode* n = m_NodeTreeModel->GetNode(index);
      if (n)
        n->SetColor(newColor.redF(), newColor.greenF(), newColor.blueF());
    }

    QString style = QString("QPushButton { background-color: %1; border: 1px solid #555; }").arg(newColor.name());
    m_ColorButton->setStyleSheet(style);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_DataExplorerView::RemoveSelectedNodes()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();

  if (selectedIndices.isEmpty())
    return;

  QMessageBox::StandardButton reply = QMessageBox::question(
    m_NodeTreeView,
    "Remove Nodes",
    QString("Remove %1 selected node(s)?").arg(selectedIndices.size()),
    QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes)
    return;

  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNull())
    return;

  QList<mitk::DataNode::Pointer> nodesToRemove;
  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      nodesToRemove.append(mitk::DataNode::Pointer(node));
    }
  }

  for (const mitk::DataNode::Pointer& node : nodesToRemove)
  {
    storage->Remove(node);
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::ToggleVisibility()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();

  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      bool isVisible = true;
      node->GetBoolProperty("visible", isVisible);
      node->SetBoolProperty("visible", !isVisible);
    }
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::GlobalReinit()
{
  mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(GetDataStorage());
}

void xq_DataExplorerView::RenameSelectedNode()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node)
    return;

  QString currentName = QString::fromStdString(node->GetName());
  bool ok = false;
  QString newName = QInputDialog::getText(
    m_NodeTreeView, "Rename Node", "New name:",
    QLineEdit::Normal, currentName, &ok);

  if (ok && !newName.isEmpty() && newName != currentName)
  {
    node->SetName(newName.toStdString());
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
}

void xq_DataExplorerView::ShowOnlySelected()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNull())
    return;

  // Collect selected nodes
  QSet<mitk::DataNode*> selectedSet;
  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();
  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
      selectedSet.insert(node);
  }

  // Hide all, then show only selected
  mitk::DataStorage::SetOfObjects::ConstPointer allNodes = storage->GetAll();
  for (auto it = allNodes->begin(); it != allNodes->end(); ++it)
  {
    (*it)->SetBoolProperty("visible", selectedSet.contains(*it));
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::SetAllNodesVisibility(bool visible)
{
  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNull())
    return;

  mitk::DataStorage::SetOfObjects::ConstPointer allNodes = storage->GetAll();
  for (auto it = allNodes->begin(); it != allNodes->end(); ++it)
  {
    (*it)->SetBoolProperty("visible", visible);
  }

  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::MakeAllVisible()
{
  SetAllNodesVisibility(true);
}

void xq_DataExplorerView::MakeAllInvisible()
{
  SetAllNodesVisibility(false);
}

void xq_DataExplorerView::ReinitializeSelectedNode()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node || !node->GetData())
    return;

  mitk::RenderingManager::GetInstance()->InitializeViews(node->GetData()->GetTimeGeometry());
}

void xq_DataExplorerView::ShowNodeInfo()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node)
    return;

  auto* dialog = new QDialog(m_NodeTreeView);
  dialog->setWindowTitle(QString("Node: %1").arg(QString::fromStdString(node->GetName())));
  dialog->resize(500, 400);

  auto* layout = new QVBoxLayout(dialog);

  auto* table = new QTableWidget(dialog);
  table->setColumnCount(2);
  table->setHorizontalHeaderLabels({"Property", "Value"});
  table->horizontalHeader()->setStretchLastSection(true);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setAlternatingRowColors(true);

  // Collect properties
  auto* propList = node->GetPropertyList();
  if (propList)
  {
    auto* map = propList->GetMap();
    if (map)
    {
      for (auto it = map->begin(); it != map->end(); ++it)
      {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(it->first)));
        if (it->second)
          table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(it->second->GetValueAsString())));
        else
          table->setItem(row, 1, new QTableWidgetItem("<no value>"));
      }
    }
  }

  // Add data type info
  if (node->GetData())
  {
    int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem("Data Class"));
    table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(node->GetData()->GetNameOfClass())));
  }

  table->sortItems(0);
  layout->addWidget(table);

  auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
  connect(btnBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
  layout->addWidget(btnBox);

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

void xq_DataExplorerView::ShowDicomInfo()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node)
    return;

  mitk::BaseData* baseData = node->GetData();
  if (!baseData)
  {
    QMessageBox::warning(m_NodeTreeView, "DICOM Information", "Selected node has no data.");
    return;
  }

  auto* image = dynamic_cast<mitk::Image*>(baseData);
  if (!image)
  {
    QMessageBox::warning(m_NodeTreeView, "DICOM Information", "Selected node is not an image.");
    return;
  }

  // Collect all rows as (tag, value) pairs for the table and export
  QList<QPair<QString, QString>> rows;

  // Basic image info
  {
    unsigned int dim = image->GetDimension();
    QString dimStr;
    for (unsigned int i = 0; i < dim; ++i)
    {
      if (i > 0) dimStr += " x ";
      dimStr += QString::number(image->GetDimension(i));
    }
    rows.append(qMakePair(QStringLiteral("Image Dimensions"), dimStr));
  }

  rows.append(qMakePair(QStringLiteral("Pixel Type"),
                         QString::fromStdString(image->GetPixelType().GetTypeAsString())));

  rows.append(qMakePair(QStringLiteral("Number of Components"),
                         QString::number(image->GetPixelType().GetNumberOfComponents())));

  if (image->GetGeometry())
  {
    auto spacing = image->GetGeometry()->GetSpacing();
    rows.append(qMakePair(QStringLiteral("Spacing"),
                           QString("%1, %2, %3").arg(spacing[0]).arg(spacing[1]).arg(spacing[2])));

    auto origin = image->GetGeometry()->GetOrigin();
    rows.append(qMakePair(QStringLiteral("Origin"),
                           QString("%1, %2, %3").arg(origin[0]).arg(origin[1]).arg(origin[2])));
  }

  // DICOM metadata from node properties
  bool hasDicomTags = false;
  auto* propList = node->GetPropertyList();
  if (propList)
  {
    auto* map = propList->GetMap();
    if (map)
    {
      for (auto it = map->begin(); it != map->end(); ++it)
      {
        QString key = QString::fromStdString(it->first);
        if (key.contains("DICOM", Qt::CaseInsensitive) ||
            key.contains("patient", Qt::CaseInsensitive) ||
            key.contains("study", Qt::CaseInsensitive) ||
            key.contains("series", Qt::CaseInsensitive) ||
            key.contains("modality", Qt::CaseInsensitive))
        {
          hasDicomTags = true;
          QString val = it->second
                            ? QString::fromStdString(it->second->GetValueAsString())
                            : QStringLiteral("<no value>");
          rows.append(qMakePair(key, val));
        }
      }
    }
  }

  if (!hasDicomTags)
    rows.append(qMakePair(QStringLiteral("DICOM Metadata"),
                           QStringLiteral("No DICOM metadata available for this image.")));

  // Build dialog
  auto* dialog = new QDialog(m_NodeTreeView);
  dialog->setWindowTitle("DICOM Information");
  dialog->setMinimumSize(600, 500);

  auto* layout = new QVBoxLayout(dialog);

  auto* table = new QTableWidget(rows.size(), 2, dialog);
  table->setHorizontalHeaderLabels({"Tag", "Value"});
  table->horizontalHeader()->setStretchLastSection(true);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->setVisible(false);

  for (int i = 0; i < rows.size(); ++i)
  {
    table->setItem(i, 0, new QTableWidgetItem(rows[i].first));
    table->setItem(i, 1, new QTableWidgetItem(rows[i].second));
  }
  table->resizeColumnsToContents();
  layout->addWidget(table);

  // Buttons
  auto* btnLayout = new QHBoxLayout;

  auto* copyBtn = new QPushButton("Copy to Clipboard", dialog);
  connect(copyBtn, &QPushButton::clicked, dialog, [rows]() {
    QString text;
    for (const auto& r : rows)
      text += r.first + "\t" + r.second + "\n";
    QApplication::clipboard()->setText(text);
  });
  btnLayout->addWidget(copyBtn);

  auto* csvBtn = new QPushButton("Export to CSV", dialog);
  connect(csvBtn, &QPushButton::clicked, dialog, [rows, dialog]() {
    QString path = QFileDialog::getSaveFileName(dialog, "Export DICOM Info",
                                                QString(), "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "Tag,Value\n";
    for (const auto& r : rows)
    {
      QString tag = r.first;
      QString val = r.second;
      tag.replace("\"", "\"\"");
      val.replace("\"", "\"\"");
      out << "\"" << tag << "\",\"" << val << "\"\n";
    }
  });
  btnLayout->addWidget(csvBtn);

  btnLayout->addStretch();

  auto* closeBtn = new QPushButton("Close", dialog);
  connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
  btnLayout->addWidget(closeBtn);

  layout->addLayout(btnLayout);

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

void xq_DataExplorerView::NodeChanged(const mitk::DataNode* /*node*/)
{
  // Debounce: coalesce rapid property changes into a single render update
  if (m_RenderDebounceTimer)
    m_RenderDebounceTimer->start();
}

void xq_DataExplorerView::SetRepresentationSurface()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();
  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      node->SetBoolProperty("volumerendering", false);
      node->SetProperty("material.representation",
                        mitk::IntProperty::New(2)); // VTK_SURFACE
      node->SetBoolProperty("material.wireframe", false);
    }
  }
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::SetRepresentationWireframe()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();
  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      node->SetProperty("material.representation",
                        mitk::IntProperty::New(1)); // VTK_WIREFRAME
      node->SetBoolProperty("material.wireframe", true);
    }
  }
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::SetRepresentationPoints()
{
  if (!m_NodeTreeModel || !m_NodeTreeView->selectionModel())
    return;

  QModelIndexList selectedIndices = m_NodeTreeView->selectionModel()->selectedIndexes();
  for (const QModelIndex& index : selectedIndices)
  {
    mitk::DataNode* node = m_NodeTreeModel->GetNode(index);
    if (node)
    {
      node->SetProperty("material.representation",
                        mitk::IntProperty::New(0)); // VTK_POINTS
      node->SetBoolProperty("material.wireframe", false);
    }
  }
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::ExportSelectedNode()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node || !node->GetData())
  {
    QMessageBox::information(m_NodeTreeView, "Export", "No data node selected.");
    return;
  }

  QString nodeName = QString::fromStdString(node->GetName());
  QString className = QString::fromStdString(node->GetData()->GetNameOfClass());

  // Build filter string based on data type
  QString filter;
  if (dynamic_cast<mitk::Surface*>(node->GetData()))
    filter = "STL Files (*.stl);;VTP Files (*.vtp);;VTK Files (*.vtk);;All Files (*)";
  else if (dynamic_cast<mitk::Image*>(node->GetData()))
    filter = "NRRD Files (*.nrrd);;NIfTI Files (*.nii *.nii.gz);;All Files (*)";
  else if (dynamic_cast<mitk::PointSet*>(node->GetData()))
    filter = "MPS Files (*.mps);;All Files (*)";
  else
    filter = "All Files (*)";

  QString fileName = QFileDialog::getSaveFileName(
    m_NodeTreeView,
    QString("Export '%1' (%2)").arg(nodeName, className),
    nodeName,
    filter);

  if (fileName.isEmpty())
    return;

  try
  {
    mitk::IOUtil::Save(node->GetData(), fileName.toStdString());
    QMessageBox::information(m_NodeTreeView, "Export",
                             QString("Successfully exported '%1' to:\n%2").arg(nodeName, fileName));
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(m_NodeTreeView, "Export Failed",
                         QString("Failed to export '%1':\n%2").arg(nodeName, QString::fromStdString(e.GetDescription())));
  }
  catch (const std::exception& e)
  {
    QMessageBox::warning(m_NodeTreeView, "Export Failed",
                         QString("Failed to export '%1':\n%2").arg(nodeName, e.what()));
  }
}

void xq_DataExplorerView::AddImageToSelectedFolder()
{
  mitk::DataNode* node = GetSelectedNode();
  if (!node || !node->GetData())
  {
    QMessageBox::information(m_NodeTreeView, "Add Image", "No image folder selected.");
    return;
  }

  QString className = QString::fromStdString(node->GetData()->GetNameOfClass());
  if (className != QStringLiteral("xq_ImageFolder"))
  {
    QMessageBox::information(m_NodeTreeView, "Add Image", "Please select the Images folder.");
    return;
  }

  QString filePath = QFileDialog::getOpenFileName(
    m_NodeTreeView,
    "Select Image File",
    QString(),
    "Image Files (*.nii *.nii.gz *.nrrd *.dcm *.vti);;All Files (*)");

  if (filePath.isEmpty())
    return;

  try
  {
    QList<mitk::BaseData::Pointer> loadedData = QmitkIOUtil::Load(QStringList() << filePath, nullptr);
    if (loadedData.isEmpty())
    {
      QMessageBox::warning(m_NodeTreeView, "Load Error", "Failed to load the selected image file.");
      return;
    }

    mitk::DataNode::Pointer newNode = mitk::DataNode::New();
    newNode->SetData(loadedData[0]);
    QFileInfo fileInfo(filePath);
    newNode->SetName(fileInfo.baseName().toStdString());

    GetDataStorage()->Add(newNode, node);
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(m_NodeTreeView, "Load Error",
      QString("Failed to load image: %1").arg(e.GetDescription()));
  }
}

void xq_DataExplorerView::TogglePropertiesPanel()
{
  bool show = m_PropertiesToggle->isChecked();
  m_PropertiesTable->setVisible(show);
  m_PropertiesToggle->setText(show ? QString::fromUtf8("\u25BC Properties") : QString::fromUtf8("\u25B6 Properties"));

  if (show)
  {
    UpdatePropertiesTable(GetSelectedNode());
  }
}

void xq_DataExplorerView::DeferredRenderUpdate()
{
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_DataExplorerView::UpdatePropertiesTable(mitk::DataNode* node)
{
  if (!m_PropertiesTable)
    return;

  m_PropertiesTable->setRowCount(0);

  if (!node)
    return;

  // Add name
  int row = 0;
  m_PropertiesTable->insertRow(row);
  m_PropertiesTable->setItem(row, 0, new QTableWidgetItem("Name"));
  m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(node->GetName())));
  ++row;

  // Add data class
  if (node->GetData())
  {
    m_PropertiesTable->insertRow(row);
    m_PropertiesTable->setItem(row, 0, new QTableWidgetItem("Data Type"));
    m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(
        QString::fromStdString(node->GetData()->GetNameOfClass())));
    ++row;
  }

  // Add visibility
  bool visible = true;
  node->GetBoolProperty("visible", visible);
  m_PropertiesTable->insertRow(row);
  m_PropertiesTable->setItem(row, 0, new QTableWidgetItem("Visible"));
  m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(visible ? "true" : "false"));
  ++row;

  // Add opacity
  float opacity = 1.0f;
  node->GetFloatProperty("opacity", opacity);
  m_PropertiesTable->insertRow(row);
  m_PropertiesTable->setItem(row, 0, new QTableWidgetItem("Opacity"));
  m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(opacity, 'f', 2)));
  ++row;

  // Add color
  float rgb[3] = {1.0f, 1.0f, 1.0f};
  node->GetColor(rgb);
  m_PropertiesTable->insertRow(row);
  m_PropertiesTable->setItem(row, 0, new QTableWidgetItem("Color"));
  m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(
      QString("(%1, %2, %3)").arg(QString::number(rgb[0], 'f', 2),
                                   QString::number(rgb[1], 'f', 2),
                                   QString::number(rgb[2], 'f', 2))));
  ++row;

  // Add additional properties from property list (key ones)
  auto* propList = node->GetPropertyList();
  if (propList)
  {
    auto* map = propList->GetMap();
    if (map)
    {
      for (auto it = map->begin(); it != map->end(); ++it)
      {
        // Skip already-shown and internal properties
        if (it->first == "name" || it->first == "visible" || it->first == "opacity" || it->first == "color")
          continue;
        // Show key properties for the panel, including XQ pipeline/source
        // metadata used by Data Notes double-click tool routing.
        if (it->first.rfind("xq.", 0) == 0 ||
            it->first == "binary" || it->first == "volumerendering" ||
            it->first == "material.representation" || it->first == "layer" ||
            it->first == "show contour" || it->first == "levelwindow" ||
            it->first.find("DICOM") != std::string::npos ||
            it->first.find("dicom") != std::string::npos)
        {
          m_PropertiesTable->insertRow(row);
          m_PropertiesTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(it->first)));
          if (it->second)
            m_PropertiesTable->setItem(row, 1, new QTableWidgetItem(
                QString::fromStdString(it->second->GetValueAsString())));
          else
            m_PropertiesTable->setItem(row, 1, new QTableWidgetItem("<null>"));
          ++row;
        }
      }
    }
  }

  m_PropertiesTable->resizeColumnsToContents();
}

void xq_DataExplorerView::OnDataNotesDoubleClicked(int row, int column)
{
  Q_UNUSED(column);
  if (!m_PropertiesTable || row < 0)
    return;

  mitk::DataNode* selectedNode = GetSelectedNode();

  // Prefer the row that was actually double-clicked. If it doesn't name a
  // tool-backed node, fall back to the current Data Manager selection.
  mitk::DataNode* target = ResolveDataNotesTargetNode(row);
  if (!target)
    target = selectedNode && !xq::pipeline::ResolveToolViewIdForNode(selectedNode).isEmpty()
                 ? selectedNode
                 : nullptr;

  OpenToolForNode(target);
}

void xq_DataExplorerView::OnDataTreeDoubleClicked(const QModelIndex& index)
{
  if (!index.isValid() || !m_NodeTreeModel)
    return;

  OpenToolForNode(m_NodeTreeModel->GetNode(index));
}

void xq_DataExplorerView::OpenToolForNode(mitk::DataNode* node)
{
  QString viewId = xq::pipeline::ResolveToolViewIdForNode(node);
  if (viewId.isEmpty())
  {
    const QString nodeName = node
      ? QString::fromStdString(node->GetName())
      : QStringLiteral("<null>");
    const QString message =
      QStringLiteral("No XQ tool is registered for Data Manager node '%1'.")
        .arg(nodeName);
    MITK_WARN << message.toStdString();
    mitk::StatusBar::GetInstance()->DisplayText(message.toStdString().c_str());
    return;
  }

  berry::IWorkbenchPage::Pointer page = GetSite()->GetPage();
  if (page.IsNull())
    return;

  try
  {
    SelectNodeInTree(node);
    page->ShowView(viewId);
  }
  catch (...)
  {
    const QString message =
      QStringLiteral("Failed to open XQ tool view '%1'.").arg(viewId);
    MITK_WARN << message.toStdString();
    mitk::StatusBar::GetInstance()->DisplayText(message.toStdString().c_str());
  }
}

void xq_DataExplorerView::SelectNodeInTree(mitk::DataNode* node)
{
  if (!node || !m_NodeTreeModel || !m_NodeTreeView ||
      !m_NodeTreeView->selectionModel())
    return;

  const QModelIndex index = m_NodeTreeModel->GetIndex(node);
  if (!index.isValid())
    return;

  m_NodeTreeView->selectionModel()->select(
    index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  m_NodeTreeView->setCurrentIndex(index);
  m_NodeTreeView->scrollTo(index);
}

mitk::DataNode* xq_DataExplorerView::ResolveDataNotesTargetNode(int row)
{
  if (!m_PropertiesTable || row < 0 || row >= m_PropertiesTable->rowCount())
    return nullptr;

  QString keyText, valueText;
  auto* keyItem = m_PropertiesTable->item(row, 0);
  if (keyItem) keyText = keyItem->text().trimmed();
  auto* valItem = m_PropertiesTable->item(row, 1);
  if (valItem) valueText = valItem->text().trimmed();

  mitk::DataStorage::Pointer storage = GetDataStorage();
  if (storage.IsNull())
    return nullptr;

  // Candidates: valueText first (e.g. "aorta" in col 1), then keyText
  QStringList candidates;
  AddDataNotesNodeNameCandidates(candidates, valueText);
  AddDataNotesNodeNameCandidates(candidates, keyText);

  for (const QString& candidate : candidates)
  {
    // Skip generic/placeholder values that cannot be node names
    if (candidate == QStringLiteral("true") ||
        candidate == QStringLiteral("false") ||
        candidate == QStringLiteral("<null>") ||
        candidate == QStringLiteral("<no value>") ||
        candidate == QStringLiteral("Name"))
      continue;

    // Skip numeric-only strings (e.g. opacity values like "1.00")
    bool isNumeric = false;
    candidate.toDouble(&isNumeric);
    if (isNumeric)
      continue;

    // Traverse DataStorage for a node whose name matches
    auto allNodes = storage->GetAll();
    for (auto it = allNodes->begin(); it != allNodes->end(); ++it)
    {
      mitk::DataNode* n = it->GetPointer();
      if (!n) continue;
      if (QString::fromStdString(n->GetName()) == candidate &&
          !xq::pipeline::ResolveToolViewIdForNode(n).isEmpty())
        return n;
    }
  }

  return nullptr;
}

#include "xq_DataExplorerView.moc"
