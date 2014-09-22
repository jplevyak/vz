/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#include "defs.h"
#include "conn.h"
#include "editline/readline.h"
#include <string>
#include <sector.h>
#include <client.h>
#include <clientmgmt.h>
#include <conf.h>
#include <sys/xattr.h>
#include <dlfcn.h>
#include <typeinfo>
#include "vz.h"

using namespace std;
using namespace vz;

#define CLI_PROMPT "(vz) "

class CliService : public Service { public:
  void start();
} cli_service;

class CliServer : public Server { public:
  ClassFreeList<Cli> cli_freelist;
  CliServer();
};

CliServer::CliServer() : Server("cli") {
  conn_freelist = &cli_freelist;
}

Cli::Cli() {
  cwd[0] = 0;
}

Cli::Cli(int aofd, int aifd) {
  init((Server*)new ConnFactory("fd"), aifd, aofd);
}

Cli::~Cli() {
  delete factory;
}

int Cli::done() {
  if (cwd[0])
    chdir(cwd);
  return Conn::done();
}

Cli *Cli::init(Server *aserver, int aifd, int aofd) {
  factory = aserver;
  Conn::init();
  ifd = aifd;
  ofd = aofd;
  iprogram = 0;
  return this;
}

static int match_cli(char *ac, cchar *str) {
  char *c = ac;
  while (*c) {
    if (isspace(*c))
      return 1;
    if (tolower(*c) != tolower(*str))
      return 0;
    c++; str++;
  }
  if (ac != c)
    return 1;
  else
    return 0;
}

static void cli_set_conf(Cli *c) {
  char *s = skip_token((char*)c->rbuf.line);
  char *e = (char*)memchr(s, '=', strlen(s));
  *e = 0;
  char *x = e - 1;
  while (x > s && isspace(*x)) *x-- = 0;
  char *vv = e+1;
  cchar *v = 0;
  while (*vv && isspace(*vv)) vv++;
  if (string_config(SET_CONFIG, &v, vv, s)) {
    c->APPEND_STRING("set failed\n");
    return;
  }
  c->append_string(s);
  c->append_string(" changed from '");
  if (!v)
    c->append_string("(null)");
  else
    c->append_string(v);
  c->append_string("' to '");
  c->append_string(vv);
  c->append_string("'\n");
  char cfile[1024];
  e = strchr(config_filenames,',');
  strcpyn(cfile, config_filenames, sizeof(cfile), e ? e-config_filenames : -1);
  expand_filename(cfile, sizeof(cfile));
  replace_config(cfile);
}

static void cli_conf(Cli *c) {
  char *s = (char*)c->rbuf.line;
  s = skip_token(s);
  char *e = (char*)memchr(s, '=', strlen(s));
  if (e) {
    cli_set_conf(c);
  }
  if (!*s) {
    c->append_string("configuration variables:\n");
    write_config(c);
    return;
  }
  while (1) {
    char ss[512], *sss = ss;
    cchar *v = 0;
    char *e = strchr(s, ' ');
    if (e)
      strcpyn(ss, s, sizeof(ss), e-s);
    else
      sss = s;
    int r = string_config(GET_CONFIG, &v, "", sss);
    if (r >= 0) {
      c->append_string(sss);
      c->append_string(" = ");
      c->append_string(v);
      c->append_string("\n");
    } else {
      c->append_string(s);
      c->append_string(" not found\n");
    }
    s = skip_token(s);
    if (!*s) break;
  }
}

static void cli_status(Cli *c) { 
  if (!*session->cluster_name)
    c->append_string("standalone\n");
  else {
    c->append_print("cluster name: %s\n", session->cluster_name);
  }
}

static void cli_standalone(Cli *c) {
  memset(session->cluster_name, 0, sizeof(session->cluster_name));
  c->append_string("entering standalone mode\n");
}

static void cli_start(Cli *c) {
  char *s = skip_token((char*)c->rbuf.line);
  int l = get_token(&s);
  if (!l) {
    cli_standalone(c);
    return;
  }
  Vec<char *> ss;
  Vec<int> ll;
  do {
    ss.add(s); ll.add(l);
    s += l; l = get_token(&s);
  } while (l);
  
  char b[2048];
  for (int i = 0; i < ss.n; i++) {
    char t[512];
    int r = 0;
    memcpy(t, ss[i], ll[i]);
    t[ll[i]] = 0;

    if (!i)
      strcpy(session->cluster_name, t);

    if (verbose_level)
      printf("killing %s...\n", t);
    strcpy(b, "ssh "); strncat(b, ss[i], ll[i]); strcat(b, " killall -q vz");
    if (verbose_level > 1) printf("executing '%s'\n", b);
    if ((r = system(b))) {
      if (verbose_level)
        c->append_print("kill failed: error status %d\n", r);
    } else if (verbose_level)
      printf("kill succeded\n");

    if (verbose_level)
      printf("mkdir ~/.vz on %s...\n", t);
    strcpy(b, "ssh "); strncat(b, ss[i], ll[i]); strcat(b, " \"mkdir -p ~/.vz\"");
    if ((r = system(b)))
      c->append_print("mkdir failed: error status %d\n", r);
    else if (verbose_level)
      printf("mkdir succeded\n");

    if (verbose_level)
      printf("copying libs to %s...\n", t);
    strcpy(b, "scp -p -r ");
    if (!verbose_level)
      strcat(b, "-q ");
    strcat(b, "`which vz | sed 's/\\/[^\\/]*$/\\/\\\\*.so*/'` \""); strncat(b, ss[i], ll[i]); strcat(b, ":~/.vz/\"");
    if (verbose_level > 1) printf("executing '%s'\n", b);
    if ((r = system(b)))
      c->append_print("copy of libs failed: error status %d\n", r);
    else if (verbose_level)
      printf("copy of libs succeded\n");

    if (verbose_level)
      printf("copying vz to %s...\n", t);
    strcpy(b, "scp -p ");
    if (!verbose_level)
      strcat(b, "-q ");
    strcat(b, "`which vz` \""); strncat(b, ss[i], ll[i]); strcat(b, ":~/.vz/vz\"");
    if (verbose_level > 1) printf("executing '%s'\n", b);
    if ((r = system(b)))
      c->append_print("copy of vz failed: error status %d\n", r);
    else if (verbose_level)
      printf("copy of vz succeded\n");

    if (verbose_level)
      printf("starting %s...\n", t);
    strcpy(b, "ssh "); strncat(b, ss[i], ll[i]); 
    strcat(b, " \"cd ~/.vz;sh -c 'export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH;./vz -d");
    if (i) {
      strcat(b, " -n ");
      strncat(b, ss[0], ll[0]); 
    }
    strcat(b, "'\"");
    if (verbose_level > 1) printf("executing '%s'\n", b);
    if ((r = system(b)))
      c->append_print("startup failed: error status %d\n", r);
    else if (verbose_level)
      printf("startup succeded\n");
  }
}

