#ifndef XQ_APP_ABOUT_DIALOG_H
#define XQ_APP_ABOUT_DIALOG_H
#include <QDialog>
namespace xq {
// Modal "About XQ" dialog: product name, one-line description, the component
// versions the app is built against, and a close button. Pure Qt Widgets.
class XQAboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit XQAboutDialog(QWidget* parent = nullptr);
};
} // namespace xq
#endif
