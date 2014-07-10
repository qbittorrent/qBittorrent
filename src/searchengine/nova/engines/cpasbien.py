# -*- coding: utf-8 -*-
#VERSION: 1.0
#AUTHOR: Davy39 <davy39@hmamail.com>

#Copyleft

from novaprinter import prettyPrinter
import urllib2, tempfile, sgmllib, os

class cpasbien(object):
  url = "http://www.cpasbien.pe"
  name = "Cpasbien (french)"
  supported_categories = {"all": "", "books": "ebook/", "movies": "films/", "tv": "series/", "music": "musique/", "software": "logiciels/", "games": ""}
  
  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)
  
  def download_torrent(self, url):
    file, path = tempfile.mkstemp(".torrent")
    file = os.fdopen(file, "wb")
    dat = urllib2.urlopen(urllib2.Request(url, headers={"User-Agent" : "Mozilla/5.0"} )).read()
    file.write(dat)
    file.close()
    print path+" "+url
    
  class SimpleSGMLParser(sgmllib.SGMLParser):
    def __init__(self, results, url, *args):
      sgmllib.SGMLParser.__init__(self)
      self.url = url
      self.data_counter = None
      self.current_item = None
      self.results = results
    
    def start_a(self, attr):
      params = dict(attr)
      if params.has_key("href") and params["href"].startswith("http://www.cpasbien.pe/dl-torrent"):
	self.current_item = {}
        self.data_counter = 0
        self.current_item["desc_link"]=params["href"].strip()
        self.current_item["link"] = "http://www.cpasbien.pe/dl_torrent.php?permalien="+str(params["href"].strip().split("/")[6].split(".")[0])

    def handle_data(self, data):
	if isinstance(self.data_counter,int):
          self.data_counter += 1
          if self.data_counter == 3:
            self.current_item["name"] = data.strip()
          elif self.data_counter == 6:
            self.current_item["size"] = data.strip()
          elif self.data_counter == 9:
            self.current_item["seeds"] = data.strip()
          elif self.data_counter == 11:
            self.current_item["leech"] = data.strip()
            self.current_item["engine_url"] = self.url
            self.data_counter = None
            prettyPrinter(self.current_item)
            self.results.append("a")

  def search(self, what, cat="all"):
    i = 0
    while i<50:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      if cat == "games":
        dat = urllib2.urlopen(urllib2.Request(self.url+"/recherche/jeux-pc/%s/page-%d,trie-seeds-d"%(what, i), headers={"User-Agent" : "Mozilla/5.0"} )).read().replace(" & ", " et ")
        dat += urllib2.urlopen(urllib2.Request(self.url+"/recherche/jeux-consoles/%s/page-%d,trie-seeds-d"%(what, i), headers={"User-Agent" : "Mozilla/5.0"} )).read().replace(" & ", " et ")
      else:
      	dat = urllib2.urlopen(urllib2.Request(self.url+"/recherche/%s%s/page-%d,trie-seeds-d"%(self.supported_categories[cat], what, i), headers={"User-Agent" : "Mozilla/5.0"} )).read().replace(" & ", " et ")
      parser.feed(dat)
      parser.close()
      if len(results) <= 0:
        break
      i += 1