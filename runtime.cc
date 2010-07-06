/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#include "defs.h"
#include "vz.h"
#include "sphere.h"

using namespace vz;

namespace vz {

struct Channel {
  const SInput *in;
  int64_t iin;
  const SOutput *out;
  int64_t iout;
  int64_t rout;
  Channel() : in(0), iin(0), out(0), iout(0), rout(0) {}
};

int input(Chan c, byte_t &b) {
  if (!c->in)
    return 0;
  if (c->iin >= c->in->m_iRows)
    return 0;
  b = (byte_t)c->in->m_pcUnit[c->iin];
  c->iin++;
  return 1;
}

int input(Chan c, text_t &l) {
  if (!c->in)
    return 0;
  if (c->iin >= c->in->m_iRows)
    return 0;
  l.str = c->in->m_pcUnit + c->in->m_pllIndex[c->iin];
  l.len = c->in->m_pllIndex[c->iin+1] - c->in->m_pllIndex[c->iin];
  c->iin++;
  return 1;
}

int input(Chan c, void *p, size_t s) {
  if (!c->in)
    return 0;
  if (c->iin >= c->in->m_iRows)
    return 0;
  memcpy(p, c->in->m_pcUnit + c->in->m_pllIndex[c->iin],
         c->in->m_pllIndex[c->iin+1] - c->in->m_pllIndex[c->iin]);
  c->iin++;
  return 1;
}

void send(Chan c, int id, byte_t b) {
  assert(0);
}

void done(Chan c) {
}

int64_t index(Chan c) {
  return 0;
}

void output(Chan c, text_t &t) {
  if (t.len) {
    if ((int)(c->iout + t.len) > c->out->m_iBufSize)
      ((SOutput*)c->out)->resizeResBuf((int)c->iout + ((int)t.len > c->iout ? (int)t.len : c->iout));
    memcpy(c->out->m_pcResult + c->iout, t.str, t.len);
    c->iout += t.len;
  }

  ((SOutput*)c->out)->m_iResSize = (int)c->iout;
  if (c->rout + 1 > c->out->m_iIndSize)
    ((SOutput*)c->out)->resizeIdxBuf(c->rout + c->rout);
  c->out->m_pllIndex[c->rout] = c->iout;
  c->rout++;
  ((SOutput*)c->out)->m_iRows = (int)c->rout-1;
}

void output(Chan c, byte_t t) {
  if ((int)(c->iout + sizeof(byte_t)) > c->out->m_iBufSize)
    ((SOutput*)c->out)->resizeResBuf((int)(c->iout + c->iout));
  *((byte_t*)(c->out->m_pcResult + c->iout)) = t;
  c->iout += sizeof(byte_t);

  ((SOutput*)c->out)->m_iResSize = (int)c->iout;
  if (c->rout + 1 > c->out->m_iIndSize)
    ((SOutput*)c->out)->resizeIdxBuf(c->rout + c->rout);
  c->out->m_pllIndex[c->rout] = c->iout;
  c->rout++;
  ((SOutput*)c->out)->m_iRows = (int)c->rout-1;
}

void output(Chan c, void *p, size_t s) {
  if ((int)(c->iout + s) > c->out->m_iBufSize)
    ((SOutput*)c->out)->resizeResBuf((int)(c->iout + c->iout));
  memcpy(c->out->m_pcResult + c->iout, p, s);
  c->iout += s;

  ((SOutput*)c->out)->m_iResSize = (int)c->iout;
  if (c->rout + 1 > c->out->m_iIndSize)
    ((SOutput*)c->out)->resizeIdxBuf(c->rout + c->rout);
  c->out->m_pllIndex[c->rout] = c->iout;
  c->rout++;
  ((SOutput*)c->out)->m_iRows = (int)c->rout-1;
}

}

int _vz_call_run(const SInput *input, const SOutput *output, SFile *file, VZ__RUN_ r) {
  CTX ctx;
  ctx.id = 0;
  ctx.n_in = 1;
  ctx.n_out = 1;
  Channel in, out, node, *pin = &in, *pout = &out;
  in.in = input;
  out.out = output;
  ctx.in = &pin;
  ctx.out = &pout;
  ctx.node = &node;

  // have to add the zero manually (who would have guessed)
  if (out.rout + 1 > out.out->m_iIndSize)
    ((SOutput*)out.out)->resizeIdxBuf(out.rout + out.rout);
  out.rout++;
  out.out->m_pllIndex[out.rout] = 0;
  return r(&ctx, 0, 0);
}

