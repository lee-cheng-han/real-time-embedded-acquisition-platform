#ifndef DRATS_TEST_SUPPORT_H
#define DRATS_TEST_SUPPORT_H

#include <stdio.h>

#define CHECK(condition) do {                                                   \
    if (!(condition)) {                                                         \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,      \
                #condition);                                                    \
        return 1;                                                               \
    }                                                                           \
} while (0)

#endif
