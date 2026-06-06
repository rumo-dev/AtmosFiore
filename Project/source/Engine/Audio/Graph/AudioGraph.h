#pragma once
#include "../Core/IAudioDSPNode.h"
#include <vector>
#include <memory>
#include <cstddef>

namespace Audio {

class AudioGraph {
public:
	AudioGraph();
	~AudioGraph();

	size_t AddNode(std::shared_ptr<IAudioDSPNode> node);
	void Connect(size_t outNodeIdx, size_t inNodeIdx);
	bool ValidateAndPrepare(); // topological sort, cycle detection
	void Process(float** outputs, size_t frames, size_t channels);

private:
	std::vector<std::shared_ptr<IAudioDSPNode>> m_nodes;
	std::vector<std::vector<size_t>> m_adjacency; // from node -> list of targets
	std::vector<size_t> m_executionOrder;

	bool TopologicalSort();
	bool DetectCycle();
};

} // namespace Audio
