#include "utils.h"
#include "list.h"
#include "ucp_private.h"

#define UCP_INTERNAL_MSG_TYPE_VID_DELAY  1
#define UCP_INTERNAL_MSG_TYPE_RC_DELAY   2
#define UCP_INTERNAL_MSG_TYPE_VDLY_USB   3
#define UCP_INTERNAL_MSG_TOTALLEN_VID_DELAY 10  //"UCP:"+1B len+1B type+4B dly
#define UCP_INTERNAL_MSG_TOTALLEN_RC_DELAY  14  //"UCP:"+1B len+1B type+4B dly+4B dly
#define UCP_INTERNAL_MSG_BODYLEN_VID_DELAY  5   //1B type+4B dly
#define UCP_INTERNAL_MSG_BODYLEN_RC_DELAY   9   //1B type+4B dly+4B dly


static RX_CB_FUN g_rx_msg_cb;
static RX_CB_FUN g_rx_video_cb;
static RX_CB_FUN g_rx_audio_cb;

/* 接收路径函数：
    ucp_recv(): 用户侧接收函数，阻塞式，每次返回的包长度不超过一个MTU，
                将报文从接收队列拷贝给用户，然后将报文从接收队列里释放

    ucp_parse_una(): 处理接收ucp报文中的una, 释放snd_buf中sn<una的包
    ucp_parse_bak(): hb包中携带bak，根据block ack信息释放snd_buf中已确认的包
    ucp_parse_hb(): 处理hb包，此函数调用ucp_parse_bak
    ucp_parse_data(): 梳理非hb包，将包放入rcv_buf队列进行缓存排序
    ucp_make_bak(): 根据rcv_buf，生成block ack bitmap

    ucp_input(): 下层协议收包后调用，ucp协议栈收包入口。调用完上面5个函数后，
                 将sn连续的包转移到rcv_q, 通知应用层接收

*/
static inline void ucp_handle_video_delay_message(uint8_t *msg)
{
    uint32_t *last_ts, cur_ts;
    int32_t delay;
    int32_t res_idx;
    int32_t *res;
    ucp_sock_t *sock = ucp_get_sock();

    last_ts = (uint32_t *)(msg + 6);
    cur_ts = os_get_us_ts();
    delay = utils_u32_diff(cur_ts, *last_ts);

    res_idx = sock->stat_res.vdly_res_idx;
    res = sock->stat_res.videodelay_res;

    //如果当前delay是最小值，则更新最小delay index
    if (0 == res[sock->stat_res.vdly_min_idx]) {
        sock->stat_res.vdly_min_idx = 0;
    } else if (delay < res[sock->stat_res.vdly_min_idx]) {
        sock->stat_res.vdly_min_idx = res_idx;
    }

    //如果当前delay是最大值，则更新最大delay index
    if (delay > res[sock->stat_res.vdly_max_idx]) {
        sock->stat_res.vdly_max_idx = res_idx;
    }

    sock->stat_res.vdly_sum -= res[res_idx];
    sock->stat_res.vdly_sum += delay;
    sock->stat_res.vdly_avg = sock->stat_res.vdly_sum/UCP_STAT_RES_LEN;

    res[res_idx] = delay;

    res_idx++;
    if (res_idx >= UCP_STAT_RES_LEN) {
        res_idx = 0;
    }
    sock->stat_res.vdly_res_idx = res_idx;
}

static inline void ucp_handle_usb_vdly_message(uint8_t *msg)
{
    uint32_t *last_ts, cur_ts;
    int32_t delay;
    int32_t res_idx;
    int32_t *res;
    ucp_sock_t *sock = ucp_get_sock();

    last_ts = (uint32_t *)(msg + 6);
    cur_ts = os_get_us_ts();
    delay = utils_u32_diff(cur_ts, *last_ts);

    res_idx = sock->stat_res.usb_vdly_res_idx;
    res = sock->stat_res.usb_vdly_res;

    //如果当前delay是最小值，则更新最小delay index
    if (0 == res[sock->stat_res.usb_vdly_min_idx]) {
        sock->stat_res.usb_vdly_min_idx = 0;
    } else if (delay < res[sock->stat_res.usb_vdly_min_idx]) {
        sock->stat_res.usb_vdly_min_idx = res_idx;
    }

    //如果当前delay是最大值，则更新最大delay index
    if (delay > res[sock->stat_res.usb_vdly_max_idx]) {
        sock->stat_res.usb_vdly_max_idx = res_idx;
    }

    sock->stat_res.usb_vdly_sum -= res[res_idx];
    sock->stat_res.usb_vdly_sum += delay;
    sock->stat_res.usb_vdly_avg = sock->stat_res.usb_vdly_sum/UCP_STAT_RES_LEN;

    res[res_idx] = delay;

    res_idx++;
    if (res_idx >= UCP_STAT_RES_LEN) {
        res_idx = 0;
    }
    sock->stat_res.usb_vdly_res_idx = res_idx;
}


