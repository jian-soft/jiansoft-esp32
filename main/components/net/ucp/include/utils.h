#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>

int16_t utils_u16_diff(uint16_t new_value, uint16_t old_value);
int32_t utils_u32_diff(uint32_t new_value, uint32_t old_value);

void utils_dump_pkt(uint8_t *buffer, uint32_t len);

#define UTILS_INC(x)    ((x)++)
#define UTILS_ADD(x, a) ((x) += a)


#endif /* _UTILS_H_ */

