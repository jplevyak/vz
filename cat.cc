/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/

#if defined(ITYPE_BYTE) && defined(OTYPE_BYTE)
int run(CTX *vz, int, char**) {
  byte_t t;
  while (input(vz->in, t)) output(vz->out, t);
  return 0;
}
#endif

#if defined(ITYPE_TEXT) && defined(OTYPE_TEXT)
int run(CTX *vz, int, char**) {
  text_t t;
  while (input(vz->in, t)) output(vz->out, t);
  return 0;
}
#endif

#if defined(ITYPE_STRUCT) && defined(OTYPE_STRUCT)
int run(CTX *vz, int, char**) {
  in_t t;
  while (input(vz->in, t)) {
    out_t tt(t)
    output(vz->out, tt);
  }
  return 0;
}
#endif

#if defined(ITYPE_BYTE) && defined(OTYPE_TEXT)

// convert bytes to lines
int run(CTX *vz, int, char**) {
  byte_t t;
  buf_t line;
  int send_to_previous = vz->id;
  while (input(vz->in, t)) {
    if (t == '\r') // handle DOS
      continue;
    if (send_to_previous)
      send(vz->node, vz->id-1, t);
    else { 
      if (t == '\n') {
        if (send_to_previous)
          send_to_previous = 0;
        else
          output(vz->out, line);
        reset(line);
      } else
        append(line, t);
    }
  }
  done(vz->node);
  while (input(vz->node, t)) {
    if (send_to_previous)
      send(vz->node, vz->id-1, t);
    else { 
      if (t == '\n') {
        if (send_to_previous)
          send_to_previous = 0;
        else
          output(vz->out, line);
        reset(line);
      } else
        append(line, t);
    }
  }
  dealloc(line);
  return 0;
}

#endif

#if defined(ITYPE_BYTE) && defined(OTYPE_STRUCT)

// convert bytes to structs
int run(CTX *vz, int, char**) {
  byte_t t;
  buf_t buf;
  int64_t i = index(vz->in);
  int send_to_previous = 0;
  if (i > 0)
    send_to_previous = i % (int64_t)sizeof(out_t);
  while (input(vz->in, t)) {
    if (send_to_previous)
      send(vz->node, vz->id-1, t);
    else
      append(buf, t);
    if (buf.len == (uint64_t)sizeof(out_t)) {
      if (send_to_previous)
        send_to_previous = 0;
      else
        output(vz->out, *(out_t*)buf.str);
      reset(buf);
    }
  }
  done(vz->node);
  while (input(vz->node, t)) {
    if (send_to_previous)
      send(vz->node, vz->id-1, t);
    else
      append(buf, t);
    if (buf.len == (uint64_t)sizeof(out_t)) {
      if (send_to_previous)
        send_to_previous = 0;
      else
        output(vz->out, *(out_t*)buf.str);
      reset(buf);
    }
  }
  dealloc(buf);
  return 0;
}

#endif

#if defined(ITYPE_TEXT) && defined(OTYPE_STRUCT)

#include <stdio.h>
#include <string.h>
#include <string>

int run(CTX *vz, int, char**) {
  text_t line;
  char sep = '\t';
  int not_done = 1;
  while ((not_done = input(vz->in, line))) {
    if (!line.len)
      continue;
    if (memchr(line.str, '\t', line.len))
      break;
    if (memchr(line.str, ';', line.len)) {
      sep = ';';
      break;
    }
    if (memchr(line.str, ',', line.len)) {
      sep = ',';
      break;
    }
    sep = ' ';
    break;
  }
  if (not_done) do {
    out_t t;
    char *p = line.str;
    parse(t, line, sep);
    output(vz->out, t);
  } while (input(vz->in, line));
  return 0;
}

#endif

#if defined(ITYPE_STRUCT) && defined(OTYPE_TEXT)

#include <stdio.h>
#include <string.h>
#include <string>

int run(CTX *vz, int, char**) {
  in_t i;
  buf_t line;
  while (input(vz->in, i)) {
    reset(line);
    print(line, i);
    output(vz->out, line);
  }
  dealloc(line);
  return 0;
}

#endif







