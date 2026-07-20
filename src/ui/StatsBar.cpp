#include "ui/StatsBar.h"

#include <QHBoxLayout>
#include <QLabel>

StatsBar::StatsBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 3, 10, 3);
    layout->setSpacing(18);

    m_cpuLabel = new QLabel(QStringLiteral("CPU —"), this);
    m_gpuLabel = new QLabel(QStringLiteral("GPU —"), this);
    m_ramLabel = new QLabel(QStringLiteral("RAM —"), this);
    m_fpsLabel = new QLabel(QStringLiteral("FPS —"), this);
    m_showLabel = new QLabel(QStringLiteral(""), this);

    const QString mono = QStringLiteral(
        "color: #8b95a8; font-size: 11px; font-family: 'SF Mono', 'Menlo', 'Consolas', monospace;");
    for (QLabel *l : {m_cpuLabel, m_gpuLabel, m_ramLabel, m_fpsLabel}) {
        l->setStyleSheet(mono);
        layout->addWidget(l);
    }
    layout->addStretch();
    m_showLabel->setStyleSheet(QStringLiteral(
        "color: #c4ccd8; font-size: 11px; font-family: 'Avenir Next', 'Helvetica Neue', sans-serif;"));
    layout->addWidget(m_showLabel);
}

void StatsBar::setStats(double cpuPercent, double gpuPercent, double ramUsedMb, double ramTotalMb,
                        double fps)
{
    m_cpuLabel->setText(QStringLiteral("CPU %1%").arg(cpuPercent, 0, 'f', 0));
    if (gpuPercent < 0)
        m_gpuLabel->setText(QStringLiteral("GPU n/a"));
    else
        m_gpuLabel->setText(QStringLiteral("GPU %1%").arg(gpuPercent, 0, 'f', 0));
    m_ramLabel->setText(QStringLiteral("RAM %1 / %2 MB")
                            .arg(ramUsedMb, 0, 'f', 0)
                            .arg(ramTotalMb, 0, 'f', 0));
    m_fpsLabel->setText(QStringLiteral("FPS %1").arg(fps, 0, 'f', 1));
}

void StatsBar::setShowStatus(const QString &status)
{
    m_showLabel->setTextFormat(Qt::RichText);
    m_showLabel->setText(status);
}
