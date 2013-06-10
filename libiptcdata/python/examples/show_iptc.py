#small example program to print out IPTC data from a file

import sys

import iptcdata

try:
    f = iptcdata.open(sys.argv[1])
except IndexError:
    print "show_iptc.py filename"

if (len(f.datasets) == 0):
    print "No IPTC data!"
    sys.exit(0)

for ds in f.datasets:
    print "Tag: %d\nRecord: %d" % (ds.record, ds.tag)
    print "Title: %s" % (ds.title)
    descr = iptcdata.get_tag_description(record=ds.record, tag=ds.tag)
    print "Description: %s" % (descr)
    print "Value: %s" % (ds.value)
    print

f.close()
