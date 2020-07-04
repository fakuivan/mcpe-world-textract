leveldb_deps = -pthread -lleveldb -lz -Ileveldb-mcpe/include -Lleveldb-mcpe/out-static -DDLLX=
json_deps = -Ijson/single_include
deps = -lstdc++fs -std=c++17
opts = $(leveldb_deps) $(json_deps) $(deps)

all: main main-debug

main:
	g++ main.cpp -Wall $(opts) -o $@

main-debug:
	g++ main.cpp -Wall $(opts) -g -o $@

clean:
	rm -f main-debug main
