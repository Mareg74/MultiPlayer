#include "media/PlayerModule.h"

#include <QtGlobal>

QString PlayerModule::defaultNameForIndex(int index)
{
    return QStringLiteral("M%1").arg(index + 1);
}

PlayerModule::PlayerModule(int index, QObject *parent)
    : QObject(parent)
    , m_index(index)
    , m_name(defaultNameForIndex(index))
    , m_decoder(std::make_unique<Decoder>())
{
    // Placement initial : décalé autour du centre du canvas (pas de grille fixe).
    const float ox = static_cast<float>((index % 5) - 2) * 72.f;
    const float oy = static_cast<float>((index / 5)) * 72.f;
    m_transform.posX = 960.f + ox;
    m_transform.posY = 540.f + oy;
    m_transform.scale = 0.35f;
    m_transform.scaleY = 0.35f;

    connect(m_decoder.get(), &Decoder::frameReady, this, [this]() {
        m_decoder->takeFrame(m_cachedFrame);
        emit frameUpdated();
    });
    connect(m_decoder.get(), &Decoder::positionChanged, this, &PlayerModule::positionChanged);
    connect(m_decoder.get(), &Decoder::opened, this, [this](auto...) { emit stateChanged(); });
    connect(m_decoder.get(), &Decoder::ended, this, [this]() { emit stateChanged(); });
    connect(m_decoder.get(), &Decoder::errorOccurred, this, [this](const QString &msg) {
        m_lastError = msg;
        emit stateChanged();
        emit errorOccurred(msg);
    });
}

void PlayerModule::setIndex(int index)
{
    const QString oldDefault = defaultNameForIndex(m_index);
    m_index = index;
    if (m_name.isEmpty() || m_name == oldDefault)
        m_name = defaultNameForIndex(index);
}

void PlayerModule::setName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_name = trimmed.isEmpty() ? defaultNameForIndex(m_index) : trimmed;
}

bool PlayerModule::load(const QString &path)
{
    m_lastError.clear();
    const bool ok = m_decoder->open(path);
    if (!ok && m_lastError.isEmpty())
        m_lastError = QStringLiteral("Échec chargement");
    emit stateChanged();
    return ok;
}

void PlayerModule::play()
{
    m_decoder->play();
    emit stateChanged();
}

void PlayerModule::pause()
{
    m_decoder->pause();
    emit stateChanged();
}

void PlayerModule::stop()
{
    m_decoder->stop();
    emit stateChanged();
}

void PlayerModule::togglePlayPause()
{
    if (m_decoder->isPlaying())
        pause();
    else
        play();
}

QImage PlayerModule::currentFrame() const
{
    return m_cachedFrame;
}
