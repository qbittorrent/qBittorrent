#VERSION: 1.00
#AUTHORS: Fabien Devaux (fab@gnux.info)
from novaprinter import prettyPrinter
import urllib
import re

# TODO: add multipage
class btjunkie(object):
	url = 'http://btjunkie.org'
	name = 'btjunkie'

	def search(self, what):
		dat = urllib.urlopen(self.url+'/search?q=%s'%what).read().decode('utf8', 'replace')
		# I know it's not very readable, but the SGML parser feels in pain
		section_re = re.compile('(?s)href="/torrent\?do=download.*?<tr>')
		torrent_re = re.compile('(?s)href="(?P<link>.*?do=download[^"]+).*?'
		'class="BlckUnd">(?P<name>.*?)</a>.*?'
		'>(?P<size>\d+MB)</font>.*?'
		'>(?P<seeds>\d+)</font>.*?'
		'>(?P<leech>\d+)</font>')
		for match in section_re.finditer(dat):
			txt = match.group(0)
			m = torrent_re.search(txt)
			if m:
				torrent_infos = m.groupdict()
				torrent_infos['name'] = re.sub('</?font.*?>', '', torrent_infos['name'])
				torrent_infos['engine_url'] = self.url
				torrent_infos['link'] = self.url+torrent_infos['link']
				prettyPrinter(torrent_infos)