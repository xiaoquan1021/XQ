#include "xq_ExtractCenterlinesAction.h"

#include <xq_Model.h>
#include <xq_PipelineDataUtils.h>
#include <xq_VascularGeometry.h>

#include <mitkSurface.h>
#include <mitkRenderingManager.h>

#include <vtkPolyData.h>
#include <vtkCenterOfMass.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkSmartPointer.h>

#include <QMessageBox>

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

// --- WorkerThread ---

xq_ExtractCenterlinesAction::WorkerThread::WorkerThread(
  mitk::DataStorage::Pointer ds,
  mitk::DataNode::Pointer node)
  : m_DataStorage(ds)
  , m_ModelNode(node)
  , m_ResultNode(nullptr)
  , m_Success(false)
{
}

void xq_ExtractCenterlinesAction::WorkerThread::run()
{
  // XQ-native full model-to-centerline extraction is not implemented.
  // Do not create a single-point seed and label it as a centerline output.
  m_ResultNode = nullptr;
  m_Success = false;
}

// --- xq_ExtractCenterlinesAction ---

xq_ExtractCenterlinesAction::xq_ExtractCenterlinesAction(QObject* parent)
  : QAction("Extract Centerline Paths", parent)
  , m_DataStorage(nullptr)
  , m_ModelNode(nullptr)
  , m_Worker(nullptr)
{
  connect(this, &QAction::triggered, this, &xq_ExtractCenterlinesAction::Execute);
}

xq_ExtractCenterlinesAction::~xq_ExtractCenterlinesAction()
{
  if (m_Worker)
  {
    m_Worker->wait();
    delete m_Worker;
  }
}

void xq_ExtractCenterlinesAction::SetDataStorage(
  mitk::DataStorage::Pointer dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_ExtractCenterlinesAction::SetModelNode(
  mitk::DataNode::Pointer modelNode)
{
  m_ModelNode = modelNode;
}

void xq_ExtractCenterlinesAction::Execute()
{
  if (m_DataStorage.IsNull() || m_ModelNode.IsNull())
  {
    QMessageBox::warning(nullptr, "Extract Paths",
      "No model selected for path extraction.");
    return;
  }

  QMessageBox::information(nullptr, "Extract Paths",
    "XQ-native full centerline extraction from models is not available yet. "
    "No centerline node was created; use the existing Vessel Planning tools "
    "for native path creation.");
  emit ExtractionFinished(false);
}

void xq_ExtractCenterlinesAction::OnExtractionComplete()
{
  if (!m_Worker)
    return;

  if (m_Worker->WasSuccessful())
  {
    mitk::DataNode::Pointer resultNode = m_Worker->GetResultNode();
    if (resultNode.IsNotNull() && m_DataStorage.IsNotNull())
    {
      m_DataStorage->Add(resultNode, m_ModelNode);
      mitk::RenderingManager::GetInstance()->RequestUpdateAll();
    }
    emit ExtractionFinished(true);
  }
  else
  {
    QMessageBox::warning(nullptr, "Extract Paths",
      "Centerline extraction failed.");
    emit ExtractionFinished(false);
  }
}
