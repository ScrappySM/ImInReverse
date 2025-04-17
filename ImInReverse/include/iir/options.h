#pragma once

#include "pch.h"

namespace IIR {
	class OptionsManager {
	public:
		static OptionsManager& GetInstance() {
			static OptionsManager instance;
			return instance;
		}

		ImVec4 nameColour = ImVec4(0.60f, 0.75f, 0.95f, 1.00f); // Pale/desaturated blue (class/type/variable names)
		ImVec4 offsetColour = ImVec4(0.90f, 0.30f, 0.30f, 1.00f); // Red (offsets)
		ImVec4 addressColour = ImVec4(0.45f, 0.85f, 0.45f, 1.00f); // Green (addresses)
		ImVec4 typeColour = ImVec4(0.30f, 0.60f, 0.90f, 1.00f); // Blue (types like int, float)
		ImVec4 numberColour = ImVec4(0.95f, 0.85f, 0.40f, 1.00f); // Yellow/golden (numeric values)
		ImVec4 textColour = ImGui::GetStyle().Colors[ImGuiCol_Text]; // Use ImGui's default text color

	private:
		OptionsManager() = default;
		~OptionsManager() = default;
		OptionsManager(const OptionsManager&) = delete;
		OptionsManager& operator=(const OptionsManager&) = delete;
	};
}
