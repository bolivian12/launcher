#pragma once

#include <QDialog>
#include <QList>
#include "McInstanceManager.hpp"

class QLineEdit;
class QTreeWidget;
class QCheckBox;
class QLabel;
class NiceButton;

// Prism-style "New instance" dialog: icon + Nombre/Grupo on top, source list
// left (Personalizado / Importar / Modrinth / CurseForge / ATLauncher / FTB /
// Technic), 3-column version table with type filters + search, loader radios.
class NewInstanceDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewInstanceDialog(const QList<McVersion> &versions, QWidget *parent = nullptr);

    QString instanceName() const;
    QString mcVersion() const;
    QString loader() const;          // vanilla | fabric | quilt | forge | neoforge | liteloader
    QString loaderVersion() const;   // chosen loader build (empty = latest)
    QString importZipPath() const;   // set when importing a modpack (.zip/.mrpack)

private slots:
    void refillVersions();
    void onSourceRow(int row);
    void pickImportZip();
    void pickOnline();
    void fetchLoaderVersions();

private:
    QList<McVersion> m_versions;
    QLineEdit *m_nameEdit;
    QTreeWidget *m_versionList;
    QLineEdit *m_search;
    QCheckBox *m_fReleases;
    QCheckBox *m_fSnapshots;
    QCheckBox *m_fBetas;
    QCheckBox *m_fAlphas;
    QLabel *m_importLabel;
    NiceButton *m_zipBtn;
    NiceButton *m_browseBtn;
    QLabel *m_loaderVerLabel;
    QTreeWidget *m_loaderList;
    QString m_importZip;
    QString m_selectedLoader = QStringLiteral("vanilla");
    QString m_loaderVersion;
};
