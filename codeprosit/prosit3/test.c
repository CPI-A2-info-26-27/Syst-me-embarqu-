/* Minimal bare-metal blinky (STM32F4-style, LED on PA5). No libraries. */

#define RCC_AHB1ENR (*(volatile unsigned int *)0x40023830)
#define GPIOA_MODER (*(volatile unsigned int *)0x40020000)
#define GPIOA_ODR   (*(volatile unsigned int *)0x40020014)

extern unsigned int _estack;

int main(void)
{
    RCC_AHB1ENR |= 1;                                   /* clock GPIOA */
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10); /* PA5 output */

    while (1) {
        GPIOA_ODR ^= (1u << 5);
        for (volatile int i = 0; i < 500000; i++);
    }
}

void Reset_Handler(void)
{
    main();
}

/* Vector table: initial stack pointer + reset handler */
__attribute__((section(".vectors"), used))
void (*const vectors[])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
};
