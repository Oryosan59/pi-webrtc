use tauri::Manager;
use warp::Filter;
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use futures::{FutureExt, StreamExt, SinkExt};
use tokio::sync::mpsc;

// クライアント管理用: Client ID -> 送信チャンネル
type Clients = Arc<Mutex<HashMap<usize, mpsc::UnboundedSender<Result<warp::ws::Message, warp::Error>>>>>;

// グローバルなクライアントIDカウンター
static NEXT_USER_ID: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(1);

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            // WebSocketサーバーを別タスクで起動
            tauri::async_runtime::spawn(async move {
                start_signaling_server().await;
            });
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

async fn start_signaling_server() {
    let clients = Clients::default();
    let clients_filter = warp::any().map(move || clients.clone());

    let ws_route = warp::path::end()
        .and(warp::ws())
        .and(clients_filter)
        .map(|ws: warp::ws::Ws, clients| {
            ws.on_upgrade(move |socket| client_connection(socket, clients))
        });

    println!("Signaling server starting on 0.0.0.0:9001");
    // 0.0.0.0 でリッスンして外部（ラズパイ）からの接続を受け付ける
    warp::serve(ws_route).run(([0, 0, 0, 0], 9001)).await;
}

async fn client_connection(ws: warp::ws::WebSocket, clients: Clients) {
    let my_id = NEXT_USER_ID.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
    println!("New client connected: {}", my_id);

    // WebSocket ストリームを送信・受信に分割
    let (mut client_ws_tx, mut client_ws_rx) = ws.split();
    
    // クライアントへの送信チャンネルを作成
    let (tx, rx) = mpsc::unbounded_channel();
    let mut rx = tokio_stream::wrappers::UnboundedReceiverStream::new(rx);

    // 送信タスク: チャンネルから受け取ったメッセージをWebSocketに書き込む
    tokio::task::spawn(async move {
        while let Some(message) = rx.next().await {
            if let Ok(msg) = message {
                if let Err(e) = client_ws_tx.send(msg).await {
                    eprintln!("websocket send error: {}", e);
                    break;
                }
            }
        }
    });

    // クライアントリストに登録
    clients.lock().unwrap().insert(my_id, tx);

    // 受信ループ: WebSocketからメッセージを受け取り、他へブロードキャスト
    while let Some(result) = client_ws_rx.next().await {
        match result {
            Ok(msg) => {
                client_msg(&clients, my_id, msg).await;
            }
            Err(e) => {
                eprintln!("websocket error(uid={}): {}", my_id, e);
                break;
            }
        }
    }

    // 切断処理
    clients.lock().unwrap().remove(&my_id);
    println!("Client disconnected: {}", my_id);
}

async fn client_msg(clients: &Clients, my_id: usize, msg: warp::ws::Message) {
    // テキストメッセージのみ処理（WebRTCシグナリングはJSON文字列）
    let msg_str = if let Ok(s) = msg.to_str() { 
        s 
    } else { 
        return 
    };

    println!("Received message from {}: {}", my_id, msg_str);

    // 自分以外の全クライアントに転送（ブロードキャスト）
    let clients_guard = clients.lock().unwrap();
    for (&uid, tx) in clients_guard.iter() {
        if my_id != uid {
            // エラーは無視（相手が切断されている場合など）
            let _ = tx.send(Ok(warp::ws::Message::text(msg_str.to_string())));
        }
    }
}
