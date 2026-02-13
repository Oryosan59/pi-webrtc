#include <glib.h>
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <iostream>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string>

// グローバル変数
static GstElement *pipeline = nullptr;
static GstElement *webrtc = nullptr;
static SoupWebsocketConnection *ws_conn = nullptr;

// コマンドライン引数
static gint width = 1920;
static gint height = 1080;
static gint fps = 30;
static gint bitrate = 2000000;
static gchar **ws_urls = nullptr;

static GOptionEntry entries[] = {
    {"width", 'w', 0, G_OPTION_ARG_INT, &width, "Video width", "1920"},
    {"height", 'h', 0, G_OPTION_ARG_INT, &height, "Video height", "1080"},
    {"fps", 'f', 0, G_OPTION_ARG_INT, &fps, "Video FPS", "30"},
    {"bitrate", 'b', 0, G_OPTION_ARG_INT, &bitrate, "Video bitrate (bps)",
     "2000000"},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &ws_urls,
     "WebSocket URL", "ws://PC_IP:9001"},
    {nullptr}};

// WebSocket経由でJSONメッセージ送信
void send_ws_message(const gchar *type, const gchar *data) {
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "type");
  json_builder_add_string_value(builder, type);
  if (data) {
    json_builder_set_member_name(builder, "data");
    json_builder_add_string_value(builder, data);
  }
  json_builder_end_object(builder);

  JsonGenerator *gen = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  gchar *msg_str = json_generator_to_data(gen, nullptr);

  if (ws_conn && soup_websocket_connection_get_state(ws_conn) ==
                     SOUP_WEBSOCKET_STATE_OPEN) {
    soup_websocket_connection_send_text(ws_conn, msg_str);
  }

  g_free(msg_str);
  g_object_unref(gen);
  g_object_unref(builder);
  json_node_free(root);
}

// ICE Candidate受信時の処理
static void on_ice_candidate(GstElement *webrtc, guint mline_index,
                             gchar *candidate, gpointer user_data) {
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "candidate");
  json_builder_add_string_value(builder, candidate);
  json_builder_set_member_name(builder, "sdpMLineIndex");
  json_builder_add_int_value(builder, mline_index);
  json_builder_end_object(builder);

  JsonGenerator *gen = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  gchar *candidate_str = json_generator_to_data(gen, nullptr);

  send_ws_message("ice", candidate_str);

  g_free(candidate_str);
  g_object_unref(gen);
  g_object_unref(builder);
  json_node_free(root);
}

// WebRTCのネゴシエーション（今回は受信側からのOffer待ちなので何もしない）
static void on_negotiation_needed(GstElement *element, gpointer user_data) {}

// WebSocketからのメッセージ処理
static void on_ws_message(SoupWebsocketConnection *conn, gint type,
                          GBytes *message, gpointer user_data) {
  if (type != SOUP_WEBSOCKET_DATA_TEXT)
    return;

  gsize sz;
  const gchar *ptr = (const gchar *)g_bytes_get_data(message, &sz);
  std::string msg_str(ptr, sz);

  JsonParser *parser = json_parser_new();
  if (!json_parser_load_from_data(parser, msg_str.c_str(), -1, nullptr)) {
    g_object_unref(parser);
    return;
  }

  JsonNode *root = json_parser_get_root(parser);
  JsonObject *obj = json_node_get_object(root);
  const gchar *msg_type = json_object_get_string_member(obj, "type");

  if (g_strcmp0(msg_type, "offer") == 0) {
    const gchar *sdp_str = json_object_get_string_member(obj, "data");
    GstSDPMessage *sdp;
    gst_sdp_message_new(&sdp);
    gst_sdp_message_parse_buffer((const guint8 *)sdp_str, strlen(sdp_str), sdp);

    GstWebRTCSessionDescription *offer =
        gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
    GPromise *promise = gst_promise_new();
    g_signal_emit_by_name(webrtc, "set-remote-description", offer, promise);
    gst_promise_wait(promise);
    gst_promise_unref(promise);
    gst_webrtc_session_description_free(offer);

    // Create Answer
    promise = gst_promise_new_with_change_func(
        [](GPromise *p, gpointer user_data) {
          GstWebRTCSessionDescription *answer = nullptr;
          const GstStructure *reply = gst_promise_get_reply(p);
          gst_structure_get(reply, "answer",
                            GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer,
                            nullptr);
          g_signal_emit_by_name(webrtc, "set-local-description", answer,
                                nullptr);

          gchar *sdp_text = gst_sdp_message_as_text(answer->sdp);
          send_ws_message("answer", sdp_text);
          g_free(sdp_text);
          gst_webrtc_session_description_free(answer);
        },
        nullptr, nullptr);

    g_signal_emit_by_name(webrtc, "create-answer", nullptr, promise);

  } else if (g_strcmp0(msg_type, "ice") == 0) {
    const gchar *candidate_json = json_object_get_string_member(obj, "data");
    JsonParser *c_parser = json_parser_new();
    if (json_parser_load_from_data(c_parser, candidate_json, -1, nullptr)) {
      JsonObject *c_obj = json_node_get_object(json_parser_get_root(c_parser));
      const gchar *candidate =
          json_object_get_string_member(c_obj, "candidate");
      gint mline_index = json_object_get_int_member(c_obj, "sdpMLineIndex");
      g_signal_emit_by_name(webrtc, "add-ice-candidate", mline_index,
                            candidate);
    }
    g_object_unref(c_parser);
  }

  g_object_unref(parser);
}

