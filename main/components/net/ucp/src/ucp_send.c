
#include "utils.h"
#include "ucp_private.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
static SNDBUF_NUM_CB_FUN g_sndbuf_num_cb;


ucp_sock_t g_sock_objs;
inline ucp_sock_t* ucp_get_sock()
{
    return &(g_sock_objs);
}

void ucp_notify_sndbuf_num(int num, int wnd, int conn_state)
{
    if (g_sndbuf_num_cb) {
        g_sndbuf_num_cb(num, wnd, conn_state);
    }
}

/* @state: 0-断开, 1-连接 */
void ucp_set_conn_state(int state)
{
    ucp_sock_t *sock = ucp_get_sock();
    ucp_seg_t *seg, *seg_next;

    if (sock->conn_status == state) {
        return;
    }

    sock->conn_status = state;
    os_log("#### UCP CONN STATE: %d\n", state);
    if (0 == state) {
        //reset send q and send buf
        os_mutex_lock(&sock->sndq_mutex);
        list_for_each_entry_safe(seg, seg_next, &sock->snd_queue, head) {
            _list_del(&seg->head);
            ucp_delete_segment(seg);
            sock->nsnd_que--;
        }

        list_for_each_entry_safe(seg, seg_next, &sock->snd_buf, head) {
            _list_del(&seg->head);
            ucp_delete_segment(seg);
            sock->nsnd_buf--;
        }

        sock->snd_nxt = 0;
        sock->rcv_nxt = 0;
        sock->video_snd_nxt = 0;
        sock->video_rcv_nxt = 0;
        sock->last_hb_ts = 0;
        sock->rcv_one_hb = 0;
        ucp_notify_sndbuf_num(0, sock->snd_wnd, 0);
        if (sock->nsnd_buf != 0 || sock->nsnd_que != 0) {
            os_log("FATAL, after disconnection, nsnd_buf:%u, nsnd_que:%u\n", (unsigned int)sock->nsnd_buf, (unsigned int)sock->nsnd_que);
        }

        os_memset(&sock->stat_res, 0, sizeof(ucp_stat_t));
        os_memset(&sock->curr_stat, 0, sizeof(ucp_stat_process_t));
        os_memset(&sock->last_stat, 0, sizeof(ucp_stat_process_t));
        sock->last_stat_ms = 0;
        os_mutex_unlock(&sock->sndq_mutex);

        //reset rcv buf and bak info
        os_mutex_lock(&sock->rcvq_mutex);
        list_for_each_entry_safe(seg, seg_next, &sock->rcv_buf, head) {
            _list_del(&seg->head);
            ucp_delete_segment(seg);
            sock->nrcv_buf--;
        }

        if (sock->nrcv_buf != 0) {
            os_log("FATAL, after disconnection, nrcv_buf:%u\n", (unsigned int)sock->nrcv_buf);
        }
        os_mutex_unlock(&sock->rcvq_mutex);

        os_mutex_lock(&sock->bak_mutex);
        sock->bak_una = 0;
        sock->bak_bitmap = 0;
        sock->bak_bitmap2 = 0;
        os_mutex_unlock(&sock->bak_mutex);
    }
}

/* @return: 0-断开, 1-连接 */
inline int ucp_get_conn_state()
{
    return g_sock_objs.conn_status;
}

/* 传入UCP payload长度，即UCP包减去报头的长度 */
ucp_seg_t* ucp_new_segment(uint32_t size)
{
    ucp_seg_t *seg;
    seg = os_malloc(sizeof(ucp_seg_t) + size);

    if (seg) {
        os_memset(seg, 0, sizeof(ucp_seg_t));
        seg->ucp_header.len = size + UCP_PKT_HDR_LEN;  //len在申请的时候直接填
    }

    return seg;
}

void ucp_test_tx_delay(ucp_pkt_hdr_t *hdr, char *str)
{
    uint32_t cur_ts = os_get_us_ts();
    int32_t delta;

    delta = utils_u32_diff(cur_ts, hdr->timestamp);

    os_log("%s delay: %d, len:%d, sn:%d\n", str, delta, hdr->len, hdr->sn);
}

