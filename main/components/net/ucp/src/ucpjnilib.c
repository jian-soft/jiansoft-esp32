#ifdef __ANDROID__
#include "os_interfaces.h"
#include "ucp.h"
#include <jni.h>

/** 对用户app的接口 **/
int32_t Java_net_jiansoft_botfpv_UCP_ucpSendMsg(JNIEnv* env, jobject thiz,
                jbyteArray buffer_, jint len, jbyte need_confirm)
{
    int32_t ret;
    uint8_t *buffer = (*env)->GetByteArrayElements(env, buffer_, NULL);
    ret = ucp_send_msg(buffer, len, need_confirm);
    (*env)->ReleaseByteArrayElements(env, buffer_, buffer, 0);
    return ret;
}

int32_t Java_net_jiansoft_botfpv_UCP_ucpSendHeartBeat(JNIEnv* env, jobject thiz,
                jbyteArray buffer_, jint len)
{
    int32_t ret;
    uint8_t *buffer = (*env)->GetByteArrayElements(env, buffer_, NULL);
    ret = ucp_send_heart_beat(buffer, len);
    (*env)->ReleaseByteArrayElements(env, buffer_, buffer, 0);
    return ret;
}

int32_t Java_net_jiansoft_botfpv_UCP_ucpCheck(JNIEnv* env, jobject thiz,
                int32_t value)
{
    os_log("pass in:%d", value);

    return 1213;
}

int32_t Java_net_jiansoft_botfpv_UCP_ucpStartClientConnect(JNIEnv* env, jobject thiz)
{
    return ucp_start_udpclient_connect();
}
int32_t Java_net_jiansoft_botfpv_UCP_ucpStopClientConnect(JNIEnv* env, jobject thiz)
{
    return ucp_stop_udpclient_connect();
}


typedef struct ucp_context {
    JavaVM *javaVM;

    jclass ucpClz;
    jobject ucpObj;

    jbyteArray rxBuffer;
    jbyteArray rxVideoBuffer;
    jbyteArray rxAudioBuffer;
    int vb_offset;
    jmethodID rcvMsgCbId;
    jmethodID rcvVideoCbId;
    jmethodID rcvAudioCbId;

    JNIEnv *rxCbEnv;
} UcpContext;
UcpContext g_ctx;

static int jni_attach_thread_env()
{
    if (NULL != g_ctx.rxCbEnv) {
        os_log("error, env is already attached\n");
        return -1;
    }

    JavaVM *javaVM = g_ctx.javaVM;
    JNIEnv *env;
    jint res = (*javaVM)->GetEnv(javaVM, (void **)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);
        if (JNI_OK != res) {
          os_log("Failed to AttachCurrentThread, ErrorCode = %d\n", res);
          return -1;
        }
        os_log("AttachCurrentThread ok\n");
        g_ctx.rxCbEnv = env;
    } else {
        os_log("env is already attached, should not be here\n");
        g_ctx.rxCbEnv = env;
    }

    return 0;
}

void jni_rx_msg_cb(uint8_t *msg, int32_t len, ucp_pkt_info_t *info)
{
    int ret;

    if (g_ctx.rcvMsgCbId == NULL) {
        if (g_ctx.rxCbEnv == NULL) {
            ret = jni_attach_thread_env();
            if (ret) {
                return;
            }
        }
        g_ctx.rcvMsgCbId = (*g_ctx.rxCbEnv)->GetMethodID(g_ctx.rxCbEnv, g_ctx.ucpClz, "rcvMsgCb", "([BII)V");
    }

    (*g_ctx.rxCbEnv)->SetByteArrayRegion(g_ctx.rxCbEnv, g_ctx.rxBuffer, 0, len, (jbyte *)msg);
    (*g_ctx.rxCbEnv)->CallVoidMethod(g_ctx.rxCbEnv, g_ctx.ucpObj, g_ctx.rcvMsgCbId, g_ctx.rxBuffer, len, info->is_hb);
}
#if 0
void jni_rx_video_cb(uint8_t *buffer, int32_t len, ucp_pkt_info_t *info)
{
    int ret;
    if (g_ctx.rcvVideoCbId == NULL) {
        if (g_ctx.rxCbEnv == NULL) {
            ret = jni_attach_thread_env();
            if (ret) {
                return;
            }
        }

        g_ctx.rcvVideoCbId = (*g_ctx.rxCbEnv)->GetMethodID(g_ctx.rxCbEnv, g_ctx.ucpClz, "rcvVideoCb", "([BI)V");
    }

    if (g_ctx.vb_offset + len > 200000) {
        os_log("error, vb_offset is too long:%d, len:%d\n", g_ctx.vb_offset, len);
        g_ctx.vb_offset = 0;
        return;
    }

    (*g_ctx.rxCbEnv)->SetByteArrayRegion(g_ctx.rxCbEnv, g_ctx.rxVideoBuffer, g_ctx.vb_offset, len, (jbyte *)buffer);
    g_ctx.vb_offset += len;
    if (info->moredata == 0) {
        //os_log("recv one frame, len:%d\n", g_ctx.vb_offset);
        (*g_ctx.rxCbEnv)->CallVoidMethod(g_ctx.rxCbEnv, g_ctx.ucpObj, g_ctx.rcvVideoCbId, g_ctx.rxVideoBuffer, g_ctx.vb_offset);
        g_ctx.vb_offset = 0;
    }
}
#else
void jni_rx_video_cb(uint8_t *buffer, int32_t len, ucp_pkt_info_t *info)
{
    int ret;
    if (g_ctx.rcvVideoCbId == NULL) {
        if (g_ctx.rxCbEnv == NULL) {
            ret = jni_attach_thread_env();
            if (ret) {
                return;
            }
        }

        g_ctx.rcvVideoCbId = (*g_ctx.rxCbEnv)->GetMethodID(g_ctx.rxCbEnv, g_ctx.ucpClz, "rcvVideoCb", "([BI)V");
    }

    (*g_ctx.rxCbEnv)->SetByteArrayRegion(g_ctx.rxCbEnv, g_ctx.rxVideoBuffer, 0, len, (jbyte *)buffer);
    (*g_ctx.rxCbEnv)->CallVoidMethod(g_ctx.rxCbEnv, g_ctx.ucpObj, g_ctx.rcvVideoCbId, g_ctx.rxVideoBuffer, len);
}
#endif


