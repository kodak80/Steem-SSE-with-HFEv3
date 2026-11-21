# Tests for the PicoGUS card emulation (3rdparty/picogus), built outside
# Steem. Linux/macOS/MSYS with g++.
#
#   make            synthetic tests of the card (control port, GUS, SB, OPL)
#                   and of steem/isa_picogus.cpp (ISA window, audio mixing)
#   make pgusinit PGUSINIT=/path/to/raven/sw/isa/drivers/picogus
#                   build the Atari (Raven060) pgusinit.c against the
#                   emulated card and run it through mode changes, port
#                   changes, /save and power cycles

CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -std=c++17 -g -O1 -Wall -Wno-sign-compare -Wno-unused-variable -Wno-unused-function -Wno-class-memaccess -fsanitize=address,undefined -fno-sanitize=shift-base
CFLAGS ?= -g -O1 -Wall -Wno-unused-function -fsanitize=address,undefined -fno-sanitize=shift-base
PG = ../../3rdparty/picogus
INC = -I$(PG)/shim -I$(PG)/sw -I$(PG)
B = build
CARD = pgemu pg_gus pg_sb pg_opl pg_opl3 pg_volctrl pg_platform
CARD_OBJS = $(CARD:%=$(B)/%.o)

all: $(B)/test_card $(B)/test_steem
	cd $(B) && ./test_card && ./test_steem

$(B)/%.o: $(PG)/%.cpp $(wildcard $(PG)/*.h $(PG)/shim/*.h $(PG)/shim/*/*.h)
	@mkdir -p $(B)
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<

$(B)/test_card: test_card.cpp $(CARD_OBJS)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ -lm

# steem/isa_picogus.cpp against mocks of the Steem globals it uses
$(B)/isa_picogus.cpp: ../../steem/isa_picogus.cpp ../../steem/headers/isa_picogus.h
	@mkdir -p $(B)
	cp ../../steem/isa_picogus.cpp ../../steem/headers/isa_picogus.h $(B)/

$(B)/test_steem: test_steem.cpp $(B)/isa_picogus.cpp $(CARD_OBJS) $(wildcard mock_steem/*.h)
	$(CXX) $(CXXFLAGS) -Imock_steem -I$(B) -I../../3rdparty -o $@ test_steem.cpp $(B)/isa_picogus.cpp $(CARD_OBJS) -lm

# pgusinit: its sources are laid out as in the raven repo so that its
# relative includes (../common/picogus.h, ../../../isa.h) find the mocks
PGI = $(B)/isa/drivers/picogus
pgusinit: $(B)/run_pgusinit
	cd $(B) && ./run_pgusinit

$(B)/run_pgusinit: run_pgusinit.cpp pgusinit_wrap.c $(CARD_OBJS) $(wildcard mock/*.h mock/mint/*.h)
	@test -n "$(PGUSINIT)" || { echo "set PGUSINIT=/path/to/raven/sw/isa/drivers/picogus"; exit 1; }
	mkdir -p $(PGI)/pgusinit $(PGI)/common
	cp $(PGUSINIT)/pgusinit/pgusinit.c $(PGUSINIT)/pgusinit/pgusinit.h $(PGI)/pgusinit/
	cp $(PGUSINIT)/common/picogus.h $(PGI)/common/
	cp mock/isa.h $(B)/isa/isa.h
	$(CC) $(CFLAGS) -w -D__ATARI__ -Imock -I$(PG) -I$(PGI)/pgusinit -c -o $(B)/pgusinit_wrap.o pgusinit_wrap.c
	$(CXX) $(CXXFLAGS) -I$(PG) -o $@ run_pgusinit.cpp $(B)/pgusinit_wrap.o $(CARD_OBJS) -lm

clean:
	rm -rf $(B)

.PHONY: all pgusinit clean