static void cli_login(Cli *c) {
  char *s = skip_token((char*)c->rbuf.line);
  int l = get_token(&s);
  if (!l) {
    c->append_string("please provide a cluster name\n");
    return;
  }
  strncpy(session->cluster_name, s, l);
  session->cluster_name[l] = 0;
  c->append_print("login to %s\n", session->cluster_name);
}

static void cli_version(Cli *c) {
  char ver[64];
  get_version(ver);
  c->append_print("vz Version %s ", ver);
  c->append_string(
#include "COPYRIGHT.i"
    );
}

static char *first_token(char *s, string &str, int skip = 0,  bool quoted = false) {
  s += skip;
  while (isspace(*s)) s++;
  char *e = 0;
  if (*s == '"' || quoted) {
    if (!quoted)
      s++;
    e = strchr(s, '"');
  } else
    e = strchr(s, ' ');
  if (!e)
    e = s + strlen(s);
  if (!s)
    str.assign("");
  else
    str.assign(s, e-s);
  if (*e == '"') e++;
  while (isspace(*e)) e++;
  return e;
}

static char *next_token(char *c, string &str) {
  char *s = skip_token(c);
  first_token(s, str);
  return s;
}

static int number_of_digits(int64_t x) {
  int n = 0;
  if (!x)
    return 1;
  if (x < 0)
    n = 1;
  while (x) {
    x /= 10;
    n++;
  }
  return n;
}

static void cli_ls(Cli *c) {
  string orig;
  char *s = next_token((char*)c->rbuf.line, orig);
  bool lflag = false;
  if (*s == '-') {
    char *ss = s;
    while (*ss && !isspace(*ss)) {
      if (*ss == 'l') lflag = true;
      ss++;
    }
    next_token(s, orig);
  }

  string path = orig;
  bool wc = WildCard::isWildCard(path);
  if (wc) {
    size_t p = path.rfind('/');
    if (p == string::npos)
      path = "/";
    else {
      path = path.substr(0, p);
      orig = orig.substr(p + 1, orig.length() - p);
    }
  }

  Client *client = init_sector_client();
  vector<SNode> list;
  int r = client->list(path, list);
  if (r < 0)
    c->append_print("ls: '%s' : %s\n", orig.c_str(), SectorError::getErrorMsg(r).c_str());
  else {
    int maxsize = 0;
    for (vector<SNode>::iterator i = list.begin(); i != list.end(); ++i) {
      int s = number_of_digits(i->m_llSize);
      if (maxsize < s)
        maxsize = s;
    }
    for (vector<SNode>::iterator i = list.begin(); i != list.end(); ++i) {
      if (wc && !WildCard::match(orig, i->m_strName))
        continue;
      if (STRSUFFIX(i->m_strName.c_str(), ".idx"))
        continue;
      if (lflag) {
        if (i->m_bIsDir)
          c->APPEND_STRING("drw-rw-rw- ");
        else
          c->APPEND_STRING("-rw-rw-rw- ");
        c->append_print("%d ", i->m_sLocation.size());
        c->APPEND_STRING("nobody nobody ");
        c->append_print("%*d ", maxsize, i->m_llSize);
        time_t t = i->m_llTimeStamp;
        char buf[64];
        ctime_r(&t, buf);
        char *b = strchr(buf, ' '); b++;
        char *e = strrchr(buf, ':'); 
        c->append_string(b, e);
        c->APPEND_STRING(" ");
      }
      if (path != "") {
        c->append_string(path.c_str());
        if (path.substr(path.length()-1) != "/")
          c->APPEND_STRING("/");
      }
      c->append_string(i->m_strName.c_str());
      if (i->m_bIsDir)
        c->APPEND_STRING("/\n");
      else
        c->APPEND_STRING("\n");
    }
  }
  client->logout(); client->close();
}

int get_file_list(const string& path, vector<string>& fl) {
  fl.push_back(path);
  struct stat64 s;
  stat64(path.c_str(), &s);
  if (S_ISDIR(s.st_mode)) {
    dirent **namelist;
    int n = scandir(path.c_str(), &namelist, 0, alphasort);
    if (n < 0)
      return -1;
    for (int i = 0; i < n; ++ i) {
      if (namelist[i]->d_name[0] == '.') {
        free(namelist[i]);
        continue;
      }
      string subdir = path + "/" + namelist[i]->d_name;
      if (stat64(subdir.c_str(), &s) < 0)
        continue;
      if (S_ISDIR(s.st_mode))
        get_file_list(subdir, fl);
      else
        fl.push_back(subdir);
    }
  }
  return fl.size();
}

static SectorFile *createSectorFile(Client *c) {
  FSClient* f = c->createFSClient();
  SectorFile* sf = new SectorFile;
  sf->m_iID = g_ClientMgmt.insertFS(f);
  return sf;
}

static int releaseSectorFile(Client *c, SectorFile *sf) {
  FSClient* f = g_ClientMgmt.lookupFS(sf->m_iID);
  g_ClientMgmt.removeFS(sf->m_iID);
  c->releaseFSClient(f);
  delete sf;
  return 0;
}

static int upload_one(Cli *c, Client *client, string &src, string &dest) {
  SectorFile* f = createSectorFile(client);
  if (f->open(dest, SF_MODE::WRITE) < 0) {
    c->append_print("cp: '%s': File exists\n", dest.c_str());
    return -1;
  }
  bool ok = true;
  if (f->upload(src.c_str()) < 0LL)
    ok = false;
  f->close();
  releaseSectorFile(client, f);
  if (!ok) {
    c->append_print("cp: '%s': Remote I/O error\n", dest.c_str());
    return -1;
  }
  return 0;
}

static void upload_all(Cli *c, string &src, string &dest) {
  bool wc = WildCard::isWildCard(src);
  vector<string> files;
  if (!wc) {
    struct stat64 st;
    if (::stat64(src.c_str(), &st) < 0) {
      c->append_print("cp: file:'%s': No such file or directory\n", src.c_str());
      return;
    }
    if (get_file_list(src, files) < 0) {
      c->append_print("cp: file:'%s': %s\n", src.c_str(), strerror(errno));
      return;
    }
  } else {
    string path = src;
    string orig = path;
    size_t p = path.rfind('/');
    if (p == string::npos)
      path = "/";
    else {
      path = path.substr(0, p);
      orig = orig.substr(p + 1, orig.length() - p);
    }
    dirent **namelist;
    int n = scandir(path.c_str(), &namelist, 0, alphasort);
    if (n < 0) {
      c->append_print("cp: file:'%s': %s\n", src.c_str(), strerror(errno));
      return;
    }
    for (int i = 0; i < n; ++ i) {
      if (namelist[i]->d_name[0] == '.') {
        free(namelist[i]);
        continue;
      }
      if (WildCard::match(orig, namelist[i]->d_name))
        get_file_list(path + "/" + namelist[i]->d_name, files);
    }
  }
  string olddir;
  for (int i = strlen(src.c_str()) - 1; i >= 0; --i) {
    if (src[i] != '/') {
      olddir = src.substr(0, i);
      break;
    }
  }
  size_t p = olddir.rfind('/');
  if (p == string::npos)
    olddir = "";
  else
    olddir = olddir.substr(0, p);
  string newdir = dest;
  SNode attr;
  Client *client = init_sector_client();
  int r = client->stat(newdir, attr);
  if ((r < 0) || !attr.m_bIsDir) {
    c->append_print("cp: cannot create file '%s': Destination not directory\n", dest.c_str());
    goto Lreturn;
  }
  for (vector<string>::const_iterator i = files.begin(); i != files.end(); ++ i) {
    string dst = *i;
    if (olddir.length() > 0)
      dst.replace(0, olddir.length(), newdir);
    else
      dst = newdir + "/" + dst;
    struct stat64 s;
    if (stat64(i->c_str(), &s) < 0)
      continue;
    if (S_ISDIR(s.st_mode))
      client->mkdir(dst);
    else {
      string s = *i;
      if (upload_one(c, client, s, dst) < 0)
        goto Lreturn;
    }
  }
Lreturn:
  client->logout(); client->close();
}

