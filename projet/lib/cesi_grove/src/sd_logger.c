#include "sd_logger.h"
#include "board_pins.h"
#include "stm32l4xx_hal.h"
#include "grove_rtc_ds1307.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include <stdio.h>
#include <string.h>

#define SD_CMD0     0
#define SD_CMD1     1
#define SD_CMD8     8
#define SD_CMD55    55
#define SD_CMD58    58
#define SD_ACMD41   41

#define SD_R1_IDLE_STATE      0x01U
#define SD_R1_ILLEGAL_COMMAND 0x04U

#define SD_INIT_TIMEOUT_MS 1000U
#define SD_POWERUP_DELAY_MS 50U

#define SD_CMD9     9
#define SD_CMD12    12
#define SD_CMD16    16
#define SD_CMD17    17
#define SD_CMD18    18
#define SD_CMD24    24
#define SD_CMD25    25

#define SD_BLOCK_SIZE           512U
#define SD_TOKEN_START_BLOCK    0xFEU
#define SD_TOKEN_START_MULTI    0xFCU
#define SD_TOKEN_STOP_TRAN      0xFDU
#define SD_DATA_RESP_MASK       0x1FU
#define SD_DATA_RESP_ACCEPTED   0x05U
#define SD_READ_TIMEOUT_MS      200U
#define SD_WRITE_TIMEOUT_MS     500U

#define SD_SPI_SLOW_PRESCALER   SPI_BAUDRATEPRESCALER_256
#define SD_SPI_FAST_MAX_HZ      10000000UL

#define SD_LOG_NAME_SIZE        32U
#define SD_LOG_MAX_REVISION     9999U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    const char *label;
} SDChipSelect;

extern SPI_HandleTypeDef SD_SPI_HANDLE;

static SDLoggerCardType g_card_type = SDLOGGER_CARD_UNKNOWN;
static SDLoggerInitStatus g_init_status = SDLOGGER_INIT_ERR_CMD0;
static uint8_t g_active_cs_index = 0;

static DSTATUS g_disk_status = STA_NOINIT;
static FATFS g_fatfs;
static FIL g_fichier;
static bool g_monte = false;
static bool g_carte_pleine = false;
static uint32_t g_max_file_size = SDLOGGER_FILE_MAX_SIZE_DEFAULT;

static const SDChipSelect g_cs_candidates[] = {
    {SD_CS_PORT, SD_CS_PIN, "D4/PB5"}
};

#define SD_CS_CANDIDATE_COUNT ((uint8_t)(sizeof(g_cs_candidates) / sizeof(g_cs_candidates[0])))

static uint8_t SD_SPI_Transfer(uint8_t tx)
{
    uint8_t rx = 0xFF;
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &tx, &rx, 1, 100);
    return rx;
}

static void SD_Deselect(void)
{
    HAL_GPIO_WritePin(g_cs_candidates[g_active_cs_index].port,
                      g_cs_candidates[g_active_cs_index].pin,
                      GPIO_PIN_SET);
    (void)SD_SPI_Transfer(0xFF);
}

static void SD_Select(void)
{
    HAL_GPIO_WritePin(g_cs_candidates[g_active_cs_index].port,
                      g_cs_candidates[g_active_cs_index].pin,
                      GPIO_PIN_RESET);
}

static void SD_SetActiveChipSelect(uint8_t index)
{
    uint8_t i;

    g_active_cs_index = index;
    for (i = 0; i < SD_CS_CANDIDATE_COUNT; ++i)
    {
        HAL_GPIO_WritePin(g_cs_candidates[i].port, g_cs_candidates[i].pin, GPIO_PIN_SET);
    }
}

static bool SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (SD_SPI_Transfer(0xFF) == 0xFF)
        {
            return true;
        }
    }

    return false;
}

static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response = 0xFF;
    uint8_t frame[6];
    uint8_t i;

    SD_Deselect();
    SD_Select();

    if (cmd != SD_CMD0)
    {
        if (!SD_WaitReady(50U))
        {
            SD_Deselect();
            return 0xFF;
        }
    }

    frame[0] = (uint8_t)(0x40U | cmd);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);
    frame[5] = crc;

    for (i = 0; i < 6; ++i)
    {
        (void)SD_SPI_Transfer(frame[i]);
    }

    for (i = 0; i < 10; ++i)
    {
        response = SD_SPI_Transfer(0xFF);
        if ((response & 0x80U) == 0U)
        {
            return response;
        }
    }

    return response;
}

