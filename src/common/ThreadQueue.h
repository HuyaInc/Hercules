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

#include <deque>
#include <vector>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <utility>

namespace hercules
{

    template <typename T, typename D = std::deque<T>>
    class ThreadQueue
    {
    public:
        ThreadQueue() : m_size(0) {}

    public:
        typedef D queue_type;

        T front();

        bool pop_front(T &t, size_t millsecond = 0, bool wait = true);

        bool pop_front();

        void notifyT();

        void push_back(const T &t, bool notify = true);

        void push_back(const queue_type &qt, bool notify = true);

        void push_front(const T &t, bool notify = true);

        void push_front(const queue_type &qt, bool notify = true);

        bool swap(queue_type &q, size_t millsecond = 0, bool wait = true);

        size_t size() const;

        void clear();

        bool empty() const;

    protected:
        ThreadQueue(const ThreadQueue &) = delete;
        ThreadQueue(ThreadQueue &&) = delete;
        ThreadQueue &operator=(const ThreadQueue &) = delete;
        ThreadQueue &operator=(ThreadQueue &&) = delete;

    protected:
        queue_type m_queue;

        size_t m_size;

        std::condition_variable m_cond;

        mutable std::mutex m_mutex;
    };

    template <typename T, typename D>
    T ThreadQueue<T, D>::front()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_queue.front();
    }

    template <typename T, typename D>
    bool ThreadQueue<T, D>::pop_front(T &t, size_t millsecond, bool wait)
    {
        if (wait)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_queue.empty())
            {
                if (millsecond == 0)
                {
                    return false;
                }
                if (millsecond == (size_t)-1)
                {
                    m_cond.wait(lock);
                }
                else
                {
                    if (m_cond.wait_for(lock, std::chrono::milliseconds(millsecond)) ==
                        std::cv_status::timeout)
                    {
                        return false;
                    }
                }
            }

            if (m_queue.empty())
            {
                return false;
            }

            t = m_queue.front();
            m_queue.pop_front();
            assert(m_size > 0);
            --m_size;

            return true;
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                return false;
            }

            t = m_queue.front();
            m_queue.pop_front();
            assert(m_size > 0);
            --m_size;

            return true;
        }
    }

    template <typename T, typename D>
    bool ThreadQueue<T, D>::pop_front()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty())
        {
            return false;
        }

        m_queue.pop_front();
        assert(m_size > 0);
        --m_size;

        return true;
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::notifyT()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.notify_all();
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::push_back(const T &t, bool notify)
    {
        if (notify)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_queue.push_back(t);
            ++m_size;

            m_cond.notify_one();
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(t);
            ++m_size;
        }
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::push_back(const queue_type &qt, bool notify)
    {
        if (notify)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            typename queue_type::const_iterator it = qt.begin();
            typename queue_type::const_iterator itEnd = qt.end();
            while (it != itEnd)
            {
                m_queue.push_back(*it);
                ++it;
                ++m_size;
            }
            m_cond.notify_all();
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            typename queue_type::const_iterator it = qt.begin();
            typename queue_type::const_iterator itEnd = qt.end();
            while (it != itEnd)
            {
                m_queue.push_back(*it);
                ++it;
                ++m_size;
            }
        }
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::push_front(const T &t, bool notify)
    {
        if (notify)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cond.notify_one();

            m_queue.push_front(t);

            ++m_size;
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_queue.push_front(t);

            ++m_size;
        }
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::push_front(const queue_type &qt, bool notify)
    {
        if (notify)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            typename queue_type::const_iterator it = qt.begin();
            typename queue_type::const_iterator itEnd = qt.end();
            while (it != itEnd)
            {
                m_queue.push_front(*it);
                ++it;
                ++m_size;
            }

            m_cond.notify_all();
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            typename queue_type::const_iterator it = qt.begin();
            typename queue_type::const_iterator itEnd = qt.end();
            while (it != itEnd)
            {
                m_queue.push_front(*it);
                ++it;
                ++m_size;
            }
        }
    }

    template <typename T, typename D>
    bool ThreadQueue<T, D>::swap(queue_type &q, size_t millsecond, bool wait)
    {
        if (wait)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_queue.empty())
            {
                if (millsecond == 0)
                {
                    return false;
                }
                if (millsecond == (size_t)-1)
                {
                    m_cond.wait(lock);
                }
                else
                {
                    if (m_cond.wait_for(lock, std::chrono::milliseconds(millsecond)) ==
                        std::cv_status::timeout)
                    {
                        return false;
                    }
                }
            }

            if (m_queue.empty())
            {
                return false;
            }

            q.swap(m_queue);
            m_size = m_queue.size();

            return true;
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_queue.empty())
            {
                return false;
            }

            q.swap(m_queue);

            m_size = m_queue.size();

            return true;
        }
    }

    template <typename T, typename D>
    size_t ThreadQueue<T, D>::size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

    template <typename T, typename D>
    void ThreadQueue<T, D>::clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
        m_size = 0;
    }

    template <typename T, typename D>
    bool ThreadQueue<T, D>::empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

} // namespace hercules
