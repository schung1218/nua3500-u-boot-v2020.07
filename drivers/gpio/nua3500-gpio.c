// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Nuvoton Technology Corp.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/bitops.h>

#define GPIO_MODE 0x0
#define GPIO_DOUT 0x4
#define GPIO_PIN 0x8

/*!< Input Mode */
#define GPIO_MODE_INPUT 0x0UL
/*!< Output Mode */
#define GPIO_MODE_OUTPUT 0x1UL
/*!< Open-Drain Mode */
#define GPIO_MODE_OPEN_DRAIN 0x2UL
/*!< Quasi-bidirectional Mode */
#define GPIO_MODE_QUASI 0x3UL

#define GPIO_PIN_DATA(base, pin) \
	(*((unsigned int *)((base+0x800) + ((pin)<<2))))

/*!< Generate the MODE mode setting for each pin  */
#define GPIO_SET_MODE(pin, mode) ((mode) << ((pin)<<1))

#define GPIO_PIN_DATA(base, pin) \
	(*((unsigned int *)((base+0x800) + ((pin)<<2))))

struct nua3500_gpio_priv {
	void __iomem *regs;
	int bank;
};

static int nua3500_gpio_get_value(struct udevice *dev,
					unsigned int offset)
{
	struct nua3500_gpio_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;

	return GPIO_PIN_DATA(base, offset);
}

static int nua3500_gpio_set_value(struct udevice *dev,
					unsigned int gpio_num,
				  int val)
{
	struct nua3500_gpio_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;

	GPIO_PIN_DATA(base, gpio_num) = val;
	return 0;
}

static void nua3500_gpio_set_direction(struct udevice *dev,
					unsigned int gpio_num,
					bool val)
{
	struct nua3500_gpio_priv *priv = dev_get_priv(dev);
	unsigned long value;
	void __iomem *base = priv->regs;

	value = __raw_readl(base + GPIO_MODE);
	value &= ~GPIO_SET_MODE(gpio_num, GPIO_MODE_QUASI);
	value |= GPIO_SET_MODE(gpio_num, val & 0x1);
	__raw_writel(value, base + GPIO_MODE);
}

static int nua3500_gpio_direction_input(struct udevice *dev,
					unsigned int offset)
{
	nua3500_gpio_set_direction(dev, offset, false);
	return 0;
}

static int nua3500_gpio_direction_output(struct udevice *dev,
					unsigned int offset,
					int value)
{

	/* write GPIO value to output before selecting output mode of pin */
	nua3500_gpio_set_value(dev, offset, value);
	nua3500_gpio_set_direction(dev, offset, true);
	return 0;
}

static int nua3500_gpio_get_function(struct udevice *dev,
					unsigned int offset)
{
	struct nua3500_gpio_priv *priv = dev_get_priv(dev);
	unsigned long value;
	void __iomem *base = priv->regs;

	value = __raw_readl(base + GPIO_MODE);
	if ((value >> (offset << 1)) & 0x1)
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static const struct dm_gpio_ops nua3500_gpio_ops = {
	.direction_input = nua3500_gpio_direction_input,
	.direction_output = nua3500_gpio_direction_output,
	.get_value = nua3500_gpio_get_value,
	.set_value = nua3500_gpio_set_value,
	.get_function = nua3500_gpio_get_function,
};

static int nua3500_gpio_probe(struct udevice *dev)
{
	char *str, name[32];
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct nua3500_gpio_priv *priv = dev_get_priv(dev);

	priv->bank = dev_get_driver_data(dev);
	__raw_writel(__raw_readl(0x40460000 + 0x204) | (priv->bank << 16),
		0x40460000 + 0x208);	//Enable GPIO
	uc_priv->gpio_count = 16;
	uc_priv->gpio_base = priv->bank * uc_priv->gpio_count;

	strncpy(name, dev->name, 5);
	name[5] = '\0';
	str = strdup(name);
	if (!str)
		return -ENOMEM;
	uc_priv->bank_name = str;
	priv->regs = dev_remap_addr_index(dev, 0);
	return 0;
}

U_BOOT_DRIVER(nua3500_gpio) = {
	.name = "nua3500_gpio",
	.id = UCLASS_GPIO,
	.ops = &nua3500_gpio_ops,
	.priv_auto_alloc_size = sizeof(struct nua3500_gpio_priv),
	.probe = nua3500_gpio_probe,
};
