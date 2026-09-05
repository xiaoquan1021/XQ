#include "app/XQMainWindow.h"

#include "core/NodeId.h"
#include "core/XQDataNode.h"
#include "core/XQProject.h"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    xq::XQProject project;
    project.open();
    project.scene().insert(xq::XQDataNode(xq::NodeId(1), "volume", "Demo Volume"));
    project.scene().insert(xq::XQDataNode(xq::NodeId(2), "path", "Demo Centerline"));
    project.scene().insert(xq::XQDataNode(xq::NodeId(3), "mesh", "Demo Surface"));

    xq::XQMainWindow window(&project.scene());
    window.show();

    return app.exec();
}
