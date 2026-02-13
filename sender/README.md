# Pi WebRTC Sender

Raspberry Pi から GStreamer を使用して WebRTC 経由で H.264 映像を送信するプログラムです。

## 必要条件

- Raspberry Pi (Raspberry Pi OS 64-bit 推奨)
- V4L2 互換カメラモジュール
- GStreamer 1.18+

## セットアップ

```bash
chmod +x setup_pi.sh
./setup_pi.sh
```

スクリプトが以下を自動的に行います:

1. 依存パッケージのインストール（GStreamer, libsoup, json-glib）
2. ソースコードのビルド
3. カメラデバイスの検出と設定
4. シグナリングサーバーの接続先設定

## 手動ビルド

```bash
# 依存関係のインストール
make install_deps

# ビルド
make

# 実行（引数: Windows側のシグナリングサーバーURL）
./pi_webrtc_sender ws://<WINDOWS_IP>:9001
```

## ファイル構成

| ファイル | 説明 |
| :--- | :--- |
| `pi_webrtc_sender.cpp` | WebRTC 送信機の実装（C++, GStreamer） |
| `Makefile` | ビルド設定 |
| `setup_pi.sh` | セットアップスクリプト |

## 技術詳細

- **映像ソース**: V4L2 (`/dev/video2`)
- **コーデック**: H.264 (Baseline Profile)
- **解像度**: 1920x1080 @ 30fps
- **シグナリング**: WebSocket (JSON)
- **STUN**: `stun.l.google.com:19302`
