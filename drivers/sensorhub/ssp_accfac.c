/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"
#define CALIBRATION_DATA_AMOUNT	10

int accel_open_calibration(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);

		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		return iRet;
	}

	iRet = cal_filp->f_op->read(cal_filp, (char *)&data->caldata,
		3 * sizeof(int), &cal_filp->f_pos);
	if (iRet != 3 * sizeof(int))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	ssp_dbg("[SSP] : open accel calibration %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);
	return iRet;
}

static int accel_do_calibrate(struct ssp_data *data, int iEnable)
{
	int iSum[3] = { 0, };
	int iRet = 0, iCount;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	data->caldata.x = 0;
	data->caldata.y = 0;
	data->caldata.z = 0;

	if (iEnable) {
		for (iCount = 0; iCount < CALIBRATION_DATA_AMOUNT; iCount++) {
			iSum[0] += data->buf[ACCELEROMETER_SENSOR].x;
			iSum[1] += data->buf[ACCELEROMETER_SENSOR].y;
			iSum[2] += (data->buf[ACCELEROMETER_SENSOR].z - 1024);
			mdelay(10);
		}

		data->caldata.x = (iSum[0] / CALIBRATION_DATA_AMOUNT);
		data->caldata.y = (iSum[1] / CALIBRATION_DATA_AMOUNT);
		data->caldata.z = (iSum[2] / CALIBRATION_DATA_AMOUNT);
	} else {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;
	}

	ssp_dbg("[SSP] : do accel calibrate %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		return iRet;
	}

	iRet = cal_filp->f_op->write(cal_filp, (char *)&data->caldata,
		3 * sizeof(int), &cal_filp->f_pos);
	if (iRet != 3 * sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

static ssize_t accel_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet;
	int iCount = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = accel_open_calibration(data);
	if (iRet < 0)
		pr_err("%s: accel_open_calibration() failed\n", __func__);

	ssp_dbg("[SSP] Cal data : %d %d %d - %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z, iRet);

	iCount = sprintf(buf, "%d %d %d %d\n", iRet, data->caldata.x,
			data->caldata.y, data->caldata.z);
	return iCount;
}

static ssize_t accel_calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = strict_strtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	iRet = accel_do_calibrate(data, (int)dEnable);
	if (iRet < 0)
		pr_err("%s: accel_do_calibrate() failed\n", __func__);

	if (!dEnable) {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;
	}
	return size;
}

static ssize_t raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[ACCELEROMETER_SENSOR].x,
		data->buf[ACCELEROMETER_SENSOR].y,
		data->buf[ACCELEROMETER_SENSOR].z);
}

static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	accel_calibration_show, accel_calibration_store);

static DEVICE_ATTR(raw_data, S_IRUGO | S_IWUSR | S_IWGRP,
	raw_data_read, NULL);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_calibration,
	&dev_attr_raw_data,
	NULL,
};

int initialize_accel_factorytest(struct ssp_data *data)
{
	struct device *acc_device = NULL;

	sensors_register(acc_device, data, acc_attrs,
		"accelerometer_sensor");

	return true;
}
