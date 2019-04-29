# -*- mode: makefile-gmake -*-

all:
.PHONY: all

include ../make_variables.mk

CPPFLAGS = $(default_CPPFLAGS) -DUNICODE -D_UNICODE

outdir = ../../out/twin
directories += $(outdir)

all: test1.exe
test1_LIBS := -lgdi32 -limm32
-include $(wildcard $(outdir)/*.dep)
test1.exe: $(outdir)/test1.o $(outdir)/win_messages.o ../../out/ansi/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(test1_LIBS)
$(outdir)/%.o: %.cpp | $(outdir)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(directories):
	mkdir -p $@
