#ifndef XQ_APPLICATION_H
#define XQ_APPLICATION_H

#include <berryIApplication.h>

class xq_Application : public QObject, public berry::IApplication
{
    Q_OBJECT
    Q_INTERFACES(berry::IApplication)

public:
    xq_Application();

    QVariant Start(berry::IApplicationContext* context) override;
    void Stop() override;
};

#endif // XQ_APPLICATION_H
