/*
 * Module
 *
 * Copyright (C) 2017, Posch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME		"firdata"

#define FIR_DATA_RESET	((s32) 0x0)
#define FIR_DATA_DATA_LEN	((u32) 1)


/* ----------------------------------------------------------------------------
 * Function declarations
 * ----------------------------------------------------------------------------
 */

/* This function is called on read to the char file */
static ssize_t file_read(struct file *filp, char __user *buff,
				size_t count, loff_t *offp);

/* This function is called on write to the char file */
static ssize_t file_write(struct file *filp, const char __user *buff,
				size_t count, loff_t *offp);

/* ----------------------------------------------------------------------------
 * Global Varbles
 * ----------------------------------------------------------------------------
 */

/* This struct holds the file access functions */
static const struct file_operations fops = {
	.read		= file_read,
	.write		= file_write
};

struct firdata_dat {
	void *regs;
	int size;
	struct miscdevice misc;
	s32 buffer[FIR_DATA_DATA_LEN];
};

/* ----------------------------------------------------------------------------
 * Function definitions
 * ----------------------------------------------------------------------------
 */


static inline struct firdata_dat *to_my_struct(struct file *file)
{
	struct miscdevice *miscdev = file->private_data;

	return container_of(miscdev, struct firdata_dat, misc);
}

static ssize_t file_read(struct file *filp, char __user *buff,
					size_t count, loff_t *offp)
{
	struct firdata_dat *data = to_my_struct(filp);
	int transfer;
	int max;
	int writeBytes;

	max = data->size - *offp;

	if (max > count)
		writeBytes = count;
	else
		writeBytes = max;

	if (writeBytes <= 0)
		return 0;

	transfer = writeBytes -
		copy_to_user(buff, data->buffer + *offp, writeBytes);

	*offp += transfer;
	return transfer;
}

static ssize_t file_write(struct file *filp, const char __user *buff,
					size_t count, loff_t *offp)
{
	struct firdata_dat *data = to_my_struct(filp);
	int transfer;
	int max;
	int writeBytes;
	int offs;

	max = data->size - *offp;

	if (max > count)
		writeBytes = count;
	else
		writeBytes = max;

	if (writeBytes <= 0)
		return -EINVAL;

	transfer = writeBytes -
		copy_from_user(data->buffer + *offp, buff, writeBytes);

	iowrite32(*(data->buffer), (data->regs)+offs*4);

	*offp += transfer;
	return transfer;
}

static int firdata_probe(struct platform_device *pdev)
{
	struct firdata_dat *data;
	struct resource *io;
	int retval;

	/* alloc space for the data structure */
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, data);

	/* get io resource */
	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(data->regs))
		return PTR_ERR(data->regs);
	data->size = io->end - io->start + 1;

	data->misc.name   = DRIVER_NAME;
	data->misc.minor  = MISC_DYNAMIC_MINOR;
	data->misc.fops   = &fops;
	data->misc.parent = &pdev->dev;
	retval = misc_register(&data->misc);
	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	dev_info(&pdev->dev, "altrsevseg started");

	iowrite8(FIR_DATA_RESET, data->regs);

	return 0;
}

static int firdata_remove(struct platform_device *pdev)
{
	struct firdata_dat *data = platform_get_drvdata(pdev);

	/* Reset the device */
	iowrite8(FIR_DATA_RESET, data->regs);

	misc_deregister(&data->misc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id firdata_of_match[] = {
	{ .compatible = "asps,de1soc-firdata", },
	{ },
};
MODULE_DEVICE_TABLE(of, firdata_of_match);

static struct platform_driver firdata_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(firdata_of_match),
	},
	.probe		= firdata_probe,
	.remove		= firdata_remove,
};

module_platform_driver(firdata_driver);

MODULE_AUTHOR("Johannes Posch");
MODULE_DESCRIPTION("Fir-Filter data Device Functions");
MODULE_LICENSE("GPL v2");
