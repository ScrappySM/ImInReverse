#include "pch.h"

#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

#include "widgets.h"

#include "iir/process.h"
#include "iir/structure.h"
#include "iir/options.h"

#include "log.h"

// Window filling entire screen, shouldn't ever go to top, etc
constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;

//static IIR::Field* g_selectedField = nullptr;
static size_t fieldIndex = 0;

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
	bool openNewStructure = false;

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
			if (ImGui::Button("New structure")) openNewStructure = true;

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
	}
	else if (openSettings) {
		ImGui::OpenPopup("Settings");
		timeSinceOpened = std::chrono::high_resolution_clock::now();
	}
	else if (openNewStructure) {
		ImGui::OpenPopup("New Structure");
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
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		// Save item ID to detect focus later
		ImGui::PushID("SearchInput");
		bool searchActive = ImGui::InputText("Search", search, sizeof(search), ImGuiInputTextFlags_EnterReturnsTrue);
		bool isSearchFocused = ImGui::IsItemActive();
		ImGui::PopID();

		ImGui::Separator();
		const auto& processes = pm.GetProcesses();

		// If enter is pressed while the search box is focused, select the top match
		if (searchActive && isSearchFocused) {
			for (const auto& process : processes) {
				if (process.name.find(search) != std::string::npos) {
					pm.OpenProcess(process);
					SetWindowTextA(window.hWnd, std::format("ImInReverse - {} : ({})", process.name, process.pid).c_str());
					ImGui::CloseCurrentPopup();
					break;
				}
			}
		}

		for (const auto& process : processes) {
			if (process.name.find(search) != std::string::npos) {
				ImGui::PushID(&process);
				if (ImGui::Selectable(process.name.c_str(), false)) {
					pm.OpenProcess(process);
					SetWindowTextA(window.hWnd, std::format("ImInReverse - {} : ({})", process.name, process.pid).c_str());
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopID();
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
			(ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(windowRect.Min, windowRect.Max) &&
				timeSinceOpened + std::chrono::milliseconds(500) < std::chrono::high_resolution_clock::now())) {
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

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("New Structure", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImRect windowRect = ImGui::GetCurrentWindow()->Rect();
		auto& om = IIR::OptionsManager::GetInstance();

		static char sizeBuf[128] = "";
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::PushID("SearchInput");
		bool searchActive = ImGui::InputText("Search", sizeBuf, sizeof(sizeBuf), ImGuiInputTextFlags_EnterReturnsTrue);
		bool isSearchFocused = ImGui::IsItemActive();
		ImGui::PopID();

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
	// Keep ribbon directly under the menu bar
	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2((float)window.width, 138.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

	ImGui::BeginChild("##Ribbon", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	constexpr float buttonWidth = 104.0f;
	ImGui::BeginButtonGroup("Add");
	for (int size : { 8, 16, 32, 64, 128 }) {
		if (ImGui::GroupedButton(std::format(ICON_LC_PLUS " Add {}", size).c_str(), buttonWidth)) {
			sm.Lock();
			sm.structure.second.AddFields(size / 8);
			sm.Unlock();
		}
	}
	bool openAddN = false;
	if (ImGui::GroupedButton(ICON_LC_PLUS " Add N", buttonWidth)) {
		openAddN = true;
	}
	ImGui::EndButtonGroup();

	if (openAddN) {
		ImGui::OpenPopup("Add N");
	}

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Add N", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImRect windowRect = ImGui::GetCurrentWindow()->Rect();
		static char sizeBuf[64] = "0";
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::InputText("Size", sizeBuf, sizeof(sizeBuf));
		ImGui::Separator();
		if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
			try {
				int size = std::stoi(sizeBuf);
				//sm.AddBytes(size);
				sm.structure.second.AddFields(size);
			}
			catch (std::exception& e) {
				spdlog::error("{}", e.what());
			}
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(windowRect.Min, windowRect.Max))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	constexpr float castWidth = 94.0f;
	auto CastButton = [&](const char* label, int size, IIR::FieldType type) {
		if (ImGui::GroupedButton(label, castWidth)) {
			sm.structure.second.ResizeField(fieldIndex, size);
			sm.structure.second.fields[fieldIndex].fieldType = type;
		}
	};

	ImGui::BeginButtonGroup("Selected");
	CastButton(ICON_LC_SQUARE " Hex 64", 8, IIR::FieldType::unk);
	CastButton(ICON_LC_ROWS_2 " Hex 32", 4, IIR::FieldType::unk);
	CastButton(ICON_LC_ROWS_3 " Hex 16", 2, IIR::FieldType::unk);
	CastButton(ICON_LC_ROWS_4 " Hex 8", 1, IIR::FieldType::unk);
	ImGui::EndButtonGroup();

	ImGui::BeginButtonGroup("Casting");
	CastButton(ICON_LC_HASH " As i8", 1, IIR::FieldType::i8);
	CastButton(ICON_LC_HASH " As u8", 1, IIR::FieldType::u8);
	CastButton(ICON_LC_HASH " As i16", 2, IIR::FieldType::i16);
	CastButton(ICON_LC_HASH " As u16", 2, IIR::FieldType::u16);
	CastButton(ICON_LC_HASH " As i32", 4, IIR::FieldType::i32);
	CastButton(ICON_LC_HASH " As u32", 4, IIR::FieldType::u32);
	CastButton(ICON_LC_HASH " As i64", 8, IIR::FieldType::i64);
	CastButton(ICON_LC_HASH " As u64", 8, IIR::FieldType::u64);
	CastButton(ICON_LC_HASH " As f32", 4, IIR::FieldType::f32);
	CastButton(ICON_LC_HASH " As f64", 8, IIR::FieldType::f64);
	CastButton(ICON_LC_HASH " As bool", 1, IIR::FieldType::boolean);
	CastButton(ICON_LC_HASH " As char*", sizeof(char*), IIR::FieldType::str);
	//CastButton(ICON_LC_HASH " As vec2", sizeof(DirectX::XMFLOAT2), IIR::FieldType::vec2);
	//CastButton(ICON_LC_HASH " As vec3", sizeof(DirectX::XMFLOAT3), IIR::FieldType::vec3);
	CastButton(ICON_LC_HASH " As ptr", sizeof(void*), IIR::FieldType::ptr);
	ImGui::EndButtonGroup();

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}

auto GetFieldTypeString(IIR::FieldType fieldType) -> std::string_view {
   switch (fieldType) {
       case IIR::FieldType::i8: return "i8";
       case IIR::FieldType::u8: return "u8";
       case IIR::FieldType::i16: return "i16";
       case IIR::FieldType::u16: return "u16";
       case IIR::FieldType::i32: return "i32";
       case IIR::FieldType::u32: return "u32";
       case IIR::FieldType::i64: return "i64";
       case IIR::FieldType::u64: return "u64";
       case IIR::FieldType::f32: return "f32";
       case IIR::FieldType::f64: return "f64";
       case IIR::FieldType::str: return "char*";
       case IIR::FieldType::ptr: return "ptr";
       case IIR::FieldType::boolean: return "bool";
       case IIR::FieldType::vec2: return "vec2";
       case IIR::FieldType::vec3: return "vec3";
       default: return "unk";
   }
}

void RenderFields(const IIR::Structure& structure) {
	ImGui::Indent();
	auto& om = IIR::OptionsManager::GetInstance();

	/*ImGuiListClipper lc;
	lc.Begin((int)structure.fields.size());*/
	//while (lc.Step()) {
	{
		//for (auto& f : structure.fields) {
		for (int i = 0; i < structure.fields.size(); i++) {
			ImGui::PushID(i);
			ImGui::BeginGroup();

			ImGui::SetNextItemAllowOverlap();
			bool open = ImGui::TreeNodeEx("##Ptr");
			if (open) ImGui::TreePop();

			ImGui::SameLine();

			ImGui::SetNextItemAllowOverlap();
			if (ImGui::SelectableRow("##ControlLine", fieldIndex == i))
				fieldIndex = i;

			ImGui::SameLine();
			ImGui::Text("Hi");

			// TODO: Content

			ImGui::EndGroup();

			if (open) {
				ImGui::Indent();
				RenderFields(structure);
				ImGui::Unindent();
			}

			ImGui::PopID();
		}
	}
	ImGui::Unindent();
}

void MemoryPane(const Window& window, IIR::StructureManager& sm, IIR::OptionsManager& om, IIR::ProcessManager& pm) {
	ImGui::BeginChild("##MemoryPane");

	auto& pair = sm.structure;  // Direct reference to structure

	// === First Line: Controls ===
	if (ImGui::SelectableRow("##ControlLine", fieldIndex == -1))
		fieldIndex = -1;
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, om.offsetColour);
	ImGui::Text(ICON_LC_SHAPES);
	ImGui::SameLine();

	static char addressBuf[64] = "0";
	if (ImGui::InlineEditText("##AddressEditor", addressBuf, sizeof(addressBuf))) {
		for (size_t i = 0; i < strlen(addressBuf); ++i)
			addressBuf[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(addressBuf[i])));

		try {
			if (addressBuf[0] == '+') {
				size_t offset = std::stoull(&addressBuf[1], nullptr, 16);
				HMODULE hMods[1024]; DWORD cbNeeded;
				if (EnumProcessModulesEx(pm.GetHandle(), hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
					// Directly modifying the structure's baseAddr
					pair.second.baseAddr = reinterpret_cast<uintptr_t>(hMods[0]);
				} else {
					spdlog::error("Failed to enumerate process modules.");
				}
			} else {
				size_t ptr = std::stoull(addressBuf, nullptr, 16);
				// Directly modifying the structure's baseAddr
				pair.second.baseAddr = ptr;
			}
		} catch (const std::exception& e) {
			spdlog::error("Invalid address input: {}", e.what());
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::TextColored(om.typeColour, "Class");
	ImGui::SameLine();

	static char classNameBuf[64] = "Unnamed";
	ImGui::PushStyleColor(ImGuiCol_Text, om.nameColour);
	if (ImGui::InlineEditText("##ClassNameEditor", classNameBuf, sizeof(classNameBuf))) {
		// Find and rename class
		auto it = sm.structures.find(pair.first);
		if (it != sm.structures.end()) {
			// Insert new entry with the new class name
			sm.structures[classNameBuf] = std::move(it->second);
			// Erase the old class name
			sm.structures.erase(pair.first);
			// Update sm.structure reference
			sm.structure = *sm.structures.find(classNameBuf);
		}
	}
	ImGui::PopStyleColor();

	ImGui::SameLine();
	auto size = pair.second.size;
	ImGui::TextColored(om.numberColour, "[%zu %s 0x%zX]", size, ICON_LC_ARROW_LEFT_RIGHT, size);

	// === Fields ===
	RenderFields(pair.second);

	ImGui::EndChild();
}

void Render(const Window& window) {
	auto& pm = IIR::ProcessManager::GetInstance();
	auto& sm = IIR::StructureManager::GetInstance();
	auto& om = IIR::OptionsManager::GetInstance();

	static auto origin = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(origin);
	ImGui::SetNextWindowSize(ImVec2((float)window.width, (float)window.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("ImInReverse", nullptr, windowFlags/* | ImGuiWindowFlags_MenuBar*/);

	MenuBar(window, pm);
	Ribbon(window, sm);

	sm.Lock();
	MemoryPane(window, sm, om, pm);
	sm.Unlock();

	ImGui::End();
	ImGui::PopStyleVar(2);

#ifdef _DEBUG
	static bool showDemo = false;
	if (ImGui::IsKeyPressed(ImGuiKey_F1)) showDemo = !showDemo;
	if (showDemo) ImGui::ShowDemoWindow(&showDemo);

	if (showDemo) {
		ImGui::Begin("Debug stuff", &showDemo);

		ImGui::Text("Structures: %lld", sm.structures.size());
		for (auto& [k, v] : sm.structures) {
			ImGui::Text("K: %s | %lldk's", k.c_str(), v.fields.size());
		}

		ImGui::End();
	}
#endif

	ImGui::ToastSystem::RenderAll();
}

int main(int argc, char* argv[]) {
	SetupLogger();

	auto& pm = IIR::ProcessManager::GetInstance();
	auto& sm = IIR::StructureManager::GetInstance();
	sm.Init();
	pm.Init();

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