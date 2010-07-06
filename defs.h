/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#ifndef _defs_H_
#define _defs_H_

#include "plib.h"
#include "cli.h"
#include "vz.h"

#define SESSION_VERSION 1
#define DEFAULT_BASE_PORT 8800
#define DEFAULT_REPLICAS 3

#define SECURITY_PORT(_base_port) (_base_port + 1)
#define CLIENT_PORT(_base_port) (_base_port + 2) // kfs client
#define META_PORT(_base_port) (_base_port + 3) // kfs metaserver
#define DATA_PORT(_base_port) (_base_port + 4) // kfs chunkserver

struct SessionState {
  int version;
  char cluster_name[128];
};

typedef int (*VZ__RUN_)(vz::CTX *, int argc, char *argv[]);

EXTERN int run_flag EXTERN_INIT(true);
EXTERN int base_port EXTERN_INIT(DEFAULT_BASE_PORT);
EXTERN int replicas EXTERN_INIT(DEFAULT_REPLICAS);
EXTERN char program_name[100] EXTERN_INIT("vz");
EXTERN char cluster_name[128] EXTERN_INIT("");
EXTERN char session_identifier[100] EXTERN_INIT("");
EXTERN char user_directory[512] EXTERN_INIT("~/.vz/");
EXTERN char system_directory[512] EXTERN_INIT("~/.vz/");
EXTERN char metadata_directory[512] EXTERN_INIT("~/.vz/metadata/");
EXTERN char data_directory[512] EXTERN_INIT("~/.vz/data/");
EXTERN char checkpoint_directory[512] EXTERN_INIT("~/.vz/checkpoint/");
EXTERN char chunks_directory[512] EXTERN_INIT("~/.vz/chunks/");
EXTERN int save_intermediates EXTERN_INIT(0);
EXTERN int verbose_level EXTERN_INIT(0);
#ifdef DEBUG
EXTERN int debug_builtins EXTERN_INIT(1);
#else
EXTERN int debug_builtins EXTERN_INIT(0);
#endif
EXTERN int debug_level EXTERN_INIT(0);

void get_version(char *);
void cli_usage(Cli *);

class Client;
void init_sector();
void start_sector_security();
void start_sector_master();
void start_sector_slave();
Client *init_sector_client();
void stop_sector();

void init_kfs();
void start_kfs_metaserver();
void start_kfs_chunkserver();
void start_kfs_cleaner();
void stop_kfs();

EXTERN struct sockaddr_in local_sa;
EXTERN char local_ip[32] EXTERN_INIT("");
EXTERN SessionState *session EXTERN_INIT(0);
EXTERN int session_fd EXTERN_INIT(-1);

void set_cluster_name(cchar *cluster_name);

#endif
