# -+- mode: makefile-gmake -*-

all:
.PHONY: all

include make_variables.mk

SHELL := /bin/bash
ostype := $(shell echo $$OSTYPE)

objdir = ../out
gendir = ../out/gen
directories += $(objdir) $(objdir)/experimental $(objdir)/ansi

-include $(wildcard $(objdir)/*.dep)
-include $(wildcard $(objdir)/experimental/*.dep)
-include $(wildcard $(objdir)/ansi/*.dep)
$(objdir)/%.o: %.cpp | $(objdir) $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/ansi/%.o: ansi/%.cpp | $(objdir)/ansi $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/experimental/%.o: experimental/%.cpp | $(objdir)/experimental $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

$(gendir)/term.mode_def.hpp: make_command.sh ansi/term.mode.def
	bash make_command.sh

all: impl1
impl1_objs = \
  $(objdir)/impl1.o \
  $(objdir)/ansi/observer.tty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/ansi/enc.c2w.o \
  $(objdir)/ansi/enc.utf8.o \
  $(objdir)/contradef.o
impl1: $(impl1_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^

all: impl2
impl2_objs = \
  $(objdir)/impl2.o \
  $(objdir)/impl.o \
  $(objdir)/ansi/observer.tty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/ansi/enc.c2w.o \
  $(objdir)/ansi/enc.utf8.o \
  $(objdir)/contradef.o
impl2: $(impl2_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl2.o: impl2.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

all: impl3
impl3_objs = \
  $(objdir)/impl3.o \
  $(objdir)/impl.o \
  $(objdir)/ansi/observer.tty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/ansi/enc.c2w.o \
  $(objdir)/ansi/enc.utf8.o \
  $(objdir)/contradef.o
impl3: $(impl3_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl3.o: impl3.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

ifeq ($(ostype),cygwin)

all: twin
twin_CPPFLAGS = $(CPPFLAGS) -DUNICODE -D_UNICODE
twin_LIBS := -lgdi32 -limm32
twin_objs = \
  $(objdir)/twin/twin.o \
  $(objdir)/twin/win_messages.o \
  $(objdir)/ansi/enc.utf8.o \
  $(objdir)/impl.o \
  $(objdir)/ansi/observer.tty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/ansi/enc.c2w.o \
  $(objdir)/contradef.o
twin.exe: $(twin_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(twin_LIBS)
directories += $(objdir)/twin
$(objdir)/twin/%.o: twin/%.cpp | $(objdir)/twin
	$(CXX) $(twin_CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

endif

# all: experimental/bidi
experimental/bidi: $(objdir)/experimental/bidi.o
	$(CXX) $(CXXFLAGS) -o $@ $^

#all: minimal_openpt
minimal_openpt: $(objdir)/minimal_openpt.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/minimal_openpt.o: minimal_openpt.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(directories):
	mkdir -p $@

clean:
	-rm -rf $(objdir)
.PHONY: clean
