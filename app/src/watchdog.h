#ifndef WATCH_DOG_H
#define WATCH_DOG_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

int dog_init();
const struct device *get_dog();
int get_dog_channel();

#endif