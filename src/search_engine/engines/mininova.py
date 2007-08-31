#VERSION: 1.00
#AUTHORS: Fabien Devaux (fab@gnux.info)
from novaprinter import prettyPrinter
import urllib
from xml.dom import minidom
import re

class mininova(object):
	url = 'http://www.mininova.org'
	name = 'Mininova'
	table_items = 'added cat name size seeds leech'.split()

	def search(self, what):
		order = 'seeds' # must be one in self.table_items

		def get_link(lnk):
                        lnks = lnk.getElementsByTagName('a')
                        if lnks.item(0).attributes.get('href').value.startswith('/faq'):
                          if len(lnks) > 1:
                            return self.url+lnks.item(1).attributes.get('href').value
                        else:
			 return self.url+lnks.item(0).attributes.get('href').value

		def get_text(txt):
			if txt.nodeType == txt.TEXT_NODE:
				return txt.toxml()
			else:
				return ''.join([ get_text(n) for n in txt.childNodes])
		dat = urllib.urlopen(self.url+'/search/%s/seeds'%(what,)).read().decode('utf-8', 'replace')
		dat = re.sub("<a href=\"http://www.boardreader.com/index.php.*\"", "<a href=\"plop\"", dat)
                dat = re.sub("<=", "&lt;=", dat)
		x = minidom.parseString(dat.encode('utf-8', 'replace'))
		table = x.getElementsByTagName('table').item(0)
		if not table: return
		for tr in table.getElementsByTagName('tr'):
			tds = tr.getElementsByTagName('td')
			if tds:
				i = 0
				vals = {}
				for td in tds:
					if self.table_items[i] == 'name':
						vals['link'] = get_link(td).strip()
					vals[self.table_items[i]] = get_text(td).strip()
					i += 1
				vals['engine_url'] = self.url
				if not vals['seeds'].isdigit():
					vals['seeds'] = 0
				if not vals['leech'].isdigit():
					vals['leech'] = 0
				prettyPrinter(vals)
