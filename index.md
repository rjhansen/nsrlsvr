# nsrlsvr
The latest stable version is [1.7.0](https://github.com/rjhansen/nsrlsvr/archive/1.7.0.tar.gz).

## What’s nsrlsvr?
The National Institute of Standards and Technology (NIST) maintains the National Software Reference Library (NSRL) — a giant compendium of software contributed by vendors.  It’s not a library in the sense that you can check things out from it, though: it’s more a library that you can check to see whether a given file already exists.  All the system files from Windows 7 are in the NSRL, as is the latest releases of Firefox and Opera and Chrome, Winamp and…

It's large, really large: over forty million distinct hashes.

## What’s the RDS?
The Reference Data Set (RDS) is a list of hashes for all of the files maintained within the NSRL.  It’s over forty million hashes, each one corresponding to a known piece of software.  This isn’t to say everything listed is known good, known safe, or anything like that — just that it’s known.

## Why is the RDS important?
Forensic investigators, first responders and technical support staff often have a needle-in-a-haystack problem: of all the files on a given storage medium they are probably only interested in a handful.  A good way to begin is by finding out what things may be ignored.  The odds are excellent that a file present in the RDS is of no real interest to the investigator.

## What does `nsrlsvr` do?
It keeps track of 40 million hash values in an in-memory dataset and allows users to query that set at extremely high volume.  This allows an investigator using an NSRL tool (such as `nsrllookup`) to winnow through large numbers of files in a very short period of time.

## Who wrote it?
I did — Rob Hansen, or rjhansen on GitHub.  Feel free to [email me](mailto:rjh@sixdemonbag.org?subject=nsrlsvr).

## Build instructions

You will need:

* The [source code](https://github.com/rjhansen/nsrlsvr/archive/1.7.0.tar.gz)
* A _good_ C++14 compiler.  GCC 5.0 will work, barely.  On the latest GCC and Clang it hums nicely.
* [CMake](https://www.cmake.org) 3.4 or later
* [Boost](https://boost.org) 1.66 or later
* A copy of the [minimal NSRL RDS](https://s3.amazonaws.com/rds.nsrl.nist.gov/RDS/current/rds_modernm.zip)

Once you've uncompressed the latest archive, go into that directory and:

```
cmake -DPYTHON_EXECUTABLE=`which python3` -DCMAKE_BUILD_TYPE=Release .
make
sudo make install
```

You will need a database of hashes to load into `nsrlsvr`.  Extract the file `NSRLFile.txt` from `rds_modernm.zip` and run `nsrlupdate`:

```
sudo nsrlupdate /path/to/NSRLFile.txt
```

Once that's done you should be able to type

```
nsrlsvr
```

and have it start up.
