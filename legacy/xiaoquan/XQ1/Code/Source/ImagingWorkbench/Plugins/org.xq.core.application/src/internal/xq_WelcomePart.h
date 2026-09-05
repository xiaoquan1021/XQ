#ifndef XQ_WELCOME_PART_H
#define XQ_WELCOME_PART_H

#include <berryQtIntroPart.h>

class xq_WelcomePart : public berry::QtIntroPart
{
    Q_OBJECT

public:
    xq_WelcomePart();
    ~xq_WelcomePart() override;

    void CreateQtPartControl(QWidget* parent) override;
    void StandbyStateChanged(bool standby) override;
    void SetFocus() override;

private:
    QWidget* m_Control;
};

#endif
