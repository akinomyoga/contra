# -+- mode: makefile-gmake -*-

all:
.PHONY: all

ifeq ($(HOSTNAME),padparadscha)
  libmwg_dir := $(HOME)/opt/libmwg-201509
  libmwg_cxxprefix := i686-pc-linux-gnu-gcc-6.3.1+cxx11-release
  CXX := g++
  CXXFLAGS := -Wall -Wextra -O2 \
    -I $(libmwg_dir)/include \
    -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
    -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
endif

objdir = ../out

CXXFLAGS += -std=c++14
CPPFLAGS = -MP -MD -MF $(@:.o=.dep)

directories += $(objdir) $(objdir)/experimental $(objdir)/bidi

-include $(objdir)/impl1.dep
-include $(objdir)/tty_observer.dep
-include $(objdir)/tty_player.dep
-include $(objdir)/board.dep
all: impl1
impl1: $(objdir)/impl1.o $(objdir)/tty_observer.o $(objdir)/tty_player.o $(objdir)/board.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl1.o: impl1.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/tty_observer.o: tty_observer.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/tty_player.o: tty_player.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<
$(objdir)/board.o: board.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

-include $(objdir)/impl2.dep
all: impl2
impl2: $(objdir)/impl2.o $(objdir)/tty_observer.o $(objdir)/tty_player.o $(objdir)/board.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl2.o: impl2.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

-include $(objdir)/experimental/bidi.dep
all: experimental/bidi
experimental/bidi: $(objdir)/experimental/bidi.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/experimental/bidi.o: experimental/bidi.cpp | $(objdir)/experimental
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

-include $(objdir)/bidi/impl1.dep
all: bidi/impl1
bidi/impl1: $(objdir)/bidi/impl1.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/bidi/impl1.o: bidi/impl1.cpp | $(objdir)/bidi
	$(CXX) $(CXXFLAGS) -c -o $@ $(CPPFLAGS) $<

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
