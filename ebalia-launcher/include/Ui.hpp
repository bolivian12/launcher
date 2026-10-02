#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>
class QDialog;
class QWidget;
// Shared look of the launcher: Lucide icons (resources/icons/ui, ISC license), the dark theme and window helpers.
namespace Ui {
QPixmap pixmap(const QString &name,int size,const QColor &color=QColor(232,232,236));
QIcon icon(const QString &name,const QColor &color=QColor(232,232,236));
QString styleSheet();
// Landscape art cropped to size, with rounded corners when radius > 0.
QPixmap art(const QString &resource,QSize size,int radius=0);
// Any picture cropped to fill size (centered horizontally, top kept), with rounded corners when radius > 0.
QPixmap cover(const QPixmap &source,QSize size,int radius=0);
// Round avatar with the player's initial; an empty name gives the generic user icon.
QPixmap avatar(const QString &name,int size);
QString artFor(const QString &key);
// Opens a dialog as an ordinary window and waits for it. Window managers such as GNOME attach modal
// dialogs to their parent (both move together and the dialog cannot be resized), so the dialog is not
// window-modal; the window behind it is disabled instead.
int openWindow(QDialog &dialog);
int openWindows();
void fitToScreen(QWidget *window,QSize preferred);
}