static inline void ucp_handle_rc_delay_message(uint8_t *msg)
{
    uint32_t *last_ts, cur_ts;
    int32_t delay;
    int32_t res_idx;
    int32_t *res;
    ucp_sock_t *sock = ucp_get_sock();

    last_ts = (uint32_t *)(msg + 6);
    cur_ts = os_get_us_ts();
    delay = utils_u32_diff(cur_ts, *last_ts);

    res_idx = sock->stat_res.rdly_res_idx;
    res = sock->stat_res.rcdelay_res;

    if (0 == res[sock->stat_res.rdly_min_idx]) {
        sock->stat_res.rdly_min_idx = 0;
    } else if (delay < res[sock->stat_res.rdly_min_idx]) {
        sock->stat_res.rdly_min_idx = res_idx;
    }

    if (delay > res[sock->stat_res.rdly_max_idx]) {
        sock->stat_res.rdly_max_idx = res_idx;
    }

    sock->stat_res.rdly_sum -= res[res_idx];
    sock->stat_res.rdly_sum += delay;
    sock->stat_res.rdly_avg = sock->stat_res.rdly_sum/UCP_STAT_RES_LEN;

    res[res_idx] = delay;

    res_idx++;
    if (res_idx >= UCP_STAT_RES_LEN) {
        res_idx = 0;
    }
    sock->stat_res.rdly_res_idx = res_idx;

    uint32_t *vdly = (uint32_t *)(msg + 10);
    sock->stat_res.vdly_avg_to_rc = *vdly;
}

static inline void ucp_handle_rc_message(uint32_t ts)
{
#define RC_DELAY_SAMPLE_CNT 25 //每多少个摇杆包采样一次时延
    static int cnt = 0;
    cnt++;
    if (cnt % RC_DELAY_SAMPLE_CNT == 0) {
        uint8_t rc_delay_message[UCP_INTERNAL_MSG_TOTALLEN_RC_DELAY];

        //每隔几个命令后，反馈一个RCDL消息 用于测时延
        rc_delay_message[0] = 'U';
        rc_delay_message[1] = 'C';
        rc_delay_message[2] = 'P';
        rc_delay_message[3] = ':';
        rc_delay_message[4] = UCP_INTERNAL_MSG_BODYLEN_RC_DELAY; //1B type + 4B rc delay + 4B video delay
        rc_delay_message[5] = UCP_INTERNAL_MSG_TYPE_RC_DELAY;

        uint32_t *sendding_timestamps = (uint32_t *)(rc_delay_message + 6);
        *sendding_timestamps = ts;

        //将视频时延也发到RC端，用于app debug显示
        ucp_sock_t *sock = ucp_get_sock();
        uint32_t *video_delay = (uint32_t *)(rc_delay_message + 10);
        *video_delay = sock->stat_res.vdly_avg;

        ucp_send_msg(rc_delay_message, sizeof(rc_delay_message), 0);
    }
}

static inline void ucp_handle_video_message(ucp_pkt_info_t* pkt_info)
{
#define VID_DELAY_SAMPLE_CNT 60 //每多少个视频包采样一次时延
    static uint8_t video_delay_message[UCP_INTERNAL_MSG_TOTALLEN_VID_DELAY] =
        {'U', 'C', 'P', ':', UCP_INTERNAL_MSG_BODYLEN_VID_DELAY, UCP_INTERNAL_MSG_TYPE_VID_DELAY, 0, 0, 0, 0}; //5-len
    static int cnt = 0;
    uint32_t *sendding_timestamps;
    pkt_info->sampled = 0;

    if (pkt_info->moredata == 0) {
        cnt++;
        if (cnt % VID_DELAY_SAMPLE_CNT == 0) {
            sendding_timestamps = (uint32_t *)(video_delay_message + 6);
            *sendding_timestamps = pkt_info->timestamp;
            ucp_send_msg(video_delay_message, sizeof(video_delay_message), 0);
            pkt_info->sampled = 1;
        }
    }
}

