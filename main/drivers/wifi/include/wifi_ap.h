/**
 * @file wifi_ap.h
 * @brief WiFi Access Point driver
 * @author
 * @date 2025-11-19
 */

#ifndef _WIFI_AP_H_
#define _WIFI_AP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------------*/
#include <stdint.h>


/* Exported types ----------------------------------------------------------*/

typedef struct
{
    void (*on_start)(void);
    void (*on_stop)(void);
    void (*on_client_connected)(uint8_t mac[6]);
    void (*on_client_disconnected)(uint8_t mac[6]);
} wifi_ap_callback_t;

/* Exported macros ---------------------------------------------------------*/

/* Exported functions ------------------------------------------------------*/

int wifi_ap_init();
int wifi_ap_deinit();
int wifi_ap_start();
int wifi_ap_stop();
int wifi_ap_config();

int wifi_ap_register_callback(wifi_ap_callback_t *callback);

#ifdef __cplusplus
}
#endif

#endif /* _WIFI_AP_H_ */
