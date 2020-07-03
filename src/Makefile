# -+- mode: makefile-gmake -*-

all:
.PHONY: all test

make_config.mk: ./configure
	./configure

include make_config.mk
include make_variables.mk
LIBS = $(config_LIBS)
CPPFLAGS = $(config_CPPFLAGS) $(default_CPPFLAGS)
CXXFLAGS = $(config_CXXFLAGS)

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

test: impl1
impl1_objs = \
  $(objdir)/impl1.o \
  $(objdir)/ttty/buffer.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/sys.path.o \
  $(objdir)/contradef.o
impl1: $(impl1_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: impl2
impl2_objs = \
  $(objdir)/impl2.o \
  $(objdir)/pty.o \
  $(objdir)/dict.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/sys.signal.o \
  $(objdir)/sys.path.o \
  $(objdir)/sys.terminfo.o \
  $(objdir)/contradef.o
impl2: $(impl2_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
$(objdir)/impl2.o: impl2.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

directories += out

all: gen/iso2022-convert.exe
gen/iso2022-convert.exe: $(objdir)/gen/iso2022-convert.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: gen/iso2022.gendef-table.exe
gen/iso2022.gendef-table.exe: $(objdir)/gen/iso2022.gendef-table.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: gen/iso2022-dump.exe
gen/iso2022-dump.exe: $(objdir)/gen/iso2022-dump.o $(objdir)/enc.utf8.o $(objdir)/enc.c2w.o $(objdir)/iso2022.o $(objdir)/sys.path.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
all: out/iso2022.html
out/iso2022.html: gen/iso2022-dump.exe res/iso2022.def res/iso2022-jis.def | out
	gen/iso2022-dump.exe

$(gendir)/iso2022.jisx0212-plane.def: gen/iso2022.create-table.sh | $(gendir)
	cd gen && bash iso2022.create-table.sh
res/iso2022.jisx-1.bin: gen/iso2022-dump.exe gen/iso2022.generate-bin.def $(gendir)/iso2022.jisx0212-plane.def
	gen/iso2022-dump.exe gen/iso2022.generate-bin.def

#------------------------------------------------------------------------------
# contra

all: contra
CXXFLAGS += -I /usr/include/freetype2
contra_LDFLAGS := $(LDFLAGS)
contra_LIBS := -lXft -lX11 $(LIBS)
contra_objs := \
  $(objdir)/contra.o \
  $(objdir)/ttty.o \
  $(objdir)/tx11.o \
  $(objdir)/dict.o \
  $(objdir)/pty.o \
  $(objdir)/ansi/term.o \
  $(objdir)/ansi/line.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/enc.c2w.o \
  $(objdir)/iso2022.o \
  $(objdir)/sys.signal.o \
  $(objdir)/sys.path.o \
  $(objdir)/sys.terminfo.o \
  $(objdir)/contradef.o

contra_LIBS := -lncursesw $(contra_LIBS)

ifeq ($(ostype),cygwin)

directories += $(objdir)/twin
twin_CPPFLAGS = $(CPPFLAGS) -DUNICODE -D_UNICODE
$(objdir)/twin/%.o: twin/%.cpp | $(objdir)/twin
	$(CXX) $(twin_CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

CXXFLAGS += -Duse_twin
contra_objs += \
  $(objdir)/twin/twin.o \
  $(objdir)/twin/win_messages.o

contra_LIBS := -lgdi32 -limm32 $(contra_LIBS)
contra_LDFLAGS += -mwindows
#contra_LDFLAGS += -static-libstdc++ -static-libgcc

endif

contra: $(contra_objs)
	$(CXX) $(CXXFLAGS) $(contra_LDFLAGS) -o $@ $^ $(contra_LIBS)

#------------------------------------------------------------------------------
# twin

ifeq ($(ostype),cygwin)
twin_LIBS := -lgdi32 -limm32 $(LIBS)

all: twin/test-win1.exe
twin/test-win1.exe: $(objdir)/twin/test-win1.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(twin_LIBS)
all: twin/test-fontfamily.exe
twin/test-fontfamily.exe: $(objdir)/twin/test-fontfamily.o $(objdir)/enc.utf8.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(twin_LIBS)
endif

#------------------------------------------------------------------------------
# samples

# all: experimental/bidi
experimental/bidi: $(objdir)/experimental/bidi.o
	$(CXX) $(CXXFLAGS) -o $@ $^

#all: minimal_openpt
minimal_openpt: $(objdir)/minimal_openpt.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/minimal_openpt.o: ../min/minimal_openpt.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

#------------------------------------------------------------------------------
# test

test: test_seq
test_seq_objs := \
  $(objdir)/test_seq.o \
  $(objdir)/contradef.o \
  $(objdir)/enc.utf8.o \
  $(objdir)/iso2022.o \
  $(objdir)/sys.path.o
test_seq:  $(test_seq_objs)
	$(CXX) $(CXXFLAGS) -o $@ $^

#------------------------------------------------------------------------------

$(directories):
	mkdir -p $@

clean:
	-rm -rf $(objdir)
.PHONY: clean