int get_file_list(Client *client, const string &path, vector<string> &files) {
  int r = 0;
  SNode attr;
  if ((r = client->stat(path.c_str(), attr)) < 0)
    return r;
  files.push_back(path);
  if (attr.m_bIsDir) {
    vector<SNode> subdir;
    if ((r = client->list(path, subdir)) < 0)
      return r;
    for (vector<SNode>::iterator i = subdir.begin(); i != subdir.end(); ++i) {
      if (i->m_bIsDir) {
        if ((r = get_file_list(client, path + "/" + i->m_strName, files)) < 0)
          return r;
      } else
        files.push_back(path + "/" + i->m_strName);
    }
  }
  return files.size();
}

int download_one(Cli *c, Client *client, const char *src, const char *dest) {
  SNode attr;
  string localpath;
  int r = 0, sn = 0;
  SectorFile* f = 0;
  if ((r = client->stat(src, attr)) < 0) goto Lerror;
  if (attr.m_bIsDir) {
    ::mkdir((string(dest) + "/" + src).c_str(), S_IRWXU);
    return 0;
  }
  f = createSectorFile(client);
  if ((r = f->open(src))) goto Lerror;
  sn = strlen(src) - 1;
  for (; sn >= 0; sn--) {
    if (src[sn] == '/')
      break;
  }
  if (dest[strlen(dest) - 1] != '/')
    localpath = string(dest) + string("/") + string(src + sn + 1);
  else
    localpath = string(dest) + string(src + sn + 1);
  r = f->download(localpath.c_str(), true);
  f->close();
  releaseSectorFile(client, f);
  if (r >= 0) goto Lreturn;
Lerror:
  c->append_print("cp: '%s' '%s': %s\n", src, dest, SectorError::getErrorMsg(r).c_str());
Lreturn:
  return r;
}

static void download_all(Cli *c, string &src, string &dest) {
  int r = 0;
  Client *client = init_sector_client();
  bool wc = WildCard::isWildCard(src);
  struct stat64 st;
  string olddir;
  vector<string> files;
  if (!wc) {
    SNode attr;
    if (client->stat(src.c_str(), attr) < 0) {
      c->append_print("cp: file:'%s': No such file or directory\n", src.c_str());
      return;
    }
    if ((r = get_file_list(client, src, files)) < 0) goto Lerror;
  } else {
    string path = src;
    string orig = path;
    size_t p = path.rfind('/');
    if (p == string::npos)
      path = "/";
    else {
      path = path.substr(0, p);
      orig = orig.substr(p + 1, orig.length() - p);
    }
    vector<SNode> filelist;
    if ((r = client->list(path, filelist)) < 0) goto Lerror;
    for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i) {
      if (WildCard::match(orig, i->m_strName))
        if ((r = get_file_list(client, path + "/" + i->m_strName, files)) < 0) goto Lerror;
    }
  }
  r = stat64(dest.c_str(), &st);
  if ((r < 0) || !S_ISDIR(st.st_mode)) {
    c->append_print("cp: cannot create file '%s': Destination not directory\n", dest.c_str());
    goto Lreturn;
  }
  for (int i = src.length() - 1; i >= 0; --i) {
    if (src[i] != '/') {
      olddir = src.substr(0, i);
      break;
    }
  }
  {
    size_t p = olddir.rfind('/');
    if (p == string::npos)
      olddir = "";
    else
      olddir = olddir.substr(0, p);
  }
  for (vector<string>::iterator i = files.begin(); i != files.end(); ++i) {
    string dst = *i;
    if (olddir.length() > 0)
      dst.replace(0, olddir.length(), dest);
    else
      dst = dest + "/" + dst;
    string localdir = dst.substr(0, dst.rfind('/'));
    if (stat64(localdir.c_str(), &st) < 0) {
      for (unsigned int p = 0; p < localdir.length(); ++p)
        if (localdir.c_str()[p] == '/')
          if ((::mkdir(localdir.substr(0,p).c_str(), S_IRWXU) < 0) && (errno != EEXIST)) goto Local_error;
      if ((::mkdir(localdir.c_str(), S_IRWXU) < 0) && (errno != EEXIST)) goto Local_error;
    }
    if (download_one(c, client, i->c_str(), localdir.c_str()) < 0) goto Lreturn;
  }
  goto Lreturn;
Local_error:
  c->append_print("cp: '%s' '%s': %s\n", src.c_str(), dest.c_str(), strerror(errno));
  goto Lreturn;
Lerror:
  c->append_print("cp: '%s' '%s': %s\n", src.c_str(), dest.c_str(), SectorError::getErrorMsg(r).c_str());
Lreturn:
  client->logout(); client->close();
}

static void cli_cp(Cli *c) {
  int r = 0;
  string orig;
  char *s = next_token((char*)c->rbuf.line, orig);
  bool rflag = false;
  if (*s == '-') {
    char *ss = s;
    while (*ss && !isspace(*ss)) {
      if (*ss == 'r') rflag = true;
      ss++;
    }
    s = next_token(s, orig);
  }
  if (!s) {
    c->append_print("cp: missing source file operand\n");
    return;
  }
  string dest;
  bool download = false;
  s = next_token(s, dest);
  if (!s) {
    c->append_print("cp: missing destination file operand\n");
    return;
  }
  if (!strncmp("file:", dest.c_str(), 4)) {
    download = true;
    dest = dest.c_str() + 5;
  }
  string src = orig;
  bool upload = false;
  if (!strncmp("file:", src.c_str(), 4)) {
    upload = true;
    src = src.c_str() + 5;
  }
  if (upload && download) {
    char cmd[2048] = "cp ";
    strcat(cmd, src.c_str());
    strcat(cmd, " ");
    strcat(cmd, dest.c_str());
    if ((r = system(cmd)) < 0)
      c->append_print("cp: %s\n", strerror(errno));
    return;
  }
  if (upload) {
    upload_all(c, src, dest);
    return;
  }
  if (download) {
    download_all(c, src, dest);
    return;
  }
  bool wc = WildCard::isWildCard(src);
  Client *client = init_sector_client();
  if (!wc) {
    if ((r = client->copy(src, dest)) < 0) goto Lerror;
  } else {
    SNode attr;
    if ((r = client->stat(dest, attr)) < 0)
      goto Lerror;
    if (!attr.m_bIsDir) {
      c->append_print("cp: destination must be a directory\n");
      goto Lreturn;
    }
    size_t p = src.rfind('/');
    if (p == string::npos)
      src = "/";
    else {
      src = src.substr(0, p);
      orig = orig.substr(p + 1, orig.length() - p);
    }
    vector<SNode> filelist;
    if ((r = client->list(src, filelist)) < 0) goto Lerror;
    vector<string> filtered;
    for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i)
      if (WildCard::match(orig, i->m_strName)) {
        if (src == "/")
          filtered.push_back(src + i->m_strName);
        else
          filtered.push_back(src + "/" + i->m_strName);
      }
    for (vector<string>::iterator i = filtered.begin(); i != filtered.end(); ++ i)
      if ((r = client->copy(*i, dest)) < 0) goto Lerror;
  }
  goto Lreturn;
