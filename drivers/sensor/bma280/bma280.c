/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <i2c.h>
#include <init.h>
#include <sensor.h>
#include <misc/__assert.h>

#include "bma280.h"

static int bma280_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct bma280_data *drv_data = dev->driver_data;
	uint8_t buf[6];
	uint8_t lsb;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	/*
	 * since all accel data register addresses are consecutive,
	 * a burst read can be used to read all the samples
	 */
	if (i2c_burst_read(drv_data->i2c, BMA280_I2C_ADDRESS,
			   BMA280_REG_ACCEL_X_LSB, buf, 6) < 0) {
		SYS_LOG_DBG("Could not read accel axis data");
		return -EIO;
	}

	lsb = (buf[0] & BMA280_ACCEL_LSB_MASK) >> BMA280_ACCEL_LSB_SHIFT;
	drv_data->x_sample = (((int8_t)buf[1]) << BMA280_ACCEL_LSB_BITS) | lsb;

	lsb = (buf[2] & BMA280_ACCEL_LSB_MASK) >> BMA280_ACCEL_LSB_SHIFT;
	drv_data->y_sample = (((int8_t)buf[3]) << BMA280_ACCEL_LSB_BITS) | lsb;

	lsb = (buf[4] & BMA280_ACCEL_LSB_MASK) >> BMA280_ACCEL_LSB_SHIFT;
	drv_data->z_sample = (((int8_t)buf[5]) << BMA280_ACCEL_LSB_BITS) | lsb;

	if (i2c_reg_read_byte(drv_data->i2c, BMA280_I2C_ADDRESS,
			      BMA280_REG_TEMP,
			      (uint8_t *)&drv_data->temp_sample) < 0) {
		SYS_LOG_DBG("Could not read temperature data");
		return -EIO;
	}

	return 0;
}

static void bma280_channel_accel_convert(struct sensor_value *val,
					int64_t raw_val)
{
	/*
	 * accel_val = (sample * BMA280_PMU_FULL_RAGE) /
	 *             (2^data_width * 10^6)
	 */
	val->type = SENSOR_VALUE_TYPE_INT_PLUS_MICRO;
	raw_val = (raw_val * BMA280_PMU_FULL_RANGE) /
		  (1 << (8 + BMA280_ACCEL_LSB_BITS));
	val->val1 = raw_val / 1000000;
	val->val2 = raw_val % 1000000;

	/* normalize val to make sure val->val2 is positive */
	if (val->val2 < 0) {
		val->val1 -= 1;
		val->val2 += 1000000;
	}
}

static int bma280_channel_get(struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct bma280_data *drv_data = dev->driver_data;

	/*
	 * See datasheet "Sensor data" section for
	 * more details on processing sample data.
	 */
	if (chan == SENSOR_CHAN_ACCEL_X) {
		bma280_channel_accel_convert(val, drv_data->x_sample);
	} else if (chan == SENSOR_CHAN_ACCEL_Y) {
		bma280_channel_accel_convert(val, drv_data->y_sample);
	} else if (chan == SENSOR_CHAN_ACCEL_Z) {
		bma280_channel_accel_convert(val, drv_data->z_sample);
	} else if (chan == SENSOR_CHAN_ACCEL_ANY) {
		bma280_channel_accel_convert(val, drv_data->x_sample);
		bma280_channel_accel_convert(val + 1, drv_data->y_sample);
		bma280_channel_accel_convert(val + 2, drv_data->z_sample);
	} else if (chan == SENSOR_CHAN_TEMP) {
		/* temperature_val = 23 + sample / 2 */
		val->type = SENSOR_VALUE_TYPE_INT_PLUS_MICRO;
		val->val1 = (drv_data->temp_sample >> 1) + 23;
		val->val2 = 500000 * (drv_data->temp_sample & 1);
		return 0;
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api bma280_driver_api = {
#if CONFIG_BMA280_TRIGGER
	.attr_set = bma280_attr_set,
	.trigger_set = bma280_trigger_set,
#endif
	.sample_fetch = bma280_sample_fetch,
	.channel_get = bma280_channel_get,
};

int bma280_init(struct device *dev)
{
	struct bma280_data *drv_data = dev->driver_data;
	uint8_t id = 0;

	drv_data->i2c = device_get_binding(CONFIG_BMA280_I2C_MASTER_DEV_NAME);
	if (drv_data->i2c == NULL) {
		SYS_LOG_DBG("Could not get pointer to %s device",
			    CONFIG_BMA280_I2C_MASTER_DEV_NAME);
		return -EINVAL;
	}

	/* read device ID */
	if (i2c_reg_read_byte(drv_data->i2c, BMA280_I2C_ADDRESS,
			      BMA280_REG_CHIP_ID, &id) < 0) {
		SYS_LOG_DBG("Could not read chip id");
		return -EIO;
	}

	if (id != BMA280_CHIP_ID) {
		SYS_LOG_DBG("Unexpected chip id (%x)", id);
		return -EIO;
	}

	if (i2c_reg_write_byte(drv_data->i2c, BMA280_I2C_ADDRESS,
			       BMA280_REG_PMU_BW, BMA280_PMU_BW) < 0) {
		SYS_LOG_DBG("Could not set data filter bandwidth");
		return -EIO;
	}

	/* set g-range */
	if (i2c_reg_write_byte(drv_data->i2c, BMA280_I2C_ADDRESS,
			       BMA280_REG_PMU_RANGE, BMA280_PMU_RANGE) < 0) {
		SYS_LOG_DBG("Could not set data g-range");
		return -EIO;
	}

#ifdef CONFIG_BMA280_TRIGGER
	if (bma280_init_interrupt(dev) < 0) {
		SYS_LOG_DBG("Could not initialize interrupts");
		return -EIO;
	}
#endif

	dev->driver_api = &bma280_driver_api;

	return 0;
}

struct bma280_data bma280_driver;

DEVICE_INIT(bma280, CONFIG_BMA280_NAME, bma280_init, &bma280_driver,
	    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);
