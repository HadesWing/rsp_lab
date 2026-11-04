/*
 * spi_loop.c —— SPI0 用户态自测（spidev 直通，不需要写内核驱动）
 *
 * 用法：
 *   ./spi_loop                      # 默认 /dev/spidev0.0, mode0, 8bit, 500kHz, 16 字节
 *   ./spi_loop -l 32 -s 1000000     # 32 字节, 1MHz
 *   ./spi_loop -m 3 -b 8            # SPI_MODE_3
 *
 * 判据：
 *   MISO(GPIO9) 与 MOSI(GPIO10) 短接时 → rx 应逐个等于 tx，退出码 0
 *   未短接（MISO 悬空）时          → rx 全是 0x00 或 0xff，退出码 3，这才是正常现象
 *
 * 编译：gcc -O2 -Wall -o spi_loop spi_loop.c
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

static void hexdump(const char *tag, const uint8_t *p, int n)
{
    printf("%s(%2d):", tag, n);
    for (int i = 0; i < n; i++) {
        if (i && i % 16 == 0)
            printf("\n         ");
        printf(" %02x", p[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *dev = "/dev/spidev0.0";
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 500000;
    int len = 16;
    int opt;

    while ((opt = getopt(argc, argv, "D:m:b:s:l:h")) != -1) {
        switch (opt) {
        case 'D': dev   = optarg; break;
        case 'm': mode  = (uint8_t)strtoul(optarg, NULL, 0); break;
        case 'b': bits  = (uint8_t)strtoul(optarg, NULL, 0); break;
        case 's': speed = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'l': len   = atoi(optarg); break;
        default:
            printf("用法: %s [-D 设备] [-m 模式0-3] [-b 位宽] [-s 速度Hz] [-l 长度]\n", argv[0]);
            return 2;
        }
    }
    if (len <= 0 || len > 4096) {
        fprintf(stderr, "长度必须在 1..4096\n");
        return 2;
    }

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s 失败: %s\n", dev, strerror(errno));
        fprintf(stderr, "提示：确认 dtparam=spi=on 已开且已重启，ls /dev/spidev*\n");
        return 1;
    }

    uint8_t  wm = mode,  wb = bits;
    uint32_t ws = speed;
    if (ioctl(fd, SPI_IOC_WR_MODE, &wm) < 0)            perror("SPI_IOC_WR_MODE");
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &wb) < 0)   perror("SPI_IOC_WR_BITS_PER_WORD");
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &ws) < 0)    perror("SPI_IOC_WR_MAX_SPEED_HZ");

    uint8_t  rm = 0, rb = 0;
    uint32_t rs = 0;
    if (ioctl(fd, SPI_IOC_RD_MODE, &rm) < 0 ||
        ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &rb) < 0 ||
        ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &rs) < 0) {
        perror("SPI_IOC_RD_* 回读");
        close(fd);
        return 1;
    }
    printf("设备 %s 生效配置：mode=%u  bits=%u  speed=%u Hz\n", dev, rm, rb, rs);

    uint8_t *tx = calloc(1, (size_t)len);
    uint8_t *rx = calloc(1, (size_t)len);
    if (!tx || !rx) { close(fd); return 1; }

    for (int i = 0; i < len; i++)
        tx[i] = (uint8_t)(0xA0 + i);

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf        = (__u64)(uintptr_t)tx;
    tr.rx_buf        = (__u64)(uintptr_t)rx;
    tr.len           = (__u32)len;
    tr.speed_hz      = speed;
    tr.bits_per_word = bits;
    tr.delay_usecs   = 0;

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI_IOC_MESSAGE(1) 传输失败");
        free(tx); free(rx); close(fd);
        return 1;
    }

    hexdump("TX", tx, len);
    hexdump("RX", rx, len);

    int same = 0;
    for (int i = 0; i < len; i++)
        if (rx[i] == tx[i]) same++;

    printf("匹配 %d/%d 字节\n", same, len);

    if (same == len) {
        printf("结论：回读完全一致 → MOSI/MISO 已短接（或器件回环），SPI 通路闭环成功\n");
    } else if (same == 0) {
        printf("结论：一个字节都对不上 → 典型「MISO 悬空」\n");
        printf("      MOSI 确实在发（CS/CLK 都有波形），只是没有从设备把数据送回来。\n");
        printf("      接一根杜邦线短接 GPIO10(MOSI, pin19) ↔ GPIO9(MISO, pin21) 再跑一次。\n");
    } else {
        printf("结论：部分匹配 → 时钟/模式/位宽可能不匹配，检查 -m（模式0-3）与 -b\n");
    }

    free(tx);
    free(rx);
    close(fd);
    return (same == len) ? 0 : 3;
}
