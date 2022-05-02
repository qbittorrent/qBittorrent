# qBittorrent-nox Docker Image

This Dockerfile allows you to build a docker image containing qBittorrent-nox

## Prerequisities

In order to build/run this image you'll need docker installed: https://docs.docker.com/get-docker/

## Building docker image

In this docker folder run:
```shell
export \
  QBT_VERSION=devel
docker build \
  --build-arg QBT_VERSION \
  -t qbittorrent-nox:"$QBT_VERSION" \
  .
```

### Parameters

* `QBT_VERSION`
  This environment variable specifies the version of qBittorrent-nox to be built. \
  For example, `4.4.0` is a valid entry. You can find all tagged versions [here](https://github.com/qbittorrent/qBittorrent/tags). \
  Or you can put `devel` to build the latest development version.

## Running docker image

* To start the the docker image simply run:
  ```shell
  export \
    QBT_VERSION=devel \
    QBT_EULA=accept \
    QBT_WEBUI_PORT=8080
  docker run \
    -it \
    --rm \
    --name qbittorrent-nox \
    -e QBT_EULA \
    -e QBT_WEBUI_PORT \
    -p "$QBT_WEBUI_PORT":"$QBT_WEBUI_PORT" \
    -p 6881:6881/tcp \
    -p 6881:6881/udp \
    -v /your_path/config:/config \
    -v /your_path/downloads:/downloads \
    qbittorrent-nox:"$QBT_VERSION"
  ```
  Then you can login at: `http://127.0.0.1:8080`

* To stop the container:
  ```shell
  docker stop -t 1800 qbittorrent-nox
  ```

### Parameters

* `QBT_VERSION` \
  The same as [above](#variables).
* `QBT_EULA` \
  This environment variable defines whether you accept the end-user license agreement (EULA) of qBittorrent. \
  Put `accept` only if you understand and accepted the EULA. You can find
  the EULA [here](https://github.com/qbittorrent/qBittorrent/blob/56667e717b82c79433ecb8a5ff6cc2d7b315d773/src/app/main.cpp#L320-L323).
* `QBT_WEBUI_PORT` \
  This environment variable sets the port number which qBittorrent WebUI will be binded to.

### Volumes

There are some paths involved:
* `/your_path/config` on your host machine will contain qBittorrent configurations
* `/your_path/downloads` on your host machine will contain the files downloaded by qBittorrent
