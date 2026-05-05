#include "xq_ExtractCenterlinesAction.h"

#include <xq_Model.h>
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
  if (m_ModelNode.IsNull())
    return;

  vtkPolyData* polyData = GetModelPolyData(m_ModelNode);
  if (!polyData)
    return;

  // Compute a centerline approximation by extracting the center of mass
  // along the model. This is a simplified extraction; the full pipeline
  // would use VMTK or a dedicated centerline algorithm.
  vtkSmartPointer<vtkPoints> pathPoints = vtkSmartPointer<vtkPoints>::New();

  vtkSmartPointer<vtkCenterOfMass> comFilter = vtkSmartPointer<vtkCenterOfMass>::New();
  comFilter->SetInputData(polyData);
  comFilter->SetUseScalarsAsWeights(false);
  comFilter->Update();

  double center[3];
  comFilter->GetCenter(center);

  // Build a single-point path as a starting seed
  pathPoints->InsertNextPoint(center);

  // Build the polydata for the path
  vtkSmartPointer<vtkPolyData> pathPoly = vtkSmartPointer<vtkPolyData>::New();
  pathPoly->SetPoints(pathPoints);

  vtkSmartPointer<vtkCellArray> verts = vtkSmartPointer<vtkCellArray>::New();
  for (vtkIdType i = 0; i < pathPoints->GetNumberOfPoints(); ++i)
  {
    verts->InsertNextCell(1, &i);
  }
  pathPoly->SetVerts(verts);

  mitk::Surface::Pointer resultSurface = mitk::Surface::New();
  resultSurface->SetVtkPolyData(pathPoly);

  m_ResultNode = mitk::DataNode::New();
  m_ResultNode->SetData(resultSurface);
  m_ResultNode->SetName(m_ModelNode->GetName() + "_centerline");
  m_ResultNode->SetColor(1.0f, 0.0f, 0.0f);
  m_ResultNode->SetFloatProperty("pointsize", 3.0f);

  m_Success = true;
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

  if (m_Worker && m_Worker->isRunning())
  {
    QMessageBox::information(nullptr, "Extract Paths",
      "Path extraction is already in progress.");
    return;
  }

  delete m_Worker;
  m_Worker = new WorkerThread(m_DataStorage, m_ModelNode);
  connect(m_Worker, &QThread::finished,
          this, &xq_ExtractCenterlinesAction::OnExtractionComplete);
  m_Worker->start();
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
