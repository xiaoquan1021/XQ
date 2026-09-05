#ifndef XQ_PATHCREATE_H
#define XQ_PATHCREATE_H

#include <QDialog>

namespace Ui {
class xq_PathCreate;
}

class xq_PathCreate : public QDialog
{
  Q_OBJECT

public:
  explicit xq_PathCreate(QWidget* parent = nullptr);
  ~xq_PathCreate() override;

  QString GetPathName() const;
  int GetSubdivisionNumber() const;
  int GetCalculationNumber() const;

private:
  Ui::xq_PathCreate* m_Ui;
};

#endif // XQ_PATHCREATE_H