bool SDLogger_Init(void)
{
    uint8_t cs_index;
    uint8_t i;
    uint8_t r1;
    uint8_t r7[4];
    uint8_t ocr[4];
    uint32_t start;
    uint8_t cmd0_try;
    uint8_t idle_samples[8];
    uint8_t cmd0_samples[8];
    uint8_t sample_count;

    g_card_type = SDLOGGER_CARD_UNKNOWN;
    g_init_status = SDLOGGER_INIT_ERR_CMD0;

    for (cs_index = 0; cs_index < SD_CS_CANDIDATE_COUNT; ++cs_index)
    {
        SD_SetActiveChipSelect(cs_index);
        SD_Deselect();

        printf("[SD diag] try CS %s\r\n", g_cs_candidates[cs_index].label);

        HAL_Delay(SD_POWERUP_DELAY_MS);

        /* Send at least 74 clocks with CS high as mandated by SD SPI mode. */
        for (i = 0; i < 10; ++i)
        {
            (void)SD_SPI_Transfer(0xFF);
        }

        for (i = 0; i < 8; ++i)
        {
            idle_samples[i] = SD_SPI_Transfer(0xFF);
        }
        printf("[SD diag] MISO idle bytes: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               idle_samples[0], idle_samples[1], idle_samples[2], idle_samples[3],
               idle_samples[4], idle_samples[5], idle_samples[6], idle_samples[7]);

        r1 = 0xFFU;
        sample_count = 0U;
        for (cmd0_try = 0; cmd0_try < 32U; ++cmd0_try)
        {
            r1 = SD_SendCommand(SD_CMD0, 0x00000000UL, 0x95U);
            SD_Deselect();

            if (sample_count < 8U)
            {
                cmd0_samples[sample_count] = r1;
                ++sample_count;
            }

            if (r1 == SD_R1_IDLE_STATE)
            {
                break;
            }

            HAL_Delay(2U);
        }

        if (sample_count > 0U)
        {
            printf("[SD diag] CMD0 R1 first bytes:");
            for (i = 0; i < sample_count; ++i)
            {
                printf(" %02X", cmd0_samples[i]);
            }
            printf("\r\n");
        }

        if (r1 != SD_R1_IDLE_STATE)
        {
            g_init_status = SDLOGGER_INIT_ERR_CMD0;
            printf("[SD diag] CMD0 failed on %s\r\n", g_cs_candidates[cs_index].label);
            continue;
        }

        r1 = SD_SendCommand(SD_CMD8, 0x000001AAUL, 0x87U);
        if (r1 == SD_R1_IDLE_STATE)
        {
            for (i = 0; i < 4; ++i)
            {
                r7[i] = SD_SPI_Transfer(0xFF);
            }
            SD_Deselect();

            if ((r7[2] != 0x01U) || (r7[3] != 0xAAU))
            {
                g_init_status = SDLOGGER_INIT_ERR_CMD8_PATTERN;
                continue;
            }

            start = HAL_GetTick();
            do
            {
                r1 = SD_SendCommand(SD_CMD55, 0x00000000UL, 0x01U);
                SD_Deselect();
                if (r1 > 0x01U)
                {
                    g_init_status = SDLOGGER_INIT_ERR_ACMD41_TIMEOUT;
                    break;
                }

                r1 = SD_SendCommand(SD_ACMD41, 0x40000000UL, 0x01U);
                SD_Deselect();
                if (r1 == 0x00U)
                {
                    break;
                }
            }
            while ((HAL_GetTick() - start) < SD_INIT_TIMEOUT_MS);

            if (r1 != 0x00U)
            {
                g_init_status = SDLOGGER_INIT_ERR_ACMD41_TIMEOUT;
                continue;
            }

            r1 = SD_SendCommand(SD_CMD58, 0x00000000UL, 0x01U);
            if (r1 != 0x00U)
            {
                SD_Deselect();
                g_init_status = SDLOGGER_INIT_ERR_CMD58;
                continue;
            }

            for (i = 0; i < 4; ++i)
            {
                ocr[i] = SD_SPI_Transfer(0xFF);
            }
            SD_Deselect();

            g_card_type = ((ocr[0] & 0x40U) != 0U) ? SDLOGGER_CARD_SDHC : SDLOGGER_CARD_SDSC;
            g_init_status = SDLOGGER_INIT_OK;
            return true;
        }

        if ((r1 & SD_R1_ILLEGAL_COMMAND) == 0U)
        {
            SD_Deselect();
            g_init_status = SDLOGGER_INIT_ERR_CMD8_PATTERN;
            continue;
        }

        SD_Deselect();

        start = HAL_GetTick();
        do
        {
            r1 = SD_SendCommand(SD_CMD1, 0x00000000UL, 0x01U);
            SD_Deselect();
            if (r1 == 0x00U)
            {
                g_card_type = SDLOGGER_CARD_SDSC;
                g_init_status = SDLOGGER_INIT_OK;
                return true;
            }
        }
        while ((HAL_GetTick() - start) < SD_INIT_TIMEOUT_MS);

        g_init_status = SDLOGGER_INIT_ERR_CMD1_TIMEOUT;
    }

    return false;
}

