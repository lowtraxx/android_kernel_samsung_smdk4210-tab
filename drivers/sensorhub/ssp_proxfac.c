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

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
#if 0
		data->buf[PROXIMITY_RAW].prox[0]);
#endif
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n",
		data->buf[PROXIMITY_RAW].prox[0], 0, 0);
}

static ssize_t proximity_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char tempbuf[2];
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = strict_strtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	tempbuf[0] = 10;
	tempbuf[1] = 0;

	if (dEnable)
		ssp_send_inst(data, ADD_SENSOR, PROXIMITY_RAW, tempbuf, 2);
	else
		ssp_send_inst(data, ADD_SENSOR, PROXIMITY_RAW, tempbuf, 2);

	return size;
}

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	proximity_state_show, proximity_state_store);

static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	proximity_avg_show, NULL);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_state,
	&dev_attr_prox_avg,
	NULL,
};

int initialize_prox_factorytest(struct ssp_data *data)
{
	struct device *prox_device = NULL;

	sensors_register(prox_device, data, prox_attrs,
		"proximity_sensor");

	return true;
}
