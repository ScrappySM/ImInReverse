#pragma once

#include "pch.h"

namespace ImGui {
    void TextUnderlined(const char* text) {
        ImGui::TextUnformatted(text);
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        ImGui::GetForegroundDrawList()->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
    }

    struct ButtonData {
        std::string label = "";
        float width = 96.0f;
    };

    // TODO: Optimise massively.
    static std::vector<ButtonData> buttons;
    static std::string currentLabel;
    static std::vector<bool> buttonClicked; // Track which buttons are clicked
    static std::unordered_map<std::string, bool> lastFrameButtonState; // Map button labels to their clicked state

    void BeginButtonGroup(std::string_view label) {
        currentLabel = std::string(label);

        // Before clearing anything, update the map of last frame's button states
        for (size_t i = 0; i < buttons.size(); i++) {
            if (i < buttonClicked.size()) {
                lastFrameButtonState[std::string(buttons[i].label)] = buttonClicked[i];
            }
        }

        // Reset for this frame
        buttons.clear();
        buttonClicked.clear();
    }

    bool GroupedButton(std::string_view label, float width = 96.0f) {
        // Convert to std::string for map lookup
        std::string labelStr(label);

        // Add this button to current frame's group
        //buttons.push_back(labelStr);
        buttons.emplace_back(labelStr, width);
        buttonClicked.push_back(false); // Initialize as not clicked in this frame

        // Return whether this button was clicked in the previous frame
        return lastFrameButtonState.count(labelStr) > 0 && lastFrameButtonState[labelStr];
    }

    void EndButtonGroup() {
        const float buttonHeight = 24.0f;
        const float spacingY = 4.0f;
        const float spacingX = 8.0f;
        const float outerPadding = 5.0f;
        const int minButtonRows = 3; // Minimum number of rows per column
        const int maxButtonRows = 3; // Maximum number of rows per column
        const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
        const float labelPaddingTop = 12.0f; // Padding above the label

        ImU32 wbg = ImGui::GetColorU32(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_Button, wbg);

        auto count = static_cast<int>(buttons.size());
        int numColumns = (count + maxButtonRows - 1) / maxButtonRows; // ceil(count / 3.0)
        int rowsPerColumn = std::min(std::max(minButtonRows, (count + numColumns - 1) / numColumns), maxButtonRows);

        // Compute max width for each column
        std::vector<float> colWidths(numColumns, 0.0f);
        for (int col = 0; col < numColumns; ++col) {
            for (int row = 0; row < maxButtonRows; ++row) {
                int idx = col * maxButtonRows + row;
                if (idx < count) {
                    colWidths[col] = std::max(colWidths[col], buttons[idx].width);
                }
            }
        }

        float contentWidth = 0.0f;
        for (int col = 0; col < numColumns; ++col) {
            contentWidth += colWidths[col];
            if (col != numColumns - 1)
                contentWidth += spacingX;
        }
        float buttonsHeight = rowsPerColumn * buttonHeight + (rowsPerColumn - 1) * spacingY;

        // Add space for the label area
        float totalContentHeight = buttonsHeight + labelPaddingTop + labelHeight;

        float totalWidth = contentWidth + 2 * outerPadding;
        float totalHeight = totalContentHeight + 2 * outerPadding;

        // Begin the main group
        ImGui::BeginGroup();

        // Get initial position for drawing
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw background and border
        drawList->AddRectFilled(
            startPos,
            ImVec2(startPos.x + totalWidth, startPos.y + totalHeight),
            ImGui::GetColorU32(ImGuiCol_WindowBg),
            4.0f
        );
        drawList->AddRect(
            startPos,
            ImVec2(startPos.x + totalWidth, startPos.y + totalHeight),
            ImGui::GetColorU32(ImGuiCol_Border),
            4.0f
        );

        // Add a rect at the bottom to highlight the label area along with a line to separate it
        drawList->AddRectFilled(
            ImVec2(startPos.x + 1, startPos.y + totalHeight - labelHeight - labelPaddingTop),
            ImVec2(startPos.x + totalWidth - 1, startPos.y + totalHeight - 1),
            ImGui::GetColorU32(ImGuiCol_ChildBg),
            4.0f
        );
        drawList->AddLine(
            ImVec2(startPos.x, startPos.y + totalHeight - labelHeight - labelPaddingTop),
            ImVec2(startPos.x + totalWidth, startPos.y + totalHeight - labelHeight - labelPaddingTop),
            ImGui::GetColorU32(ImGuiCol_Border),
            1.0f
        );

        // Draw columns and buttons
        float colX = startPos.x + outerPadding;
        for (int col = 0, btnIdx = 0; col < numColumns; ++col) {
            float colWidth = colWidths[col];
            for (int row = 0; row < rowsPerColumn && btnIdx < count; ++row, ++btnIdx) {
                float buttonY = startPos.y + outerPadding + row * (buttonHeight + spacingY);
                ImGui::SetCursorScreenPos(ImVec2(colX, buttonY));
                ImGui::PushID(btnIdx);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
                if (ImGui::Button(buttons[btnIdx].label.c_str(), ImVec2(buttons[btnIdx].width, buttonHeight))) {
                    buttonClicked[btnIdx] = true;
                }
                ImGui::PopStyleVar();
                ImGui::PopID();
            }
            colX += colWidth + spacingX;
        }

        // Add the label section below the buttons
        float labelY = startPos.y + outerPadding + buttonsHeight + labelPaddingTop;

        // Center the label across the entire content area (all columns)
        float labelWidth = ImGui::CalcTextSize(currentLabel.c_str()).x;
        float centeredX = startPos.x + outerPadding + (contentWidth - labelWidth) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(centeredX, labelY));
        ImGui::Text("%s", currentLabel.c_str());

