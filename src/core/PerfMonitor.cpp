#include "core/PerfMonitor.h"

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

PerfMonitor::PerfMonitor()
{
    update();
}

void PerfMonitor::update()
{
#if defined(__APPLE__)
    // RAM
    int64_t memSize = 0;
    size_t len = sizeof(memSize);
    sysctlbyname("hw.memsize", &memSize, &len, nullptr, 0);
    m_ramTotal = memSize / (1024.0 * 1024.0);

    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                  &count)
        == KERN_SUCCESS) {
        m_ramUsed = info.resident_size / (1024.0 * 1024.0);
    }

    // CPU (host load average approximation via host_statistics)
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t countCpu = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info_t>(&cpuinfo), &countCpu)
        == KERN_SUCCESS) {
        unsigned long long total = 0;
        for (int i = 0; i < CPU_STATE_MAX; ++i)
            total += cpuinfo.cpu_ticks[i];
        const unsigned long long idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
        if (m_prevTotal > 0) {
            const double totalDelta = static_cast<double>(total - m_prevTotal);
            const double idleDelta = static_cast<double>(idle - m_prevIdle);
            if (totalDelta > 0)
                m_cpu = (1.0 - idleDelta / totalDelta) * 100.0;
        }
        m_prevTotal = total;
        m_prevIdle = idle;
    }
    m_gpu = -1.0; // Requires Metal counters — shown as N/A in UI when < 0
#elif defined(_WIN32)
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        m_ramTotal = mem.ullTotalPhys / (1024.0 * 1024.0);
        m_ramUsed = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0 * 1024.0);
    }

    FILETIME idleTime, kernelTime, userTime;
    static ULARGE_INTEGER prevIdle{}, prevKernel{}, prevUser{};
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        const ULONGLONG idleDiff = idle.QuadPart - prevIdle.QuadPart;
        const ULONGLONG kernelDiff = kernel.QuadPart - prevKernel.QuadPart;
        const ULONGLONG userDiff = user.QuadPart - prevUser.QuadPart;
        const ULONGLONG total = kernelDiff + userDiff;
        if (total > 0)
            m_cpu = (1.0 - (static_cast<double>(idleDiff) / static_cast<double>(total))) * 100.0;
        prevIdle = idle;
        prevKernel = kernel;
        prevUser = user;
    }
    m_gpu = -1.0;
#else
    m_cpu = 0;
    m_gpu = -1;
    m_ramUsed = 0;
    m_ramTotal = 0;
#endif
}
