#include "EditInstanceDialog.hpp"
#include "NiceButton.hpp"
#include "JavaRunner.hpp"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QUrl>
#include <QProcess>
#include <QStyle>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonArray>
#include <QMouseEvent>

// Recursive directory size in bytes
static qint64 dirSizeBytes(const QString &path)
{
    qint64 total = 0;
    const QDir d(path);
    for (const QFileInfo &e : d.entryInfoList(QDir::Dirs | QDir::Files |
                                              QDir::NoDotAndDotDot | QDir::Hidden)) {
        total += e.isDir() ? dirSizeBytes(e.absoluteFilePath()) : e.size();
    }
    return total;
}

// Recursive copy (fresh destination)
static bool copyRec(const QString &src, const QString &dst)
{
    const QDir from(src);
    if (!from.exists())
        return false;
    QDir().mkpath(dst);
    for (const QFileInfo &e : from.entryInfoList(QDir::Dirs | QDir::Files |
                                                 QDir::NoDotAndDotDot | QDir::Hidden)) {
        const QString target = dst + QLatin1Char('/') + e.fileName();
        if (e.isDir()) {
            if (!copyRec(e.absoluteFilePath(), target))
                return false;
        } else if (!QFile::copy(e.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}

// Modrinth mod browser for an instance: search → pick → download the jar
// straight into <instance>/mods. Lambda-wired, no Q_OBJECT.
class ModrinthModsDialog : public QDialog {
public:
    ModrinthModsDialog(const QString &mcVersion, const QString &loader,
                       const QString &modsDir, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_mcver(mcVersion)
        , m_loader(loader)
        , m_modsDir(modsDir)
    {
        setWindowTitle(QStringLiteral("Download mods — Modrinth"));
        setModal(true);
        resize(600, 500);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 14);
        lay->setSpacing(8);

        auto *row = new QHBoxLayout;
        m_query = new QLineEdit(this);
        m_query->setObjectName(QStringLiteral("licenseInput"));
        m_query->setPlaceholderText(
            QStringLiteral("Search mods for %1%2…")
                .arg(m_mcver,
                     m_loader.isEmpty() || m_loader == QStringLiteral("vanilla")
                         ? QString() : QStringLiteral(" (") + m_loader + QStringLiteral(")")));
        auto *go = new NiceButton(QStringLiteral("SEARCH"), NiceButton::Primary, this);
        go->setFixedSize(100, 32);
        row->addWidget(m_query, 1);
        row->addWidget(go);
        lay->addLayout(row);

        m_list = new QListWidget(this);
        lay->addWidget(m_list, 1);

        m_status = new QLabel(QStringLiteral("Type a query and press SEARCH"), this);
        m_status->setObjectName(QStringLiteral("cardDesc"));
        lay->addWidget(m_status);

        auto *btns = new QHBoxLayout;
        btns->addStretch(1);
        auto *close = new NiceButton(QStringLiteral("CLOSE"), NiceButton::Secondary, this);
        close->setFixedSize(110, 34);
        auto *ok = new NiceButton(QStringLiteral("DOWNLOAD"), NiceButton::Primary, this);
        ok->setFixedSize(110, 34);
        btns->addWidget(close);
        btns->addWidget(ok);
        lay->addLayout(btns);

        m_nam = new QNetworkAccessManager(this);

        connect(go, &QPushButton::clicked, this, [this] { search(); });
        connect(m_query, &QLineEdit::returnPressed, this, [this] { search(); });
        connect(close, &QPushButton::clicked, this, [this] { accept(); });
        connect(ok, &QPushButton::clicked, this, [this] { downloadSelected(); });
        connect(m_list, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *) { downloadSelected(); });
    }

    int downloadedCount() const { return m_downloaded; }

private:
    QNetworkReply *get(const QUrl &url)
    {
        QNetworkRequest req{url};
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EBALIA-Launcher/3.0"));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        return m_nam->get(req);
    }

    static QString pctEnc(const QString &s)
    {
        return QString::fromLatin1(QUrl::toPercentEncoding(s));
    }

    void search()
    {
        const QString q = m_query->text().trimmed();
        if (q.isEmpty())
            return;
        m_status->setText(QStringLiteral("Searching…"));

        // [["project_type:mod"],["versions:<mcver>"],["categories:<loader>"]]
        QStringList groups { QStringLiteral("[\"project_type:mod\"]") };
        if (!m_mcver.isEmpty())
            groups << QStringLiteral("[\"versions:%1\"]").arg(m_mcver);
        if (!m_loader.isEmpty() && m_loader != QStringLiteral("vanilla"))
            groups << QStringLiteral("[\"categories:%1\"]").arg(m_loader);
        const QString facets = QLatin1Char('[') + groups.join(QLatin1Char(',')) +
                               QLatin1Char(']');

        const QString url = QStringLiteral(
            "https://api.modrinth.com/v2/search?limit=15&query=%1&facets=%2")
            .arg(pctEnc(q), pctEnc(facets));
        QNetworkReply *r = get(QUrl(url));
        connect(r, &QNetworkReply::finished, this, [this, r] {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) {
                m_status->setText(QStringLiteral("Search failed: ") + r->errorString());
                return;
            }
            const QJsonArray hits = QJsonDocument::fromJson(r->readAll()).object()
                                        [QStringLiteral("hits")].toArray();
            m_list->clear();
            for (const QJsonValue &hv : hits) {
                const QJsonObject h = hv.toObject();
                const QString title = h[QStringLiteral("title")].toString();
                const QString author = h[QStringLiteral("author")].toString();
                const qint64 dls = h[QStringLiteral("downloads")].toVariant().toLongLong();
                auto *item = new QListWidgetItem(
                    QStringLiteral("%1  ·  %2  ·  %3 downloads")
                        .arg(title, author).arg(dls), m_list);
                item->setData(Qt::UserRole,
                              h[QStringLiteral("slug")].toString().isEmpty()
                                  ? h[QStringLiteral("project_id")].toString()
                                  : h[QStringLiteral("slug")].toString());
            }
            m_status->setText(QStringLiteral("%1 results").arg(hits.size()));
        });
    }

    void downloadSelected()
    {
        auto *item = m_list->currentItem();
        if (!item || m_busy)
            return;
        const QString slug = item->data(Qt::UserRole).toString();
        m_busy = true;
        m_status->setText(QStringLiteral("Fetching versions…"));

        // game_versions=["<mcver>"]&loaders=["<loader>"] (loader skipped on vanilla)
        QString url = QStringLiteral("https://api.modrinth.com/v2/project/") + slug +
                      QStringLiteral("/versions?");
        if (!m_mcver.isEmpty())
            url += QStringLiteral("game_versions=") +
                   pctEnc(QStringLiteral("[\"%1\"]").arg(m_mcver)) + QLatin1Char('&');
        if (!m_loader.isEmpty() && m_loader != QStringLiteral("vanilla"))
            url += QStringLiteral("loaders=") +
                   pctEnc(QStringLiteral("[\"%1\"]").arg(m_loader)) + QLatin1Char('&');

        QNetworkReply *r = get(QUrl(url));
        connect(r, &QNetworkReply::finished, this, [this, r] {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) {
                m_busy = false;
                m_status->setText(QStringLiteral("Failed: ") + r->errorString());
                return;
            }
            const QJsonArray versions = QJsonDocument::fromJson(r->readAll()).array();
            if (versions.isEmpty()) {
                m_busy = false;
                m_status->setText(QStringLiteral(
                    "No release matches this game version/loader."));
                return;
            }
            const QJsonArray files = versions.first().toObject()
                                         [QStringLiteral("files")].toArray();
            QJsonObject file;
            for (const QJsonValue &fv : files) {
                file = fv.toObject();
                if (file[QStringLiteral("primary")].toBool())
                    break;
            }
            const QString fileUrl = file[QStringLiteral("url")].toString();
            const QString fileName = file[QStringLiteral("filename")].toString();
            if (fileUrl.isEmpty() || fileName.isEmpty()) {
                m_busy = false;
                m_status->setText(QStringLiteral("No downloadable file."));
                return;
            }

            QNetworkReply *dr = get(QUrl(fileUrl));
            connect(dr, &QNetworkReply::downloadProgress, this,
                    [this](qint64 recv, qint64 total) {
                if (total > 0)
                    m_status->setText(QStringLiteral("Downloading… %1%")
                                          .arg(int(recv * 100 / total)));
            });
            connect(dr, &QNetworkReply::finished, this, [this, dr, fileName] {
                dr->deleteLater();
                m_busy = false;
                if (dr->error() != QNetworkReply::NoError) {
                    m_status->setText(QStringLiteral("Download failed: ") +
                                      dr->errorString());
                    return;
                }
                QDir().mkpath(m_modsDir);
                const QString dest = m_modsDir + QLatin1Char('/') + fileName;
                QFile out(dest);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(dr->readAll());
                    out.close();
                    ++m_downloaded;
                    m_status->setText(QStringLiteral("Installed %1 — pick another "
                                                     "or CLOSE.").arg(fileName));
                } else {
                    m_status->setText(QStringLiteral("Cannot write into mods/."));
                }
            });
        });
    }

    QString m_mcver, m_loader, m_modsDir;
    QNetworkAccessManager *m_nam;
    QLineEdit *m_query;
    QListWidget *m_list;
    QLabel *m_status;
    int m_downloaded = 0;
    bool m_busy = false;
};

