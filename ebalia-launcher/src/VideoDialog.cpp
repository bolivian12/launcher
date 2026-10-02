#include "VideoDialog.hpp"

#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>

VideoDialog::VideoDialog(const QString &title, const QString &resourcePath,
                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    resize(880, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));
    root->addWidget(titleLabel);

    m_video = new QVideoWidget(this);
    m_video->setMinimumHeight(420);
    root->addWidget(m_video, 1);

    auto *controls = new QHBoxLayout;
    m_playPause = new QPushButton(QStringLiteral("❚❚"), this);
    m_playPause->setObjectName(QStringLiteral("dialogBtnSecondary"));
    m_playPause->setFixedWidth(56);
    m_playPause->setCursor(Qt::PointingHandCursor);
    controls->addWidget(m_playPause);

    m_seek = new QSlider(Qt::Horizontal, this);
    m_seek->setRange(0, 0);
    controls->addWidget(m_seek, 1);
    root->addLayout(controls);

    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_video);
    m_audio->setVolume(0.7f);
    m_player->setSource(QUrl(QStringLiteral("qrc") + resourcePath));

    connect(m_playPause, &QPushButton::clicked, this, [this] {
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
            m_playPause->setText(QStringLiteral("▶"));
        } else {
            m_player->play();
            m_playPause->setText(QStringLiteral("❚❚"));
        }
    });
    connect(m_player, &QMediaPlayer::durationChanged, m_seek, &QSlider::setMaximum);
    connect(m_player, &QMediaPlayer::positionChanged, m_seek, &QSlider::setValue);
    connect(m_seek, &QSlider::sliderMoved, m_player, &QMediaPlayer::setPosition);

    m_player->play();
}

VideoDialog::~VideoDialog()
{
    m_player->stop();
}
