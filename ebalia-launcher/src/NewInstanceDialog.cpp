#include "NewInstanceDialog.hpp"
#include "NiceButton.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QVariant>
#include <QDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>

// ── Generic online pack browser ─────────────────────────────────
// One dialog class serving Modrinth / CurseForge / FTB / Technic / ATLauncher.
class OnlinePackDialog : public QDialog {
public:
    enum class Source { Modrinth, CurseForge, FTB, Technic, ATLauncher };

    OnlinePackDialog(Source src, const QString &apiKey, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_src(src)
        , m_key(apiKey)
    {
        static const char *titles[] = {
            "Modrinth modpacks", "CurseForge modpacks", "FTB modpacks",
            "Technic modpacks", "ATLauncher packs"
        };
        setWindowTitle(QString::fromLatin1(titles[static_cast<int>(src)]));
        setModal(true);
        resize(620, 500);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 14, 14, 14);
        lay->setSpacing(8);

        auto *row = new QHBoxLayout;
        m_query = new QLineEdit(this);
        m_query->setObjectName(QStringLiteral("licenseInput"));
        m_query->setPlaceholderText(QStringLiteral("Search…"));
        auto *go = new NiceButton(QStringLiteral("SEARCH"), NiceButton::Primary, this);
        go->setFixedSize(100, 32);
        row->addWidget(m_query, 1);
        row->addWidget(go);
        lay->addLayout(row);

        m_list = new QListWidget(this);
        lay->addWidget(m_list, 1);

        m_status = new QLabel(QStringLiteral("Type a query and press SEARCH"), this);
        m_status->setObjectName(QStringLiteral("cardDesc"));
        m_status->setWordWrap(true);
        lay->addWidget(m_status);

        auto *btns = new QHBoxLayout;
        btns->addStretch(1);
        auto *cancel = new NiceButton(QStringLiteral("CANCELAR"), NiceButton::Secondary, this);
        cancel->setFixedSize(110, 34);
        auto *ok = new NiceButton(QStringLiteral("IMPORT"), NiceButton::Primary, this);
        ok->setFixedSize(110, 34);
        btns->addWidget(cancel);
        btns->addWidget(ok);
        lay->addLayout(btns);

        m_nam = new QNetworkAccessManager(this);

        connect(go, &QPushButton::clicked, this, [this] { search(); });
        connect(m_query, &QLineEdit::returnPressed, this, [this] { search(); });
        connect(cancel, &QPushButton::clicked, this, [this] { reject(); });
        connect(ok, &QPushButton::clicked, this, [this] { downloadSelected(); });
        connect(m_list, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *) { downloadSelected(); });
    }

    QString packPath() const { return m_packPath; }

