/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#define EXTERN
#include <signal.h>
#include <sys/resource.h>
#include <editline/readline.h>
#include "defs.h"

int do_unit_tests = 0;
int do_daemonize = 0;
int do_foreground = 0;
int do_interactive = 0;

static void
help(ArgumentState *arg_state, char *arg_unused) {
  char ver[30];
  get_version(ver);
  fprintf(stderr, "VZ Version %s ", ver);
  fprintf(stderr, 
#include "COPYRIGHT.i"
);
  fprintf(stderr, "Usage: %s [flags] command\n", arg_state->program_name);
  usage(arg_state, (char*)"noexit");
  Cli cli(STDOUT_FILENO);
  cli_usage(&cli);
  exit(0);
}

static void
version(ArgumentState *arg_state, char *arg_unused) {
  char ver[64];
  get_version(ver);
  fprintf(stderr, "VZ Version %s ", ver);
  fprintf(stderr, 
#include "COPYRIGHT.i"
);
  exit(0);
}

static void
license(ArgumentState *arg_state, char *arg_unused) {
  fprintf(stdout,
#include "LICENSE.i"
);
  exit(0);
}

static void
reload_config(ArgumentState *arg_state, char *arg_unused) {
  reinit_config();
}

static ArgumentDescription arg_desc[] = {
  {"name", 'n', "Cluster Hostname", "S99", cluster_name, "VZ_NAME", NULL},
  {"port", 'p', "Base Port", "I", &base_port, "VZ_PORT", NULL},
  {"interactive", 'i', "Interactive Mode", "F", &do_interactive, "VZ_INTERACTIVE", NULL},
  {"session", 's', "Session Identifier", "S99", session_identifier, "VZ_SESSION", NULL},
  {"foreground", 'f', "Run Server in Foreground", "F", &do_foreground, "VZ_FOREGROUND", NULL},
  {"daemon", 'd', "Run Server in Background", "F", &do_daemonize, "VZ_DAEMON", NULL},
  {"config-file", 'c', "Configuration File", "S511", &config_filenames, "VZ_CONFIG_FILE", reload_config},
  {"system-directory", 'D', "System Directory", "S511", system_directory, "VZ_SYSTEM_DIRECTORY", NULL},
  {"verbose", 'v', "Verbosity Level", "+", &verbose_level, "VZ_VERBOSE", NULL},
#ifdef DEBUG
  {"debugbuiltins", ' ', "Debug Builtins", "+", &debug_builtins, "VZ_DEBUG_BUILTINS", NULL},
  {"save", 'S', "Save Intermediates", "F", &save_intermediates, "VZ_SAVE_INTERMEDIATES", NULL},
#endif
  {"debug", 'd', "Debugging Level", "+", &debug_level, "VZ_DEBUG", NULL},
  {"license", ' ', "Show License", NULL, NULL, NULL, license},
  {"version", ' ', "Version", NULL, NULL, NULL, version},
  {"help", 'h', "Help", NULL, NULL, NULL, help},
  {0}
};

static ArgumentState arg_state("vz", arg_desc, 
                               ARG_OPT_STOP_ON_FILE_ARGUMENT |
                               ARG_OPT_NO_DEFAULT_USAGE_HEADER
  );

static void hup_signal(int sig) {
  reinit_config();
  Service::reinit_all();
}

static void quit_signal(int sig) {
  run_flag = false;
}

static void init_system() {
  setvbuf(stdout, (char*)0, _IOLBF, 0);
  signal(SIGPIPE, SIG_IGN);
  struct rlimit nfiles;
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
  nfiles.rlim_cur = nfiles.rlim_max;
  assert(!setrlimit(RLIMIT_NOFILE, &nfiles));
  assert(!getrlimit(RLIMIT_NOFILE, &nfiles));
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = hup_signal;
  sigaction(SIGHUP, &sa, NULL);
  sa.sa_handler = quit_signal;
  sigaction(SIGQUIT, &sa, NULL);
  if (getifaddrname(&local_sa) < 0)
    memset(&local_sa, 0, sizeof(local_sa));
  strcpy(local_ip, inet_ntoa(local_sa.sin_addr));
}