// ── Dialog ──

EditInstanceDialog::EditInstanceDialog(const McInstance &inst, McInstanceManager *mgr,
                                       const QList<McVersion> &versions, QWidget *parent)
    : QDialog(parent)
    , m_dir(inst.dir)
    , m_inst(inst)
    , m_mgr(mgr)
    , m_versions(versions)
{
    setWindowTitle(QStringLiteral("Edit instance — ") + inst.name);
    setModal(true);
    resize(780, 580);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildOverviewTab(),  QStringLiteral("OVERVIEW"));
    tabs->addTab(buildContentTab(QStringLiteral("mods"),
                                 QStringLiteral("Mod jars (*.jar *.jar.disabled)"),
                                 true),
                                 QStringLiteral("MODS"));
    tabs->addTab(buildContentTab(QStringLiteral("resourcepacks"),
                                 QStringLiteral("Resource packs (*.zip *.jar *.disabled)"),
                                 false),
                                 QStringLiteral("RESOURCE PACKS"));
    tabs->addTab(buildContentTab(QStringLiteral("shaderpacks"),
                                 QStringLiteral("Shader packs (*.zip *.disabled)"),
                                 false),
                                 QStringLiteral("SHADER PACKS"));
    tabs->addTab(buildWorldsTab(),      QStringLiteral("WORLDS"));
    tabs->addTab(buildScreenshotsTab(), QStringLiteral("SCREENSHOTS"));
    tabs->addTab(buildNotesTab(),       QStringLiteral("NOTES"));
    tabs->addTab(buildLogsTab(),        QStringLiteral("LOGS"));
    tabs->addTab(buildSettingsTab(),    QStringLiteral("SETTINGS"));
    connect(tabs, &QTabWidget::currentChanged, this, [this](int) { saveNotes(); });
    lay->addWidget(tabs, 1);
}

