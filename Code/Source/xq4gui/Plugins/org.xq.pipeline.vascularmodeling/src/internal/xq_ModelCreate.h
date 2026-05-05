#ifndef XQ_MODELCREATE_H
#define XQ_MODELCREATE_H

#include <QDialog>
#include <mitkDataStorage.h>

namespace Ui {
class xq_ModelCreate;
}

class xq_ModelCreate : public QDialog
{
  Q_OBJECT

public:
  explicit xq_ModelCreate(mitk::DataStorage::Pointer dataStorage,
                          QWidget* parent = nullptr);
  ~xq_ModelCreate() override;

  QString GetModelName() const;
  QString GetModelType() const;
  int GetNumSamplingPoints() const;

private slots:
  void OnModelTypeChanged(int index);

private:
  Ui::xq_ModelCreate* m_Ui;
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_MODELCREATE_H
