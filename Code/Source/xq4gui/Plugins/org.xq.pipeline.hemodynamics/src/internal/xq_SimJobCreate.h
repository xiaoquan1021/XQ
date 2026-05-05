#ifndef XQ_SIMJOBCREATE_H
#define XQ_SIMJOBCREATE_H

#include <QDialog>
#include <mitkDataStorage.h>

namespace Ui {
class xq_SimJobCreate;
}

class xq_SimJobCreate : public QDialog
{
  Q_OBJECT

public:
  explicit xq_SimJobCreate(mitk::DataStorage::Pointer dataStorage,
                           QWidget* parent = nullptr);
  ~xq_SimJobCreate() override;

  QString GetJobName() const;
  QString GetSelectedMesh() const;

private:
  Ui::xq_SimJobCreate* m_Ui;
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_SIMJOBCREATE_H
