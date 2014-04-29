EXTRA_DIST = handler.hpp Doxyfile
bin_PROGRAMS = nsrlsvr
nodist_pkgdata_DATA = NSRLFile.txt
nsrlsvr_SOURCES = main.cc handler.cc to_pair64.cc
nsrlsvr_CPPFLAGS = -DPKGDATADIR="\"$(pkgdatadir)\"" -DPACKAGE_VERSION="\"$(PACKAGE_VERSION)\"" -DPACKAGE_URL="\"$(PACKAGE_URL)\"" -DPACKAGE_BUGREPORT="\"$(PACKAGE_BUGREPORT)\"" $(PTHREAD_CFLAGS)
nsrlsvr_LDFLAGS=$(PTHREAD_CFLAGS)
