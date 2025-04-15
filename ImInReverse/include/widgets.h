#pragma once

#include "pch.h"

namespace ImGui {
	void TextUnderlined(const char* text) {
		ImGui::TextUnformatted(text);
		auto min = ImGui::GetItemRectMin();
		auto max = ImGui::GetItemRectMax();
		ImGui::GetForegroundDrawList()->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
	}
}
