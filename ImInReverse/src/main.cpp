#include "pch.h"

#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

#include "widgets.h"
#include "iir/process.h"

// Window filling entire screen, shouldn't ever go to top, etc
constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;

void Test() {
}

void MenuBar(const Window& window, IIR::ProcessManager& pm) {
	bool openAbout = false;
	bool openProcPicker = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
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
			ImGui::Text("get outtt!");

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

	ImGui::SetNextWindowPos(ImVec2((float)window.width / 2.0f, (float)window.height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("About ImInReverse").x) / 2.0f);
		ImGui::TextUnderlined("ImInReverse");

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::Text("A simple reverse engineering tool used for exploring memory and debugging.");
		ImGui::Text("Written in C++ using ImGui and DirectX11.");
		ImGui::Spacing();
		ImGui::Text("Made with love by Ben McAvoy <3");
		ImGui::Spacing();
		ImGui::Spacing();
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

	if (!pm.GetSelectedProcess().has_value()) {
		SetWindowTextA(window.hWnd, "ImInReverse - No Process");
	}
}

void Ribbon(const Window& window) {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

	// (0, height of menu bar)
	ImGui::SetCursorPos(ImVec2(0.0f, ImGui::GetFrameHeight()));
	ImGui::BeginChild("##Ribbon", ImVec2((float)window.width, 138), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::BeginButtonGroup("Add");
	if (ImGui::GroupedButton("Add 4")) spdlog::info("Add 4");
	if (ImGui::GroupedButton("Add 8")) spdlog::info("Add 8");
	if (ImGui::GroupedButton("Add 64")) spdlog::info("Add 64");
	if (ImGui::GroupedButton("Add 512")) spdlog::info("Add 512");
	if (ImGui::GroupedButton("Add 1024")) spdlog::info("Add 1024");
	if (ImGui::GroupedButton("Add N")) spdlog::info("Add N");
	ImGui::EndButtonGroup();

	ImGui::BeginButtonGroup("Selected");
	if (ImGui::GroupedButton("Hex 64")) spdlog::info("Hex 64");
	if (ImGui::GroupedButton("Hex 32")) spdlog::info("Hex 32");
	if (ImGui::GroupedButton("Hex 16")) spdlog::info("Hex 16");
	if (ImGui::GroupedButton("Hex 8")) spdlog::info("Hex 8");
	// No bits for now... painful...
	ImGui::EndButtonGroup();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void Render(const Window& window) {
	auto& pm = IIR::ProcessManager::GetInstance();
	MenuBar(window, pm);

	static auto origin = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(origin);
	ImGui::SetNextWindowSize(ImVec2((float)window.width, (float)window.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("ImInReverse", nullptr, windowFlags | ImGuiWindowFlags_MenuBar);
	Ribbon(window);
	ImGui::End();
	ImGui::PopStyleVar();
}

int main(int argc, char* argv[]) {
	auto& pm = IIR::ProcessManager::GetInstance();
	pm.Init();

	auto window = WindowBuilder()
		.Name("ImInReverse", "ImInReverseClass")
		.Size(800, 600)
		.VSync(true)
		.ImmersiveTitlebar()
		.Plugin<WindowBuilderImGui>()
		.OnRender(Render)
		.Build();

	window->Show();

	return 0;
}