#VERSION: 1.02
#AUTHORS: Gekko Dam Beer (gekko04@users.sourceforge.net)
from novaprinter import prettyPrinter
import sgmllib
import urllib

class torrentreactor(object):
	url = 'http://www.torrentreactor.net'
	name = 'TorrentReactor.Net'

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.id = None
			self.url = url

		def start_a(self, attr):
			params = dict(attr)
			if params['href'].startswith('http://dl.torrentreactor.net/download.php'):
				self.current_item = {}
				self.td_counter = 0
				self.current_item['link'] = params['href'].strip()

		def handle_data(self, data):
			if self.td_counter == 0:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 1:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			elif self.td_counter == 2:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 3:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 3:
					self.td_counter = None
					# add item to results
					if self.current_item:
						self.current_item['engine_url'] = self.url
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						self.has_results = True
						self.results.append('a')

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	def search(self, what):
		i = 0
		while True and i<11:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/search.php?search=&words=%s&cid=&sid=&type=2&orderby=a.seeds&asc=0&skip=%s'%(what,(i*35))).read().decode('utf-8', 'replace')
			#print "loading page: "+self.url+'/search.php?search=&words=%s&cid=&sid=&type=2&orderby=a.seeds&asc=0&skip=%s'%(what,(i*35))
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1
