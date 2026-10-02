#pragma once

#include <QDialog>
#include <QString>
#include <QJsonObject>
#include "McInstanceManager.hpp"

class QTabWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPlainTextEdit;

// Prism-style instance editor, EBALIA dark skin: overview (play, duplicate,
// export, kill, change MC version), content tabs (mods with Modrinth
// download / resource packs / shader packs with enable toggles), worlds,
// screenshots, notes, logs, and per-instance settings (rename, RAM, Java).
class EditInstanceDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditInstanceDialog(const McInstance &inst, McInstanceManager *mgr,
                                const QList<McVersion> &versions,
                                QWidget *parent = nullptr);
    ~EditInstanceDialog() override;

signals:
    void playRequested(const QString &dir);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void applySettings();

private:
    QWidget *buildOverviewTab();
    QWidget *buildContentTab(const QString &subDir, const QString &addFilter,
                             bool modsTab);
    QWidget *buildWorldsTab();
    QWidget *buildScreenshotsTab();
    QWidget *buildNotesTab();
    QWidget *buildLogsTab();
    QWidget *buildSettingsTab();
    void reloadContentList(QListWidget *list, const QString &absDir);
    void onContentItemChanged(QListWidget *list, const QString &absDir,
                              QListWidgetItem *item);
    void duplicateInstance();
    void exportInstance();
    void saveNotes();
    QJsonObject readInstanceJson() const;
    void writeInstanceJson(const QJsonObject &o);

    QString m_dir;
    McInstance m_inst;
    McInstanceManager *m_mgr = nullptr;
    QList<McVersion> m_versions;
    QLineEdit *m_nameEdit = nullptr;
    QSpinBox *m_xmxSpin = nullptr;
    QComboBox *m_javaCombo = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QLabel *m_settingsStatus = nullptr;
};