EditInstanceDialog::~EditInstanceDialog()
{
    saveNotes();
}

QJsonObject EditInstanceDialog::readInstanceJson() const
{
    QFile f(m_dir + QStringLiteral("/instance.json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void EditInstanceDialog::writeInstanceJson(const QJsonObject &o)
{
    QFile out(m_dir + QStringLiteral("/instance.json"));
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(o).toJson());
        out.close();
    }
}

void EditInstanceDialog::saveNotes()
{
    if (!m_notes)
        return;
    QFile f(m_dir + QStringLiteral("/notes.txt"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(m_notes->toPlainText().toUtf8());
        f.close();
    }
}

// Screenshot enlarge — same approach as the fanart viewer in MainWindow
bool EditInstanceDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *lbl = qobject_cast<QLabel*>(watched);
        const QString path = lbl ? lbl->property("shotPath").toString() : QString();
        if (lbl && !path.isEmpty()) {
            const QPixmap p(path);
            if (!p.isNull()) {
                QDialog dlg(this);
                dlg.setWindowTitle(QFileInfo(path).fileName());
                auto *lay = new QVBoxLayout(&dlg);
                auto *img = new QLabel(&dlg);
                img->setPixmap(p.scaled(880, 620, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
                img->setAlignment(Qt::AlignCenter);
                lay->addWidget(img);
                auto *cap = new QLabel(QFileInfo(path).fileName(), &dlg);
                cap->setObjectName(QStringLiteral("dialogTier"));
                cap->setAlignment(Qt::AlignCenter);
                lay->addWidget(cap);
                dlg.exec();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

// ── Overview ──

QWidget *EditInstanceDialog::buildOverviewTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(18, 18, 18, 18);
    lay->setSpacing(8);

    auto *name = new QLabel(m_inst.name, w);
    name->setObjectName(QStringLiteral("pageHeader"));
    lay->addWidget(name);

    // MC version — editable combo of releases from the live manifest
    auto *verRow = new QHBoxLayout;
    verRow->setSpacing(8);
    auto *verLbl = new QLabel(QStringLiteral("Minecraft"), w);
    verLbl->setObjectName(QStringLiteral("cardName"));
    auto *verCombo = new QComboBox(w);
    verCombo->setObjectName(QStringLiteral("versionCombo"));
    for (const McVersion &v : m_versions)
        if (v.type == QStringLiteral("release"))
            verCombo->addItem(v.id, v.id);
    if (verCombo->findData(m_inst.mcVersion) < 0)
        verCombo->insertItem(0, m_inst.mcVersion, m_inst.mcVersion);
    verCombo->setCurrentIndex(qMax(0, verCombo->findData(m_inst.mcVersion)));
    verRow->addWidget(verLbl);
    verRow->addWidget(verCombo, 1);
    lay->addLayout(verRow);

    auto *loader = new QLabel(m_inst.loader.isEmpty()
                                  ? QStringLiteral("VANILLA")
                                  : m_inst.loader.toUpper(), w);
    loader->setObjectName(QStringLiteral("cardCategory"));
    lay->addWidget(loader);

    auto *status = new QLabel(m_inst.ready ? QStringLiteral("READY")
                                           : QStringLiteral("NOT INSTALLED"), w);
    status->setObjectName(m_inst.ready ? QStringLiteral("cardInstalled")
                                       : QStringLiteral("cardNotInstalled"));
    lay->addWidget(status);

    auto *verNote = new QLabel(w);
    verNote->setObjectName(QStringLiteral("cardDesc"));
    lay->addWidget(verNote);

    connect(verCombo, &QComboBox::activated, this,
            [this, status, verNote](int row) {
        auto *combo = qobject_cast<QComboBox*>(sender());
        const QString newVer = combo ? combo->itemData(row).toString() : QString();
        if (newVer.isEmpty() || newVer == m_inst.mcVersion)
            return;
        QJsonObject o = readInstanceJson();
        o[QStringLiteral("mcVersion")] = newVer;
        o[QStringLiteral("ready")] = false;
        writeInstanceJson(o);
        m_inst.mcVersion = newVer;
        m_inst.ready = false;
        status->setText(QStringLiteral("NOT INSTALLED"));
        status->setObjectName(QStringLiteral("cardNotInstalled"));
        status->style()->unpolish(status);
        status->style()->polish(status);
        verNote->setText(QStringLiteral(
            "Version changed — press INSTALL on the instance card to download it."));
    });

    lay->addSpacing(6);

    if (m_inst.totalSecs > 0) {
        const qint64 h = m_inst.totalSecs / 3600;
        const qint64 m = (m_inst.totalSecs % 3600) / 60;
        auto *played = new QLabel(QStringLiteral("Time played: %1h %2m").arg(h).arg(m), w);
        played->setObjectName(QStringLiteral("cardDesc"));
        lay->addWidget(played);
    }
    if (m_inst.lastPlayed > 0) {
        const QString when = QDateTime::fromSecsSinceEpoch(m_inst.lastPlayed)
                                 .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        auto *last = new QLabel(QStringLiteral("Last played: %1").arg(when), w);
        last->setObjectName(QStringLiteral("cardDesc"));
        lay->addWidget(last);
    }

    lay->addStretch(1);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    if (m_mgr && m_mgr->isRunning(m_dir)) {
        auto *kill = new NiceButton(QStringLiteral("KILL"), NiceButton::Outline, w);
        kill->setFixedSize(110, 36);
        connect(kill, &QPushButton::clicked, this, [this, kill] {
            m_mgr->killInstance(m_dir);
            kill->setEnabled(false);
            kill->setText(QStringLiteral("KILLED"));
        });
        btnRow->addWidget(kill);
    } else {
        auto *play = new NiceButton(QStringLiteral("PLAY"), NiceButton::Primary, w);
        play->setFixedSize(110, 36);
        connect(play, &QPushButton::clicked, this, [this] {
            emit playRequested(m_dir);
            accept();
        });
        btnRow->addWidget(play);
    }

    auto *folder = new NiceButton(QStringLiteral("OPEN FOLDER"), NiceButton::Secondary, w);
    folder->setFixedSize(130, 36);
    const QString dir = m_dir;
    connect(folder, &QPushButton::clicked, this, [dir] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    btnRow->addWidget(folder);

    auto *dup = new NiceButton(QStringLiteral("DUPLICATE"), NiceButton::Secondary, w);
    dup->setFixedSize(120, 36);
    connect(dup, &QPushButton::clicked, this, &EditInstanceDialog::duplicateInstance);
    btnRow->addWidget(dup);

    auto *exp = new NiceButton(QStringLiteral("EXPORT"), NiceButton::Secondary, w);
    exp->setFixedSize(110, 36);
    connect(exp, &QPushButton::clicked, this, &EditInstanceDialog::exportInstance);
    btnRow->addWidget(exp);

    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    return w;
}

void EditInstanceDialog::duplicateInstance()
{
    const QDir parent = QFileInfo(m_dir).dir();
    QString safe = m_inst.name + QStringLiteral(" copy");
    safe.replace(QRegularExpression(QStringLiteral("[^\\w\\-. ]")), QString());
    QString target = parent.filePath(safe);
    int n = 2;
    while (QFile::exists(target))
        target = parent.filePath(safe + QStringLiteral(" %1").arg(n++));

    if (!copyRec(m_dir, target)) {
        QMessageBox::warning(this, QStringLiteral("Duplicate"),
                             QStringLiteral("Copy failed."));
        return;
    }
    // The copy keeps a fresh identity
    QFile f(target + QStringLiteral("/instance.json"));
    if (f.open(QIODevice::ReadOnly)) {
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        o[QStringLiteral("name")] = QFileInfo(target).fileName();
        QFile out(target + QStringLiteral("/instance.json"));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QJsonDocument(o).toJson());
            out.close();
        }
    }
    QMessageBox::information(this, QStringLiteral("Duplicate"),
        QStringLiteral("Created \"%1\". It appears in the instance list when "
                       "you close this dialog.").arg(QFileInfo(target).fileName()));
}

void EditInstanceDialog::exportInstance()
{
    const QString out = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export instance"), m_inst.name + QStringLiteral(".zip"),
        QStringLiteral("Zip archives (*.zip)"));
    if (out.isEmpty())
        return;

    const QString parentPath = QFileInfo(m_dir).dir().absolutePath();
    const QString dirName = QFileInfo(m_dir).fileName();

    bool ok = false;
    QProcess p;
    p.start(QStringLiteral("bsdtar"),
            {QStringLiteral("-acf"), out, QStringLiteral("-C"), parentPath, dirName});
    if (p.waitForStarted(5000)) {
        p.waitForFinished(-1);
        ok = (p.exitCode() == 0);
    }
    if (!ok) {
        QProcess z;
        z.setWorkingDirectory(parentPath);
        z.start(QStringLiteral("zip"), {QStringLiteral("-r"), out, dirName});
        if (z.waitForStarted(5000)) {
            z.waitForFinished(-1);
            ok = (z.exitCode() == 0);
        }
    }
    QMessageBox::information(this, QStringLiteral("Export"),
        ok ? QStringLiteral("Exported to %1").arg(out)
           : QStringLiteral("Export failed (bsdtar/zip not available)."));
}

// ── Content tabs (mods / resource packs / shader packs) ──

QWidget *EditInstanceDialog::buildContentTab(const QString &subDir,
                                             const QString &addFilter, bool modsTab)
{
    const QString absDir = m_dir + QLatin1Char('/') + subDir;
    QDir().mkpath(absDir);

    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto *hint = new QLabel(QStringLiteral("Uncheck an entry to disable it (.disabled)."), w);
    hint->setObjectName(QStringLiteral("cardDesc"));
    lay->addWidget(hint);

    auto *list = new QListWidget(w);
    lay->addWidget(list, 1);
    reloadContentList(list, absDir);
    connect(list, &QListWidget::itemChanged, this,
            [this, list, absDir](QListWidgetItem *item) {
        onContentItemChanged(list, absDir, item);
    });

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto *add = new NiceButton(QStringLiteral("ADD FILE"), NiceButton::Primary, w);
    add->setFixedSize(110, 32);
    connect(add, &QPushButton::clicked, this, [this, list, absDir, addFilter] {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, QStringLiteral("Add files"), QString(), addFilter);
        for (const QString &src : files) {
            const QString dest = absDir + QLatin1Char('/') + QFileInfo(src).fileName();
            if (!QFile::exists(dest))
                QFile::copy(src, dest);
        }
        if (!files.isEmpty())
            reloadContentList(list, absDir);
    });
    btnRow->addWidget(add);

    if (modsTab) {
        auto *dl = new NiceButton(QStringLiteral("DOWNLOAD"), NiceButton::Primary, w);
        dl->setFixedSize(120, 32);
        connect(dl, &QPushButton::clicked, this, [this, list, absDir] {
            ModrinthModsDialog dlg(m_inst.mcVersion, m_inst.loader, absDir, this);
            dlg.exec();
            if (dlg.downloadedCount() > 0)
                reloadContentList(list, absDir);
        });
        btnRow->addWidget(dl);
    }

    auto *del = new NiceButton(QStringLiteral("DELETE"), NiceButton::Outline, w);
    del->setFixedSize(110, 32);
    connect(del, &QPushButton::clicked, this, [this, list, absDir] {
        const auto sel = list->selectedItems();
        if (sel.isEmpty())
            return;
        for (QListWidgetItem *it : sel) {
            QFile::remove(absDir + QLatin1Char('/') + it->data(Qt::UserRole).toString());
            delete it;
        }
    });
    btnRow->addWidget(del);

    btnRow->addStretch(1);

    auto *open = new NiceButton(QStringLiteral("OPEN FOLDER"), NiceButton::Secondary, w);
    open->setFixedSize(120, 32);
    connect(open, &QPushButton::clicked, this, [absDir] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absDir));
    });
    btnRow->addWidget(open);
    lay->addLayout(btnRow);

    return w;
}