/*
    @buffer: 外部传输接收包存放的buffer
    @len: buffer length
    @pkt_info: 对外暴露的，接收包所带有的UCP元信息
*/
static int32_t ucp_recv(uint8_t *buffer, uint32_t len, ucp_session_type_t *session_type, ucp_pkt_info_t *pkt_info)
{
    ucp_sock_t *sock = ucp_get_sock();
    ucp_seg_t *seg;
    int32_t ret = 0;

    if (len < 1500) {
        return UCP_ERR_BUF_IS_SMALL;
    }

RECV:
    ret = 0;
    /* 从接收队列取数据 */
    os_mutex_lock(&sock->rcvq_mutex);
    if (!list_empty(&sock->rcv_queue)) {
        seg = list_first_entry(&sock->rcv_queue, ucp_seg_t, head);
        ret = seg->ucp_header.len - UCP_PKT_HDR_LEN;

        _list_del(&seg->head);
        sock->nrcv_que--;
    }
    os_mutex_unlock(&sock->rcvq_mutex);

    if (ret > 0) {
        os_memcpy(buffer, seg->ucp_payload, ret);
        *session_type = seg->ucp_header.session_type;
        pkt_info->moredata = seg->ucp_header.moredata;
        pkt_info->sampled = 0;
        pkt_info->is_hb = seg->ucp_header.hb_flag;
        pkt_info->timestamp = seg->ucp_header.timestamp;

        ucp_delete_segment(seg);
        return ret;
    }

    ret = os_sem_wait(&sock->rcvq_sem);
    if (0 != ret) {
        os_log("ucp_recv: os_sem_wait error:%s\n", strerror(errno));
        return -1;
    }

    goto RECV;

    return 0;
}

static void ucp_parse_una(ucp_sock_t *sock, uint16_t una)
{
    ucp_seg_t *seg, *seg_next;
    LIST_HEAD(seg_to_del);

    os_mutex_lock(&sock->sndq_mutex);
    list_for_each_entry_safe(seg, seg_next, &sock->snd_buf, head) {
        if (utils_u16_diff(seg->ucp_header.sn, una) < 0) {
            list_move_tail(&seg->head, &seg_to_del);
            sock->nsnd_buf--;
        } else {
            break;
        }
    }
    os_mutex_unlock(&sock->sndq_mutex);

    list_for_each_entry_safe(seg, seg_next, &seg_to_del, head) {
        ucp_delete_segment(seg);
    }
}

static void ucp_parse_bak(ucp_sock_t *sock, ucp_pkt_hdr_t *pkt_hdr)
{
    uint8_t *bak_buf;
    uint32_t bitmap, bitmap2;
    uint16_t sn, una;
    LIST_HEAD(seg_to_del);
    ucp_seg_t *seg, *seg_next;
    uint16_t index = 1;
    int has_fastack = 0;

    bak_buf = (uint8_t *)pkt_hdr + pkt_hdr->len - UCP_PKT_HB_BAK_LEN;

    if (bak_buf[0] != UCP_PKT_MAGIC) {
        os_log("ucp_parse_bak: ERROR, pkt with no bak magic number\n");
        return;
    }

    una = *((uint16_t *)(bak_buf + 1));
    bitmap = *((uint32_t *)(bak_buf + 3));
    bitmap2 = *((uint32_t *)(bak_buf + 7));

#ifdef UCP_VERBOSE_LOG
    os_log("RECV BAK: una:%d, bitmap:%x %x\n", una, (unsigned int)bitmap, (unsigned int)bitmap2);
#endif

    if (0 == bitmap && 0 == bitmap2) {
        return;
    }

    os_mutex_lock(&sock->sndq_mutex);
    if (list_empty(&sock->snd_buf)) {
        os_mutex_unlock(&sock->sndq_mutex);
        goto OUT;
    }

    seg_next = list_first_entry(&sock->snd_buf, ucp_seg_t, head);
HANDLE_BITMAP:
    while (bitmap) {
        seg = seg_next;
        if (bitmap & 0x1) {
            sn = una + index;
            list_for_each_entry_safe_from(seg, seg_next, &sock->snd_buf, head) {
                if (seg->ucp_header.sn == sn) {
                    list_move_tail(&seg->head, &seg_to_del);
                    sock->nsnd_buf--;
                    break;
                } else if (utils_u16_diff(seg->ucp_header.sn, sn) > 0) {
                    seg_next = seg;
                    break;
                } else {
                    seg->fastack = 1;
                    has_fastack = 1;
                }
            }
        }
        bitmap >>= 1;
        index++;
    }
    if (bitmap2) {
        bitmap = bitmap2;
        index = 33;
        bitmap2 = 0;
        goto HANDLE_BITMAP;
    }
    os_mutex_unlock(&sock->sndq_mutex);

    if (has_fastack) {
        os_sem_post(&sock->sndq_sem);
    }

    list_for_each_entry_safe(seg, seg_next, &seg_to_del, head) {
        ucp_delete_segment(seg);
    }

OUT:
    return;
}

