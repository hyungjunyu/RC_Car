/*
 * Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether expressed or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 */

#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>

#define TPIC2810_WS_COMMAND 0x44

/**
 * struct tpic2810 - GPIO driver data
 * @chip: GPIO controller chip
 * @client: I2C device pointer
 * @buffer: Buffer for device register
 * @lock: Protects write sequences
  * struct tpic2810 - GPIO 드라이버 데이터
  * @ 칩 : GPIO 컨트롤러 칩
  * @client : I2C 장치 포인터
  * @buffer : 디바이스 레지스터 용 버퍼
  * @lock : 기입 순서를 보호합니다.
 */
struct tpic2810 {
	struct gpio_chip chip;
	struct i2c_client *client;
	u8 buffer;
	struct mutex lock;
};

static inline struct tpic2810 *to_tpic2810(struct gpio_chip *chip)
{
	return container_of(chip, struct tpic2810, chip);
}

static void tpic2810_set(struct gpio_chip *chip, unsigned offset, int value);

static int tpic2810_get_direction(struct gpio_chip *chip,
				  unsigned offset)
{
	/* This device always output */
	return 0;
}

static int tpic2810_direction_input(struct gpio_chip *chip,
				    unsigned offset)
{
	/* This device is output only */
	return -EINVAL;
}

static int tpic2810_direction_output(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	/* This device always output */
	tpic2810_set(chip, offset, value);
	return 0;
}

static void tpic2810_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct tpic2810 *gpio = to_tpic2810(chip);

	mutex_lock(&gpio->lock);

	if (value)
		gpio->buffer |= BIT(offset);
	else
		gpio->buffer &= ~BIT(offset);

	i2c_smbus_write_byte_data(gpio->client, TPIC2810_WS_COMMAND,
				  gpio->buffer);

	mutex_unlock(&gpio->lock);
}

static void tpic2810_set_multiple(struct gpio_chip *chip, unsigned long *mask,
				  unsigned long *bits)
{
	struct tpic2810 *gpio = to_tpic2810(chip);

	mutex_lock(&gpio->lock);

	/* clear bits under mask */
	gpio->buffer &= ~(*mask);
	/* set bits under mask */
	gpio->buffer |= ((*mask) & (*bits));

	i2c_smbus_write_byte_data(gpio->client, TPIC2810_WS_COMMAND,
				  gpio->buffer);

	mutex_unlock(&gpio->lock);
}

static struct gpio_chip template_chip = {
	.label			= "tpic2810",
	.owner			= THIS_MODULE,
	.get_direction		= tpic2810_get_direction,
	.direction_input	= tpic2810_direction_input,
	.direction_output	= tpic2810_direction_output,
	.set			= tpic2810_set,
	.set_multiple		= tpic2810_set_multiple,
	.base			= -1,
	.ngpio			= 8,
	.can_sleep		= true,
};

static const struct of_device_id tpic2810_of_match_table[] = {
	{ .compatible = "ti,tpic2810" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tpic2810_of_match_table);

static int tpic2810_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct tpic2810 *gpio;
	int ret;

	gpio = devm_kzalloc(&client->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	i2c_set_clientdata(client, gpio);

	gpio->chip = template_chip;
	gpio->chip.dev = &client->dev;

	gpio->client = client;

	mutex_init(&gpio->lock);

	ret = gpiochip_add(&gpio->chip);
	if (ret < 0) {
		dev_err(&client->dev, "Unable to register gpiochip\n");
		return ret;
	}

	return 0;
}

static int tpic2810_remove(struct i2c_client *client)
{
	struct tpic2810 *gpio = i2c_get_clientdata(client);

	gpiochip_remove(&gpio->chip);

	return 0;
}

static const struct i2c_device_id tpic2810_id_table[] = {
	{ "tpic2810", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, tpic2810_id_table);

static struct i2c_driver tpic2810_driver = {
	.driver = {
		.name = "tpic2810",
		.of_match_table = tpic2810_of_match_table,
	},
	.probe = tpic2810_probe,
	.remove = tpic2810_remove,
	.id_table = tpic2810_id_table,
};
module_i2c_driver(tpic2810_driver);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("TPIC2810 8-Bit LED Driver GPIO Driver");
MODULE_LICENSE("GPL v2");
