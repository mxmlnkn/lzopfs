#pragma once

#include <cstdio>

#ifndef NDEBUG
    #define DEBUG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG(fmt, ...) (void)0;
#endif