Lerror:
  c->append_print("cp: '%s' '%s': %s\n", orig.c_str(), dest.c_str(), SectorError::getErrorMsg(r).c_str());
Lreturn:
  client->logout(); client->close();
}

static void cli_view(Cli *c) {
  c->append_print("view: unimplemented\n");
}

static void cli_rm(Cli *c) {
  int r = 0;
  string path;
  char *s = next_token((char*)c->rbuf.line, path);
  bool rflag = false, fflag = false;
  if (*s == '-') {
    char *ss = s;
    while (*ss && !isspace(*ss)) {
      if (*ss == 'r') rflag = true;
      if (*ss == 'f') fflag = true;
      ss++;
    }
    next_token(s, path);
  }
  if (path == "") {
    c->APPEND_STRING("rm: missing operand\n");
    return;
  }
  Client *client = init_sector_client();
  bool wc = WildCard::isWildCard(path);
  string orig = path;
  if (!wc) {
    if (rflag) {
      if ((r = client->rmr(path)) < 0) goto Lerror;
    } else {
      if ((client->remove(path)) < 0) goto Lerror;
    }
  } else {
    size_t p = path.rfind('/');
    if (p == string::npos)
      path = "/";
    else {
      path = path.substr(0, p);
      orig = orig.substr(p + 1, orig.length() - p);
    }
    vector<SNode> filelist;
    if ((r = client->list(path, filelist)) < 0) goto Lerror;
    for (vector<SNode>::iterator i = filelist.begin(); i != filelist.end(); ++ i) {
      if (WildCard::match(orig, i->m_strName)) {
        if (rflag) {
          if ((r = client->rmr(path + "/" + i->m_strName)) < 0) goto Lerror;
        } else {
          if ((r = client->remove(path + "/" + i->m_strName)) < 0) goto Lerror;
        }
      }
    }
  }
  goto Lreturn;
Lerror:
  c->append_print("rm: '%s': %s\n", orig.c_str(), SectorError::getErrorMsg(r).c_str());
Lreturn:
  client->logout(); client->close();
}

static void cli_mkdir(Cli *c) {
  string new_dir;
  next_token((char*)c->rbuf.line, new_dir);
  if (new_dir == "") {
    c->APPEND_STRING("mkdir: missing operand\n");
    return;
  }
  Client *client = init_sector_client();
  int r = client->mkdir(new_dir);
  if (r < 0)
    c->append_print("mkdir: '%s' : %s\n", new_dir.c_str(), SectorError::getErrorMsg(r).c_str());
  client->logout(); client->close();
}

static void cli_formats(Cli *c) {
  c->APPEND_STRING(
    "Formats:\n"
    "  byte        Raw Bytes (Fixed 1 Record Size)\n"
    "  text        Newline Terminated Strings (Variable Record Size)\n"
    "  *.struct    C Struct (Fixed Record Size)\n"
    "  *.proto     Protobuf Type (Variable Record Size)\n"
    "  avro        Avro File Container\n"
    "  *.avro      Avro Type (Variable Record Size)\n"
    "  *.genavro   GenAvro Type (Variable Record Size)\n"
    );
}

// use OTL support for databases or use sqoop for loading?
static void cli_filesystems(Cli *c) {
  c->APPEND_STRING(
    "Filesystems:\n"
    "  file:       Local\n"
    "  hdfs:       Hadoop\n"
    "  kfs:        Cloudstore/Kosmos\n"
    "  odbc:       ODBC Table Load\n"
    "  http:       HTTP Request*\n"
    "  ftp:        FTP Get*\n"
    "  scp:        Secure Copy*\n"
    "  global:     VZ Global File (target)\n"
    "  <node>:     VZ Node File on <node>\n"
    "  *           libcurl Protocols\n"
     );
}

static void cli_programs(Cli *c) {
  c->APPEND_STRING(
    "Programs:\n"
    "  gen_sort_sample [-s#]    Generate # GB of Sample Sort Data (default 10GB)\n"
    "  sort_sample              Sort Sample Data\n"
    );
}

static void cli_type(Cli *c) {
  string f;
  char *s = next_token((char*)c->rbuf.line, f);
  if (f == "") {
    c->APPEND_STRING("usage: type FILE [type]\n");
    return;
  }
  string t;
  next_token(s, t);
  int r = 0;
  if (STRPREFIX(f.c_str(), "file:")) {
    if (t == "") {
      char typebuf[4096] = "";
      if ((r = getxattr(f.substr(5,f.length()).c_str(), "user.vz.type", typebuf, 4096) < 0))
        if (r != ENODATA) {
          c->append_print("type: unable to getxattr '%s'\n", f.c_str());
          return;
        }
      if (!*typebuf)
        c->APPEND_STRING("byte\n");
      else {
        c->append_string(typebuf);
        c->APPEND_STRING("\n");
      }
      return;
    } else {
      if (setxattr(f.substr(5,f.length()).c_str(), "user.vz.type", t.c_str(), t.length()+1, 0) < 0)
        c->append_print("type: unable to setxattr '%s'\n", f.c_str());
    }
    return;
  }
  Client *client = init_sector_client();
  if (t == "") {
    SNode attr;
    if ((r = client->stat(f, attr) < 0))
      goto Lerror;
    if (attr.m_type != "") {
      c->append_string(attr.m_type.c_str());
      c->APPEND_STRING("\n");
    } else
      c->APPEND_STRING("byte\n");
  } else {
    if ((r = client->settype(f, t) < 0))
      goto Lerror;
  }
  goto Lreturn;
Lerror:
  c->append_print("type: '%s': %s\n", f.c_str(), SectorError::getErrorMsg(r).c_str());
Lreturn:
  client->logout(); client->close();
}

