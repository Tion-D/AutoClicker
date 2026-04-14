#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <functional>

namespace GameSenseUI {

    struct MenuTab {
        std::string name;
        std::string icon;
        std::function<void()> render;
        int id;
    };

    class GameSenseMenu {
    private:
        int current_tab = 0;
        std::vector<MenuTab> tabs;

    public:
        void AddTab(const std::string& name, const std::string& icon, std::function<void()> render_func) {
            tabs.push_back({name, icon, render_func, (int)tabs.size()});
        }

        void SetActiveTab(int tab_id) {
            if (tab_id >= 0 && tab_id < (int)tabs.size()) {
                current_tab = tab_id;
            }
        }

        int GetActiveTab() const { return current_tab; }

        void Render(const std::string& title = "GameSense Menu") {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
            ImGui::Begin("##MainWindow", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar);

            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 200);

            RenderSidebar();
            ImGui::NextColumn();

            RenderContent();

            ImGui::Columns(1);
            ImGui::End();
        }

    private:
        void RenderSidebar() {
            ImGui::BeginChild("##Sidebar", ImVec2(0, 0), true);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.00f));
            ImGui::TextUnformatted("MENU");
            ImGui::PopStyleColor();

            ImGui::Separator();
            ImGui::Spacing();

            for (size_t i = 0; i < tabs.size(); i++) {
                bool is_selected = (current_tab == (int)i);

                if (is_selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.56f, 1.00f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 0.00f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.00f));
                }

                if (ImGui::Button(tabs[i].name.c_str(), ImVec2(-1, 0))) {
                    current_tab = i;
                }

                ImGui::PopStyleColor(2);
                ImGui::Spacing();
            }

            ImGui::EndChild();
        }

        void RenderContent() {
            ImGui::BeginChild("##Content", ImVec2(0, 0), true);

            if (current_tab >= 0 && current_tab < (int)tabs.size()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.00f));
                ImGui::TextUnformatted(tabs[current_tab].name.c_str());
                ImGui::PopStyleColor();

                ImGui::Separator();
                ImGui::Spacing();

                if (tabs[current_tab].render) {
                    tabs[current_tab].render();
                }
            }

            ImGui::EndChild();
        }
    };

    inline void SetupGamesenseStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(4, 3);
        style.ItemSpacing = ImVec2(8, 4);
        style.ItemInnerSpacing = ImVec2(4, 4);
        style.IndentSpacing = 21.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.80f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.06f, 0.06f, 0.06f, 0.95f);
        colors[ImGuiCol_Border]                 = ImVec4(0.14f, 0.14f, 0.14f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.56f, 1.00f, 0.50f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.04f, 0.04f, 0.04f, 0.75f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.37f, 0.67f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.56f, 1.00f, 0.80f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.52f, 0.96f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.52f, 0.96f, 0.80f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.28f, 0.56f, 1.00f, 0.80f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.73f);
    }

    inline void GroupBoxBegin(const char* label, ImVec2 size = ImVec2(0, 0)) {
        ImGui::BeginChild(label, size, true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 1.00f, 1.00f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
    }

    inline void GroupBoxEnd() {
        ImGui::EndChild();
    }

}
