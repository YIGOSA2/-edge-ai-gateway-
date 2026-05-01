#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>

#define MPU6050_ADDR  0x68
#define I2C_BUS       "/dev/i2c-1"

static int mpu_fd = -1;

static void i2c_write_reg(int fd, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    write(fd, buf, 2);
}

static void i2c_read_reg(int fd, uint8_t reg, uint8_t *buf, uint8_t len)
{
    write(fd, &reg, 1);
    read(fd, buf, len);
}

int mpu6050_init(void)
{
    mpu_fd = open(I2C_BUS, O_RDWR);
    if (mpu_fd < 0) {
        perror("[MPU6050] open I2C bus failed");
        return -1;
    }

    if (ioctl(mpu_fd, I2C_SLAVE, MPU6050_ADDR) < 0) {
        perror("[MPU6050] set I2C address failed");
        return -1;
    }

    // 验证 WHO_AM_I（MPU6050=0x68, MPU6500=0x70）
    uint8_t who_am_i;
    i2c_read_reg(mpu_fd, 0x75, &who_am_i, 1);
    if (who_am_i != 0x68 && who_am_i != 0x70) {
        fprintf(stderr, "[MPU6050] not found! ID: 0x%02X\n", who_am_i);
        return -1;
    }
    printf("[MPU6050] WHO_AM_I: 0x%02X - OK\n", who_am_i);

    // 唤醒（解除睡眠模式）
    i2c_write_reg(mpu_fd, 0x6B, 0x00);
    usleep(100000);

    // 加速度量程 ±2g
    i2c_write_reg(mpu_fd, 0x1C, 0x00);

    printf("[MPU6050] Initialized\n");
    return 0;
}

int mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    if (mpu_fd < 0)
        return -1;

    uint8_t buf[6];
    i2c_read_reg(mpu_fd, 0x3B, buf, 6);

    *ax = (int16_t)(buf[0] << 8 | buf[1]);
    *ay = (int16_t)(buf[2] << 8 | buf[3]);
    *az = (int16_t)(buf[4] << 8 | buf[5]);

    return 0;
}

void mpu6050_close(void)
{
    if (mpu_fd >= 0) {
        close(mpu_fd);
        mpu_fd = -1;
    }
}