SDLoggerCardType SDLogger_GetCardType(void)
{
    return g_card_type;
}

SDLoggerInitStatus SDLogger_GetLastInitStatus(void)
{
    return g_init_status;
}

const char *SDLogger_GetActiveCsLabel(void)
{
    return g_cs_candidates[g_active_cs_index].label;
}

/* ---------- bit-bang diagnostic ----------------------------------------- */

static void SD_ReconfigPinsGPIO(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Disable SPI1 peripheral while we take over the pins */
    SD_SPI_HANDLE.Instance->CR1 &= ~SPI_CR1_SPE;

    /* PA5 = SCK, PA7 = MOSI: push-pull outputs */
    gpio.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); /* SCK idle low  */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   /* MOSI idle high */

    /* PA6 = MISO: input with pull-up */
    gpio.Pin  = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void SD_ReconfigPinsSPI(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_6;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    SD_SPI_HANDLE.Instance->CR1 |= SPI_CR1_SPE;
}

static uint8_t SD_BitBangByte(uint8_t tx)
{
    uint8_t rx = 0;
    uint8_t bit;

    for (bit = 0; bit < 8U; ++bit)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7,
                          (tx & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        tx <<= 1;
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   /* rising edge  */
        rx <<= 1;
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET)
        {
            rx |= 1U;
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); /* falling edge */
    }

    return rx;
}

bool SDLogger_BitBangCMD0Test(void)
{
    uint8_t i;
    uint8_t r1 = 0xFFU;
    GPIO_PinState cs_rb;

    SD_ReconfigPinsGPIO();

    /* --- GPIO self-test: verify CS output and MISO input are accessible --- */
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    cs_rb = HAL_GPIO_ReadPin(SD_CS_PORT, SD_CS_PIN);
    printf("[SD bitbang] CS readback HIGH: %d (expected 1)\r\n", cs_rb);

    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
    cs_rb = HAL_GPIO_ReadPin(SD_CS_PORT, SD_CS_PIN);
    printf("[SD bitbang] CS readback LOW:  %d (expected 0)\r\n", cs_rb);

    /* Raw IDR dump: GPIOA bits 5(SCK) 6(MISO) 7(MOSI), GPIOB bit 5(CS) */
    {
        uint32_t idr_a = GPIOA->IDR;
        uint32_t idr_b = GPIOB->IDR;
        printf("[SD bitbang] GPIOA IDR: SCK(5)=%lu MISO(6)=%lu MOSI(7)=%lu\r\n",
               (idr_a >> 5U) & 1U, (idr_a >> 6U) & 1U, (idr_a >> 7U) & 1U);
        printf("[SD bitbang] GPIOB IDR: CS(5)=%lu\r\n", (idr_b >> 5U) & 1U);
    }

    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    HAL_Delay(10U);

    /* --- 80 init clocks with CS high --- */
    for (i = 0; i < 10U; ++i)
    {
        (void)SD_BitBangByte(0xFFU);
    }

    /* MISO state just before CMD0 */
    printf("[SD bitbang] MISO before CMD0: %d\r\n",
           HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6));

    /* CS low, send CMD0 */
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
    (void)SD_BitBangByte(0x40U);
    (void)SD_BitBangByte(0x00U);
    (void)SD_BitBangByte(0x00U);
    (void)SD_BitBangByte(0x00U);
    (void)SD_BitBangByte(0x00U);
    (void)SD_BitBangByte(0x95U);

    printf("[SD bitbang] CMD0 responses:");
    for (i = 0; i < 10U; ++i)
    {
        r1 = SD_BitBangByte(0xFFU);
        printf(" %02X", r1);
        if ((r1 & 0x80U) == 0U)
        {
            break;
        }
    }
    printf("\r\n");

    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    (void)SD_BitBangByte(0xFFU);

    SD_ReconfigPinsSPI();

    if (r1 == 0x01U)
    {
        printf("[SD bitbang] CMD0 OK -> SPI hardware config issue\r\n");
        return true;
    }

    printf("[SD bitbang] CMD0 still failed -> physical wiring/shield issue\r\n");
    return false;
}

