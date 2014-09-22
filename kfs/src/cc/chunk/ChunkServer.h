//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id: ChunkServer.h 385 2010-05-27 15:58:30Z sriramsrao $
//
// Created 2006/03/16
//
// Copyright 2008 Quantcast Corp.
// Copyright 2006-2008 Kosmix Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// 
//----------------------------------------------------------------------------

#ifndef _CHUNKSERVER_H
#define _CHUNKSERVER_H

#include <string>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>

using std::string;

#include "qcdio/qcutils.h"
#include "libkfsIO/TelemetryClient.h"
#include "libkfsIO/Globals.h"

#include "ChunkManager.h"
#include "ClientManager.h"
#include "ClientSM.h"
#include "MetaServerSM.h"
#include "RemoteSyncSM.h"

namespace KFS
{
namespace chunk
{
class ChunkServer {
public:
    ChunkServer() : mOpCount(0), mKickNetThread(false) { };
    
    void Init();

    void MainLoop(int clientAcceptPort, const std::string & serverHostname);

    bool IsLocalServer(const ServerLocation &location) const {
        return mLocation == location;
    }
    RemoteSyncSMPtr FindServer(const ServerLocation &location,
                               bool connect = true);
    void RemoveServer(RemoteSyncSM *target);

    std::string GetMyLocation() const {
        return mLocation.ToString();
    }

    void ToggleNetThreadKicking (bool v) {
        mKickNetThread = v;
    }

    bool NeedToKickNetThread() {
        return mKickNetThread;
    }
    
    void OpInserted() {
        mOpCount++;
    }

    void OpFinished() {
        mOpCount--;
        if (mOpCount < 0)
            mOpCount = 0;
    }
    int GetNumOps() const {
        return mOpCount;
    }

