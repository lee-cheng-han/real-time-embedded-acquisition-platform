#ifndef DRATS_STATUS_H
#define DRATS_STATUS_H

typedef enum {
    DRATS_OK = 0,
    DRATS_E_ARGUMENT = -1,
    DRATS_E_STATE = -2,
    DRATS_E_FULL = -3,
    DRATS_E_EMPTY = -4,
    DRATS_E_STALE = -5,
    DRATS_E_SIZE = -6,
    DRATS_E_CRC = -7,
    DRATS_E_FORMAT = -8,
    DRATS_E_VERSION = -9,
    DRATS_E_AUTH = -10,
    DRATS_E_TIMEOUT = -11,
    DRATS_E_IO = -12
} drats_status_t;

#endif
