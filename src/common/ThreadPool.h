// Copyright 2021 HUYA
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "Singleton.h"
#include "Log.h"

#include <queue>
#include <mutex>
#include <vector>
#include <future>
#include <memory>
#include <thread>
#include <functional>
#include <condition_variable>
#include <utility>

namespace hercules
{

    template <size_t ThreadCount = 0>
    class ThreadPoolT : public Singleton<ThreadPoolT<ThreadCount>>
    {
    public:
        ThreadPoolT()
            : m_stop(false)
        {
            size_t count = ThreadCount == 0 ? std::thread::hardware_concurrency() : ThreadCount;
            for (size_t i = 0; i < count; ++i)
            {
                m_workers.emplace_back([this]() {
                    for (;;)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->m_mutex);
                            this->m_condition.wait(lock, [this]() {
                                return this->m_stop || !this->m_tasks.empty();
                            });
                            if (this->m_stop && this->m_tasks.empty())
                            {
                                return;
                            }
                            task = std::move(this->m_tasks.front());
                            this->m_tasks.pop();
                        }
                        task();
                    }
                });
            }
        }

        template <class F, class... Args>
        inline std::future<typename std::result_of<F(Args...)>::type>
            enqueue(F &&f, Args &&... args)
        {
            typedef typename std::result_of<F(Args...)>::type return_type;
            auto task = std::make_shared<std::packaged_task<return_type()>>
                (std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_stop)
                {
                    // TODO
                    logErr(MIXLOG << "ThreadPoolT stop enqueue error");
                    return res;
                }
                m_tasks.emplace([task]() { (*task)(); });
            }
            m_condition.notify_one();
            return res;
        }

        ~ThreadPoolT()
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_stop = true;
            }

            m_condition.notify_all();

            for (std::thread &worker : m_workers)
            {
                worker.join();
            }
        }

    private:
        bool m_stop;

        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;

        std::mutex m_mutex;
        std::condition_variable m_condition;
    };

    typedef ThreadPoolT<5> ThreadPool;

} // namespace hercules
