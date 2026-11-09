# qBittorrent WebUI

[![GitHub Actions CI Status](https://github.com/qbittorrent/qBittorrent/actions/workflows/ci_webui.yaml/badge.svg)](https://github.com/qbittorrent/qBittorrent/actions/workflows/ci_webui.yaml)

## Development Workflow

0. Prerequisite

   * [npm](https://docs.npmjs.com/cli/) installed

1. Setup development environment

   ```shell
   npm install --allow-remote=root
   ```

2. Run static analyzer

   ```shell
   npm run lint
   ```

3. Apply formatting

   ```shell
   npm run format
   ```

## Supported Browser Versions

The upper bound is always the latest stable release.

| Browser        | Lower bound                                             |
| -------------- | ------------------------------------------------------- |
| Chrome         | [The release from 1 year ago][Chrome-lower-bound-link]  |
| Firefox        | [The release from 1 year ago][Firefox-lower-bound-link] |
| Microsoft Edge | [The release from 1 year ago][MSEdge-lower-bound-link]  |
| Safari         | [The release from 1 year ago][Safari-lower-bound-link]  |

[Supported browsers table](https://browsersl.ist/?results#q=Chrome+%3E+0+and+last+1+year%0AEdge+%3E+0+and+last+1+year%0AFirefox+%3E+0+and+last+1+year%0ASafari+%3E+0+and+last+1+year)

[Chrome-lower-bound-link]: https://browsersl.ist/?results#q=Chrome+%3E+0+and+last+1+year
[Firefox-lower-bound-link]: https://browsersl.ist/?results#q=Firefox+%3E+0+and+last+1+year
[MSEdge-lower-bound-link]: https://browsersl.ist/?results#q=Edge+%3E+0+and+last+1+year
[Safari-lower-bound-link]: https://browsersl.ist/?results#q=Safari+%3E+0+and+last+1+year
