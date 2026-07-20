#include "ui/VideoPreviewWidget.h"

#include <QPainter>

VideoPreviewWidget::VideoPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(120, 68);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoPreviewWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void VideoPreviewWidget::clearFrame()
{
    m_frame = QImage();
    update();
}

void VideoPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 20, 24));

    if (m_frame.isNull()) {
        p.setPen(QColor(80, 88, 100));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    QSize target = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    QRect dest((width() - target.width()) / 2, (height() - target.height()) / 2, target.width(),
               target.height());
    p.drawImage(dest, m_frame);
}
