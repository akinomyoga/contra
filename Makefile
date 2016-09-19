# -+- mode: makefile-gmake -*-

all:
.PHONY: all

ifeq ($(HOSTNAME),padparadscha)
CXX := cxx
CXXFLAGS := -Wall -Wextra
endif

directories += out

all: impl1
impl1: out/impl1.o
	$(CXX) $(CXXFLAGS) -o $@ $^
out/impl1.o: impl1.cpp | out
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(directories):
	mkdir -p $@
