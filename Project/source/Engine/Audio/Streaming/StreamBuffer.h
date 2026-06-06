#pragma once
#include <cstddef>
#include <atomic>

namespace Audio {

class StreamRingBuffer {
public:
	StreamRingBuffer(size_t capacityFrames, size_t channels);
	~StreamRingBuffer();

	size_t Write(const float* data, size_t frames); // decode thread
	size_t Read(float* out, size_t frames);         // audio thread
	size_t AvailableRead() const noexcept;
	size_t AvailableWrite() const noexcept;

	StreamRingBuffer(const StreamRingBuffer&) = delete;
	StreamRingBuffer& operator=(const StreamRingBuffer&) = delete;

private:
	size_t m_capacityFrames;
	size_t m_channels;
	float* m_buffer;
	std::atomic<size_t> m_writeIdx;
	std::atomic<size_t> m_readIdx;
};

} // namespace Audio
