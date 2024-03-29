/**
 *    \copyright Copyright 2021 Aqueti, Inc. All rights reserved.
 *    \license This project is released under the MIT Public License.
**/

/**
 * \file MultiThread.h
 **/

#pragma once

#include "Thread.h"
#include <vector>
#include <map>

namespace acl
{

class MultiThread : public Thread
{
public:
    MultiThread(int numThreads=2): m_numThreads(numThreads) {}
    virtual ~MultiThread();
    virtual bool setNumThreads(unsigned);
    virtual bool Start();
    virtual bool Join();
    virtual bool Detach();

protected:
    virtual int getMyId();

    unsigned                    m_numThreads;   //<! Number of threads to spawn
    std::vector<std::thread>    m_threads;      //<! Running threads
    std::map<std::thread::id,int>  m_idMap;     //<! map of Ids
    std::mutex                  m_threadMutex;  //<! mutex for joining threads
};
}