void EditInstanceDialog::reloadContentList(QListWidget *list, const QString &absDir)
{
    list->setProperty("reloading", true);
    list->clear();
    const QDir d(absDir);
    for (const QFileInfo &e : d.entryInfoList(QDir::Files, QDir::Name)) {
        const bool enabled = !e.fileName().endsWith(QStringLiteral(".disabled"));
        QString shown = e.fileName();
        if (!enabled)
            shown.chop(9); // ".disabled"
        auto *item = new QListWidgetItem(shown, list);
        item->setData(Qt::UserRole, e.fileName());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    list->setProperty("reloading", false);
}

void EditInstanceDialog::onContentItemChanged(QListWidget *list, const QString &absDir,
                                              QListWidgetItem *item)
{
    if (list->property("reloading").toBool())
        return;

    const QString cur = item->data(Qt::UserRole).toString();
    const bool enabled = item->checkState() == Qt::Checked;

    QString wanted = cur;
    if (enabled && cur.endsWith(QStringLiteral(".disabled")))
        wanted = cur.left(cur.size() - 9);
    else if (!enabled && !cur.endsWith(QStringLiteral(".disabled")))
        wanted = cur + QStringLiteral(".disabled");
    if (wanted == cur)
        return;

    if (QFile::rename(absDir + QLatin1Char('/') + cur,
                      absDir + QLatin1Char('/') + wanted)) {
        list->setProperty("reloading", true);
        item->setData(Qt::UserRole, wanted);
        item->setText(enabled ? wanted
                              : wanted.left(wanted.size() - 9));
        list->setProperty("reloading", false);
    }
}

// ── Worlds ──

QWidget *EditInstanceDialog::buildWorldsTab()
{
    const QString savesDir = m_dir + QStringLiteral("/saves");
    QDir().mkpath(savesDir);

    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto *list = new QListWidget(w);
    lay->addWidget(list, 1);

    const QDir d(savesDir);
    for (const QFileInfo &e : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                              QDir::Name)) {
        const qint64 mib = dirSizeBytes(e.absoluteFilePath()) / (1024 * 1024);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1    —    %2 MiB").arg(e.fileName()).arg(mib), list);
        item->setData(Qt::UserRole, e.absoluteFilePath());
    }
    if (list->count() == 0) {
        auto *item = new QListWidgetItem(QStringLiteral("No worlds yet"), list);
        item->setFlags(Qt::NoItemFlags);
    }

    connect(list, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem *it) {
        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    });

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *open = new NiceButton(QStringLiteral("OPEN FOLDER"), NiceButton::Secondary, w);
    open->setFixedSize(120, 32);
    connect(open, &QPushButton::clicked, this, [savesDir] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(savesDir));
    });
    btnRow->addWidget(open);
    lay->addLayout(btnRow);

    return w;
}

