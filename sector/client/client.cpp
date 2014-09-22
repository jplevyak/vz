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
   Yunhong Gu, last updated 07/05/2010
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif
#include <ssltransport.h>
#include <tcptransport.h>
#include <crypto.h>
#include <common.h>
#include "client.h"
#include <iostream>

using namespace std;

Client::Client():
m_strUsername(""),
m_strPassword(""),
m_strCert(""),
m_strServerHost(""),
m_strServerIP(""),
m_iKey(0),
m_iCount(0),
m_bActive(false),
m_iID(0)
{
#ifndef WIN32
   pthread_mutex_init(&m_MasterSetLock, NULL);
   pthread_mutex_init(&m_KALock, NULL);
   pthread_cond_init(&m_KACond, NULL);
   pthread_mutex_init(&m_IDLock, NULL);
#else
   m_MasterSetLock = CreateMutex(NULL, false, NULL);
   m_KALock = CreateMutex(NULL, false, NULL);
   m_KACond = CreateEvent(NULL, false, false, NULL);
   m_IDLock = CreateMutex(NULL, false, NULL);
#endif
}

Client::~Client()
{
#ifndef WIN32
   pthread_mutex_destroy(&m_MasterSetLock);
   pthread_mutex_destroy(&m_KALock);
   pthread_cond_destroy(&m_KACond);
   pthread_mutex_destroy(&m_IDLock);
#else
   CloseHandle(m_MasterSetLock);
   CloseHandle(m_KALock);
   CloseHandle(m_KACond);
   CloseHandle(m_IDLock);
#endif
}

int Client::init(const string& server, const int& port)
{
   if (m_iCount ++ > 0)
      return 0;

#ifdef WIN32
    WSADATA wsaData = {0};
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) 
    {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }
#endif

   m_ErrorInfo.init();

   struct addrinfo* result;
   if (getaddrinfo(server.c_str(), NULL, NULL, &result) != 0)
   {
#ifdef DEBUG
      cerr << "incorrect host name.\n";
#endif
      return -1;
   }

   m_strServerHost = server;

   char hostip[NI_MAXHOST];
   getnameinfo((sockaddr *)result->ai_addr, result->ai_addrlen, hostip, sizeof(hostip), NULL, 0, NI_NUMERICHOST);
   m_strServerIP = hostip;
   freeaddrinfo(result);

   m_iServerPort = port;

   Crypto::generateKey(m_pcCryptoKey, m_pcCryptoIV);

   Transport::initialize();
   if (m_GMP.init(0) < 0)
   {
#ifdef DEBUG
      cerr << "unable to init GMP.\n ";
#endif
      return -1;
   }

   int dataport = 0;
   if (m_DataChn.init("", dataport) < 0)
   {
#ifdef DEBUG
      cerr << "unable to init data channel.\n";
#endif
      return -1;
   }

   m_bActive = true;
#ifndef WIN32
   pthread_create(&m_KeepAlive, NULL, keepAlive, this);
#else
   m_KeepAlive = CreateThread(NULL, 0, keepAlive, this, 0, NULL);
#endif

   return 0;
}

