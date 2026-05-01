#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "collector.h"

#define MQTT_BROKER_IP   "your-mqtt-broker-ip"
#define MQTT_PORT         1883
#define MQTT_TOPIC        "edge-gateway/sensor"
#define MQTT_INTERVAL     5

void *mqtt_thread(void *arg)
{
    printf("[MQTT线程] 启动\n");
    printf("[MQTT线程] Broker: %s:%d\n", MQTT_BROKER_IP, MQTT_PORT);
    printf("[MQTT线程] Topic:  %s\n", MQTT_TOPIC);

    int fail_count = 0;

    while (g_shared.running) {
        sensor_data_t local;
        int ai_st;

        pthread_mutex_lock(&g_shared.lock);
        local = g_shared.data;
        ai_st = g_shared.ai_status;
        pthread_mutex_unlock(&g_shared.lock);

        char msg[512];
        snprintf(msg, sizeof(msg),
            "{\"timestamp\":%lu,"
            "\"temp\":%.2f,"
            "\"humi\":%.2f,"
            "\"vib_x\":%d,"
            "\"vib_y\":%d,"
            "\"vib_z\":%d,"
            "\"ai_status\":%d,"
            "\"ai_label\":\"%s\"}",
            (unsigned long)local.timestamp,
            local.temp, local.humi,
            local.vib_x, local.vib_y, local.vib_z,
            ai_st,
            ai_st == 0 ? "normal" : "anomaly");

        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "mosquitto_pub -h %s -p %d -t \"%s\" -m '%s' -q 1 2>/dev/null",
            MQTT_BROKER_IP, MQTT_PORT, MQTT_TOPIC, msg);

        int ret = system(cmd);
        if (ret != 0) {
            fail_count++;
            if (fail_count == 1) {
                printf("[MQTT线程] 发送失败 (已失败 %d 次)\n", fail_count);
            }
        } else {
            if (fail_count > 0) {
                printf("[MQTT线程] 恢复连接\n");
            }
            fail_count = 0;
        }

        sleep(MQTT_INTERVAL);
    }

    printf("[MQTT线程] 退出\n");
    return NULL;
}
