/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/

#include "defs.h"
#include <sys/vfs.h>
#include "master.h"
#include "security.h"
#include "filesrc.h"
#include "slave.h"
#include "client.h"
#include "ink_base64.h"

using namespace std;

static SServer *sector_security = 0;
static Master *sector_master = 0;
static Slave *sector_slave = 0;

// default keys for insecure installations

static const char *default_security_node_cert_base64 = 
"MIICxTCCAi6gAwIBAgIJAK3rGmEwOIi4MA0GCSqGSIb3DQEBBQUAMEwxCzAJBgNV"
"BAYTAkdCMRIwEAYDVQQIEwlCZXJrc2hpcmUxEDAOBgNVBAcTB05ld2J1cnkxFzAV"
"BgNVBAoTDk15IENvbXBhbnkgTHRkMB4XDTEwMDMyMjE5NDI1OFoXDTExMDMyMjE5"
"NDI1OFowTDELMAkGA1UEBhMCR0IxEjAQBgNVBAgTCUJlcmtzaGlyZTEQMA4GA1UE"
"BxMHTmV3YnVyeTEXMBUGA1UEChMOTXkgQ29tcGFueSBMdGQwgZ8wDQYJKoZIhvcN"
"AQEBBQADgY0AMIGJAoGBAMeWhjm1xrthuZqZYE2C1vsrc8IRp0EpnO7kNZhDuk2A"
"cYcM3VnFtESTDzIpDksxka43bVdoTZXYLKwfCq/d8ETMdrQEz4e6D9CkIbsTLAKV"
"5M0vAZdtitUdR87WeYOhB2pihyXg42tcTtFDjcU8CZkpSKVTYf6IxLQ3Au0so7FH"
"AgMBAAGjga4wgaswHQYDVR0OBBYEFFF4gLvkh1l0OXlDYaxLgF0a3lFpMHwGA1Ud"
"IwR1MHOAFFF4gLvkh1l0OXlDYaxLgF0a3lFpoVCkTjBMMQswCQYDVQQGEwJHQjES"
"MBAGA1UECBMJQmVya3NoaXJlMRAwDgYDVQQHEwdOZXdidXJ5MRcwFQYDVQQKEw5N"
"eSBDb21wYW55IEx0ZIIJAK3rGmEwOIi4MAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcN"
"AQEFBQADgYEAXkaF1yR0jse4DKsU9beEAZXuSviY7mTMjZqSWdJnz7r/jM+FTBDj"
"NnB80HZ+ZJohPQV7myMVDTJY+VXry5Dsq5JVHhLbwewXfQI/waaM/r9pbZUqtO9M"
"5zSCaWvIn7k9Zo0R763YlMLqME/hL4HD188XIwhC8tr3GT+lXyaj3Uk=";
static uint8 default_security_node_cert[64 * 16];
static int default_security_node_cert_len = 0;

static const char *default_security_node_key_base64 = 
"MIICXgIBAAKBgQDHloY5tca7YbmamWBNgtb7K3PCEadBKZzu5DWYQ7pNgHGHDN1Z"
"xbREkw8yKQ5LMZGuN21XaE2V2CysHwqv3fBEzHa0BM+Hug/QpCG7EywCleTNLwGX"
"bYrVHUfO1nmDoQdqYocl4ONrXE7RQ43FPAmZKUilU2H+iMS0NwLtLKOxRwIDAQAB"
"AoGAFIUjU7eesRFBFeYDArUbCUPUaPWLrqVqnx0TbY45D1LPWUU3VM2A1TQRceTK"
"tWxpP/Iu6betkmRhY2jlnmNBGoMyHEhlGufcaxz4gMmJgoy5ZJXbHzBX7bEOi4Hj"
"U6xMzRvbWZ/rgZCiBJ2dzMCUZirt+0MG8vGRG4Gx5cC3U8ECQQDpU3LGhTKYQp4h"
"bOjyYcHo4agbBNTl+CEleu9Z1A8UNzI8rwcZYr4aXAt8hXamYXKeItkWbEaAlm0F"
"r3xX/SIXAkEA2vvC7w8qUI8nqusCOe4w8AjyMgNoM5CmZ6Th5uubnxuyYsSv8S0v"
"zEKp1AKrgJCQ82VkP+pSavY8JOMARUxYUQJBAMBtGu5QcY6S+lj0uhUTvFk0qXBH"
"BtH2VlfT0EwUEK+KafsUGlW8Uq3olWyHoXfMTDvQ35qXh3mLFbSnqnPihtcCQQCp"
"O9gymC3DhF7LoENgMcxwNPm33Royf5+aibprk7n5BJaf7hFq+djuqrZEuqt/+DmN"
"YNZQ7u4y64kfmK8k3xOBAkEAkplI/4Rdf/HVNiNF9JMjRjWVyADkGf9L/0PBE2dr"
"Rj0z80932tDXlKxenBzIFhOcUeHglaKrKoKbLevYBw6g1A==";
static uint8 default_security_node_key[64 * 16];
static int default_security_node_key_len = 0;

