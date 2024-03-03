qBittorrent WebUI
---

### Browser compatibility

The upper bound will always be the latest stable release.

| Browser           | Lower bound                                            |
| ----------------- | ------------------------------------------------------ |
| Chrome            | [The release from 1 year ago][Chrome-lower-bound-link] |
| Firefox           | [Latest active ESR release][Firefox-ESR-link]          |
| Microsoft Edge    | [The release from 1 year ago][MSEdge-lower-bound-link] |
| Safari            | [The release from 1 year ago][Safari-lower-bound-link] |

[Supported browsers table](https://browsersl.ist/?results#q=Chrome+%3E+0+and+last+1+year%0AEdge+%3E+0+and+last+1+year%0AFirefox+ESR%0ASafari+%3E+0+and+last+1+year)

[Chrome-lower-bound-link]: https://browsersl.ist/?results#q=Chrome+%3E+0+and+last+1+year
[Firefox-ESR-link]: https://browsersl.ist/?results#q=Firefox+ESR
[MSEdge-lower-bound-link]: https://browsersl.ist/?results#q=Edge+%3E+0+and+last+1+year
[Safari-lower-bound-link]: https://browsersl.ist/?results#q=Safari+%3E+0+and+last+1+year

### Notices

* The [MooTools library][mootools-home] has seen [little maintenance][mootools-code-frequency] and therefore
  its usage is [discouraged][mootools-deprecate]. \
  New code should prefer vanilla JavaScript and resort to MooTools only if necessary. \
  See also: [CVE-2021-20088](https://cve.mitre.org/cgi-bin/cvename.cgi?name=2021-20088)

[mootools-home]: https://mootools.net/
[mootools-code-frequency]: https://github.com/mootools/mootools-core/graphs/code-frequency
[mootools-deprecate]: https://github.com/mootools/mootools-core/issues/2798
