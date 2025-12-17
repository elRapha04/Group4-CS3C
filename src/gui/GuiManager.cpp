#include "GuiManager.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Buffers
static char codeBuffer[1024 * 16] = "x = 10 + 20";
static char regexBuffer[256] = "(a|b)*c";

namespace GUI {

    GuiManager::GuiManager() : parserStepIndex(-1), hasDebugData(false) {}
    GuiManager::~GuiManager() {}

    bool GuiManager::init() {
        lexer.init();
        // Init default debug NFA
        debugNFA = Automata::RegexParser::toNFA(Automata::RegexParser::toPostfix(regexBuffer));
        debugDFA = Automata::RegexParser::toDFA(debugNFA, TOKEN_UNKNOWN);
        hasDebugData = true;
        return true;
    }

    void GuiManager::renderUI() {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        
        drawCodeEditor();
        drawTokenTable();
        drawParserView();
        drawRegexPlayground();
    }

    void GuiManager::drawCodeEditor() {
        ImGui::Begin("Source Code");
        ImGui::InputTextMultiline("##source", codeBuffer, IM_ARRAYSIZE(codeBuffer), ImVec2(-FLT_MIN, 200));
        
        if (ImGui::Button("Compile & Run")) {
            sourceCode = std::string(codeBuffer);
            tokens = lexer.tokenize(sourceCode);
            pda.loadInput(tokens);
            // Auto run to end for history
            while(pda.step());
            parserStepIndex = 0;
        }
        ImGui::End();
    }

    void GuiManager::drawTokenTable() {
        ImGui::Begin("Tokens");
        if (ImGui::BeginTable("TokenTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for (const auto& t : tokens) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // Simple mapping...
                ImGui::Text("%d", t.type); 
                ImGui::TableNextColumn();
                ImGui::Text("%s", t.value.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    void GuiManager::drawParserView() {
        ImGui::Begin("Syntactic Analysis (PDA)");
        if (pda.history.empty()) {
            ImGui::Text("No data."); 
            ImGui::End(); 
            return; 
        }
        
        // Scrubbing
        ImGui::SliderInt("Step", &parserStepIndex, 0, (int)pda.history.size() - 1);
        
        const auto& step = pda.history[parserStepIndex];
        ImGui::TextColored(ImVec4(1,1,0,1), "%s", step.actionDesc.c_str());
        
        ImGui::Separator();
        ImGui::Text("Stack (Top Down):");
        for (int i = step.stackSnapshot.size() - 1; i >= 0; i--) {
            ImGui::Text(" [ %s ]", step.stackSnapshot[i].value.c_str());
        }
        ImGui::End();
    }

    // Helper to draw states in a circle
    void drawAutomaton(const std::vector<Automata::State>& states, int startId, const char* label) {
        ImGui::Text("%s (%d states)", label, (int)states.size());
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        // Layout: Circle
        float radius = 150.0f;
        ImVec2 center = ImVec2(p.x + 200, p.y + 200);
        float nodeRadius = 15.0f;
        
        // Pre-calc positions
        std::vector<ImVec2> positions(states.size());
        int count = states.size();
        for (int i = 0; i < count; i++) {
            float angle = (float)i / (float)count * 6.28f;
            positions[i] = ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius);
        }
        
        // Draw Transitions
        for (int i = 0; i < count; i++) {
            const auto& s = states[i];
            for (const auto& t : s.transitions) {
                if (t.targetStateId < count) {
                     ImVec2 p1 = positions[i];
                     ImVec2 p2 = positions[t.targetStateId];
                     draw_list->AddLine(p1, p2, IM_COL32(200, 200, 200, 100), 2.0f);
                     
                     // Label
                     ImVec2 mid = ImVec2((p1.x+p2.x)*0.5f, (p1.y+p2.y)*0.5f);
                     char label[2] = { t.input == '\0' ? 'E' : t.input, '\0' };
                     draw_list->AddText(mid, IM_COL32(255, 255, 0, 255), label);
                }
            }
        }
        
        // Draw Nodes
        for (int i = 0; i < count; i++) {
             auto col = (states[i].isFinal) ? IM_COL32(0, 200, 0, 255) : IM_COL32(100, 100, 255, 255);
             if (states[i].id == startId) col = IM_COL32(200, 200, 0, 255); // Start = Yellow
             
             draw_list->AddCircleFilled(positions[i], nodeRadius, col);
             draw_list->AddCircle(positions[i], nodeRadius, IM_COL32(255,255,255,255));
             
             // ID
             char idBuf[16]; sprintf(idBuf, "%d", states[i].id);
             ImVec2 txtPos = ImVec2(positions[i].x - 5, positions[i].y - 5);
             draw_list->AddText(txtPos, IM_COL32(255, 255, 255, 255), idBuf);
        }
        
        // Advance cursor
        ImGui::Dummy(ImVec2(400, 400));
    }

    void GuiManager::drawRegexPlayground() {
        ImGui::Begin("Regex Playground");
        
        ImGui::InputText("Regex", regexBuffer, 256);
        ImGui::SameLine();
        if (ImGui::Button("Visualize")) {
            std::string r = regexBuffer;
            try {
                if(r.find("(") != std::string::npos && r.find(")") == std::string::npos) {
                    // Safe guard
                } else {
                    debugNFA = Automata::RegexParser::toNFA(Automata::RegexParser::toPostfix(r));
                    debugDFA = Automata::RegexParser::toDFA(debugNFA, TOKEN_UNKNOWN);
                    hasDebugData = true;
                }
            } catch(...) {
                // Ignore
            }
        }
        
        if (hasDebugData) {
            if (ImGui::BeginTabBar("Graphs")) {
                if (ImGui::BeginTabItem("NFA")) {
                    drawAutomaton(debugNFA.states, debugNFA.startStateId, "Thompson NFA");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("DFA")) {
                    drawAutomaton(debugDFA.states, debugDFA.startStateId, "Optimized DFA");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        
        ImGui::End();
    }

}
