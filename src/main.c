#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "collector.h"
#include "aht30.h"
#include "mpu6050.h"

shared_data_t g_shared;

void signal_handler(int sig)
{
    printf("\n收到信号 %d，正在停止...\n", sig);
    g_shared.running = 0;
}

int main()
{
    printf("=== 边缘 AI 智能网关 启动 ===\n\n");

    pthread_mutex_init(&g_shared.lock, NULL);
    g_shared.running = 1;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (aht30_init() != 0) {
        fprintf(stderr, "AHT30 初始化失败\n");
        return 1;
    }
    if (mpu6050_init() != 0) {
        fprintf(stderr, "MPU6050 初始化失败\n");
        aht30_close();
        return 1;
    }

    pthread_t tid_sensor, tid_storage, tid_ai, tid_mqtt;
    pthread_create(&tid_sensor, NULL, sensor_thread, NULL);
    pthread_create(&tid_storage, NULL, storage_thread, NULL);
    pthread_create(&tid_ai, NULL, ai_thread, NULL);
    pthread_create(&tid_mqtt, NULL, mqtt_thread, NULL);

    printf("所有线程已启动，按 Ctrl+C 停止\n\n");

    pthread_join(tid_sensor, NULL);
    pthread_join(tid_storage, NULL);
    pthread_join(tid_ai, NULL);
    pthread_join(tid_mqtt, NULL);

    aht30_close();
    mpu6050_close();
    pthread_mutex_destroy(&g_shared.lock);

    printf("程序正常退出\n");
    return 0;
}
