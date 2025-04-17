#include "pch.h"

#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

#include "widgets.h"

#include "iir/process.h"
#include "iir/structure.h"
#include "iir/options.h"

// Window filling entire screen, shouldn't ever go to top, etc
constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;

static const IIR::Field* g_selectedField = nullptr;

bool IsProbablyPointer(HANDLE process, uintptr_t value) {
	if (value < 0x10000 || value % sizeof(uintptr_t) != 0)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQueryEx(process, (LPCVOID)value, &mbi, sizeof(mbi)) == 0)
		return false;

	// Check if memory is committed and readable
	DWORD protect = mbi.Protect;
	bool isReadable =
		(protect & PAGE_READONLY) ||
		(protect & PAGE_READWRITE) ||
		(protect & PAGE_EXECUTE_READ) ||
		(protect & PAGE_EXECUTE_READWRITE);

	if (mbi.State != MEM_COMMIT || !isReadable)
		return false;

	return true;
}

void MenuBar(const Window& window, IIR::ProcessManager& pm) {
	bool openAbout = false;
	bool openProcPicker = false;
	bool openSettings = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Options")) {
				openSettings = true;
			}

			if (ImGui::MenuItem("Exit")) {
				PostQuitMessage(0);
			}

			ImGui::EndMenu();
		}

		const auto& selectedProcess = pm.GetSelectedProcess();
		if (ImGui::BeginMenu("Process")) {
			// Open process picker
			if (ImGui::MenuItem("Open Process")) {
				openProcPicker = true;
			}

			ImGui::BeginDisabled(!selectedProcess.has_value());
			if (ImGui::MenuItem("Close Process")) {
				pm.CloseProcess();
			}

			static bool processSuspended = pm.IsProcessSuspended();
			if (ImGui::MenuItem(processSuspended ? "Resume Process" : "Suspend Process")) {
				processSuspended ? pm.ResumeProcess() : pm.SuspendProcess();
				processSuspended = !processSuspended;
			}
			ImGui::EndDisabled();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Memory")) {
			ImGui::Text("TODO");

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("About")) {
				openAbout = true;
			}
			ImGui::EndMenu();
		}

		const auto fpsText = std::format("FPS: {:.2f}", ImGui::GetIO().Framerate);
		ImGui::SetCursorPosX(window.width - ImGui::CalcTextSize(fpsText.c_str()).x - 10.0f);
		ImGui::Text("%s", fpsText.c_str());

		ImGui::EndMainMenuBar();
	}

	static std::chrono::time_point timeSinceOpened = std::chrono::high_resolution_clock::now();
	if (openAbout) {
		ImGui::OpenPopup("About");
	}
	else if (openProcPicker) {
		ImGui::OpenPopup("Process Picker");
		timeSinceOpened = std::chrono::high_resolution_clock::now();
	} else if (openSettings) {
		ImGui::OpenPopup("Settings");
		timeSinceOpened = std::chrono::high_resolution_clock::now();
	}

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("About ImInReverse").x) / 2.0f);
		ImGui::TextUnderlined("ImInReverse");

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::Text("A simple reverse engineering tool used for exploring memory and debugging.");
		ImGui::Text("Written in C++ using ImGui and DirectX11.");
		ImGui::Text("Made with love by Ben McAvoy <3");
		ImGui::Spacing();
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 120) / 2.0f);
		if (ImGui::Button("OK", ImVec2(120, 32))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Process Picker", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImRect windowRect = ImGui::GetCurrentWindow()->Rect();

		static char search[128] = "";
		ImGui::InputText("Search", search, sizeof(search));
		ImGui::Separator();
		const auto& processes = pm.GetProcesses();
		for (const auto& process : processes) {
			ImGui::PushID(&process);
			if (process.name.find(search) != std::string::npos) {
				if (ImGui::Selectable(process.name.c_str(), false)) {
					pm.OpenProcess(process);
					SetWindowTextA(window.hWnd, std::format("ImInReverse - {} : ({})", process.name, process.pid).c_str());
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::PopID();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(windowRect.Min, windowRect.Max) && timeSinceOpened + std::chrono::milliseconds(500) < std::chrono::high_resolution_clock::now())) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImRect windowRect = ImGui::GetCurrentWindow()->Rect();
		auto& om = IIR::OptionsManager::GetInstance();

		ImGui::ColorEdit4("Address colour", &om.addressColour.x);
		ImGui::ColorEdit4("Name colour", &om.nameColour.x);
		ImGui::ColorEdit4("Number colour", &om.numberColour.x);
		ImGui::ColorEdit4("Offset colour", &om.offsetColour.x);
		ImGui::ColorEdit4("Type colour", &om.typeColour.x);

		if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(windowRect.Min, windowRect.Max) && timeSinceOpened + std::chrono::milliseconds(500) < std::chrono::high_resolution_clock::now())) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (!pm.GetSelectedProcess().has_value()) {
		SetWindowTextA(window.hWnd, "ImInReverse - No Process");
	}
}

