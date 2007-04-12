#!/usr/bin/python

import os

LANG_FOLDER = "lang/"
PO_FOLDER = 'po_files'

if __name__ == '__main__':
  po_files = os.listdir(os.path.join(LANG_FOLDER, PO_FOLDER))
  po_files = [x for x in po_files if os.path.splitext(x)[-1] == ".po"]
  for file in po_files:
    # First clean up the ts file
    ts_file = os.path.join(LANG_FOLDER, os.path.splitext(file)[0]+'.ts')
    po_file = os.path.join(LANG_FOLDER, PO_FOLDER, file)
    if os.path.exists(ts_file):
      os.remove(ts_file)
    # Convert to PO
    print "Converting %s..." % (po_file,)
    os.system("po2ts %s -o %s" % (po_file, ts_file))
