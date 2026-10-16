#ifndef NIMCP_EXPORT_H
#define NIMCP_EXPORT_H

#if defined _WIN32 || defined __CYGWIN__
    #ifdef BUILDING_NIMCP_CORE
        #define NIMCP_EXPORT __declspec(dllexport)
    #else
        #define NIMCP_EXPORT __declspec(dllimport)
    #endif
#else
    #ifdef BUILDING_NIMCP_CORE
        #define NIMCP_EXPORT __attribute__((visibility("default")))
    #else
        #define NIMCP_EXPORT
    #endif
#endif

#endif  // NIMCP_EXPORT_H