private:
    QNetworkReply *get(const QUrl &url)
    {
        QNetworkRequest req{url};
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EBALIA-Launcher/3.0"));
        if (m_src == Source::CurseForge && !m_key.isEmpty())
            req.setRawHeader("x-api-key", m_key.toUtf8());
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        return m_nam->get(req);
    }

    void addResult(const QString &label, const QVariant &id, const QString &extra = QString())
    {
        auto *item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, extra);
    }

    void search()
    {
        const QString q = m_query->text().trimmed();
        if (q.isEmpty() && m_src != Source::Technic && m_src != Source::ATLauncher)
            return;
        m_list->clear();
        m_status->setText(QStringLiteral("Searching…"));

        QString url;
        switch (m_src) {
        case Source::Modrinth:
            url = QStringLiteral("https://api.modrinth.com/v2/search?limit=15"
                                 "&facets=%5B%5B%22project_type:modpack%22%5D%5D&query=%1")
                      .arg(q);
            break;
        case Source::CurseForge:
            url = QStringLiteral("https://api.curseforge.com/v1/mods/search?gameId=432"
                                 "&classId=4471&pageSize=15&searchFilter=%1").arg(q);
            break;
        case Source::FTB:
            url = QStringLiteral("https://api.modpacks.ch/public/modpacks?limit=30&search=%1").arg(q);
            break;
        case Source::Technic:
            url = QStringLiteral("https://api.technicpack.net/discover/");
            break;
        case Source::ATLauncher:
            url = QStringLiteral("https://atlcdn.net/packs.json");
            break;
        }

        QNetworkReply *r = get(QUrl(url));
        connect(r, &QNetworkReply::finished, this, [this, r, q] { onSearchReply(r, q); });
    }

    void onSearchReply(QNetworkReply *r, const QString &q)
    {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            m_status->setText(QStringLiteral("Search failed: ") + r->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());

        switch (m_src) {
        case Source::Modrinth: {
            for (const QJsonValue &hv : doc.object()[QStringLiteral("hits")].toArray()) {
                const QJsonObject h = hv.toObject();
                addResult(QStringLiteral("%1  ·  %2  ·  %3 downloads")
                              .arg(h[QStringLiteral("title")].toString(),
                                   h[QStringLiteral("author")].toString())
                              .arg(h[QStringLiteral("downloads")].toVariant().toLongLong()),
                          h[QStringLiteral("slug")].toString());
            }
            break;
        }
        case Source::CurseForge: {
            for (const QJsonValue &dv : doc.object()[QStringLiteral("data")].toArray()) {
                const QJsonObject d = dv.toObject();
                addResult(QStringLiteral("%1  ·  %2 downloads")
                              .arg(d[QStringLiteral("name")].toString())
                              .arg(d[QStringLiteral("downloadCount")].toVariant().toLongLong()),
                          d[QStringLiteral("id")].toVariant().toLongLong());
            }
            break;
        }
        case Source::FTB: {
            for (const QJsonValue &pv : doc.object()[QStringLiteral("packs")].toArray()) {
                const QJsonObject p = pv.toObject();
                addResult(QStringLiteral("%1  ·  %2 installs")
                              .arg(p[QStringLiteral("name")].toString())
                              .arg(p[QStringLiteral("installs")].toVariant().toLongLong()),
                          p[QStringLiteral("id")].toVariant().toLongLong());
            }
            break;
        }
        case Source::Technic: {
            // Discover returns trending/official packs
            const QJsonObject root = doc.object();
            const QStringList groups = { QStringLiteral("trending"), QStringLiteral("official") };
            for (const QString &g : groups) {
                for (const QJsonValue &pv : root[g].toArray()) {
                    const QJsonObject p = pv.toObject();
                    addResult(QStringLiteral("%1  ·  %2").arg(p[QStringLiteral("name")].toString(),
                                                             g),
                              p[QStringLiteral("id")].toVariant().toLongLong(),
                              p[QStringLiteral("url")].toString());
                }
            }
            break;
        }
        case Source::ATLauncher: {
            for (const QJsonValue &pv : doc.array()) {
                const QJsonObject p = pv.toObject();
                if (!q.isEmpty() &&
                    !p[QStringLiteral("name")].toString().contains(q, Qt::CaseInsensitive))
                    continue;
                addResult(p[QStringLiteral("name")].toString(),
                          p[QStringLiteral("id")].toVariant().toLongLong());
            }
            break;
        }
        }
        m_status->setText(QStringLiteral("%1 results").arg(m_list->count()));
    }

    void downloadSelected()
    {
        auto *item = m_list->currentItem();
        if (!item || m_busy)
            return;
        m_busy = true;
        const QVariant id = item->data(Qt::UserRole);

        switch (m_src) {
        case Source::Modrinth: {
            m_status->setText(QStringLiteral("Fetching versions…"));
            QNetworkReply *r = get(QUrl(QStringLiteral(
                "https://api.modrinth.com/v2/project/%1/versions").arg(id.toString())));
            connect(r, &QNetworkReply::finished, this, [this, r] {
                r->deleteLater();
                const QJsonArray vers = QJsonDocument::fromJson(r->readAll()).array();
                QString url, fname;
                if (!vers.isEmpty()) {
                    const QJsonArray files = vers.first().toObject()
                                                 [QStringLiteral("files")].toArray();
                    if (!files.isEmpty()) {
                        url    = files.first().toObject()[QStringLiteral("url")].toString();
                        fname  = files.first().toObject()[QStringLiteral("filename")].toString();
                    }
                }
                downloadFile(url, fname);
            });
            break;
        }
        case Source::CurseForge: {
            m_status->setText(QStringLiteral("Fetching files…"));
            QNetworkReply *r = get(QUrl(QStringLiteral(
                "https://api.curseforge.com/v1/mods/%1/files?pageSize=25")
                                            .arg(id.toLongLong())));
            connect(r, &QNetworkReply::finished, this, [this, r] {
                r->deleteLater();
                const QJsonArray files = QJsonDocument::fromJson(r->readAll()).object()
                                             [QStringLiteral("data")].toArray();
                QString url, fname;
                if (!files.isEmpty()) {
                    const QJsonObject f = files.first().toObject();
                    url   = f[QStringLiteral("downloadUrl")].toString();
                    fname = f[QStringLiteral("fileName")].toString();
                    if (url.isEmpty()) {
                        const qint64 fid = f[QStringLiteral("id")].toVariant().toLongLong();
                        url = QStringLiteral("https://edge.forgecdn.net/files/%1/%2/%3")
                                  .arg(fid / 1000).arg(fid % 1000).arg(fname);
                    }
                }
                downloadFile(url, fname);
            });
            break;
        }
        case Source::FTB: {
            // FTB: get pack detail → latest version → its file list lives in
            // the version object; we download the server zip if published,
            // otherwise tell the user the client files list
            m_status->setText(QStringLiteral("Fetching pack…"));
            QNetworkReply *r = get(QUrl(QStringLiteral(
                "https://api.modpacks.ch/public/modpack/%1").arg(id.toLongLong())));
            connect(r, &QNetworkReply::finished, this, [this, r, id] {
                r->deleteLater();
                const QJsonArray vers = QJsonDocument::fromJson(r->readAll()).object()
                                            [QStringLiteral("versions")].toArray();
                if (vers.isEmpty()) { fail(QStringLiteral("No versions")); return; }
                const qint64 packId = 0; // unused
                Q_UNUSED(packId)
                const qint64 verId = vers.first().toObject()[QStringLiteral("id")]
                                         .toVariant().toLongLong();
                const QString name = QStringLiteral("ftb_pack_%1.zip").arg(verId);
                // FTB exposes a full client zip per version
                downloadFile(QStringLiteral("https://api.modpacks.ch/public/modpack/%1/%2/client")
                                 .arg(id.toLongLong()).arg(verId),
                             name);
            });
            break;
        }
        case Source::Technic: {
            const QString url = item->data(Qt::UserRole + 1).toString();
            if (url.isEmpty()) { fail(QStringLiteral("No direct download for this pack")); return; }
            downloadFile(url, QStringLiteral("technic_pack.zip"));
            break;
        }
        case Source::ATLauncher: {
            m_status->setText(QStringLiteral("Fetching pack…"));
            QNetworkReply *r = get(QUrl(QStringLiteral(
                "https://atlcdn.net/packs/%1.json").arg(id.toLongLong())));
            connect(r, &QNetworkReply::finished, this, [this, r] {
                r->deleteLater();
                const QJsonArray vers = QJsonDocument::fromJson(r->readAll()).object()
                                            [QStringLiteral("versions")].toArray();
                QString url;
                if (!vers.isEmpty())
                    url = vers.first().toObject()[QStringLiteral("downloadURL")].toString();
                downloadFile(url, QStringLiteral("atlauncher_pack.zip"));
            });
            break;
        }
        }
    }

    void downloadFile(const QString &url, const QString &fname)
    {
        if (url.isEmpty()) {
            fail(QStringLiteral("No direct download available for this pack"));
            return;
        }
        m_status->setText(QStringLiteral("Downloading…"));
        QNetworkReply *r = get(QUrl(url));
        connect(r, &QNetworkReply::finished, this, [this, r, fname] {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError) {
                fail(r->errorString());
                return;
            }
            const QString dest = QDir::tempPath() + QLatin1Char('/') +
                                 (fname.isEmpty() ? QStringLiteral("pack.zip") : fname);
            QFile out(dest);
            if (!out.open(QIODevice::WriteOnly)) {
                fail(QStringLiteral("Cannot write temp file"));
                return;
            }
            out.write(r->readAll());
            out.close();
            m_packPath = dest;
            accept();
        });
        connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
            if (total > 0)
                m_status->setText(QStringLiteral("Downloading… %1%")
                                      .arg(int(got * 100 / total)));
        });
    }

    void fail(const QString &err)
    {
        m_status->setText(QStringLiteral("Failed: ") + err);
        m_busy = false;
    }

    Source m_src;
    QString m_key;
    QString m_packPath;
    bool m_busy = false;
    QLineEdit *m_query;
    QListWidget *m_list;
    QLabel *m_status;
    QNetworkAccessManager *m_nam;
};

