#VERSION: 1.1
#AUTHORS: Douman (custparasite@gmx.se)
#CONTRIBUTORS: Diego de las Heras (diegodelasheras@gmail.com)

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of the author nor the names of its contributors may be
#      used to endorse or promote products derived from this software without
#      specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

from html.parser import HTMLParser
from http.client import HTTPSConnection as https
from re import compile as re_compile
from re import DOTALL
from itertools import islice
#qBt
from novaprinter import prettyPrinter
from helpers import download_file

class demonoid(object):
    """ Search engine class """
    url = "https://www.demonoid.pw"
    name = "Demonoid"
    supported_categories = {'all': '0',
                            'music': '2',
                            'movies': '1',
                            'games': '4',
                            'software': '5',
                            'books': '11',
                            'anime': '9',
                            'tv': '3'}

    def download_torrent(self, info):
        """ Downloader """
        print(download_file(info))

    class MyHtmlParseWithBlackJack(HTMLParser):
        """ Parser class """
        def __init__(self, url):
            HTMLParser.__init__(self)
            self.url = url
            self.current_item = None
            self.save_data = None
            self.seeds_leech = False
            self.size_repl = re_compile(",")

        def handle_starttag(self, tag, attrs):
            """ Parser's start tag handler """
            if tag == "a":
                params = dict(attrs)
                if "href" in params:
                    link = params["href"]
                    if link.startswith("/files/details"):
                        self.current_item = dict()
                        self.current_item["desc_link"] = "".join((self.url, link))
                        self.current_item["engine_url"] = self.url
                        self.save_data = "name"
                    elif link.startswith("/files/download"):
                        self.current_item["link"] = "".join((self.url, link))

            elif self.current_item:
                if tag == "td":
                    params = dict(attrs)
                    if "class" in params and "align" in params:
                        if params["class"].startswith("tone"):
                            if params["align"] == "right":
                                self.save_data = "size"
                            elif params["align"] == "center":
                                self.seeds_leech = True

                elif self.seeds_leech and tag == "font":
                    for attr in attrs:
                        if "class" in attr:
                            if attr[1] == "green":
                                self.save_data = "seeds"
                            elif attr[1] == "red":
                                self.save_data = "leech"

                    self.seeds_leech = False


        def handle_data(self, data):
            """ Parser's data handler """
            if self.save_data:
                if self.save_data == "name":
                    # names with special characters like '&' are splitted in several pieces
                    if 'name' not in self.current_item:
                        self.current_item['name'] = ''
                    self.current_item['name'] += data
                else:
                    self.current_item[self.save_data] = data
                    self.save_data = None
                if self.current_item.__len__() == 7:
                    self.current_item["size"] = self.size_repl.sub("", self.current_item["size"])
                    prettyPrinter(self.current_item)
                    self.current_item = None

        def handle_endtag(self, tag):
            """ Parser's end tag handler """
            if self.save_data == "name":
                self.save_data = None

    def search(self, what, cat='all'):
        """ Performs search """
        connection = https("www.demonoid.pw")

        #prepare query
        cat = self.supported_categories[cat.lower()]
        query = "".join(("/files/?category=", cat, "&subcategory=All&quality=All&seeded=2&external=2&query=", what, "&to=1&uid=0&sort=S"))

        connection.request("GET", query)
        response = connection.getresponse()
        if response.status != 200:
            return

        data = response.read().decode("utf-8")
        add_res_list = re_compile("/files.*page=[0-9]+")
        torrent_list = re_compile("start torrent list -->(.*)<!-- end torrent", DOTALL)
        data = torrent_list.search(data).group(0)
        list_results = add_res_list.findall(data)

        parser = self.MyHtmlParseWithBlackJack(self.url)
        parser.feed(data)

        del data

        if list_results:
            for search_query in islice((add_res_list.search(result).group(0) for result in list_results[1].split(" | ")), 0, 5):
                connection.request("GET", search_query)
                response = connection.getresponse()
                parser.feed(torrent_list.search(response.read().decode('utf-8')).group(0))
                parser.close()

        connection.close()
        return
