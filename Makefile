build: src/jsonpaff.c src/test_jsonpaff.c dir
	gcc -c src/jsonpaff.c -Iinclude -o build/libjsonpaff.a
	gcc src/jsonpaff.c -lc -Iinclude -o build/private_tests -DPRIVATE_TESTS -g
	gcc src/jsonpaff.c src/test_jsonpaff.c -lc -Iinclude -o build/test

test: build
	./build/private_tests

dir:
	mkdir -p build