static const char *default_master_node_cert_base64 = 
"MIICxTCCAi6gAwIBAgIJAJEAGNK0qLVlMA0GCSqGSIb3DQEBBQUAMEwxCzAJBgNV"
"BAYTAkdCMRIwEAYDVQQIEwlCZXJrc2hpcmUxEDAOBgNVBAcTB05ld2J1cnkxFzAV"
"BgNVBAoTDk15IENvbXBhbnkgTHRkMB4XDTEwMDMyMjE5NDMwMFoXDTExMDMyMjE5"
"NDMwMFowTDELMAkGA1UEBhMCR0IxEjAQBgNVBAgTCUJlcmtzaGlyZTEQMA4GA1UE"
"BxMHTmV3YnVyeTEXMBUGA1UEChMOTXkgQ29tcGFueSBMdGQwgZ8wDQYJKoZIhvcN"
"AQEBBQADgY0AMIGJAoGBANR/COkm7Ypq76jSi0EQFe/Nlvnqj2XM1eCbDnSf8zED"
"9LD5UX3et4WICc1U0uBqrrU8OmhRODlrUbqAvIokaWVmFddf9oSRl2hhVp71sWq8"
"uh5i3v3pO9ZSfFWw5L6v1sfrugNS6Vs27pSBQG6g5H3evTbJWDzzA/OLOKg47FFN"
"AgMBAAGjga4wgaswHQYDVR0OBBYEFGDXi75QxW7BULEw4DKwe62cYk+2MHwGA1Ud"
"IwR1MHOAFGDXi75QxW7BULEw4DKwe62cYk+2oVCkTjBMMQswCQYDVQQGEwJHQjES"
"MBAGA1UECBMJQmVya3NoaXJlMRAwDgYDVQQHEwdOZXdidXJ5MRcwFQYDVQQKEw5N"
"eSBDb21wYW55IEx0ZIIJAJEAGNK0qLVlMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcN"
"AQEFBQADgYEAoAzahm2z9K/dtxGNb0QtIuWszIf7vUP9JSdI77Ps73MKmVOyPKuG"
"YndmsrS4lsMxINR3G0F1qD7s90Tts0xUv1JsKxk8Dx21KGeoLUFh5Tu4cq6eaXur"
"sm7bJFfAHOQjy69St4Pe6ziyoD1IkTYflWa/IgWNcNMN/a1ol9o0beg=";
uint8 default_master_node_cert[64 * 16];
int default_master_node_cert_len = 0;

