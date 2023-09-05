qBittorent man pages
===

Our man pages are written in Markdown format (.md) and we use [Pandoc](https://pandoc.org/) to
convert them to roff (the man page format).
Refer to [Pandoc's Markdown](https://pandoc.org/MANUAL.html#pandocs-markdown) for info
on that specific format.

To build the man pages:
```shell
pandoc -s -f markdown -t man qbittorrent.1.md -o qbittorrent.1
pandoc -s -f markdown -t man qbittorrent-nox.1.md -o qbittorrent-nox.1
```

There is also an online converter you can use if you have trouble installing a
local one: [link](https://pandoc.org/try/?params=%7B%22to%22%3A%22man%22%2C%22from%22%3A%22markdown%22%2C%22standalone%22%3Atrue%7D) \
You'll need to be careful when you copy the output to file as some headers will be missing.
Careful not to overwrite the existing leading headers or trim off the trailing headers.

Remember to commit Markdown files (\*.md) and the generated man pages (\*.1 files) after editing!

### Translation

To translate the man pages into a new language, create a new subdirectory here
with the translated files and add it to the `manPageLanguages` list in
[`dist/unix/CMakeLists.txt`](../dist/unix/CMakeLists.txt).
