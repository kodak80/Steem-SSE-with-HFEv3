# Unit tests for steem/greaseweazle_flux.cpp (flux stream decoding,
# revolutions, PLL + sector check). Linux/g++: make
CXXFLAGS=-std=c++17 -O1 -w -Imock -I../../steem/headers -I../../3rdparty
all: build/test_gw
	cd build && ./test_gw
build/test_gw: test_gw.cpp ../../steem/greaseweazle_flux.cpp ../../steem/headers/greaseweazle_flux.h ../../steem/floppybridge_drive.cpp mock/pch.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) test_gw.cpp -o $@ -ldl -lpthread
clean:
	rm -rf build
