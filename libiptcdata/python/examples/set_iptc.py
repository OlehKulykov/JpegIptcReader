# sample Python program to set IPTC data in a JPEG file.

# This puts IPTC data in a format suitable for Flickr to get metadata
# about the photo; it is probably wrong for other uses.

import sys
from optparse import OptionParser

import iptcdata

def check_for_dataset(f, rs):
    for ds in f.datasets:
        if (ds.record, ds.tag) == rs:
            return ds
    return None

usage = "set_iptc.py [-c caption] [-t tag,tag,...] [-T title] filename"

parser = OptionParser(usage, version="0.1")

parser.add_option("-c", "--caption", dest="caption", action="store",
                  type="string", help="Caption data for photo")
parser.add_option("-t", "--tags", dest="tags", action="store",
                  type="string", help="Tags")
parser.add_option("-T", "--title", dest="title", action="store",
                  type="string", help="Title for photo")

(options, args) = parser.parse_args()

try:
    f = iptcdata.open(args[0])
except IndexError:
    print usage
    sys.exit(1)

# first do the "title" which flickr wants as a "headline"
if (options.title):
    # we want to replace this record set if it is there
    print "Setting \'Headline\' to %s" % (options.title)
    rs = iptcdata.find_record_by_name("Headline")
    ds = check_for_dataset(f, rs)
    if (ds == None):
        ds = f.add_dataset(rs)
    ds.value = options.title

# a "caption" gives the caption
if (options.caption):
    #also want just one caption
    print "Setting \'Caption\' to %s" % (options.caption)
    rs = iptcdata.find_record_by_name("Caption")
    ds = check_for_dataset(f, rs)
    if (ds == None):
        ds = f.add_dataset(rs)
    ds.value = options.caption

# now just add tags, as keywords
if (options.tags):
    tags = options.tags.split(",")
    rs = iptcdata.find_record_by_name("Keywords")
    for tag in tags:
        print "Setting \'Keyword\' to %s" % (tag)
        ds = f.add_dataset(rs)
        ds.value = tag
        
f.save()
f.close()
