#ifndef CUSTOM_SERVICE_H
#define CUSTOM_SERVICE_H

#include <zephyr/types.h>

void custom_service_init(void);
void custom_service_notify(uint8_t value);

#endif