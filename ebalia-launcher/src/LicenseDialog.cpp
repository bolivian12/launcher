#include "LicenseDialog.hpp"
#include "LicenseManager.hpp"
#include "PatreonAuth.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>

LicenseDialog::LicenseDialog(LicenseManager *license, PatreonAuth *auth, QWidget *parent)
    : QDialog(parent)
    , m_license(license)
    , m_auth(auth)
{
    setWindowTitle(QStringLiteral("EBALIA Membership"));
    setModal(true);
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto *logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/ebalia.png"))
                        .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    auto *title = new QLabel(QStringLiteral("MEMBERSHIP"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("dialogTier"));
    m_status->setAlignment(Qt::AlignCenter);
    root->addWidget(m_status);

    auto *tiers = new QLabel(
        QStringLiteral("All versions and mods are FREE for everyone.\n"
                       "Patreon members (any tier) unlock the live feed:\n"
                       "dev leaks, early builds and announcements."), this);
    tiers->setObjectName(QStringLiteral("dialogInfo"));
    tiers->setAlignment(Qt::AlignCenter);
    root->addWidget(tiers);

    // ── How to connect (easy guide) ──
    auto *guide = new QLabel(
        QStringLiteral("HOW TO CONNECT\n\n"
                       "1.  Press CONNECT WITH PATREON.\n"
                       "2.  Your browser opens Patreon — log in and press Allow.\n"
                       "3.  Come back here. Your name and tier appear\n"
                       "     in the sidebar, top left. That's it."), this);
    guide->setObjectName(QStringLiteral("dialogInfo"));
    root->addWidget(guide);

    m_feedback = new QLabel(this);
    m_feedback->setObjectName(QStringLiteral("dialogFeedback"));
    m_feedback->setWordWrap(true);
    m_feedback->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_feedback);

    // ── Developer setup (hidden unless needed) ──
    if (!m_auth->isConfigured()) {
        auto *devToggle = new QPushButton(QStringLiteral("Developer setup ▸"), this);
        devToggle->setObjectName(QStringLiteral("dialogBtnSecondary"));
        devToggle->setCursor(Qt::PointingHandCursor);
        root->addWidget(devToggle);

        auto *devBox = new QWidget(this);
        devBox->setObjectName(QStringLiteral("devBox"));
        auto *devLay = new QVBoxLayout(devBox);
        devLay->setContentsMargins(0, 4, 0, 0);
        devLay->setSpacing(8);

        auto *cfgTitle = new QLabel(
            QStringLiteral("Create a client at patreon.com/portal/registration/register-clients\n"
                           "with redirect URI http://127.0.0.1/callback and paste:"), devBox);
        cfgTitle->setObjectName(QStringLiteral("dialogInfo"));
        devLay->addWidget(cfgTitle);

        auto *idEdit = new QLineEdit(devBox);
        idEdit->setObjectName(QStringLiteral("licenseInput"));
        idEdit->setPlaceholderText(QStringLiteral("Client ID"));
        devLay->addWidget(idEdit);

        auto *secretEdit = new QLineEdit(devBox);
        secretEdit->setObjectName(QStringLiteral("licenseInput"));
        secretEdit->setPlaceholderText(QStringLiteral("Client Secret"));
        secretEdit->setEchoMode(QLineEdit::Password);
        devLay->addWidget(secretEdit);

        auto *saveBtn = new QPushButton(QStringLiteral("SAVE CREDENTIALS"), devBox);
        saveBtn->setObjectName(QStringLiteral("dialogBtnSecondary"));
        saveBtn->setCursor(Qt::PointingHandCursor);
        devLay->addWidget(saveBtn);

        devBox->hide();
        root->addWidget(devBox);

        connect(devToggle, &QPushButton::clicked, this, [devToggle, devBox] {
            const bool show = !devBox->isVisible();
            devBox->setVisible(show);
            devToggle->setText(show ? QStringLiteral("Developer setup ▾")
                                    : QStringLiteral("Developer setup ▸"));
        });

        connect(saveBtn, &QPushButton::clicked, this, [this, idEdit, secretEdit] {
            QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
            s.setValue(QStringLiteral("patreon/client_id"), idEdit->text().trimmed());
            s.setValue(QStringLiteral("patreon/client_secret"), secretEdit->text().trimmed());
            if (m_auth->isConfigured()) {
                m_feedback->setStyleSheet(QStringLiteral("color:#8be86e;"));
                m_feedback->setText(QStringLiteral("Credentials saved — you can connect now."));
            } else {
                m_feedback->setStyleSheet(QStringLiteral("color:#e05a5a;"));
                m_feedback->setText(QStringLiteral("Both fields are required."));
            }
        });
    }

    // ── Buttons ──
    auto *row = new QHBoxLayout;
    m_disconnectBtn = new QPushButton(QStringLiteral("DISCONNECT"), this);
    m_disconnectBtn->setObjectName(QStringLiteral("dialogBtnSecondary"));
    m_disconnectBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_disconnectBtn);
    row->addStretch(1);
    m_connectBtn = new QPushButton(QStringLiteral("CONNECT WITH PATREON"), this);
    m_connectBtn->setObjectName(QStringLiteral("dialogBtn"));
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setDefault(true);
    row->addWidget(m_connectBtn);
    root->addLayout(row);

    // ── Dev preview: see the launcher as a member (developer only) ──
    auto *devPreview = new QPushButton(QStringLiteral("DEV PREVIEW: OFF"), this);
    devPreview->setObjectName(QStringLiteral("dialogBtnSecondary"));
    devPreview->setCursor(Qt::PointingHandCursor);
    devPreview->setToolTip(QStringLiteral("Developer: cycle through membership tiers to preview them"));
    root->addWidget(devPreview);

    connect(devPreview, &QPushButton::clicked, this, [this, devPreview] {
        static const char *names[] = { "OFF", "EXCLUSIVE", "EXPLICIT", "SENIOR" };
        const int current = LicenseManager::tierRank(m_license->tier());
        const int next = (current + 1) % 4;
        if (next == 0)
            m_license->unlink();
        else
            m_license->linkPatreon(QStringLiteral("DevPreview"), next);
        devPreview->setText(QStringLiteral("DEV PREVIEW: %1").arg(QLatin1String(names[next])));
    });

    connect(m_connectBtn, &QPushButton::clicked, this, &LicenseDialog::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &LicenseDialog::onDisconnect);
    connect(m_license, &LicenseManager::licenseChanged, this, &LicenseDialog::refresh);
    connect(m_auth, &PatreonAuth::loginFailed, this, &LicenseDialog::onAuthError);
    connect(m_auth, &PatreonAuth::identityReady, this, &LicenseDialog::onIdentity);

    refresh();
}