    void SendTelemetryReport(KfsOp_t op, double timeSpent);

private:
    int mClientAcceptPort;
    // # of ops in the system
    int mOpCount;
    bool mKickNetThread;
    ServerLocation mLocation;
    std::list<RemoteSyncSMPtr> mRemoteSyncers;
    // telemetry reporter...used for notifying slow writes thru this node
    TelemetryClient mTelemetryReporter;
};

// Fork is more reliable, but might confuse existing scripts. Using debugger
// with fork is a little bit more involved.
// The intention here is to do graceful restart, this is not intended as an
// external "nanny" / monitoring / watchdog.

class Restarter;

class Restarter
{
public:
    Restarter()
        : mCwd(0),
          mArgs(0),
          mEnv(0),
          mMaxGracefulRestartSeconds(60 * 6),
          mExitOnRestartFlag(false)
        {}
    ~Restarter()
        { Cleanup(); }
    bool Init(int argc, char **argv)
    {
        ::alarm(0);
        if (::signal(SIGALRM, &Restarter::SigAlrmHandler) == SIG_ERR) {
            QCUtils::FatalError("signal(SIGALRM)", errno);
        }
        Cleanup();
        if (argc < 1 || ! argv) {
            return false;
        }
        for (int len = PATH_MAX; len < PATH_MAX * 1000; len += PATH_MAX) {
            mCwd = (char*)::malloc(len);
            if (! mCwd || ::getcwd(mCwd, len)) {
                break;
            }
            const int err = errno;
            ::free(mCwd);
            mCwd = 0;
            if (err != ERANGE) {
                break;
            }
        }
        if (! mCwd) {
            return false;
        }
        mArgs = new char*[argc + 1];
        int i;
        for (i = 0; i < argc; i++) {
            if (! (mArgs[i] = ::strdup(argv[i]))) {
                Cleanup();
                return false;
            }
        }
        mArgs[i] = 0;
        char** ptr = environ;
        for (i = 0; *ptr; i++, ptr++)
            {}
        mEnv = new char*[i + 1];
        for (i = 0, ptr = environ; *ptr; ) {
            if (! (mEnv[i++] = ::strdup(*ptr++))) {
                Cleanup();
                return false;
            }
        }
        mEnv[i] = 0;
        return true;
    }
    void SetParameters(const Properties& props, string prefix)
    {
        mMaxGracefulRestartSeconds = props.getValue(
            prefix + "maxGracefulRestartSeconds",
            mMaxGracefulRestartSeconds
        );
        mExitOnRestartFlag = props.getValue(
            prefix + "exitOnRestartFlag",
            mExitOnRestartFlag
        );
    }
    string Restart()
    {
        if (! mCwd || ! mArgs || ! mEnv || ! mArgs[0] || ! mArgs[0][0]) {
            return string("not initialized");
        }
        if (! mExitOnRestartFlag) {
            struct stat res = {0};
            if (::stat(mCwd, &res) != 0) {
                return QCUtils::SysError(errno, mCwd);
            }
            if (! S_ISDIR(res.st_mode)) {
                return (mCwd + string(": not a directory"));
            }
            string execpath(mArgs[0][0] == '/' ? mArgs[0] : mCwd);
            if (mArgs[0][0] != '/') {
                if (! execpath.empty() &&
                        execpath.at(execpath.length() - 1) != '/') {
                    execpath += "/";
                }
                execpath += mArgs[0];
            } 
            if (::stat(execpath.c_str(), &res) != 0) {
                return QCUtils::SysError(errno, execpath.c_str());
            }
            if (! S_ISREG(res.st_mode)) {
                return (execpath + string(": not a file"));
            }
        }
        if (::signal(SIGALRM, &Restarter::SigAlrmHandler) == SIG_ERR) {
            QCUtils::FatalError("signal(SIGALRM)", errno);
        }
        if (mMaxGracefulRestartSeconds > 0) {
            if (sInstance) {
                return string("restart in progress");
            }
            sInstance = this;
            if (::atexit(&Restarter::RestartSelf)) {
                sInstance = 0;
                return QCUtils::SysError(errno, "atexit");
            }
            ::alarm((unsigned int)mMaxGracefulRestartSeconds);
            libkfsio::globalNetManager().Shutdown();
        } else {
            ::alarm((unsigned int)-mMaxGracefulRestartSeconds);
            Exec();
        }
        return string();
    }
private:
    char*  mCwd;
    char** mArgs;
    char** mEnv;
    int    mMaxGracefulRestartSeconds;
    bool   mExitOnRestartFlag;

    static Restarter* sInstance;

    static void FreeArgs(char** args)
    {
        if (! args) {
            return;
        }
        char** ptr = args;
        while (*ptr) {
            ::free(*ptr++);
        }
        delete [] args;
    }
    void Cleanup()
    {
        free(mCwd);
        mCwd = 0;
        FreeArgs(mArgs);
        mArgs = 0;
        FreeArgs(mEnv);
        mEnv = 0;
    }
    void Exec()
    {
        if (mExitOnRestartFlag) {
            _exit(0);
        }
#ifdef QC_OS_NAME_LINUX
        ::clearenv();
#else
        environ = 0;
#endif
        if (mEnv) {
            for (char** ptr = mEnv; *ptr; ptr++) {
                if (::putenv(*ptr)) {
                    QCUtils::FatalError("putenv", errno);
                }
            }
        }
        if (::chdir(mCwd) != 0) {
            QCUtils::FatalError(mCwd, errno);
        }
        execvp(mArgs[0], mArgs);
        QCUtils::FatalError(mArgs[0], errno);
    }
    static void RestartSelf()
    {
        if (! sInstance) {
            ::abort();
        }
        sInstance->Exec();
    }
    static void SigAlrmHandler(int /* sig */)
    {
        write(2, "SIGALRM\n", 8);
        ::abort();
    }
};

extern Restarter sRestarter;
extern void verifyExecutingOnNetProcessor();
extern void verifyExecutingOnEventProcessor();
extern void StopNetProcessor(int status);

extern char **environ;
extern ChunkServer gChunkServer;
}
}

#endif // _CHUNKSERVER_H