static const char *default_master_node_key_base64 = 
"MIICXAIBAAKBgQDUfwjpJu2Kau+o0otBEBXvzZb56o9lzNXgmw50n/MxA/Sw+VF9"
"3reFiAnNVNLgaq61PDpoUTg5a1G6gLyKJGllZhXXX/aEkZdoYVae9bFqvLoeYt79"
"6TvWUnxVsOS+r9bH67oDUulbNu6UgUBuoOR93r02yVg88wPzizioOOxRTQIDAQAB"
"AoGAI8KhKEOB1Dp8zIdqIe/oESwjDTnZGgOntp3BiATm80e8JRBAE5OohNNI+wIc"
"XuH1NtDE74tDj/4sidtiX0kTZ0uHu8pojiSW1XToAzc4Q7jNc+wF1rx/gsTFj3TX"
"0ve9EilradRze1QtaSzj0CB4bWJJxg+wFlotRzHyIzqwSIECQQDvrMYBqwMzCiiK"
"yTsCOMsuwpVIt+zhJxYb72yT6aHMYaMDoH53Jy2mbhwOjaqagGbFhXS90f/YW9Ih"
"IS1EMSLhAkEA4vhYXn7t5zLW4a7V3izQ1MIQBQ8SRvSSzgVxWwTR1SKbNFYSc7Go"
"D0QcwWiskyICp99KbN6+SxuZabB8lbbn7QJAadf1AZuSKCIeUVwYsRx/rkDESH7h"
"H3VxcBjF7HRKvcWIhfuEnf4xTMRno42wf9agjC4ppgJtaBD0tSi/otPIQQJARRzC"
"K+QSpTgB3q7SSNj0rhR5tFCmjBylJz7ZCZIqIvBqukELVVdfiWaAKxSomBTfIicI"
"Dqa040IAMQKgrSYI8QJBALrsFUSEq/7Fow51EV1JgSFNhnlAcohKbcOBGCEcRFT2"
"tXGqhESLTwI++0SiCsuUAySHLnY7H9t9G97QGTTkoLk=";
static uint8 default_master_node_key[64 * 16];
static int default_master_node_key_len = 0;

static void master_conf(MasterConf &c) {
  c.m_iServerPort = base_port;
  c.m_strSecServIP = cluster_name[0] ? cluster_name : local_ip;
  c.m_iSecServPort = SECURITY_PORT(base_port);
  c.m_iMaxActiveUser = INT_MAX;
  c.m_iReplicaNum = replicas;
  c.m_strHomeDir = metadata_directory;
  c.m_MetaType = MEMORY;
}

void* Master::vz_serviceEx(void* param) {
  Master* self = (Master*)param;
  char filename[512];
  SSLTransport secconn;

  strcpy(filename, system_directory); strcat(filename, "security_node.cert");
  if (file_exists(filename)) {
    if (secconn.initClientCTX(filename) < 0)
      fail("unable to load security node certificate");
  } else {
    if (secconn.initClientCTX_ASN1(default_security_node_cert_len, default_security_node_cert) < 0)
      fail("unable to load security node certificate");
  }

  while (true) {
    ServiceJobParam *p = (ServiceJobParam*)self->m_ServiceJobQueue.pop();
    if (!p)
      break;

    int32_t cmd;
    if (p->ssl->recv((char*)&cmd, 4) < 0)
      goto Ldone;

    if (secconn.send((char*)&cmd, 4) < 0) {
      //if the permanent connection to the security server is broken, re-connect
      secconn.open(NULL, 0);
      if (secconn.connect(self->m_SysConfig.m_strSecServIP.c_str(), self->m_SysConfig.m_iSecServPort) < 0)
      {
        cmd = SectorError::E_NOSECSERV;
        goto Ldone;
      }
      secconn.send((char*)&cmd, 4);
    }

    switch (cmd) {
      case 1: // slave node join
        self->processSlaveJoin(*p->ssl, secconn, p->ip);
        break;
      case 2: // user login
        self->processUserJoin(*p->ssl, secconn, p->ip);
        break;
      case 3: // master join
        self->processMasterJoin(*p->ssl, secconn, p->ip);
        break;
    }
  Ldone:
    p->ssl->close();
    delete p;
  }
  secconn.close();
  return NULL;
}

