# -*- mode: makefile-gmake -*-

all:
.PHONY: all

include ../make_variables.mk

CPPFLAGS = $(default_CPPFLAGS) -DUNICODE -D_UNICODE

outdir = ../../out/twin
directories += $(outdir)


ansi_objs := \
  ../../out/ansi/enc.utf8.o \
  ../../out/impl.o \
  ../../out/ansi/observer.tty.o \
  ../../out/ansi/term.o \
  ../../out/ansi/line.o \
  ../../out/ansi/enc.c2w.o \
  ../../out/contradef.o

all: test1.exe
test1_LIBS := -lgdi32 -limm32
-include $(wildcard $(outdir)/*.dep)
test1.exe: $(outdir)/test1.o $(outdir)/win_messages.o $(ansi_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(test1_LIBS)
$(outdir)/%.o: %.cpp | $(outdir)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(directories):
	mkdir -p $@
