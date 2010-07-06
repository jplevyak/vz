/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#include "defs.h"

void
get_version(char *v) {
  v += sprintf(v, "%d.%d", MAJOR_VERSION, MINOR_VERSION);
  if (strcmp("",BUILD_VERSION)) {
    char build[10];
    strncpy(build, BUILD_VERSION, 8);
    build[8] = 0;
    v += sprintf(v, " (build %s)", build);
  }
}

