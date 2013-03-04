#!/usr/bin/env python

from __future__ import print_function

import re, sys, os

hash_re = re.compile(r"([0-9A-Fa-f]{64}|[0-9A-Fa-f]{40}|[0-9A-Fa-f]{32})")

if len(sys.argv) != 2:
    print("No file specified.")
    exit(-1)
if not os.access(sys.argv[1], os.R_OK):
    print("Couldn't read " + sys.argv[1])
    exit(-2)
    
with open(sys.argv[1]) as fh:
    hashes = [hash_re.search(X).group(1) for X in fh.readlines() if hash_re.search(X)]

if not hashes:
    print("Zero hashes found -- check to see if this is correct.")
    exit(-4)

first_len = len(hashes[0])
if [X for X in hashes[1:] if len(X) != first_len]:
    print("Multiple different hash algorithms present in " + sys.argv[1])
    exit(-8)
    
with open("src/NSRLFile.txt", "w") as output:
    for hash in hashes:
        output.write(hash.upper() + "\n")
