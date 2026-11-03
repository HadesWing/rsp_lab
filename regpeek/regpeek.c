/* T5: 用户态裸寄存器实验 —— /dev/mem 直读 BCM2835 外设寄存器
 *
 * 目的：证明"改寄存器"这件事树莓派上完全能做。
 * 不用内核驱动、不用 STM32，mmap 物理地址后，read/write 的语义
 * 与 STM32 裸机读写 USART->SR 是同一件事，区别只是：
 *   ① 地址不同（BCM2835 外设物理基址 0x20000000，总线 0x7E000000）
 *   ② 这里 Linux 还在跑（有调度器、有 MMU 开着）
 *
 * 本实验只读不写：GPFSEL 写错一个 bit 会把串口弄丢，要写等 LED 实验。
 *
 * 寄存器依据（对应 bsp 实验讲过的三层）：
 *   GPFSEL1 = gpio 基址 + 0x04，GPIO14 功能字段在 bit 14:12
 *   UART0_FR = uart 基址 + 0x18，PL011 标志寄存器
 *     bit3 BUSY  bit4 RXFE  bit5 TXFF  bit6 RXFF  bit7 TXFE
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

/* Pi 1 外设物理基址：总线地址 0x7E000000 映射到 ARM 物理 0x20000000 */
#define PERI_BASE   0x20000000UL
#define GPIO_OFF    0x200000          /* 总线 0x7E200000 -> 物理 0x20200000 */
#define UART0_OFF   0x201000          /* 总线 0x7E201000 -> 物理 0x20201000 */

#define GPFSEL1_IDX 1                 /* 基址 + 0x04 */
#define UART_FR_IDX 6                 /* 基址 + 0x18 */

static volatile unsigned *map_peri(int fd, unsigned off)
{
	void *p = mmap(NULL, 0x1000, PROT_READ, MAP_SHARED, fd, PERI_BASE + off);
	return (p == MAP_FAILED) ? NULL : (volatile unsigned *)p;
}

int main(void)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem（需要 root）");
		return 1;
	}

	volatile unsigned *gpio = map_peri(fd, GPIO_OFF);
	volatile unsigned *uart = map_peri(fd, UART0_OFF);
	if (!gpio || !uart) {
		perror("mmap");
		return 1;
	}

	/* 读 GPIO 功能选择寄存器：GPIO14/15 现在被配成什么功能？ */
	unsigned sel1 = gpio[GPFSEL1_IDX];
	printf("GPFSEL1 = 0x%08x\n", sel1);
	printf("  GPIO14 功能字段 = %u   (4 = ALT0 = TXD0)\n", (sel1 >> 12) & 7);
	printf("  GPIO15 功能字段 = %u   (4 = ALT0 = RXD0)\n", (sel1 >> 15) & 7);

	/* 读 UART0 标志寄存器：发送 FIFO 现在空不空？ */
	unsigned fr = uart[UART_FR_IDX];
	printf("UART0_FR = 0x%08x\n", fr);
	printf("  TXFE=%u (TX FIFO 空)  RXFE=%u (RX FIFO 空)  BUSY=%u\n",
	       (fr >> 7) & 1, (fr >> 4) & 1, (fr >> 3) & 1);

	printf("\n与 STM32 的等价关系：\n");
	printf("  gpio[GPFSEL1]  ==  读 STM32 的 GPIOA->CRL\n");
	printf("  uart[UART_FR]  ==  读 STM32 的 USART1->SR\n");
	return 0;
}