static int32_t ucp_parse_hb(ucp_sock_t *sock, ucp_pkt_hdr_t *pkt_hdr)
{
    ucp_seg_t *seg_new;

#ifdef UCP_VERBOSE_LOG_MORE
    os_log("ucp_input: sn:hb, len:%d\n", pkt_hdr->len);
#endif

    sock->last_hb_ts = os_get_us_ts();

    if (pkt_hdr->len < UCP_PKT_HDR_LEN + UCP_PKT_HB_BAK_LEN + 1) {
        os_log("ucp_parse_hb: FATAL ERROR, hb pkt len is too small:%d\n", pkt_hdr->len);
        return UCP_ERR_INVAL;
    }

    //处理block ack
    ucp_parse_bak(sock, pkt_hdr);

    pkt_hdr->len -= UCP_PKT_HB_BAK_LEN;
    seg_new = ucp_new_segment(pkt_hdr->len - UCP_PKT_HDR_LEN);
    if (NULL == seg_new) {
        os_log("ERROR, ucp_parse_hb: new seg fail, no memory.\n");
        return UCP_ERR_NOMEM;
    }
    os_memcpy(&seg_new->ucp_header, (uint8_t *)pkt_hdr, pkt_hdr->len);

    //心跳包无需重排序，直接进接收队列
    os_mutex_lock(&sock->rcvq_mutex);
    list_add_tail(&seg_new->head, &sock->rcv_queue);
    sock->nrcv_que++;
    os_mutex_unlock(&sock->rcvq_mutex);

    UTILS_INC(sock->curr_stat.rcv_hb_cnt);
    UTILS_INC(sock->curr_stat.rcv_all_cnt);


    UTILS_ADD(sock->curr_stat.rcv_all_bytes, (pkt_hdr->len + UCP_PKT_HB_BAK_LEN));
    if (!sock->rcv_one_hb) {
        sock->rcv_one_hb = 1;
        ucp_notify_sndbuf_num(0, sock->snd_wnd, 1);
    }

    os_sem_post(&sock->rcvq_sem);
    return 0;
}

static int32_t ucp_parse_noconfirm(ucp_sock_t *sock, ucp_pkt_hdr_t *pkt_hdr)
{
    ucp_seg_t *seg_new;

    sock->last_hb_ts = os_get_us_ts();

    seg_new = ucp_new_segment(pkt_hdr->len - UCP_PKT_HDR_LEN);
    if (NULL == seg_new) {
        os_log("ERROR, ucp_parse_noconfirm: new seg fail, no memory.\n");
        return UCP_ERR_NOMEM;
    }
    os_memcpy(&seg_new->ucp_header, (uint8_t *)pkt_hdr, pkt_hdr->len);

    //无需确认包直接进接收队列
    os_mutex_lock(&sock->rcvq_mutex);
    list_add_tail(&seg_new->head, &sock->rcv_queue);
    sock->nrcv_que++;
    os_mutex_unlock(&sock->rcvq_mutex);
    UTILS_INC(sock->curr_stat.rcv_all_cnt);
    UTILS_ADD(sock->curr_stat.rcv_all_bytes, pkt_hdr->len);

    if (UCP_SESSION_VIDEO == pkt_hdr->session_type) {
        UTILS_INC(sock->curr_stat.rcv_video_cnt);
        UTILS_ADD(sock->curr_stat.rcv_loss_cnt, utils_u16_diff(pkt_hdr->sn, sock->video_rcv_nxt));
        sock->video_rcv_nxt = pkt_hdr->sn + 1;
#ifdef UCP_VERBOSE_LOG_MORE
        os_log("video no confirm, sn:%d, moredata：%d, payload len:%d\n",
                        pkt_hdr->sn, pkt_hdr->moredata, pkt_hdr->len - UCP_PKT_HDR_LEN);
#endif
    }

    os_sem_post(&sock->rcvq_sem);
    return 0;
}

