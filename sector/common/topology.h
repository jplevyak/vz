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
   Yunhong Gu, last updated 01/02/2010
*****************************************************************************/


#ifndef __SECTOR_TOPOLOGY_H__
#define __SECTOR_TOPOLOGY_H__

#include <string>
#include <set>
#include <map>
#include <vector>
#include <sector.h>

#ifndef WIN32
   #include <stdint.h>
#endif

class SlaveNode
{
public:
   SlaveNode();

public:
   int m_iNodeID;					// unique slave node ID

   //Address m_Addr;
   std::string m_strIP;					// ip address (used for internal communication)
   std::string m_strPublicIP;				// public ip address, used for client communication. currently NOT used
   int m_iPort;						// GMP control port number
   int m_iDataPort;					// UDT data channel port number

   std::string m_strStoragePath;			// data storage path on the local file system

   int64_t m_llAvailDiskSpace;				// available disk space
   int64_t m_llTotalFileSize;				// total data size

   int64_t m_llTimeStamp;				// last statistics refresh time
   int64_t m_llCurrMemUsed;				// physical memory used by the slave
   int64_t m_llCurrCPUUsed;				// CPU percentage used by the slave
   int64_t m_llTotalInputData;				// total network input
   int64_t m_llTotalOutputData;				// total network output
   std::map<std::string, int64_t> m_mSysIndInput;	// network input from each other slave
   std::map<std::string, int64_t> m_mSysIndOutput;	// network outout to each other slave
   std::map<std::string, int64_t> m_mCliIndInput;	// network input from each client
   std::map<std::string, int64_t> m_mCliIndOutput;	// network output to each client

   int64_t m_llLastUpdateTime;				// last update time
   int m_iRetryNum;					// number of retries since lost communication
   int m_iStatus;					// 0: inactive 1: active-normal 2: active-disk full

   std::set<int> m_sBadVote;				// set of bad votes by other slaves
   int64_t m_llLastVoteTime;				// timestamp of last vote

   std::vector<int> m_viPath;				// topology path, from root to this node on the tree structure

public:
   int deserialize(const char* buf, int size);
};

struct Cluster
{
   int m_iClusterID;
   std::vector<int> m_viPath;

   int m_iTotalNodes;
   int64_t m_llAvailDiskSpace;
   int64_t m_llTotalFileSize;

   int64_t m_llTotalInputData;
   int64_t m_llTotalOutputData;
   std::map<std::string, int64_t> m_mSysIndInput;
   std::map<std::string, int64_t> m_mSysIndOutput;
   std::map<std::string, int64_t> m_mCliIndInput;
   std::map<std::string, int64_t> m_mCliIndOutput;

   std::map<int, Cluster> m_mSubCluster;
   std::set<int> m_sNodes;
};

class Topology
{
friend class SlaveManager;

public:
   Topology();
   ~Topology();

public:
   int init(const char* topoconf);
   int xdcs_init();
   int lookup(const char* ip, std::vector<int>& path);

   unsigned int match(std::vector<int>& p1, std::vector<int>& p2);

      // Functionality:
      //    compute the distance between two IP addresses.
      // Parameters:
      //    0) [in] ip1: first IP address
      //    1) [in] ip2: second IP address
      // Returned value:
      //    0 if ip1 = ip2, 1 if on the same rack, etc.

   unsigned int distance(const char* ip1, const char* ip2);
   unsigned int distance(const Address& addr, const std::set<Address, AddrComp>& loclist);

   int getTopoDataSize();
   int serialize(char* buf, int& size);
   int deserialize(const char* buf, const int& size);

private:
   int parseIPRange(const char* ip, uint32_t& digit, uint32_t& mask);
   int parseTopo(const char* topo, std::vector<int>& tm);

private:
   unsigned int m_uiLevel;

   struct TopoMap
   {
      uint32_t m_uiIP;
      uint32_t m_uiMask;
      std::vector<int> m_viPath;
   };

   std::vector<TopoMap> m_vTopoMap;
};

#endif
