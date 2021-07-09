FROM alpine:latest AS builder

RUN wget https://github.com/arvidn/libtorrent/releases/download/libtorrent-1_2_1/libtorrent-rasterbar-1.2.1.tar.gz && \
    tar xf libtorrent-rasterbar-1.2.1.tar.gz && \
    cd libtorrent-rasterbar-1.2.1 && \
    apk add --no-cache make g++ boost-dev openssl-dev && \
    ./configure --disable-static && \
    make -j`nproc` && \
    make install && \
    strip /usr/local/lib/libtorrent-rasterbar.so.10.0.0

RUN wget https://github.com/qbittorrent/qBittorrent/archive/master.zip && \
    unzip master.zip && \
    cd qBittorrent-master && \
    apk add --no-cache qt5-qttools-dev && \
    ./configure --disable-gui && \
    make -j`nproc` && \
    make install

FROM alpine:latest

COPY --from=builder /usr/local/lib/libtorrent-rasterbar.so.10.0.0 /usr/lib/libtorrent-rasterbar.so.10

COPY --from=builder /usr/local/bin/qbittorrent-nox /usr/bin/qbittorrent-nox

COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh && apk add --no-cache qt5-qtbase

ENV WEBUI_PORT="8080"

EXPOSE 6881 6881/udp 8080

VOLUME /config /downloads

ENTRYPOINT ["/entrypoint.sh"]
