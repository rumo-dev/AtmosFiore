#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Audio {

using CommandFn = void(*)(void*);

struct AudioCommand {
	CommandFn fn;
	void* userData;
};

class AudioCommandQueue {
public:
	explicit AudioCommandQueue(size_t capacity = 1024);
	~AudioCommandQueue();

	bool Push(const AudioCommand& cmd); // game/gui thread
	bool Pop(AudioCommand& out);        // audio thread (non-blocking)

	AudioCommandQueue(const AudioCommandQueue&) = delete;
	AudioCommandQueue& operator=(const AudioCommandQueue&) = delete;

private:
	size_t m_capacity;
	AudioCommand* m_buffer;
	std::atomic<size_t> m_head;
	std::atomic<size_t> m_tail;
};

} // namespace Audio
