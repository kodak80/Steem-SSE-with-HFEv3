# Unit tests for steem/disk_hfe.cpp (HFE v1 and v3), built outside Steem
# against small mocks of the Steem globals. Linux/macOS/MSYS with g++.
#
#   make            build and run the synthetic tests
#   make real HXCFE=/path/to/hxcfe   (HxC's hxcfe needs LD_LIBRARY_PATH
#                   set to its build folder to find libhxcfe.so)
#                   also convert .st images to HFE v1/v3 with HxC's own
#                   converter and check every sector reads back
#
# disk_hfe.cpp/.h are copied next to the mocks so that the mock
# floppy_drive.h is picked up instead of Steem's.

CXX ?= g++
CXXFLAGS ?= -std=c++11 -g -O1 -Wall -Wno-unknown-pragmas -Wno-unused-result -fsanitize=address,undefined
B = build
HXCFE ?= hxcfe

all: $(B)/test_hfe
	cd $(B) && ./test_hfe

$(B)/disk_hfe.cpp: ../../steem/disk_hfe.cpp ../../steem/headers/disk_hfe.h $(wildcard mock/*.h)
	mkdir -p $(B)/hfe
	cp ../../steem/disk_hfe.cpp ../../steem/headers/disk_hfe.h mock/*.h $(B)/
	cp mock/hfe/libhxcfe.h ../../3rdparty/hfe/hfe_format.h $(B)/hfe/

$(B)/test_hfe: test_hfe.cpp $(B)/disk_hfe.cpp
	$(CXX) $(CXXFLAGS) -I$(B) -o $@ test_hfe.cpp $(B)/disk_hfe.cpp

$(B)/test_real: test_real.cpp $(B)/disk_hfe.cpp
	$(CXX) $(CXXFLAGS) -I$(B) -o $@ test_real.cpp $(B)/disk_hfe.cpp

$(B)/make_st.py: make_st.py
	cp make_st.py $(B)/

real: $(B)/test_real $(B)/make_st.py
	cd $(B) && python3 make_st.py
	cd $(B) && for f in dd hd; do \
	  $(HXCFE) -finput:$$f.st -conv:HXC_HFE -foutput:$${f}_v1.hfe >/dev/null && \
	  $(HXCFE) -finput:$$f.st -conv:HXC_HFEV3 -foutput:$${f}_v3.hfe >/dev/null || exit 1; done
	cd $(B) && ./test_real dd_v1.hfe dd.st 9 dd_v3.hfe dd.st 9 hd_v1.hfe hd.st 18 hd_v3.hfe hd.st 18

clean:
	rm -rf $(B)

.PHONY: all real clean
