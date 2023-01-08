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

/************* Global Variables *******************/
static constexpr size_t k_PatternSize = 100; // (aaabbc)*
static constexpr size_t k_NumberOfThreads = 3;
static constexpr uint64_t k_SleepForMilliseconds = 20;
/*************************************************/

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

/*******Diagnostic thread safe logger*************/
struct ThreadSafeLog
{
	inline ThreadSafeLog& operator<<(bool Condition)
	{
		std::lock_guard<std::mutex> lock(s_Lock);
		uint64_t epoch = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
			).count();
		assert(m_Stream.find(epoch) == m_Stream.end());
		m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load() << ", thread_id: " << std::this_thread::get_id() << ", with condition: " << Condition << std::endl;
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

		m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load() << ", thread_id: " << ThreadId << std::endl;
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
		m_Stream[epoch] << "ThreadContext::s_Counter: " << ThreadContext::s_Counter.load() << ", thread_id : " << std::this_thread::get_id() << ", " << Line << std::endl;
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
	std::mutex s_Lock{};

	// Map with key as epoch to print logs by timewise
	std::map<uint64_t, std::stringstream> m_Stream{};
};

// Function to know if pattern is complete
static inline bool IsPatterComplete()
{
	return ThreadContext::s_PatternCount >= k_PatternSize;
}

int main()
{
#if 1
	/*
	By default clog is disabled here.
	change #if 1 to #if 0 to enable diagnostic logging.
	*/
	std::clog.setstate(std::ios_base::failbit);
#endif

	static ThreadSafeLog g_log{};

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
						bool condition = ThreadContext::s_Counter.load() >= 0 && ThreadContext::s_Counter.load() <= 2 || IsPatterComplete();
						g_log << "Blocking: " << std::this_thread::get_id() << ", with condition=" << condition;
						fflush(stdout);
						return condition;
					}
				);
				assert(ThreadContext::s_Counter.load() >= 0 && ThreadContext::s_Counter.load() <= 2);
				g_log << "Unlocking: " << std::this_thread::get_id();
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
					g_log << "Unblocking: s_Cv[1]";
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
						bool condition = ThreadContext::s_Counter.load() >= 3 && ThreadContext::s_Counter.load() <= 4 || IsPatterComplete();
						g_log << "Blocking: " << std::this_thread::get_id() << ", with condition=" << condition;
						return condition;
					}
				);
				if (!IsPatterComplete())
				{
					assert(ThreadContext::s_Counter.load() >= 3 && ThreadContext::s_Counter.load() <= 4);
				}
				g_log << "Unlocking: " << std::this_thread::get_id();
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
					g_log << "Unlocking: s_Cv[2]";
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
						bool condition = ThreadContext::s_Counter.load() == 5 || IsPatterComplete();
						g_log << "Blocking: " << std::this_thread::get_id() << ", with condition=" << condition;
						return condition;
					}
				);
				if (!IsPatterComplete())
				{
					assert(ThreadContext::s_Counter.load() == 5);
				}
				g_log << "Unlocking: " << std::this_thread::get_id();
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
					g_log << "Unlocking: s_Cv[0]";
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
			g_log << " Current Thread is: " << std::this_thread::get_id();
			localContext.cb_WaitForCondition();
			if (IsPatterComplete())
			{
				localContext.cb_ResetCounterToDefault();
				localContext.cb_TryNotifyNextThread();
				g_log << "Pattern complete, Exiting from thread: " << std::this_thread::get_id() << ", with Counter=" << ThreadContext::s_Counter.load();
				return;
			}
			localContext.cb_PerformPrimaryTask();
			localContext.cb_TryNotifyNextThread();
		}
			}
		);
	}

	for (size_t i = 0; i < k_NumberOfThreads; ++i)
	{
		threads[i].join();
	}

	return 0;
}