void* Master::vz_service(void *xself) {
  Master *self = (Master*)xself;
  const int ServiceWorker = 4;
  for (int i = 0; i < ServiceWorker; ++ i) {
    pthread_t t;
    pthread_create(&t, NULL, vz_serviceEx, self);
    pthread_detach(t);
  }

  int port = self->m_SysConfig.m_iServerPort;
  SSLTransport serv;
  char fn1[512], fn2[512];
  strcpy(fn1, system_directory); strcat(fn1, "master_node.cert");
  strcpy(fn2, system_directory); strcat(fn2, "master_node.key");
  if (file_exists(fn1) && file_exists(fn2)) {
    if (serv.initServerCTX(fn1, fn2) < 0)
      fail("failed to initialize service thread at port %d", port);
  } else
    if (serv.initServerCTX_ASN1(
          default_master_node_cert_len, default_master_node_cert,
          default_master_node_key_len, default_master_node_key) < 0)
      fail("failed to initialize service thread at port %d", port);

  serv.open(NULL, port);
  serv.listen();
  while (self->m_Status == RUNNING) {
    char ip[64];
    int port;
    SSLTransport* s = serv.accept(ip, port);
    if (!s)
      continue;
    ServiceJobParam* p = new ServiceJobParam;
    p->ip = ip;
    p->port = port;
    p->ssl = s;
    self->m_ServiceJobQueue.push(p);
  }
  return NULL;
}

int Master::vz_init() {
  char filename[512];
#ifdef DEBUG
  strcpy(filename, system_directory); strcat(filename, "meta.log");
  m_SectorLog.init(filename);
#endif
  master_conf(m_SysConfig);
  strcpy(filename, system_directory);
  strcat(filename, "topology.conf");
  m_SlaveManager.init(filename);
  m_SlaveManager.serializeTopo(m_pcTopoData, m_iTopoDataSize);
  m_strHomeDir = m_SysConfig.m_strHomeDir;

  if (((xmkdir((m_strHomeDir + ".metadata").c_str(), S_IRWXU) < 0) && errno != EEXIST) || 
      ((xmkdir((m_strHomeDir + ".tmp").c_str(), S_IRWXU) < 0) && errno != EEXIST))
    fail("unable to create data files");

  if (m_SysConfig.m_MetaType == DISK)
    m_pMetadata = new Index2;
  else
    m_pMetadata = new Index;
  m_pMetadata->init(m_strHomeDir + ".metadata");

  // load slave list and addresses
#if 0
  // loadSlaveAddr("../conf/slaves.list");
  SlaveAddr sa;
  sa.m_strAddr = getenv("LOGNAME");
  sa.m_strAddr = sa.m_strAddr + "@" + local_ip;
  sa.m_strBase = newline;
  sa.m_strBase = data_directory;
  string ip = local_ip;
  m_mSlaveAddrRec[ip] = sa;
#endif

  // add "slave" as a special user
  User* au = new User;
  au->m_strName = "system";
  au->m_iKey = 0;
  au->m_vstrReadList.insert(au->m_vstrReadList.begin(), "/");
  m_UserManager.insert(au);

  // running...
  m_Status = RUNNING;

  Transport::initialize();

  // start GMP
  if (m_GMP.init(m_SysConfig.m_iServerPort) < 0)
    fail("cannot initialize port %d", m_SysConfig.m_iServerPort);

  // connect security server to get ID
  SSLTransport secconn;
  strcpy(filename, system_directory); strcat(filename, "security_node.cert");
  if (file_exists(filename)) {
    if (secconn.initClientCTX(filename) < 0)
      fail("unable to load security node certificate");
  } else {
    unsigned char *x = (unsigned char *)malloc(default_security_node_cert_len+100);
    memset(x, 0, default_security_node_cert_len+100);
    memcpy(x, default_security_node_cert, default_security_node_cert_len);
    if (secconn.initClientCTX_ASN1(default_security_node_cert_len, default_security_node_cert) < 0)
      fail("unable to load security node certificate");
  }
  secconn.open(NULL, 0);
  if (secconn.connect(m_SysConfig.m_strSecServIP.c_str(), m_SysConfig.m_iSecServPort) < 0)
    fail("unable to find security server");

  int32_t cmd = 4;
  secconn.send((char*)&cmd, 4);
  secconn.recv((char*)&m_iRouterKey, 4);
  secconn.close();

  Address addr;
  addr.m_strIP = "";
  addr.m_iPort = m_SysConfig.m_iServerPort;
  m_Routing.insert(m_iRouterKey, addr);

  // start service thread
  pthread_t svcserver;
  pthread_create(&svcserver, NULL, vz_service, this);
  pthread_detach(svcserver);

  // start management/process thread
  pthread_t msgserver;
  pthread_create(&msgserver, NULL, process, this);
  pthread_detach(msgserver);

  // start replica thread
  pthread_t repserver;
  pthread_create(&repserver, NULL, replica, this);
  pthread_detach(repserver);

  m_llStartTime = time(NULL);
  m_SectorLog.insert("vz started");
  return 1;
}

