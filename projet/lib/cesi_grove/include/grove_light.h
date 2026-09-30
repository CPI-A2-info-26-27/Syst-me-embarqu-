#ifndef GROVE_LIGHT_H
#define GROVE_LIGHT_H

#include <stdint.h>
#include <stdbool.h>

void GroveLight_Init(void);
uint16_t GroveLight_ReadRaw(void);
bool GroveLight_ReadWithTimeout(uint16_t *valeur, uint32_t timeout_ms);

#endif