static void program_filename(Cli *cli, string &program) {
  string sysdir = system_directory;
  if (debug_builtins)
    sysdir = "./";
  char intbuf[99], intbuf2[99];
  memset(intbuf, 0, sizeof(intbuf));
  memset(intbuf2, 0, sizeof(intbuf2));  
  program = sysdir + "run." + xlltoa(getpid(), intbuf+sizeof(intbuf)-2) + "." + 
    xitoa(cli->iprogram++, intbuf2+sizeof(intbuf2)-2);
}

static int get_field(cchar *&p, string &type, string &field) {
  while (isspace(*p)) p++;
  if (*p == '}')
    return 0;
  cchar *f = strchr(p, ' ');
  if (!f)
    f = p + strlen(p);
  type = string(p, f-p);
  while (isspace(*f)) f++;
  cchar *e = strchr(p, ';');
  if (!e)
    e = strchr(p, '}');
  if (!e)
    e = f+strlen(f);
  field = string(f, e-f);
  p = e;
  return 1;
}

static cchar *write_field_parser(FILE *fp, cchar *p) {
  string type, field;
  if (!get_field(p, type, field))
    return 0;
  if (field != "")
    field = "." + field;
  cchar *t = type.c_str();
  fprintf(fp, "  r = xmemchr(s, sep, e-s); if (!r) r = e;\n");
  if (STRPREFIX(t, "byte_t") || STRPREFIX(t, "uint"))
    fprintf(fp, "  t%s = (%s)vz_atoll(s, r); s = r + 1;\n", field.c_str(), type.c_str());
  else if (STRPREFIX(t, "int"))
    fprintf(fp, "  t%s = (%s)vz_atoull(s, r); s = r + 1;\n", field.c_str(), type.c_str());
  else if (STRPREFIX(t, "text_t")) {
    fprintf(fp, "  t%s.str = s; t%s.len = r-s; s = r + 1;\n", field.c_str(), field.c_str());
  } else {
    fprintf(stderr, "vz: unknown type: '%s' for field '%s'\n", type.c_str(), field.c_str());
    return 0;
  }
  fprintf(fp, "  if (s >= e) return;\n");
  if (*p)
    return p + 1;
  else
    return 0;
}

static void write_parser(FILE *fp, cchar *p) {
  fprintf(fp, "void parse(out_t &t, text_t &b, char sep) {\n");
  fprintf(fp, "  char *s = b.str, *e = b.str + b.len, *r = e;\n");
  if (*p == '{') {
    p++;
    while ((p = write_field_parser(fp, p)));
  } else
    write_field_parser(fp, p);
  fprintf(fp, "}\n");
}

static cchar *write_field_printer(FILE *fp, cchar *p, cchar *sep = 0) {
  string type, field;
  if (!get_field(p, type, field))
    return 0;
  if (field != "")
    field = "." + field;
  cchar *t = type.c_str();
  if (sep)
    fprintf(fp, "  append(b, (void*)\"%s\", %d);\n", sep, (int)strlen(sep));
  if (STRPREFIX(t, "byte_t") || STRPREFIX(t, "uint"))
    fprintf(fp, "  expand(b, b.len + 100); b.len += sprintf(b.str + b.len, \"%%llu\", (unsigned long long int)t%s);\n", 
            field.c_str());
  else if (STRPREFIX(t, "int"))
    fprintf(fp, "  expand(b, b.len + 100); b.len += sprintf(b.str + b.len, \"%%lld\", (long long int)t%s);\n", 
            field.c_str());
  else if (STRPREFIX(t, "text_t")) {
    fprintf(fp, "  append(b, t%s);\n", field.c_str());
  } else {
    fprintf(stderr, "vz: unknown type: '%s' for field '%s'\n", type.c_str(), field.c_str());
    return 0;
  }
  if (*p)
    return p + 1;
  else
    return 0;
}

static void write_printer(FILE *fp, cchar *p) {
  fprintf(fp, "void print(buf_t &b, in_t &t) {\n");
  if (*p == '{') {
    p++;
    p = write_field_printer(fp, p);
    while (p) {
      p = write_field_printer(fp, p, ", ");
    }
  } else
    write_field_printer(fp, p);
  fprintf(fp, "}\n");
}

static cchar *write_field_selecter(FILE *fp, cchar *p) {
  string type, field;
  if (!get_field(p, type, field))
    return 0;
  if (field != "")
    field = "." + field;
  fprintf(fp, "  o%s = i%s;\n", field.c_str(), field.c_str());
  if (*p)
    return p + 1;
  else
    return 0;
}

static void write_selecter(FILE *fp, cchar *p) {
  fprintf(fp, "void select(out_t &o, in_t &i) {\n");
  if (*p == '{') {
    p++;
    while ((p = write_field_selecter(fp, p)));
  } else
    write_field_printer(fp, p);
  fprintf(fp, "}\n");
}

static int write_header(Cli *cli, string target, string source, bool selecter = false, string *pexpr = 0) {
  string h = target + ".h";
  int res = 0;
  FILE *hfp = fopen(h.c_str(), "w");
  if (!hfp) {
    cli->append_string("vz: create header failed\n");
    goto Lreturn;
  }
  // from
  fprintf(hfp,
          "class SInput; class SOutput; class SFile;\n"
          "#include <stdio.h>\n"
          "#include \"vz.h\"\n"
          "using namespace vz;\n"
          "typedef int (*VZ__RUN_)(vz::CTX *, int argc, char *argv[]);\n"
          "int run(vz::CTX*, int argc, char *argv[]);\n"
          "int _vz_call_run(const SInput *input, const SOutput *output, SFile *file, VZ__RUN_ r);\n"
          "extern \"C\" int _vz__run_(const SInput *input, SOutput *output, SFile *file) {\n"
          "  return _vz_call_run(input, output, file, run);\n"
          "}\n");
            
  if (STRPREFIX(cli->itype.c_str(), "byte")) {
    fprintf(hfp, "#define ITYPE_BYTE 1\n");
    fprintf(hfp, "typedef byte_t in_t;\n");
  } else if (STRPREFIX(cli->itype.c_str(), "text")) {
    fprintf(hfp, "#define ITYPE_TEXT 1\n");
    fprintf(hfp, "typedef buf_t in_t;\n");
  } else if (STRPREFIX(cli->itype.c_str(), "struct:")) {
    fprintf(hfp, "#define ITYPE_STRUCT 1\n");
    const char *p = cli->itype.c_str() + sizeof("struct:")-1;
    while (isspace(*p)) p++;
    if (!pexpr)
      fprintf(hfp, "typedef %s%s in_t;\n", *p == '{' ? "struct ":"", p);
    else {
      p++;
      fprintf(hfp, "struct in_t { bool expr() { return %s; } ", pexpr->c_str());
      fprintf(hfp, " %s;\n", p); 
    }
    write_printer(hfp, p);
  } else if (STRPREFIX(cli->itype.c_str(), "avro:"))
    fprintf(hfp, "#define ITYPE_AVRO 1\n");
  else if (STRPREFIX(cli->itype.c_str(), "protobuf:"))
    fprintf(hfp, "#define ITYPE_PROTOBUF 1\n");
  else {
    cli->append_string("vz: unknown input type\n");
    goto Lerror;
  }

  if (STRPREFIX(cli->otype.c_str(), "byte")) {
    fprintf(hfp, "#define OTYPE_BYTE 1\n");
    fprintf(hfp, "typedef byte_t out_t;\n");
  } else if (STRPREFIX(cli->otype.c_str(), "text")) {
    fprintf(hfp, "#define OTYPE_TEXT 1\n");
    fprintf(hfp, "typedef buf_t out_t;\n");
  } else if (STRPREFIX(cli->otype.c_str(), "struct:")) {
    fprintf(hfp, "#define OTYPE_STRUCT 1\n");
    cchar *p = cli->otype.c_str() + sizeof("struct:")-1;
    while (isspace(*p)) p++;
    fprintf(hfp, "typedef %s%s out_t;\n", *p == '{' ? "struct ":"", p);
    write_parser(hfp, p);
    if (selecter)
      write_selecter(hfp, p);
  } else if (STRPREFIX(cli->otype.c_str(), "avro:"))
    fprintf(hfp, "#define OTYPE_AVRO 1\n");
  else if (STRPREFIX(cli->otype.c_str(), "protobuf:"))
    fprintf(hfp, "#define OTYPE_PROTOBUF 1\n");
  else {
    cli->append_string("vz: unknown output type\n");
    goto Lerror;
  }
  fprintf(hfp, "#line 1 \"%s.cc\"\n", source.c_str());
  goto Lreturn;
Lerror:
  res = -1;
Lreturn:
  if (hfp)
    fclose(hfp);
  return res;
}

