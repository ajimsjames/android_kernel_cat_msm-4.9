/*
 * Cat S22 Flip Hall Switch Driver
 * Driver for "hall-switch" node (GPIO 44)
 * Emits standard EV_SW / SW_LID events to Linux input subsystem
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

struct hall_switch_data {
    struct input_dev *input_dev;
    int gpio;
    int irq;
    bool wakeup;
};

static irqreturn_t hall_irq_handler(int irq, void *dev_id)
{
    struct hall_switch_data *data = dev_id;
    int state = gpio_get_value(data->gpio);

    /* GPIO 44: High (pull-up) = flip open (SW_LID = 0), Low (magnet near) = flip closed (SW_LID = 1) */
    pr_info("hall_switch: irq triggered, gpio val=%d\n", state);
    input_report_switch(data->input_dev, SW_LID, !state);
    input_sync(data->input_dev);

    return IRQ_HANDLED;
}

static int hall_switch_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct hall_switch_data *data;
    int ret;

    if (!np)
        return -ENODEV;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->gpio = of_get_named_gpio(np, "linux,gpio-int", 0);
    if (data->gpio < 0) {
        dev_err(dev, "Failed to get gpio from linux,gpio-int: %d\n", data->gpio);
        return data->gpio;
    }

    ret = devm_gpio_request_one(dev, data->gpio, GPIOF_IN, "hall_switch");
    if (ret) {
        dev_err(dev, "Failed to request hall gpio %d: %d\n", data->gpio, ret);
        return ret;
    }

    data->irq = gpio_to_irq(data->gpio);
    if (data->irq < 0) {
        dev_err(dev, "Failed to get irq for hall gpio %d\n", data->gpio);
        return data->irq;
    }

    data->wakeup = of_property_read_bool(np, "linux,wakeup");

    data->input_dev = devm_input_allocate_device(dev);
    if (!data->input_dev)
        return -ENOMEM;

    data->input_dev->name = "hall_switch";
    data->input_dev->phys = "hall_switch/input0";
    data->input_dev->id.bustype = BUS_HOST;

    input_set_capability(data->input_dev, EV_SW, SW_LID);

    ret = input_register_device(data->input_dev);
    if (ret) {
        dev_err(dev, "Failed to register hall input device: %d\n", ret);
        return ret;
    }

    /* Initial state */
    input_report_switch(data->input_dev, SW_LID, !gpio_get_value(data->gpio));
    input_sync(data->input_dev);

    ret = devm_request_threaded_irq(dev, data->irq, NULL, hall_irq_handler,
                                    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                    "hall_switch", data);
    if (ret) {
        dev_err(dev, "Failed to request threaded irq %d: %d\n", data->irq, ret);
        return ret;
    }

    if (data->wakeup)
        enable_irq_wake(data->irq);

    platform_set_drvdata(pdev, data);
    pr_info("hall_switch: successfully probed on gpio %d, irq %d\n", data->gpio, data->irq);

    return 0;
}

static int hall_switch_remove(struct platform_device *pdev)
{
    struct hall_switch_data *data = platform_get_drvdata(pdev);

    if (data && data->wakeup)
        disable_irq_wake(data->irq);

    return 0;
}

static const struct of_device_id hall_switch_of_match[] = {
    { .compatible = "hall-switch", },
    { },
};
MODULE_DEVICE_TABLE(of, hall_switch_of_match);

static struct platform_driver hall_switch_driver = {
    .probe = hall_switch_probe,
    .remove = hall_switch_remove,
    .driver = {
        .name = "hall_switch",
        .of_match_table = hall_switch_of_match,
    },
};

module_platform_driver(hall_switch_driver);

MODULE_DESCRIPTION("Cat S22 Flip Hall Switch Driver");
MODULE_LICENSE("GPL v2");
