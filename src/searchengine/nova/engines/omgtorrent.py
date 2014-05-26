# -*- coding: utf-8 -*-
#VERSION: 1.0
#AUTHOR: Davy39 <davy39@hmamail.com>

#Copyleft

from novaprinter import prettyPrinter
import sgmllib, urllib2, tempfile, os

class omgtorrent(object):
  url = 'http://www.omgtorrent.com'
  name = 'OMGtorrent (french)'
  supported_categories = {'all': ''}
  
  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)
  
  def download_torrent(self, url):
    file, path = tempfile.mkstemp(".torrent")
    file = os.fdopen(file, "wb")
    dat = urllib2.urlopen(urllib2.Request(url, headers={'User-Agent' : "Mozilla/5.0"} )).read()
    file.write(dat)
    file.close()
    print path+" "+url

  class SimpleSGMLParser(sgmllib.SGMLParser):
    def __init__(self, results, url, *args):
      sgmllib.SGMLParser.__init__(self)
      self.url = url
      self.td_counter = None
      self.current_item = None
      self.results = results
    
    def start_a(self, attr):
      params = dict(attr)
      if params.has_key('href') and params.has_key('class') and params['class'] == 'torrent':
	self.current_item = {}
        self.td_counter = 0
        self.current_item['desc_link']= self.url+params['href'].strip()
        self.current_item['link'] = "http://www.omgtorrent.com/clic_dl.php?groupe=torrents&id="+str(params['href'].strip().split('_')[1].split('.')[0])

    def handle_data(self, data):
      if self.td_counter == 0:  
        if not self.current_item.has_key('name'):
          self.current_item['name'] = data.strip()
      elif self.td_counter == 1:
        if not self.current_item.has_key('size'):
          self.current_item['size'] = data.strip()
      elif self.td_counter == 2:
        if not self.current_item.has_key('seeds'):
          self.current_item['seeds'] = data.strip().replace(',', '')
      elif self.td_counter == 3:
        if not self.current_item.has_key('leech'):
          self.current_item['leech'] = data.strip().replace(',', '')

    def start_td(self,attr):
        if isinstance(self.td_counter,int):
          self.td_counter += 1
          if self.td_counter > 3:
            self.td_counter = None
            self.current_item['engine_url'] = self.url
            prettyPrinter(self.current_item)
            self.results.append('a')

  def search(self, what, cat='all'):
    i = 1
    while i<35:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      dat = urllib2.urlopen(urllib2.Request(self.url+'/recherche/?order=seeders&orderby=desc&query=%s&page=%d'%(what, i), headers={'User-Agent' : "Mozilla/5.0"} )).read()
      parser.feed(dat)
      parser.close()
      if len(results) <= 0:
        break
      i += 1