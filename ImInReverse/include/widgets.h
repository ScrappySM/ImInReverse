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
        bool* selectAll = reinterpret_cast<bool*>(data->UserData);
        if (*selectAll) {
            data->SelectionStart = 0;
            data->SelectionEnd = (int)std::strlen(data->Buf);
            *selectAll = false;
        }
        return 0;
    }

    inline bool InlineEditText(const char* id, char* buf, size_t buf_size) {
#ifdef _DEBUG
        assert(id != nullptr);
        assert(buf != nullptr);
        assert(buf_size > 0);
        assert(buf_size > strlen(buf));
        assert(strlen(buf) < buf_size);
#endif

        // Fine to have global state since realistically you can only have one being edited at a time
        // due to the click-off detection logic further down
        static bool selectNextFrame = false;

        ImGui::PushID(id);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        ImGuiID imgui_id = ImGui::GetID(id);
        bool isEditing = storage->GetBool(imgui_id, false);
        bool resBool = false;

        if (isEditing) {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::CalcTextSize(buf).x);

            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGui::InputText(id, buf, buf_size,
                ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll,
                InlineEditTextCallback, &selectNextFrame);

            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();

            // End editing if user clicks outside, presses Escape, or finishes editing
            if ((ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(min, max)) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                ImGui::IsItemDeactivatedAfterEdit())
            {
                isEditing = false;
                storage->SetBool(imgui_id, false);

                bool isBoxEmpty = (buf[0] == '\0');
                if (isBoxEmpty) {
                    strcpy_s(buf, buf_size, "None");
                    resBool = false;
                }
                else {
                    resBool = true;
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }
        else {
            ImGui::Text("%s", buf);
            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();

            if (ImGui::IsMouseHoveringRect(min, max) && ImGui::IsMouseDoubleClicked(0)) {
                isEditing = true;
                selectNextFrame = true;
                storage->SetBool(imgui_id, true);
            }
        }

        ImGui::PopID();
        return resBool;
    }

    inline void SelectableText(const char* id, const char* text) {
        // Fine to have global state since realistically you can only have one being selected at a time
        // due to the click-off detection logic
        static bool selectAllNextFrame = false;

        ImGui::PushID(id);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        ImGuiID imgui_id = ImGui::GetID(id);
        bool isSelected = storage->GetBool(imgui_id, false);

        // Use a static buffer for the InputText - need to ensure it's large enough
        const size_t bufferSize = 1024; // Adjust based on your expected maximum text length
        static char buffer[bufferSize];

        if (isSelected) {
            // First time selection - copy text to buffer
            if (selectAllNextFrame) {
                strncpy_s(buffer, bufferSize, text, _TRUNCATE);
            }

            // Configure the InputText to look like regular text but be selectable
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            ImGui::SetNextItemWidth(ImGui::CalcTextSize(text).x);

            // If it's the first frame of selection, set keyboard focus and select all
            if (selectAllNextFrame) {
                ImGui::SetKeyboardFocusHere();
                selectAllNextFrame = false;
            }

            // Show read-only InputText that allows selection and copying
            ImGui::InputText(id, buffer, bufferSize,
                ImGuiInputTextFlags_ReadOnly |
                ImGuiInputTextFlags_AutoSelectAll |
                ImGuiInputTextFlags_NoHorizontalScroll);

            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();

            // Exit selection mode if clicked outside
            if (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(min, max)) {
                isSelected = false;
                storage->SetBool(imgui_id, false);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        else {
            // Regular text display
            ImGui::Text("%s", text);
            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();

            // Enter selection mode on double click
            if (ImGui::IsMouseHoveringRect(min, max) && ImGui::IsMouseDoubleClicked(0)) {
                isSelected = true;
                selectAllNextFrame = true;
                storage->SetBool(imgui_id, true);
            }
        }

        ImGui::PopID();
    }

    // Overload for std::string
    inline void SelectableText(const char* id, const std::string& text) {
        SelectableText(id, text.c_str());
    }

    // Wrapper for SelectableText with color and printf-style formatting
    inline void ColoredSelectableText(ImVec4 color, const char* id, const char* fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::SelectableText(id, buffer);
        ImGui::PopStyleColor();
    }

    inline void ColoredSelectableText(ImU32 color, const char* id, const char* fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::SelectableText(id, buffer);
        ImGui::PopStyleColor();
    }

    bool BeginSelectableRow(const char* id, bool selected) {
        ImVec4 transparentHighlight = ImGui::GetStyleColorVec4(ImGuiCol_Header);
        transparentHighlight.w = 0.1f;

        ImGui::PushStyleColor(ImGuiCol_Header, transparentHighlight);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparentHighlight);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparentHighlight);

        bool clicked = ImGui::Selectable(id, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);

        ImGui::PopStyleColor(3);

        if (selected) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImU32 col = ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.1f);
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, col);
        }

        ImGui::SameLine(0, 0);
        ImGui::BeginGroup();

        return clicked;
    }

    void EndSelectableRow() {
        ImGui::EndGroup();
    }

    enum class ToastLevel {
        Debug,  // New debug level
        Info,
        Warning,
        Error
    };

    class ToastSystem {
    public:
        class Toast {
        public:
            Toast(std::string message, ToastLevel level) : message(message), level(level) {}
            std::string message;
            ToastLevel level;

            // Animation variables
            float alpha = 1.0f;
            float fadeSpeed = 0.75f; // Reduced fade speed for better visibility
            float displayTime = 3.0f; // Time to display the toast
            float startTime = 0.0f; // Time when the toast was created
            bool isFading = false; // Whether the toast is currently fading out
            bool isVisible = true; // Whether the toast is currently visible
        };

    private:
        static std::vector<Toast> toastQueue;
        static std::mutex toastMutex;  // Add mutex for thread safety

    public:
        // Create a new toast notification (now thread-safe)
        static void Show(const std::string& message, ToastLevel level) {
            std::lock_guard<std::mutex> Lock(toastMutex);  // Lock during queue modification

            toastQueue.emplace_back(message, level);
            auto& toast = toastQueue.back();
            toast.startTime = ImGui::GetTime();
            toast.isFading = false;
            toast.isVisible = true;
            toast.alpha = 1.0f; // Ensure full opacity to start
            toast.fadeSpeed = 0.75f;
            toast.displayTime = 3.0f;
        }

        // Helper to calculate wrapped text height
        static float CalculateTextHeight(const std::string& text, float wrapWidth) {
            ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
            return textSize.y;
        }

        // Render all active toasts (now thread-safe)
        static void RenderAll() {
            // Create a local copy of the toast queue to minimize lock time
            std::vector<Toast> localToastQueue;
            {
                std::lock_guard<std::mutex> Lock(toastMutex);
                localToastQueue = toastQueue;  // Make a copy to work with
            }

            const float screenWidth = ImGui::GetIO().DisplaySize.x;
            float yOffset = ImGui::GetIO().DisplaySize.y - 20.0f;  // Start from bottom
            const float padding = 10.0f;
            const float margin = 10.0f;
            const float toastWidth = 300.0f;  // Fixed width for toasts
            const float iconWidth = 20.0f;  // Space for icon

            // Save original ImGui state
            ImGuiStyle& style = ImGui::GetStyle();
            float originalAlpha = style.Alpha;

            // Process toasts in reverse order to display newest at the bottom
            std::vector<Toast> updatedToasts;
            updatedToasts.reserve(localToastQueue.size());

            for (auto it = localToastQueue.rbegin(); it != localToastQueue.rend(); ++it) {
                auto toast = *it;  // Work with a copy
                float currentTime = ImGui::GetTime();
                float elapsedTime = currentTime - toast.startTime;

                // Check if toast should start fading
                if (elapsedTime > toast.displayTime && !toast.isFading) {
                    toast.isFading = true;
                }

                // Handle fading
                if (toast.isFading) {
                    toast.alpha -= toast.fadeSpeed * ImGui::GetIO().DeltaTime;
                    if (toast.alpha <= 0.0f) {
                        toast.isVisible = false;
                        continue;
                    }
                }

                // The toast is still visible, so add it to our updated list
                updatedToasts.push_back(toast);

                // Calculate available width for text (accounting for padding and icon)
                float textAreaWidth = toastWidth - (padding * 2.0f) - iconWidth;

                // Calculate wrapped text height
                float textHeight = CalculateTextHeight(toast.message, textAreaWidth);
                float toastHeight = std::max(textHeight, 24.0f) + (padding * 2.0f); // Ensure minimum height

                // Calculate position (right-aligned)
                ImVec2 position(screenWidth - toastWidth - margin, yOffset - toastHeight);

                // Set colors based on toast level - use ImGui's theme colors as a base
                ImVec4 bgColor;
                ImVec4 borderColor;
                ImVec4 textColor = style.Colors[ImGuiCol_Text];

                switch (toast.level) {
                case ToastLevel::Debug:
                    bgColor = ImVec4(0.1f, 0.1f, 0.3f, 1.0f);  // Dark blue
                    borderColor = ImVec4(0.2f, 0.2f, 0.5f, 1.0f);
                    break;
                case ToastLevel::Info:
                    bgColor = ImVec4(0.0f, 0.6f, 0.0f, 1.0f);  // Green background
                    borderColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);  // Lighter green border
                    textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White text for contrast
                    break;
                case ToastLevel::Warning:
                    bgColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
                    borderColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                    textColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                    break;
                case ToastLevel::Error:
                    bgColor = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);  // More muted, darker red
                    borderColor = ImVec4(0.7f, 0.2f, 0.2f, 1.0f);  // Softer red border
                    textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    break;
                }

                // Draw standalone toast using ImDrawList directly
                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                // Calculate the area for the toast
                ImVec2 rectMin = position;
                ImVec2 rectMax(position.x + toastWidth, position.y + toastHeight);

                // We will directly manipulate colors and apply alpha ourselves
                ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(bgColor.x, bgColor.y, bgColor.z, bgColor.w * toast.alpha));
                ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(borderColor.x, borderColor.y, borderColor.z, borderColor.w * toast.alpha));
                ImU32 textColorU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(textColor.x, textColor.y, textColor.z, textColor.w * toast.alpha));

                // Draw background and border
                drawList->AddRectFilled(rectMin, rectMax, bgColorU32, 6.0f);
                drawList->AddRect(rectMin, rectMax, borderColorU32, 6.0f, 0, 2.0f);

                // Add icon based on toast level
                const char* icon = "";
                switch (toast.level) {
                case ToastLevel::Debug:   icon = ICON_LC_CODE; break;       // Code icon for Debug
                case ToastLevel::Info:    icon = ICON_LC_INFO; break;
                case ToastLevel::Warning: icon = ICON_LC_OCTAGON_ALERT; break;
                case ToastLevel::Error:   icon = ICON_LC_CIRCLE_X; break;
                }

                // Draw icon
                drawList->AddText(
                    ImVec2(rectMin.x + padding, rectMin.y + padding),
                    textColorU32,
                    icon
                );

                // Draw message text with proper wrapping
                float textX = rectMin.x + padding + iconWidth;
                float textY = rectMin.y + padding;

                // Store current clip rect
                ImVec4 clipRect = ImVec4(textX, textY, rectMax.x - padding, rectMax.y - padding);
                drawList->PushClipRect(
                    ImVec2(clipRect.x, clipRect.y),
                    ImVec2(clipRect.z, clipRect.w),
                    true
                );

                // Draw wrapped text
                ImGui::PushFont(ImGui::GetFont());
                drawList->AddText(
                    ImGui::GetFont(),
                    ImGui::GetFontSize(),
                    ImVec2(textX, textY),
                    textColorU32,
                    toast.message.c_str(),
                    NULL,
                    textAreaWidth
                );
                ImGui::PopFont();

                // Pop the clip rect
                drawList->PopClipRect();

                // Update y-offset for next toast
                yOffset -= (toastHeight + margin);
            }

            // Restore the original ImGui alpha
            style.Alpha = originalAlpha;

            // Update the toast queue with our processed toasts
            {
                std::lock_guard<std::mutex> Lock(toastMutex);
                // Reverse the order since we processed them backwards
                toastQueue.clear();
                for (auto it = updatedToasts.rbegin(); it != updatedToasts.rend(); ++it) {
                    toastQueue.push_back(*it);
                }
            }
        }
    };

    // Define static members
    std::vector<ToastSystem::Toast> ToastSystem::toastQueue;
    std::mutex ToastSystem::toastMutex;
}