// ── Screenshots ──

QWidget *EditInstanceDialog::buildScreenshotsTab()
{
    const QString shotsDir = m_dir + QStringLiteral("/screenshots");
    QDir().mkpath(shotsDir);

    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto *scroll = new QScrollArea(w);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *container = new QWidget;
    auto *grid = new QGridLayout(container);
    grid->setSpacing(10);

    const QDir d(shotsDir);
    const auto imgs = d.entryInfoList({QStringLiteral("*.png"), QStringLiteral("*.jpg"),
                                       QStringLiteral("*.jpeg")},
                                      QDir::Files, QDir::Time);
    int i = 0;
    for (const QFileInfo &e : imgs) {
        const QPixmap p(e.absoluteFilePath());
        if (p.isNull())
            continue;
        auto *thumb = new QLabel(container);
        thumb->setPixmap(p.scaled(220, 140, Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation));
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setMinimumSize(230, 150);
        thumb->setCursor(Qt::PointingHandCursor);
        thumb->setProperty("shotPath", e.absoluteFilePath());
        thumb->setStyleSheet(QStringLiteral(
            "QLabel { background-color:#1c1c1c; border:1px solid #2a2a2a;"
            " border-radius:8px; padding:6px; }"
            "QLabel:hover { border-color:#44a02d; }"));
        thumb->installEventFilter(this);
        grid->addWidget(thumb, i / 3, i % 3);
        ++i;
    }
    if (i == 0) {
        auto *empty = new QLabel(
            QStringLiteral("No screenshots yet — take some in-game with F2."), container);
        empty->setObjectName(QStringLiteral("cardDesc"));
        empty->setAlignment(Qt::AlignCenter);
        grid->addWidget(empty, 0, 0);
    }
    scroll->setWidget(container);
    lay->addWidget(scroll, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *open = new NiceButton(QStringLiteral("OPEN FOLDER"), NiceButton::Secondary, w);
    open->setFixedSize(120, 32);
    connect(open, &QPushButton::clicked, this, [shotsDir] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(shotsDir));
    });
    btnRow->addWidget(open);
    lay->addLayout(btnRow);

    return w;
}

