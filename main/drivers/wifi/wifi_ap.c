/**
 * @file wifi_ap.c
 * @brief WiFi Access Point driver implementation
 * @author
 * @date 2025-11-19
 */

/* Includes ----------------------------------------------------------------*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi_ap.h"

/* 参考
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi.html
https://github.com/espressif/esp-idf/tree/d3ffbccf/examples/wifi/getting_started/softAP
*/

/* Private typedef ---------------------------------------------------------*/

/* Private define ----------------------------------------------------------*/

/* Private variables -------------------------------------------------------*/

/* Private function prototypes ---------------------------------------------*/

/* Private functions -------------------------------------------------------*/

/* Exported functions ------------------------------------------------------*/


