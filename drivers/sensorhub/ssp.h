/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <asm/div64.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/miscdevice.h>
#include <linux/ssp_platformdata.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>

#define SSP_DBG 1

#if SSP_DBG
#define SSP_FUNC_DBG 1
#define SSP_DATA_DBG 0

#define ssp_dbg(dev, format, ...) do { \
	printk(KERN_INFO dev, format, ##__VA_ARGS__); \
	} while (0)
#else
#define ssp_dbg(dev, format, ...)
#endif

#if SSP_FUNC_DBG
#define func_dbg()	do { \
	printk(KERN_INFO "%s : [SSP]\n", __func__); \
	} while (0)
#else
#define func_dbg()
#endif

#if SSP_DATA_DBG
#define data_dbg(dev, format, ...) do { \
	printk(KERN_INFO dev, format, ##__VA_ARGS__); \
	} while (0)
#else
#define data_dbg(dev, format, ...)
#endif

#define DEFUALT_POLLING_DELAY	(180 * NSEC_PER_MSEC)

/* Sensor Sampling Time Define */
enum {
	SENSOR_NS_DELAY_FASTEST = 10000000,	/* 10msec */
	SENSOR_NS_DELAY_GAME = 20000000,	/* 20msec */
	SENSOR_NS_DELAY_UI = 60000000,		/* 60msec */
	SENSOR_NS_DELAY_NORMAL = 180000000,	/* 180msec */
};

enum {
	SENSOR_MS_DELAY_FASTEST = 10,	/* 10msec */
	SENSOR_MS_DELAY_GAME = 20,	/* 20msec */
	SENSOR_MS_DELAY_UI = 60,	/* 60msec */
	SENSOR_MS_DELAY_NORMAL = 180,	/* 180msec */
};

enum {
	SENSOR_CMD_DELAY_FASTEST = 0,	/* 10msec */
	SENSOR_CMD_DELAY_GAME,		/* 20msec */
	SENSOR_CMD_DELAY_UI,		/* 60msec */
	SENSOR_CMD_DELAY_NORMAL,	/* 180msec */
};

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* kernel -> ssp manager cmd*/
#define SSP_LIBRARY_SLEEP_CMD		(1 << 5)
#define SSP_LIBRARY_LARGE_DATA_CMD	(1 << 6)
#define SSP_LIBRARY_WAKEUP_CMD		(1 << 7)

/* ioctl command */
#define AKMIO				0xA1
#define ECS_IOCTL_GET_FUSEROMDATA	_IOR(AKMIO, 0x01, unsigned char[3])
#define ECS_IOCTL_GET_MAGDATA	        _IOR(AKMIO, 0x02, unsigned char[8])
#define ECS_IOCTL_GET_ACCDATA	        _IOR(AKMIO, 0x03, int[3])

/* AP -> SSP Instruction */
#define MSG2SSP_INST_BYPASS_SENSOR_ADD		0xA1
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0xA2
#define MSG2SSP_INST_REMOVE_ALL			0xA3
#define MSG2SSP_INST_CHANGE_DELAY		0xA4
#define MSG2SSP_INST_LIBRARY_ADD		0xB1
#define MSG2SSP_INST_LIBRARY_REMOVE		0xB2

#define MSG2SSP_AP_STT				0xC8
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP			0xD2
#define MSG2SSP_AP_WHOAMI			0x0f
#define MSG2SSP_AP_FIRMWARE_REV			0xf0
#define MSG2SSP_AP_SENSOR_FORMATION		0xf1
#define MSG2SSP_AP_SENSOR_PROXTHRESHOLD		0xf2
#define MSG2SSP_AP_FUSEROM			0X01

/* AP -> SSP Data Protocol Frame Field */
#define MSG2SSP_SSP_SLEEP	0xC1
#define MSG2SSP_STS		0xC2	/* Start to Send */
#define MSG2SSP_RTS		0xC4	/* Ready to Send */
#define MSG2SSP_SRM		0xCA	/* Start to Read MSG */
#define MSG2SSP_SSM		0xCB	/* Start to Send MSG */
#define MSG2SSP_SSD		0xCE	/* Start to Send Data Type & Length */

