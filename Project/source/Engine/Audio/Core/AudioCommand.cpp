#include "AudioCommand.h"
#include <cstdlib>
#include <new>

namespace Audio {

AudioCommandQueue::AudioCommandQueue(size_t cap)
	: m_capacity(cap)
	, m_buffer(nullptr)
	, m_head(0)
	, m_tail(0)
{
	// allocate raw buffer with malloc to avoid exceptions/new in audio path
	m_buffer = static_cast<AudioCommand*>(std::malloc(sizeof(AudioCommand) * m_capacity));
}

AudioCommandQueue::~AudioCommandQueue() {
	if (m_buffer) std::free(m_buffer);
}

bool AudioCommandQueue::Push(const AudioCommand& cmd) {
	size_t head = m_head.load(std::memory_order_relaxed);
	size_t next = (head + 1) % m_capacity;
	if (next == m_tail.load(std::memory_order_acquire)) return false; // full
	m_buffer[head] = cmd;
	m_head.store(next, std::memory_order_release);
	return true;
}

bool AudioCommandQueue::Pop(AudioCommand& out) {
	size_t tail = m_tail.load(std::memory_order_relaxed);
	if (tail == m_head.load(std::memory_order_acquire)) return false; // empty
	out = m_buffer[tail];
	m_tail.store((tail + 1) % m_capacity, std::memory_order_release);
	return true;
}

} // namespace Audio
