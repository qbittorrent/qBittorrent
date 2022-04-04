# Docker Container Name

This Dockerfile allows you to build a qBittorrent-nox container

## Getting Started

### Prerequisities


In order to run this container you'll need docker installed.

* [Windows](https://docs.docker.com/windows/started)
* [OS X](https://docs.docker.com/mac/started/)
* [Linux](https://docs.docker.com/linux/started/)


## Built

in the docker folder run

```shell
release="4.2.0" ; sudo docker build --build-arg BUILD_TYPE=Release --build-arg RELEASE=$release -t qbittorrent-nox:$release --rm  .
```

where:

* the `release` variable is the specific tagged version you want to build
* the `BUILD_TYPE` argument is the build you want to create `Debug` or `Release`
* the `RELEASE` argument works as the but is the actual argument given to docker, in the above script is defined by the `release` variable


### Usage

#### Container Variables

there is one important variable to run the container:

* the `LEGAL` varible defines if you accept the Legal Notice, put accept as a value only if you understand and accept the Legal Notice

#### Volumes

there are two main locations:

* `downloads` contains the files downloaded by qBittorrent
* `config` contains qBittorrent configurations

```shell
docker run give.example.org/of/your/container:v0.2.1 parameters
```

#### Network

on the port `8080` the webinterface is run

#### RUN

To start the the docker image simply run

```shell
docker run --env LEGAL=accept -p 8080:8080 -v /your/path/config:/config -v /your/path/download:/downloads --name qBittorrent qbittorrent-nox:4.2.0
```

to stop the container

```shell
docker stop qBittorrent
```
