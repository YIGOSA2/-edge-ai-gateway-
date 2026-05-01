#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    float    temp;
    float    humi;
    int16_t  vib_x;
    int16_t  vib_y;
    int16_t  vib_z;
    uint64_t timestamp;
} sensor_data_t;

typedef struct {
    sensor_data_t   data;
    int             ai_status;
    pthread_mutex_t lock;
    volatile int    running;
} shared_data_t;

extern shared_data_t g_shared;

void *sensor_thread(void *arg);
void *storage_thread(void *arg);
void *ai_thread(void *arg);
void *mqtt_thread(void *arg);

uint64_t get_time_ms(void);

#endif
