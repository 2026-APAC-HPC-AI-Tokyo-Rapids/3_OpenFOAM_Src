#ifndef issue33Trace_H
#define issue33Trace_H

// Diagnostic hook supplied by the ASPIRE-2A PMPI profiler when preloaded.
// A missing hook has no effect on the numerical computation.
extern "C" void openfoam_issue33_phase_marker(const char*, const char*) __attribute__((weak));

inline void issue33TraceMark(const char* phase, const char* field = nullptr)
{
    if (openfoam_issue33_phase_marker)
    {
        openfoam_issue33_phase_marker(phase, field);
    }
}

#endif
