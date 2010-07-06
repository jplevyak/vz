# Makefile for VZite vz

all: defaulttarget

USE_PLIB=1
MODULE=vz
DEBUG=1
#OPTIMIZE=1
#PROFILE=1
#USE_GC=1
#LEAK_DETECT=1
#VALGRIND=1

MAJOR=0
MINOR=1

include ../plib/Makefile

CFLAGS += -rdynamic -I../plib
# LIBS = -Wl,-Bstatic 

LIBS += -ledit -lpcre 
ifdef USE_GC
LIBS += -L../plib -lplib_gc -lgc 
else
LIBS += -L../plib -lplib
endif

# kfs
CFLAGS += -I../kfs/src/cc -I../kfs/src/cc/meta
LIBS += -L../kfs/build/lib/static -lchunk -lkfsClient -lkfsEmulator -lkfsMeta -lkfsIO -lkfsCommon -lqcdio -ltools

# sector
CFLAGS += -I../sector/common -I../sector/master -I../sector/client -I../sector/slave -I../sector/security -I../sector/include -I../sector/gmp -I../sector/udt
LIBS += -L../sector/lib -lmaster -lclient -lslave -lsecurity -lrpc -lcommon -ludt

# trafficserver
CFLAGS += -I../ts/libinktomi++
LIBS += -L../ts/libinktomi++ -L../ts/iocore/eventsystem -L../ts/iocore/net -L../ts/iocore/cache -L../ts/libinktomi++ -linknet -linkcache -linkevent -linktomi++ 

LIBS += -Wl,-Bdynamic -llog4cpp -lssl -lcurses -ltcl -lcrypto -lz -ldl -lpthread -lrt -lm

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/vz.1 $(MODULE)/vz.cat

LIB_SRCS = vz_kfs.cc vz_sector.cc cli.cc runtime.cc
LIB_OBJS = $(LIB_SRCS:%.cc=%.o)

DCS_SRCS = main.cc version.cc
DCS_OBJS = $(DCS_SRCS:%.cc=%.o)

EXECUTABLE_FILES = vz
LIBRARY = libvz.a
LIBRARIES = libvz.a
INSTALL_LIBRARIES = libvz.a
#INCLUDES =
MANPAGES = vz.1

CLEAN_FILES += *.cat

ifeq ($(OS_TYPE),CYGWIN)
EXECUTABLES = $(EXECUTABLE_FILES:%=%.exe)
DCS = vz.exe
else
EXECUTABLES = $(EXECUTABLE_FILES)
DCS = vz
endif

PROGRAM_SRCS = builtin_vz.cc cat.cc

ALL_SRCS = $(DCS_SRCS) $(LIB_SRCS) $(PROGRAM_SRCS)

defaulttarget: $(EXECUTABLES) vz.cat builtin_vz

help:
	@echo "available commands:"
	@echo "  make"
	@echo "  make release"
	@echo "  make install"
	@echo "  make deinstall"
	@echo "  make version"
	@echo "  make depend"
	@echo "  make test"

release: $(DCS)
	$(MAKE) clean
	$(MAKE) DEBUG=0 OPTIMIZE=1
	strip $(DCS)

importlibs:
	cp `ldd vz | grep "=>" | grep -v linux-vdso | awk '{ printf "%s " $$3, "" }'` .
	touch importlibs

builtin_vz: builtin_vz.cc
	$(CXX) $(CFLAGS) $^ version.o -o $@ -L../plib -lplib -lpthread
	ln -f -s builtin_vz cat.vz

a.out: t.cc
	g++ -g -Wno-invalid-offsetof t.cc -I ~/src/avro-trunk/lang/c++/api -L/usr/local/lib -L/home/jplevyak/src/avro-trunk/lang/c++/.libs -lavrocpp -lboost_system-mt -lboost_filesystem-mt -lboost_regex-mt

$(LIBRARY):  $(LIB_OBJS)
	ar $(AR_FLAGS) $@ $^

install:
	cp $(EXECUTABLES) $(PREFIX)/bin
	cp $(MANPAGES) $(PREFIX)/man/man1
#	cp $(INCLUDES) $(PREFIX)/include
#	cp $(INSTALL_LIBRARIES) $(PREFIX)/lib

deinstall:
	rm $(EXECUTABLES:%=$(PREFIX)/bin/%)
	rm $(MANPAGES:%=$(PREFIX)/man/man1/%)
#	rm $(INCLUDES:%=$(PREFIX)/include/%)
#	rm $(INSTALL_LIBRARIES:%=$(PREFIX)/lib/%)