static char *get_ifile(Cli *cli, char *s, Client *client) {
  // ifile
  if (cli->ifile == "")
    s = first_token(s, cli->ifile);
  if (cli->ifile == "")
    cli->ifile = "-";

  // itype
  if (cli->itype == "" && cli->ifile != "-") {
    SNode attr;
    int r;
    if ((r = client->stat(cli->ifile, attr) < 0)) {
      cli->append_print("vz: unable to stat '%s': %s\n", cli->ifile.c_str(),
                        SectorError::getErrorMsg(r).c_str());
      goto Lerror;
    }
    cli->itype = attr.m_type;
  }
  if (cli->itype == "")
    cli->itype = "byte";
  goto Lreturn;
Lerror:
  s = 0;
Lreturn:
  return s;
}

static char *get_ofile(Cli *cli, char *s, Client *client) {
  // ofile
  if (cli->ofile == "")
    s = first_token(s, cli->ofile);
  if (cli->ofile == "")
    cli->ofile = "-";

  // otype
  if (cli->otype == "" && cli->ofile != "-") {
    SNode attr;
    int r;
    if ((r = client->stat(cli->ofile, attr) < 0)) {
      cli->append_print("vz: unable to stat '%s': %s\n", cli->ofile.c_str(),
                        SectorError::getErrorMsg(r).c_str());
      goto Lerror;
    }
    cli->otype = attr.m_type;
  }
  if (cli->otype == "")
    cli->otype = cli->itype;
  goto Lreturn;
Lerror:
  s = 0;
Lreturn:
  return s;
}

static char *get_iofiles(Cli *cli, char *s, Client *client) {
  if (!(s = get_ifile(cli, s, client)))
    return 0;
  if (!(s = get_ofile(cli, s, client)))
    return 0;
  return s;
}

static int make_program(Cli *cli, string target, string source) {
  char cmd[2048];
  void *so = NULL;
  VZ__RUN_ run = NULL;
  int res = 0;

  sprintf(cmd, "make -f Makefile.vz SOURCE=%s PROG=%s %s", source.c_str(), target.c_str(), source.c_str());
  if (system(cmd)) {
    cli->append_string("vz: make builtin failed\n");
    goto Lerror;
  }
  
  if (!(so = dlopen((target + ".so").c_str(), RTLD_NOW | RTLD_DEEPBIND))) {
    cli->append_print("vz: load failed: %s\n", dlerror());
    goto Lerror;
  }

  if (!(run = (VZ__RUN_)dlsym(so, "_vz__run_"))) {
    cli->append_print("vz: unable to find function in library\n");
    goto Lerror;
  }
  goto Lreturn;

Lerror:
  res = -1;
Lreturn:
  if (so)
    dlclose(so);
  return res;
}

static int run_program(Cli *cli, string target, Client *client) {
  int res = 0;
  DCClient *sp = 0;
  vector<string> ifiles;
  SphereStream iss, oss;
  int streamout = cli->ofile == "-";
  int textout = cli->otype == "text";

  ifiles.push_back(cli->ifile);
  if (iss.init(ifiles) < 0) {
    cli->append_print("vz: unable to initialize input\n");
    goto Lerror;
  }
    
  if (!streamout) {
    oss.init(-1);
    oss.setOutputPath(".", cli->ofile);
  } else
    oss.init(0);
  
  sp = client->createDCClient();
  if (sp->loadOperator((target + ".so").c_str()) < 0) {
    cli->append_print("vz: unable to load library\n");
    goto Lerror;
  }

  if (sp->run(iss, oss, "_vz__run_", 1) < 0) {
    cli->append_print("vz: unable to run\n");
    goto Lerror;
  }

  timeval nowt, lastt;
  gettimeofday(&nowt, 0);
  lastt = nowt;
  while (1) {
    SphereResult *res = NULL;
    if (sp->read(res) <= 0) {
      if (sp->checkProgress() < 0) {
        cli->append_print("vz: run failed\n");
        goto Lerror;
      }
      if (sp->checkProgress() == 100)
        break;
    } else {
      if (streamout) {
        for (int i = 0; i < res->m_iIndexLen-1; i++) {
          cli->append_bytes(res->m_pcData + res->m_pllIndex[i], res->m_pllIndex[i+1] - res->m_pllIndex[i]);
          if (textout)
            cli->APPEND_STRING("\n");
        }
      }
      delete res;
    }
    gettimeofday(&nowt, 0);
    if (lastt.tv_sec - nowt.tv_sec > 60) {
      cli->append_print("vz: progress %d%%\n", sp->checkProgress());
      cli->put();
      lastt = nowt;
    }
  }
  if (!streamout && (res = client->settype(cli->ofile, cli->otype)) < 0) {
    cli->append_print("vz: unable to set output type '%s' on %s: %d\n", 
                      cli->otype.c_str(), cli->ofile.c_str(), res);
    goto Lerror;
  }
  goto Lreturn;

Lerror:
  res = -1;
Lreturn:
  if (sp) {
    sp->close();
    client->releaseDCClient(sp);
  }
  return res;
}

static void close_and_unlink(string &path, Client *client) {
  if (!save_intermediates) {
    //unlink((path + ".h").c_str());
    //unlink((path + ".cc").c_str());
    unlink((path + ".so").c_str());
  }
  if (client) {
    client->logout();
    client->close();
  }
}

