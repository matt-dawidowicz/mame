#if defined(__linux__)
#include <signal.h>

/*
 * Catch v1.7.0 assumes SIGSTKSZ is an integer constant expression.
 * Modern glibc may provide it dynamically, which makes Catch's static
 * alternate signal-stack array ill-formed.  Keep this compatibility
 * workaround local to the native test runner.
 */
#if defined(__GLIBC__) && defined(SIGSTKSZ)
#undef SIGSTKSZ
#define SIGSTKSZ (256 * 1024)
#endif
#endif

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