$(DCS): $(DCS_OBJS) $(LIB_OBJS) $(LIBRARIES) ../plib/libplib.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS) 

vz.cat: vz.1
	rm -f vz.cat
	nroff -man vz.1 | sed -e 's/.//g' > vz.cat

vz.o: LICENSE.i COPYRIGHT.i

$(DCS): ../sector/lib/libcommon.a ../sector/lib/libclient.a ../sector/lib/libmaster.a \
../sector/lib/librpc.a ../sector/lib/libslave.a ../sector/lib/libsecurity.a \
../kfs/build/lib/static/libchunk.a ../kfs/build/lib/static/libkfsIO.a \
../kfs/build/lib/static/libkfsClient.a ../kfs/build/lib/static/libkfsMeta.a \
../kfs/build/lib/static/libkfsCommon.a ../kfs/build/lib/static/libqcdio.a \
../kfs/build/lib/static/libkfsEmulator.a ../kfs/build/lib/static/libtools.a \
../ts/iocore/net/libinknet.a


version.o: Makefile

-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

main.o: main.cc defs.h ../plib/plib.h ../plib/arg.h ../plib/barrier.h \
 ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h ../plib/freelist.h \
 ../plib/defalloc.h ../plib/list.h ../plib/log.h ../plib/vec.h \
 ../plib/map.h ../plib/threadpool.h ../plib/misc.h ../plib/util.h \
 ../plib/conn.h ../plib/md5.h ../plib/mt64.h ../plib/hash.h \
 ../plib/persist.h ../plib/prime.h ../plib/service.h ../plib/timer.h \
 ../plib/unit.h cli.h vz.h vz_inline.h COPYRIGHT.i LICENSE.i
version.o: version.cc defs.h ../plib/plib.h ../plib/arg.h \
 ../plib/barrier.h ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h \
 ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
 ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
 ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
 ../plib/hash.h ../plib/persist.h ../plib/prime.h ../plib/service.h \
 ../plib/timer.h ../plib/unit.h cli.h vz.h vz_inline.h
vz_kfs.o: vz_kfs.cc defs.h ../plib/plib.h ../plib/arg.h ../plib/barrier.h \
 ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h ../plib/freelist.h \
 ../plib/defalloc.h ../plib/list.h ../plib/log.h ../plib/vec.h \
 ../plib/map.h ../plib/threadpool.h ../plib/misc.h ../plib/util.h \
 ../plib/conn.h ../plib/md5.h ../plib/mt64.h ../plib/hash.h \
 ../plib/persist.h ../plib/prime.h ../plib/service.h ../plib/timer.h \
 ../plib/unit.h cli.h vz.h vz_inline.h ../kfs/src/cc/common/properties.h \
 ../kfs/src/cc/libkfsIO/NetManager.h ../kfs/src/cc/libkfsIO/ITimeout.h \
 ../kfs/src/cc/libkfsIO/NetConnection.h \
 ../kfs/src/cc/libkfsIO/KfsCallbackObj.h ../kfs/src/cc/libkfsIO/Event.h \
 ../kfs/src/cc/libkfsIO/IOBuffer.h ../kfs/src/cc/libkfsIO/TcpSocket.h \
 ../kfs/src/cc/common/kfsdecls.h ../kfs/src/cc/libkfsIO/Globals.h \
 ../kfs/src/cc/libkfsIO/NetManager.h ../kfs/src/cc/libkfsIO/Counter.h \
 ../kfs/src/cc/meta/NetDispatch.h ../kfs/src/cc/libkfsIO/Acceptor.h \
 ../kfs/src/cc/meta/ChunkServerFactory.h \
 ../kfs/src/cc/libkfsIO/KfsCallbackObj.h ../kfs/src/cc/meta/kfstypes.h \
 ../kfs/src/cc/common/kfstypes.h ../kfs/src/cc/meta/ChunkServer.h \
 ../kfs/src/cc/libkfsIO/NetConnection.h ../kfs/src/cc/meta/request.h \
 ../kfs/src/cc/meta/meta.h ../kfs/src/cc/common/config.h \
 ../kfs/src/cc/meta/base.h ../kfs/src/cc/meta/thread.h \
 ../kfs/src/cc/meta/util.h ../kfs/src/cc/libkfsIO/IOBuffer.h \
 ../kfs/src/cc/meta/queue.h ../kfs/src/cc/meta/ClientManager.h \
 ../kfs/src/cc/meta/ClientSM.h ../kfs/src/cc/meta/startup.h \
 ../kfs/src/cc/chunk/ChunkServer.h ../kfs/src/cc/qcdio/qcutils.h \
 ../kfs/src/cc/libkfsIO/TelemetryClient.h \
 ../kfs/src/cc/telemetry/packet.h ../kfs/src/cc/chunk/ChunkManager.h \
 ../kfs/src/cc/chunk/Chunk.h ../kfs/src/cc/common/log.h \
 ../kfs/src/cc/common/BufferedLogWriter.h \
 ../kfs/src/cc/libkfsIO/FileHandle.h ../kfs/src/cc/libkfsIO/Checksum.h \
 ../kfs/src/cc/chunk/Utils.h ../kfs/src/cc/chunk/KfsOps.h \
 ../kfs/src/cc/libkfsIO/Event.h ../kfs/src/cc/chunk/DiskIo.h \
 ../kfs/src/cc/qcdio/qcdiskqueue.h ../kfs/src/cc/qcdio/qciobufferpool.h \
 ../kfs/src/cc/qcdio/qcmutex.h ../kfs/src/cc/qcdio/qcdllist.h \
 ../kfs/src/cc/common/cxxutil.h ../kfs/src/cc/chunk/ClientManager.h \
 ../kfs/src/cc/chunk/ClientSM.h ../kfs/src/cc/chunk/RemoteSyncSM.h \
 ../kfs/src/cc/libkfsIO/ITimeout.h ../kfs/src/cc/meta/queue.h \
 ../kfs/src/cc/chunk/BufferManager.h ../kfs/src/cc/qcdio/qciobufferpool.h \
 ../kfs/src/cc/chunk/MetaServerSM.h ../kfs/src/cc/chunk/ChunkManager.h \
 ../kfs/src/cc/meta/ChildProcessTracker.h \
 ../kfs/src/cc/meta/LayoutManager.h ../kfs/src/cc/meta/LeaseCleaner.h \
 ../kfs/src/cc/meta/ChunkReplicator.h ../kfs/src/cc/libkfsIO/Counter.h \
 ../kfs/src/cc/chunk/Logger.h ../kfs/src/cc/meta/thread.h