static void do_cmd(Cli &c, char *argv[]) {
  int l = 1;
  for (char **p = argv; *p; p++) l += strlen(*p) + 1 + 2;
  char ss[l], *s = ss;
  char **p = argv;
  s = scpy(s, *p); p++;
  for (; *p; p++) {
    *s++ = ' ';
    if (!strchr(*p, ' '))
      s = scpy(s, *p);
    else {
      *s++ = '"';
      s = scpy(s, *p);
      *s++ = '"';
    }
  }
  *s = 0;
  c.main(ss);
}

static void start_vz() {
  start_sector_security();
  start_sector_master();
  start_sector_slave();
  init_kfs();
  start_kfs_metaserver();
  start_kfs_chunkserver();
  start_kfs_cleaner();
}

static void stop_vz() {
  stop_sector();
  stop_kfs();
}

void set_cluster_name(cchar *cluster_name) {
  if (STRCMP(cluster_name, "standalone"))
    strcpy(session->cluster_name, cluster_name);
  else
    memset(session->cluster_name, 0, sizeof(session->cluster_name));
}

static void open_session() {
  if (session_fd >= 0)
    ::close(session_fd);
  char fn[512];
  strcpy(fn, user_directory); strcat(fn, "session");
  if (*session_identifier) { strcat(fn, "."); strcat(fn, session_identifier); }
  session_fd = ::open(fn, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if (session_fd < 0)
    fail("unable to open session state file %s: %s", fn, strerror(errno));
  if (ftruncate(session_fd, sizeof(SessionState)) < 0)
    fail("unable to size session state file %s: %s", fn, strerror(errno));
  session = (SessionState*)mmap(0, sizeof(SessionState), PROT_READ|PROT_WRITE, MAP_SHARED, session_fd, 0);
  if (session->version != SESSION_VERSION) {
    printf("initializing session %s\n", *session_identifier ? session_identifier : "standalone");
    memset(session, 0, sizeof(SessionState));
    session->version = SESSION_VERSION;
  }
  if (*cluster_name)
    set_cluster_name(cluster_name);
}

int main(int argc, char *argv[]) {
  if (argv[0]) {
    strncpy(program_name, argv[0], sizeof(program_name)-1);
    program_name[sizeof(program_name)-1] = 0;
  }
  MEM_INIT();
  INIT_RAND64(0x1234567);
  strcpy(config_filenames, "~/.vz/vz.conf,/opt/vz/vz.conf");
  init_system();
  init_config();
  int_config(DYNAMIC_CONFIG, &base_port, base_port, "base_port");
  int_config(DYNAMIC_CONFIG, &replicas, replicas, "replicas");
  int arg_done = process_args(&arg_state, argc, argv);
  if (!do_daemonize && !do_foreground && !do_interactive && !arg_state.nfile_arguments)
    help(&arg_state, 0);
  if (do_daemonize) daemon(1,0);
  expand_filename(user_directory, sizeof(user_directory));
  expand_filename(system_directory, sizeof(system_directory));
  expand_filename(metadata_directory, sizeof(metadata_directory));
  expand_filename(data_directory, sizeof(data_directory));
  expand_filename(checkpoint_directory, sizeof(checkpoint_directory));
  expand_filename(chunks_directory, sizeof(chunks_directory));
  if (xmkdir(user_directory, 0700) < 0 && errno != EEXIST)
    fail("unable to make user directory '%s' : %d %s", user_directory, errno, strerror(errno));
  init_sector();
  open_session();
  Service::start_all();
  int start = do_daemonize || do_foreground || !*cluster_name;
  if (start)
    start_vz();
  int r = 0;
  if (do_unit_tests)
    r = UnitTest::run_all();
  else {
    if (!(do_daemonize || do_foreground)) {
      Cli cli(STDOUT_FILENO, do_interactive ? STDIN_FILENO : -1);
      if (arg_state.nfile_arguments)
        do_cmd(cli, argv + 1 + arg_done);
      else if (do_interactive)
        cli.main();
    } else {
      while (1) sleep(INT_MAX); // this thread could be reused
    }
  }
  stop_vz();
  Service::stop_all();
  return r;
}
