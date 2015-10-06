EXTRA_DIST = main.h Doxyfile nsrlupdate.in
bin_PROGRAMS = nsrlsvr
bin_SCRIPTS = nsrlupdate
nsrlsvr_SOURCES = main.cc handler.cc to_pair64.cc
nsrlsvr_CPPFLAGS = -DPKGDATADIR="\"$(pkgdatadir)\"" -I..

nsrlupdate: nsrlupdate.in
	-$(SED) -e 's,[@]pkgdatadir[@],$(pkgdatadir),g' < nsrlupdate.in > nsrlupdate
	-chmod +x nsrlupdate
