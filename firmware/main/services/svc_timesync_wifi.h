#pragma once

/** After UI + RTC: optional WiFi STA + SNTP, then write PCF85063 (see Kconfig Watch WiFi / SNTP). */
void svc_timesync_wifi_start(void);
