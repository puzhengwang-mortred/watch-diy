#include "svc_system.h"

#include "esp_check.h"
#include "nvs_flash.h"

esp_err_t svc_system_early_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), "svc_system", "nvs erase");
        err = nvs_flash_init();
    }
    return err;
}
