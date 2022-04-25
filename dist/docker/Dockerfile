
FROM alpine:latest AS builder

ARG BUILD_TYPE
ARG RELEASE

RUN if [ $RELEASE = "master" ] ; \
    then \
      wget https://github.com/qbittorrent/qBittorrent/archive/refs/heads/master.zip && \
      unzip master.zip && \
      cd qBittorrent-master ; \
    else \
      wget https://github.com/qbittorrent/qBittorrent/archive/refs/tags/release-${RELEASE}.tar.gz && \
      tar xf release-${RELEASE}.tar.gz && \
      cd qBittorrent-release-${RELEASE} ; \
    fi && \
    apk add --no-cache qt6-qttools-dev g++ libtorrent-rasterbar-dev cmake boost-dev ninja && \
    cmake -B build-nox -G "Ninja" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DGUI=OFF -DQT6=ON -DSTACKTRACE=OFF && \
    cmake --build build-nox && \
    cmake --build build-nox --target install/strip

FROM alpine:latest

COPY --from=builder /usr/local/bin/qbittorrent-nox /usr/bin/qbittorrent-nox

COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh && \
    apk add --no-cache qt6-qtbase libtorrent-rasterbar

ENV WEBUI_PORT="8080"

EXPOSE 6881 6881/udp 8080

VOLUME /config /downloads

ENTRYPOINT ["/entrypoint.sh"]
