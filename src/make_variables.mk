# -+- mode: makefile-gmake -*-

# ifeq ($(HOSTNAME),padparadscha)
#   libmwg_dir := $(HOME)/opt/libmwg-201509
#   libmwg_cxxprefix := i686-pc-linux-gnu-gcc-6.3.1+cxx11-release
#   CXX := g++
#   CXXFLAGS := -Wall -Wextra -O2 \
#     -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
#     -I $(libmwg_dir)/include \
#     -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
# else ifeq ($(HOSTNAME),chatoyancy)
#   libmwg_dir := $(HOME)/opt/libmwg-20170824
#   libmwg_cxxprefix := x86_64-pc-linux-gnu-gcc-7.3.1+cxx98-debug
#   CXXFLAGS := -Wall -Wextra \
#     -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
#     -I $(libmwg_dir)/include \
#     -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
# else ifeq ($(HOSTNAME),magnate2016)
#   libmwg_dir := $(HOME)/opt/libmwg-201707
#   libmwg_cxxprefix := i686-cygwin-gcc-5.4.0+cxx17-debug
#   CXXFLAGS := -Wall -Wextra \
#     -I $(libmwg_dir)/include/$(libmwg_cxxprefix) \
#     -I $(libmwg_dir)/include \
#     -L $(libmwg_dir)/lib/$(libmwg_cxxprefix)
# else
#   CXXFLAGS := -Wall -Wextra -I ../ext
# endif

CXXFLAGS := -Wall -Wextra
CXXFLAGS += -O1
#CXXFLAGS += -O3 -s -DNDEBUG
#CXXFLAGS += -O2 -s -DNDEBUG

default_CPPFLAGS = -MP -MD -MF $(@:.o=.dep)
CPPFLAGS = $(default_CPPFLAGS)
