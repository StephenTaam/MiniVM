CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

all: rhino_lab

rhino_lab: src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: rhino_lab
	./rhino_lab run examples/minimal.json
	./rhino_lab run examples/minimal.json --dump-tables --trace
	./rhino_lab run examples/function.json
	./rhino_lab run examples/containers.json
	./rhino_lab run examples/loop.json

clean:
	rm -f rhino_lab