/* SSP -> AP Instruction */
#define MSG2AP_INST_BYPASS_DATA			0x00
#define MSG2AP_INST_LIBRARY_DATA		0x01

/* SSP -> AP ACK about write CMD */
#define MSG_ACK					0x80	/* ACK from SSP to AP */
#define MSG_NAK					0x70	/* NAK from SSP to AP */

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
};

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,
	GYROSCOPE_SENSOR,
	GEOMAGNETIC_SENSOR,
	PRESSURE_SENSOR,
	GESTURE_SENSOR,
	PROXIMITY_SENSOR,
	LIGHT_SENSOR,
	PROXIMITY_RAW,
	ORIENTATION_SENSOR,
	SENSOR_MAX,
};

struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {
			s16 r;
			s16 g;
			s16 b;
		};
		u8 prox[2];
		s16 data[4];
		s32 pressure[2];
	};
};

struct accelerometer_calibraion_data {
	int x;
	int y;
	int z;
};

struct ssp_data {
	struct input_dev *acc_input_dev;
	struct input_dev *gyro_input_dev;
	struct input_dev *mag_input_dev;
	struct input_dev *gesture_input_dev;
	struct input_dev *pressure_input_dev;
	struct input_dev *light_input_dev;
	struct input_dev *prox_input_dev;

	struct i2c_client *client;
	struct mutex lock;
	struct wake_lock ssp_wake_lock;
	struct miscdevice akmd_device;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct work_struct work_debug;
	struct accelerometer_calibraion_data caldata;
	struct sensor_value buf[SENSOR_MAX];

	bool bCheckSuspend;
	bool bDebugEnable;
	u8 uFuseRomData[3];
	char *pchLibraryBuf;
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];
	atomic_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];

	int (*wakeup_mcu)(void);
	int (*check_mcu_ready)(void);
	int (*set_mcu_reset)(int);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

int initialize_input_dev(struct ssp_data *data);
int initialize_sysfs(struct ssp_data *data);
int initialize_accel_factorytest(struct ssp_data *data);
int initialize_prox_factorytest(struct ssp_data *data);
int accel_open_calibration(struct ssp_data *data);
void check_fwbl(struct ssp_data *data);
void remove_input_dev(struct ssp_data *data);
void remove_sysfs(struct ssp_data *data);
int ssp_sleep_mode(struct ssp_data *data);
int ssp_resume_mode(struct ssp_data *data);
int sensors_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
int ssp_send_inst(struct ssp_data *data, u8 uInst,
	u8 uSensorType, u8 *uSendBuf, u8 uLength);
int ssp_select_irq_msg(struct ssp_data *data);
void ssp_get_fuserom_data(struct ssp_data *data);
void ssp_set_sensor_position(struct ssp_data *data);
void ssp_set_proximity_threshold(struct ssp_data *data);
unsigned int ssp_get_firmware_rev(struct ssp_data *data);
int parse_dataframe(struct ssp_data *data,
	char *pchRcvDataFrame, int iLength);
void enable_debug_timer(struct ssp_data *data);
void disable_debug_timer(struct ssp_data *data);
void report_acc_data(struct ssp_data *data, struct sensor_value *accdata);
void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata);
void report_mag_data(struct ssp_data *data, struct sensor_value *magdata);
void report_gesture_data(struct ssp_data *data, struct sensor_value *gesdata);
void report_pressure_data(struct ssp_data *data, struct sensor_value *predata);
void report_light_data(struct ssp_data *data, struct sensor_value *lightdata);
void report_prox_data(struct ssp_data *data, struct sensor_value *proxdata);
void report_prox_raw_data(struct ssp_data *data,
	struct sensor_value *proxrawdata);
long akmd_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
u8 get_msdelay(int64_t dDelayRate);
#endif
