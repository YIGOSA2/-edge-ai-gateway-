#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

int  mpu6050_init(void);
int  mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az);
void mpu6050_close(void);

#endif
