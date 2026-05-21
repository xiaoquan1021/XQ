#ifndef XQ_MITK_APP_H
#define XQ_MITK_APP_H

#include <mitkBaseApplication.h>
#include <QObject>

class xq_MitkApp : public QObject, public mitk::BaseApplication
{
    Q_OBJECT

public:
    xq_MitkApp(int argc, char** argv);
    ~xq_MitkApp();

protected:
    void initializeLibraryPaths() override;
};

#endif // XQ_MITK_APP_H
