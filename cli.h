/* -*-Mode: c++; -*-
Copyright (c) 2010 John Plevyak, All Rights Reserved
*/
#ifndef _cli_H_
#define _cli_H_

#include "limits.h"
#include <string>

class Cli : public Conn { public:
  Cli *init(Server *aserver, int aifd, int aofd);
  int main(char *cmd = 0);

  int iprogram;
  char cwd[PATH_MAX];
  std::string ifile;
  std::string itype;
  std::string ofile;
  std::string otype;

  int done();
  Cli();
  Cli(int aofd, int aifd = -1);
  ~Cli();
};

#endif
