/*
 *  arch/arm/mach-exynos/p4-input.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S7301)
#include <linux/synaptics_s7301.h>
static bool have_tsp_ldo;
static struct charger_callbacks *charger_callbacks;

void synaptics_ts_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void synaptics_ts_register_callback(struct charger_callbacks *cb)
{
	charger_callbacks = cb;
	printk(KERN_DEBUG "[TSP] %s\n", __func__);
}

static int synaptics_ts_set_power(bool en)
{
	if (!have_tsp_ldo)
		return -1;
	printk(KERN_DEBUG "[TSP] %s(%d)\n", __func__, en);

	if (en) {
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_SFN(0x3));
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_SFN(0x3));
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_UP);

		s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_LDO_ON, 1);
		s3c_gpio_cfgpin(GPIO_TSP_RST, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_RST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_RST, 1);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_SDA_18V, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_SCL_18V, 0);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_INT, 0);
		s3c_gpio_cfgpin(GPIO_TSP_RST, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_RST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_RST, 0);
		s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_LDO_ON, 0);
	}

	return 0;
}

static void synaptics_ts_reset(void)
{
	printk(KERN_DEBUG "[TSP] %s\n", __func__);
	s3c_gpio_cfgpin(GPIO_TSP_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TSP_RST, 0);
	msleep(100);
	gpio_set_value(GPIO_TSP_RST, 1);
}

static struct synaptics_platform_data synaptics_ts_pdata = {
	.gpio_attn = GPIO_TSP_INT,
	.max_x = 1279,
	.max_y = 799,
	.max_pressure = 255,
	.max_width = 100,
	.x_line = 27,
	.y_line = 42,
	.set_power = synaptics_ts_set_power,
	.hw_reset = synaptics_ts_reset,
	.register_cb = synaptics_ts_register_callback,
};

static struct i2c_board_info i2c_synaptics[] __initdata = {
	{
		I2C_BOARD_INFO(SYNAPTICS_TS_NAME,
			SYNAPTICS_TS_ADDR),
		.platform_data = &synaptics_ts_pdata,
	},
};
#endif	/* CONFIG_TOUCHSCREEN_SYNAPTICS_S7301 */

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT1664S)
#include <linux/i2c/mxt1664s.h>
static struct mxt_callbacks *mxt_callbacks;
void ts_charger_infom(bool en)
{
	u8 buf[] = {0,
		46, 2, 10,
		46, 3, 16,
		56, 36, 0,
		62, 1, 0,
		62, 9, 16,
		62, 11, 16,
		62, 13, 16,
		62, 19, 128,
		62, 20, 20,};
	u8 buf2[] = {1,
		46, 2, 24,
		46, 3, 24,
		56, 36, 3,
		62, 1, 1,
		62, 9, 20,
		62, 11, 20,
		62, 13, 20,
		62, 19, 112,
		62, 20, 30,};
	int length = sizeof(buf);
	int i = 3;

	if (en) {
		buf[0] = 1;
		while (i < length) {
			buf[i] = buf2[i];
			i += 3;
		}
	}

	if (mxt_callbacks && mxt_callbacks->inform_charger)
		mxt_callbacks->inform_charger(mxt_callbacks, buf, length);
}

static void ts_register_callback(struct mxt_callbacks *cb)
{
	mxt_callbacks = cb;
	printk(KERN_DEBUG "[TSP] %s\n", __func__);
}

static int ts_power_on(void)
{
	int gpio = 0;

	/* touch reset pin */
	gpio = GPIO_TSP_RST;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	/* touch xvdd en pin */
	gpio = GPIO_TSP_LDO_ON2;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	gpio = GPIO_TSP_LDO_ON;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 1);

	gpio = GPIO_TSP_LDO_ON1;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 1);

	gpio = GPIO_TSP_LDO_ON2;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 1);

	/* reset ic */
	usleep_range(1000, 1500);
	gpio_set_value(GPIO_TSP_RST, 1);

	/* touch interrupt pin */
	gpio = GPIO_TSP_INT;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);

	msleep(MXT_1664S_HW_RESET_TIME);

	printk(KERN_ERR "mxt_power_on is finished\n");

	return 0;
}

