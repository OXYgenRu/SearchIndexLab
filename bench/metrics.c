#include "metrics.h"

#ifdef _WIN32

#include <windows.h>
#include <psapi.h>

double getCurrentTimeMs(void) {
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return (double) counter.QuadPart * 1000.0 / (double) frequency.QuadPart;
}

long getPeakMemoryKb(void) {
    PROCESS_MEMORY_COUNTERS info;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
        return -1;
    }
    return (long) (info.PeakWorkingSetSize / 1024);
}

#else

#include <time.h>
#include <sys/resource.h>

double getCurrentTimeMs(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return (double) clock() * 1000.0 / (double) CLOCKS_PER_SEC;
    }
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1.0e6;
}

long getPeakMemoryKb(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
    /* ru_maxrss: Linux — кБ, macOS/BSD — байты */
#ifdef __APPLE__
    return (long) (usage.ru_maxrss / 1024);
#else
    return (long) usage.ru_maxrss;
#endif
}

#endif