static int32_t ucp_parse_data(ucp_sock_t *sock, ucp_pkt_hdr_t *pkt_hdr)
{
    ucp_seg_t *seg, *seg_new;
    int32_t repeat = 0;

    //如果是已确认过的包，直接丢弃
    if (utils_u16_diff(pkt_hdr->sn, sock->rcv_nxt) < 0) {
        UTILS_INC(sock->stat_res.rcv_dup_cnt);
        os_log("ucp_parse_data: dup: sn:%d, len:%d, rcv_next:%d\n", pkt_hdr->sn, pkt_hdr->len, sock->rcv_nxt);
        return 0;
    }

#ifdef UCP_VERBOSE_LOG_MORE
    os_log("ucp_input: sn:%d, len:%d, moredata:%d\n", pkt_hdr->sn, pkt_hdr->len, pkt_hdr->moredata);
#endif

    seg_new = ucp_new_segment(pkt_hdr->len - UCP_PKT_HDR_LEN);
    if (NULL == seg_new) {
        os_log("ERROR, ucp_parse_data: new seg fail, no memory.\n");
        return UCP_ERR_NOMEM;
    }
    os_memcpy(&seg_new->ucp_header, (uint8_t *)pkt_hdr, pkt_hdr->len);

    //按sn递增排序插入到接收缓存队列
    list_for_each_entry(seg, &sock->rcv_buf, head) {
        if (pkt_hdr->sn == seg->ucp_header.sn) {
            repeat = 1;
            break;
        }
        if (utils_u16_diff(pkt_hdr->sn, seg->ucp_header.sn) < 0) {
            break;
        }
    }
    if (0 == repeat) {
        _list_add(&seg_new->head, seg->head.prev, &seg->head);
        sock->nrcv_buf++;
        UTILS_INC(sock->curr_stat.rcv_all_cnt);
        UTILS_ADD(sock->curr_stat.rcv_all_bytes, pkt_hdr->len);
    } else {
        UTILS_INC(sock->stat_res.rcv_dup_cnt);
        os_log("ucp_parse_data: dup: sn:%d, len:%d\n", pkt_hdr->sn, pkt_hdr->len);
        ucp_delete_segment(seg_new);
    }

    return 0;
}

static uint32_t ucp_get_wait_rcv(ucp_sock_t *sock)
{
    return sock->nrcv_que + sock->nrcv_buf;
}


/* 往seg_bak的后5个字节填充bak信息 */
static void ucp_make_bak(ucp_sock_t *sock)
{
    ucp_seg_t *seg;
    int16_t offset;
    uint32_t bitmap = 0;
    uint32_t bitmap2 = 0;

    list_for_each_entry(seg, &sock->rcv_buf, head) {
        offset = utils_u16_diff(seg->ucp_header.sn, sock->rcv_nxt);
        if (offset < 0) {
            offset = -offset;
        }
        if (offset <= 0 || offset > sock->snd_wnd) {
            os_log("ucp_make_bak: FATAL ERROR!, offset too large:%d, sn:%d, len:%d, rcv_next:%d\n",
                    offset, seg->ucp_header.sn, seg->ucp_header.len, sock->rcv_nxt);
            utils_dump_pkt((uint8_t *)(&seg->ucp_header), seg->ucp_header.len);
            break;
        }

        if (offset <= 32) {
            bitmap |= 1 << (offset - 1);
        } else if (offset <= 64) {
            bitmap2 |= 1 << (offset - 33);
        } else {
            os_log("ucp_make_bak: bitmap overflow, una:%u, sn:%u\n", sock->rcv_nxt, seg->ucp_header.sn);
            break;
        }
    }

    if (sock->bak_una == sock->rcv_nxt && sock->bak_bitmap == bitmap && sock->bak_bitmap2 == bitmap2) {
        //no bak changes since last, just return;
        return;
    }

    os_mutex_lock(&sock->bak_mutex);
    sock->bak_una = sock->rcv_nxt;
    sock->bak_bitmap = bitmap;
    sock->bak_bitmap2 = bitmap2;
    os_mutex_unlock(&sock->bak_mutex);
}

