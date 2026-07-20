#include "output/SpoutSender.h"

#include <QImage>
#include <QString>

// mac branch: Spout is Windows-only — stub implementation.

SpoutSender::SpoutSender() = default;

SpoutSender::~SpoutSender()
{
    stop();
}

bool SpoutSender::isAvailable() const
{
    return false;
}

bool SpoutSender::start(const QString &name, int width, int height)
{
    Q_UNUSED(name);
    Q_UNUSED(width);
    Q_UNUSED(height);
    return true;
}

void SpoutSender::stop() {}

bool SpoutSender::send(const QImage &frame)
{
    Q_UNUSED(frame);
    return true;
}