// ── Dialog (Prism layout, EBALIA style) ─────────────────────────

NewInstanceDialog::NewInstanceDialog(const QList<McVersion> &versions, QWidget *parent)
    : QDialog(parent)
    , m_versions(versions)
{
    setWindowTitle(QStringLiteral("New instance"));
    setModal(true);
    resize(880, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // ── Top: icon + Nombre/Grupo ──
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    auto *icon = new QLabel(this);
    icon->setPixmap(QPixmap(QStringLiteral(":/ebalia.png"))
                        .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setFixedSize(64, 64);
    topRow->addWidget(icon, 0, Qt::AlignTop);

    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(6);
    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("Nombre:"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("licenseInput"));
    m_nameEdit->setPlaceholderText(QStringLiteral("1.20.1 survival"));
    nameRow->addWidget(m_nameEdit, 1);
    nameCol->addLayout(nameRow);

    auto *groupRow = new QHBoxLayout;
    groupRow->addWidget(new QLabel(QStringLiteral("Grupo:"), this));
    auto *groupCombo = new QComboBox(this);
    groupCombo->setObjectName(QStringLiteral("versionCombo"));
    groupCombo->addItem(QStringLiteral("Sin grupo"));
    groupRow->addWidget(groupCombo, 1);
    nameCol->addLayout(groupRow);
    topRow->addLayout(nameCol, 1);
    root->addLayout(topRow);

    // ── Middle: sources | version table | filters/loaders ──
    auto *mid = new QHBoxLayout;
    mid->setSpacing(12);

    auto *srcList = new QListWidget(this);
    srcList->setObjectName(QStringLiteral("cardsScroll"));
    srcList->setFixedWidth(150);
    srcList->addItem(QStringLiteral("Personalizado"));
    srcList->addItem(QStringLiteral("Importar"));
    srcList->addItem(QStringLiteral("Modrinth"));
    srcList->addItem(QStringLiteral("CurseForge"));
    srcList->addItem(QStringLiteral("ATLauncher"));
    srcList->addItem(QStringLiteral("FTB"));
    srcList->addItem(QStringLiteral("Technic"));
    srcList->setCurrentRow(0);
    connect(srcList, &QListWidget::currentRowChanged,
            this, &NewInstanceDialog::onSourceRow);
    mid->addWidget(srcList);

    auto *centerCol = new QVBoxLayout;
    centerCol->setSpacing(6);

    m_versionList = new QTreeWidget(this);
    m_versionList->setObjectName(QStringLiteral("cardsScroll"));
    m_versionList->setHeaderLabels({ QStringLiteral("Versión"),
                                     QStringLiteral("Lanzamiento"),
                                     QStringLiteral("Tipo") });
    m_versionList->setRootIsDecorated(false);
    m_versionList->setUniformRowHeights(true);
    m_versionList->header()->setStretchLastSection(false);
    m_versionList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    centerCol->addWidget(m_versionList, 1);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("licenseInput"));
    m_search->setPlaceholderText(QStringLiteral("Buscar"));
    connect(m_search, &QLineEdit::textChanged, this, &NewInstanceDialog::refillVersions);
    centerCol->addWidget(m_search);

    // Loader versions table (like Prism's second table)
    m_loaderVerLabel = new QLabel(QStringLiteral("Versión del loader"), this);
    m_loaderVerLabel->setObjectName(QStringLiteral("cardName"));
    centerCol->addWidget(m_loaderVerLabel);
    m_loaderList = new QTreeWidget(this);
    m_loaderList->setObjectName(QStringLiteral("cardsScroll"));
    m_loaderList->setHeaderLabels({ QStringLiteral("Versión") });
    m_loaderList->setRootIsDecorated(false);
    m_loaderList->setUniformRowHeights(true);
    m_loaderList->setMaximumHeight(170);
    connect(m_loaderList, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
        m_loaderVersion = cur ? cur->text(0) : QString();
    });
    centerCol->addWidget(m_loaderList);
    m_loaderVerLabel->hide();
    m_loaderList->hide();

    // Import row (hidden unless a non-Custom source is selected)
    auto *importRow = new QHBoxLayout;
    m_importLabel = new QLabel(QStringLiteral("No file selected"), this);
    m_importLabel->setObjectName(QStringLiteral("cardDesc"));
    m_zipBtn = new NiceButton(QStringLiteral("CHOOSE .ZIP"), NiceButton::Secondary, this);
    m_zipBtn->setFixedSize(130, 32);
    connect(m_zipBtn, &QPushButton::clicked, this, &NewInstanceDialog::pickImportZip);
    m_browseBtn = new NiceButton(QStringLiteral("BROWSE"), NiceButton::Secondary, this);
    m_browseBtn->setFixedSize(120, 32);
    connect(m_browseBtn, &QPushButton::clicked, this, &NewInstanceDialog::pickOnline);
    importRow->addWidget(m_importLabel, 1);
    importRow->addWidget(m_zipBtn);
    importRow->addWidget(m_browseBtn);
    centerCol->addLayout(importRow);
    m_importLabel->hide();
    m_zipBtn->hide();
    m_browseBtn->hide();

    mid->addLayout(centerCol, 1);

    // Right column: filters + loaders
    auto *filterCol = new QVBoxLayout;
    filterCol->setSpacing(4);

    auto *filterTitle = new QLabel(QStringLiteral("Filtrar"), this);
    filterTitle->setObjectName(QStringLiteral("cardName"));
    filterCol->addWidget(filterTitle);

    m_fReleases  = new QCheckBox(QStringLiteral("Lanzamientos"), this);
    m_fSnapshots = new QCheckBox(QStringLiteral("Snapshots"), this);
    m_fBetas     = new QCheckBox(QStringLiteral("Betas"), this);
    m_fAlphas    = new QCheckBox(QStringLiteral("Alfas"), this);
    m_fReleases->setChecked(true);
    for (auto *cb : { m_fReleases, m_fSnapshots, m_fBetas, m_fAlphas }) {
        cb->setStyleSheet(QStringLiteral("color:#d0d0d0; font-size:12px;"));
        connect(cb, &QCheckBox::toggled, this, &NewInstanceDialog::refillVersions);
        filterCol->addWidget(cb);
    }

    filterCol->addSpacing(8);
    auto *loaderTitle = new QLabel(QStringLiteral("Loader de mods"), this);
    loaderTitle->setObjectName(QStringLiteral("cardName"));
    filterCol->addWidget(loaderTitle);

    auto *loaderGroup = new QButtonGroup(this);
    const struct { const char *label; const char *id; } loaders[] = {
        { "Ninguno",   "vanilla"    },
        { "NeoForge",  "neoforge"   },
        { "Forge",     "forge"      },
        { "Fabric",    "fabric"     },
        { "Quilt",     "quilt"      },
        { "LiteLoader","liteloader" },
    };
    for (int i = 0; i < 6; ++i) {
        auto *rb = new QRadioButton(QString::fromLatin1(loaders[i].label), this);
        rb->setStyleSheet(QStringLiteral("color:#d0d0d0; font-size:12px;"));
        rb->setProperty("loaderId", QLatin1String(loaders[i].id));
        loaderGroup->addButton(rb, i);
        filterCol->addWidget(rb);
        if (i == 0)
            rb->setChecked(true);
    }
    connect(loaderGroup, &QButtonGroup::idClicked, this, [this, loaderGroup](int) {
        m_selectedLoader = loaderGroup->checkedButton()
                               ->property("loaderId").toString();
        fetchLoaderVersions();
    });
    connect(m_versionList, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *, QTreeWidgetItem *) { fetchLoaderVersions(); });
    filterCol->addStretch(1);
    mid->addLayout(filterCol);
    root->addLayout(mid, 1);

    // ── Bottom: Ayuda / Cancelar / OK ──
    auto *btnRow = new QHBoxLayout;
    auto *help = new NiceButton(QStringLiteral("AYUDA"), NiceButton::Secondary, this);
    help->setFixedSize(100, 34);
    btnRow->addWidget(help);
    btnRow->addStretch(1);
    auto *cancel = new NiceButton(QStringLiteral("CANCELAR"), NiceButton::Secondary, this);
    cancel->setFixedSize(120, 34);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *ok = new NiceButton(QStringLiteral("OK"), NiceButton::Primary, this);
    ok->setFixedSize(120, 34);
    connect(ok, &QPushButton::clicked, this, [this] {
        if (m_importZip.isEmpty() && m_nameEdit->text().trimmed().isEmpty())
            return;
        if (m_importZip.isEmpty() && !m_versionList->currentItem())
            return;
        accept();
    });
    btnRow->addWidget(cancel);
    btnRow->addWidget(ok);
    root->addLayout(btnRow);

    refillVersions();
}

