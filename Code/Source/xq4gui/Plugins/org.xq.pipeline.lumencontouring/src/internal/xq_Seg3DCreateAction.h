#ifndef XQ_SEG3DCREATEACTION_H
#define XQ_SEG3DCREATEACTION_H

#include <QAction>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>

class QDoubleSpinBox;
class QComboBox;
class QDialog;

class xq_Seg3DCreateAction : public QAction
{
  Q_OBJECT

public:
  explicit xq_Seg3DCreateAction(mitk::DataStorage::Pointer dataStorage,
                                QObject* parent = nullptr);
  ~xq_Seg3DCreateAction() override;

  void SetDataStorage(mitk::DataStorage::Pointer dataStorage);

public slots:
  void Execute();

private:
  void PerformThresholdSegmentation(mitk::DataNode::Pointer imageNode,
                                    double minVal, double maxVal);
  void PerformRegionGrowing(mitk::DataNode::Pointer imageNode,
                            double minVal, double maxVal);

  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_SEG3DCREATEACTION_H
