FROM ubuntu:latest AS builder

RUN apt update && apt install -y build-essential libboost-system-dev libboost-chrono-dev libboost-random-dev libssl-dev

ADD https://github.com/arvidn/libtorrent/releases/download/libtorrent-1_1_13/libtorrent-rasterbar-1.1.13.tar.gz libtorrent-rasterbar-1.1.13.tar.gz

RUN tar xf libtorrent-rasterbar-1.1.13.tar.gz && cd libtorrent-rasterbar-1.1.13 && ./configure && make -j`nproc` && make install

ADD https://github.com/qbittorrent/qBittorrent/archive/master.zip master.zip

RUN apt install -y unzip pkg-config qt5-default qttools5-dev libz-dev && unzip master.zip && cd qBittorrent-master && ./configure --disable-gui && make -j`nproc` && make install && strip /usr/local/lib/libtorrent-rasterbar.so.9.0.0

FROM ubuntu:latest

COPY --from=builder /usr/local/lib/libtorrent-rasterbar.so.9.0.0 /usr/lib/x86_64-linux-gnu/libtorrent-rasterbar.so.9

COPY --from=builder /usr/local/bin/qbittorrent-nox /usr/bin/qbittorrent-nox

COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh && apt update && apt install -y libboost-system1.65.1 libssl1.1 libqt5core5a libqt5network5 libqt5xml5 && rm -r /var/lib/apt /var/cache/apt && useradd -d /config -m qbittorrent

EXPOSE 6881 6881/udp 8080

VOLUME /config /downloads

ENV HOME="/config" XDG_CONFIG_HOME="/config" XDG_DATA_HOME="/config" CHUID="1000" CHGID="1000" WEBUI_PORT="8080"

WORKDIR /config

ENTRYPOINT ["/entrypoint.sh"]
