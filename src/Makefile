# -+- mode: makefile-gmake -*-

all:
.PHONY: all

ifeq ($(HOSTNAME),padparadscha)
CXX := cxx
CXXFLAGS := -Wall -Wextra -O2
endif

objdir = ../out

CXXFLAGS += -std=c++14

directories += $(objdir)

-include $(objdir)/impl1.dep
-include $(objdir)/tty_observer.dep
-include $(objdir)/tty_player.dep
-include $(objdir)/board.dep
all: impl1
impl1: $(objdir)/impl1.o $(objdir)/tty_observer.o $(objdir)/tty_player.o $(objdir)/board.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl1.o: impl1.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(@:.o=.dep) $<
$(objdir)/tty_observer.o: tty_observer.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(@:.o=.dep) $<
$(objdir)/tty_player.o: tty_player.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(@:.o=.dep) $<
$(objdir)/board.o: board.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(@:.o=.dep) $<

-include $(objdir)/impl2.dep
all: impl2
impl2: $(objdir)/impl2.o $(objdir)/tty_observer.o $(objdir)/tty_player.o $(objdir)/board.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(objdir)/impl2.o: impl2.cpp | $(objdir)
	$(CXX) $(CXXFLAGS) -c -o $@ -MD -MF $(@:.o=.dep) $<


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
