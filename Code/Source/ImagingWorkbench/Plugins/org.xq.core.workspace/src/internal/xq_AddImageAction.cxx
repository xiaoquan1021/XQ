#include "xq_AddImageAction.h"

#include <mitkNodePredicateDataType.h>
#include <QmitkIOUtil.h>

#include <QFileDialog>
#include <QMessageBox>

xq_AddImageAction::xq_AddImageAction()
{
}

xq_AddImageAction::~xq_AddImageAction()
{
}

void xq_AddImageAction::SetDataStorage(mitk::DataStorage* dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_AddImageAction::Run(const QList<mitk::DataNode::Pointer>& selectedNodes)
{
  if (selectedNodes.isEmpty())
  {
    return;
  }

  mitk::DataNode::Pointer folderNode = selectedNodes[0];

  QString filePath = QFileDialog::getOpenFileName(
    nullptr,
    "Select Image File",
    QString(),
    "Image Files (*.nii *.nii.gz *.nrrd *.dcm *.vti);;All Files (*)");

  if (filePath.isEmpty())
  {
    return;
  }

  try
  {
    QStringList fileList;
    fileList << filePath;

    QList<mitk::BaseData::Pointer> loadedData = QmitkIOUtil::Load(fileList, nullptr);

    if (loadedData.isEmpty())
    {
      QMessageBox::warning(nullptr, "Load Error", "Failed to load the selected image file.");
      return;
    }

    mitk::DataNode::Pointer newNode = mitk::DataNode::New();
    newNode->SetData(loadedData[0]);

    QFileInfo fileInfo(filePath);
    newNode->SetName(fileInfo.baseName().toStdString());

    if (m_DataStorage.IsNotNull())
    {
      m_DataStorage->Add(newNode, folderNode);
    }
  }
  catch (const mitk::Exception& e)
  {
    QMessageBox::warning(nullptr, "Load Error",
      QString("Failed to load image: %1").arg(e.GetDescription()));
  }
}
