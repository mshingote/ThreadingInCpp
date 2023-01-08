#pragma once

#include <map>
#include <cassert>
#include <string>
#include <thread>
#include <iomanip>
#include <mutex>
#include <utility>
#include <iostream>

#include "ThreadContext.hpp"

#define DISABLE_CLOG

/************* Global Variables *******************/
static constexpr uint64_t k_SleepForMilliseconds = 20;

/*******Diagnostic thread safe logger*************/
struct ThreadSafeLog
{
    ThreadSafeLog()
    {
#ifdef DISABLE_CLOG
    /*
        By default clog is disabled here.
    */
    std::clog.setstate(std::ios_base::failbit);
#endif

    }
    inline ThreadSafeLog& operator<<(bool Condition)
    {
        std::lock_guard<std::mutex> lock(s_Lock);
        uint64_t epoch = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        assert(m_Stream.find(epoch) == m_Stream.end());
        m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load()
            << ", thread_id: " << std::this_thread::get_id()
            << ", with condition: " << Condition << std::endl;
        Print();
        return *this;
    }

    inline ThreadSafeLog& operator<<(const std::thread::id& ThreadId)
    {
        std::lock_guard<std::mutex> lock(s_Lock);
        uint64_t epoch = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        assert(m_Stream.find(epoch) == m_Stream.end());

        m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load()
            << ", thread_id: " << ThreadId << std::endl;
        Print();
        return *this;
    }

    inline ThreadSafeLog& operator<<(std::string Line)
    {
        std::lock_guard<std::mutex> lock(s_Lock);
        uint64_t epoch = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        assert(m_Stream.find(epoch) == m_Stream.end());
        m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load()
            << ", thread_id : " << std::this_thread::get_id() << ", " << Line << std::endl;
        Print();
        return *this;
    }

    inline void Print()
    {
        for (auto& itr : m_Stream)
        {
            std::clog << "Time: " << itr.first << ", " << itr.second.str();
            itr.second.clear();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(16ULL, k_SleepForMilliseconds)));
    }
private:
    static std::mutex s_Lock;

    // Map with key as epoch to print logs by timewise;
    static std::map<uint64_t, std::stringstream> m_Stream;
};

std::mutex ThreadSafeLog::s_Lock;
std::map<uint64_t, std::stringstream> ThreadSafeLog::m_Stream;