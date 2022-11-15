bin_PROGRAMS=nsrlsvr$(EXEEXT)

nsrlsvr$(EXEEXT): cargo/src/main.rs
	cd cargo && cargo build --release && cp target/release/nsrlsvr ../nsrlsvr && cd ..

clean:
	rm -f nsrlsvr
	cd cargo && cargo clean && cd ..