void NewInstanceDialog::onSourceRow(int row)
{
    // 0 Personalizado, 1 Importar, 2..6 online sources
    m_importLabel->setVisible(row != 0);
    m_zipBtn->setVisible(row == 1);
    m_browseBtn->setVisible(row >= 2);
    m_versionList->setEnabled(row == 0);
    m_search->setEnabled(row == 0);
}

void NewInstanceDialog::pickImportZip()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import modpack"), QString(),
        QStringLiteral("Modpacks (*.zip *.mrpack)"));
    if (path.isEmpty())
        return;
    m_importZip = path;
    m_importLabel->setText(QFileInfo(path).fileName());
    if (m_nameEdit->text().trimmed().isEmpty())
        m_nameEdit->setText(QFileInfo(path).completeBaseName());
}

void NewInstanceDialog::pickOnline()
{
    auto *srcList = findChild<QListWidget*>();
    const int row = srcList ? srcList->currentRow() : 0;

    OnlinePackDialog::Source src;
    switch (row) {
    case 2: src = OnlinePackDialog::Source::Modrinth;    break;
    case 3: src = OnlinePackDialog::Source::CurseForge;  break;
    case 4: src = OnlinePackDialog::Source::ATLauncher;  break;
    case 5: src = OnlinePackDialog::Source::FTB;         break;
    case 6: src = OnlinePackDialog::Source::Technic;     break;
    default: return;
    }

    QString key;
    if (src == OnlinePackDialog::Source::CurseForge) {
        QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
        key = s.value(QStringLiteral("curseforge/apikey")).toString();
        if (key.isEmpty()) {
            m_importLabel->setText(QStringLiteral(
                "No API key — Settings → CurseForge API key (free at console.curseforge.com)"));
            return;
        }
    }

    OnlinePackDialog dlg(src, key, this);
    if (dlg.exec() != QDialog::Accepted || dlg.packPath().isEmpty())
        return;
    m_importZip = dlg.packPath();
    m_importLabel->setText(QFileInfo(m_importZip).fileName());
    if (m_nameEdit->text().trimmed().isEmpty())
        m_nameEdit->setText(QFileInfo(m_importZip).completeBaseName());
}

