/**
 *    \copyright Copyright 2021 Aqueti, Inc. All rights reserved.
 *    \license This project is released under the MIT Public License.
**/

/******************************************************************************
 *
 * \file ThreadWorker.cpp
 * \brief 
 * \author Andrew Ferg
 *
 * Copyright Aqueti 2019
 *
 *****************************************************************************/

#include "ThreadWorker.h"

namespace acl {

ThreadWorker::ThreadWorker(std::function<void()> f)
{
    m_mainLoopFunction = f;
}

ThreadWorker::~ThreadWorker()
{
    Stop();
    Join();
}

void ThreadWorker::setMainLoopFunction(std::function<void()> f)
{
    std::lock_guard<std::mutex> l(m_mainLoopMutex);
    m_mainLoopFunction = f;
}

void ThreadWorker::mainLoop()
{
    std::lock_guard<std::mutex> l(m_mainLoopMutex);
    if (m_mainLoopFunction) {
        m_mainLoopFunction();
    }
}

} //end namespace acl
