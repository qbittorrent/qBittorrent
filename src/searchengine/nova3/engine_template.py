#!/usr/bin/env python3

#VERSION: 1.00
#Author: Douman (custparasite@gmx.se)

from sys import argv

def generate_header():
    """ Generator of engine's header """
    yield "#VERSION: 1.00\n"
    yield "#AUTHORS: YOUR_NAME (YOUR_EMAIL)\n\n"
    yield "# Redistribution and use in source and binary forms, with or without\n"
    yield "# modification, are permitted provided that the following conditions are met:\n"
    yield "#\n"
    yield "#    * Redistributions of source code must retain the above copyright notice,\n"
    yield "#      this list of conditions and the following disclaimer.\n"
    yield "#    * Redistributions in binary form must reproduce the above copyright\n"
    yield "#      notice, this list of conditions and the following disclaimer in the\n"
    yield "#      documentation and/or other materials provided with the distribution.\n"
    yield "#    * Neither the name of the author nor the names of its contributors may be\n"
    yield "#      used to endorse or promote products derived from this software without\n"
    yield "#      specific prior written permission.\n"
    yield "#\n"
    yield "# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"
    yield "# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
    yield "# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
    yield "# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n"
    yield "# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n"
    yield "# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n"
    yield "# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n"
    yield "# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n"
    yield "# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n"
    yield "# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n"
    yield "# POSSIBILITY OF SUCH DAMAGE.\n\n"

def generate_imports():
    """ Generates default imports """
    yield "from html.parser import HTMLParser\n"
    yield "#qBt\n"
    yield "from novaprinter import prettyPrinter\n"
    yield "from helpers import download_file\n\n"

def generate_body(class_name):
    """ Generates engine class template """
    tab = " " * 4
    yield "".join(("class ", class_name, "(object):\n"))
    yield tab + "url = 'http://www.engine-url.org'\n"
    yield tab + "name = 'Full engine name' # spaces and special characters are allowed here\n"
    yield tab + "# Which search categories are supported by this search engine and their corresponding id\n"
    yield tab + "# Possible categories are ('all', 'movies', 'tv', 'music', 'games', 'anime', 'software', 'pictures', 'books')\n"
    yield tab + "supported_categories = {'all': '0', 'movies': '6', 'tv': '4', 'music': '1', 'games': '2', 'anime': '7', 'software': '3'}\n"
    yield "\n"
    yield tab + "def __init__(self):\n"
    yield "".join((tab, tab, "# some initialization\n"))
    yield "".join((tab, tab, "pass\n"))
    yield "\n"
    yield tab + "def download_torrent(self, info):\n"
    yield "".join((tab, tab, "# Providing this function is optional. It can however be interesting to provide\n"))
    yield "".join((tab, tab, "# your own torrent download implementation in case the search engine in question\n"))
    yield "".join((tab, tab, "# does not allow traditional downloads (for example, cookie-based download).\n"))
    yield "".join((tab, tab, "print(download_file(info))\n"))
    yield "\n"
    yield "".join((tab, "class MyHtmlParser(HTMLParser):\n"))
    yield "".join((tab, tab, "\"\"\" Parser \"\"\"\n"))
    yield "".join((tab, tab, "def __init__(self):\n"))
    yield "".join((tab, tab, tab, "HTMLParser.__init__(self)\n\n"))
    yield "".join((tab, tab, "def handle_starttag(self, tag, attrs):\n"))
    yield "".join((tab, tab, tab, "\"\"\" parse html's start tag \"\"\"\n"))
    yield "".join((tab, tab, tab, "pass\n\n"))
    yield "".join((tab, tab, "def handle_endtag(self, tag):\n"))
    yield "".join((tab, tab, tab, "\"\"\" parse html's end tag \"\"\"\n"))
    yield "".join((tab, tab, tab, "pass\n\n"))
    yield "".join((tab, tab, "def handle_data(self, data):\n"))
    yield "".join((tab, tab, tab, "\"\"\" parse data in <start_tag>data</end_tag>\"\"\"\n"))
    yield "".join((tab, tab, tab, "pass\n\n"))
    yield tab + "# DO NOT CHANGE the name and parameters of this function\n"
    yield tab + "# This function will be the one called by nova2.py\n"
    yield tab + "def search(self, what, cat='all'):\n"
    yield "".join((tab, tab, "# what is a string with the search tokens, already escaped (e.g. \"Ubuntu+Linux\")\n"))
    yield "".join((tab, tab, "# cat is the name of a search category in ('all', 'movies', 'tv', 'music', 'games', 'anime', 'software', 'pictures', 'books')\n"))
    yield "".join((tab, tab, "#\n"))
    yield "".join((tab, tab, "# Here you can do what you want to get the result from the\n"))
    yield "".join((tab, tab, "# search engine website.\n"))
    yield "".join((tab, tab, "# everytime you parse a result line, store it in a dictionary\n"))
    yield "".join((tab, tab, "# and call the prettyPrint(your_dict) function\n"))
    yield "".join((tab, tab, "data = ''\n\n"))
    yield "".join((tab, tab, "parser = self.MyHtmlParser()\n"))
    yield "".join((tab, tab, "parser.feed(data)\n"))
    yield "".join((tab, tab, "parser.close()\n"))

def main(args):
    if not args:
        print("Usage: engine_template [file_name]")
        return

    file_name = args[0].lower() if args[0].endswith(".py") else ".".join((args[0].lower(), "py"))
    with open(file_name, "w") as fd_temp:
        fd_temp.write("".join(generate_header()))
        fd_temp.write("".join(generate_imports()))
        fd_temp.write("".join(generate_body(file_name.split(".")[0])))

if __name__ == "__main__":
    main(argv[1:])
