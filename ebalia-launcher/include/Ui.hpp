#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include <functional>
class QBoxLayout;
class QDialog;
class QWidget;
// Shared look of the launcher: Lucide icons (resources/icons/ui, ISC license), the dark theme and window helpers.
namespace Ui {
// A row that becomes a column before its controls need to squeeze together.
class ResponsiveRow:public QWidget {
public:
    explicit ResponsiveRow(int breakpoint=740,QWidget *parent=nullptr);
    QBoxLayout *box() const {return m_box;}
    QSize minimumSizeHint() const override;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QBoxLayout *m_box;int m_breakpoint;
};
QPixmap pixmap(const QString &name,int size,const QColor &color=QColor(232,232,236));
QIcon icon(const QString &name,const QColor &color=QColor(232,232,236));
QIcon navigationIcon(const QString &name);
QString styleSheet();
// Landscape art cropped to size, with rounded corners when radius > 0.
QPixmap art(const QString &resource,QSize size,int radius=0);
// Decode and resize artwork on a bounded worker pool. Callbacks run on the UI thread
// only while the receiver is alive; identical requests share one job and a memory cache.
void loadArt(const QString &resource,QSize bounds,QObject *receiver,std::function<void(const QPixmap &)> ready);
QPixmap cachedArt(const QString &resource,QSize bounds);
// Any picture cropped to fill size (centered horizontally, top kept), with rounded corners when radius > 0.
QPixmap cover(const QPixmap &source,QSize size,int radius=0);
// Round avatar with the player's initial; an empty name gives the generic user icon.
QPixmap avatar(const QString &name,int size);
QString artFor(const QString &key);
QString lostArtFor(const QString &id,const QString &category);
// Opens a dialog as an ordinary window and waits for it. Window managers such as GNOME attach modal
// dialogs to their parent (both move together and the dialog cannot be resized), so the dialog is not
// window-modal; the window behind it is disabled instead.
int openWindow(QDialog &dialog);
int openWindows();
void fitToScreen(QWidget *window,QSize preferred);
}
