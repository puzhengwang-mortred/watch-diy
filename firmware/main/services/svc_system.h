#pragma once

#include "esp_err.h"

/** NVS and other early platform init; call once before UI/BSP that need NVS. */
esp_err_t svc_system_early_init(void);
