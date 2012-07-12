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
/* AKM Daemon Library ioctl						 */
/*************************************************************************/

static int akmd_copy_in(unsigned int cmd, void __user *argp,
			void *buf, size_t buf_size)
{
	if (!(cmd & IOC_IN))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_from_user(buf, argp, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

static int akmd_copy_out(unsigned int cmd, void __user *argp,
			 void *buf, size_t buf_size)
{
	if (!(cmd & IOC_OUT))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_to_user(argp, buf, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

long akmd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int iRet;
	void __user *argp = (void __user *)arg;
	struct ssp_data *data = container_of(file->private_data,
					struct ssp_data, akmd_device);

	union {
		u8 uData[8];
		u8 uMagData[8];
		u8 uFuseData[3];
		int iAccData[3];
	} akmdbuf;

	iRet = akmd_copy_in(cmd, argp, akmdbuf.uData, sizeof(akmdbuf));
	if (iRet)
		return iRet;

	switch (cmd) {
	case ECS_IOCTL_GET_MAGDATA:
		akmdbuf.uMagData[0] = 1;
		akmdbuf.uMagData[1] = data->buf[GEOMAGNETIC_SENSOR].x & 0xff;
		akmdbuf.uMagData[2] = data->buf[GEOMAGNETIC_SENSOR].x >> 8;
		akmdbuf.uMagData[3] = data->buf[GEOMAGNETIC_SENSOR].y & 0xff;
		akmdbuf.uMagData[4] = data->buf[GEOMAGNETIC_SENSOR].y >> 8;
		akmdbuf.uMagData[5] = data->buf[GEOMAGNETIC_SENSOR].z & 0xff;
		akmdbuf.uMagData[6] = data->buf[GEOMAGNETIC_SENSOR].z >> 8;
		akmdbuf.uMagData[7] = 0;
		break;
	case ECS_IOCTL_GET_ACCDATA:
		akmdbuf.iAccData[0] = data->buf[ACCELEROMETER_SENSOR].x * (-1);
		akmdbuf.iAccData[1] = data->buf[ACCELEROMETER_SENSOR].y * (-1);
		akmdbuf.iAccData[2] = data->buf[ACCELEROMETER_SENSOR].z;
		break;
	case ECS_IOCTL_GET_FUSEROMDATA:
		akmdbuf.uFuseData[0] = data->uFuseRomData[0];
		akmdbuf.uFuseData[1] = data->uFuseRomData[1];
		akmdbuf.uFuseData[2] = data->uFuseRomData[2];
		break;
	default:
		return -ENOTTY;
	}

	if (iRet < 0)
		return iRet;

	return akmd_copy_out(cmd, argp, akmdbuf.uData, sizeof(akmdbuf));
}

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/

static void get_i32_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 16;
	sensorsdata->pressure[0] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	sensorsdata->pressure[0] += iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->pressure[0] += iTemp;


	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	sensorsdata->pressure[1] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->pressure[1] += iTemp;

	data_dbg("p : %u, t: %u\n", sensorsdata->pressure[0],
		sensorsdata->pressure[1]);
}

static void get_i16_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->x = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->y = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->z = iTemp;

	data_dbg("x: %d, y: %d, z: %d\n", sensorsdata->x,
		sensorsdata->y, sensorsdata->z);

}

static void get_u8_proxdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	sensorsdata->prox[0] = (u8)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->prox[1] = (u8)pchRcvDataFrame[(*iDataIdx)++];

	data_dbg("prox : %u, %u\n", sensorsdata->prox[0], sensorsdata->prox[1]);
}

static void get_u8_proxrawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	sensorsdata->prox[0] = (u8)pchRcvDataFrame[(*iDataIdx)++];

	data_dbg("prox : %u\n", sensorsdata->prox[0]);
}

static void get_i16_gesturedata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[0] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[1] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[2] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[3] = iTemp;

	data_dbg("A: %d, B: %d, C: %d, D: %d\n",
		sensorsdata->data[0], sensorsdata->data[1],
		sensorsdata->data[2], sensorsdata->data[3]);
}

int parse_dataframe(struct ssp_data *data,
	char *pchRcvDataFrame, int iLength)
{
	int iDataIdx;
	struct sensor_value *sensorsdata;

	sensorsdata = kzalloc(sizeof(*sensorsdata), GFP_KERNEL);
	if (sensorsdata == NULL)
		return false;

	for (iDataIdx = 0; iDataIdx < iLength;) {
		if (pchRcvDataFrame[iDataIdx] == MSG2AP_INST_BYPASS_DATA) {
			iDataIdx++;

			switch (pchRcvDataFrame[iDataIdx++]) {
			case ACCELEROMETER_SENSOR:
				data_dbg("  %d ", ACCELEROMETER_SENSOR);
				get_i16_sensordata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_acc_data(data, sensorsdata);
				break;
			case GYROSCOPE_SENSOR:
				data_dbg("  %d ", GYROSCOPE_SENSOR);
				get_i16_sensordata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_gyro_data(data, sensorsdata);
				break;
			case GEOMAGNETIC_SENSOR:
				data_dbg("  %d ", GEOMAGNETIC_SENSOR);
				get_i16_sensordata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_mag_data(data, sensorsdata);
				break;
			case PRESSURE_SENSOR:
				data_dbg("  %d ", PRESSURE_SENSOR);
				get_i32_sensordata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_pressure_data(data, sensorsdata);
				break;
			case GESTURE_SENSOR:
				data_dbg("  %d ", GESTURE_SENSOR);
				get_i16_gesturedata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_gesture_data(data, sensorsdata);
				break;
			case PROXIMITY_SENSOR:
				data_dbg("  %d ", PROXIMITY_SENSOR);
				get_u8_proxdata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_prox_data(data, sensorsdata);
				break;
			case PROXIMITY_RAW:
				data_dbg("  %d ", PROXIMITY_RAW);
				get_u8_proxrawdata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_prox_raw_data(data, sensorsdata);
				break;
			case LIGHT_SENSOR:
				data_dbg("  %d ", LIGHT_SENSOR);
				get_i16_sensordata(pchRcvDataFrame,
					&iDataIdx, sensorsdata);
				report_light_data(data, sensorsdata);
				break;
			default:
				kfree(sensorsdata);
				return false;
			}
		} else
			iDataIdx++;
	}
	kfree(sensorsdata);
	return true;
}
