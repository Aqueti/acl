/**
 *    \copyright Copyright 2021 Aqueti, Inc. All rights reserved.
 *    \license This project is released under the MIT Public License.
**/

/**
 * \file ThreadPool.h
 **/

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "MultiThread.h"
#include "TSQueue.tcc"

#include <functional>
#include <atomic>
#include "ThreadPool.h"
#include <thread>
#include "Timer.h"

#pragma once

namespace acl
{

    /**
    * \brief class to run thread pool
    **/
    class ThreadPool: public MultiThread, private TSQueue<std::function<void()>>
    {
    public:
        ThreadPool(int numThreads = 1, int maxJobLength = 50, double timeout = 1);

        bool push_job(std::function<void()> f);
        void setTimeout(double timeout);

        using TSQueue<std::function<void()>>::size;
        using TSQueue<std::function<void()>>::delete_all;
        using TSQueue<std::function<void()>>::set_max_size;
        using TSQueue<std::function<void()>>::get_max_size;
        using TSQueue<std::function<void()>>::wait_until_empty;

    private:
        std::atomic<double> m_timeout;                  //!< Timeout value of the thread pool
        virtual void mainLoop();
    };
}

#endif /* THREADPOOL_H_ */