/* 输入的包可以是单个UCP包，也可以是多个UCP包合在一起 */
static void ucp_input(uint8_t *data, uint32_t len)
{
    ucp_sock_t *sock = ucp_get_sock();
    ucp_pkt_hdr_t *pkt;
    ucp_seg_t *seg, *seg_next;
    int32_t ret = 0, has_newdata = 0;
    int data_len = len, pkt_len;
    int32_t span;

    if (data_len <= UCP_PKT_HDR_LEN) {
        return;
    }

    while (data_len > 0) {
        pkt = (ucp_pkt_hdr_t *)data;
        pkt_len = pkt->len;

        if (pkt_len < UCP_PKT_HDR_LEN || pkt_len > data_len) {
            os_log("ucp_input: FATAL ERROR, wrong pkt len, pkt_len(%d) > data_len(%d)\n", pkt_len, data_len);
            utils_dump_pkt(data, data_len);
            ret = UCP_ERR_INVAL;
            break;
        }

        data += pkt_len;
        data_len -= pkt_len;

        //校验帧头
        if (pkt->version != UCP_PKT_VERSION || pkt->magic != UCP_PKT_MAGIC) {
            os_log("ucp_input: FATAL ERROR, pkt->version:%d, pkt->magic:%d\n",
                            pkt->version, pkt->magic);
            ret = UCP_ERR_INVAL;
            break;
        }

        //处理对端发来的UNA
        ucp_parse_una(sock, pkt->una);

        //接收队列已满
        if (ucp_get_wait_rcv(sock) > sock->rcv_wnd) {
            os_log("ucp_input: ERROR, rcvq is full. n_rque:%u, n_rbuf:%u\n", (unsigned int)sock->nrcv_que, (unsigned int)sock->nrcv_buf);
            ret = UCP_ERR_RCVQ_FULL;
            break;
        }

        //parse hb, hb contains bak
        if (pkt->hb_flag) {
            ucp_parse_hb(sock, pkt);
            continue;
        }

        //parse noconfirm pkt
        if (!pkt->need_confirm) {
            ucp_parse_noconfirm(sock, pkt);
            continue;
        }

        span = utils_u16_diff(pkt->sn, sock->rcv_nxt);
        if (span > sock->rcv_wnd) {
            os_log("ucp_input: ERROR, pkt sn:%u is out of window. rcv_nxt:%u\n", pkt->sn, sock->rcv_nxt);
            ret = UCP_ERR_RCVQ_FULL;
            break;
        }

        ret = ucp_parse_data(sock, pkt);
        if (0 != ret) {
            break;
        }
    }

    //将收到的连续的包，转移到接收队列
    os_mutex_lock(&sock->rcvq_mutex);
    list_for_each_entry_safe(seg, seg_next, &sock->rcv_buf, head) {
        if (seg->ucp_header.sn == sock->rcv_nxt) {
            sock->nrcv_buf--;
            list_move_tail(&seg->head, &sock->rcv_queue);
            sock->rcv_nxt++;
            sock->nrcv_que++;
            has_newdata = 1;
        } else {
            break;
        }
    }
    os_mutex_unlock(&sock->rcvq_mutex);
    ucp_make_bak(sock);

    if (has_newdata) {
        os_sem_post(&sock->rcvq_sem);
    }

    //return ret;
}

void udp_connstate_cb(int state)
{
    //if (state) {
        ucp_set_conn_state(state);
    //}
}

int ucp_init_udp_server(ucp_sock_t *sock)
{
    int ret;

    udp_register_rx_cb(ucp_input);
    udp_register_connstate_cb(udp_connstate_cb);

    ret = os_create_thread(udpserver_accept_and_recv_thread, NULL);
    if (0 != ret) {
        os_log("os_create_thread fail. ucp exit\n");
        return -1;
    }

    return 0;
}

