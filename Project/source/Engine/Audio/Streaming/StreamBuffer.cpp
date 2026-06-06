#include "StreamBuffer.h"
#include <cstdlib>
#include <algorithm>

namespace Audio {

StreamRingBuffer::StreamRingBuffer(size_t capacityFrames, size_t channels)
	: m_capacityFrames(capacityFrames)
	, m_channels(channels)
	, m_buffer(nullptr)
	, m_writeIdx(0)
	, m_readIdx(0)
{
	m_buffer = static_cast<float*>(std::malloc(sizeof(float) * m_capacityFrames * m_channels));
}

StreamRingBuffer::~StreamRingBuffer() {
	if (m_buffer) std::free(m_buffer);
}

size_t StreamRingBuffer::AvailableRead() const noexcept {
	size_t w = m_writeIdx.load(std::memory_order_acquire);
	size_t r = m_readIdx.load(std::memory_order_acquire);
	if (w >= r) return w - r;
	return m_capacityFrames - (r - w);
}

size_t StreamRingBuffer::AvailableWrite() const noexcept {
	return m_capacityFrames - AvailableRead();
}

size_t StreamRingBuffer::Write(const float* data, size_t frames) {
	size_t canWrite = std::min(frames, AvailableWrite());
	size_t w = m_writeIdx.load(std::memory_order_relaxed);
	size_t first = std::min(canWrite, m_capacityFrames - w);
	size_t second = canWrite - first;
	float* dst = m_buffer + w * m_channels;
	std::copy(data, data + first * m_channels, dst);
	if (second > 0) {
		std::copy(data + first * m_channels, data + (first + second) * m_channels, m_buffer);
	}
	m_writeIdx.store((w + canWrite) % m_capacityFrames, std::memory_order_release);
	return canWrite;
}

size_t StreamRingBuffer::Read(float* out, size_t frames) {
	size_t canRead = std::min(frames, AvailableRead());
	size_t r = m_readIdx.load(std::memory_order_relaxed);
	size_t first = std::min(canRead, m_capacityFrames - r);
	size_t second = canRead - first;
	float* src = m_buffer + r * m_channels;
	std::copy(src, src + first * m_channels, out);
	if (second > 0) {
		std::copy(m_buffer, m_buffer + second * m_channels, out + first * m_channels);
	}
	m_readIdx.store((r + canRead) % m_capacityFrames, std::memory_order_release);
	return canRead;
}

} // namespace Audio
