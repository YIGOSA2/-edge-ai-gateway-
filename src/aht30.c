#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <string.h>

#define AHT30_ADDR   0x38
#define I2C_BUS      "/dev/i2c-1"

static int aht_fd = -1;

static void i2c_write(int fd, const uint8_t *buf, int len)
{
    write(fd, buf, len);
}

static void i2c_read(int fd, uint8_t *buf, int len)
{
    read(fd, buf, len);
}

int aht30_init(void)
{
    aht_fd = open(I2C_BUS, O_RDWR);
    if (aht_fd < 0) {
        perror("[AHT30] open I2C bus failed");
        return -1;
    }

    if (ioctl(aht_fd, I2C_SLAVE, AHT30_ADDR) < 0) {
        perror("[AHT30] set I2C address failed");
        return -1;
    }

    // 软复位
    uint8_t reset_cmd = 0xBA;
    i2c_write(aht_fd, &reset_cmd, 1);
    usleep(40000); // 等待 40ms

    // 检查校准状态：读状态字
    uint8_t status;
    i2c_write(aht_fd, (uint8_t[]){0x71}, 1);
    usleep(10000);
    i2c_read(aht_fd, &status, 1);

    if ((status & 0x68) != 0x08) {
        // 校准未使能，发送校准命令
        uint8_t calib_cmd[] = {0xBE, 0x08, 0x00};
        i2c_write(aht_fd, calib_cmd, 3);
        usleep(10000);
    }

    printf("[AHT30] Initialized (status=0x%02X)\n", status);
    return 0;
}

int aht30_read(float *temp, float *humi)
{
    if (aht_fd < 0)
        return -1;

    // 发送触发测量命令
    uint8_t measure_cmd[] = {0xAC, 0x33, 0x00};
    i2c_write(aht_fd, measure_cmd, 3);

    // 等待测量完成，带重试
    uint8_t buf[6];
    int retry = 0;
    do {
        usleep(20000); // 每次 20ms
        memset(buf, 0, sizeof(buf));
        i2c_read(aht_fd, buf, 6);
        retry++;
    } while ((buf[0] & 0x80) && retry < 10);

    if (buf[0] & 0x80) {
        fprintf(stderr, "[AHT30] device busy after retry\n");
        return -1;
    }

    // 湿度（20 位）
    uint32_t hum_raw = ((uint32_t)buf[1] << 12) |
                       ((uint32_t)buf[2] << 4) |
                       ((uint32_t)buf[3] >> 4);

    // 温度（20 位）
    uint32_t temp_raw = (((uint32_t)buf[3] & 0x0F) << 16) |
                        ((uint32_t)buf[4] << 8) |
                        ((uint32_t)buf[5]);

    *humi = (float)hum_raw / 1048576.0f * 100.0f;
    *temp = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;

    return 0;
}

void aht30_close(void)
{
    if (aht_fd >= 0) {
        close(aht_fd);
        aht_fd = -1;
    }
}
