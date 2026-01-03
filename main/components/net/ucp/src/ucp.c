#include "os_interfaces.h"
#include "utils.h"
#include "ucp_private.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
/* @role: ucp_role_t,
   @server_ip: server's ip addr, like: "192.168.7.1"; null when role is server
*/
int32_t __ucp_init(ucp_role_t role, const char *server_ip)
{
    int ret;
    ucp_sock_t *sock = ucp_get_sock();
    if (sock->inited) {
        os_log("ucp_init: has inited.\n");
        return 0;
    }

    INIT_LIST_HEAD(&sock->snd_queue);
    INIT_LIST_HEAD(&sock->rcv_queue);
    INIT_LIST_HEAD(&sock->snd_buf);
    INIT_LIST_HEAD(&sock->rcv_buf);
    sock->mtu = UCP_DEFAULT_MTU;
    sock->snd_wnd = UCP_SND_WINDOW_SIZE;
    sock->rcv_wnd = UCP_RCV_WINDOW_SIZE;

    ret = os_mutex_init(&(sock->sndq_mutex));
    ret |= os_mutex_init(&(sock->rcvq_mutex));
    ret |= os_mutex_init(&(sock->bak_mutex));
    if (0 != ret) {
        os_log("ucp_init: os_mutex_init fail.\n");
        return -1;
    }
    ret = os_sem_init(&sock->sndq_sem);
    ret |= os_sem_init(&sock->rcvq_sem);
    if (0 != ret) {
        os_log("ucp_init: os_sem_init fail.\n");
        return -1;
    }

    sock->role = role;
    if (UCP_ROLE_CLIENT == role && server_ip) {
        ret = ucp_init_udp_client(sock, server_ip);
    } else if (UCP_ROLE_SERVER == role) {
        ret = ucp_init_udp_server(sock);
    } else {
        os_log("ucp_init fail. invalid role or server_ip:%d\n", role);
        return -1;
    }
    if (0 != ret) {
        os_log("ucp_init_udp fail. role:%d\n", role);
        return -1;
    }

    ret = ucp_init_tx();
    if (0 != ret) {
        os_log("ucp_init_tx fail\n");
        return -1;
    }

    ret = ucp_init_rx();
    if (0 != ret) {
        os_log("ucp_init_rx fail\n");
        return -1;
    }

    sock->inited = 1;
    os_log("ucp_init successful.\n");
    return ret;
}

int32_t ucp_client_init(const char *server_ip)
{
    return __ucp_init(UCP_ROLE_CLIENT, server_ip);
}

int32_t ucp_server_init()
{
    return __ucp_init(UCP_ROLE_SERVER, NULL);
}

void ucp_register_callback(
                RX_CB_FUN rx_msg_cb,
                RX_CB_FUN rx_video_cb,
                RX_CB_FUN rx_audio_cb,
                SNDBUF_NUM_CB_FUN sndbuf_num_cb)
{
    ucp_register_rx_cb(rx_msg_cb, rx_video_cb, rx_audio_cb);
    ucp_register_sndbuf_num_cb(sndbuf_num_cb);
}

ucp_stat_t* ucp_get_stats()
{
    ucp_sock_t *sock = ucp_get_sock();
    return &sock->stat_res;
}

void ucp_calc_stats()
{
    ucp_sock_t *sock;
    ucp_stat_process_t *curr_stat, *last_stat;
    ucp_stat_t *stat_res;
    uint32_t curr_ms, delta_ms;

    if (!ucp_get_conn_state()) {
        return;
    }

    sock = ucp_get_sock();
    curr_ms = os_get_ms_ts();
    if (0 == sock->last_stat_ms) {
        sock->last_stat_ms = curr_ms;
        return;
    }

    delta_ms = utils_u32_diff(curr_ms, sock->last_stat_ms);
    if (delta_ms < 1000) {
        return;
    }
    sock->last_stat_ms = curr_ms;

    curr_stat = &sock->curr_stat;
    last_stat = &sock->last_stat;
    stat_res = &sock->stat_res;

    stat_res->snd_kbps = (curr_stat->snd_all_bytes - last_stat->snd_all_bytes) * 8 / delta_ms;
    stat_res->rcv_kbps = (curr_stat->rcv_all_bytes - last_stat->rcv_all_bytes) * 8 / delta_ms;

    if (UCP_ROLE_CLIENT == sock->role) {
        uint32_t delta_rcv_all, delta_loss;
        delta_rcv_all = curr_stat->rcv_video_cnt - last_stat->rcv_video_cnt;
        delta_loss = curr_stat->rcv_loss_cnt - last_stat->rcv_loss_cnt;
        if (delta_rcv_all != 0) {
            stat_res->loss_rate = delta_loss*100/(delta_rcv_all+delta_loss);
        }
    }

    stat_res->max_retry_times = curr_stat->max_retry_times_tmp;
    curr_stat->max_retry_times_tmp = 0;

    stat_res->video_fps = (curr_stat->rcv_frame_cnt - last_stat->rcv_frame_cnt)*1000/delta_ms;
    stat_res->rcv_hb_ps = (curr_stat->rcv_hb_cnt - last_stat->rcv_hb_cnt)*1000/delta_ms;
    stat_res->audio_hz = (curr_stat->rcv_audio_cnt - last_stat->rcv_audio_cnt)*1000/delta_ms;

    *last_stat = *curr_stat;
}

void ucp_print_stats()
{
    ucp_sock_t *sock;
    ucp_stat_t *stat_res;
    ucp_stat_process_t *stat_curr;

    if (!ucp_get_conn_state()) {
        return;
    }

    sock = ucp_get_sock();
    stat_res = &sock->stat_res;
    stat_curr = &sock->curr_stat;

    if (UCP_ROLE_SERVER == sock->role) {
        os_log("UCP: TX:%ukbps, RX:%ukbps,fps:%u, hps:%u, vdavg:%u, tx_r:%u, rx_du:%u, aud:%u\r\n",
                stat_res->snd_kbps,
                stat_res->rcv_kbps,
                stat_res->video_fps,
                stat_res->rcv_hb_ps,
                stat_res->vdly_avg,
                stat_res->retry_cnt,
                stat_res->rcv_dup_cnt,
                stat_res->audio_hz);
    } else {
        os_log("UCP: TX:%ukbps, RX:%ukbps, fps:%u, hps:%u, cdavg:%u, tx_r:%u, rx_du:%u, aud:%u\r\n",
                stat_res->snd_kbps,
                stat_res->rcv_kbps,
                stat_res->video_fps,
                stat_res->rcv_hb_ps,
                stat_res->rdly_avg,
                stat_res->retry_cnt,
                stat_res->rcv_dup_cnt,
                stat_res->audio_hz);
        if (stat_curr->rcv_video_cnt != 0) {
            os_log("rx_all:%u, loss:%u, lrate:%u\r\n",
                (unsigned int)stat_curr->rcv_video_cnt,
                (unsigned int)stat_curr->rcv_loss_cnt,
                (unsigned int)stat_res->loss_rate);
        }
    }
}

void ucp_calc_and_print_stats()
{
    ucp_calc_stats();
    ucp_print_stats();
}

#pragma GCC diagnostic pop