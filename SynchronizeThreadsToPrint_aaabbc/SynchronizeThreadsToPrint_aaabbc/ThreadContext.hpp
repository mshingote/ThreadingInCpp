#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <functional>

/************* Global Variables *******************/
static constexpr size_t k_NumberOfThreads = 3;

/**********ThreadContext is thread data **********/
struct ThreadContext
{
    // Used to take lock on thread and context preparation
    static std::mutex s_Lock[k_NumberOfThreads];

    // Used to wait on lock and notify necessary threads
    static std::condition_variable s_Cv[k_NumberOfThreads];

    // Used for context switching between the threads
    static std::atomic_uint16_t s_Counter;

    // Used to track pattern
    static std::atomic_uint16_t s_PatternCount;

    // Used to block thread until specific condition met
    std::function<void()> cb_WaitForCondition{};

    // Used to perform task, e.g. stdout "a"
    std::function<void()> cb_PerformPrimaryTask{};

    // Used to reset s_Counter after desired patter is complete
    std::function<void()> cb_ResetCounterToDefault{};

    // Used to wakeup next thread after specific condition met
    std::function<void()> cb_TryNotifyNextThread{};
};

std::atomic_uint16_t ThreadContext::s_Counter;
std::atomic_uint16_t ThreadContext::s_PatternCount;
std::condition_variable ThreadContext::s_Cv[k_NumberOfThreads];
std::mutex ThreadContext::s_Lock[k_NumberOfThreads];