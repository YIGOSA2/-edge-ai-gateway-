#include <stdio.h>
#include <unistd.h>
#include "collector.h"
#include "ai_infer.h"

#define HIST_LEN 5
#define VIB_THRESHOLD 1000000.0f

void *ai_thread(void *arg)
{
    printf("[AI线程] 启动\n");

    int alarm_count = 0;
    int total_alarm = 0;
    int total_normal = 0;

    float hist_x[HIST_LEN] = {0};
    float hist_y[HIST_LEN] = {0};
    float hist_z[HIST_LEN] = {0};
    int hist_idx = 0;

    while (g_shared.running) {
        sensor_data_t local;

        pthread_mutex_lock(&g_shared.lock);
        local = g_shared.data;
        pthread_mutex_unlock(&g_shared.lock);

        hist_x[hist_idx] = local.vib_x;
        hist_y[hist_idx] = local.vib_y;
        hist_z[hist_idx] = local.vib_z;
        hist_idx = (hist_idx + 1) % HIST_LEN;

        float sum_x = 0, sum_y = 0, sum_z = 0;
        for (int i = 0; i < HIST_LEN; i++) {
            sum_x += hist_x[i];
            sum_y += hist_y[i];
            sum_z += hist_z[i];
        }
        float avg_x = sum_x / HIST_LEN;
        float avg_y = sum_y / HIST_LEN;
        float avg_z = sum_z / HIST_LEN;

        float total_var = 0;
        for (int i = 0; i < HIST_LEN; i++) {
            total_var += (hist_x[i] - avg_x) * (hist_x[i] - avg_x)
                       + (hist_y[i] - avg_y) * (hist_y[i] - avg_y)
                       + (hist_z[i] - avg_z) * (hist_z[i] - avg_z);
        }
        total_var /= HIST_LEN;

        int vib_alarm = (total_var > VIB_THRESHOLD);
        int ai_alarm = ai_predict(local.temp, local.humi,
                                  local.vib_x, local.vib_y, local.vib_z);
        int result = vib_alarm || ai_alarm;

        pthread_mutex_lock(&g_shared.lock);
        g_shared.ai_status = result;
        pthread_mutex_unlock(&g_shared.lock);

        if (result == 0) {
            total_normal++;
            alarm_count = 0;
        } else {
            alarm_count++;
            if (alarm_count >= 3) {
                const char *reason = "";
                if (vib_alarm && ai_alarm) reason = "AI:振动+温湿度异常";
                else if (vib_alarm) reason = "AI:振动异常";
                else reason = "AI:温湿度异常";
                printf("[AI线程] !!! %s !!! "
                       "T:%.1fC H:%.1f%% X:%d Y:%d Z:%d\n",
                       reason, local.temp, local.humi,
                       local.vib_x, local.vib_y, local.vib_z);
                total_alarm++;
            }
        }

        sleep(1);
    }

    printf("[AI线程] 退出 (正常:%d, 报警:%d)\n", total_normal, total_alarm);
}
