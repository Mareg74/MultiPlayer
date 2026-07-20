#pragma once

#include <QWidget>

class QLabel;

class StatsBar : public QWidget
{
    Q_OBJECT

public:
    explicit StatsBar(QWidget *parent = nullptr);
    void setStats(double cpuPercent, double gpuPercent, double ramUsedMb, double ramTotalMb, double fps);
    void setShowStatus(const QString &status);

private:
    QLabel *m_cpuLabel = nullptr;
    QLabel *m_gpuLabel = nullptr;
    QLabel *m_ramLabel = nullptr;
    QLabel *m_fpsLabel = nullptr;
    QLabel *m_showLabel = nullptr;
};
