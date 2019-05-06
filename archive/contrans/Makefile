# -*- mode: makefile-gmake -*-

all:
.PHONY: all

all: contrans.exe piano.exe samp.exe
contrans.exe: contrans.o escseq.o cls.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

piano.exe: piano.o escseq.o cls.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

samp.exe: samp.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
