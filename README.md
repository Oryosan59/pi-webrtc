# Pi WebRTC

Raspberry Pi のカメラ映像を WebRTC 経由で Windows デスクトップアプリにリアルタイム配信するシステムです。

## アーキテクチャ

```
┌─────────────────────┐       WebSocket        ┌─────────────────────┐
│   Raspberry Pi      │    (Signaling: 9001)    │   Windows PC        │
│                     │◄──────────────────────►│                     │
│  GStreamer + WebRTC  │                        │  Tauri 2.0 Viewer   │
│  (H.264 Sender)     │ ─── WebRTC P2P ──────► │  (React + Rust)     │
│                     │    (Video Stream)       │                     │
└─────────────────────┘                        └─────────────────────┘
```

## ディレクトリ構成

| ディレクトリ | 説明 |
| :--- | :--- |
| [`sender/`](./sender/) | Raspberry Pi 側の映像送信プログラム（C++ / GStreamer） |
| [`viewer/`](./viewer/) | Windows 側のビューアーアプリ（Tauri 2.0 / React + Rust） |

## クイックスタート

### 1. Viewer（Windows 側）

```bash
cd viewer
npm install
npm run tauri dev
```

### 2. Sender（Raspberry Pi 側）

```bash
cd sender
chmod +x setup_pi.sh
./setup_pi.sh
```

詳しい手順は各ディレクトリの README.md を参照してください。

## 必要な環境

| コンポーネント | 要件 |
| :--- | :--- |
| **Sender** | Raspberry Pi OS (64-bit), V4L2 カメラ |
| **Viewer** | Windows 10/11, Node.js 18+, Rust 1.70+ |