int Client::login(const string& username, const string& password, const char* cert,
                  int default_asn1_cert_len, const unsigned char *default_asn1_cert)
{
   if (m_iKey > 0)
      return m_iKey;

   string master_cert;
   if ((cert != NULL) && (0 != strlen(cert)))
      master_cert = cert;
   else if (retrieveMasterInfo(master_cert) < 0)
      return -1;

   SSLTransport::init();

   int result;
   SSLTransport secconn;
   if ((result = secconn.initClientCTX(master_cert.c_str())) < 0) {
     if (default_asn1_cert) {
       if ((result = secconn.initClientCTX_ASN1(default_asn1_cert_len, default_asn1_cert)) < 0)
         return result;
     } else
       return result;
   }
   if ((result = secconn.open(NULL, 0)) < 0)
      return result;

   if ((result = secconn.connect(m_strServerHost.c_str(), m_iServerPort)) < 0)
   {
#ifdef DEBUG
      cerr << "cannot set up secure connection to the master.\n";
#endif
      return result;
   }

   int cmd = 2;
   secconn.send((char*)&cmd, 4);

   // send username and password
   char buf[128];
   strncpy(buf, username.c_str(), 64);
   secconn.send(buf, 64);
   strncpy(buf, password.c_str(), 128);
   secconn.send(buf, 128);

   secconn.send((char*)&m_iKey, 4);
   secconn.recv((char*)&m_iKey, 4);
   if (m_iKey < 0)
      return SectorError::E_SECURITY;

   int32_t port = m_GMP.getPort();
   secconn.send((char*)&port, 4);
   port = m_DataChn.getPort();
   secconn.send((char*)&port, 4);

   // send encryption key/iv
   secconn.send((char*)m_pcCryptoKey, 16);
   secconn.send((char*)m_pcCryptoIV, 8);

   int size = 0;
   secconn.recv((char*)&size, 4);
   if (size > 0)
   {
      char* tmp = new char[size];
      secconn.recv(tmp, size);
      m_Topology.deserialize(tmp, size);
   }

   Address addr;
   int key = 0;
   secconn.recv((char*)&key, 4);
   addr.m_strIP = m_strServerIP;
   addr.m_iPort = m_iServerPort;
   m_Routing.insert(key, addr);

   CGuard::enterCS(m_MasterSetLock);
   m_sMasters.insert(addr);
   CGuard::leaveCS(m_MasterSetLock);

   int num;
   secconn.recv((char*)&num, 4);
   for (int i = 0; i < num; ++ i)
   {
      char ip[64];
      int size = 0;
      secconn.recv((char*)&key, 4);
      secconn.recv((char*)&size, 4);
      secconn.recv(ip, size);
      addr.m_strIP = ip;
      secconn.recv((char*)&addr.m_iPort, 4);
      m_Routing.insert(key, addr);
   }

   int32_t tmp;
   secconn.recv((char*)&tmp, 4);

   secconn.close();
   SSLTransport::destroy();

   m_strUsername = username;
   m_strPassword = password;
   m_strCert = master_cert;

   return m_iKey;
}

int Client::login(const string& serv_ip, const int& serv_port)
{
   Address addr;
   addr.m_strIP = serv_ip;
   addr.m_iPort = serv_port;

   CGuard::enterCS(m_MasterSetLock);
   if (m_sMasters.find(addr) != m_sMasters.end())
   {
      CGuard::leaveCS(m_MasterSetLock);
      return 0;
   }
   CGuard::leaveCS(m_MasterSetLock);

   if (m_iKey < 0)
      return -1;

   SSLTransport::init();

   int result;
   SSLTransport secconn;
   if ((result = secconn.initClientCTX(m_strCert.c_str())) < 0)
      return result;
   if ((result = secconn.open(NULL, 0)) < 0)
      return result;

   if ((result = secconn.connect(serv_ip.c_str(), serv_port)) < 0)
   {
#ifdef DEBUG
      cerr << "cannot set up secure connection to the master.\n";
#endif
      return result;
   }

   int cmd = 2;
   secconn.send((char*)&cmd, 4);

   // send username and password
   char buf[128];
   strncpy(buf, m_strUsername.c_str(), 64);
   secconn.send(buf, 64);
   strncpy(buf, m_strPassword.c_str(), 128);
   secconn.send(buf, 128);

   secconn.send((char*)&m_iKey, 4);
   int32_t key = -1;
   secconn.recv((char*)&key, 4);
   if (key < 0)
      return SectorError::E_SECURITY;

   int32_t port = m_GMP.getPort();
   secconn.send((char*)&port, 4);
   port = m_DataChn.getPort();
   secconn.send((char*)&port, 4);

   // send encryption key/iv
   secconn.send((char*)m_pcCryptoKey, 16);
   secconn.send((char*)m_pcCryptoIV, 8);

   int32_t tmp;
   secconn.recv((char*)&tmp, 4);

   secconn.close();
   SSLTransport::destroy();

   CGuard::enterCS(m_MasterSetLock);
   m_sMasters.insert(addr);
   CGuard::leaveCS(m_MasterSetLock);

   return 0;
}

