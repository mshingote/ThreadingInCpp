#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <chrono>
#include <mutex>
#include <cassert>
#include <functional>
#include <atomic>
#include <array>
#include "ThreadSafeLogger.hpp"

/************* Global Variables *******************/
static constexpr size_t k_PatternSize = 2; // (aaabbc)*

// Function to know if pattern is complete
static inline bool IsPatterComplete()
{
    return ThreadContext::s_PatternCount >= k_PatternSize;
}

int main()
{
    static ThreadSafeLog s_log{};

#ifdef DISABLE_CLOG
    // Disabling logs
#define s_log if constexpr(0) s_log
#endif

    static constexpr size_t k_NumberOfThreads = 3;

    // Array of std::thread with k_NumberOfThreads size
    std::array<std::thread, k_NumberOfThreads> threads{};

    // Preparing context for each thread function.
    ThreadContext context[k_NumberOfThreads] =
    {
        // Thread 1 data
        {
            .cb_WaitForCondition = []()
            {
                std::unique_lock<std::mutex> lock(ThreadContext::s_Lock[0]);
                ThreadContext::s_Cv[0].wait(lock, []()
                    {
                        bool condition = ThreadContext::s_Counter.load() >= 0 &&
                                         ThreadContext::s_Counter.load() <= 2 ||
                                         IsPatterComplete();
                        s_log << "Blocking: " << std::this_thread::get_id()
                              << ", with condition=" << condition;
                        fflush(stdout);
                        return condition;
                    }
                );
                assert(ThreadContext::s_Counter.load() >= 0 &&
                       ThreadContext::s_Counter.load() <= 2);
                s_log << "Unlocking: " << std::this_thread::get_id();
                fflush(stdout);
            },
            .cb_PerformPrimaryTask = []() {std::cout << "a"; ++ThreadContext::s_Counter; },
            .cb_ResetCounterToDefault = []() { ThreadContext::s_Counter = 3; },
            .cb_TryNotifyNextThread = []()
            {
                assert(ThreadContext::s_Counter.load() <= 4);
                if (ThreadContext::s_Counter == 3)
                {
                    ThreadContext::s_Cv[1].notify_one();
                    s_log << "Unblocking: s_Cv[1]";
                }
            }
        },

        // Thread 2 data
        {
            .cb_WaitForCondition = []()
            {
                std::unique_lock<std::mutex> lock(ThreadContext::s_Lock[1]);
                ThreadContext::s_Cv[1].wait(lock, []()
                    {
                        bool condition = ThreadContext::s_Counter.load() >= 3 &&
                                         ThreadContext::s_Counter.load() <= 4 ||
                                         IsPatterComplete();
                        s_log << "Blocking: " << std::this_thread::get_id()
                              << ", with condition=" << condition;
                        return condition;
                    }
                );
                if (!IsPatterComplete())
                {
                    assert(ThreadContext::s_Counter.load() >= 3 &&
                           ThreadContext::s_Counter.load() <= 4);
                }
                s_log << "Unlocking: " << std::this_thread::get_id();
                fflush(stdout);
            },
            .cb_PerformPrimaryTask = []() {std::cout << "b"; ++ThreadContext::s_Counter; },
            .cb_ResetCounterToDefault = []() { ThreadContext::s_Counter = 5; },
            .cb_TryNotifyNextThread = []()
            {
                assert(ThreadContext::s_Counter.load() <= 5);
                if (ThreadContext::s_Counter.load() == 5)
                {
                    ThreadContext::s_Cv[2].notify_one();
                    s_log << "Unlocking: s_Cv[2]";
                }
            }
        },

        // Thread 3 data
        {
            .cb_WaitForCondition = []()
            {
                std::unique_lock<std::mutex> lock(ThreadContext::s_Lock[2]);
                ThreadContext::s_Cv[2].wait(lock, []()
                    {
                        bool condition = ThreadContext::s_Counter.load() == 5 ||
                                         IsPatterComplete();
                        s_log << "Blocking: " << std::this_thread::get_id()
                              << ", with condition=" << condition;
                        return condition;
                    }
                );
                if (!IsPatterComplete())
                {
                    assert(ThreadContext::s_Counter.load() == 5);
                }
                s_log << "Unlocking: " << std::this_thread::get_id();
                fflush(stdout);
            },
            .cb_PerformPrimaryTask = []() {std::cout << "c"; ++ThreadContext::s_PatternCount; },
            .cb_ResetCounterToDefault = []() { ThreadContext::s_Counter = 0; },
            .cb_TryNotifyNextThread = []()
            {
                if (IsPatterComplete())
                {
                    assert(ThreadContext::s_Counter.load() <= 5);
                }
                if (ThreadContext::s_Counter.load() == 5)
                {
                    ThreadContext::s_Counter = 0;
                    ThreadContext::s_Cv[0].notify_one();
                    s_log << "Unlocking: s_Cv[0]";
                }
            }
        }
    };

    for (size_t i = 0; i < k_NumberOfThreads; ++i)
    {
        threads[i] = std::thread([&context, i]()
        {
                auto& localContext = context[i];
                while (ThreadContext::s_PatternCount <= k_PatternSize)
                {
                    s_log << " Current Thread is: " << std::this_thread::get_id();
                    localContext.cb_WaitForCondition();
                    if (IsPatterComplete())
                    {
                        localContext.cb_ResetCounterToDefault();
                        localContext.cb_TryNotifyNextThread();
                        s_log << "Pattern complete, Exiting from thread: " << std::this_thread::get_id()
                              << ", with Counter=" << ThreadContext::s_Counter.load();
                        return;
                    }
                    localContext.cb_PerformPrimaryTask();
                    localContext.cb_TryNotifyNextThread();
                }
        });
    }

    for (size_t i = 0; i < k_NumberOfThreads; ++i)
    {
        threads[i].join();
    }

    return 0;
}