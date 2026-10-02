#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QPixmap;
class LicenseManager;
class PatreonAuth;

// Membership dialog: link a Patreon account, see the active tier, disconnect.
class LicenseDialog : public QDialog {
    Q_OBJECT
public:
    explicit LicenseDialog(LicenseManager *license, PatreonAuth *auth,
                           QWidget *parent = nullptr);

private slots:
    void refresh();
    void onConnect();
    void onDisconnect();
    void onAuthError(const QString &err);
    void onIdentity(const QString &name, int rank, const QString &title);

private:
    LicenseManager *m_license;
    PatreonAuth *m_auth;
    QLabel *m_status;
    QLabel *m_feedback;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
};