void jni_rx_audio_cb(uint8_t *buffer, int32_t len, ucp_pkt_info_t *info)
{
    int ret;
    if (g_ctx.rcvAudioCbId == NULL) {
        if (g_ctx.rxCbEnv == NULL) {
            ret = jni_attach_thread_env();
            if (ret) {
                return;
            }
        }

        g_ctx.rcvAudioCbId = (*g_ctx.rxCbEnv)->GetMethodID(g_ctx.rxCbEnv, g_ctx.ucpClz, "rcvAudioCb", "([BI)V");
    }

    (*g_ctx.rxCbEnv)->SetByteArrayRegion(g_ctx.rxCbEnv, g_ctx.rxAudioBuffer, 0, len, (jbyte *)buffer);
    (*g_ctx.rxCbEnv)->CallVoidMethod(g_ctx.rxCbEnv, g_ctx.ucpObj, g_ctx.rcvAudioCbId, g_ctx.rxAudioBuffer, len);
}


void jni_sndbuf_num_cb(int32_t num, int32_t wnd, int32_t conn_state)
{

}

/* role: */
int32_t Java_net_jiansoft_botfpv_UCP_ucpInit(JNIEnv* env, jobject thiz,
                int32_t role, jstring server_ip_)
{
    int32_t ret;
    const char *server_ip = (*env)->GetStringUTFChars(env, server_ip_, 0);
    os_log("server_ip is:%s\n", server_ip);

    jclass clz = (*env)->GetObjectClass(env, thiz);
    g_ctx.ucpClz = (*env)->NewGlobalRef(env, clz);
    g_ctx.ucpObj = (*env)->NewGlobalRef(env, thiz);

    jbyteArray array_ = (*env)->NewByteArray(env, 1500);
    jbyteArray array2_ = (*env)->NewByteArray(env, 200000);
    jbyteArray array3_ = (*env)->NewByteArray(env, 1500);
    g_ctx.rxBuffer = (*env)->NewGlobalRef(env, array_);
    g_ctx.rxVideoBuffer = (*env)->NewGlobalRef(env, array2_);
    g_ctx.rxAudioBuffer = (*env)->NewGlobalRef(env, array3_);

    ucp_register_callback(jni_rx_msg_cb, jni_rx_video_cb, jni_rx_audio_cb, jni_sndbuf_num_cb);
    ret = ucp_client_init(server_ip);

    (*env)->ReleaseStringUTFChars(env,  server_ip_, server_ip);

    return ret;
}

int32_t Java_net_jiansoft_botfpv_UCP_ucpGetStats(JNIEnv* env, jobject thiz,
                jobject stats_)
{
    jclass stats = (*env)->GetObjectClass(env, stats_);
    jfieldID txKbps = (*env)->GetFieldID(env, stats, "txKbps", "I");
    jfieldID rxKbps = (*env)->GetFieldID(env, stats, "rxKbps", "I");
    jfieldID maxRcDelay = (*env)->GetFieldID(env, stats, "maxRcDelay", "I");
    jfieldID avgRcDelay = (*env)->GetFieldID(env, stats, "avgRcDelay", "I");
    jfieldID avgVidDelay = (*env)->GetFieldID(env, stats, "avgVidDelay", "I");
    jfieldID rcvDupCnt = (*env)->GetFieldID(env, stats, "rcvDupCnt", "I");
    jfieldID lossRate = (*env)->GetFieldID(env, stats, "lossRate", "I");
    jfieldID fps = (*env)->GetFieldID(env, stats, "fps", "I");
    jfieldID hbps = (*env)->GetFieldID(env, stats, "hbps", "I");
    ucp_calc_stats();
    ucp_stat_t *stat_res = ucp_get_stats();
    (*env)->SetIntField(env, stats_ , txKbps, stat_res->snd_kbps);
    (*env)->SetIntField(env, stats_ , rxKbps, stat_res->rcv_kbps);
    (*env)->SetIntField(env, stats_ , maxRcDelay, stat_res->rcdelay_res[stat_res->rdly_max_idx]);
    (*env)->SetIntField(env, stats_ , avgRcDelay, stat_res->rdly_avg);
    (*env)->SetIntField(env, stats_ , avgVidDelay, stat_res->vdly_avg_to_rc);
    (*env)->SetIntField(env, stats_ , rcvDupCnt, stat_res->rcv_dup_cnt);
    (*env)->SetIntField(env, stats_ , lossRate, stat_res->loss_rate);
    (*env)->SetIntField(env, stats_ , fps, stat_res->video_fps);
    (*env)->SetIntField(env, stats_ , hbps, stat_res->rcv_hb_ps);

    return 0;
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;

    os_log("JNI_OnLoad in\n");

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.javaVM = vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;  // JNI version not supported.
    }

    return JNI_VERSION_1_6;
}


#endif

