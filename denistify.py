#!/usr/bin/env python
#coding=UTF-8

from __future__ import unicode_literals
import re, os, codecs, sys

md5_re = re.compile(r'^[A-Fa-f0-9]{32}$')
count = 0

sys.stdout.write("\n")

try:
    os.unlink("dummyfile")
except:
    pass

with codecs.open("NSRLFile.txt", encoding="UTF-8", errors="replace") as infile:
    with open("dummyfile", "w") as outfile:
        line = infile.readline()[44:76]
        sys.stdout.write("Hashes processed: 0")
        while line:
            match = md5_re.match(line)
            if match:
                outfile.write(line + "\n")
                if count % 1000 == 0:
                    sys.stdout.write("\rHashes processed: " + str(count))
                count += 1
            line = infile.readline()[44:76]

sys.stdout.write("\rHashes processed: " + str(count) + "\n")
