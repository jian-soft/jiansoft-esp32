#include <stdio.h>
#include "utils.h"


/*
    如果new_value没有回绕，old - new自然小于0，自然old < new
    如果new_value回绕了，比如 old-new 为 0xFFFF-1 = 0xFFFE = -2 < 0, 则 old < new
*/
int16_t utils_u16_diff(uint16_t new_value, uint16_t old_value)
{
    return (int16_t)(new_value - old_value);
}

int32_t utils_u32_diff(uint32_t new_value, uint32_t old_value)
{
    return (int32_t)(new_value - old_value);
}

void utils_dump_pkt(uint8_t *buffer, uint32_t len)
{
    int i;
    int dump_len = len > 32 ? 32 : len;
    for (i = 0; i < dump_len; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    if (i % 16 != 0) {
        printf("\n");
    }
    printf("\n");
}

