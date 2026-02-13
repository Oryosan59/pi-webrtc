#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string>

// グローバル変数
static GstElement *pipeline = nullptr;
static GstElement *webrtc = nullptr;
static SoupWebsocketConnection *ws_conn = nullptr;

// WebSocket経由でJSONメッセージ送信
void send_ws_message(const gchar *type, const gchar *data) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, type);
    json_builder_set_member_name(builder, "data");
    json_builder_add_string_value(builder, data);
    json_builder_end_object(builder);
    
    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *json_str = json_generator_to_data(gen, nullptr);
    
    soup_websocket_connection_send_text(ws_conn, json_str);
    
    g_free(json_str);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}

// ICE候補が生成されたときのコールバック
void on_ice_candidate(GstElement *webrtc_elem, guint mline_idx, gchar *candidate, gpointer user_data) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "candidate");
    json_builder_add_string_value(builder, candidate);
    json_builder_set_member_name(builder, "sdpMLineIndex");
    json_builder_add_int_value(builder, mline_idx);
    json_builder_end_object(builder);
    
    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *json_str = json_generator_to_data(gen, nullptr);
    
    send_ws_message("ice", json_str);
    
    g_free(json_str);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}

// Offer SDP作成完了時のコールバック
void on_offer_created(GstPromise *promise, gpointer user_data) {
    GstWebRTCSessionDescription *offer = nullptr;
    const GstStructure *reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr);
    gst_promise_unref(promise);
    
    // Local Descriptionをセット
    g_signal_emit_by_name(webrtc, "set-local-description", offer, nullptr);
    
    // SDP文字列を取得してWebSocket経由で送信
    gchar *sdp_string = gst_sdp_message_as_text(offer->sdp);
    send_ws_message("offer", sdp_string);
    g_free(sdp_string);
    
    gst_webrtc_session_description_free(offer);
}

// WebRTC Offerを作成
void create_offer() {
    GstPromise *promise = gst_promise_new_with_change_func(on_offer_created, nullptr, nullptr);
    g_signal_emit_by_name(webrtc, "create-offer", nullptr, promise);
}

// WebSocketメッセージ受信時のコールバック
void on_ws_message(SoupWebsocketConnection *conn, SoupWebsocketDataType type, GBytes *message, gpointer user_data) {
    if (type != SOUP_WEBSOCKET_DATA_TEXT) return;
    
    gsize size;
    const gchar *data = (const gchar *)g_bytes_get_data(message, &size);
    
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, data, size, nullptr)) {
        g_object_unref(parser);
        return;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    JsonObject *obj = json_node_get_object(root);
    const gchar *msg_type = json_object_get_string_member(obj, "type");
    
    if (g_strcmp0(msg_type, "answer") == 0) {
        // Answer SDPを受信
        const gchar *sdp_str = json_object_get_string_member(obj, "data");
        
        GstSDPMessage *sdp;
        gst_sdp_message_new_from_text(sdp_str, &sdp);
        
        GstWebRTCSessionDescription *answer = gst_webrtc_session_description_new(
            GST_WEBRTC_SDP_TYPE_ANSWER, sdp
        );
        
        g_signal_emit_by_name(webrtc, "set-remote-description", answer, nullptr);
        gst_webrtc_session_description_free(answer);
        
    } else if (g_strcmp0(msg_type, "ice") == 0) {
        // ICE候補を受信
        const gchar *ice_data = json_object_get_string_member(obj, "data");
        JsonParser *ice_parser = json_parser_new();
        json_parser_load_from_data(ice_parser, ice_data, -1, nullptr);
        JsonObject *ice_obj = json_node_get_object(json_parser_get_root(ice_parser));
        
        const gchar *candidate = json_object_get_string_member(ice_obj, "candidate");
        gint sdp_mline_index = json_object_get_int_member(ice_obj, "sdpMLineIndex");
        
        g_signal_emit_by_name(webrtc, "add-ice-candidate", sdp_mline_index, candidate);
        
        g_object_unref(ice_parser);
    }
    
    g_object_unref(parser);
}

// WebSocket接続確立時のコールバック
void on_ws_connected(GObject *session, GAsyncResult *res, gpointer user_data) {
    GError *error = nullptr;
    ws_conn = soup_session_websocket_connect_finish(SOUP_SESSION(session), res, &error);
    
    if (error) {
        g_printerr("WebSocket connection failed: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    g_print("WebSocket connected!\n");
    g_signal_connect(ws_conn, "message", G_CALLBACK(on_ws_message), nullptr);
    
    // 接続後すぐにOfferを作成
    create_offer();
}

// GStreamerパイプライン構築
gboolean setup_pipeline() {
    GError *error = nullptr;
    
    // パイプライン作成
    pipeline = gst_parse_launch(
        "v4l2src device=/dev/video2 io-mode=dmabuf ! "
        "video/x-h264,profile=baseline,stream-format=byte-stream,alignment=au,framerate=30/1,width=1920,height=1080 ! "
        "h264parse config-interval=1 ! "
        "rtph264pay pt=96 config-interval=-1 mtu=1200 ! "
        "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
        "webrtcbin name=sendonly bundle-policy=max-bundle stun-server=stun://stun.l.google.com:19302",
        &error
    );
    
    if (error) {
        g_printerr("Pipeline parse error: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    webrtc = gst_bin_get_by_name(GST_BIN(pipeline), "sendonly");
    if (!webrtc) {
        g_printerr("Failed to get webrtcbin element\n");
        return FALSE;
    }
    
    // ICE候補通知のシグナル接続
    g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(on_ice_candidate), nullptr);
    
    // パイプライン開始
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Pipeline started\n");
    
    return TRUE;
}

int main(int argc, char *argv[]) {
    // 初期化
    gst_init(&argc, &argv);
    
    // GStreamerパイプライン構築
    if (!setup_pipeline()) {
        return -1;
    }
    
    // WebSocket接続（シグナリングサーバーのアドレス）
    const char *ws_url = "ws://192.168.4.10:9001";
    if (argc > 1) {
        ws_url = argv[1];
    }
    
    SoupSession *session = soup_session_new();
    SoupMessage *msg = soup_message_new("GET", ws_url);
    
    soup_session_websocket_connect_async(
        session, msg, nullptr, nullptr, nullptr,
        on_ws_connected, nullptr
    );
    
    // メインループ開始
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_print("Starting main loop... (Connect to %s)\n", ws_url);
    g_main_loop_run(loop);
    
    // クリーンアップ
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    
    return 0;
}
