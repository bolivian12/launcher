#pragma once
#include <QWidget>
#include <QStringList>

namespace Bedrock {
struct Command {
    QString program;
    QStringList arguments;
    bool valid() const { return !program.isEmpty(); }
};
enum class Platform { Windows, Linux, MacOS, Unsupported };
Platform hostPlatform();
// Build an argument vector, never a shell command. macOS bundles are opened with /usr/bin/open.
Command localCommand(const QString &path, Platform platform);
Command flatpakCommand(const QString &executable);
}
class QProcess;
class BedrockPage : public QWidget {
public:
    explicit BedrockPage(const QString &dataRoot, QWidget *parent = nullptr);
    ~BedrockPage() override;
private:
    QProcess *m_probe = nullptr;
};