static void cli_cat(Cli *cli) {
  string p, builtin, option;
  int64 count = 0;
  char *s = (char*)cli->rbuf.line;

  s = first_token(s, builtin);
  program_filename(cli, p);

  // args -iITYPE -oOTYPE
  while (*s == '-' || *s == '"') {
    bool quoted = *s == '"';
    if (quoted) s++;
    if (s[1] == 'i') s = first_token(s, cli->itype, 2, quoted);
    else if (s[1] == 't') s = first_token(s, cli->otype, 2, quoted);
    else if (s[1] == 'o') s = first_token(s, cli->ofile, 2, quoted);
    else if (s[1] == 'c') {
      s = first_token(s, option, 2, quoted);
      count = atoll(option.c_str());
    } else {
      cli->append_string("vz: unknown option\n");
      return;
    }
  }

  Client *client = init_sector_client();

  if ((s = get_iofiles(cli, s, client)) == NULL)
    goto Lreturn;
  if (write_header(cli, p, builtin) < 0)
    goto Lreturn;
  if (make_program(cli, p, builtin) < 0)
    goto Lreturn;
  if (run_program(cli, p, client) < 0)
    goto Lreturn;

Lreturn:
  close_and_unlink(p, client);
}

static void cli_select(Cli *cli) {
  string p, builtin, sfields;
  vector<string> fields;
  bool selecter = false;
  char *s = (char*)cli->rbuf.line;

  s = first_token(s, builtin);
  program_filename(cli, p);

  // args -iITYPE -oOTYPE
  while (*s == '-' || *s == '"') {
    bool quoted = *s == '"';
    if (quoted) s++;
    if (s[1] == 'i') s = first_token(s, cli->itype, 2, quoted);
    else if (s[1] == 't') s = first_token(s, cli->otype, 2, quoted);
    else if (s[1] == 'o') s = first_token(s, cli->ofile, 2, quoted);
    else {
      cli->append_string("vz: unknown option\n");
      return;
    }
  }

  Client *client = init_sector_client();

  // if -ofile has a type, use that
  if (cli->ofile != "")
    if ((s = get_ofile(cli, s, client)) == NULL)
      goto Lreturn;
  if (cli->otype == "") {
    // collect selected fields
    s = first_token(s, sfields);
    if (sfields == "") {
      cli->append_string("vz: missing fields\n");
      return;
    }
    cchar *f = sfields.c_str(), *comma = 0;
    while ((comma = strchr(f, ','))) {
      fields.push_back(string(f, comma-f));
      f = comma + 1;
    }
    fields.push_back(string(f));
  }

  if ((s = get_ifile(cli, s, client)) == NULL)
    goto Lreturn;
  if (cli->otype == "") {
    if (!STRPREFIX(cli->itype.c_str(), "struct:")) {
      cli->append_string("vz: unable to generate output type\n");
      goto Lreturn;
    }

    const char *t = cli->itype.c_str() + sizeof("struct:")-1;
    while (isspace(*t)) t++;
    if (*t != '{') {
      cli->append_string("vz: unable to generate output type\n");
      goto Lreturn;
    }
    t++;
    string type, field;
    cli->otype += "struct:{ ";
    while (get_field(t, type, field)) {
      for (vector<string>::iterator i = fields.begin(); i != fields.end(); ++i)
        if (*i == field) goto Lkeep;
      goto Ltoss;
    Lkeep:
      cli->otype += type + " " + field + "; ";
    Ltoss:
      if (!*t) break;
      t++;
    }
    cli->otype += "}";
  }

  if ((s = get_ofile(cli, s, client)) == NULL)
    goto Lreturn;
  selecter = STRPREFIX(cli->itype.c_str(), "struct");
  if (write_header(cli, p, builtin, selecter) < 0)
    goto Lreturn;
  if (make_program(cli, p, builtin) < 0)
    goto Lreturn;
  if (run_program(cli, p, client) < 0)
    goto Lreturn;

Lreturn:
  close_and_unlink(p, client);
}

static void cli_where(Cli *cli) {
  string p, builtin, expr;
  char *s = (char*)cli->rbuf.line;

  s = first_token(s, builtin);
  program_filename(cli, p);

  while (*s == '-' || (*s == '"' && s[1] == '-')) {
    bool quoted = *s == '"';
    if (quoted) s++;
    if (s[1] == 'o') s = first_token(s, cli->ofile, 2, quoted);
    else {
      cli->append_string("vz: unknown option\n");
      return;
    }
  }

  while (isspace(*s)) s++;
  char *e = (char*)cli->rbuf.end;
  while (e > s && isspace(*e)) e--;
  while (e > s && !isspace(*e)) e--;
  while (e > s && isspace(*e)) e--;
  while (e > s && !isspace(*e)) e--;
  if (e == s) {
    cli->append_string("vz: missing expression\n");
    return;
  }
  char *sexpr = s;
  s = e;
  while (e > sexpr && isspace(*e)) e--;
  if (*sexpr == '"' && *e == '"') {
    char *ss = sexpr;
    int quotes = 0;
    while (e >= ss) 
      if (*ss++ == '"') quotes++;
    if ((quotes / 2) % 2) {
      sexpr++;
      e--;
    }
  }
  expr = string(sexpr, e+1);
  fprintf(stderr, "'%s'\n", expr.c_str());

  Client *client = init_sector_client();
  if ((s = get_ifile(cli, s, client)) == NULL)
    goto Lreturn;
  cli->otype = cli->itype;
  if (!STRPREFIX(cli->itype.c_str(), "struct:")) {
    cli->append_string("vz: unable to filter type\n");
    goto Lreturn;
  }
  if ((s = get_ofile(cli, s, client)) == NULL)
    goto Lreturn;
  if (write_header(cli, p, builtin, false, &expr) < 0)
    goto Lreturn;
  if (make_program(cli, p, builtin) < 0)
    goto Lreturn;
  if (run_program(cli, p, client) < 0)
    goto Lreturn;

Lreturn:
  close_and_unlink(p, client);
}


struct CliTable {
  cchar *cmd;
  void (*fn)(Cli *c);
  cchar *cmd_doc;
  cchar *doc;
} cli_table[] = {
  { "status", cli_status, 0, "Show Status of Cluster" },
  { "standalone", cli_standalone, 0, "Enter Standalone Mode" },
  { "start", cli_start, "start node+", "Start Server on Nodes" },
  { "login", cli_login, "login node", "Login to Cluster Node" },
  { 0, 0, "PROG [SRC* DEST FORMAT]", "Run a Distributed Program" },
  { "view", cli_view, "view PROG [SRC* DEST FORMAT]", "Create the View DEST" },
  { "ls", cli_ls, "ls [-l] DIR", "List Directory Contents" },
  { "cp", cli_cp, "cp SRC DEST [FORMAT]", "Copy File(s)" },
  { "mkdir", cli_mkdir, "mkdir [-p] DIR", "Make a Directory" },
  { "rm", cli_rm, "rm [-rf] FILE", "Remove a File/Directory" },
  { "rmdir", cli_rm, "rmdir DIR", "Remove a Directory (or use rm)" },
  { "type", cli_type, "type FILE [type]", "Set/Print File Type" },
  { "cat",  cli_cat, "cat [-c#] [-tOTYPE] [IFILE+] [OFILE]", 
    "Concatinate IFILE+ to OFILE"},
  { "select",  cli_select, "select x,y,z [IFILE] [OFILE]", "Select Fields"},
  { "where",  cli_where, "where \"x < 10\" [IFILE] [OFILE]", "Filter By Field Values"},
  { "conf", cli_conf, "conf [NAME [= VALUE]]", "Print/Set Config Variable(s)" },
  { "types", cli_formats, "types", "Show Supported Types" },
  { "filesystems", cli_filesystems, "filesystems", "Show Supported Filesystems" },
  { "programs", cli_programs, "programs", "Show Builtin Programs" },
  { "version", cli_version, "reinit", "Reload Initial Configuration" },
  { 0, 0, "help,? [cmd]", 0 },
  { 0, 0, "quit,exit", 0 }
};

