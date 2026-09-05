#ifndef XQ_CONTOURGROUPCREATE_H
#define XQ_CONTOURGROUPCREATE_H

#include <QDialog>
#include <mitkDataStorage.h>

namespace Ui {
class xq_ProfileGroupCreate;
}

class xq_ProfileGroupCreate : public QDialog
{
  Q_OBJECT

public:
  explicit xq_ProfileGroupCreate(mitk::DataStorage::Pointer dataStorage,
                                 QWidget* parent = nullptr);
  ~xq_ProfileGroupCreate() override;

  QString GetGroupName() const;
  QString GetSelectedPathName() const;
  void SelectPath(const QString& pathName);

private:
  Ui::xq_ProfileGroupCreate* m_Ui;
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_CONTOURGROUPCREATE_H