static void *start_master_thread(void *) {
  sector_master->run();
  // sleep(1); // ??
  return 0;
}

int Master::vz_join(const char* ip, const int& port) {
   // join the server
   SSLTransport s;
   char filename[512];
   strcpy(filename, system_directory); strcat(filename, "master_node.cert");
   if (file_exists(filename)) {
     if (s.initClientCTX(filename) < 0)
       fail("unable to load cluster certificate");
   } else
     if (s.initClientCTX_ASN1(default_master_node_cert_len, default_master_node_cert) < 0)
       fail("unable to load cluster certificate");

   s.open(NULL, 0);
   if (s.connect(ip, port) < 0)
   {
      cerr << "unable to set up secure channel to the existing master.\n";
      return -1;
   }

   int cmd = 3;
   s.send((char*)&cmd, 4);
   int32_t key = -1;
   s.recv((char*)&key, 4);
   if (key < 0) {
     // cerr << "security check failed. code: " << key << endl;
     return -1;
   }

   Address addr;
   addr.m_strIP = ip;
   addr.m_iPort = port;
   m_Routing.insert(key, addr);

   s.send((char*)&m_SysConfig.m_iServerPort, 4);
   s.send((char*)&m_iRouterKey, 4);

   // recv master list
   int num = 0;
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   for (int i = 0; i < num; ++ i)
   {
      char ip[64];
      int port = 0;
      int id = 0;
      int size = 0;
      s.recv((char*)&id, 4);
      s.recv((char*)&size, 4);
      s.recv(ip, size);
      s.recv((char*)&port, 4);
      Address saddr;
      saddr.m_strIP = ip;
      saddr.m_iPort = port;
      m_Routing.insert(id, addr);
   }

   // recv slave list
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   int size = 0;
   if (s.recv((char*)&size, 4) < 0)
      return -1;
   char* buf = new char [size];
   s.recv(buf, size);
   m_SlaveManager.deserializeSlaveList(num, buf, size);
   delete [] buf;

   // recv user list
   if (s.recv((char*)&num, 4) < 0)
      return -1;
   for (int i = 0; i < num; ++ i)
   {
      size = 0;
      s.recv((char*)&size, 4);
      char* ubuf = new char[size];
      s.recv(ubuf, size);
      User* u = new User;
      u->deserialize(ubuf, size);
      delete [] ubuf;
      m_UserManager.insert(u);
   }

   // recv metadata
   size = 0;
   s.recv((char*)&size, 4);
   s.recvfile((m_strHomeDir + ".tmp/master_meta.dat").c_str(), 0, size);
   m_pMetadata->deserialize("/", m_strHomeDir + ".tmp/master_meta.dat", NULL);
   unlink(".tmp/master_meta.dat");

   s.close();

   return 0;
}