void NewInstanceDialog::refillVersions()
{
    m_versionList->clear();
    const QString needle = m_search ? m_search->text().trimmed() : QString();

    auto typeAllowed = [this](const QString &t) {
        if (t == QStringLiteral("release"))   return m_fReleases->isChecked();
        if (t == QStringLiteral("snapshot"))  return m_fSnapshots->isChecked();
        if (t == QStringLiteral("old_beta"))  return m_fBetas->isChecked();
        if (t == QStringLiteral("old_alpha")) return m_fAlphas->isChecked();
        return false;
    };

    for (const McVersion &v : m_versions) {
        if (!typeAllowed(v.type))
            continue;
        if (!needle.isEmpty() && !v.id.contains(needle, Qt::CaseInsensitive))
            continue;
        auto *item = new QTreeWidgetItem(QStringList{
            v.id,
            v.releaseTime,
            v.type == QStringLiteral("release") ? QStringLiteral("release") : v.type,
        });
        item->setData(0, Qt::UserRole, v.id);
        m_versionList->addTopLevelItem(item);
    }
    if (m_versionList->topLevelItemCount())
        m_versionList->setCurrentItem(m_versionList->topLevelItem(0));
}

QString NewInstanceDialog::instanceName() const
{
    if (!m_importZip.isEmpty() && m_nameEdit->text().trimmed().isEmpty())
        return QFileInfo(m_importZip).completeBaseName();
    return m_nameEdit->text().trimmed();
}

