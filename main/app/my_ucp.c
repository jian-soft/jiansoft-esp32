/*
 * SPDX-FileCopyrightText: 2025 Jiansoft
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file my_ucp.c
 * @brief Brief description of the implementation
 * @author Jiansoft
 * @date 2025-11-19
 */

/* Includes ----------------------------------------------------------------*/
#include "ucp.h"
#include "os_interfaces.h"
#include "my_ucp.h"

#include "esp_log.h"
/* Private typedef ---------------------------------------------------------*/

/* Private define ----------------------------------------------------------*/

/* Private variables -------------------------------------------------------*/
static const char *TAG = "my_ucp";

int g_is_ucp_full = 0;
/* Private function prototypes ---------------------------------------------*/

/* Private functions -------------------------------------------------------*/
void ucp_sndbuf_num_cb(int32_t num, int32_t wnd, int32_t state)
{
    if (num > (wnd*2/3)) {
        g_is_ucp_full = 1;
    } else {
        g_is_ucp_full = 0;
    }
}

void server_rx_msg_cb(uint8_t *msg, int32_t len, ucp_pkt_info_t *info)
{

}

void server_rx_video_cb(uint8_t *msg, int32_t len, ucp_pkt_info_t *info)
{

}

void server_rx_audio_cb(uint8_t *msg, int32_t len, ucp_pkt_info_t *info)
{

}


void* send_test_data_thread(void *arg)
{
    static uint8_t video_buf[20836];

    while (1) {
        usleep(8333);  //8.3ms 120fps
        //usleep(33333);  //33.3ms 30fps
        if (!ucp_get_conn_state()) {
            continue;
        }

        if (!g_is_ucp_full) {
            ucp_send_video(video_buf, sizeof(video_buf), 1);
        } else {
            printf("dddd: ucp is full\n");
        }
    }

    return NULL;
}

/* Exported functions ------------------------------------------------------*/
int my_ucp_server_init(void)
{
    int ret;
    ucp_register_callback(server_rx_msg_cb, server_rx_video_cb,
                          server_rx_audio_cb, ucp_sndbuf_num_cb);

    ret = ucp_server_init();
    if (0 != ret) {
        ESP_LOGE(TAG, "ucp_server_init fail");
        return ret;
    }

    ESP_LOGI(TAG, "ucp_server_init successful");
    //os_create_thread(send_test_data_thread, NULL);
    return 0;
}