int Client::logout()
{
   CGuard::enterCS(m_MasterSetLock);
   for (set<Address, AddrComp>::iterator i = m_sMasters.begin(); i != m_sMasters.end(); ++ i)
   {
      SectorMsg msg;
      msg.setKey(m_iKey);
      msg.setType(2);
      msg.m_iDataLength = SectorMsg::m_iHdrSize;
      m_GMP.rpc(i->m_strIP.c_str(), i->m_iPort, &msg, &msg);
   }
   m_sMasters.clear();
   CGuard::leaveCS(m_MasterSetLock);

   m_iKey = 0;
   return 0;
}

int Client::close()
{
   if (-- m_iCount == 0)
   {
      if (m_iKey > 0)
         logout();

#ifndef WIN32
      pthread_mutex_lock(&m_KALock);
      m_bActive = false;
      pthread_cond_signal(&m_KACond);
      pthread_mutex_unlock(&m_KALock);
      pthread_join(m_KeepAlive, NULL);
#else
      m_bActive = false;
      SetEvent(m_KACond);
      WaitForSingleObject(m_KeepAlive, INFINITE);
#endif

      m_strServerHost = "";
      m_strServerIP = "";
      m_iServerPort = 0;
      m_GMP.close();
      Transport::release();

#ifdef WIN32
      WSACleanup();
#endif
   }

   return 0;
}

