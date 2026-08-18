#if defined(__linux__)
#include <signal.h>
#undef SIGSTKSZ
#define SIGSTKSZ (64 * 1024)
#endif

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
