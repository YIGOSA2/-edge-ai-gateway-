#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "collector.h"
#include "aht30.h"
#include "mpu6050.h"

uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void *sensor_thread(void *arg)
{
    printf("[采集线程] 启动\n");

    while (g_shared.running) {
        sensor_data_t local;

        aht30_read(&local.temp, &local.humi);
        mpu6050_read_accel(&local.vib_x, &local.vib_y, &local.vib_z);
        local.timestamp = get_time_ms();

        pthread_mutex_lock(&g_shared.lock);
        g_shared.data = local;
        pthread_mutex_unlock(&g_shared.lock);

        usleep(100000);
    }

    printf("[采集线程] 退出\n");
    return NULL;
}