static void on_ws_closed(SoupWebsocketConnection *conn, gpointer user_data) {
  std::cout << "WebSocket disconnected" << std::endl;
  ws_conn = nullptr;
  g_main_loop_quit((GMainLoop *)user_data);
}

static void on_ws_connected(SoupSession *session, GAsyncResult *res,
                            gpointer user_data) {
  GError *error = nullptr;
  ws_conn = soup_session_websocket_connect_finish(session, res, &error);
  if (error) {
    std::cerr << "WebSocket connection failed: " << error->message << std::endl;
    g_error_free(error);
    g_main_loop_quit((GMainLoop *)user_data);
    return;
  }
  std::cout << "WebSocket connected!" << std::endl;
  g_signal_connect(ws_conn, "message", G_CALLBACK(on_ws_message), nullptr);
  g_signal_connect(ws_conn, "closed", G_CALLBACK(on_ws_closed), user_data);
}

int main(int argc, char *argv[]) {
  gst_init(&argc, &argv);

  GError *error = nullptr;
  GOptionContext *context = g_option_context_new("WS_URL - Pi WebRTC Sender");
  g_option_context_add_main_entries(context, entries, nullptr);
  g_option_context_add_group(context, gst_init_get_option_group());
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    std::cerr << "Option parsing failed: " << error->message << std::endl;
    return 1;
  }

  if (!ws_urls || !ws_urls[0]) {
    std::cout << g_option_context_get_help(context, TRUE, nullptr);
    std::cerr << "Error: WebSocket URL is required." << std::endl;
    return 1;
  }

  std::string url = ws_urls[0];
  g_strfreev(ws_urls);
  g_option_context_free(context);

  std::cout << "Connecting to: " << url << std::endl;
  std::cout << "Format: " << width << "x" << height << " @ " << fps << "fps"
            << std::endl;
  std::cout << "Bitrate: " << bitrate << " bps" << std::endl;

  SoupSession *session = soup_session_new();
  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);

  // パイプライン構築
  // ビットレート制御のため、x264enc / v4l2h264enc に bitrate/video_bitrate
  // を設定できると良いが
  // ここではシンプルさを保つため、まず解像度とFPSを反映する
  gchar *pipeline_str = g_strdup_printf(
      "v4l2src device=/dev/video2 io-mode=dmabuf ! "
      "video/"
      "x-h264,profile=baseline,stream-format=byte-stream,alignment=au,"
      "framerate=%d/1,width=%d,height=%d ! "
      "h264parse ! "
      "rtph264pay pt=96 config-interval=-1 aggregate-mode=zero-latency "
      "mtu=1200 ! "
      "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
      "webrtcbin name=sendonly bundle-policy=max-bundle "
      "stun-server=stun://stun.l.google.com:19302 latency=0",
      fps, width, height);

  std::cout << "Pipeline: " << pipeline_str << std::endl;

  pipeline = gst_parse_launch(pipeline_str, &error);
  g_free(pipeline_str);

  if (error) {
    std::cerr << "Pipeline parsing error: " << error->message << std::endl;
    return 1;
  }

  webrtc = gst_bin_get_by_name(GST_BIN(pipeline), "sendonly");
  g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(on_ice_candidate),
                   nullptr);
  g_signal_connect(webrtc, "on-negotiation-needed",
                   G_CALLBACK(on_negotiation_needed), nullptr);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  soup_session_websocket_connect_async(
      session, soup_message_new(SOUP_METHOD_GET, url.c_str()), nullptr, nullptr,
      nullptr, on_ws_connected, loop);

  g_main_loop_run(loop);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}
