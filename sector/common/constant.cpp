/*****************************************************************************
Copyright (c) 2005 - 2009, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 07/15/2009
*****************************************************************************/

#include <sector.h>

using namespace std;


map<int, string> SectorError::s_mErrorMsg;

int SectorError::init()
{
   s_mErrorMsg.clear();
   s_mErrorMsg[-1] = "Unknown error";
   s_mErrorMsg[-1001] = "Permission denied";
   s_mErrorMsg[-1002] = "File exists";
   s_mErrorMsg[-1003] = "No such file or directory";
   s_mErrorMsg[-1004] = "Device or resource busy";
   s_mErrorMsg[-1005] = "Local filesystem error";
   s_mErrorMsg[-1006] = "Directory not empty";
   s_mErrorMsg[-2000] = "Security check failed";
   s_mErrorMsg[-2001] = "No certificate found or wrong certificate";
   s_mErrorMsg[-2002] = "Account does not exist";
   s_mErrorMsg[-2003] = "Password is incorrect";
   s_mErrorMsg[-2004] = "Request from an illegal IP address";
   s_mErrorMsg[-2005] = "Failed to initialize SSL CTX";
   s_mErrorMsg[-2006] = "No response from security server";
   s_mErrorMsg[-2007] = "Client timeout";
   s_mErrorMsg[-3000] = "Connection failed";
   s_mErrorMsg[-4000] = "Out of resource";
   s_mErrorMsg[-5000] = "Timeout";
   s_mErrorMsg[-6000] = "Invalid argument";
   s_mErrorMsg[-6001] = "Operation not supported";
   s_mErrorMsg[-7001] = "Bucket process failed";

   return s_mErrorMsg.size();
}

string SectorError::getErrorMsg(int ecode)
{
   map<int, string>::const_iterator i = s_mErrorMsg.find(ecode);
   if (i == s_mErrorMsg.end())
      return "unknown error.";

   return i->second;      
}