int Client::list(const string& path, vector<SNode>& attr)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.resize(65536);
   msg.setType(101);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   if (lookup(revised_path, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   string filelist = msg.getData();

   unsigned int s = 0;
   while (s < filelist.length())
   {
      int t = filelist.find('\02', s);
      SNode sn;
      sn.deserialize(filelist.substr(s, t - s).c_str());
      attr.insert(attr.end(), sn);
      s = t + 1;
   }

   return attr.size();
}

int Client::stat(const string& path, SNode& attr)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.resize(65536);
   msg.setType(102);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   if (lookup(revised_path, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   attr.deserialize(msg.getData());

   int n = (msg.m_iDataLength - SectorMsg::m_iHdrSize - 128) / 68;
   char* al = msg.getData() + 128;

   for (int i = 0; i < n; ++ i)
   {
      Address addr;
      addr.m_strIP = al + 68 * i;
      addr.m_iPort = *(int32_t*)(al + 68 * i + 64);
      attr.m_sLocation.insert(addr);
   }

   // check local cache: updated files may not be sent to the master yet
   m_Cache.stat(path, attr);

   return 0;
}

int Client::mkdir(const string& path)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(103);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   if (lookup(revised_path, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::move(const string& oldpath, const string& newpath)
{
   string src = Metadata::revisePath(oldpath);
   string dst = Metadata::revisePath(newpath);

   SectorMsg msg;
   msg.setType(104);
   msg.setKey(m_iKey);

   int32_t size = src.length() + 1;
   msg.setData(0, (char*)&size, 4);
   msg.setData(4, src.c_str(), src.length() + 1);
   size = dst.length() + 1;
   msg.setData(4 + src.length() + 1, (char*)&size, 4);
   msg.setData(4 + src.length() + 1 + 4, dst.c_str(), dst.length() + 1);

   Address serv;
   if (lookup(src, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::remove(const string& path)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(105);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);

   Address serv;
   if (lookup(revised_path, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::rmr(const string& path)
{
   SNode attr;
   int r = stat(path.c_str(), attr);
   if (r < 0)
      return r;

   if (attr.m_bIsDir)
   {
      vector<SNode> subdir;
      list(path, subdir);

      for (vector<SNode>::iterator i = subdir.begin(); i != subdir.end(); ++ i)
      {
         if (i->m_bIsDir)
            rmr(path + "/" + i->m_strName);
         else
            remove(path + "/" + i->m_strName);
      }
   }

   return remove(path);
}

int Client::copy(const string& src, const string& dst)
{
   string rsrc = Metadata::revisePath(src);
   string rdst = Metadata::revisePath(dst);

   SectorMsg msg;
   msg.setType(106);
   msg.setKey(m_iKey);

   int32_t size = rsrc.length() + 1;
   msg.setData(0, (char*)&size, 4);
   msg.setData(4, rsrc.c_str(), rsrc.length() + 1);
   size = rdst.length() + 1;
   msg.setData(4 + rsrc.length() + 1, (char*)&size, 4);
   msg.setData(4 + rsrc.length() + 1 + 4, rdst.c_str(), rdst.length() + 1);

   Address serv;
   if (lookup(rsrc, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::utime(const string& path, const int64_t& ts)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(107);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);
   msg.setData(revised_path.length() + 1, (char*)&ts, 8);

   Address serv;
   if (lookup(revised_path, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::settype(const string& path, const string& newtype)
{
   string revised_path = Metadata::revisePath(path);

   SectorMsg msg;
   msg.setType(108);
   msg.setKey(m_iKey);
   msg.setData(0, revised_path.c_str(), revised_path.length() + 1);
   msg.setData(revised_path.length() + 1, newtype.c_str(), newtype.length() + 1);

   Address serv;
   m_Routing.lookup(revised_path, serv);
   login(serv.m_strIP, serv.m_iPort);

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   return 0;
}

int Client::sysinfo(SysStat& sys)
{
   SectorMsg msg;
   msg.setKey(m_iKey);
   msg.setType(3);
   msg.m_iDataLength = SectorMsg::m_iHdrSize;

   Address serv;
   if (lookup(m_iKey, serv) < 0)
      return SectorError::E_CONNECTION;

   if (m_GMP.rpc(serv.m_strIP.c_str(), serv.m_iPort, &msg, &msg) < 0)
      return SectorError::E_CONNECTION;

   if (msg.getType() < 0)
      return *(int32_t*)(msg.getData());

   deserializeSysStat(sys, msg.getData(), msg.m_iDataLength);

   for (vector<Address>::iterator i = sys.m_vMasterList.begin(); i != sys.m_vMasterList.end(); ++ i)
   {
      if (i->m_strIP.length() == 0)
      {
         i->m_strIP = serv.m_strIP;
         break;
      }
   }

   return 0;
}

int Client::setMaxCacheSize(const int64_t ms)
{
   return m_Cache.setMaxCacheSize(ms);
}

int Client::updateMasters()
{
   SectorMsg msg;
   msg.setKey(m_iKey);

   map<uint32_t, Address> al;
   m_Routing.getListOfMasters(al);
   for (map<uint32_t, Address>::iterator i = al.begin(); i != al.end(); ++ i)
   {
      msg.setType(5);

      if (m_GMP.rpc(i->second.m_strIP.c_str(), i->second.m_iPort, &msg, &msg) >= 0)
      {
         Address addr;
         addr.m_strIP = i->second.m_strIP;
         addr.m_iPort = i->second.m_iPort;
         uint32_t key = i->first;
         
         m_Routing.init();
         m_Routing.insert(key, addr);

         int n = *(int32_t*)msg.getData();
         int p = 4;
         for (int m = 0; m < n; ++ m)
         {
            key = *(int32_t*)(msg.getData() + p);
            p += 4;
            addr.m_strIP = msg.getData() + p;
            p += addr.m_strIP.length() + 1;
            addr.m_iPort = *(int32_t*)(msg.getData() + p);
            p += 4;

            m_Routing.insert(key, addr);
         }

         return n + 1;
      }
   }

   return -1;
}

#ifndef WIN32
void* Client::keepAlive(void* param)
#else
DWORD WINAPI Client::keepAlive(LPVOID param)
#endif
{
   Client* self = (Client*)param;

   while (self->m_bActive)
   {
#ifndef WIN32
      timeval t;
      gettimeofday(&t, NULL);
      timespec ts;
      ts.tv_sec  = t.tv_sec + 60 * 10;
      ts.tv_nsec = t.tv_usec * 1000;

      pthread_mutex_lock(&self->m_KALock);
      pthread_cond_timedwait(&self->m_KACond, &self->m_KALock, &ts);
      pthread_mutex_unlock(&self->m_KALock);
#else
      WaitForSingleObject(self->m_KACond, 600000);
#endif

      if (!self->m_bActive)
         break;

      vector<Address> ml;
	  CGuard::enterCS(self->m_MasterSetLock);
      for (set<Address, AddrComp>::iterator i = self->m_sMasters.begin(); i != self->m_sMasters.end(); ++ i)
         ml.push_back(*i);
	  CGuard::leaveCS(self->m_MasterSetLock);

      for (vector<Address>::iterator i = ml.begin(); i != ml.end(); ++ i)
      {
         // send keep-alive msg to each logged in master
         SectorMsg msg;
         msg.setKey(self->m_iKey);
         msg.setType(6);
         msg.m_iDataLength = SectorMsg::m_iHdrSize;
         self->m_GMP.rpc(i->m_strIP.c_str(), i->m_iPort, &msg, &msg);
      }
   }

#ifndef WIN32
   return NULL;
#else
   return 0;
#endif
}

int Client::deserializeSysStat(SysStat& sys, char* buf, int size)
{
   if (size < 52)
      return -1;

   sys.m_llStartTime = *(int64_t*)buf;
   sys.m_llAvailDiskSpace = *(int64_t*)(buf + 8);
   sys.m_llTotalFileSize = *(int64_t*)(buf + 16);
   sys.m_llTotalFileNum = *(int64_t*)(buf + 24);
   sys.m_llTotalSlaves = *(int64_t*)(buf + 32);

   char* p = buf + 40;
   int c = *(int32_t*)p;
   sys.m_vCluster.resize(c);
   p += 4;
   for (vector<SysStat::ClusterStat>::iterator i = sys.m_vCluster.begin(); i != sys.m_vCluster.end(); ++ i)
   {
      i->m_iClusterID = *(int32_t*)p;
      i->m_iTotalNodes = *(int32_t*)(p + 4);
      i->m_llAvailDiskSpace = *(int64_t*)(p + 8);
      i->m_llTotalFileSize = *(int64_t*)(p + 16);
      i->m_llTotalInputData = *(int64_t*)(p + 24);
      i->m_llTotalOutputData = *(int64_t*)(p + 32);

      p += 40;
   }

   int m = *(int32_t*)p;
   p += 4;
   sys.m_vMasterList.resize(m);
   for (vector<Address>::iterator i = sys.m_vMasterList.begin(); i != sys.m_vMasterList.end(); ++ i)
   {
      i->m_strIP = p;
      p += 16;
      i->m_iPort = *(int32_t*)p;
      p += 4;
   }

   int n = *(int32_t*)p;
   p += 4;
   sys.m_vSlaveList.resize(n);
   for (vector<SysStat::SlaveStat>::iterator i = sys.m_vSlaveList.begin(); i != sys.m_vSlaveList.end(); ++ i)
   {
      i->m_strIP = p;
      i->m_llAvailDiskSpace = *(int64_t*)(p + 16);
      i->m_llTotalFileSize = *(int64_t*)(p + 24);
      i->m_llCurrMemUsed = *(int64_t*)(p + 32);
      i->m_llCurrCPUUsed = *(int64_t*)(p + 40);
      i->m_llTotalInputData = *(int64_t*)(p + 48);
      i->m_llTotalOutputData = *(int64_t*)(p + 56);
      i->m_llTimeStamp = *(int64_t*)(p + 64);

      p += 72;
   }

   return 0;
}

int Client::lookup(const string& path, Address& serv_addr)
{
   if (m_Routing.lookup(path, serv_addr) < 0)
      return -1;

   if (login(serv_addr.m_strIP, serv_addr.m_iPort) < 0)
   {
      if (updateMasters() < 0)
         return -1;

       m_Routing.lookup(path, serv_addr);
       return login(serv_addr.m_strIP, serv_addr.m_iPort);
   }

   return 0;
}

int Client::lookup(const int32_t& key, Address& serv_addr)
{
   if (m_Routing.lookup(key, serv_addr) < 0)
      return -1;

   if (login(serv_addr.m_strIP, serv_addr.m_iPort) < 0)
   {
      if (updateMasters() < 0)
         return -1;

       m_Routing.lookup(key, serv_addr);
       return login(serv_addr.m_strIP, serv_addr.m_iPort);
   }

   return 0;
}

int Client::retrieveMasterInfo(string& certfile)
{
   TCPTransport t;
   t.open(NULL, 0);
   if (t.connect(m_strServerIP.c_str(), m_iServerPort - 1) < 0)
      return -1;

   certfile = "";
#ifndef WIN32
   certfile = "/tmp/master_node.cert";
#else
   certfile = "master_node.cert";
#endif

   int32_t size = 0;
   t.recv((char*)&size, 4);
   int64_t recvsize = t.recvfile(certfile.c_str(), 0, size);
   t.close();

   if (recvsize <= 0)
      return -1;

   return 0;
}
