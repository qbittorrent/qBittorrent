# -*- coding: utf-8 -*-
#VERSION: 1.0
#AUTHOR: Davy39 <davy39@hmamail.com>

#Copyleft

from novaprinter import prettyPrinter
import StringIO, gzip, urllib2, tempfile, sgmllib, re, os

class smartorrent(object):
  url = 'http://www.smartorrent.com'
  name = 'Smartorrent (french)'
  supported_categories = {'all': ['0'],
                          'movies': ["57","49","26","1","37","11","29","39","41","2","18","38","17","25"], 
                          'music': ["54","3"], 
                          'tv': ["33","43","40"], 
                          'anime': ["19","44"], 
                          'games': ["13","20","4","42","14","22","23","28"], 
                          'books': ["5"], 
                          'software': ["12","21","46"] 
                         }
  
  def __init__(self):
    self.results = []
    self.parser = self.SimpleSGMLParser(self.results, self.url)
  
  def download_torrent(self, url):
    opener = urllib2.build_opener(urllib2.BaseHandler())
    file, path = tempfile.mkstemp(".torrent")
    file = os.fdopen(file, "wb")
    dat = opener.open(url).read()
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
      if params.has_key('href') and params['href'].startswith("http://smartorrent.com/torrent/Torrent-"):
	self.current_item = {}
        self.td_counter = 0
        self.current_item['desc_link']=params['href'].strip()
        self.current_item['link'] = "http://smartorrent.com/?page=download&tid="+str(params['href'].strip().split('/')[5])

    def handle_data(self, data):
      if self.td_counter == 0:
        if not self.current_item.has_key('name'):
          self.current_item['name'] = data.strip()
      elif self.td_counter == 2:
        if not self.current_item.has_key('size'):
          self.current_item['size'] = data.strip()
      elif self.td_counter == 3:
        if not self.current_item.has_key('seeds'):
          self.current_item['seeds'] = data.strip()
      elif self.td_counter == 4:
        if not self.current_item.has_key('leech'):
          self.current_item['leech'] = data.strip()

    def start_td(self,attr):
        if isinstance(self.td_counter,int):
          self.td_counter += 1
          if self.td_counter > 4:
            self.td_counter = None
            if self.current_item:
              self.current_item['engine_url'] = self.url
              prettyPrinter(self.current_item)
              self.results.append('a')

  def search(self, what, cat='all'):
    opener = urllib2.build_opener(urllib2.BaseHandler())
    ret = []
    i = 1
    while i<35:
      results = []
      parser = self.SimpleSGMLParser(results, self.url)
      dat = ''
      for subcat in self.supported_categories[cat]:
        #f = opener.open(self.url+'/?page=search&term=%s&voir=%d&ordre=sd'%(what, i))
        dat += opener.open(self.url+'/?page=search&term=%s&cat=%s&voir=%d&ordre=sd'%(what, subcat, i)).read().decode('iso-8859-1', 'replace').replace('<b><font color="#474747">', '').replace('</font></b>', '')
      parser.feed(dat)
      parser.close()
      if len(results) <= 0:
        break
      i += 1