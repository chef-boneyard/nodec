ROOT := $(shell pwd)
PLATFORM := $(shell uname)
PROG := depnode

GECODE_VSN := 4.0.0
GECODE_CONFIG_OPTS := --disable-flatzinc --disable-qt --disable-gist --disable-shared \
	              --enable-static --disable-doc-chm --disable-doc-docset --prefix=$(ROOT)/deps

LIBEVENT_VSN := 2.0.21-stable
LIBEVENT_CONFIG_OPTS := --disable-shared --enable-static --prefix=$(ROOT)/deps

GLOG_VSN := 0.3.3
GLOG_CONFIG_OPTS := --disable-shared --enable-static --disable-rtti --prefix=$(ROOT)/deps

DEP_LIBS := deps/lib/libgecodekernel.a deps/lib/libevent.a deps/lib/libglog.a

EI_PATH := $(wildcard $(shell dirname $(shell which epmd))/../lib/erlang/lib/erl_interface-*)
EI_INCLUDE := $(EI_PATH)/include
EI_LIB := $(EI_PATH)/lib

CXXDEFINES := -D_LARGEFILE_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DPROGNAME=\\"$(PROG)\\"
CXXFLAGS := $(CXXDEFINES) -Ideps/include -Isrc -I$(EI_INCLUDE)  -Wall -Werror -pedantic -g -O3 -std=c++0x
LINK_FLAGS := -Ldeps/lib -luv -lglog -lgecodekernel -lgecodeint -lgecodefloat -lgecodesearch -lgecodesupport \
	      -L$(EI_LIB) -lei -lerl_interface

ifeq (Darwin, $(PLATFORM))
LINK_FLAGS += -framework Foundation \
             -framework CoreServices \
             -framework ApplicationServices
endif

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:.cpp=.o)

all: $(PROG)

$(PROG): $(DEP_LIBS) $(OBJS)
	$(CXX) -o $(PROG) $(LINK_FLAGS) $(OBJS)

deps/lib/libgecodekernel.a:
	cd deps/gecode-$(GECODE_VSN) && CXXFLAGS= ./configure $(GECODE_CONFIG_OPTS) && make -j2 && make install

deps/lib/libglog.a:
	cd deps/glog-$(GLOG_VSN) && ./configure $(GLOG_CONFIG_OPTS) && make -j2 && make install

deps/lib/libevent.a:
	cd deps/libevent-$(LIBEVENT_VSN) && ./configure $(LIBEVENT_CONFIG_OPTS) && make -j2 && make install

depclean: gecode-clean libevent-clean glog-clean
	@rm -rf deps/lib deps/include

gecode-clean:
	cd deps/gecode-$(GECODE_VSN) && make clean distclean

libevent-clean:
	cd deps/libevent-$(LIBEVENT_VSN) && make clean distclean

glog-clean:
	cd deps/glog-$(GLOG_VSN) && make clean distclean


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -f $(PROG) $(OBJS)

debug:
	echo $(PLATFORM)
