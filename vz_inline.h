/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#ifndef _vz_inline_H_
#define _vz_inline_H_

#include <stdlib.h>
#include <string.h>

namespace vz {

const int base_buf_max_len = 128;

inline void output(Chan c, buf_t &b) { output(c, *(text_t*)&b); }
inline void send(Chan c, int id, buf_t &b) { send(c, id, *(text_t*)&b); }
inline void output(Chan *c, buf_t &b) { output(*c, *(text_t*)&b); }
inline void send(Chan *c, int id, buf_t &b) { send(*c, id, *(text_t*)&b); }

inline int input(Chan *c, byte_t &b) { return input(*c, b); }
inline int input(Chan *c, text_t &t) { return input(*c, t); }
inline int input(Chan *c, void *p, size_t s) { return input(*c, p, s); }
inline void output(Chan *c, byte_t b) { return output(*c, b); }
inline void output(Chan *c, text_t &t) { return output(*c, t); }
inline void output(Chan *c, void *p, size_t s) { return output(*c, p, s); }
inline void send(Chan *c, int id, byte_t b) { return send(*c, id, b); }
inline void send(Chan *c, int id, text_t &t) { return send(*c, id, t); }

inline void done(Chan *c) { return done(*c); }
inline int64_t index(Chan *c) { return index(*c); }
inline int64_t ids(Chan *c) { return ids(*c); }
inline int phase(Chan *c, int i_am_done) { return phase(*c, i_am_done); }

inline void append(buf_t &b, byte_t t) {
  if (b.len + sizeof(b) > b.max_len) {
    if (!b.max_len)
      b.max_len = base_buf_max_len;
    while (b.max_len < b.len + sizeof(b))
      b.max_len = b.max_len * 2;
    b.str = (char*)realloc(b.str, (size_t)b.max_len);
  }
  b.str[b.len] = t;
  b.len++;
}

inline void expand(buf_t &b, size_t s) {
  if (b.max_len < (uint64_t)s) {
    b.max_len = s;
    b.str = (char*)realloc(b.str, (size_t)b.max_len);
  }
}

inline void append(buf_t &b, text_t &t) {
  if (b.len + t.len > b.max_len) {
    if (!b.max_len)
      b.max_len = base_buf_max_len;
    while (b.max_len < b.len + t.len)
      b.max_len = b.max_len * 2;
    b.str = (char*)realloc(b.str, (size_t)b.max_len);
  }
  memcpy(b.str + b.len, t.str, t.len);
  b.len += t.len;
}
inline void append(buf_t &b, buf_t &t) { append(b, *(text_t*)&b); }

inline void append(buf_t &b, void *p, size_t s) {
  if (b.len + s > b.max_len) {
    if (!b.max_len)
      b.max_len = base_buf_max_len;
    while (b.max_len < b.len + s)
      b.max_len = b.max_len * 2;
    b.str = (char*)realloc(b.str, (size_t)b.max_len);
  }
  memcpy(b.str + b.len, p, s);
  b.len += s;
}

inline void reset(buf_t &t) { t.len = 0; }
inline void dealloc(buf_t &t) { if (t.str) free(t.str); t.str = 0; t.len = 0; t.max_len = 0; }

template <class C> inline void output(Chan c, C &s) { output(c, (void*)&s, sizeof(s)); }
template <class C> inline void output(Chan *c, C &s) { output(*c, (void*)&s, sizeof(s)); }
template <class C> inline int input(Chan c, C &s) { return input(c, (void*)&s, sizeof(s)); }
template <class C> inline int input(Chan *c, C &s) { return input(*c, (void*)&s, sizeof(s)); }

#define xisdigit(_c) ((((unsigned)_c) - '0') < 10)

static inline char *xmemchr(char *p, int c, size_t s) {
  char *e = p + s;
  for (;p < e; p++) if (c == *p) return p;
  return 0;
}

static inline uint64_t vz_atoull(char *s, char *e) {
  uint64_t n = 0;
  while (s < e && *s == ' ') s++;
  if (s >= e)
    return 0;
  if (xisdigit(*s)) {
    n = *s - '0';
    s++;
    if (s >= e) return n;
  }
  while (xisdigit(*s)) {
    n *= 10;
    n += *s - '0';
    s++;
    if (s >= e) return n;
  }
  return n;
}

static inline int64_t vz_atoll(char *s, char *e) {
  int64_t n = 0;
  int neg = 0;
  while (s < e && *s == ' ') s++;
  if (s >= e)
    return 0;
  if (*s == '-') {
    s++;
    neg = 1;
    if (s >= e) return 0;
  }
  if (xisdigit(*s)) {
    n = *s - '0';
    s++;
    if (s >= e) return neg ? -n : n;
  }
  while (xisdigit(*s)) {
    n *= 10;
    n += *s - '0';
    s++;
    if (s >= e) return neg ? -n : n;
  }
  return neg ? -n : n;
}

}

#endif
