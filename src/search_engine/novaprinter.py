def prettyPrinter(dictionnary):
	dictionnary['size'] = anySizeToBytes(dictionnary['size'])
	print "%(link)s|%(name)s|%(size)s|%(seeds)s|%(leech)s|%(engine_url)s" % dictionnary

def anySizeToBytes(size_string):
	"""
	Convert a string like '1 KB' to '1024' (bytes)
	"""
	# separate integer from unit
	try:
		size, unit = size_string.split()
	except (ValueError, TypeError):
		try:
			size = size_string.strip()
			unit = ''.join([c for c in size if c.isalpha()])
			size = size[:-len(unit)]
		except(ValueError, TypeError):
			return -1

	size = float(size)
	short_unit = unit.upper()[0]

	# convert
	units_dict = { 'T': 40, 'G': 30, 'M': 20, 'K': 10 }
	if units_dict.has_key( short_unit ):
		size = size * 2**units_dict[short_unit]
	return int(size)