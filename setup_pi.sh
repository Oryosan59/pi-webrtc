#!/bin/bash

set -e

echo "=================================="
echo "Pi WebRTC Sender - クイックスタート"
echo "=================================="
echo ""

# 依存パッケージチェック
echo "[1/4] 依存パッケージのチェック..."
if ! pkg-config --exists gstreamer-1.0 gstreamer-webrtc-1.0 libsoup-3.0 json-glib-1.0; then
    echo "❌ 必要なパッケージがインストールされていません"
    echo ""
    read -p "今すぐインストールしますか? (y/N): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        make install_deps
    else
        echo "インストールをキャンセルしました"
        exit 1
    fi
fi
echo "✅ 依存パッケージOK"
echo ""

# ビルド
echo "[2/4] ビルド中..."
if [ ! -f "pi_webrtc_sender" ]; then
    make
fi
echo "✅ ビルド完了"
echo ""

# カメラチェック
echo "[3/4] カメラデバイスのチェック..."
if [ ! -e "/dev/video2" ]; then
    echo "⚠️  /dev/video2 が見つかりません"
    echo "利用可能なカメラ:"
    v4l2-ctl --list-devices
    echo ""
    read -p "使用するデバイスパス (例: /dev/video0): " DEVICE
    if [ -z "$DEVICE" ]; then
        echo "デバイスが指定されませんでした"
        exit 1
    fi
    # コード内のデバイスパスを置き換え（簡易版）
    sed -i "s|/dev/video2|$DEVICE|g" pi_webrtc_sender.cpp
    make clean && make
else
    echo "✅ /dev/video2 が見つかりました"
fi
echo ""

# シグナリングサーバーアドレス入力
echo "[4/4] シグナリングサーバー設定..."
read -p "WindowsマシンのIPアドレスを入力 (例: 192.168.4.10): " WINDOWS_IP
if [ -z "$WINDOWS_IP" ]; then
    echo "IPアドレスが入力されませんでした"
    exit 1
fi

WS_URL="ws://${WINDOWS_IP}:9001"
echo ""
echo "=================================="
echo "✅ セットアップ完了！"
echo "=================================="
echo ""
echo "接続先: $WS_URL"
echo ""
echo "次のステップ:"
echo "1. WindowsマシンでTauriアプリを起動してください"
echo "2. このターミナルで以下を実行:"
echo ""
echo "   ./pi_webrtc_sender $WS_URL"
echo ""
read -p "今すぐ起動しますか? (y/N): " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "起動中..."
    ./pi_webrtc_sender "$WS_URL"
fi