vz_sector.o: vz_sector.cc defs.h ../plib/plib.h ../plib/arg.h \
 ../plib/barrier.h ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h \
 ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
 ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
 ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
 ../plib/hash.h ../plib/persist.h ../plib/prime.h ../plib/service.h \
 ../plib/timer.h ../plib/unit.h cli.h vz.h vz_inline.h \
 ../sector/master/master.h ../sector/gmp/gmp.h \
 ../sector/common/transport.h ../sector/udt/udt.h \
 ../sector/common/crypto.h ../sector/include/sector.h \
 ../sector/gmp/message.h ../sector/gmp/prec.h \
 ../sector/master/../common/log.h ../sector/include/conf.h \
 ../sector/common/index.h ../sector/common/meta.h \
 ../sector/common/topology.h ../sector/common/index2.h \
 ../kfs/src/cc/meta/meta.h ../kfs/src/cc/common/config.h \
 ../kfs/src/cc/meta/base.h ../kfs/src/cc/meta/kfstypes.h \
 ../kfs/src/cc/common/kfstypes.h ../sector/common/ssltransport.h \
 ../sector/common/routing.h ../sector/common/dhash.h \
 ../sector/common/topology.h ../sector/master/slavemgmt.h \
 ../sector/master/transaction.h ../sector/master/user.h \
 ../sector/master/../common/threadpool.h ../sector/security/security.h \
 ../sector/security/filesrc.h ../sector/slave/slave.h \
 ../sector/common/datachn.h ../plib/log.h ../sector/include/sphere.h \
 ../sector/client/client.h ../sector/client/fscache.h \
 ../ts/libinktomi++/ink_base64.h \
 ../ts/libinktomi++/ink_apidefs.h
runtime.o: runtime.cc defs.h ../plib/plib.h ../plib/arg.h \
 ../plib/barrier.h ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h \
 ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
 ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
 ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
 ../plib/hash.h ../plib/persist.h ../plib/prime.h ../plib/service.h \
 ../plib/timer.h ../plib/unit.h cli.h vz.h vz_inline.h \
 ../sector/include/sphere.h
builtin_vz.o: builtin_vz.cc defs.h ../plib/plib.h ../plib/arg.h \
 ../plib/barrier.h ../plib/config.h ../plib/stat.h ../plib/dlmalloc.h \
 ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
 ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
 ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
 ../plib/hash.h ../plib/persist.h ../plib/prime.h ../plib/service.h \
 ../plib/timer.h ../plib/unit.h cli.h vz.h vz_inline.h COPYRIGHT.i \
 LICENSE.i
cat.o: cat.cc

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
