EXTRA_DIST = main.h Doxyfile nsrlupdate.in literate.nw
bin_PROGRAMS = nsrlsvr
bin_SCRIPTS = nsrlupdate
nsrlsvr_SOURCES = main.cc handler.cc to_pair64.cc
nsrlsvr_CPPFLAGS = -DPKGDATADIR="\"$(pkgdatadir)\"" -DPACKAGE_VERSION="\"$(PACKAGE_VERSION)\"" -DPACKAGE_URL="\"$(PACKAGE_URL)\"" -DPACKAGE_BUGREPORT="\"$(PACKAGE_BUGREPORT)\""
CLEANFILES=nsrlupdate nsrlappend

clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	-rm -rf *~ *.aux *.log *.toc *.pdf *.tex

nsrlupdate: nsrlupdate.in
	-$(SED) -e 's,[@]pkgdatadir[@],$(pkgdatadir),g' < nsrlupdate.in > nsrlupdate
	-chmod +x nsrlupdate

literate: literate.nw
	noweave -delay literate.nw > literate.tex
	notangle -Rmain.cc literate.nw > main.cc
	notangle -Rmain.h literate.nw > main.h
	notangle -Rto\\_pair64.cc literate.nw > to_pair64.cc
	notangle -Rhandler.cc literate.nw > handler.cc
	pdflatex literate.tex
	pdflatex literate.tex
	astyle main.cc
	astyle handler.cc
	astyle to_pair64.cc
	astyle main.h
	astyle handler.cc
