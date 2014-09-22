include ../Makefile.common

ifeq ($(STATIC), 1)
   LDFLAGS += ../lib/libclient.a ../lib/libsecurity.a ../lib/librpc.a ../lib/libcommon.a ../lib/libudt.a
else
   LDFLAGS += -L../lib -lsecurity -lrpc -ludt -lcommon -lclient
endif

all: sector_download sector_upload sector_ls sector_rm sector_mkdir sector_stat sector_sysinfo sector_mv sector_cp sector_pipe sphere_stream

%.o: %.cpp
	$(C++) -fPIC $(CCFLAGS) $< -c

sector_download: download.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_upload: upload.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_ls: ls.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_rm: rm.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_mkdir: mkdir.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_stat: stat.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_sysinfo: sysinfo.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_mv: mv.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_cp: cp.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sector_pipe: pipe.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sphere_stream: stream.cpp probot.o
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

clean:
	rm -f *.o sector_download sector_upload sector_ls sector_rm sector_mkdir sector_stat sector_sysinfo sector_mv sector_cp sector_pipe sphere_stream

install:
	
