/*****************************************************************************
Copyright (c) 2005 - 2009, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 04/23/2010
*****************************************************************************/

#ifndef __SECTOR_FS_FUSE_H__
#define __SECTOR_FS_FUSE_H__

#define FUSE_USE_VERSION 26

#include <sector.h>
#include <conf.h>
#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string>
#include <map>

struct FileTracker
{
   std::string m_strName;
   int m_iCount;

   SectorFile* m_pHandle;
};

class SectorFS
{
public:
   static void* init(struct fuse_conn_info *conn);
   static void destroy(void *);

   static int getattr(const char *, struct stat *);
   static int fgetattr(const char *, struct stat *, struct fuse_file_info *);
   static int mknod(const char *, mode_t, dev_t);
   static int mkdir(const char *, mode_t);
   static int unlink(const char *);
   static int rmdir(const char *);
   static int rename(const char *, const char *);
   static int statfs(const char *, struct statvfs *);
   static int utime(const char *, struct utimbuf *);
   static int utimens(const char *, const struct timespec tv[2]);
   static int opendir(const char *, struct fuse_file_info *);
   static int readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
   static int releasedir(const char *, struct fuse_file_info *);
   static int fsyncdir(const char *, int, struct fuse_file_info *);

   //static int readlink(const char *, char *, size_t);
   //static int symlink(const char *, const char *);
   //static int link(const char *, const char *);

   static int chmod(const char *, mode_t);
   static int chown(const char *, uid_t, gid_t);

   static int create(const char *, mode_t, struct fuse_file_info *);
   static int truncate(const char *, off_t);
   static int ftruncate(const char *, off_t, struct fuse_file_info *);
   static int open(const char *, struct fuse_file_info *);
   static int read(const char *, char *, size_t, off_t, struct fuse_file_info *);
   static int write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
   static int flush(const char *, struct fuse_file_info *);
   static int fsync(const char *, int, struct fuse_file_info *);
   static int release(const char *, struct fuse_file_info *);

   //static int setxattr(const char *, const char *, const char *, size_t, int);
   //static int getxattr(const char *, const char *, char *, size_t);
   //static int listxattr(const char *, char *, size_t);
   //static int removexattr(const char *, const char *);

   static int access(const char *, int);

   static int lock(const char *, struct fuse_file_info *, int cmd, struct flock *);

public:
   static Sector g_SectorClient;
   static Session g_SectorConfig;

private:
   static std::map<std::string, FileTracker*> m_mOpenFileList;
   static pthread_mutex_t m_OpenFileLock;

private:
   static int translateErr(int sferr);
};

#endif