static void SD_SetSpiPrescaler(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(&SD_SPI_HANDLE);
    MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_CR1_BR, prescaler);
    SD_SPI_HANDLE.Init.BaudRatePrescaler = prescaler;
    __HAL_SPI_ENABLE(&SD_SPI_HANDLE);
}

static uint32_t SD_GetFastPrescaler(void)
{
    static const uint32_t prescalers[] = {
        SPI_BAUDRATEPRESCALER_2,  SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8,  SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256
    };
    uint32_t pclk = HAL_RCC_GetPCLK2Freq();
    uint8_t i;

    for (i = 0; i < 8U; ++i)
    {
        if ((pclk >> (i + 1U)) <= SD_SPI_FAST_MAX_HZ)
        {
            return prescalers[i];
        }
    }

    return SPI_BAUDRATEPRESCALER_256;
}

static bool SD_FinishInit(void)
{
    uint8_t r1;

    if (g_card_type == SDLOGGER_CARD_SDSC)
    {
        r1 = SD_SendCommand(SD_CMD16, SD_BLOCK_SIZE, 0x01U);
        SD_Deselect();
        if (r1 != 0x00U)
        {
            return false;
        }
    }

    SD_SetSpiPrescaler(SD_GetFastPrescaler());
    return true;
}

static uint32_t SD_SectorAddress(DWORD sector)
{
    if (g_card_type == SDLOGGER_CARD_SDHC)
    {
        return (uint32_t)sector;
    }

    return (uint32_t)sector * SD_BLOCK_SIZE;
}

static bool SD_ReceiveDataBlock(uint8_t *buffer, uint32_t len)
{
    uint8_t token;
    uint32_t i;
    uint32_t start = HAL_GetTick();

    do
    {
        token = SD_SPI_Transfer(0xFF);
    }
    while ((token == 0xFFU) && ((HAL_GetTick() - start) < SD_READ_TIMEOUT_MS));

    if (token != SD_TOKEN_START_BLOCK)
    {
        return false;
    }

    for (i = 0; i < len; ++i)
    {
        buffer[i] = SD_SPI_Transfer(0xFF);
    }

    (void)SD_SPI_Transfer(0xFF);
    (void)SD_SPI_Transfer(0xFF);

    return true;
}

static bool SD_SendDataBlock(const uint8_t *buffer, uint8_t token)
{
    uint8_t response;
    uint32_t i;

    if (!SD_WaitReady(SD_WRITE_TIMEOUT_MS))
    {
        return false;
    }

    (void)SD_SPI_Transfer(token);

    for (i = 0; i < SD_BLOCK_SIZE; ++i)
    {
        (void)SD_SPI_Transfer(buffer[i]);
    }

    (void)SD_SPI_Transfer(0xFF);
    (void)SD_SPI_Transfer(0xFF);

    response = SD_SPI_Transfer(0xFF);
    return (response & SD_DATA_RESP_MASK) == SD_DATA_RESP_ACCEPTED;
}

static void SD_StopTransmission(void)
{
    uint8_t frame[6] = {(uint8_t)(0x40U | SD_CMD12), 0x00U, 0x00U, 0x00U, 0x00U, 0x01U};
    uint8_t i;

    for (i = 0; i < 6U; ++i)
    {
        (void)SD_SPI_Transfer(frame[i]);
    }

    (void)SD_SPI_Transfer(0xFF);

    for (i = 0; i < 10U; ++i)
    {
        if ((SD_SPI_Transfer(0xFF) & 0x80U) == 0U)
        {
            break;
        }
    }

    (void)SD_WaitReady(SD_WRITE_TIMEOUT_MS);
}

