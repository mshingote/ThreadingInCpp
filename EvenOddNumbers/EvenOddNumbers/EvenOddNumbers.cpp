// EvenOddNumbers.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cassert>

static constexpr uint64_t k_MaxNumber = 100;

struct ThreadContext
{
    static std::mutex s_Lock[2];
    static std::condition_variable s_Cv[2];
    static uint64_t s_Number;
};
std::mutex ThreadContext::s_Lock[2];
std::condition_variable ThreadContext::s_Cv[2];
uint64_t ThreadContext::s_Number;

static bool IsNumberReachedAtMax()
{
    return ThreadContext::s_Number >= k_MaxNumber;
}

void ThreadFunction(uint8_t Reminder)
{
    while (!IsNumberReachedAtMax())
    {
        std::unique_lock<std::mutex> lock(ThreadContext::s_Lock[Reminder]);
        ThreadContext::s_Cv[Reminder].wait(lock, [&Reminder]()
            {
                if (IsNumberReachedAtMax())
                {
                    return true;
                }
                return ThreadContext::s_Number % 2 == Reminder;
            }
        );
        if (IsNumberReachedAtMax())
        {
            return;
        }
        std::clog << (Reminder ? "\t\t\t\t\t" : "") << "\tCurrent thread: " << std::this_thread::get_id() << ", Number: " << ThreadContext::s_Number << "\n";
        ++ThreadContext::s_Number;
        ThreadContext::s_Cv[!Reminder].notify_one();
    }
}
int main()
{
    std::thread EvenNumberProcessThread(ThreadFunction, 0);
    std::thread OddNumberProcessThread(ThreadFunction, 1);

    EvenNumberProcessThread.join();
    OddNumberProcessThread.join();
}