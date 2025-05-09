#pragma once

#include "pch.h"

namespace ImGui {
    inline void TextUnderlined(const char* text) {
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

    inline void BeginButtonGroup(std::string_view label) {
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

    inline bool GroupedButton(std::string_view label, float width = 96.0f) {
        // Convert to std::string for map lookup
        std::string labelStr(label);

        // Add this button to current frame's group
        //buttons.push_back(labelStr);
        buttons.emplace_back(labelStr, width);
        buttonClicked.push_back(false); // Initialize as not clicked in this frame

        // Return whether this button was clicked in the previous frame
        return lastFrameButtonState.count(labelStr) > 0 && lastFrameButtonState[labelStr];
    }

    inline void EndButtonGroup() {
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

    inline void SelectableText(const char* id, const std::string& text) {
        SelectableText(id, text.c_str());
    }

    // Store ImGuiTreeNodeStackData for just submitted node.
    // Currently only supports 32 level deep and we are fine with (1 << Depth) overflowing into a zero, easy to increase.
    static void TreeNodeStoreStackData(ImGuiTreeNodeFlags flags) {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        g.TreeNodeStack.resize(g.TreeNodeStack.Size + 1);
        ImGuiTreeNodeStackData* tree_node_data = &g.TreeNodeStack.back();
        tree_node_data->ID = g.LastItemData.ID;
        tree_node_data->TreeFlags = flags;
        tree_node_data->ItemFlags = g.LastItemData.ItemFlags;
        tree_node_data->NavRect = g.LastItemData.NavRect;
        window->DC.TreeHasStackDataDepthMask |= (1 << window->DC.TreeDepth);
    }

    bool PointerTreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        //const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
        const bool display_frame = false;
        //const ImVec2 padding = (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));:
        const ImVec2 padding = { 1, 1 };

        if (!label_end)
            label_end = FindRenderedTextEnd(label);
        const ImVec2 label_size = CalcTextSize(label, label_end, false);

        const float text_offset_x = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);   // Collapsing arrow width + Spacing
        const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);            // Latch before ItemSize changes it
        const float text_width = g.FontSize + label_size.x + padding.x * 2;                         // Include collapsing arrow

        // We vertically grow up to current line height up the typical widget height.
        const float frame_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), label_size.y + padding.y * 2);
        const bool span_all_columns = (flags & ImGuiTreeNodeFlags_SpanAllColumns) != 0 && (g.CurrentTable != NULL);
        ImRect frame_bb;
        frame_bb.Min.x = span_all_columns ? window->ParentWorkRect.Min.x : (flags & ImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x : window->DC.CursorPos.x;
        frame_bb.Min.y = window->DC.CursorPos.y;
        frame_bb.Max.x = span_all_columns ? window->ParentWorkRect.Max.x : (flags & ImGuiTreeNodeFlags_SpanTextWidth) ? window->DC.CursorPos.x + text_width + padding.x : window->WorkRect.Max.x;
        frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
        if (display_frame)
        {
            const float outer_extend = IM_TRUNC(window->WindowPadding.x * 0.5f); // Framed header expand a little outside of current limits
            frame_bb.Min.x -= outer_extend;
            frame_bb.Max.x += outer_extend;
        }

        ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
        ItemSize(ImVec2(text_width, frame_height), padding.y);

        // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
        ImRect interact_bb = frame_bb;
        if ((flags & (ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanTextWidth | ImGuiTreeNodeFlags_SpanAllColumns)) == 0)
            interact_bb.Max.x = frame_bb.Min.x + text_width + (label_size.x > 0.0f ? style.ItemSpacing.x * 2.0f : 0.0f);

        // Compute open and multi-select states before ItemAdd() as it clear NextItem data.
        ImGuiID storage_id = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasStorageID) ? g.NextItemData.StorageId : id;
        bool is_open = TreeNodeUpdateNextOpen(storage_id, flags);

        bool is_visible = ItemAdd(interact_bb, id);
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
        g.LastItemData.DisplayRect = frame_bb;

        // If a NavLeft request is happening and ImGuiTreeNodeFlags_NavLeftJumpsBackHere enabled:
        // Store data for the current depth to allow returning to this node from any child item.
        // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode() and TreePop().
        // It will become tempting to enable ImGuiTreeNodeFlags_NavLeftJumpsBackHere by default or move it to ImGuiStyle.
        bool store_tree_node_stack_data = false;
        if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        {
            if ((flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) && is_open && !g.NavIdIsAlive)
                if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window && NavMoveRequestButNoResultYet())
                    store_tree_node_stack_data = true;
        }

        const bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
        if (!is_visible)
        {
            if (store_tree_node_stack_data && is_open)
                TreeNodeStoreStackData(flags); // Call before TreePushOverrideID()
            if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
                TreePushOverrideID(id);
            IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) | (is_open ? ImGuiItemStatusFlags_Opened : 0));
            return is_open;
        }

        if (span_all_columns)
        {
            TablePushBackgroundChannel();
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
            g.LastItemData.ClipRect = window->ClipRect;
        }

        ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
        if ((flags & ImGuiTreeNodeFlags_AllowOverlap) || (g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap))
            button_flags |= ImGuiButtonFlags_AllowOverlap;
        if (!is_leaf)
            button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

        // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
        // allow browsing a tree while preserving selection with code implementing multi-selection patterns.
        // When clicking on the rest of the tree node we always disallow keyboard modifiers.
        const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
        const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
        const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);

        const bool is_multi_select = (g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect) != 0;
        if (is_multi_select) // We absolutely need to distinguish open vs select so _OpenOnArrow comes by default
            flags |= (flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0 ? ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick : ImGuiTreeNodeFlags_OpenOnArrow;

        // Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
        // Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to requirements for multi-selection and drag and drop support.
        // - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
        // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
        // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
        // - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
        // - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and _OpenOnArrow=0)
        // It is rather standard that arrow click react on Down rather than Up.
        // We set ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be active on the initial MouseDown in order for drag and drop to work.
        if (is_mouse_x_over_arrow)
            button_flags |= ImGuiButtonFlags_PressedOnClick;
        else if (flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
            button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
        else
            button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

        bool selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;
        const bool was_selected = selected;

        // Multi-selection support (header)
        if (is_multi_select)
        {
            // Handle multi-select + alter button flags for it
            MultiSelectItemHeader(id, &selected, &button_flags);
            if (is_mouse_x_over_arrow)
                button_flags = (button_flags | ImGuiButtonFlags_PressedOnClick) & ~ImGuiButtonFlags_PressedOnClickRelease;
        }
        else
        {
            if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
                button_flags |= ImGuiButtonFlags_NoKeyModsAllowed;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
        bool toggled = false;
        if (!is_leaf)
        {
            if (pressed && g.DragDropHoldJustPressedId != id)
            {
                if ((flags & ImGuiTreeNodeFlags_OpenOnMask_) == 0 || (g.NavActivateId == id && !is_multi_select))
                    toggled = true; // Single click
                if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                    toggled |= is_mouse_x_over_arrow && !g.NavHighlightItemUnderNav; // Lightweight equivalent of IsMouseHoveringRect() since ButtonBehavior() already did the job
                if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseClickedCount[0] == 2)
                    toggled = true; // Double click
            }
            else if (pressed && g.DragDropHoldJustPressedId == id)
            {
                IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
                if (!is_open) // When using Drag and Drop "hold to open" we keep the node highlighted after opening, but never close it again.
                    toggled = true;
                else
                    pressed = false; // Cancel press so it doesn't trigger selection.
            }

            if (g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open)
            {
                toggled = true;
                NavClearPreferredPosForAxis(ImGuiAxis_X);
                NavMoveRequestCancel();
            }
            if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right && !is_open) // If there's something upcoming on the line we may want to give it the priority?
            {
                toggled = true;
                NavClearPreferredPosForAxis(ImGuiAxis_X);
                NavMoveRequestCancel();
            }

            if (toggled)
            {
                is_open = !is_open;
                window->DC.StateStorage->SetInt(storage_id, is_open);
                g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
            }
        }

        // Multi-selection support (footer)
        if (is_multi_select)
        {
            bool pressed_copy = pressed && !toggled;
            MultiSelectItemFooter(id, &selected, &pressed_copy);
            if (pressed)
                SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, interact_bb);
        }

        if (selected != was_selected)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

        // Render
        {
            const ImU32 text_col = GetColorU32(ImGuiCol_Text);
            ImGuiNavRenderCursorFlags nav_render_cursor_flags = ImGuiNavRenderCursorFlags_Compact;
            if (is_multi_select)
                nav_render_cursor_flags |= ImGuiNavRenderCursorFlags_AlwaysDraw; // Always show the nav rectangle
            if (display_frame)
            {
                // Framed type
                const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
                RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
                RenderNavCursor(frame_bb, id, nav_render_cursor_flags);
                if (flags & ImGuiTreeNodeFlags_Bullet)
                    RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f), text_col);
                else if (!is_leaf)
                    RenderArrow(window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col, is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up : ImGuiDir_Down) : ImGuiDir_Right, 1.0f);
                else // Leaf without bullet, left-adjusted text
                    text_pos.x -= text_offset_x - padding.x;
                if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
                    frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
                if (g.LogEnabled)
                    LogSetNextTextDecoration("###", "###");
            }
            else
            {
                // Unframed typed for tree nodes
                if (hovered || selected)
                {
                    const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
                    RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
                }
                RenderNavCursor(frame_bb, id, nav_render_cursor_flags);
                if (flags & ImGuiTreeNodeFlags_Bullet)
                    RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f), text_col);
                else if (!is_leaf)
                    RenderArrow(window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f), text_col, is_open ? ((flags & ImGuiTreeNodeFlags_UpsideDownArrow) ? ImGuiDir_Up : ImGuiDir_Down) : ImGuiDir_Right, 0.70f);
                if (g.LogEnabled)
                    LogSetNextTextDecoration(">", NULL);
            }

            if (span_all_columns)
                TablePopBackgroundChannel();

            // Label
            if (display_frame)
                RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
            else
                RenderText(text_pos, label, label_end, false);
        }

        if (store_tree_node_stack_data && is_open)
            TreeNodeStoreStackData(flags); // Call before TreePushOverrideID()
        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id); // Could use TreePush(label) but this avoid computing twice

        return is_open;
    }

    inline bool CollapsingPointer(const char* label, ImGuiTreeNodeFlags flags = 0) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiID id = window->GetID(label);
        return PointerTreeNodeBehavior(id, flags | ImGuiTreeNodeFlags_CollapsingHeader, label, NULL);
    }

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

    inline bool SelectableRow(const char* id, bool selected) {
        ImVec4 transparentHighlight = ImGui::GetStyleColorVec4(ImGuiCol_Header);
        transparentHighlight.w = 0.1f;

        ImGui::PushStyleColor(ImGuiCol_Header, transparentHighlight);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparentHighlight);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparentHighlight);

		ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(id, selected);

        ImGui::PopStyleColor(3);

        if (selected) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImU32 col = ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.1f);
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, col);
        }

        return clicked;
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
            toast.startTime = (float)ImGui::GetTime();
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
                float currentTime = (float)ImGui::GetTime();
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
