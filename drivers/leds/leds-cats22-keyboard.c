/*
 * Cat S22 Flip Keyboard Backlight LED Driver
 * Binds to "qcom,leds-gpio-keyboard_light" node (GPIO 130)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

struct cats22_kb_led_data {
    struct led_classdev cdev;
    int gpio;
};

static void cats22_kb_brightness_set(struct led_classdev *led_cdev,
                                     enum led_brightness value)
{
    struct cats22_kb_led_data *data =
        container_of(led_cdev, struct cats22_kb_led_data, cdev);

    gpio_set_value(data->gpio, value > 0 ? 1 : 0);
}

static int cats22_kb_led_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct cats22_kb_led_data *data;
    const char *name = "keyboard_light";
    const char *default_trigger = NULL;
    int ret;

    if (!np)
        return -ENODEV;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->gpio = of_get_named_gpio(np, "qcom,keyboard_light", 0);
    if (data->gpio < 0) {
        /* Fallback: try standard gpios */
        data->gpio = of_get_gpio(np, 0);
        if (data->gpio < 0) {
            dev_err(dev, "Failed to get gpio from qcom,keyboard_light: %d\n", data->gpio);
            return data->gpio;
        }
    }

    ret = devm_gpio_request_one(dev, data->gpio, GPIOF_OUT_INIT_LOW, "keyboard_light");
    if (ret) {
        dev_err(dev, "Failed to request keyboard_light gpio %d: %d\n", data->gpio, ret);
        return ret;
    }

    of_property_read_string(np, "linux,name", &name);
    of_property_read_string(np, "linux,default-trigger", &default_trigger);

    data->cdev.name = name;
    data->cdev.default_trigger = default_trigger;
    data->cdev.brightness_set = cats22_kb_brightness_set;
    data->cdev.max_brightness = 255;
    data->cdev.brightness = LED_OFF;

    ret = devm_led_classdev_register(dev, &data->cdev);
    if (ret) {
        dev_err(dev, "Failed to register keyboard_light led classdev: %d\n", ret);
        return ret;
    }

    platform_set_drvdata(pdev, data);
    dev_info(dev, "Registered Cat S22 Flip keyboard_light on GPIO %d\n", data->gpio);

    return 0;
}

static const struct of_device_id cats22_kb_led_of_match[] = {
    { .compatible = "qcom,leds-gpio-keyboard_light", },
    { },
};
MODULE_DEVICE_TABLE(of, cats22_kb_led_of_match);

static struct platform_driver cats22_kb_led_driver = {
    .probe = cats22_kb_led_probe,
    .driver = {
        .name = "cats22-keyboard-light",
        .of_match_table = cats22_kb_led_of_match,
    },
};

module_platform_driver(cats22_kb_led_driver);

MODULE_DESCRIPTION("Cat S22 Flip Keyboard Backlight Driver");
MODULE_LICENSE("GPL v2");