static inline void ucp_init_pkt_hdr(
                ucp_pkt_hdr_t *hdr,
                ucp_session_type_t s_type,
                uint8_t flag,
                uint32_t timestamp)
{
    hdr->version = UCP_PKT_VERSION;
    hdr->magic = UCP_PKT_MAGIC;
    hdr->session_type = s_type;
    hdr->need_confirm = flag & UCP_SENDFLAG_NEEDCONFIRM;
    hdr->moredata = 1;  //init to 1, will change to 0 when necessary
    hdr->timestamp = timestamp;

    if (flag & UCP_SENDFLAG_HB) {
        hdr->hb_flag = 1;
    }
}

static inline int32_t ucp_get_wait_snd(ucp_sock_t *sock)
{
    return (int32_t)(sock->nsnd_que + sock->nsnd_buf);
}

static inline int ucp_get_sndbuf_sn_span(ucp_sock_t *sock)
{
    ucp_seg_t *first_seg;
    int span;

    if (list_empty(&sock->snd_buf)) {
        return 0;
    }

    first_seg = list_first_entry(&sock->snd_buf, ucp_seg_t, head);
    span = utils_u16_diff(sock->snd_nxt, first_seg->ucp_header.sn);
    if (span < 0) {
        os_log("FATAL error, snd_nxt:%u < snd_buf first sn:%u\n", sock->snd_nxt, first_seg->ucp_header.sn);
        span = sock->snd_wnd;
    }

    return span;
}

/* 往seg_bak的后5个字节填充bak信息 */
static inline void ucp_fill_bak(ucp_sock_t *sock, uint8_t *bak_buf)
{
    uint32_t *bitmap, *bitmap2;
    uint16_t *una;

    //0 - magic
    bak_buf[0] = UCP_PKT_MAGIC;
    //1~2 - una
    una = (uint16_t *)(bak_buf + 1);
    //3~6 - bitmap1
    bitmap = (uint32_t *)(bak_buf + 3);
    //7~10 - bitmap2
    bitmap2 = (uint32_t *)(bak_buf + 7);

    os_mutex_lock(&sock->bak_mutex);
    *una = sock->bak_una;
    *bitmap = sock->bak_bitmap;
    *bitmap2 = sock->bak_bitmap2;
    os_mutex_unlock(&sock->bak_mutex);
#ifdef UCP_VERBOSE_LOG
    os_log("SEND BAK: una:%d, bitmap:%x %x\n", *una, (unsigned int)*bitmap, (unsigned int)*bitmap2);
#endif
}

