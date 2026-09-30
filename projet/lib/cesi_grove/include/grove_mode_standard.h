#ifndef GROVE_MODE_STANDARD_H
#define GROVE_MODE_STANDARD_H

#include <stdbool.h>

void mode_standard_init(int LOG_INTERVAL, int TIMEOUT);
void mode_standard_update(void);
void mode_standard_acquisition(int TIMEOUT, bool avec_gps, bool avec_sd);

#endif
