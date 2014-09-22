/*****************************************************************************
Copyright (c) 2005 - 2010, The Board of Trustees of the University of Illinois.
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

#include <string.h>
#include <common.h>
#include "fscache.h"

using namespace std;

Cache::Cache():
m_llCacheSize(0),
m_llMaxCacheSize(10000000),
m_llMaxCacheTime(10000000)
{
#ifndef WIN32
   pthread_mutex_init(&m_Lock, NULL);
#else
   m_Lock = CreateMutex(NULL, false, NULL);
#endif
}

Cache::~Cache()
{
#ifndef WIN32
   pthread_mutex_destroy(&m_Lock);
#else
   CloseHandle(m_Lock);
#endif
}

int Cache::setMaxCacheSize(const int64_t ms)
{
   m_llMaxCacheSize = ms;
   return 0;
}

int Cache::setMaxCacheTime(const int64_t mt)
{
   m_llMaxCacheTime = mt;
   return 0;
}

void Cache::update(const std::string& path, const int64_t& ts, const int64_t& size, bool first)
{
   CGuard sg(m_Lock);

   map<string, InfoBlock>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
   {
      InfoBlock r;
      r.m_iCount = 1;
      r.m_bChange = false;
      r.m_llTimeStamp = ts;
      r.m_llSize = size;
      r.m_llLastAccessTime = CTimer::getTime();
      m_mOpenedFiles[path] = r;

      return;
   }

   // the file has been changed by others, remove all cache data
   if ((s->second.m_llTimeStamp != ts) || (s->second.m_llSize != size))
   {
      map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);
      if (c != m_mCacheBlocks.end())
      {
         for (list<CacheBlock>::iterator i = c->second.begin(); i != c->second.end(); ++ i)
            delete [] i->m_pcBlock;
         m_mCacheBlocks.erase(c);
      }

      s->second.m_bChange = true;
      s->second.m_llTimeStamp = ts;
      s->second.m_llSize = size;
      s->second.m_llLastAccessTime = CTimer::getTime();
   }

   if (first)
      s->second.m_iCount ++;
}

void Cache::remove(const std::string& path)
{
   CGuard sg(m_Lock);

   map<string, InfoBlock>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
      return;

   if (-- s->second.m_iCount == 0)
   {
      if (m_mCacheBlocks.find(path) == m_mCacheBlocks.end())
      m_mOpenedFiles.erase(s);
   }
}

int Cache::stat(const string& path, SNode& attr)
{
   CGuard sg(m_Lock);

   map<string, InfoBlock>::iterator s = m_mOpenedFiles.find(path);

   if (s == m_mOpenedFiles.end())
      return -1;

   if (!s->second.m_bChange)
      return 0;

   attr.m_llTimeStamp = s->second.m_llTimeStamp;
   attr.m_llSize = s->second.m_llSize;

   return 1;
}

int Cache::insert(char* block, const std::string& path, const int64_t& offset, const int64_t& size)
{
   CGuard sg(m_Lock);

   map<string, InfoBlock>::iterator s = m_mOpenedFiles.find(path);
   if (s == m_mOpenedFiles.end())
      return -1;

   s->second.m_llLastAccessTime = CTimer::getTime();

   CacheBlock cb;
   cb.m_llOffset = offset;
   cb.m_llSize = size;
   cb.m_llCreateTime = CTimer::getTime();
   cb.m_llLastAccessTime = CTimer::getTime();
   cb.m_pcBlock = block;

   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);

   if (c == m_mCacheBlocks.end())
      m_mCacheBlocks[path].push_front(cb);
   else
      c->second.push_back(cb);

   m_llCacheSize += cb.m_llSize;
   shrink();

   return 0;
}

int64_t Cache::read(const std::string& path, char* buf, const int64_t& offset, const int64_t& size)
{
   CGuard sg(m_Lock);

   map<string, InfoBlock>::iterator s = m_mOpenedFiles.find(path);
   if (s == m_mOpenedFiles.end())
      return -1;

   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(path);
   if (c == m_mCacheBlocks.end())
      return 0;

   for (list<CacheBlock>::iterator i = c->second.begin(); i != c->second.end(); ++ i)
   {
      // this condition can be improved to provide finer granularity
      if ((offset >= i->m_llOffset) && (i->m_llSize - (offset - i->m_llOffset) >= size))
      {
         memcpy(buf, i->m_pcBlock + offset - i->m_llOffset, int(size));
         i->m_llLastAccessTime = CTimer::getTime();
         // update the file's last access time; it must be equal to the block's last access time
         s->second.m_llLastAccessTime = i->m_llLastAccessTime;
         return size;
      }
   }

   return 0;
}

int Cache::shrink()
{
   if (m_llCacheSize < m_llMaxCacheSize)
      return 0;

   string last_file = "";
   int64_t latest_time = CTimer::getTime();

   // find the file with the earliest last access time
   for (map<string, InfoBlock>::iterator i = m_mOpenedFiles.begin(); i != m_mOpenedFiles.end(); ++ i)
   {
      if (i->second.m_llLastAccessTime < latest_time)
      {
         map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(i->first);
         if ((c != m_mCacheBlocks.end()) && !c->second.empty())
         {
            last_file = i->first;
            latest_time = i->second.m_llLastAccessTime;
         }
      }
   }

   // find the block with the earliest lass access time
   // currently we assume all blocks have equal size, so removing one block is enough for a new block
   map<string, list<CacheBlock> >::iterator c = m_mCacheBlocks.find(last_file);
   latest_time = CTimer::getTime();
   list<CacheBlock>::iterator d = c->second.begin();
   for (list<CacheBlock>::iterator i = c->second.begin(); i != c->second.end(); ++ i)
   {
      if (i->m_llLastAccessTime < latest_time)
      {
         latest_time = i->m_llLastAccessTime;
         d = i;
      }
   }

   delete [] d->m_pcBlock;
   d->m_pcBlock = NULL;
   m_llCacheSize -= d->m_llSize;
   c->second.erase(d);

   return 0;
}
