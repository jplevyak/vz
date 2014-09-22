#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <sphere.h>

using namespace std;

extern "C"
{

int mr_word(const SInput* input, SOutput* output, SFile* file)
{
   string ifile = file->m_strHomeDir + input->m_pcUnit;
   string ofile = ifile + ".result";

   system((string("") + "../examples/funcs/mr_word" + " " +  " " + ifile + " " +  " > " + ofile).c_str());

   output->m_iRows = 0;

   string sfile;
   for (int i = 1, n = strlen(input->m_pcUnit); i < n; ++ i)
   {
      if (input->m_pcUnit[i] == '/')
         sfile.push_back('.');
      else
         sfile.push_back(input->m_pcUnit[i]);
   }
   sfile = string("test/xxx") + "/" + sfile;

   system(("mkdir -p " + file->m_strHomeDir + "test/xxx").c_str());
   system(("mv " + ofile + " " + file->m_strHomeDir + sfile).c_str());

   file->m_sstrFiles.insert(sfile);

   return 0;
}

}