static int ts_power_off(void)
{
	int gpio = 0;

	gpio = GPIO_TSP_LDO_ON1;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	gpio = GPIO_TSP_LDO_ON;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	/* touch interrupt pin */
	gpio = GPIO_TSP_INT;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);

	/* touch reset pin */
	gpio = GPIO_TSP_RST;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	/* touch xvdd en pin */
	gpio = GPIO_TSP_LDO_ON2;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_set_value(gpio, 0);

	printk(KERN_ERR "mxt_power_off is finished\n");

	return 0;
}

/*
	Configuration for MXT1664-S
*/
#define MXT1664S_MAX_MT_FINGERS	10
#define MXT1664S_BLEN_BATT		112
#define MXT1664S_CHRGTIME_BATT	150
#define MXT1664S_THRESHOLD_BATT	65

static u8 t7_config_s[] = { GEN_POWERCONFIG_T7,
	48, 255, 150, 3
};

static u8 t8_config_s[] = { GEN_ACQUISITIONCONFIG_T8,
	MXT1664S_CHRGTIME_BATT, 0, 5, 10, 0, 0, 255, 255, 0, 0
};

static u8 t9_config_s[] = { TOUCH_MULTITOUCHSCREEN_T9,
	131, 0, 0, 27, 42, 0, MXT1664S_BLEN_BATT, MXT1664S_THRESHOLD_BATT, 2, 1,
	0, 5, 1, 65, MXT1664S_MAX_MT_FINGERS, 10, 20, 20, 255, 15,
	255, 15, 5, 5, 5, 5, 0, 0, 0, 0,
	32, 15, 51, 53, 0, 1
};

