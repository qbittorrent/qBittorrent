#VERSION: 1.2
#AUTHORS: Fabien Devaux (fab@gnux.info)

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

from novaprinter import prettyPrinter
from helpers import retrieve_url
from xml.dom import minidom
import re

class mininova(object):
	url = 'http://www.mininova.org'
	name = 'Mininova'
	table_items = 'added cat name size seeds leech'.split()

	def search(self, what):

		def get_link(lnk):
			lnks = lnk.getElementsByTagName('a')
			i = 0
			try:
				while not lnks.item(i).attributes.get('href').value.startswith('/get'):
					i += 1
			except:
				return None
			return (self.url+lnks.item(i).attributes.get('href').value).strip()
		
		def get_name(lnk):
			lnks = lnk.getElementsByTagName('a')
			i = 0
			try:
				while not lnks.item(i).attributes.get('href').value.startswith('/tor'):
					i += 1
			except:
				return None
			return lnks.item(i).firstChild.toxml()

		def get_text(txt):
			if txt.nodeType == txt.TEXT_NODE:
				return txt.toxml()
			else:
				return ''.join([ get_text(n) for n in txt.childNodes])
		page = 1
		while True and page<11:
			file = open('/home/chris/mytest.txt', 'w')
			file.write(self.url+'/search/%s/seeds/%d'%(what, page))
			file.close()
			res = 0
			dat = retrieve_url(self.url+'/search/%s/seeds/%d'%(what, page))
			dat = re.sub("<a href=\"http://www.boardreader.com/index.php.*\"", "<a href=\"plop\"", dat)
			dat = re.sub("<=", "&lt;=", dat)
			dat = re.sub("&\s", "&amp; ", dat)
			x = minidom.parseString(dat)
			table = x.getElementsByTagName('table').item(0)
			if not table: return
			for tr in table.getElementsByTagName('tr'):
				tds = tr.getElementsByTagName('td')
				if tds:
					i = 0
					vals = {}
					for td in tds:
						if self.table_items[i] == 'name':
							vals['link'] = get_link(td)
							vals['name'] = get_name(td)
						else:
							vals[self.table_items[i]] = get_text(td).strip()
						i += 1
					vals['engine_url'] = self.url
					if not vals['seeds'].isdigit():
						vals['seeds'] = 0
					if not vals['leech'].isdigit():
						vals['leech'] = 0
					if vals['link'] is None:
						continue
					prettyPrinter(vals)
					res = res + 1
			if res == 0:
				break
			page = page +1