//int32_t ucp_send(ucp_session_type_t s_type, const uint8_t *buffer, uint32_t len, uint8_t need_confirm)
int32_t ucp_send(ucp_send_params_t *params)
{
    int count, mod, i, span;
    uint32_t totallen, pktlen, sent = 0;
    int32_t ret;
    uint32_t timestamps;
    uint32_t pktlen_withosd;
    ucp_seg_t *seg = NULL, **seg_array;
    ucp_sock_t *sock;

    if (params->len == 0) {
        return UCP_ERR_INVAL;
    }

    if (!udp_get_connstate()) {
        return UCP_ERR_NOT_CONN;
    }

    sock = ucp_get_sock();
    timestamps = os_get_us_ts();

    totallen = params->len;

    //心跳包要预留BAK空间
    if ((params->flag & UCP_SENDFLAG_HB) && (totallen > sock->mtu - UCP_PKT_HB_BAK_LEN)) {
        os_log("ucp_send: FATAL, hb packet is too long:%d\n", totallen);
        return UCP_ERR_INVAL;
    }
    if (UCP_SESSION_VIDEO == params->type) {
        UTILS_INC(sock->curr_stat.rcv_frame_cnt);
    } else if (UCP_SESSION_AUDIO == params->type) {
        UTILS_INC(sock->curr_stat.rcv_audio_cnt);
    }

    count = totallen / sock->mtu;
    mod = totallen % sock->mtu;
    if (mod > 0) {
        count++;
    }

    os_mutex_lock(&sock->sndq_mutex);
    span = ucp_get_sndbuf_sn_span(sock);
    if (g_sndbuf_num_cb) {
        g_sndbuf_num_cb(span, sock->snd_wnd, 1);
    }
    if (count > (sock->snd_wnd - span)) {
        ret = UCP_ERR_SNDQ_FULL;
        os_mutex_unlock(&sock->sndq_mutex);
        os_log("ucp_send: error, sndbuf sn span overflow: snd_nxt:%u, span:%d, count:%d\n",
                    sock->snd_nxt, span, count);
        goto OUT;
    }

    if (count > (sock->snd_wnd - ucp_get_wait_snd(sock))) {
        ret = UCP_ERR_SNDQ_FULL;
        os_mutex_unlock(&sock->sndq_mutex);
        os_log("ucp_send: error, senq is full: nsnd_que:%u, nsnd_buf:%u, count:%d\n",
                    sock->nsnd_que, sock->nsnd_buf, count);
        goto OUT;
    }

    //将报文按MTU分隔 加包头 放入发送队列
    seg_array = os_calloc(1, count*sizeof(seg));
    if (NULL == seg_array) {
        ret = UCP_ERR_NOMEM;
        os_mutex_unlock(&sock->sndq_mutex);
        os_log("ucp_send: FATAL, os_calloc fail, alloc size:%lu\n", count*sizeof(seg));
        goto OUT;
    }

    for (i = 0; i < count; i++) {
        pktlen = totallen > sock->mtu ? sock->mtu : totallen;
        if ((i + 1 == count) && (params->flag & UCP_SENDFLAG_HAVE_OSD)) {
            //fix: OSD包隐含着也是心跳包
            pktlen_withosd = pktlen + UCP_PKT_HDR_LEN + params->osd_len + UCP_PKT_HB_BAK_LEN;
            if (pktlen_withosd > sock->mtu) {
                seg = ucp_new_segment(pktlen);
            } else {
                seg = ucp_new_segment(pktlen_withosd);
                if (NULL != seg) {
                    seg->has_osd = 1;
                    seg->osd_ucp_header = (ucp_pkt_hdr_t *)(seg->ucp_payload + pktlen);
                }
            }
        } else if (params->flag & UCP_SENDFLAG_HB) {
            //每个心跳包结尾带bak
            seg = ucp_new_segment(pktlen + UCP_PKT_HB_BAK_LEN);
        } else {
            seg = ucp_new_segment(pktlen);
        }
        if (NULL == seg) {
            ret = UCP_ERR_NOMEM;
            os_mutex_unlock(&sock->sndq_mutex);
            os_log("ucp_send: FATAL, ucp_new_segment return NULL\n");
            goto OUT2;
        }

        ucp_init_pkt_hdr(&seg->ucp_header, params->type, params->flag, timestamps);
        os_memcpy(seg->ucp_payload, params->buffer + sent, pktlen);

        if (seg->has_osd) {
            //osd包和前一个包合用一个ucp_seg_t
            os_memset(seg->osd_ucp_header, 0, UCP_PKT_HDR_LEN);
            ucp_init_pkt_hdr(seg->osd_ucp_header, UCP_SESSION_MSG, UCP_SENDFLAG_HB, timestamps);
            seg->osd_ucp_header->len = UCP_PKT_HDR_LEN + params->osd_len + UCP_PKT_HB_BAK_LEN;
            os_memcpy((uint8_t *)(seg->osd_ucp_header) + UCP_PKT_HDR_LEN, params->osd_buffer, params->osd_len);
        }

        seg_array[i] = seg;
        totallen -= pktlen;
        sent += pktlen;
    }
    seg->ucp_header.moredata = 0;

    for (i = 0; i < count; i++) {
        list_add_tail(&seg_array[i]->head, &sock->snd_queue);
        sock->nsnd_que++;
        UTILS_ADD(sock->curr_stat.snd_all_bytes, seg_array[i]->ucp_header.len);
    }
    os_mutex_unlock(&sock->sndq_mutex);
    os_sem_post(&sock->sndq_sem);

    os_free(seg_array);

    return sent;
OUT:
    os_sem_post(&sock->sndq_sem);
    return ret;

OUT2:
    os_sem_post(&sock->sndq_sem);
    for (i = 0; i < count; i++) {
        if (NULL != seg_array[i]) {
            ucp_delete_segment(seg_array[i]);
        } else {
            break;
        }
    }
    os_free(seg_array);
    return ret;
}

