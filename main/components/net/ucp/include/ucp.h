#ifndef _UCP_H_
#define _UCP_H_
#include <stdint.h>
#include <stdbool.h>
#include "os_udp.h"

typedef enum {
    UCP_SESSION_MSG = 0,
    UCP_SESSION_VIDEO = 1,
    UCP_SESSION_AUDIO = 2,

    UCP_SESSION_MAX,
} _ucp_session_type_t;
typedef uint8_t ucp_session_type_t;

/* 接收包所带有的UCP元信息 */
typedef struct {
    uint8_t moredata;    //1表示 此包由MTU分成小包
    uint8_t sampled;     //标记此包是否测延时
    uint8_t is_hb;
    uint32_t timestamp;
} ucp_pkt_info_t;


typedef void (*RX_CB_FUN)(uint8_t *buffer, int32_t len, ucp_pkt_info_t *pkt_info);
typedef void (*SNDBUF_NUM_CB_FUN)(int32_t num, int32_t wnd, int32_t conn_state);

/* error code 定义 */
enum {
    UCP_OK = 0,
    UCP_ERR_INVAL = -1,     //参数非法
    UCP_ERR_NOMEM = -2,
    UCP_ERR_SNDQ_FULL = -3,
    UCP_ERR_RCVQ_FULL = -4,
    UCP_ERR_BUF_IS_SMALL = -5,
    UCP_ERR_NOT_CONN = -6,
    UCP_ERR_SNDQ_NOT_ENOUGH = -7,
};

typedef enum {
    UCP_ROLE_SERVER = 0,
    UCP_ROLE_CLIENT = 1,

    UCP_ROLE_MAX,
} ucp_role_t;

#define UCP_STAT_RES_LEN    10

/* 统计结果数据 */
typedef struct {
    uint32_t    rcv_dup_cnt;
    uint32_t    retry_cnt;      //第一次发送的包不算重传包

    uint32_t    snd_kbps;
    uint32_t    rcv_kbps;

    uint8_t     loss_rate;      //0~100 %
    uint8_t     max_retry_times;  //一个统计周期内发生的最大重传次数
    uint32_t    video_fps;
    uint32_t    rcv_hb_ps;  //每秒钟收到的心跳包个数
    uint32_t    audio_hz;   //每秒钟音频数据包个数

    int32_t     videodelay_res[UCP_STAT_RES_LEN];  //循环记录最近10次统计的时延
    int32_t     vdly_res_idx;
    int32_t     vdly_min_idx;
    int32_t     vdly_max_idx;
    int32_t     vdly_avg;  //us，机器人侧统计的图传时延
    int32_t     vdly_sum;
    int32_t     vdly_avg_to_rc;  //遥控器侧收到的图传时延

    int32_t     usb_vdly_res[UCP_STAT_RES_LEN];  //循环记录最近10次统计的时延
    int32_t     usb_vdly_res_idx;
    int32_t     usb_vdly_min_idx;
    int32_t     usb_vdly_max_idx;
    int32_t     usb_vdly_avg;  //us
    int32_t     usb_vdly_sum;

    int32_t     rcdelay_res[UCP_STAT_RES_LEN];  //循环记录最近10次统计的时延
    int32_t     rdly_res_idx;
    int32_t     rdly_min_idx;
    int32_t     rdly_max_idx;
    int32_t     rdly_avg;  //us，遥控器侧统计的摇杆时延
    int32_t     rdly_sum;
} ucp_stat_t;

/* 统计过程数据 */
typedef struct
{
    uint32_t    rcv_hb_cnt;
    uint32_t    rcv_all_cnt;
    uint32_t    rcv_all_bytes;  //不计入dup包
    uint32_t    snd_all_cnt_out;//通过UDP发出去的包数，不计retry包
    uint32_t    snd_all_bytes;  //入队的包字节数，不计入retry包

    uint32_t    rcv_loss_cnt;   //接收视频丢包个数, 视频包无retry时使用
    uint32_t    rcv_video_cnt;  //接收视频包总个数，视频包无retry时使用

    uint8_t     max_retry_times_tmp;  //一个统计周期内发生的最大重传次数
    uint32_t    rcv_frame_cnt;  //发送或接收的帧数
    uint32_t    rcv_audio_cnt;  //发送或接收的音频包数
} ucp_stat_process_t;

#define UCP_SENDFLAG_NEEDCONFIRM    0x1
#define UCP_SENDFLAG_HB             0x8
#define UCP_SENDFLAG_HAVE_OSD       0x2


typedef struct {
    const uint8_t *buffer;
    uint32_t len;
    const uint8_t *osd_buffer;  //机器人发送视频时，可以携带机器人状态信息同时下发，此buffer不需要接收确认
    uint32_t osd_len;
    ucp_session_type_t type;
    uint8_t flag;
} ucp_send_params_t;


//发送视频前调用此接口，检查能否发包
void ucp_calc_stats();
ucp_stat_t* ucp_get_stats();
void ucp_print_stats();
void ucp_calc_and_print_stats();

int ucp_get_conn_state();
int32_t ucp_client_init(const char *server_ip);
int32_t ucp_server_init();
int32_t ucp_send(ucp_send_params_t *params);
void ucp_register_callback(
                RX_CB_FUN rx_msg_cb,
                RX_CB_FUN rx_video_cb,
                RX_CB_FUN rx_audio_cb,
                SNDBUF_NUM_CB_FUN sndbuf_num_cb);

static inline int32_t ucp_send_msg(const uint8_t *buffer, uint32_t len, uint8_t need_confirm)
{
    ucp_send_params_t params;
    params.buffer = buffer;
    params.len = len;
    params.flag = need_confirm & UCP_SENDFLAG_NEEDCONFIRM;
    params.type = UCP_SESSION_MSG;

    return ucp_send(&params);
}

//一次一帧，不能一帧多次
static inline int32_t ucp_send_video(const uint8_t *buffer, uint32_t len, uint8_t need_confirm)
{
    ucp_send_params_t params;
    params.buffer = buffer;
    params.len = len;
    params.flag = need_confirm & UCP_SENDFLAG_NEEDCONFIRM;
    params.type = UCP_SESSION_VIDEO;

    return ucp_send(&params);
}

static inline int32_t ucp_send_video_withosd(const uint8_t *buffer, uint32_t len, const uint8_t *osd_buffer, uint32_t osd_len)
{
    ucp_send_params_t params;
    params.buffer = buffer;
    params.len = len;
    params.flag = UCP_SENDFLAG_NEEDCONFIRM | UCP_SENDFLAG_HAVE_OSD;
    params.type = UCP_SESSION_VIDEO;
    params.osd_buffer = osd_buffer;
    params.osd_len = osd_len;

    return ucp_send(&params);
}

static inline int32_t ucp_send_heart_beat(const uint8_t *buffer, uint32_t len)
{
    ucp_send_params_t params;
    params.buffer = buffer;
    params.len = len;
    params.flag = UCP_SENDFLAG_HB;
    params.type = UCP_SESSION_MSG;

    return ucp_send(&params);
}

static inline int32_t ucp_send_audio(const uint8_t *buffer, uint32_t len, uint8_t need_confirm)
{
    ucp_send_params_t params;
    params.buffer = buffer;
    params.len = len;
    params.flag = need_confirm & UCP_SENDFLAG_NEEDCONFIRM;
    params.type = UCP_SESSION_AUDIO;

    return ucp_send(&params);
}


int ucp_start_udpclient_connect();
int ucp_stop_udpclient_connect();



#endif /* _UCP_H_ */
