#!/usr/bin/env python
#coding=UTF-8

import re, sys

md5_re = re.compile('^.*"([0-9A-Fa-f]{32})".*$')
hashes = []
count = 0

with open("NSRLFile.txt") as fh:
    line = fh.readline()
    while line:
        elements = line.split(",")
        if len(elements) >= 2:
            match = md5_re.match(elements[1])
            if match:
                hashes.append(match.group(1))
        line = fh.readline()

hashes.sort()
with open("src/NSRLFile.txt", "w") as fh:
    for entry in hashes:
        fh.write(entry + "\n")
