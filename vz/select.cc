/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/

#if defined(ITYPE_TEXT) && defined(OTYPE_TEXT)
int run(CTX *vz, int, char**) {
  assert(!"unimplemented");
}
#endif

#if defined(ITYPE_STRUCT) && defined(OTYPE_STRUCT)
int run(CTX *vz, int, char**) {
  in_t t;
  while (input(vz->in, t)) {
    out_t tt;
    select(tt, t);
    output(vz->out, tt);
  }
  return 0;
}
#endif
