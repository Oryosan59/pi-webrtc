# Pi WebRTC Sender

Raspberry Pi から GStreamer を使用して WebRTC 経由で映像を送信するためのツールです。
Windows 上で動作する Tauri アプリ（ビューアー）などと組み合わせて使用することを想定しています。

## 特徴

- **GStreamer ベース**: 高効率な映像処理と WebRTC プロトコルスタックを利用。
- **H.264 ハードウェアエンコード**: Raspberry Pi の機能を活用した低遅延配信。
- **簡単セットアップ**: インストールスクリプト `setup_pi.sh` により、依存関係の解決とビルドが容易。

## 必要条件

- Raspberry Pi (OS: Raspberry Pi OS 64-bit 推奨)
- カメラモジュール (V4L2 互換)
- インターネット接続（またはシグナリングサーバーへのネットワーク接続）

## セットアップ

以下のコマンドを実行して、必要なパッケージのインストールとビルドを行います。

```bash
chmod +x setup_pi.sh
./setup_pi.sh
```

このスクリプトは以下の順序で実行されます：
1. 依存パッケージ（GStreamer, libsoup, json-glib など）のチェック
2. ソースコードのビルド
3. カメラデバイスの確認
4. シグナリングサーバー（Windows 側など）の IP アドレス設定

## 手動ビルド

Makefile を使用して手動でビルドすることも可能です。

```bash
make
```

### 依存関係のインストール

Raspberry Pi OS で以下のコマンドを実行して必要なライブラリをインストールします。

```bash
make install_deps
```

## 使い方

ビルドが完了したら、シグナリングサーバーの URL を引数に指定して実行します。

```bash
./pi_webrtc_sender ws://<SERVER_IP>:9001
```

※デフォルトでは `192.168.4.10:9001` への接続を試みます。

## ファイル構成

- `pi_webrtc_sender.cpp`: WebRTC 送信機のメイン実装（C++, GStreamer）。
- `setup_pi.sh`: ラズパイでの初期設定・ビルドを支援するスクリプト。
- `Makefile`: コンパイル用。
- `.gitignore`: Git 管理対象外ファイルの定義。
