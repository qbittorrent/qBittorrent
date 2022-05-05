# qBittorrent-nox Docker Image

This Dockerfile allows you to build a docker image containing qBittorrent-nox

## Prerequisities

In order to build/run this image you'll need `docker` installed: https://docs.docker.com/get-docker/

It is also recommended to install `docker-compose` as it can significantly ease the process: https://docs.docker.com/compose/install/

## Building docker image

* If you are using docker (not docker-compose) then run the following commands in this folder:
  ```shell
  export \
    QBT_VERSION=devel
  docker build \
    --build-arg QBT_VERSION \
    -t qbittorrent-nox:"$QBT_VERSION" \
    .
  ```

* If you are using docker-compose then you should edit `.env` file first.
  You can find an explaination of the variables in the following [Parameters](#parameters) section. \
  Then run the following commands in this folder:
  ```shell
  docker compose build \
    --build-arg QBT_VERSION
  ```

### Parameters

#### Environment variables

* `QBT_EULA` \
  This environment variable defines whether you accept the end-user license agreement (EULA) of qBittorrent. \
  **Put `accept` only if you understand and accepted the EULA.** You can find
  the EULA [here](https://github.com/qbittorrent/qBittorrent/blob/56667e717b82c79433ecb8a5ff6cc2d7b315d773/src/app/main.cpp#L320-L323).
* `QBT_VERSION` \
  This environment variable specifies the version of qBittorrent-nox to be built. \
  For example, `4.4.0` is a valid entry. You can find all tagged versions [here](https://github.com/qbittorrent/qBittorrent/tags). \
  Or you can put `devel` to build the latest development version.
* `QBT_WEBUI_PORT` \
  This environment variable sets the port number which qBittorrent WebUI will be binded to.

#### Volumes

There are some paths involved:
* `<your_path>/config` on your host machine will contain qBittorrent configurations
* `<your_path>/downloads` on your host machine will contain the files downloaded by qBittorrent

## Running container

* Using docker (not docker-compose), simply run:
  ```shell
  export \
    QBT_EULA=accept \
    QBT_VERSION=devel \
    QBT_WEBUI_PORT=8080 \
    QBT_CONFIG_PATH="/tmp/bbb/config"
    QBT_DOWNLOADS_PATH="/tmp/bbb/downloads"
  docker run \
    -t \
    --read-only \
    --rm \
    --tmpfs /tmp \
    --name qbittorrent-nox \
    -e QBT_EULA \
    -e QBT_WEBUI_PORT \
    -p "$QBT_WEBUI_PORT":"$QBT_WEBUI_PORT"/tcp \
    -p 6881:6881/tcp \
    -p 6881:6881/udp \
    -v "$QBT_CONFIG_PATH":/config \
    -v "$QBT_DOWNLOADS_PATH":/downloads \
    qbittorrent-nox:"$QBT_VERSION"
  ```

* Using docker-compose:
  ```shell
  docker compose up
  ```

Then you can login at: `http://127.0.0.1:8080`

## Stopping container

* Using docker (not docker-compose):
  ```shell
  docker stop -t 1800 qbittorrent-nox
  ```

* Using docker-compose:
  ```shell
  docker compose down
  ```