void start_sector_master() {
  if (xmkdir(system_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make system directory '%s' : %d %s", system_directory, errno, strerror(errno));
  if (xmkdir(metadata_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make metadata directory '%s' :  %d %s", metadata_directory, errno, strerror(errno));;
  if (xmkdir(data_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make data directory '%s' :  %d %s", data_directory, errno, strerror(errno));;
  sector_master = new Master;
  sector_master->vz_init();
  if (cluster_name[0]) {
    if (sector_master->vz_join((cchar*)cluster_name, base_port) < 0)
      fail("unable to join cluster");
  }
  pthread_t master_thread_id;
  pthread_create(&master_thread_id, NULL, start_master_thread, NULL);
  pthread_detach(master_thread_id);
}

static void *start_security_thread(void *) {
  sector_security->run();
  return 0;
}

void start_sector_security() {
  sector_security = new SServer;
  int port = SECURITY_PORT(base_port);
  char fn1[512], fn2[512];
  strcpy(fn1, system_directory); strcat(fn1, "security_node.cert");
  strcpy(fn2, system_directory); strcat(fn2, "security_node.key");
  if (file_exists(fn1) && file_exists(fn2)) {
    if (sector_security->init(port, fn1, fn2) < 0)
      fail("failed to initialize security server at port %d", port);
  } else
    if (sector_security->init_ASN1(
          port, default_security_node_cert_len, default_security_node_cert,
          default_security_node_key_len, default_security_node_key) < 0)
      fail("failed to initialize security server at port %d", port);
  SSource* src = new FileSrc;
  strcpy(fn1, system_directory); strcat(fn1, "master_acl.conf");
  if (file_exists(fn1)) {
    if (sector_security->loadMasterACL(src, fn1) < 0)
      fail("unable to load master_acl.conf");
  } else {
    IPRange ipr; ipr.m_uiIP = 0; ipr.m_uiMask = 0;
    sector_security->m_vMasterACL.push_back(ipr);
  }
  strcpy(fn1, system_directory); strcat(fn1, "slave_acl.conf");
  if (file_exists(fn1)) {
    if (sector_security->loadSlaveACL(src, fn1) < 0)
      fail("unable to load slave_acl.conf");
  } else {
    IPRange ipr; ipr.m_uiIP = 0; ipr.m_uiMask = 0;
    sector_security->m_vSlaveACL.push_back(ipr);
  }
  strcpy(fn1, system_directory); strcat(fn1, "users");
  if (is_directory(fn1)) {
    if (sector_security->loadShadowFile(src, fn1) < 0)
      fail("unable to load users");
  } else {
    SUser u;
    u.m_iID = 0;
    u.m_strName = "test";
    u.m_strPassword = "test";
    IPRange ipr; ipr.m_uiIP = 0; ipr.m_uiMask = 0;
    u.m_vACL.push_back(ipr);
    u.m_vstrReadList.push_back("/");
    u.m_vstrWriteList.push_back("/");
    u.m_bExec = true;
    sector_security->m_mUsers[u.m_strName] = u;
  }
  delete src;
  pthread_t security_thread_id;
  pthread_create(&security_thread_id, NULL, start_security_thread, NULL);
  pthread_detach(security_thread_id);
}

static void slave_conf(SlaveConf &c) {
  c.m_strMasterHost = cluster_name[0] ? cluster_name : local_ip;
  c.m_iMasterPort = base_port;
  c.m_strHomeDir = data_directory;
  c.m_llMaxDataSize = -1; // ignored
  c.m_iMaxServiceNum = 2; // ignored
  c.m_strLocalIP = ""; // ignored
  c.m_strPublicIP = ""; // ignored
  c.m_iClusterID = 0;
  c.m_MetaType = MEMORY;
}

int Slave::vz_connect() {
  SSLTransport::init();
  string cert = m_strBase + "/../conf/master_node.cert";

  // calculate total available disk size
  struct statfs64 slavefs;
  statfs64(m_SysConfig.m_strHomeDir.c_str(), &slavefs);
  int64_t availdisk = slavefs.f_bfree * slavefs.f_bsize;

  m_iSlaveID = -1;
  m_pLocalFile->serialize("/", m_strHomeDir + ".tmp/metadata.dat");

  set<Address, AddrComp> masters;
  Address m;
  m.m_strIP = m_strMasterIP;
  m.m_iPort = m_iMasterPort;
  masters.insert(m);
  bool first = true;

  while (!masters.empty()) {
    string mip = masters.begin()->m_strIP;
    int mport = masters.begin()->m_iPort;
    masters.erase(masters.begin());

    SSLTransport secconn;
    char filename[512];
    strcpy(filename, system_directory); strcat(filename, "master_node.cert");
    if (file_exists(filename)) {
      if (secconn.initClientCTX(filename) < 0)
        fail("unable to load cluster certificate");
    } else
      if (secconn.initClientCTX_ASN1(default_master_node_cert_len, default_master_node_cert) < 0)
        fail("unable to load cluster certificate");
     secconn.open(NULL, 0);
    if (secconn.connect(mip.c_str(), mport) < 0)
      fail("unable to connect to cluster");

    if (first) {
      secconn.getLocalIP(m_strLocalHost);
      // init data exchange channel
      m_iDataPort = 0;
      if (m_DataChn.init(m_strLocalHost, m_iDataPort) < 0)
        fail("unable to create data channel");
    }

    int32_t cmd = 1;
    secconn.send((char*)&cmd, 4);

    int32_t size = m_strHomeDir.length() + 1;
    secconn.send((char*)&size, 4);
    secconn.send(m_strHomeDir.c_str(), size);

    int32_t res = -1;
    secconn.recv((char*)&res, 4);
    if (res < 0)
      return res;

    secconn.send((char*)&m_iLocalPort, 4);
    secconn.send((char*)&m_iDataPort, 4);
    secconn.send((char*)&(availdisk), 8);
    secconn.send((char*)&(m_iSlaveID), 4);

    if (first)
      m_iSlaveID = res;

    struct stat s;
    stat((m_strHomeDir + ".tmp/metadata.dat").c_str(), &s);
    size = s.st_size;
    secconn.send((char*)&size, 4);
    secconn.sendfile((m_strHomeDir + ".tmp/metadata.dat").c_str(), 0, size);

    if (!first) {
      secconn.close();
      continue;
    }

    // move out-of-date files to the ".attic" directory
    size = 0;
    secconn.recv((char*)&size, 4);

    if (size > 0)
    {
      secconn.recvfile((m_strHomeDir + ".tmp/metadata.left.dat").c_str(), 0, size);

      Metadata* attic = NULL;
      if (m_SysConfig.m_MetaType == DISK)
        attic = new Index2;
      else
        attic = new Index;
      attic->init(m_strHomeDir + ".tmp/metadata.left");
      attic->deserialize("/", m_strHomeDir + ".tmp/metadata.left.dat", NULL);
      unlink((m_strHomeDir + ".tmp/metadata.left.dat").c_str());

      vector<string> fl;
      attic->list_r("/", fl);
      for (vector<string>::iterator i = fl.begin(); i != fl.end(); ++ i)
        move(*i, ".attic", "");

      attic->clear();
      delete attic;

      m_SectorLog.insert("WARNING: certain files have been moved to ./attic due to conflicts.");
    }

    int id = 0;
    secconn.recv((char*)&id, 4);
    Address addr;
    addr.m_strIP = mip;
    addr.m_iPort = mport;
    m_Routing.insert(id, addr);

    int num;
    secconn.recv((char*)&num, 4);
    for (int i = 0; i < num; ++ i)
    {
      char ip[64];
      size = 0;
      secconn.recv((char*)&id, 4);
      secconn.recv((char*)&size, 4);
      secconn.recv(ip, size);
      addr.m_strIP = ip;
      secconn.recv((char*)&addr.m_iPort, 4);

      m_Routing.insert(id, addr);

      masters.insert(addr);
    }

    first = false;
    secconn.close();
  }
  SSLTransport::destroy();
  unlink((m_strHomeDir + ".tmp/metadata.dat").c_str());
  m_SlaveStat.init();
  return 1;
}

static void make_directory(cchar *d, cchar *dd) {
  char fn[512];
  strcpy(fn, d); strcat(fn, dd);
  if (xmkdir(fn, 0700) < 0 && errno != EEXIST)
    fail("unable to make directory '%s' : %d %s", fn, errno, strerror(errno));
}

static void make_clear_directory(cchar *d, cchar *dd) {
  char fn[512];
  strcpy(fn, "rm -rf "); strcat(fn, d); strcat(fn, dd);
  system(fn);
  strcpy(fn, d); strcat(fn, dd);
  if (xmkdir(fn, 0700) < 0 && errno != EEXIST)
    fail("unable to make directory '%s' : %d %s", fn, errno, strerror(errno));
}

int Slave::vz_init() {
  char fn[512];
  strcpy(fn, system_directory); strcat(fn, "slave.conf");
  if (file_exists(fn)) {
    if (m_SysConfig.init(fn) < 0)
      fail("unable to initialize from configuration file");
  } else 
    slave_conf(m_SysConfig);

  m_strMasterHost = m_SysConfig.m_strMasterHost;
  struct hostent* masterip = gethostbyname(m_strMasterHost.c_str());
  if (!masterip)
    fail("unable to get cluster IP");

  char buf[64];
  m_strMasterIP = inet_ntop(AF_INET, masterip->h_addr_list[0], buf, 64);
  m_iMasterPort = m_SysConfig.m_iMasterPort;

  Transport::initialize();

  m_GMP.init(0);
  m_iLocalPort = m_GMP.getPort();

  m_strHomeDir = m_SysConfig.m_strHomeDir;

#ifdef DEBUG
  strcpy(fn, system_directory); strcat(fn, "data.log");
  m_SectorLog.init(fn);
#endif

  make_clear_directory(data_directory, ".metadata");
  make_clear_directory(data_directory, ".sphere");
  make_directory(data_directory, ".sphere/perm");
  make_clear_directory(data_directory, ".tmp");

  // system((string("cp ") + m_strBase + "/sphere/*.so "  + m_strHomeDir + "/.sphere/perm/").c_str());

  if (m_SysConfig.m_MetaType == DISK)
    m_pLocalFile = new Index2;
  else
    m_pLocalFile = new Index;
  m_pLocalFile->init(m_SysConfig.m_strHomeDir + ".metadata");
  m_pLocalFile->scan(m_strHomeDir.c_str(), "/");

  m_SectorLog.insert("slave started");

  return 1;
}

static void *start_slave_thread(void *) {
  sector_slave->run();
  return 0;
}

void start_sector_slave() {
  sector_slave = new Slave;
  sector_slave->vz_init();
  if (sector_slave->vz_connect() < 0)
    fail("unable to initialize");
  pthread_t slave_thread_id;
  pthread_create(&slave_thread_id, NULL, start_slave_thread, NULL);
  pthread_detach(slave_thread_id);
}

void init_sector() {
  int len = strlen(default_security_node_cert_base64);
  default_security_node_cert_len = (len * 3) / 4;
  ink_base64_decode(default_security_node_cert_base64, strlen(default_security_node_cert_base64),
                    default_security_node_cert);
  len = strlen(default_security_node_key_base64);
  default_security_node_key_len = (len * 3) / 4;
  ink_base64_decode(default_security_node_key_base64, strlen(default_security_node_key_base64),
                    default_security_node_key);

  len = strlen(default_master_node_cert_base64);
  default_master_node_cert_len = (len * 3) / 4;
  ink_base64_decode(default_master_node_cert_base64, strlen(default_master_node_cert_base64),
                    default_master_node_cert);
  len = strlen(default_master_node_key_base64);
  default_master_node_key_len = (len * 3) / 4;
  ink_base64_decode(default_master_node_key_base64, strlen(default_master_node_key_base64),
                    default_master_node_key);
}

Client *init_sector_client() {
  Client *sector_client = new Client;
  sector_client->init(cluster_name[0] ? cluster_name : local_ip, base_port);
  sector_client->login("test", "test", 0, default_master_node_cert_len, 
                       default_master_node_cert);
  return sector_client;
}

void stop_sector() {
  SectorError::s_mErrorMsg.clear();
}

