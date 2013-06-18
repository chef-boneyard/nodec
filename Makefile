ROOT := $(shell pwd)
PLATFORM := $(shell uname)
PROG := nodec

#GECODE_VSN := 4.0.0
#GECODE_CONFIG_OPTS := --disable-flatzinc --disable-qt --disable-gist --disable-shared \
	              --enable-static --disable-doc-chm --disable-doc-docset --prefix=$(ROOT)/deps
GLOG_VSN := 0.3.3
GLOG_CONFIG_OPTS := --disable-shared --enable-static --disable-rtti --prefix=$(ROOT)/deps

GFLAGS_CONFIG_OPTS := --disable-shared --enable-static --prefix=$(ROOT)/deps

#DEP_LIBS := deps/lib/libgecodekernel.a deps/lib/libevent.a deps/lib/libglog.a

DEP_LIBS := deps/lib/libgflags.a deps/lib/libglog.a

EI_PATH := $(wildcard $(shell dirname $(shell which epmd))/../lib/erlang/lib/erl_interface-*)
EI_INCLUDE := $(EI_PATH)/include
EI_LIB := $(EI_PATH)/lib

CXXDEFINES := -D_LARGEFILE_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DPROGNAME=\\"$(PROG)\\"
CXXFLAGS := $(CXXDEFINES) -Ideps/include -Isrc -I$(EI_INCLUDE)  -Wall -Wwrite-strings -g -O
LINK_FLAGS := -L$(EI_LIB) -Ldeps/lib -lpthread -lei -lerl_interface -lgflags -lglog

ifeq (Darwin, $(PLATFORM))
LINK_FLAGS += -framework Foundation \
             -framework CoreServices \
             -framework ApplicationServices
endif

SRCS := $(wildcard src/*.cc)
OBJS := $(SRCS:.cc=.o)

all: $(PROG)

$(PROG): $(DEP_LIBS) $(OBJS)
	$(CXX) -o $(PROG) $(LINK_FLAGS) $(OBJS)

#deps/lib/libgecodekernel.a:
#	cd deps/gecode-$(GECODE_VSN) && CXXFLAGS= ./configure $(GECODE_CONFIG_OPTS) && make -j2 && make install

deps/lib/libglog.a:
	cd deps/glog && CXXFLAGS=-I$(ROOT)/deps/include LDFLAGS=-L$(ROOT)/deps/lib ./configure $(GLOG_CONFIG_OPTS) && make -j2 && make install

deps/lib/libgflags.a:
	cd deps/gflags && CXXFLAGS= ./configure $(GFLAGS_CONFIG_OPTS) && make -j2 && make install

depclean: glog-clean gflags-clean
	@rm -rf deps/lib deps/include deps/bin deps/share

#gecode-clean:
#	cd deps/gecode-$(GECODE_VSN) && make clean distclean

glog-clean:
	cd deps/glog && make clean distclean

gflags-clean:
	cd deps/gflags && make clean distclean


%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -f $(PROG) $(OBJS)

debug:
	echo $(PLATFORM)
