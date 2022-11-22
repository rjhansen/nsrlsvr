bin_PROGRAMS=nsrlsvr$(EXEEXT)

edit = sed \
       -e 's|@bindir[@]|$(bindir)|g' \
       -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
       -e 's|@prefix[@]|$(prefix)|g'

nsrlsvr$(EXEEXT): cargo/src/main.rs cargo/src/globals.rs Makefile
	$(edit) cargo/src/globals.rs > cargo/src/globals.rs.tmp
	mv -f cargo/src/globals.rs.tmp cargo/src/globals.rs
	cd cargo && cargo build --release && cp target/release/nsrlsvr ../nsrlsvr && cd ..

clean:
	rm -f nsrlsvr
	cd cargo && cargo clean && cd ..

