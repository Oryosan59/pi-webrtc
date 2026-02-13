CC = g++
CFLAGS = -Wall -O2 -DGST_USE_UNSTABLE_API
PKGS = gstreamer-1.0 gstreamer-webrtc-1.0 libsoup-2.4 json-glib-1.0

all: pi_webrtc_sender

pi_webrtc_sender: pi_webrtc_sender.cpp
	$(CC) $(CFLAGS) `pkg-config --cflags $(PKGS)` -o $@ $< `pkg-config --libs $(PKGS)`

clean:
	rm -f pi_webrtc_sender

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
