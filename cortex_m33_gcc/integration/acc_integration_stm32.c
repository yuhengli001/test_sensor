// Copyright (c) Acconeer AB, 2019-2024
// All rights reserved
// This file is subject to the terms and conditions defined in the file
// 'LICENSES/license_acconeer.txt', (BSD 3-Clause License) which is part
// of this source code package.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#include "acc_integration.h"

static uint32_t periodic_interval_ms = 0;
static uint32_t next_wakeup_tick     = 0;

void acc_integration_sleep_ms(uint32_t time_msec)
{
	HAL_Delay(time_msec);
}

void acc_integration_sleep_us(uint32_t time_usec)
{
	uint32_t time_msec = (time_usec / 1000);

	HAL_Delay(time_msec);
}

uint32_t acc_integration_get_time(void)
{
	return HAL_GetTick();
}

void *acc_integration_mem_alloc(size_t size)
{
	return malloc(size);
}

void *acc_integration_mem_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void acc_integration_mem_free(void *ptr)
{
	free(ptr);
}


void acc_integration_set_periodic_wakeup(uint32_t time_msec)
{
	periodic_interval_ms = time_msec;
	next_wakeup_tick     = HAL_GetTick() + time_msec;
}


void acc_integration_sleep_until_periodic_wakeup(void)
{
	if (periodic_interval_ms > 0)
	{
		uint32_t now = HAL_GetTick();

		if (next_wakeup_tick > now)
		{
			HAL_Delay(next_wakeup_tick - now);
		}

		next_wakeup_tick += periodic_interval_ms;
	}
}