// ── Notes ──

QWidget *EditInstanceDialog::buildNotesTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto *hint = new QLabel(
        QStringLiteral("Private notes for this instance — saved to notes.txt "
                       "when you leave the tab."), w);
    hint->setObjectName(QStringLiteral("cardDesc"));
    lay->addWidget(hint);

    m_notes = new QPlainTextEdit(w);
    QFile f(m_dir + QStringLiteral("/notes.txt"));
    if (f.open(QIODevice::ReadOnly)) {
        m_notes->setPlainText(QString::fromUtf8(f.readAll()));
        f.close();
    }
    lay->addWidget(m_notes, 1);

    return w;
}

// ── Logs ──

QWidget *EditInstanceDialog::buildLogsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    // latest.log, or the newest crash report when no log exists
    QString logPath = m_dir + QStringLiteral("/logs/latest.log");
    QString openDir = m_dir + QStringLiteral("/logs");
    if (!QFile::exists(logPath)) {
        const QDir crash(m_dir + QStringLiteral("/crash-reports"));
        const auto entries = crash.entryInfoList({QStringLiteral("*.txt")},
                                                 QDir::Files, QDir::Time);
        if (!entries.isEmpty()) {
            logPath = entries.first().absoluteFilePath();
            openDir = crash.absolutePath();
        }
    }

    auto *where = new QLabel(logPath, w);
    where->setObjectName(QStringLiteral("cardDesc"));
    lay->addWidget(where);

    auto *view = new QPlainTextEdit(w);
    view->setReadOnly(true);
    QFile f(logPath);
    if (f.open(QIODevice::ReadOnly)) {
        // Tail only: huge logs would freeze the view
        if (f.size() > 512 * 1024)
            f.seek(f.size() - 512 * 1024);
        view->setPlainText(QString::fromUtf8(f.readAll()));
        view->moveCursor(QTextCursor::End);
        f.close();
    } else {
        view->setPlainText(QStringLiteral("No log found. Launch the instance once."));
        where->hide();
    }
    lay->addWidget(view, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *open = new NiceButton(QStringLiteral("OPEN FOLDER"), NiceButton::Secondary, w);
    open->setFixedSize(120, 32);
    connect(open, &QPushButton::clicked, this, [this, openDir] {
        QDir().mkpath(openDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(openDir));
    });
    btnRow->addWidget(open);
    lay->addLayout(btnRow);

    return w;
}