void Ribbon(const Window& window, IIR::StructureManager& sm) {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

	// (0, height of menu bar)
	ImGui::SetCursorPos(ImVec2(0.0f, ImGui::GetFrameHeight()));
	ImGui::BeginChild("##Ribbon", ImVec2((float)window.width, 138), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	constexpr float width = 104.0f;
	ImGui::BeginButtonGroup("Add");
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add 8", width)) sm.AddBytes(8);
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add 16", width)) sm.AddBytes(16);
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add 32", width)) sm.AddBytes(32);
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add 64", width)) sm.AddBytes(64);
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add 128", width)) sm.AddBytes(128);
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add N", width)) spdlog::warn("Unimplemented: Add N");
	ImGui::EndButtonGroup();

	constexpr float width2 = 94.0f;
	ImGui::BeginButtonGroup("Selected");
    if (ImGui::GroupedButton(ICON_LC_SQUARE " Hex 64", width2)) {
        sm.JoinOrSplit(*g_selectedField, 8);
    }
    if (ImGui::GroupedButton(ICON_LC_ROWS_2 " Hex 32", width2)) {
        sm.JoinOrSplit(*g_selectedField, 4);
    }
    if (ImGui::GroupedButton(ICON_LC_ROWS_3 " Hex 16", width2)) {
        sm.JoinOrSplit(*g_selectedField, 2);
    }
    if (ImGui::GroupedButton(ICON_LC_ROWS_4 " Hex 8", width2)) {
        sm.JoinOrSplit(*g_selectedField, 1);
    }
	ImGui::EndButtonGroup();

	ImGui::BeginButtonGroup("Casting");
	if (ImGui::GroupedButton(ICON_LC_HASH " As i8", width2)) spdlog::info("Cast to int8_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As u8", width2)) spdlog::info("Cast to uint8_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As i16", width2)) spdlog::info("Cast to int16_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As u16", width2)) spdlog::info("Cast to uint16_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As i32", width2)) spdlog::info("Cast to int32_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As u32", width2)) spdlog::info("Cast to uint32_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As i64", width2)) spdlog::info("Cast to int64_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As u64", width2)) spdlog::info("Cast to uint64_t");
	if (ImGui::GroupedButton(ICON_LC_HASH " As f32", width2)) spdlog::info("Cast to float");
	if (ImGui::GroupedButton(ICON_LC_HASH " As f64", width2)) spdlog::info("Cast to double");
	if (ImGui::GroupedButton(ICON_LC_HASH " As bool", width2)) spdlog::info("Cast to bool");
	if (ImGui::GroupedButton(ICON_LC_HASH " As char*", width2)) spdlog::info("Cast to char");
	if (ImGui::GroupedButton(ICON_LC_HASH " As vec2", width2)) spdlog::info("Cast to vec2");
	if (ImGui::GroupedButton(ICON_LC_HASH " As vec3", width2)) spdlog::info("Cast to vec3");
	if (ImGui::GroupedButton(ICON_LC_HASH " As ptr", width2)) spdlog::info("Cast to pointer");
	ImGui::EndButtonGroup();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void MemoryPane(const Window& window, IIR::StructureManager& sm, IIR::OptionsManager& om, IIR::ProcessManager& pm) {
	static char buf[128] = "0";
	static char nameBuf[128] = "unnamed";

	ImGui::PushStyleColor(ImGuiCol_Text, om.offsetColour);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
	ImGui::Text(ICON_LC_SHAPES);
	ImGui::SameLine();
	if (ImGui::InlineEditText("memAddr", buf, sizeof(buf))) {
		if (strlen(buf) == 0) strcpy_s(buf, "0");

		for (size_t i = 0; i < strlen(buf); ++i) {
			buf[i] = std::toupper(static_cast<unsigned char>(buf[i]));
		}

		// Extremely basic parser for now, just try to convert
		try {
			if (buf[0] == '+') {
				size_t ptr = std::stoll(&buf[1], nullptr, 16);
				HMODULE hMods[1024]; DWORD cbNeeded;
				if (EnumProcessModulesEx(IIR::ProcessManager::GetInstance().GetHandle(), hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
					sm.SetBase(ptr + reinterpret_cast<uintptr_t>(hMods[0]));
				}
			}
			else {
				size_t ptr = std::stoll(buf, nullptr, 16);
				sm.SetBase(ptr);
			}
		}
		catch (std::exception e) {
			spdlog::error("{}", e.what());
		}
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip(std::format("The memory address of the structure. Current absolute value: 0x{:X}", sm.GetBase()).c_str());

	if (ImGui::BeginPopupContextWindow("Memory address")) {
		if (ImGui::Button("Copy absolute")) {
			ImGui::SetClipboardText(std::format("0x{:X}", sm.GetBase()).c_str());
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemTooltip("Copy the absolute value of the address. For example, if you typed `+123` it will give you the actual adress that is the base + 0x123");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::TextColored(om.typeColour, "Class");
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, om.nameColour);
	if (ImGui::InlineEditText("structName", nameBuf, sizeof(nameBuf))) {
		sm.SetName(nameBuf);
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::TextColored(om.numberColour, std::format("[{} {} 0x{:X}]", sm.GetSize(), ICON_LC_ARROW_LEFT_RIGHT, sm.GetSize()).c_str());

	ImGui::Indent();

	auto& fields = sm.GetFields();
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(fields.size()));
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
			auto& field = fields[i];
			auto data = sm.GetFieldData(field);

			if (field.fieldType == IIR::FieldType::unk) {
				ImGui::PushID(&field);

				// Check if this field is currently selected
				bool isSelected = (g_selectedField == &field);

				ImVec4 transparentHighlight = ImGui::GetStyleColorVec4(ImGuiCol_Header);
				transparentHighlight.w = 0.1f; // or lower for more transparency

				ImGui::PushStyleColor(ImGuiCol_Header, transparentHighlight);
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparentHighlight);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparentHighlight);

				if (ImGui::Selectable("##field_line", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
					g_selectedField = &field;
				}

				ImGui::PopStyleColor(3);

				// Highlight the row if selected
				if (isSelected) {
					ImVec2 min = ImGui::GetItemRectMin();
					ImVec2 max = ImGui::GetItemRectMax();
					ImU32 col = ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.1f);
					ImGui::GetWindowDrawList()->AddRectFilled(min, max, col);
				}

				ImGui::SameLine(0, 0);

				ImGui::BeginGroup();

				ImGui::TextColored(om.offsetColour, "%04X", (uint32_t)field.offset);
				ImGui::SameLine();
				ImGui::TextColored(om.addressColour, "%012llX", (uintptr_t)(sm.GetBase() + field.offset));
				ImGui::SameLine();

				// Text view (variable bytes as ASCII)
				{
					int asciiLen = std::min(field.size, 32); // safety
					char asciiBytes[33] = {};
					for (int j = 0; j < asciiLen; ++j) {
						unsigned char c = ((const unsigned char*)data)[j];
						asciiBytes[j] = (c >= 32 && c <= 126) ? c : '.';
					}
					asciiBytes[asciiLen] = '\0';
					ImGui::TextColored(om.typeColour, "%s", asciiBytes);
				}
				ImGui::SameLine();

				// Hex view (variable bytes)
				{
					int hexLen = std::min(field.size, 32); // safety
					std::string hexBytes;
					hexBytes.reserve(3 * hexLen + 1);
					for (int j = 0; j < hexLen; ++j) {
						char buf[4];
						snprintf(buf, sizeof(buf), "%02X ", ((const unsigned char*)data)[j]);
						hexBytes += buf;
					}
					ImGui::TextColored(om.textColour, "%s", hexBytes.c_str());
				}
				ImGui::SameLine();

				// Numeric view (show as int and hex)
				if (field.size == 8) {
					ImGui::TextColored(om.numberColour, "%lld (0x%016llX)", data->i64, data->u64);
				}
				else if (field.size == 4) {
					ImGui::TextColored(om.numberColour, "%d (0x%08X)", data->i32, data->u32);
				}
				else if (field.size == 2) {
					ImGui::TextColored(om.numberColour, "%d (0x%04X)", data->i16, data->u16);
				}
				else if (field.size == 1) {
					ImGui::TextColored(om.numberColour, "%d (0x%02X)", data->i8, data->u8);
				}
				else {
					ImGui::TextColored(om.numberColour, "(%d bytes)", field.size);
				}

				if (field.size == 8 && IsProbablyPointer(pm.GetHandle(), data->u64)) {
					ImGui::SameLine();
					ImGui::TextColored(om.offsetColour, "-> %llX", data->u64);
				}

				ImGui::EndGroup();

				ImGui::PopID();
			}
		}
	}
	clipper.End();

	ImGui::Unindent();
}

void Render(const Window& window) {
	auto& pm = IIR::ProcessManager::GetInstance();
	auto& sm = IIR::StructureManager::GetInstance();
	auto& om = IIR::OptionsManager::GetInstance();

	MenuBar(window, pm);

	static auto origin = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(origin);
	ImGui::SetNextWindowSize(ImVec2((float)window.width, (float)window.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("ImInReverse", nullptr, windowFlags | ImGuiWindowFlags_MenuBar);
	Ribbon(window, sm);
	MemoryPane(window, sm, om, pm);
	ImGui::End();
	ImGui::PopStyleVar();
}

int main(int argc, char* argv[]) {
	auto& pm = IIR::ProcessManager::GetInstance();
	auto& sm = IIR::StructureManager::GetInstance();
	pm.Init();
	sm.Init();

	auto window = WindowBuilder()
		.Name("ImInReverse", "ImInReverseClass")
		.Size(1200, 600)
		.VSync(true)
		.ImmersiveTitlebar()
		.Plugin<WindowBuilderImGui>()
		.OnRender(Render)
		.Build();

	window->Show();

	return 0;
}