static u8 t15_config_s[] = { TOUCH_KEYARRAY_T15,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u8 t18_config_s[] = { SPT_COMCONFIG_T18,
	0, 0
};

static u8 t24_config_s[] = {
	PROCI_ONETOUCHGESTUREPROCESSOR_T24,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t25_config_s[] = {
	SPT_SELFTEST_T25,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 200
};

static u8 t27_config_s[] = {
	PROCI_TWOTOUCHGESTUREPROCESSOR_T27,
	0, 0, 0, 0, 0, 0, 0
};

static u8 t40_config_s[] = { PROCI_GRIPSUPPRESSION_T40,
	0, 0, 0, 0, 0
};

static u8 t42_config_s[] = { PROCI_TOUCHSUPPRESSION_T42,
	0, 42, 50, 50, 127, 0, 0, 0, 5, 5
};

static u8 t43_config_s[] = { SPT_DIGITIZER_T43,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u8 t46_config_s[] = { SPT_CTECONFIG_T46,
	4, 0, 10, 16, 0, 0, 1, 0, 0, 0,
	15
};

static u8 t47_config_s[] = { PROCI_STYLUS_T47,
	73, 40, 60, 10, 2, 30, 0, 120, 1, 24,
	0, 0, 15
};

static u8 t55_config_s[] = {ADAPTIVE_T55,
	0, 0, 0, 0, 0, 0, 0
};

static u8 t56_config_s[] = {PROCI_SHIELDLESS_T56,
	3, 0, 1, 55, 27, 27, 27, 27, 27, 27,
	26, 26, 26, 25, 25, 25, 24, 24, 24, 23,
	23, 22, 22, 22, 21, 21, 20, 20, 20, 19,
	19, 0, 0, 0, 0, 0, 0, 128, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u8 t57_config_s[] = {PROCI_EXTRATOUCHSCREENDATA_T57,
	227, 25, 0
};

static u8 t61_config_s[] = {SPT_TIMER_T61,
	0, 0, 0, 0, 0
};

static u8 t62_config_s[] = {PROCG_NOISESUPPRESSION_T62,
	3, 0, 0, 23, 10, 0, 0, 0, 20, 16,
	0, 16, 0, 16, 2, 0, 5, 5, 10, 128,
	20, 10, 40, 10, 64, 24, 24, 4, 100, 0,
	0, 0, 0, 0, 64, 50, 2, 5, 1, 66,
	10, 20, 30, 20, 15, 5, 5, 0, 0, 0,
	0, 60, 15, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};
static u8 end_config_s[] = { RESERVED_T255 };

static const u8 *MXT1644S_config[] = {
	t7_config_s,
	t8_config_s,
	t9_config_s,
	t15_config_s,
	t18_config_s,
	t24_config_s,
	t25_config_s,
	t27_config_s,
	t40_config_s,
	t42_config_s,
	t43_config_s,
	t46_config_s,
	t47_config_s,
	t55_config_s,
	t56_config_s,
	t57_config_s,
	t61_config_s,
	t62_config_s,
	end_config_s,
};

static struct mxt_platform_data mxt1664s_pdata = {
	.max_finger_touches = MXT1664S_MAX_MT_FINGERS,
	.gpio_read_done = GPIO_TSP_INT,
	.min_x = 0,
	.max_x = 4095,
	.min_y = 0,
	.max_y = 4095,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 255,
	.config = MXT1644S_config,
	.power_on = ts_power_on,
	.power_off = ts_power_off,
	.boot_address = 0x26,
	.register_cb = ts_register_callback,
};

static struct i2c_board_info i2c_mxt1664s[] __initdata = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, 0x4A),
		.platform_data = &mxt1664s_pdata,
	},
};
#endif

void __init p4_tsp_init(u32 system_rev)
{
	int gpio = 0, irq = 0;

	printk(KERN_DEBUG "[TSP] %s rev : %u\n",
		__func__, system_rev);

	printk(KERN_DEBUG "[TSP] TSP IC : %s\n",
		(5 <= system_rev) ? "Atmel" : "Synaptics");

	gpio = GPIO_TSP_RST;
	gpio_request(gpio, "TSP_RST");
	gpio_direction_output(gpio, 1);
	gpio_export(gpio, 0);

	gpio = GPIO_TSP_LDO_ON;
	gpio_request(gpio, "TSP_LDO_ON");
	gpio_direction_output(gpio, 1);
	gpio_export(gpio, 0);

	if (5 <= system_rev) {
		gpio = GPIO_TSP_LDO_ON1;
		gpio_request(gpio, "TSP_LDO_ON1");
		gpio_direction_output(gpio, 1);
		gpio_export(gpio, 0);

		gpio = GPIO_TSP_LDO_ON2;
		gpio_request(gpio, "TSP_LDO_ON2");
		gpio_direction_output(gpio, 1);
		gpio_export(gpio, 0);
	} else if (1 <= system_rev)
		have_tsp_ldo = true;

	gpio = GPIO_TSP_INT;
	gpio_request(gpio, "TSP_INT");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(gpio);
	irq = gpio_to_irq(gpio);

#ifdef CONFIG_S3C_DEV_I2C3
	s3c_i2c3_set_platdata(NULL);

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT1664S) && \
	defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S7301)
	if (5 <= system_rev) {
		i2c_mxt1664s[0].irq = irq;
		i2c_register_board_info(3, i2c_mxt1664s,
			ARRAY_SIZE(i2c_mxt1664s));
	} else {
		i2c_synaptics[0].irq = irq;
		i2c_register_board_info(3, i2c_synaptics,
			ARRAY_SIZE(i2c_synaptics));
	}
#endif
#endif	/* CONFIG_S3C_DEV_I2C3 */

}

#if defined(CONFIG_EPEN_WACOM_G5SP)
#include <linux/wacom_i2c.h>
static struct wacom_g5_callbacks *wacom_callbacks;
static int wacom_init_hw(void);
static int wacom_suspend_hw(void);
static int wacom_resume_hw(void);
static int wacom_early_suspend_hw(void);
static int wacom_late_resume_hw(void);
static int wacom_reset_hw(void);
static void wacom_register_callbacks(struct wacom_g5_callbacks *cb);

static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = 0,
	.y_invert = 0,
	.xy_switch = 0,
	.gpio_pendct = GPIO_PEN_PDCT_18V,
#ifdef WACOM_PEN_DETECT
	.gpio_pen_insert = GPIO_S_PEN_IRQ,
