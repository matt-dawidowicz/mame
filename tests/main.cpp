#if defined(__linux__)
// Catch2 1.7 declares its alternate signal stack as a fixed-size array, while
// modern glibc exposes SIGSTKSZ dynamically. Keep this compatibility override
// local to the legacy test entry point.
#include <signal.h>
#undef SIGSTKSZ
#define SIGSTKSZ (64 * 1024)
#endif

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
