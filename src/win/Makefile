# -*- mode: makefile-gmake -*-

all:
.PHONY: all

include ../make_variables.mk

CPPFLAGS = $(default_CPPFLAGS) -DUNICODE -D_UNICODE

all: test1.exe
test1_LIBS := -lgdi32
-include $(wildcard *.dep)
test1.exe: test1.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(test1_LIBS)
test1.o: test1.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
