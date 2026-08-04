/* shared_protocol.c - shared between main MCU and supervisor builds */
#include "shared_protocol.h"

uint8_t proto_checksum(const uint8_t *bytes, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return sum;
}