/* 将包发送出去 */
static inline int32_t ucp_out()
{
    ucp_sock_t *sock = ucp_get_sock();
    ucp_seg_t *seg, *seg_next, *first_seg;
    int needsend;
    uint32_t timestamps;
    uint16_t sendlen, sndbuf_start_sn;

    //直接将报文从发送队列移到发送缓存队列
    os_mutex_lock(&sock->sndq_mutex);
    list_for_each_entry_safe(seg, seg_next, &sock->snd_queue, head) {
        sock->nsnd_que--;
        if (seg->ucp_header.need_confirm) {
            //需要确认的包，分配sn，移到发送缓存队列
            seg->ucp_header.sn = sock->snd_nxt++;  //填写sn，una最后发送时填 使una取最新值
            list_move_tail(&seg->head, &sock->snd_buf);
            sock->nsnd_buf++;
        } else {
            if (seg->ucp_header.hb_flag) {
                //心跳包填充BAK信息
                ucp_fill_bak(sock, seg->ucp_payload + seg->ucp_header.len - UCP_PKT_HDR_LEN - UCP_PKT_HB_BAK_LEN);
            } else if (UCP_SESSION_VIDEO == seg->ucp_header.session_type) {
                //用于无确认模式下，视频包丢包率
                seg->ucp_header.sn = sock->video_snd_nxt++;
#ifdef UCP_VERBOSE_LOG_MORE
                os_log("ucp_out: sn:%d, payload len:%d\n", seg->ucp_header.sn, seg->ucp_header.len - UCP_PKT_HDR_LEN);
#endif
            }

            //发送时填充una，不需要确认的包，直接发送，然后从发送队列删除
            seg->ucp_header.una = sock->rcv_nxt;
            os_udp_send((uint8_t *)(&seg->ucp_header), seg->ucp_header.len);

            _list_del(&seg->head);
            ucp_delete_segment(seg);
        }
    }

    timestamps = os_get_ms_ts();

    first_seg = list_first_entry(&sock->snd_buf, ucp_seg_t, head);
    sndbuf_start_sn = first_seg->ucp_header.sn;
    //没发送过的直接发送，发送过的判断是否要重传
    list_for_each_entry_safe(seg, seg_next, &sock->snd_buf, head) {
        needsend = 0;
        //首次传输
        if (seg->xmit_cnt == 0) {
            needsend = 1;
			seg->xmit_cnt++;
            seg->rto = UCP_TIMEOUT_RTO;
            seg->resend_ts = timestamps + seg->rto;
            seg->firstsend_ts = timestamps;
        }
        //RTO重传
        //fastack重传
        else if (seg->fastack) {
            if (seg->fastack_ts == 0 || utils_u32_diff(timestamps, seg->fastack_ts) >= 0) {
                needsend = 1;
                seg->xmit_cnt++;
                seg->fastack_ts = timestamps + UCP_FASTACK_RTO;
                seg->resend_ts = timestamps + seg->rto;
                os_log("ucp_out: fastack retry sn:%d, len:%d, live_time:%dms\n",
                    seg->ucp_header.sn, seg->ucp_header.len, utils_u32_diff(timestamps, seg->firstsend_ts));
            }
        }
        #if 1
        else if (utils_u32_diff(timestamps, seg->resend_ts) >= 0) {
            if (utils_u16_diff(seg->ucp_header.sn, sndbuf_start_sn) >= UCP_BAK_SIZE) {
                printf("dddd, rts send, sn>=ssn+64, sn:%u, ssn:%u\n", seg->ucp_header.sn, sndbuf_start_sn);
                break;
            }
            needsend = 1;
            seg->xmit_cnt++;
            seg->fastack_ts = timestamps + UCP_FASTACK_RTO;
            seg->resend_ts = timestamps + seg->rto;
            os_log("ucp_out: rto retry sn:%d, len:%d, live_time:%dms\n",
                seg->ucp_header.sn, seg->ucp_header.len, utils_u32_diff(timestamps, seg->firstsend_ts));
        }
        #endif

        if (needsend) {
            if (1 == seg->xmit_cnt) {
                UTILS_INC(sock->curr_stat.snd_all_cnt_out);
            } else {
                UTILS_INC(sock->stat_res.retry_cnt);
                if (seg->xmit_cnt - 1 > sock->curr_stat.max_retry_times_tmp) {
                    sock->curr_stat.max_retry_times_tmp = seg->xmit_cnt - 1;
                }
            }
#ifdef UCP_VERBOSE_LOG_MORE
            if (!seg->ucp_header.hb_flag) {
                os_log("ucp_out: sn:%d, len:%d\n", seg->ucp_header.sn, seg->ucp_header.len);
            } else {
                os_log("ucp_out: sn:hb, len:%d\n", seg->ucp_header.len);
            }
#endif
            //发送时填充una
            seg->ucp_header.una = sock->rcv_nxt;
            sendlen = seg->ucp_header.len;
            if (seg->has_osd) {
                ucp_fill_bak(sock, (uint8_t *)(seg->osd_ucp_header) + seg->osd_ucp_header->len - UCP_PKT_HB_BAK_LEN);
                seg->osd_ucp_header->una = sock->rcv_nxt;

                seg->ucp_header.len -= seg->osd_ucp_header->len;
            }

            os_udp_send((uint8_t *)(&seg->ucp_header), sendlen);

            if (seg->has_osd) {
                seg->has_osd = 0;
            }
        }
    }
    os_mutex_unlock(&sock->sndq_mutex);

    return 0;
}