QString NewInstanceDialog::mcVersion() const
{
    auto *item = m_versionList->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

QString NewInstanceDialog::loader() const { return m_selectedLoader; }
QString NewInstanceDialog::loaderVersion() const { return m_loaderVersion; }
QString NewInstanceDialog::importZipPath() const { return m_importZip; }

// Fetch the selected loader's available versions for the chosen MC version
void NewInstanceDialog::fetchLoaderVersions()
{
    m_loaderVersion.clear();

    const bool show = (m_selectedLoader != QStringLiteral("vanilla"));
    m_loaderVerLabel->setVisible(show);
    m_loaderList->setVisible(show);
    m_loaderList->clear();
    if (!show)
        return;

    const QString mc = mcVersion();
    if (mc.isEmpty())
        return;

    const QString loader = m_selectedLoader;
    auto *nam = new QNetworkAccessManager(this);
    auto fill = [this](const QStringList &versions) {
        m_loaderList->clear();
        for (const QString &v : versions) {
            auto *item = new QTreeWidgetItem(QStringList{ v });
            m_loaderList->addTopLevelItem(item);
        }
        if (m_loaderList->topLevelItemCount())
            m_loaderList->setCurrentItem(m_loaderList->topLevelItem(0));
    };

    QString url;
    if (loader == QStringLiteral("fabric"))
        url = QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/") + mc;
    else if (loader == QStringLiteral("quilt"))
        url = QStringLiteral("https://meta.quiltmc.org/v3/versions/loader/") + mc;
    else if (loader == QStringLiteral("forge"))
        url = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml");
    else if (loader == QStringLiteral("neoforge"))
        url = QStringLiteral("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/neoforge");
    else if (loader == QStringLiteral("liteloader")) {
        fill({ QStringLiteral("latest (≤1.12.2)") });
        return;
    } else
        return;

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EBALIA-Launcher/3.0"));
    QNetworkReply *r = nam->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, nam, loader, mc, fill] {
        r->deleteLater();
        nam->deleteLater();
        if (r->error() != QNetworkReply::NoError)
            return;
        const QByteArray body = r->readAll();
        QStringList versions;

        if (loader == QStringLiteral("forge")) {
            // maven-metadata.xml: <version>1.20.1-47.4.0</version>
            const QString xml = QString::fromUtf8(body);
            static const QRegularExpression re(
                QStringLiteral("<version>([^<]+)</version>"));
            auto it = re.globalMatch(xml);
            QStringList all;
            while (it.hasNext())
                all.prepend(it.next().captured(1));
            for (const QString &v : all)
                if (v.startsWith(mc + QLatin1Char('-')))
                    versions << v;
        } else if (loader == QStringLiteral("neoforge")) {
            const QJsonArray arr = QJsonDocument::fromJson(body).object()
                                       [QStringLiteral("versions")].toArray();
            QString prefix = mc;
            if (prefix.startsWith(QStringLiteral("1.")))
                prefix = prefix.mid(2);
            for (const QJsonValue &v : arr) {
                const QString s = v.toString();
                if (s.startsWith(prefix + QLatin1Char('.')))
                    versions.prepend(s);
            }
        } else {
            // fabric / quilt: array of {loader:{version}}
            const QJsonArray arr = QJsonDocument::fromJson(body).array();
            for (const QJsonValue &v : arr)
                versions << v.toObject()[QStringLiteral("loader")].toObject()
                              [QStringLiteral("version")].toString();
        }

        if (versions.isEmpty())
            versions << QStringLiteral("latest");
        fill(versions);
    });
}
