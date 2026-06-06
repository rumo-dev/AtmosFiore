#include "AudioGraphEditor.h"
#include "../../Graphics/UI/Imgui/imgui.h"

namespace Audio {

	AudioGraphEditor::AudioGraphEditor() {}
	AudioGraphEditor::~AudioGraphEditor() {}

	void AudioGraphEditor::Render() {
		ImGui::Begin("Audio Graph Editor");
		ImGui::Text("Node editor placeholder");
		ImGui::End();
	}

} // namespace Audio
