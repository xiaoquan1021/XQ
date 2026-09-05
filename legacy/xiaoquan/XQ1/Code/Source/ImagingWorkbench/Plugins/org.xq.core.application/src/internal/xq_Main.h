#ifndef XQ_MAIN_H
#define XQ_MAIN_H

#include <QObject>

class xq_Main : public QObject
{
    Q_OBJECT

public:
    static int Launch(int argc, char* argv[],
                      bool useProvisioningFile = true,
                      bool useWorkbench = true);
};

#endif // XQ_MAIN_H
