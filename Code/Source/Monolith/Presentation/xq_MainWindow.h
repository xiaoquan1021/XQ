#ifndef XQ_MAINWINDOW_H
#define XQ_MAINWINDOW_H

#include <QMainWindow>

class QListWidget;
class QmitkStdMultiWidget;
class QStackedWidget;
class QTextEdit;

namespace xq::core
{
class ApplicationContext;
}

namespace xq::presentation
{

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(xq::core::ApplicationContext& context,
                        QWidget* parent = nullptr);

private:
    QWidget* CreateWorkflowPage(const QString& title);
    void AddWorkflowPage(const QString& title);

    xq::core::ApplicationContext& m_Context;
    QListWidget* m_Navigation = nullptr;
    QStackedWidget* m_Pages = nullptr;
    QmitkStdMultiWidget* m_RenderHost = nullptr;
    QTextEdit* m_Diagnostics = nullptr;
};

} // namespace xq::presentation

#endif // XQ_MAINWINDOW_H
