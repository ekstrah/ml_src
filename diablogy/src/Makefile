# $Id: Makefile,v 1.42 2010/03/23 18:44:40 ctang Exp $ 

# Options:
debug=on

TOPDIR	=..
.PHONY:depend clean tags test_dlopen

# MONITOR_EVENT_DRIVEN_APP_LIMIT_EPOLL_WAIT_TO_ONE_EVENT is needed only for squid, which may perform multiple reads/writes as returned 
# by epoll_wait, and then execute the callbacks for those reads/writes. It is necessary to set 
# MONITOR_EVENT_DRIVEN_APP_LIMIT_EPOLL_WAIT_TO_ONE_EVENT=1 to discover paths for squid.
CPPFLAGS	= -DMONITOR_EVENT_DRIVEN_APP=1 -DMONITOR_EVENT_DRIVEN_APP_LIMIT_EPOLL_WAIT_TO_ONE_EVENT=1 -DMONITOR_FILE_IO=0

ifeq ($(debug), on)
  CFLAGS	+= -g -DDEBUG=1
else
  CFLAGS	+= -g -O3 -DDEBUG=0
  ifeq ($(prof), on)
    CFLAGS	+= -pg 
    LFLAGS	+= -pg 
  else
    CFLAGS	+= -fomit-frame-pointer 
  endif
endif

CFLAGS		+= -D_REENTRANT -pthread 
LFLAGS		+= -pthread
BACKTRACE_FLAG  = #-rdynamic 

CC		= gcc
CXX		= g++
#OBJDIR		= $(TOPDIR)/obj
LIBDIR		= $(TOPDIR)/lib
#BINDIR          = $(TOPDIR)/bin
DIRS		= $(OBJDIR) $(BINDIR) $(LIBDIR)

CFLAGS		+= -I. -Wall -Wno-deprecated -Wno-strict-aliasing #-Werror
LFLAGS		+= -L$(TOPDIR)/lib  -lrt
CFILES		= $(wildcard *.c *.cc)
LIBBLUECOAT	= $(LIBDIR)/libbluecoat.so
APPS		= vlog $(LIBVPATH) $(LIBBLUECOAT) #noop-test truncate-log hello UDPEchoServer UDPEchoClient TCPEchoServer TCPEchoClient # TestMemMappedFile decode

all:$(DIRS) $(APPS)

clean:
	-rm -rf $(OBJDIR) $(BINDIR) $(LIBDIR) hello truncate-log *~ /tmp/vpath core vlog bluecoat.o

cleanlog:
	-sudo rm -rf /tmp/vpath

tags: 
	ctags *.cc *.c *.h

depend:
	$(CXX) -MM $(CFLAGS) $(CFILES) $(UTILS_SRCS) | \
		sed -e 's/^[^ ]/$$(OBJDIR)\/&/g' > makefile.dep

makefile.dep:
	@touch $@
	$(MAKE) depend
include makefile.dep


$(DIRS):
	mkdir -p $@

$(OBJDIR)/%.o:%.cc
	$(CXX) $(CFLAGS) -o $@ -c $<

$(OBJDIR)/%.o:%.c
	$(CXX) $(CFLAGS) -o $@ -c $<

##############################################################
#HELLO_OBJS=$(OBJDIR)/hello.o
#hello:$(HELLO_OBJS)
#	$(CXX) -o $@ $(LFLAGS) -ldl $(HELLO_OBJS)

test_dlopen:
	$(CC) -g -fpic -shared -L../../lib -o ../lib/libdummy2.so dummyso2.c
	sudo cp ../lib/libdummy2.so /lib
	sudo ldconfig
	$(CC) -g -fpic -shared -L../../lib -o ../lib/libdummy.so -ldummy2 dummyso.c
	sudo cp ../lib/libdummy.so /lib
	sudo ldconfig
	$(CC) -o $@ -ldl test_dlopen.c

#TRUNC_OBJS=$(OBJDIR)/truncate-log.o
#truncate-log:$(TRUNC_OBJS)
#	$(CXX) -o $@ $(LFLAGS) -ldl $(TRUNC_OBJS)

vlog:vlog.c utils.c
	$(CC) -Wall -O3 -o $@ vlog.c utils.c -L/usr/lib/ -lrt -lpthread -lcassandra

noop-test:noop-test.c
	$(CC) -Wall -g -o $@ noop-test.c -ldl

DECODE_OBJS=$(addprefix $(OBJDIR)/, Decode.o utils.o MemMappedInputFile.o)
decode:$(DECODE_OBJS) $(LIBVPATH)
	$(CXX) -o $@ $(LFLAGS)  $(DECODE_OBJS)

FILEMAP_OBJS=$(addprefix $(OBJDIR)/, TestMemMappedFile.o MemMappedInputFile.o MemMappedOutputFile.o utils.o)
TestMemMappedFile:$(FILEMAP_OBJS)
	$(CXX) -o $@ $(LFLAGS) -ldl  $(FILEMAP_OBJS)

VPATH_SRCS=utils.c libc.c
$(LIBVPATH): $(VPATH_SRCS) libc.h utils.h
	$(CXX) $(CPPFLAGS) $(CFLAGS) -D__IN_VPATH__=1 -rdynamic -fpic -shared -L../../lib -pthread -o $@ $(VPATH_SRCS) -lpthread -lrt -ldl -lstdc++

BLUECOAT_SRCS=bluecoat.c
$(LIBBLUECOAT): $(BLUECOAT_SRCS)
# TODO: Remove -Wno-format-truncation flag and fix the warnings
	gcc -Wall -rdynamic -fPIC -Wno-format-truncation -c bluecoat.c
	gcc -shared -Wl,-soname,libbluecoat.so -Wl,-export-dynamic -o ../lib/libbluecoat.so bluecoat.o -ldl -lrt -lpthread
	cp ../lib/libbluecoat.so /lib64/libbluecoat.so

bcpp:
	@(for  file in *.cc *.c *.h; do bcpp $$file ;done) > t.txt 2> err.txt
	@echo "bcpp done"