int ucp_init_udp_client(ucp_sock_t *sock, const char *server_ip)
{
    int ret;
    udp_client_conn_thread_info_t *info = &(sock->udp_client_conn_thread);

    udp_register_rx_cb(ucp_input);
    udp_register_connstate_cb(udp_connstate_cb);

    snprintf(info->server_ip, sizeof(info->server_ip), "%s", server_ip);
    info->conn_thread_start = 1;
    ret = os_create_thread(udpclient_connect_and_recv_thread, (void *)info);
    if (0 != ret) {
        os_log("os_create_thread fail.\n");
        return -1;
    }

    return 0;
}

int ucp_start_udpclient_connect()
{
    ucp_sock_t *sock = ucp_get_sock();
    sock->udp_client_conn_thread.conn_thread_start = 1;
    os_log("start udpclient connect\n");
    return 0;
}
int ucp_stop_udpclient_connect()
{
    ucp_sock_t *sock = ucp_get_sock();
    sock->udp_client_conn_thread.conn_thread_start = 0;
    os_log("stop udpclient connect\n");

    ucp_set_conn_state(0);
    udp_set_connstate(0);
    return 0;
}

void* ucp_rx_thread(void *arg)
{
    int ret;
    ucp_pkt_info_t pkt_info;
    ucp_session_type_t session_type;
    static uint8_t buffer[1500];
    ucp_sock_t *sock = ucp_get_sock();

    while (1) {
        ret = ucp_recv(buffer, sizeof(buffer), &session_type, &pkt_info);
        if (ret > 0) {
            if (UCP_SESSION_MSG == session_type) {
                if (buffer[0] == 'U' && buffer[1] == 'C' && buffer[2] == 'P' && buffer[3] == ':') {
                    //UCP协议内部消息
                    char len = buffer[4];
                    char cmd = buffer[5];

                    if (UCP_INTERNAL_MSG_TYPE_VID_DELAY == cmd && UCP_INTERNAL_MSG_BODYLEN_VID_DELAY == len) {
                        //video delay
                        ucp_handle_video_delay_message(buffer);
                        continue;
                    } else if (UCP_INTERNAL_MSG_TYPE_RC_DELAY == cmd && UCP_INTERNAL_MSG_BODYLEN_RC_DELAY == len) {
                        //RC delay
                        ucp_handle_rc_delay_message(buffer);
                        continue;
                    } else if (UCP_INTERNAL_MSG_TYPE_VDLY_USB == cmd && UCP_INTERNAL_MSG_BODYLEN_VID_DELAY == len) {
                        //usb video delay
                        ucp_handle_usb_vdly_message(buffer);
                        continue;
                    }
                } else if (pkt_info.is_hb && 0 == sock->role) {
                    //server端收到的心跳包就是摇杆包
                    ucp_handle_rc_message(pkt_info.timestamp);
                }

                g_rx_msg_cb(buffer, ret, &pkt_info);
            } else if (UCP_SESSION_VIDEO == session_type) {
                ucp_handle_video_message(&pkt_info);

                g_rx_video_cb(buffer, ret, &pkt_info);
                if (0 == pkt_info.moredata) {
                    UTILS_INC(sock->curr_stat.rcv_frame_cnt);
                }
            } else if (UCP_SESSION_AUDIO == session_type) {
                g_rx_audio_cb(buffer, ret, &pkt_info);
                UTILS_INC(sock->curr_stat.rcv_audio_cnt);
            } else {
                os_log("ucp_rx_thread: unkown session type: %d\n", session_type);
            }
        } else {
            os_log("ucp_recv return error:%d\n", ret);
        }
    }

    return NULL;
}

int ucp_init_rx()
{
    int ret;

    if (NULL == g_rx_msg_cb || NULL == g_rx_video_cb || NULL == g_rx_audio_cb) {
        os_log("error, should register rx cb before\n");
        return -1;
    }

    ret = os_create_thread(ucp_rx_thread, NULL);
    if (0 != ret) {
        os_log("os_create_thread fail. ucp_rx_thread\n");
        return -1;
    }

    return 0;
}

void ucp_register_rx_cb(RX_CB_FUN rx_msg_cb, RX_CB_FUN rx_video_cb, RX_CB_FUN rx_audio_cb)
{
    g_rx_msg_cb = rx_msg_cb;
    g_rx_video_cb = rx_video_cb;
    g_rx_audio_cb = rx_audio_cb;
}



