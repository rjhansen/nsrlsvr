EXTRA_DIST = handler.hpp Doxyfile nsrlupdate.in nsrlappend.in
bin_PROGRAMS = nsrlsvr
bin_SCRIPTS = nsrlupdate nsrlappend
nodist_pkgdata_DATA = NSRLFile.txt
nsrlsvr_SOURCES = main.cc handler.cc to_pair64.cc
nsrlsvr_CPPFLAGS = -DPKGDATADIR="\"$(pkgdatadir)\"" -DPACKAGE_VERSION="\"$(PACKAGE_VERSION)\"" -DPACKAGE_URL="\"$(PACKAGE_URL)\"" -DPACKAGE_BUGREPORT="\"$(PACKAGE_BUGREPORT)\"" $(PTHREAD_CFLAGS)
nsrlsvr_LDFLAGS=$(PTHREAD_CFLAGS)
CLEANFILES=nsrlupdate nsrlappend

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-rm -rf *~

nsrlupdate: nsrlupdate.in
	-$(SED) -e 's,[@]pkgdatadir[@],$(pkgdatadir),g' < nsrlupdate.in > nsrlupdate
	-chmod +x nsrlupdate

nsrlappend: nsrlappend.in
	-$(SED) -e 's,[@]pkgdatadir[@],$(pkgdatadir),g' < nsrlappend.in > nsrlappend
	-chmod +x nsrlappend
