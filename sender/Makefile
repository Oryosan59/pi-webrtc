CC = g++
CFLAGS = -Wall -O2 -DGST_USE_UNSTABLE_API
PKGS = gstreamer-1.0 gstreamer-webrtc-1.0 libsoup-2.4 json-glib-1.0

all: pi_webrtc_sender

pi_webrtc_sender: pi_webrtc_sender.cpp
	$(CC) $(CFLAGS) `pkg-config --cflags $(PKGS)` -o $@ $< `pkg-config --libs $(PKGS)`

clean:
	rm -f pi_webrtc_sender

prune_viewer:
	@echo "Removing viewer directory..."
	@rm -rf ../viewer 2>/dev/null || true

# ビルド時にviewerを削除するかどうかはユーザーの選択次第だが
# "viewerの方が必要ないからmakeした時に削除されるようにする" という要望に応えるなら下記
clean_build: prune_viewer all

install_deps:
	@echo "Installing dependencies on Raspberry Pi..."
	sudo apt-get update
	sudo apt-get install -y \
		libgstreamer1.0-dev \
		libgstreamer-plugins-base1.0-dev \
		libgstreamer-plugins-bad1.0-dev \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad \
		gstreamer1.0-plugins-ugly \
		gstreamer1.0-nice \
		libsoup2.4-dev \
		libjson-glib-dev

.PHONY: all clean install_deps
