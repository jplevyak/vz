/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/

#include "defs.h"

#include "common/properties.h"
#include "libkfsIO/NetManager.h"
#include "libkfsIO/Globals.h"

#include "NetDispatch.h"
#include "startup.h"
#include "chunk/ChunkServer.h"
#include "chunk/ChunkManager.h"
#include "ChildProcessTracker.h"
#include "LayoutManager.h"
#include "chunk/Logger.h"
#include "libkfsIO/Globals.h"

using namespace KFS;
using namespace KFS::libkfsio;

static vector<string> gChunkDirs;
static string gMD5Sum;
static pthread_t chunkserver;

void init_kfs() {
  if (xmkdir(system_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make system directory '%s' : %d %s", system_directory, errno, strerror(errno));
  if (xmkdir(metadata_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make checkpoint directory '%s' :  %d %s", checkpoint_directory, errno, strerror(errno));;
  if (xmkdir(data_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make chunks directory '%s' :  %d %s", chunks_directory, errno, strerror(errno));;
  if (xmkdir(chunks_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make chunks directory '%s' :  %d %s", chunks_directory, errno, strerror(errno));;
#ifdef DEBUG
  char filename[512];
  strcpy(filename, system_directory); strcat(filename, "vz_k.log");
  KFS::MsgLogger::Init(filename);
#else
  KFS::MsgLogger::Init(NULL);
#endif
  libkfsio::InitGlobals();
}

void start_kfs_metaserver() {
  KFS_LOG_INFO("starting metaserver...");

  kfs_startup(system_directory, checkpoint_directory, 1, replicas, false);
  meta::setClusterKey(get_string_config_default("clusterkey", "clusterkey"));
  meta::setMD5SumFn("");
  meta::setMaxReplicasPerFile(replicas);
  meta::setChunkmapDumpDir(system_directory);

  // reuse thread from chunkserver
  meta::gNetDispatch.Start(CLIENT_PORT(base_port), META_PORT(base_port), false);
}

void* chunkserver_thread(void *) {
  chunk::gChunkServer.MainLoop(DATA_PORT(base_port), local_ip);
  return 0;
}

static bool
make_if_needed(const char *dirname, bool check) {
  struct stat s;
  int res = stat(dirname, &s);
  if (check && (res < 0)) {
    // stat failed; maybe the drive is down.  Keep going
    return true;
  }
  if ((res == 0) && S_ISDIR(s.st_mode))
    return true;
  return mkdir(dirname, 0755) == 0;
}

void setup_chunk_dirs() {
  string::size_type curr = 0, next;
  string chunkDirPaths = chunks_directory;
  while (curr < chunkDirPaths.size()) {
    string component;
    next = chunkDirPaths.find(' ', curr);
    if (next == string::npos)
      next = chunkDirPaths.size();
    component.assign(chunkDirPaths, curr, next - curr);
    curr = next + 1;
    if ((component == " ") || (component == ""))
      continue;
    if (!make_if_needed(component.c_str(), true))
      fail("unable to create %s", component.c_str());
    // also, make the directory for holding stale chunks in each "partition"
    string staleChunkDir = GetStaleChunkPath(component);
    // if the parent dir exists, make the stale chunks directory
    make_if_needed(staleChunkDir.c_str(), false);
    gChunkDirs.push_back(component);
  }
}

#if defined(__sun__)
static void
computeMD5(const char *pathname)
{
}
#else

#define MD5_DIGEST_LENGTH 16

static void
computeMD5(const char *pathname) {
  struct stat s;
  unsigned char md5sum[MD5_DIGEST_LENGTH];

  if (stat(pathname, &s) != 0)
    return;
    
  int fd = open(pathname, O_RDONLY);
  char *buf = (char *) mmap(0, s.st_size, PROT_EXEC, MAP_SHARED, fd, 0);
  if (buf != NULL) {
    MD5Data(md5sum, (uint8*)buf, s.st_size);
    munmap(buf, s.st_size);
    char md5digest[2 * MD5_DIGEST_LENGTH + 1];
    md5digest[2 * MD5_DIGEST_LENGTH] = '\0';
    for (uint32_t i = 0; i < MD5_DIGEST_LENGTH; i++)
      sprintf(md5digest + i * 2, "%02x", md5sum[i]);
    gMD5Sum = md5digest;
    KFS_LOG_VA_DEBUG("md5sum calculated from binary: %s", gMD5Sum.c_str());
  }
  close(fd);
}
#endif

void start_kfs_chunkserver() {
  ServerLocation gMetaServerLoc;
  Properties gProp;

  KFS_LOG_INFO("starting chunkserver...");
  gMetaServerLoc.hostname = cluster_name[0] ? cluster_name : local_ip;
  gMetaServerLoc.port = META_PORT(base_port);
  setup_chunk_dirs();
  computeMD5(program_name);
  chunk::gChunkServer.Init();
  gChunkManager.Init(gChunkDirs, get_int64_config(10000000000ll, "totalspace"), gProp);
  KFS::chunk::gLogger.Init(system_directory);
  if (gMD5Sum == "")
    gMD5Sum = get_string_config_default("", "md5sum");
  chunk::gClientManager.SetTimeouts(get_int_config(5*60, "iotimeout"),get_int_config(30*60, "idletimeout"));
  gMetaServerSM.SetMetaInfo(gMetaServerLoc, get_string_config_default("clusterkey", "clusterkey"), 
                            get_int_config(-1, "rackid"), gMD5Sum, gProp);
  RemoteSyncSM::SetResponseTimeoutSec(get_int_config(RemoteSyncSM::GetResponseTimeoutSec(), "responsetimeout"));
  if (get_int_config("cleanup_on_startup"))
    gChunkManager.Restart();
  //globalNetManager().mTimeoutMs = 10;
  pthread_create(&chunkserver, NULL, chunkserver_thread, 0);
}

void stop_kfs() {
  globalNetManager().Shutdown();
  void *res = 0;
  if (pthread_join(chunkserver, &res) < 0)
    perror("join");
}

// functionality of kfsclean.sh: logcompactor metabkup kfsprune.py
void start_kfs_cleaner() {
}
