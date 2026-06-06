#pragma once

// ImGui header may not be available in this environment. Provide a minimal stub include if necessary.
#include "../Platform/AudioPlatformStubs.h"

namespace Audio {

class AudioGraphEditor {
public:
	AudioGraphEditor();
	~AudioGraphEditor();

	void Render();
};

} // namespace Audio
