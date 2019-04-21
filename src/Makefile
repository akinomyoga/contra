# -+- mode: makefile-gmake -*-

all:
.PHONY: all

ifeq ($(HOSTNAME),padparadscha)
  libmwg_dir := $(HOME)/opt/libmwg-201509
  libmwg_cxxprefix := i686-pc-linux-gnu-gcc-6.3.1+cxx11-release
  CXX := g++
  CXXFLAGS := -Wall -Wextra -O2 \
    -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
    -I $(libmwg_dir)/include \
    -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
else ifeq ($(HOSTNAME),chatoyancy)
  libmwg_dir := $(HOME)/opt/libmwg-20170824
  libmwg_cxxprefix := x86_64-pc-linux-gnu-gcc-7.3.1+cxx98-debug
  CXXFLAGS := -Wall -Wextra \
    -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
    -I $(libmwg_dir)/include \
    -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
endif

objdir = ../out
gendir = ../out/gen

CXXFLAGS += -std=c++17
CPPFLAGS = -MP -MD -MF $(@:.o=.dep)

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