/** ucp发送线程 **/
static void* ucp_tx_thread(void *arg)
{
    int ret;
    ucp_sock_t *sock = ucp_get_sock();

    while (1) {
        ret = os_sem_wait(&sock->sndq_sem);
        if (0 != ret) {
            os_log("ucp_tx_thread: os_sem_wait error:%s\n", strerror(errno));
            break;
        }

        ucp_out();
    }

    return NULL;
}

/* 心跳检测线程 */
static void* ucp_check_hb_thread(void *arg)
{
    ucp_sock_t *sock = ucp_get_sock();
    uint32_t timestamps;
    uint32_t cnt = 0;

    while (1) {
        os_usleep(200*1000);
        if (!ucp_get_conn_state()) {
            continue;
        }

        timestamps = os_get_us_ts();
        if (0 == sock->last_hb_ts) {
            sock->last_hb_ts = timestamps;
        }
        else if (utils_u32_diff(timestamps, sock->last_hb_ts) > 2100000) {
            os_log("ucp connection timeout, disconnect!\n");
            ucp_set_conn_state(0);

            udp_set_connstate(0);  //重置底层udp连接

            cnt = 0;
            continue;
        }

        cnt++;
        if (cnt % 5 == 0) {  //每1秒打印一次统计信息
            ucp_calc_stats();
            ucp_print_stats();
        }
    }

    return NULL;
}

int ucp_init_tx()
{
    int ret;

    if (NULL == g_sndbuf_num_cb) {
        os_log("error, should register g_sndbuf_num_cb before\n");
        return -1;
    }

    ret = os_create_thread(ucp_tx_thread, NULL);
    if (0 != ret) {
        os_log("os_create_thread fail. ucp_tx_thread\n");
        return -1;
    }

    ret = os_create_thread(ucp_check_hb_thread, NULL);
    if (0 != ret) {
        os_log("os_create_thread fail. ucp_tx_thread\n");
        return -1;
    }

    return 0;
}

void ucp_register_sndbuf_num_cb(SNDBUF_NUM_CB_FUN cb)
{
    g_sndbuf_num_cb = cb;
}

#pragma GCC diagnostic pop