void LicenseDialog::refresh()
{
    if (m_license->isLinked()) {
        m_status->setText(QStringLiteral("%1 — %2")
                              .arg(m_license->patronName(),
                                   LicenseManager::tierName(m_license->tier())));
    } else {
        m_status->setText(QStringLiteral("Not linked — FREE tier"));
    }
    m_disconnectBtn->setVisible(m_license->isLinked());
    m_connectBtn->setVisible(!m_license->isLinked());
}

void LicenseDialog::onConnect()
{
    if (!m_auth->isConfigured()) {
        // Point exactly at what's missing, and open the setup box for them
        m_feedback->setStyleSheet(QStringLiteral("color:#e8c154;"));
        m_feedback->setText(QStringLiteral(
            "Save the Client ID and Secret first — open “Developer setup” below."));
        if (auto *devBox = findChild<QWidget*>(QStringLiteral("devBox")))
            devBox->setVisible(true);
        if (auto *t = findChild<QPushButton*>(QStringLiteral("dialogBtnSecondary"))) {
            // leave toggle text as-is; box is now visible
            Q_UNUSED(t)
        }
        return;
    }
    m_auth->startLogin();
    m_feedback->setStyleSheet(QStringLiteral("color:#8b8b8b;"));
    m_feedback->setText(QStringLiteral(
        "Opening Patreon in your browser...\nIf nothing opens, copy this link:\n")
        + m_auth->authUrl());
}

void LicenseDialog::onDisconnect()
{
    m_auth->logout();
    m_license->unlink();
    m_feedback->setStyleSheet(QStringLiteral("color:#8b8b8b;"));
    m_feedback->setText(QStringLiteral("Disconnected — back to FREE tier."));
}

void LicenseDialog::onAuthError(const QString &err)
{
    m_feedback->setStyleSheet(QStringLiteral("color:#e05a5a;"));
    m_feedback->setText(err);
}

void LicenseDialog::onIdentity(const QString &name, int rank, const QString &title)
{
    Q_UNUSED(title)
    m_license->linkPatreon(name, rank);
    m_feedback->setStyleSheet(QStringLiteral("color:#8be86e;"));
    m_feedback->setText(QStringLiteral("Linked — %1 tier active.")
                            .arg(LicenseManager::tierName(m_license->tier())));
}
