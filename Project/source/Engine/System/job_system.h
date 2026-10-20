#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

// Small reusable worker pool for CPU-only work. GPU commands and resource
// creation must stay on the render thread.
class JobSystem
{
public:
	static JobSystem& instance()
	{
		static JobSystem job_system;
		return job_system;
	}

	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	size_t worker_count() const noexcept { return workers_.size(); }

	template <typename Function>
	void parallel_for(size_t count, size_t minimum_parallel_count, Function&& function)
	{
		if (count < minimum_parallel_count || workers_.empty())
		{
			for (size_t index = 0; index < count; ++index)
				function(index);
			return;
		}

		struct Group
		{
			std::atomic<size_t> remaining{ 0 };
			std::atomic<size_t> next_index{ 0 };
			std::mutex mutex;
			std::condition_variable completed;
			std::exception_ptr exception;
		};

		auto group = std::make_shared<Group>();
		const size_t task_count = (std::min)(workers_.size(), count);
		group->remaining.store(task_count, std::memory_order_relaxed);

		for (size_t task = 0; task < task_count; ++task)
		{
			enqueue([group, count, &function]()
			{
				try
				{
					for (;;)
					{
						const size_t index = group->next_index.fetch_add(1, std::memory_order_relaxed);
						if (index >= count)
							break;
						function(index);
					}
				}
				catch (...)
				{
					std::lock_guard<std::mutex> lock(group->mutex);
					if (!group->exception)
						group->exception = std::current_exception();
				}

				if (group->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
					group->completed.notify_one();
			});
		}

		{
			std::unique_lock<std::mutex> lock(group->mutex);
			group->completed.wait(lock, [&group]()
			{
				return group->remaining.load(std::memory_order_acquire) == 0;
			});
		}

		if (group->exception)
			std::rethrow_exception(group->exception);
	}

private:
	JobSystem()
	{
		const unsigned int hardware_threads = std::thread::hardware_concurrency();
		const size_t desired_workers = hardware_threads > 1 ? hardware_threads - 1 : 1;
		const size_t worker_total = (std::min)(desired_workers, size_t{ 8 });
		workers_.reserve(worker_total);
		for (size_t index = 0; index < worker_total; ++index)
			workers_.emplace_back([this]() { worker_loop(); });
	}

	~JobSystem()
	{
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			stopping_ = true;
		}
		queue_changed_.notify_all();
		for (std::thread& worker : workers_)
			if (worker.joinable()) worker.join();
	}

	void enqueue(std::function<void()> job)
	{
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			jobs_.push(std::move(job));
		}
		queue_changed_.notify_one();
	}

	void worker_loop()
	{
		for (;;)
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				queue_changed_.wait(lock, [this]() { return stopping_ || !jobs_.empty(); });
				if (stopping_ && jobs_.empty())
					return;
				job = std::move(jobs_.front());
				jobs_.pop();
			}
			job();
		}
	}

	std::vector<std::thread> workers_;
	std::queue<std::function<void()>> jobs_;
	std::mutex queue_mutex_;
	std::condition_variable queue_changed_;
	bool stopping_ = false;
};
