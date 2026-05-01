#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "aht30.h"
#include "mpu6050.h"

int main()
{
    if (aht30_init() != 0) {
        fprintf(stderr, "AHT30 init failed\n");
        return 1;
    }
    if (mpu6050_init() != 0) {
        fprintf(stderr, "MPU6050 init failed\n");
        aht30_close();
        return 1;
    }

    printf("\n=== 开始采集 ===\n\n");

    for (int i = 0; i < 20; i++) {
        float temp, humi;
        int16_t ax, ay, az;

        aht30_read(&temp, &humi);
        mpu6050_read_accel(&ax, &ay, &az);

        printf("[%2d] T:%5.1f C  H:%5.1f%%  X:%6d Y:%6d Z:%6d\n",
               i, temp, humi, ax, ay, az);

        usleep(500000); // 500ms
    }

    aht30_close();
    mpu6050_close();
    printf("\n=== 采集完成 ===\n");
    return 0;
}
