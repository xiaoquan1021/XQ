#ifndef XQ_GRIDCREATE_H
#define XQ_GRIDCREATE_H

#include <QDialog>
#include <mitkDataStorage.h>

namespace Ui {
class xq_MeshCreate;
}

class xq_GridCreate : public QDialog
{
  Q_OBJECT

public:
  explicit xq_GridCreate(mitk::DataStorage::Pointer dataStorage,
                         QWidget* parent = nullptr);
  ~xq_GridCreate() override;

  QString GetMeshName() const;
  QString GetMeshType() const;
  QString GetSelectedModelName() const;
  void SelectModel(const QString& modelName);

private:
  Ui::xq_MeshCreate* m_Ui;
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_GRIDCREATE_H
