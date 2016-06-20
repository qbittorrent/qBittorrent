#VERSION: 2.0
#AUTHORS: Douman (custparasite@gmx.se)
#CONTRIBUTORS: Diego de las Heras (ngosang@hotmail.es)

from novaprinter import prettyPrinter
from helpers import retrieve_url, download_file
from re import compile as re_compile
from html.parser import HTMLParser

class torlock(object):
    url = "https://www.torlock.com"
    name = "TorLock"
    supported_categories = {'all'      : 'all',
                            'anime'    : 'anime',
                            'software' : 'software',
                            'games'    : 'game',
                            'movies'   : 'movie',
                            'music'    : 'music',
                            'tv'       : 'television',
                            'books'    : 'ebooks'}

    def download_torrent(self, info):
        print(download_file(info))

    class MyHtmlParser(HTMLParser):
        """ Sub-class for parsing results """
        def __init__(self, url):
            HTMLParser.__init__(self)
            self.url = url
            self.article_found = False #true when <article> with results is found
            self.item_found = False
            self.item_bad = False #set to True for malicious links
            self.current_item = None #dict for found item
            self.item_name = None #key's name in current_item dict
            self.parser_class = {"ts"  : "size",
                                 "tul" : "seeds",
                                 "tdl" : "leech"}

        def handle_starttag(self, tag, attrs):
            params = dict(attrs)
            if self.item_found:
                if tag == "td":
                    if "class" in params:
                        self.item_name = self.parser_class.get(params["class"], None)
                        if self.item_name:
                            self.current_item[self.item_name] = ""

            elif self.article_found and tag == "a":
                if "href" in params:
                    link = params["href"]
                    if link.startswith("/torrent"):
                        self.current_item["desc_link"] = "".join((self.url, link))
                        self.current_item["link"] = "".join((self.url, "/tor/", link.split('/')[2], ".torrent"))
                        self.current_item["engine_url"] = self.url
                        self.item_found = True
                        self.item_name = "name"
                        self.current_item["name"] = ""
                        self.item_bad = "rel" in params and params["rel"] == "nofollow"

            elif tag == "article":
                self.article_found = True
                self.current_item = {}

        def handle_data(self, data):
            if self.item_name:
                self.current_item[self.item_name] += data

        def handle_endtag(self, tag):
            if tag == "article":
                self.article_found = False
            elif self.item_name and (tag == "a" or tag == "td"):
                self.item_name = None
            elif self.item_found and tag == "tr":
                self.item_found = False
                if not self.item_bad:
                    prettyPrinter(self.current_item)
                self.current_item = {}

    def search(self, query, cat='all'):
        """ Performs search """
        query = query.replace("%20", "-")

        parser = self.MyHtmlParser(self.url)
        page = "".join((self.url, "/", self.supported_categories[cat], "/torrents/", query, ".html?sort=seeds&page=1"))
        html = retrieve_url(page)
        parser.feed(html)

        counter = 1
        additional_pages = re_compile("/{0}/torrents/{1}.html\?sort=seeds&page=[0-9]+".format(self.supported_categories[cat], query))
        list_searches = additional_pages.findall(html)[:-1] #last link is next(i.e. second)
        for page in map(lambda link: "".join((self.url, link)), list_searches):
            html = retrieve_url(page)
            parser.feed(html)
            counter += 1
            if counter > 3:
                break
        parser.close()
