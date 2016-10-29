# nsrlsvr

nsrlsvr is a tool to facilitate looking up data in NIST’s National Software Reference Library Reference Data Set.

## What’s that?
It’s a database of about 50 million MD5 hashes, representing every file known to NIST.

## Why do I care?
If you deal with a lot of unknown files it can be useful to separate them into “stuff NIST already knows about, ergo it’s commonplace” and “stuff NIST doesn’t know about, so maybe it’s interesting”.

You can use a tool like [hashdeep](http://hashdeep.com) to generate MD5 hashes of large filesystems and feed the output into a tool like [nsrllookup](http://rjhansen.github.io/nsrllookup), which will in turn go off and query an nsrlsvr instance to see what’s what.

## Why would I want to run my own?

Great question, especially since nsrllookup comes out-of-the-box ready to work with the freely-accessible nsrllookup.com server.

There are two use cases for standing up your own nsrlsvr instance:

1. You’re doing such high volumes that you’re concerned I’ll block your IP on nsrllookup.com, or
2. You have your own list of MD5 hashes which you want to filter for.

If either of those two describes you, read on!

## What you’ll need

1. A UNIX operating system
2. A C++ compiler that supports the C++14 standard
3. The Boost C++ libraries and development headers
4. Python 3.5 or later
5. A list of unique MD5 hashes, each line consisting of thirty-two ASCII characters terminated by a newline
6. [CMake](http://www.cmake.com)

## How to install

1. Download the [latest official release of nsrlsvr](https://github.com/rjhansen/nsrlsvr/tarball/master)
2. `tar xzf [downloaded-file]` to uncompress it
3. `cd` into the directory you uncompressed it to
4. `which python3` will tell you if you have Python 3 installed on your system, and if so, where
5. `cmake -DPYTHON_EXECUTABLE=/path/to/python3 .` will initialize the build system (don’t forget that trailing period)
6. `make install` will install nsrlsvr to `/usr/local/bin`.
7. Put your hash database in `/usr/local/share/nsrlsvr/hashes.txt`
8. Start nsrlsvr with `nsrlsvr` and you’re off to the races.  Any nsrllookup client can now use you as a hash server.

## How do I make my own dataset from NIST’s minimal RDS?

nsrlsvr comes with a tool called `nsrlupdate`.  You’ll want to use it.

1. Download the latest [NSRL RDS minimal set](http://www.nsrl.nist.gov/Downloads.htm#reduced).  Note: **only** the minimal set is supported.
2. Uncompress it and find a file called “NSRLFile.txt”.
3. `nsrlupdate /path/to/NSRLFile.txt`
4. This may take a long time but you’ll have a complete NSRL RDS hash set when you finish.

Alternately, you can drop your own file of hashes in `/usr/local/share/nsrlsvr/hashes.txt`.  They must be uppercase UTF-8 or ISO8859-1 containing **only** the letters A-F or 0-9.  There must be thirty-two of them terminated by a newline.
