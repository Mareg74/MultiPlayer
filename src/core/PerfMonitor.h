#pragma once

class PerfMonitor
{
public:
    PerfMonitor();
    void update();

    double cpuPercent() const { return m_cpu; }
    double gpuPercent() const { return m_gpu; }
    double ramUsedMb() const { return m_ramUsed; }
    double ramTotalMb() const { return m_ramTotal; }

private:
    double m_cpu = 0.0;
    double m_gpu = 0.0;
    double m_ramUsed = 0.0;
    double m_ramTotal = 0.0;

#if defined(__APPLE__)
    unsigned long long m_prevTotal = 0;
    unsigned long long m_prevIdle = 0;
#endif
};
