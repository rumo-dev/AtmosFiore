#include "AudioGraph.h"
#include <stack>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <functional>
#include <cstdint>

namespace Audio {

AudioGraph::AudioGraph() {}
AudioGraph::~AudioGraph() {}

size_t AudioGraph::AddNode(std::shared_ptr<IAudioDSPNode> node) {
	m_nodes.push_back(node);
	m_adjacency.emplace_back();
	return m_nodes.size() - 1;
}

void AudioGraph::Connect(size_t outNodeIdx, size_t inNodeIdx) {
	if (outNodeIdx >= m_nodes.size() || inNodeIdx >= m_nodes.size()) return;
	m_adjacency[outNodeIdx].push_back(inNodeIdx);
}

bool AudioGraph::DetectCycle() {
	size_t n = m_nodes.size();
	enum State { UNVISITED = 0, VISITING = 1, VISITED = 2 };
	std::vector<uint8_t> state(n, UNVISITED);
	std::function<bool(size_t)> dfs = [&](size_t u) -> bool {
		state[u] = VISITING;
		for (size_t v : m_adjacency[u]) {
			if (state[v] == VISITING) return true;
			if (state[v] == UNVISITED && dfs(v)) return true;
		}
		state[u] = VISITED;
		return false;
	};
	for (size_t i = 0; i < n; ++i) {
		if (state[i] == UNVISITED) {
			if (dfs(i)) return true;
		}
	}
	return false;
}

bool AudioGraph::TopologicalSort() {
	size_t n = m_nodes.size();
	std::vector<int> indeg(n, 0);
	for (size_t u = 0; u < n; ++u) {
		for (size_t v : m_adjacency[u]) indeg[v]++;
	}
	std::vector<size_t> order;
	order.reserve(n);
	std::vector<size_t> stack;
	for (size_t i = 0; i < n; ++i) if (indeg[i] == 0) stack.push_back(i);
	while (!stack.empty()) {
		size_t u = stack.back(); stack.pop_back();
		order.push_back(u);
		for (size_t v : m_adjacency[u]) {
			indeg[v]--;
			if (indeg[v] == 0) stack.push_back(v);
		}
	}
	if (order.size() != n) return false;
	m_executionOrder = std::move(order);
	return true;
}

bool AudioGraph::ValidateAndPrepare() {
	if (DetectCycle()) return false;
	return TopologicalSort();
}

void AudioGraph::Process(float** outputs, size_t frames, size_t channels) {
	// Very simple: call nodes in execution order. In a real engine we'd manage buffers, mixing, etc.
	if (m_executionOrder.empty()) {
		if (!ValidateAndPrepare()) return;
	}
	// temporary buffers per node would be needed; here we assume nodes write directly to outputs for simplicity
	AudioBufferView inView{ nullptr, frames, channels };
	AudioBufferView outView{ outputs, frames, channels };
	for (size_t idx : m_executionOrder) {
		auto& node = m_nodes[idx];
		if (node) node->Process(inView, outView);
	}
}

} // namespace Audio
