#include "pch.h"

#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

#include "widgets.h"
#include "iir/process.h"

// Window filling entire screen, shouldn't ever go to top, etc
constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;

void MenuBar(const Window& window) {
	bool openAbout = false;
	bool openProcPicker = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("About")) {
				openAbout = true;
			}
			ImGui::EndMenu();
		}

		const auto& selectedProcess = IIR::ProcessManager::GetInstance().GetSelectedProcess();
		if (selectedProcess.has_value() && ImGui::BeginMenu("Process")) {
			if (ImGui::MenuItem("Close Process")) {
				IIR::ProcessManager::GetInstance().CloseProcess();
			}

			static bool processSuspended = IIR::ProcessManager::GetInstance().IsProcessSuspended();

			if (ImGui::MenuItem(processSuspended ? "Resume Process" : "Suspend Process")) {
				if (processSuspended) {
					IIR::ProcessManager::GetInstance().ResumeProcess();
				}
				else {
					IIR::ProcessManager::GetInstance().SuspendProcess();
				}
				processSuspended = !processSuspended;
			}

			ImGui::EndMenu();
		}

		const auto text = selectedProcess ? std::format("{} : ({})", selectedProcess->name, selectedProcess->pid) : std::string("No process selected");

		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(text.c_str()).x) / 2);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(text.c_str());

		ImGui::PopStyleColor();
		if (ImGui::IsItemClicked()) {
			openProcPicker = true;
		}

		const auto fpsText = std::format("FPS: {:.2f}", ImGui::GetIO().Framerate);
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fpsText.c_str()).x - 10);
		ImGui::BeginDisabled();
		ImGui::Text(fpsText.c_str());
		ImGui::EndDisabled();

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

	// Process picker with search bar
	if (ImGui::BeginPopupModal("Process Picker", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		ImRect windowRect = ImGui::GetCurrentWindow()->Rect();

		static char search[128] = "";
		ImGui::InputText("Search", search, sizeof(search));
		ImGui::Separator();
		const auto& processes = IIR::ProcessManager::GetInstance().GetProcesses();
		for (const auto& process : processes) {
			ImGui::PushID(&process);
			if (strstr(process.name.c_str(), search) != nullptr) {
				if (ImGui::Selectable(process.name.c_str(), false)) {
					IIR::ProcessManager::GetInstance().OpenProcess(process);
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::PopID();
		}

		// if they click outside the window or press escape, close it
		if (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(windowRect.Min, windowRect.Max) && timeSinceOpened + std::chrono::milliseconds(500) < std::chrono::high_resolution_clock::now()) {
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void Render(const Window& window) {
	MenuBar(window);

	static auto origin = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(origin);
	ImGui::SetNextWindowSize(ImVec2((float)window.width, (float)window.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("ImInReverse", nullptr, windowFlags | ImGuiWindowFlags_MenuBar);
	ImGui::End();
	ImGui::PopStyleVar();
}

int main(int argc, char* argv[]) {
	IIR::ProcessManager::GetInstance(); // Init

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