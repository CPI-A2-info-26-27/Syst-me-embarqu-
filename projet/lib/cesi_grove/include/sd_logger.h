#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

#ifndef SDLOGGER_FILE_MAX_SIZE_DEFAULT
#define SDLOGGER_FILE_MAX_SIZE_DEFAULT 2048U
#endif

typedef enum
{
	SDLOGGER_CARD_UNKNOWN = 0,
	SDLOGGER_CARD_SDSC,
	SDLOGGER_CARD_SDHC
} SDLoggerCardType;

typedef enum
{
	SDLOGGER_INIT_OK = 0,
	SDLOGGER_INIT_ERR_CMD0,
	SDLOGGER_INIT_ERR_CMD8_PATTERN,
	SDLOGGER_INIT_ERR_ACMD41_TIMEOUT,
	SDLOGGER_INIT_ERR_CMD58,
	SDLOGGER_INIT_ERR_CMD1_TIMEOUT
} SDLoggerInitStatus;

bool SDLogger_Init(void);
SDLoggerCardType SDLogger_GetCardType(void);
SDLoggerInitStatus SDLogger_GetLastInitStatus(void);
const char *SDLogger_GetActiveCsLabel(void);
bool SDLogger_BitBangCMD0Test(void);
bool SDLogger_WriteLine(const char *filename, const char *line);
void SDLogger_SetMaxFileSize(uint32_t max_size);
uint32_t SDLogger_GetMaxFileSize(void);
bool SDLogger_IsCardFull(void);
void SDLogger_Unmount(void);

#endif
