#VERSION: 1.00
#AUTHORS: Fabien Devaux (fab@gnux.info)
from novaprinter import prettyPrinter
import sgmllib
import urllib

class piratebay(object):
	url = 'http://thepiratebay.org'
	name = 'The Pirate Bay'

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.url = url
			self.code = 0

		def start_a(self, attr):
			params = dict(attr)
			if params['href'].startswith('/browse'):
				self.current_item = {}
				self.td_counter = 0
                        elif params['href'].startswith('/tor'):
				self.code = params['href'].split('/')[2]
			elif params['href'].startswith('http://torrents.thepiratebay.org/%s'%self.code):
				self.current_item['link']=params['href'].strip()
				self.td_counter = self.td_counter+1

		def handle_data(self, data):
			if self.td_counter == 1:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 5:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			elif self.td_counter == 6:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 7:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 7:
					self.td_counter = None
					# Display item
					if self.current_item:
						self.current_item['engine_url'] = self.url
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						self.results.append('a')
	def search(self, what):
		ret = []
		i = 0
		order = 'se'
		while True:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/search/%s/%u/0/0' % (what, i)).read()
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1