void cli_usage(Cli *c) {
  c->APPEND_STRING("Commands:\n");
  const char *blanks = "                                           ";
  for (uint i = 0; i < numberof(cli_table); i++) {
    cchar *cmd_doc = cli_table[i].cmd_doc;
    if (!cmd_doc) cmd_doc = cli_table[i].cmd;
    c->APPEND_STRING("  ");
    c->append_string(cmd_doc);
    if (cli_table[i].doc) {
      c->append_string(blanks+strlen(cmd_doc));
      c->append_string(cli_table[i].doc);
    }
    c->APPEND_STRING("\n");
  }
  c->put();
}

static int match_cli_table(Cli *cli, char *c) {
  for (uint i = 0; i < numberof(cli_table); i++) {
    if (cli_table[i].fn && match_cli(c, cli_table[i].cmd)) {
      cli_table[i].fn(cli);
      return 1;
    }
  }
  return 0;
}

#if 0
static int make_program(Cli *cli, char *cmd) {
  chdir(system_directory);
  FILE *fp = 0;
  if (!(fp = popen(f.c_str(), "r"))) {
    cli->append_print("vz: unable to execute builtin %s\n", p.c_str());
    return 1;
  }
  char *buf = 0;
  int len = 0;
  if (buf_read(fileno(fp), &buf, &len) < 0) {
    cli->append_print("vz: read error on execution of %s\n", p.c_str());
    return 1;
  }
  otype = buf;
  FREE(buf);
  int e = 0;
  if ((e = pclose(fp))) {
    cli->append_print("vz: %s failed\n", p.c_str());
    return 1;
  }
  return 0;
}
#endif

static int try_program(Cli *cli, char *c) {
  return 0;
#if 0
  while (1) {
    string p;
    char *s = first_token(c, p);
    int b;
    if ((b = builtin(p)) >= 0) {
      switch (b) {
        case VZ_CAT: return make_cat(cli, s);
        case VZ_SELECT:
        case VZ_WHERE:
          break;
        default: assert(!"case"); return 0;
      }
    }
    return 0;
  }
#endif
}

#if 0
static int run_program(Cli *cli, string &otype) {
  return 1;
}

static int try_program(Cli *cli, char *c) {
  string p, otype, program;
  char *s = first_token(c, p);
  string sysdir = system_directory;
  struct stat64 st;
  string f = p + ".vz"; // local .vz
  if (stat64(f.c_str(), &st) >= 0)
    goto Lfound;
  f = sysdir + p + ".vz"; // system builtin .vz
  if (stat64(f.c_str(), &st) >= 0)
    goto Lfound;
  f = p;
  if (stat64(p.c_str(), &st) >= 0) {
    if (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) 
      goto Lfound;
    else {
      // system builtin non-executable, next arg is output type
      s = next_token(s, otype);
      program = p;
      goto Lexec;
    }
  }
  return 0;
Lfound:
  program = sysdir + "run." + xlltoa(getpid(), intbuf+sizeof(intbuf)-2) + "." + 
    xitoa(cli->iprogram++, intbuf2+sizeof(intbuf2)-2);
  f = f + " - " + skip_token(s);
  {
    FILE *fp = 0;
    if (!(fp = popen(f.c_str(), "r"))) {
      cli->append_print("vz: unable to execute builtin %s\n", p.c_str());
      return 1;
    }
    char *buf = 0;
    int len = 0;
    if (buf_read(fileno(fp), &buf, &len) < 0) {
      cli->append_print("vz: read error on execution of %s\n", p.c_str());
      return 1;
    }
    otype = buf;
    FREE(buf);
    int e = 0;
    if ((e = pclose(fp))) {
      cli->append_print("vz: %s failed\n", p.c_str());
      return 1;
    }
  }
Lexec:
  cli->append_print("vz: running with otype: %s\n", otype.c_str());
  return run_program(cli, otype);
}
#endif

int Cli::main(char *cmd) {
  if (verbose_level) printf("%s: fd %d\n", __FUNCTION__, ifd);
  getcwd(cwd, PATH_MAX);
  if (ifd == 0)
    using_history();
  int quit = ifd == -1;
  while (1) {
    if (cmd) {
      rbuf.line = (byte*)cmd;
      rbuf.end = (byte*)(cmd + strlen(cmd));
      rbuf.cur = rbuf.end + 1;
      cmd = 0;
    } else if (ifd == 0) {
      char *in = readline(CLI_PROMPT);
      if (!in)
        return done();
      add_history(in);
      rbuf.line = (byte*)in;
      rbuf.cur = rbuf.end;
    } else if (ifd == -1)
      return done();
    else {
      if (APPEND_STRING(CLI_PROMPT)) return error();
      if (get_line()) return done();
    }
    rbuf.cur[-1] = 0; // NULL terminate
    if (rbuf.cur - rbuf.line > 1 && rbuf.cur[-2] == '\r')
      rbuf.cur[-2] = 0; // NULL terminate
    char *c = (char*)rbuf.line;
    while (isspace(*c)) c++;
    if (0) {}
    else if (match_cli_table(this, c)) ;
    else if (match_cli(c, "reinit")) reinit_config();
    else if (match_cli(c, "help") || match_cli(c, "?")) cli_usage(this);
    else if (match_cli(c, "quit") || match_cli(c, "exit")) quit = 1;
    else if (try_program(this, c)) {}
    else APPEND_STRING("unknown command: try 'help'\n");
    if (wbuf.wlen()) {
      if (put()) return error();
    }
    if (quit)
      break;
    check_rbuf();
  }
  return done();
}

static void *cli_accept_thread_main(void *aserver) {
  CliServer *server = (CliServer*)aserver;
  int accept_fd = bind_port(server->port);
  while (1) {
    int fd = accept_socket(accept_fd);
    Cli *s = server->cli_freelist.alloc();
    server->thread_pool.add_job(s->init(server, fd, fd));
  }
  return 0;
}

void CliService::start() {
  CliServer *server = new CliServer();
  if (server->port)
    create_thread(cli_accept_thread_main, server);
}

