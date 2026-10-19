#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Phase 8 — SoftAP + read-only HTTP dashboard.
 * Endpoints are GET-only; no actuator control.
 */
esp_err_t web_server_start(system_status_t *status);

/** Soft-fail friendly: true after AP + HTTP started. */
bool web_server_is_running(void);

/** SSID currently advertised (empty if not running). */
void web_server_copy_ssid(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