// ── Settings ──

QWidget *EditInstanceDialog::buildSettingsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(18, 18, 18, 18);
    lay->setSpacing(8);

    auto *nameLbl = new QLabel(QStringLiteral("Instance name"), w);
    nameLbl->setObjectName(QStringLiteral("cardName"));
    lay->addWidget(nameLbl);

    m_nameEdit = new QLineEdit(m_inst.name, w);
    m_nameEdit->setObjectName(QStringLiteral("licenseInput"));
    lay->addWidget(m_nameEdit);

    lay->addSpacing(8);

    auto *ramLbl = new QLabel(QStringLiteral("Java max RAM (MB)"), w);
    ramLbl->setObjectName(QStringLiteral("cardName"));
    lay->addWidget(ramLbl);

    m_xmxSpin = new QSpinBox(w);
    m_xmxSpin->setRange(512, 32768);
    m_xmxSpin->setSingleStep(256);
    m_xmxSpin->setValue(m_inst.xmx > 0 ? m_inst.xmx : 2048);
    lay->addWidget(m_xmxSpin);

    lay->addSpacing(8);

    auto *javaLbl = new QLabel(QStringLiteral("Java"), w);
    javaLbl->setObjectName(QStringLiteral("cardName"));
    lay->addWidget(javaLbl);

    m_javaCombo = new QComboBox(w);
    m_javaCombo->setObjectName(QStringLiteral("versionCombo"));
    m_javaCombo->addItem(QStringLiteral("Auto (first found)"), QString());
    JavaRunner probe;
    const QStringList javas = probe.findJava();
    for (const QString &j : javas)
        m_javaCombo->addItem(j, j);
    const QString curJava = readInstanceJson()[QStringLiteral("javaPath")].toString();
    if (!curJava.isEmpty() && !javas.contains(curJava))
        m_javaCombo->insertItem(1, curJava, curJava);
    m_javaCombo->setCurrentIndex(qMax(0, m_javaCombo->findData(curJava)));
    lay->addWidget(m_javaCombo);

    lay->addSpacing(10);

    auto *apply = new NiceButton(QStringLiteral("APPLY"), NiceButton::Primary, w);
    apply->setFixedSize(120, 34);
    connect(apply, &QPushButton::clicked, this, &EditInstanceDialog::applySettings);
    lay->addWidget(apply, 0, Qt::AlignLeft);

    m_settingsStatus = new QLabel(w);
    m_settingsStatus->setObjectName(QStringLiteral("cardDesc"));
    lay->addWidget(m_settingsStatus);

    lay->addStretch(1);
    return w;
}

