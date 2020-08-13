leveldb_deps = -pthread -lleveldb -lz -lsnappy -Ileveldb-mcpe/include -Lleveldb-mcpe/out-static -DDLLX=
json_deps = -Ijson/single_include
maps_deps = -Icppcodec/cppcodec
deps = -lstdc++fs -std=c++17
opts = $(maps_deps) $(leveldb_deps) $(json_deps) $(deps)

all: main main-debug maps-codec-debug

main:
	g++ main.cpp -Wall $(opts) -o $@

main-debug:
	g++ main.cpp -Wall $(opts) -g -o $@

maps-codec-debug:
	g++ mapscodec.cpp -Wall $(deps) $(maps_deps) -g -o $@

clean:
	rm -f main main-debug maps-codec-debug

# https://github.com/Podshot/MCEdit-Unified/issues/919#issuecomment-392257903
# sudo -- bash -c "mkdir /usr/include/snappy && cd /usr/include/snappy && ln -s ../snappy.h snappy.h"