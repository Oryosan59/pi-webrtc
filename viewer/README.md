# Pi WebRTC Viewer

Raspberry Pi からの WebRTC 映像を受信・表示する Windows デスクトップアプリです。
Tauri 2.0（React + Rust）で構築されています。

## 必要条件

- Windows 10/11
- Node.js 18+
- Rust 1.70+
- [Tauri 2.0 の前提条件](https://v2.tauri.app/start/prerequisites/)

## セットアップ

```bash
# 依存関係のインストール
npm install

# 開発モードで起動
npm run tauri dev
```

## 使い方

1. アプリを起動すると、WebSocket シグナリングサーバーが自動的にポート **9001** で起動します。
2. Raspberry Pi 側で Sender を起動し、Windows の IP アドレスを指定します:
   ```bash
   ./pi_webrtc_sender ws://<WINDOWS_IP>:9001
   ```
3. 接続が確立されると、アプリ内に映像が表示されます。

## 技術スタック

| レイヤー | 技術 |
| :--- | :--- |
| **フロントエンド** | React + TypeScript (Vite) |
| **バックエンド** | Rust (Tauri 2.0) |
| **シグナリング** | WebSocket (warp) |
| **映像受信** | WebRTC (ブラウザ API) |

## ビルド（リリース用）

```bash
npm run tauri build
```

ビルド成果物は `src-tauri/target/release/` に生成されます。