void EditInstanceDialog::applySettings()
{
    const QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        m_settingsStatus->setText(QStringLiteral("Name cannot be empty."));
        return;
    }

    QJsonObject o = readInstanceJson();
    o[QStringLiteral("name")] = newName;
    o[QStringLiteral("xmx")]  = m_xmxSpin->value();
    o[QStringLiteral("javaPath")] = m_javaCombo
        ? m_javaCombo->currentData().toString() : QString();

    // Rename the directory too when it's safe (free target, same parent)
    QString note;
    if (newName != m_inst.name) {
        QString safe = newName;
        safe.replace(QRegularExpression(QStringLiteral("[^\\w\\-. ]")), QString());
        const QDir parent = QFileInfo(m_dir).dir();
        const QString target = parent.filePath(safe);
        if (target != m_dir && !QFile::exists(target)) {
            if (QDir().rename(m_dir, target)) {
                m_dir = target;
                m_inst.dir = target;
            } else {
                note = QStringLiteral(" (folder rename failed — kept on-disk name)");
            }
        } else if (QFile::exists(target)) {
            note = QStringLiteral(" (folder kept — target name already exists)");
        }
    }

    writeInstanceJson(o);
    m_inst.name = newName;
    m_inst.xmx  = m_xmxSpin->value();
    setWindowTitle(QStringLiteral("Edit instance — ") + newName);
    m_settingsStatus->setText(QStringLiteral("Saved.") + note);
}