static bool SD_ReadCsd(uint8_t csd[16])
{
    bool ok = false;

    if (SD_SendCommand(SD_CMD9, 0x00000000UL, 0x01U) == 0x00U)
    {
        ok = SD_ReceiveDataBlock(csd, 16U);
    }
    SD_Deselect();

    return ok;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0U)
    {
        return STA_NOINIT;
    }

    return g_disk_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0U)
    {
        return STA_NOINIT;
    }

    if ((g_disk_status & STA_NOINIT) != 0U)
    {
        SD_SetSpiPrescaler(SD_SPI_SLOW_PRESCALER);
        if (SDLogger_Init() && SD_FinishInit())
        {
            g_disk_status = 0U;
        }
    }

    return g_disk_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint32_t address;

    if ((pdrv != 0U) || (count == 0U))
    {
        return RES_PARERR;
    }
    if ((g_disk_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    address = SD_SectorAddress(sector);

    if (count == 1U)
    {
        if ((SD_SendCommand(SD_CMD17, address, 0x01U) == 0x00U) &&
            SD_ReceiveDataBlock(buff, SD_BLOCK_SIZE))
        {
            count = 0U;
        }
    }
    else
    {
        if (SD_SendCommand(SD_CMD18, address, 0x01U) == 0x00U)
        {
            do
            {
                if (!SD_ReceiveDataBlock(buff, SD_BLOCK_SIZE))
                {
                    break;
                }
                buff += SD_BLOCK_SIZE;
            }
            while (--count > 0U);

            SD_StopTransmission();
        }
    }

    SD_Deselect();

    return (count == 0U) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    uint32_t address;
    bool ok = false;

    if ((pdrv != 0U) || (count == 0U))
    {
        return RES_PARERR;
    }
    if ((g_disk_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    address = SD_SectorAddress(sector);

    if (count == 1U)
    {
        ok = (SD_SendCommand(SD_CMD24, address, 0x01U) == 0x00U) &&
             SD_SendDataBlock(buff, SD_TOKEN_START_BLOCK);
    }
    else
    {
        if (SD_SendCommand(SD_CMD25, address, 0x01U) == 0x00U)
        {
            do
            {
                if (!SD_SendDataBlock(buff, SD_TOKEN_START_MULTI))
                {
                    break;
                }
                buff += SD_BLOCK_SIZE;
            }
            while (--count > 0U);

            if (SD_WaitReady(SD_WRITE_TIMEOUT_MS))
            {
                (void)SD_SPI_Transfer(SD_TOKEN_STOP_TRAN);
                (void)SD_SPI_Transfer(0xFF);
                ok = (count == 0U);
            }
        }
    }

    if (!SD_WaitReady(SD_WRITE_TIMEOUT_MS))
    {
        ok = false;
    }
    SD_Deselect();

    return ok ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    uint8_t csd[16];
    uint32_t c_size;
    uint32_t n;
    uint32_t write_bl_len;
    bool ready;

    if (pdrv != 0U)
    {
        return RES_PARERR;
    }
    if ((g_disk_status & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
    case CTRL_SYNC:
        SD_Select();
        ready = SD_WaitReady(SD_WRITE_TIMEOUT_MS);
        SD_Deselect();
        return ready ? RES_OK : RES_ERROR;

    case GET_SECTOR_COUNT:
        if (!SD_ReadCsd(csd))
        {
            return RES_ERROR;
        }
        if ((csd[0] >> 6) == 1U)
        {
            c_size = (uint32_t)csd[9] | ((uint32_t)csd[8] << 8) | ((uint32_t)(csd[7] & 0x3FU) << 16);
            *(DWORD *)buff = (DWORD)((c_size + 1U) << 10);
        }
        else
        {
            n = (uint32_t)(csd[5] & 0x0FU)
              + ((uint32_t)(csd[10] & 0x80U) >> 7)
              + ((uint32_t)(csd[9] & 0x03U) << 1)
              + 2U;
            c_size = ((uint32_t)csd[8] >> 6)
                   | ((uint32_t)csd[7] << 2)
                   | ((uint32_t)(csd[6] & 0x03U) << 10);
            if (n < 9U)
            {
                return RES_ERROR;
            }
            *(DWORD *)buff = (DWORD)((c_size + 1U) << (n - 9U));
        }
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = (WORD)SD_BLOCK_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if (!SD_ReadCsd(csd))
        {
            return RES_ERROR;
        }
        n = (((uint32_t)(csd[10] & 0x3FU) << 1) | ((uint32_t)csd[11] >> 7)) + 1U;
        write_bl_len = ((uint32_t)(csd[12] & 0x03U) << 2) | ((uint32_t)csd[13] >> 6);
        *(DWORD *)buff = (write_bl_len >= 9U) ? (DWORD)(n << (write_bl_len - 9U)) : 1U;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    RTC_DateTime dt;

    if (!GroveRTC_GetDateTime(&dt) || (dt.month == 0U) || (dt.day == 0U))
    {
        return ((DWORD)(2020U - 1980U) << 25) | ((DWORD)1U << 21) | ((DWORD)1U << 16);
    }

    return ((DWORD)(dt.year - 1980U) << 25)
         | ((DWORD)dt.month << 21)
         | ((DWORD)dt.day << 16)
         | ((DWORD)dt.hours << 11)
         | ((DWORD)dt.minutes << 5)
         | ((DWORD)dt.seconds >> 1);
}

static FRESULT SD_FindFreeRevision(const char *filename, char *name, size_t size)
{
    const char *tiret = strrchr(filename, '_');
    const char *point = strrchr(filename, '.');
    int longueur_prefixe;
    uint32_t revision;
    FRESULT res;

    if ((tiret == 0) || (point == 0) || (point < tiret) ||
        ((size_t)(tiret - filename) + strlen(point) + 6U >= size))
    {
        return FR_INVALID_NAME;
    }

    longueur_prefixe = (int)(tiret - filename);

    for (revision = 1U; revision <= SD_LOG_MAX_REVISION; ++revision)
    {
        (void)snprintf(name, size, "%.*s_%lu%s", longueur_prefixe, filename,
                       (unsigned long)revision, point);
        res = f_stat(name, 0);
        if (res == FR_NO_FILE)
        {
            return FR_OK;
        }
        if (res != FR_OK)
        {
            return res;
        }
    }

    return FR_DENIED;
}

static FRESULT SD_Mount(void)
{
    FRESULT res;

    if (g_monte)
    {
        return FR_OK;
    }

    res = f_mount(&g_fatfs, "", 1);
    g_monte = (res == FR_OK);
    return res;
}

static bool SD_WriteFailed(FRESULT res)
{
    (void)f_close(&g_fichier);

    if ((res == FR_DISK_ERR) || (res == FR_NOT_READY) || (res == FR_INT_ERR))
    {
        g_disk_status = STA_NOINIT;
    }
    g_monte = false;

    printf("[SD] erreur FatFs %d\r\n", (int)res);
    return false;
}

void SDLogger_SetMaxFileSize(uint32_t max_size)
{
    g_max_file_size = max_size;
}

uint32_t SDLogger_GetMaxFileSize(void)
{
    return g_max_file_size;
}

bool SDLogger_IsCardFull(void)
{
    return g_carte_pleine;
}

void SDLogger_Unmount(void)
{
    (void)f_close(&g_fichier);
    (void)f_mount(0, "", 0);
    g_monte = false;
    g_disk_status = STA_NOINIT;
}

bool SDLogger_WriteLine(const char *filename, const char *line)
{
    char archive[SD_LOG_NAME_SIZE];
    FRESULT res;
    UINT written;
    UINT line_len;

    g_carte_pleine = false;

    if ((filename == 0) || (line == 0))
    {
        return false;
    }

    res = SD_Mount();
    if (res != FR_OK)
    {
        return SD_WriteFailed(res);
    }

    line_len = (UINT)strlen(line);

    res = f_open(&g_fichier, filename, FA_OPEN_APPEND | FA_WRITE);
    if (res != FR_OK)
    {
        return SD_WriteFailed(res);
    }

    if ((f_size(&g_fichier) > 0U) &&
        ((f_size(&g_fichier) + line_len + 2U) > g_max_file_size))
    {
        res = f_close(&g_fichier);
        if (res == FR_OK)
        {
            res = SD_FindFreeRevision(filename, archive, sizeof(archive));
        }
        if (res == FR_OK)
        {
            res = f_rename(filename, archive);
        }
        if (res == FR_OK)
        {
            res = f_open(&g_fichier, filename, FA_CREATE_ALWAYS | FA_WRITE);
        }
        if (res != FR_OK)
        {
            return SD_WriteFailed(res);
        }
    }

    res = f_write(&g_fichier, line, line_len, &written);
    if ((res == FR_OK) && (written != line_len))
    {
        g_carte_pleine = true;
        res = FR_DENIED;
    }
    if (res == FR_OK)
    {
        res = f_write(&g_fichier, "\r\n", 2U, &written);
        if ((res == FR_OK) && (written != 2U))
        {
            g_carte_pleine = true;
            res = FR_DENIED;
        }
    }
    if (res != FR_OK)
    {
        return SD_WriteFailed(res);
    }

    res = f_close(&g_fichier);
    if (res != FR_OK)
    {
        return SD_WriteFailed(res);
    }

    return true;
}
