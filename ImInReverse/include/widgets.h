#pragma once

#include "pch.h"

namespace ImGui {
    void TextUnderlined(const char* text) {
        ImGui::TextUnformatted(text);
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        ImGui::GetForegroundDrawList()->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
    }

    // TODO: Optimise massively.
    static std::vector<std::string> buttonLabels;
    static std::string currentLabel;
    static std::vector<bool> buttonClicked; // Track which buttons are clicked
    static std::unordered_map<std::string, bool> lastFrameButtonState; // Map button labels to their clicked state

    void BeginButtonGroup(std::string_view label) {
        currentLabel = std::string(label);

        // Before clearing anything, update the map of last frame's button states
        for (size_t i = 0; i < buttonLabels.size(); i++) {
            if (i < buttonClicked.size()) {
                lastFrameButtonState[std::string(buttonLabels[i])] = buttonClicked[i];
            }
        }

        // Reset for this frame
        buttonLabels.clear();
        buttonClicked.clear();
    }

    bool GroupedButton(std::string_view label) {
        // Convert to std::string for map lookup
        std::string labelStr(label);

        // Add this button to current frame's group
        buttonLabels.push_back(labelStr);
        buttonClicked.push_back(false); // Initialize as not clicked in this frame

        // Return whether this button was clicked in the previous frame
        return lastFrameButtonState.count(labelStr) > 0 && lastFrameButtonState[labelStr];
    }

    void EndButtonGroup() {
        const float buttonWidth = 84.0f;
        const float buttonHeight = 24.0f;
        const float spacingY = 4.0f;
        const float spacingX = 8.0f;
        const float outerPadding = 5.0f;
        const int minButtonRows = 3; // Minimum number of button rows to display
        const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
        const float labelPaddingTop = 12.0f; // Padding above the label

        auto count = static_cast<int>(buttonLabels.size());
        int leftColCount = (count + 1) / 2; // Ceiling division for the left column
        int rightColCount = count - leftColCount;
        int actualMaxRows = std::max(leftColCount, rightColCount);

        // Use the larger of actual rows or minimum rows
        int displayRows = std::max(minButtonRows, actualMaxRows);

        // Calculate total area size
        float contentWidth = 2 * buttonWidth + spacingX;
        float buttonsHeight = displayRows * buttonHeight + (displayRows - 1) * spacingY;

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

        // Calculate fixed positions for columns
        float leftColX = startPos.x + outerPadding;
        float rightColX = leftColX + buttonWidth + spacingX;

        // Left column - using absolute positioning
        for (int i = 0; i < leftColCount; i++) {
            float buttonY = startPos.y + outerPadding + i * (buttonHeight + spacingY);
            ImGui::SetCursorScreenPos(ImVec2(leftColX, buttonY));

            ImGui::PushID(i);
            if (ImGui::Button(buttonLabels[i].c_str(), ImVec2(buttonWidth, buttonHeight))) {
                // Store the button click status
                buttonClicked[i] = true;
            }
            ImGui::PopID();
        }

        // Right column - using absolute positioning
        for (int i = 0; i < rightColCount; i++) {
            int index = i + leftColCount;
            float buttonY = startPos.y + outerPadding + i * (buttonHeight + spacingY);
            ImGui::SetCursorScreenPos(ImVec2(rightColX, buttonY));

            ImGui::PushID(index);
            if (ImGui::Button(buttonLabels[index].c_str(), ImVec2(buttonWidth, buttonHeight))) {
                // Store the button click status
                buttonClicked[index] = true;
            }
            ImGui::PopID();
        }

        // Add the label section below the buttons
        float labelY = startPos.y + outerPadding + buttonsHeight + labelPaddingTop;

        // Center the label across the entire content area (both columns)
        float labelWidth = ImGui::CalcTextSize(currentLabel.c_str()).x;
        float availableWidth = contentWidth; // Width of both button columns + spacing
        float centeredX = leftColX + (availableWidth - labelWidth) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(centeredX, labelY));
        ImGui::Text("%s", currentLabel.c_str());

        // Set cursor position after the entire group content
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + totalHeight));

        ImGui::EndGroup();

        // Position for next element
        ImGui::SameLine(0.0f, 12.0f);
    }
}