
#ifndef _UCP_PRIVATE_H_
#define _UCP_PRIVATE_H_

#include <stdint.h>
#include "list.h"
#include "os_interfaces.h"
#include "ucp.h"

//#define UCP_VERBOSE_LOG_MORE  //更多调试打印信息
//#define UCP_VERBOSE_LOG  //更多调试打印信息
//#define UCP_LOSS_PKT_TEST

//关键参数
//上行心跳频率32HZ(31.25ms)，对应摇杆的灵敏度
//下行心跳频率20~30HZ以内
#define UCP_TIMEOUT_RTO             200 //单位ms
#define UCP_FASTACK_RTO             90  //心跳时间的3~4倍
#define UCP_SND_WINDOW_SIZE         400
#define UCP_RCV_WINDOW_SIZE         500 //只是用来防止接收队列没有处理，比SND_WINDOW大一些


//#define UCP_DEFAULT_MTU             1456
#define UCP_DEFAULT_MTU             1460    //以太网payload是1500B，20B IP hdr, 8B UDP hdr, 12B UCP hdr


#define UCP_PKT_VERSION     1
#define UCP_PKT_MAGIC       5
#define UCP_BAK_SIZE        64 //8字节，64bit

typedef struct {
    uint8_t version: 4;     //版本号，当前总是1
    uint8_t magic: 4;       //当前总是5，用于包头校验

    uint8_t need_confirm: 1;    //1-need confirm 0-no need confirm
    uint8_t moredata: 1;        //1表示 此包由MTU分成小包
    uint8_t session_type: 2;    //0-普通消息，1-视频, 2-音频
    uint8_t hb_flag: 1;         //心跳标识，即包含BAK信息
    uint8_t resv: 3;            //未使用的位

    uint16_t una;           //未确认的sn，也就是在此之前的sn都是确认的

    uint16_t sn;
    uint16_t len;           //UCP报文总长度，包括包头

    uint32_t timestamp;     //us时间戳，保留低32位，1.2小时翻转                 //ms时间戳，保留低32位，49.7天翻转
} ucp_pkt_hdr_t;
#define UCP_PKT_HDR_LEN     (sizeof(ucp_pkt_hdr_t))  //12B

//每个心跳包会额外携带固定字节的block ack
#define UCP_PKT_HB_BAK_LEN  11  //1B magic num + 2B una + 4B bitmap + 4B bitmap

typedef struct {
    struct list_head head;
    uint32_t        firstsend_ts;//第一次发送时的时间戳，业务没有使用，debug用
    uint32_t        resend_ts;  //重传时间戳，毫秒
    uint32_t        fastack_ts; //两次fastack重传要间隔一定时间
    uint16_t        rto;        //毫秒，用于可变rto设计
    uint8_t         xmit_cnt;
    uint8_t         fastack: 1;
    uint8_t         has_osd: 1;
    uint8_t         resv: 6;  //未使用的位
    ucp_pkt_hdr_t   *osd_ucp_header;

    /* 以下为UCP包 */
    ucp_pkt_hdr_t   ucp_header;
    uint8_t         ucp_payload[0];
} ucp_seg_t;


typedef struct {
    uint8_t role;           //0-server 1-client
    uint8_t inited;         //0-not inited 1-has inited
    uint16_t mtu;           //去掉包头，payload部分最大长度
    int16_t snd_wnd;       //发送窗口大小，单位是包个数, TBD 要根据码率动态调整
    int16_t rcv_wnd;       //接收窗口大小，单位是包个数，用于防止不处理接收包

    uint16_t snd_nxt;
    uint16_t rcv_nxt;
    uint16_t video_snd_nxt;  //无重传模式下视频的sn
    uint16_t video_rcv_nxt;

    struct list_head snd_queue;  //发送队列，数据包先拷贝到发送队列
    struct list_head rcv_queue;  //接收队列，接收到的完整顺序的包进到接收队列
    struct list_head snd_buf;  //发送缓存队列，用来等待ACK和重传
    struct list_head rcv_buf;  //接收缓存队列，用来接收排序
    uint32_t nsnd_que, nrcv_que, nsnd_buf, nrcv_buf;
    os_mutex_t sndq_mutex;  //操作发送队列和发送缓存队列，共用此锁
    os_mutex_t rcvq_mutex;
    os_sem_t sndq_sem;
    os_sem_t rcvq_sem;
    os_mutex_t bak_mutex;
    uint16_t bak_una;       //未确认的sn，也就是在此之前的sn都是确认的
    uint32_t bak_bitmap;    //bit0~bit31, 如果某一位置1，表示una+1 ~ una+32被确认
    uint32_t bak_bitmap2;   //bit0~bit31, 如果某一位置1，表示una+33 ~ una+64被确认

    int conn_status;
    uint8_t   rcv_one_hb;  //server端收到一个hb之后再开始发送
    uint32_t  last_hb_ts;  //us时间戳

    ucp_stat_t stat_res;
    ucp_stat_process_t curr_stat;
    ucp_stat_process_t last_stat;
    uint32_t   last_stat_ms;

    udp_client_conn_thread_info_t udp_client_conn_thread;
} ucp_sock_t;

//void ucp_delete_segment(ucp_seg_t *seg);
#define ucp_delete_segment os_free

ucp_sock_t* ucp_get_sock();
ucp_seg_t* ucp_new_segment(uint32_t size);

void ucp_set_conn_state(int state);
int ucp_init_tx();
int ucp_init_udp_client(ucp_sock_t *sock, const char *server_ip);
int ucp_init_udp_server(ucp_sock_t *sock);
int ucp_init_rx();
void ucp_register_rx_cb(RX_CB_FUN rx_msg_cb, RX_CB_FUN rx_video_cb, RX_CB_FUN rx_audio_cb);
void ucp_register_sndbuf_num_cb(SNDBUF_NUM_CB_FUN cb);
void ucp_notify_sndbuf_num(int num, int wnd, int conn_state);



#endif /* _UCP_PRIVATE_H_ */
