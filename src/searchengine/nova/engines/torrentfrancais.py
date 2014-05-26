# -*- coding: utf-8 -*-
#VERSION: 1.0
#AUTHOR: Davy39 <davy39@hmamail.com>

# Copyleft

from novaprinter import prettyPrinter
import sgmllib, re, urllib2, os , tempfile, webbrowser

class torrentfrancais(object):
  url = 'http://www.torrentfrancais.com'
  name = 'TorrentFrancais (french)'

  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)

  def download_torrent(self, url):
    #Open default webbrowser if  CloudFlare is blocking direct connection (quite often with VPN & proxy...)
    try:
      f = urllib2.urlopen(urllib2.Request(url, headers={'User-Agent' : "Mozilla/5.0"} ))
    except urllib2.URLError as e:
      webbrowser.open(url, new=2, autoraise=True)
      return
    if response.getcode() == 200:
      dat = f.read()
      file, path = tempfile.mkstemp(".torrent")
      file = os.fdopen(file, "wb")
      file.write(dat)
      file.close()
      print path+" "+url   
    else:
      webbrowser.open(url, new=2, autoraise=True)
      return

  class SimpleSGMLParser(sgmllib.SGMLParser):
    def __init__(self, results, url, *args):
      sgmllib.SGMLParser.__init__(self)
      self.url = url
      self.td_counter = None
      self.current_item = None
      self.results = results

    def start_a(self, attr):
      params = dict(attr)
      if params.has_key('href') and params.has_key('title') and (params.has_key('class') == False) and params['href'].startswith("http://www.torrentfrancais.com/torrent/"):
        self.current_item = {}
        self.td_counter = 0
        self.current_item['desc_link'] = params['href'].strip()
	self.current_item['name'] = params['title'].strip()
	self.current_item['link'] = re.findall('<a href="(http://torrents[^"\'>]*)', urllib2.urlopen(self.current_item['desc_link']).read())[0]

    def start_td(self, attr):
      if isinstance(self.td_counter,int):
	self.td_counter += 1
        if self.td_counter > 7:
          self.td_counter = None
          self.current_item['engine_url'] = self.url
          prettyPrinter(self.current_item)
          self.results.append('a')

    def handle_data(self, data):
        if self.td_counter == 4:
          if not self.current_item.has_key('size'):
            self.current_item['size'] = data.strip()
        if self.td_counter == 6:
          if not self.current_item.has_key('seeds'):
            self.current_item['seeds'] = data.strip()
        if self.td_counter == 7:
          if not self.current_item.has_key('leech'):
            self.current_item['leech'] = data.strip()

  def search(self, what, cat='all'):
    i = 1
    while i<50:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      dat = urllib2.urlopen(self.url+'/torrent/%d/%s.html?orderby=seed&ascdesc=desc'%(i, what)).read()
      parser.feed(dat)
      parser.close()
      if len(results) <= 0:
        break
      i += 1