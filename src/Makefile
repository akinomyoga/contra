# -+- mode: makefile-gmake -*-

all:
.PHONY: all

include make_variables.mk
CXXFLAGS += -std=gnu++17 -I ../ext

SHELL := /bin/bash
ostype := $(shell echo -n $$OSTYPE)

objdir = ../out
gendir = ../out/gen
directories += $(objdir) $(objdir)/experimental $(objdir)/ansi $(objdir)/ttty $(gendir)

-include $(shell find $(objdir) -name \*.dep)
$(objdir)/%.o: %.cpp | $(objdir) $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/ansi/%.o: ansi/%.cpp | $(objdir)/ansi $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/ttty/%.o: ttty/%.cpp | $(objdir)/ttty $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/gen/%.o: gen/%.cpp | $(objdir)/gen $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/experimental/%.o: experimental/%.cpp | $(objdir)/experimental $(gendir)/term.mode_def.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

$(gendir)/term.mode_def.hpp: make_command.sh ansi/term.mode.def
	bash make_command.sh

#------------------------------------------------------------------------------
# test

all: impl1
impl1_objs = \
  $(objdir)/impl1.o \
  $(objdir)/ttty/buffer.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/contradef.o
impl1: $(impl1_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^

all: impl2
impl2_objs = \
  $(objdir)/impl2.o \
  $(objdir)/pty.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/contradef.o
impl2: $(impl2_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
$(objdir)/impl2.o: impl2.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

directories += out

all: iso2022-convert.exe
iso2022-convert.exe: $(objdir)/iso2022-convert.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: gen/iso2022.gendef-table.exe
gen/iso2022.gendef-table.exe: $(gendir)/iso2022.gendef-table.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: iso2022-dump.exe
iso2022-dump.exe: $(objdir)/iso2022-dump.o $(objdir)/enc.utf8.o $(objdir)/enc.c2w.o $(objdir)/iso2022.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: out/iso2022.html
out/iso2022.html: iso2022-dump.exe res/iso2022.def res/iso2022-jis.def | out
	./iso2022-dump.exe

$(gendir)/iso2022.jisx0212-plane.def: gen/iso2022.create-table.sh | $(gendir)
	cd gen && bash iso2022.create-table.sh
res/iso2022.jisx-1.bin: iso2022-dump.exe gen/iso2022.generate-bin.def $(gendir)/iso2022.jisx0212-plane.def
	./iso2022-dump.exe gen/iso2022.generate-bin.def

#------------------------------------------------------------------------------
# ttty

all: ttty.exe
ttty_objs = \
  $(objdir)/ttty.o \
  $(objdir)/pty.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/contradef.o
ttty.exe: $(ttty_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
$(objdir)/ttty.o: ttty.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

#------------------------------------------------------------------------------
# twin

ifeq ($(ostype),cygwin)

twin_CPPFLAGS = $(CPPFLAGS) -DUNICODE -D_UNICODE
twin_LIBS := -lgdi32 -limm32 $(LIBS)
#twin_LDFLAGS := $(LDFLAGS) -static-libgcc -static-libstdc++ -s
twin_LDFLAGS := $(LDFLAGS)
twin_LDFLAGS += -mwindows

directories += $(objdir)/twin
$(objdir)/twin/%.o: twin/%.cpp | $(objdir)/twin
	$(CXX) $(twin_CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

all: twin.exe
twin_objs = \
  $(objdir)/twin/twin.o \
  $(objdir)/twin/win_messages.o \
  $(objdir)/pty.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/iso2022.o \
  $(objdir)/contradef.o
twin.exe: $(twin_objs)
	$(CXX) $(CXXFLAGS) $(twin_LDFLAGS) -o $@ $^ $(twin_LIBS)

all: twin/test-win1.exe
twin/test-win1.exe: $(objdir)/twin/test-win1.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(twin_LIBS)
all: twin/test-fontfamily.exe
twin/test-fontfamily.exe: $(objdir)/twin/test-fontfamily.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(twin_LIBS)
endif

#------------------------------------------------------------------------------
# tx11

all: tx11.exe
CXXFLAGS += -I /usr/include/freetype2
tx11_CXXFLAGS := $(CXXFLAGS)
tx11_LDFLAGS := $(LDFLAGS)
tx11_LIBS := -lXft -lX11 $(LIBS)
tx11_objs := \
  $(objdir)/tx11.o \
  $(objdir)/pty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/iso2022.o \
  $(objdir)/contradef.o

tx11.exe: $(tx11_objs)
	$(CXX) $(CXXFLAGS) $(tx11_LDFLAGS) -o $@ $^ $(tx11_LIBS)

#------------------------------------------------------------------------------
# samples

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
