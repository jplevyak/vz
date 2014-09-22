/*
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#ifndef _vz_H_
#define _vz_H_

#include <stdint.h>
#include <stddef.h>

namespace vz {

typedef unsigned char byte_t;
struct text_t { char *str; uint64_t len; text_t() : str(0), len(0) {} };
struct buf_t : text_t { uint64_t max_len; buf_t() : max_len(0) {} };
typedef struct Channel *Chan;

struct CTX {
  int           id;                   // 0..node_ids-1
  int           n_in, n_out;
  Chan          *in, node, *out;
};


int input(Chan, byte_t &);            // returns 0 when at end
int input(Chan, text_t &);
int input(Chan, void *p, size_t s);
void output(Chan, byte_t);
void output(Chan, text_t &);
void output(Chan, void *p, size_t s);
void send(Chan, int id, byte_t);
void send(Chan, int id, text_t &);
Chan makeChan(int n);                 // make a channel with n sinks
Chan sink(Chan, int i);               // returns the i-th sink

void done(Chan);
int64_t index(Chan);                  // returns -1 if not a table
int64_t ids(Chan);                    // returns number of ids, 1 for table
int phase(Chan, int i_am_done);       // returns 0 if consensus is done

void append(buf_t &, byte_t);
void append(buf_t &b, void *p, size_t s);
void reset(buf_t &t);
void dealloc(buf_t &t);
}

//
// Main Entry Point
// 
int run(vz::CTX*, int argc, char *argv[]);

#include "vz_inline.h"

#endif
