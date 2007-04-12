#!/usr/bin/python

import os

LANG_FOLDER = 'lang/'
PO_FOLDER = 'po_files'

if __name__ == '__main__':
  ts_files = os.listdir(LANG_FOLDER)
  ts_files = [x for x in ts_files if os.path.splitext(x)[-1] == ".ts"]
  for file in ts_files:
    # First clean up the po file
    po_file = os.path.join(LANG_FOLDER, PO_FOLDER, os.path.splitext(file)[0]+'.po')
    ts_file = os.path.join(LANG_FOLDER, file)
    if os.path.exists(po_file):
      os.remove(po_file)
    # create po folder if it doesn't exist
    if not os.path.exists(os.path.join(LANG_FOLDER, PO_FOLDER)):
      os.mkdir(os.path.join(LANG_FOLDER, PO_FOLDER))
    # Convert to PO
    print "Converting %s..." % (ts_file,)
    os.system("ts2po %s -o %s" % (ts_file, po_file))
    # Making an archive
    os.system("tar czf %s %s" % (os.path.join(LANG_FOLDER, PO_FOLDER)+'.tar.gz', os.path.join(LANG_FOLDER, PO_FOLDER)))
