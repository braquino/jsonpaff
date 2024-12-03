build: src/jsonpaff.c dir
	gcc -c src/jsonpaff.c -Iinclude -o build/libjsonpaff.a
	gcc -shared -o build/libjsonpaff.so build/libjsonpaff.a
	gcc src/jsonpaff.c -lc -Iinclude -o build/private_tests -DPRIVATE_TESTS -g

test: build
	./build/private_tests

dir:
	mkdir -p build