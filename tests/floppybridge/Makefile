# Unit tests for steem/floppybridge_drive.cpp (sector check, speed smoothing,
# descriptor parsing). Linux/g++: make
CXXFLAGS=-std=c++17 -O1 -w -Imock -I../../steem/headers -I../../3rdparty
all: build/test_fb
	cd build && ./test_fb
build/test_fb: test_fb.cpp ../../steem/floppybridge_drive.cpp ../../steem/headers/floppybridge_drive.h mock/pch.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) test_fb.cpp -o $@ -ldl -lpthread
clean:
	rm -rf build
