#pragma once

#include "pch.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WindowBuilderImGui : public WBPlugin {
public:
	void OnLoad(Window& window) override {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.IniFilename = nullptr;									// Disable .ini file
		ImGui::StyleColorsDark();

		float baseFontSize = 16.0f; // 13.0f is the size of the default font. Change to the font size you use.
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaMono.ttf", baseFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		//io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Segoeui.ttf", baseFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.FontDefault = io.Fonts->Fonts.back();

		float iconFontSize = baseFontSize * 2.5f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

		// Merge in icons from Lucide
		static const ImWchar icons_ranges[] = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };
		ImFontConfig icons_config; 
		icons_config.MergeMode = true; 
		icons_config.PixelSnapH = true; 
		icons_config.GlyphMinAdvanceX = iconFontSize;
		icons_config.FontDataOwnedByAtlas = false;
		icons_config.FontDataSize = sizeof(s_lucide_ttf);
		io.Fonts->AddFontFromMemoryTTF((void*)s_lucide_ttf, sizeof(s_lucide_ttf), iconFontSize, &icons_config, icons_ranges);
		io.Fonts->Build();

		// ImInReverse style from ImThemes
		ImGuiStyle& style = ImGui::GetStyle();

		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.6000000238418579f;
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.WindowRounding = 4.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(32.0f, 32.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ChildRounding = 4.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 4.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(10.0f, 5.0f);
		style.FrameRounding = 4.0f;
		style.FrameBorderSize = 0.0f;
		style.ItemSpacing = ImVec2(8.0f, 4.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.CellPadding = ImVec2(10.0f, 5.0f);
		style.IndentSpacing = 21.0f;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 14.0f;
		style.ScrollbarRounding = 9.0f;
		style.GrabMinSize = 10.0f;
		style.GrabRounding = 3.900000095367432f;
		style.TabRounding = 4.0f;
		style.TabBorderSize = 0.0f;
		style.TabMinWidthForCloseButton = 0.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 32.0f;

		style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07296139001846313f, 0.06857744604349136f, 0.06857744604349136f, 0.9399999976158142f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08154505491256714f, 0.07734530419111252f, 0.07734530419111252f, 0.9399999976158142f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.4862745106220245f, 0.2212072163820267f, 0.2212072163820267f, 0.5411764979362488f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.4000000059604645f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.6705882549285889f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.04721027612686157f, 0.04234741628170013f, 0.04234741628170013f, 1.0f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.47843137383461f, 0.1857439279556274f, 0.1857439279556274f, 1.0f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09442061185836792f, 0.08631584048271179f, 0.08631584048271179f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3098039329051971f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9803921580314636f, 0.3844674825668335f, 0.3844674825668335f, 1.0f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.8784313797950745f, 0.3238139152526855f, 0.3238139152526855f, 1.0f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.9803921580314636f, 0.3613994419574738f, 0.3613994419574738f, 1.0f);
		style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.3686274290084839f, 0.3686274290084839f, 0.4000000059604645f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.9764705896377563f, 0.3867589235305786f, 0.3867589235305786f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.9764705896377563f, 0.3408073484897614f, 0.3408073484897614f, 1.0f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.9764705896377563f, 0.3446366488933563f, 0.3446366488933563f, 0.3098039329051971f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.800000011920929f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 1.0f);
		style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.7490196228027344f, 0.09693194180727005f, 0.09693194180727005f, 0.7803921699523926f);
		style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.7490196228027344f, 0.2761091887950897f, 0.2761091887950897f, 1.0f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.2000000029802322f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.6705882549285889f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.9490196108818054f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.5764706134796143f, 0.2328488975763321f, 0.2328488975763321f, 0.8627451062202454f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.9764705896377563f, 0.3178315758705139f, 0.3178315758705139f, 0.800000011920929f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.6784313917160034f, 0.263390988111496f, 0.263390988111496f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06666667014360428f, 0.1019607856869698f, 0.1450980454683304f, 0.9724000096321106f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.4235294163227081f, 0.1328719705343246f, 0.1328719705343246f, 1.0f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.2359553873538971f, 0.2359553873538971f, 1.0f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.2980391979217529f, 0.2980391979217529f, 1.0f);
		style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
		style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
		style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
		style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 0.3490196168422699f);
		style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 0.0f, 0.0f, 0.9019607901573181f);
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.9764705896377563f, 0.2603921294212341f, 0.2603921294212341f, 1.0f);
		style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);

		ImGui_ImplWin32_Init(window.hWnd);
		ImGui_ImplDX11_Init(window.device, window.context);
	}

	void OnUnload(Window& window) override {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void PreRender(Window& window) override {
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void PostRender(Window& window) override {
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void HandleMessage(Window& window, UINT message, WPARAM wParam, LPARAM lParam) override {
		ImGui_ImplWin32_WndProcHandler(window.hWnd, message, wParam, lParam);
	}
};