#endif
	.init_platform_hw = wacom_init_hw,
	.suspend_platform_hw = wacom_suspend_hw,
	.resume_platform_hw = wacom_resume_hw,
	.early_suspend_platform_hw = wacom_early_suspend_hw,
	.late_resume_platform_hw = wacom_late_resume_hw,
	.reset_platform_hw = wacom_reset_hw,
	.register_cb = wacom_register_callbacks,
};

static struct i2c_board_info i2c_devs6[] __initdata = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", 0x56),
		.platform_data = &wacom_platform_data,
	},
};

static void wacom_register_callbacks(struct wacom_g5_callbacks *cb)
{
	wacom_callbacks = cb;
};

static int wacom_init_hw(void)
{
	int ret;
	ret = gpio_request(GPIO_PEN_LDO_EN, "PEN_LDO_EN");
	if (ret) {
		printk(KERN_ERR "[E-PEN] faile to request gpio(GPIO_PEN_LDO_EN)\n");
		return ret;
	}
	s3c_gpio_cfgpin(GPIO_PEN_LDO_EN, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(GPIO_PEN_LDO_EN, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_PEN_LDO_EN, 0);

	ret = gpio_request(GPIO_PEN_PDCT_18V, "PEN_PDCT");
	if (ret) {
		printk(KERN_ERR "[E-PEN] faile to request gpio(GPIO_PEN_PDCT_18V)\n");
		return ret;
	}
	s3c_gpio_cfgpin(GPIO_PEN_PDCT_18V, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_PEN_PDCT_18V, S3C_GPIO_PULL_UP);

	ret = gpio_request(GPIO_PEN_IRQ_18V, "PEN_IRQ");
	if (ret) {
		printk(KERN_ERR "[E-PEN] faile to request gpio(GPIO_PEN_IRQ_18V)\n");
		return ret;
	}
	s3c_gpio_cfgpin(GPIO_PEN_IRQ_18V, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_PEN_IRQ_18V, S3C_GPIO_PULL_DOWN);
	s5p_register_gpio_interrupt(GPIO_PEN_IRQ_18V);
	i2c_devs6[0].irq = gpio_to_irq(GPIO_PEN_IRQ_18V);

#ifdef WACOM_PEN_DETECT
	s3c_gpio_cfgpin(GPIO_S_PEN_IRQ, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_S_PEN_IRQ, S3C_GPIO_PULL_UP);
#endif

	return 0;
}

static int wacom_suspend_hw(void)
{
	return wacom_early_suspend_hw();
}

static int wacom_resume_hw(void)
{
	return wacom_late_resume_hw();
}

static int wacom_early_suspend_hw(void)
{
	gpio_set_value(GPIO_PEN_LDO_EN, 0);
	return 0;
}

static int wacom_late_resume_hw(void)
{
	gpio_set_value(GPIO_PEN_LDO_EN, 1);
	return 0;
}

static int wacom_reset_hw(void)
{
	return 0;
}

void __init p4_wacom_init(void)
{
	wacom_init_hw();
#ifdef CONFIG_S3C_DEV_I2C6
	s3c_i2c6_set_platdata(NULL);
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
#endif
}
#endif	/* CONFIG_EPEN_WACOM_G5SP */

#if defined(CONFIG_KEYBOARD_GPIO)
#include <mach/sec_debug.h>
#include <linux/gpio_keys.h>
#define GPIO_KEYS(_code, _gpio, _active_low, _iswake, _hook)	\
{							\
	.code = _code,					\
	.gpio = _gpio,					\
	.active_low = _active_low,			\
	.type = EV_KEY,					\
	.wakeup = _iswake,				\
	.debounce_interval = 10,			\
	.isr_hook = _hook,				\
	.value = 1					\
}

struct gpio_keys_button p4_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP,
		  1, 1, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN,
		  1, 1, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
};

struct gpio_keys_platform_data p4_gpiokeys_platform_data = {
	p4_buttons,
	ARRAY_SIZE(p4_buttons),
};

static struct platform_device p4_keypad = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &p4_gpiokeys_platform_data,
	},
};
#endif
void __init p4_key_init(void)
{
#if defined(CONFIG_KEYBOARD_GPIO)
	platform_device_register(&p4_keypad);
#endif
}