        // Set cursor position after the entire group content
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + totalHeight));

        ImGui::PopStyleColor();
        ImGui::EndGroup();

        // Position for next element
        ImGui::SameLine(0.0f, 12.0f);
    }

    static int InlineEditTextCallback(ImGuiInputTextCallbackData* data) {
        bool* select_all = reinterpret_cast<bool*>(data->UserData);
        if (*select_all) {
            data->SelectionStart = 0;
            data->SelectionEnd = (int)std::strlen(data->Buf);
            *select_all = false;
        }
        return 0;
    }

    inline bool InlineEditText(const char* id, char* buf, size_t buf_size) {
        static bool edit_mode = false;
        static std::string active_id;
        static bool focus_next_frame = false;
        static bool select_all = false;
        bool value_changed = false;

        std::string widget_id = std::string("##inline_edit_") + id;

        if (!edit_mode || active_id != widget_id) {
            ImGui::Text("%s", buf);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                edit_mode = true;
                active_id = widget_id;
                focus_next_frame = true;
                select_all = true;
            }
        }
        else {
            // Calculate width to ensure the input doesn't get clipped (fixes "off by one" issue)
            float text_width = ImGui::CalcTextSize(buf).x;
            float min_width = ImGui::CalcTextSize("W").x * 4.0f; // min width for empty text
            float input_width = (text_width > min_width ? text_width : min_width)
                + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f; // add extra for cursor/padding

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::SetNextItemWidth(input_width);

            if (focus_next_frame) {
                ImGui::SetKeyboardFocusHere();
                focus_next_frame = false;
            }

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways;
            if (ImGui::InputText(widget_id.c_str(), buf, buf_size, flags, InlineEditTextCallback, &select_all)) {
                edit_mode = false;
                active_id.clear();
                value_changed = true;
            }

            // Exit edit mode if focus lost or Escape pressed
            if ((!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                edit_mode = false;
                active_id.clear();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }
        return value_changed;
    }
}
