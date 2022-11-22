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
2. A Rust development environment (1.65 or later, please)
3. The GNU Autotools

## How to install

1. Download the latest development release of `nsrlsvr`
2. `tar xzf [downloaded-file]` to uncompress it
3. `cd` into the directory you uncompressed it to
4. `autoreconf && automake --copy --add-missing` to initialize the build system
5. `./configure && make` to build nsrlsvr
6. `sudo make install` to install it to `/usr/local/bin`.
7. Build your dataset (see below)
8. Start nsrlsvr with `nsrlsvr` and you’re off to the races.  Any nsrllookup client can now use you as a hash server.

## How do I make my own dataset from NIST’s minimal RDS?
