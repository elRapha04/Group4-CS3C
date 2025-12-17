#include "GuiManager.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdio> // for snprintf

// Buffers
static char codeBuffer[1024 * 16] = "x = 10 + 20";
static char regexBuffer[256] = "(a|b)*c";

namespace GUI {

    GuiManager::GuiManager() : parserStepIndex(-1), hasDebugData(false), draggedNodeId(-1), isDraggingNFA(false) {}
    GuiManager::~GuiManager() {}

    bool GuiManager::init() {
        lexer.init();
        debugNFA = Automata::RegexParser::toNFA(Automata::RegexParser::toPostfix(regexBuffer));
        debugDFA = Automata::RegexParser::toDFA(debugNFA, Automata::TOKEN_UNKNOWN);
        hasDebugData = true;
        
        // Pre-clear maps so they init on first draw
        nfaPositions.clear();
        dfaPositions.clear();
        return true;
    }

    const char* GuiManager::getTokenName(Automata::TokenType t) {
        switch(t) {
            case Automata::TOKEN_INVALID: return "INVALID";
            case Automata::TOKEN_IDENTIFIER: return "IDENTIFIER";
            case Automata::TOKEN_NUMBER: return "NUMBER";
            case Automata::TOKEN_OPERATOR_PLUS: return "OP_PLUS";
            case Automata::TOKEN_OPERATOR_MINUS: return "OP_MINUS";
            case Automata::TOKEN_OPERATOR_MULT: return "OP_MULT";
            case Automata::TOKEN_OPERATOR_DIV: return "OP_DIV";
            case Automata::TOKEN_OPERATOR_EQ: return "OP_EQ";
            case Automata::TOKEN_LPAREN: return "L_PAREN";
            case Automata::TOKEN_RPAREN: return "R_PAREN";
            case Automata::TOKEN_UNKNOWN: return "UNKNOWN";
            case Automata::TOKEN_EOF: return "EOF";
            default: return "???";
        }
    }

    void GuiManager::renderUI() {
        // Manual Layout since Docking is disabled
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float w = viewport->Size.x;
        float h = viewport->Size.y;
        float x = viewport->Pos.x;
        float y = viewport->Pos.y;

        float leftWidth = w * 0.25f;
        float rightWidth = w * 0.25f;
        float midWidth = w - leftWidth - rightWidth;
        float topHeight = h * 0.6f;
        float bottomHeight = h - topHeight;

        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(leftWidth, topHeight));
        drawCodeEditor();

        ImGui::SetNextWindowPos(ImVec2(x, y + topHeight));
        ImGui::SetNextWindowSize(ImVec2(leftWidth, bottomHeight));
        drawTokenTable();

        ImGui::SetNextWindowPos(ImVec2(x + leftWidth, y));
        ImGui::SetNextWindowSize(ImVec2(midWidth, h));
        drawRegexPlayground();

        ImGui::SetNextWindowPos(ImVec2(x + leftWidth + midWidth, y));
        ImGui::SetNextWindowSize(ImVec2(rightWidth, h));
        drawParserView();
    }

    void GuiManager::drawCodeEditor() {
        ImGui::Begin("Source Code", NULL, ImGuiWindowFlags_None);
        ImGui::InputTextMultiline("##source", codeBuffer, IM_ARRAYSIZE(codeBuffer), ImVec2(-FLT_MIN, 200));
        
        if (ImGui::Button("Compile & Run")) {
            sourceCode = std::string(codeBuffer);
            tokens = lexer.tokenize(sourceCode);
            pda.loadInput(tokens);
            while(pda.step()); // Run to end
            parserStepIndex = 0;
        }
        ImGui::End();
    }

    void GuiManager::drawTokenTable() {
        ImGui::Begin("Tokens", NULL, ImGuiWindowFlags_None);
        if (ImGui::BeginTable("TokenTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for (const auto& t : tokens) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", getTokenName(t.type)); 
                ImGui::TableNextColumn();
                ImGui::Text("%s", t.value.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    void GuiManager::drawParserView() {
        ImGui::Begin("Syntactic Analysis (PDA)", NULL, ImGuiWindowFlags_None);
        if (pda.history.empty()) {
            ImGui::Text("No data."); 
            ImGui::End(); 
            return; 
        }
        
        ImGui::SliderInt("Step", &parserStepIndex, 0, (int)pda.history.size() - 1);
        
        if (parserStepIndex >= 0 && parserStepIndex < (int)pda.history.size()) {
            const auto& step = pda.history[parserStepIndex];
            ImGui::TextColored(ImVec4(1,1,0,1), "%s", step.actionDesc.c_str());
            
            ImGui::Separator();
            ImGui::Text("Stack (Top Down):");
            if (ImGui::BeginChild("StackView")) {
                for (int i = (int)step.stackSnapshot.size() - 1; i >= 0; i--) {
                    ImGui::Text(" [ %s ]", step.stackSnapshot[i].value.c_str());
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void GuiManager::drawAutomaton(const std::vector<Automata::State>& states, int startId, const char* label, std::map<int, ImVec2>& positions, bool isNFA) {
        ImGui::Text("%s (%d states) - Drag nodes to move", label, (int)states.size());
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_p = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        
        // Invisible button to capture scroll/drag on empty space if needed, 
        // but for nodes we handle them individually.
        ImGui::Dummy(canvas_sz);
        
        // Initialize positions if empty
        if (positions.empty() || positions.size() != states.size()) {
            if (positions.empty()) {
                positions.clear();
                ImVec2 center = ImVec2(canvas_p.x + canvas_sz.x * 0.5f, canvas_p.y + canvas_sz.y * 0.5f);
                float radius = std::min(canvas_sz.x, canvas_sz.y) * 0.35f;
                int count = (int)states.size();
                for(int i=0; i<count; i++) {
                    float angle = (float)i / (float)count * 6.28f;
                    positions[states[i].id] = ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius);
                }
            } else {
                 // Incremental update? just ignore for now or reset.
            }
        }

        float nodeRadius = 20.0f;
        
        // Update Dragging
        if (ImGui::IsMouseDragging(0) && draggedNodeId != -1) {
            if (isDraggingNFA == isNFA) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                positions[draggedNodeId].x += delta.x;
                positions[draggedNodeId].y += delta.y;
            }
        } else if (!ImGui::IsMouseDown(0)) {
            draggedNodeId = -1;
        }

        // Draw Edges
        for (const auto& s : states) {
             if (positions.find(s.id) == positions.end()) continue;
             ImVec2 p1 = positions[s.id];
             
             for (const auto& t : s.transitions) {
                 if (positions.find(t.targetStateId) != positions.end()) {
                     ImVec2 p2 = positions[t.targetStateId];
                     
                     // Shorten line to touch circle edge
                     float angle = atan2(p2.y - p1.y, p2.x - p1.x);
                     ImVec2 start = ImVec2(p1.x + cos(angle)*nodeRadius, p1.y + sin(angle)*nodeRadius);
                     ImVec2 end = ImVec2(p2.x - cos(angle)*nodeRadius, p2.y - sin(angle)*nodeRadius);
                     
                     draw_list->AddLine(start, end, IM_COL32(200, 200, 200, 255), 2.0f);
                     
                     // Arrowhead
                     float arrowLen = 10.0f;
                     ImVec2 arrow1 = ImVec2(end.x - cos(angle + 0.5)*arrowLen, end.y - sin(angle + 0.5)*arrowLen);
                     ImVec2 arrow2 = ImVec2(end.x - cos(angle - 0.5)*arrowLen, end.y - sin(angle - 0.5)*arrowLen);
                     draw_list->AddTriangleFilled(end, arrow1, arrow2, IM_COL32(200, 200, 200, 255));

                     // Text Label
                     ImVec2 mid = ImVec2((start.x+end.x)*0.5f, (start.y+end.y)*0.5f);
                     // Offset slightly so it doesn't overlap line
                     mid.y -= 10.0f;
                     
                     char tLabel[2] = { t.input == '\0' ? 'E' : t.input, '\0' };
                     draw_list->AddText(mid, IM_COL32(255, 255, 0, 255), tLabel);
                 }
             }
        }

        // Draw Nodes
        for (const auto& s : states) {
            if (positions.find(s.id) == positions.end()) continue;
            ImVec2 center = positions[s.id];
            
            // Interaction
            // Check if mouse is hovering this node
            ImVec2 mouse = ImGui::GetMousePos();
            float dist = sqrt(pow(mouse.x - center.x, 2) + pow(mouse.y - center.y, 2));
            
            bool isHovered = (dist <= nodeRadius);
            
            if (isHovered && ImGui::IsMouseClicked(0)) {
                draggedNodeId = s.id;
                isDraggingNFA = isNFA;
            }
            
            ImU32 col = (s.isFinal) ? IM_COL32(0, 180, 0, 255) : IM_COL32(100, 100, 200, 255);
            if (s.id == startId) col = IM_COL32(180, 180, 0, 255);
            if (draggedNodeId == s.id && isDraggingNFA == isNFA) col = IM_COL32(200, 100, 100, 255); // Highlight dragging

            draw_list->AddCircleFilled(center, nodeRadius, col);
            draw_list->AddCircle(center, nodeRadius, IM_COL32(255, 255, 255, 255), 0, 2.0f);
            
            char idBuf[16]; snprintf(idBuf, 16, "%d", s.id);
            // Center text
            ImVec2 textSize = ImGui::CalcTextSize(idBuf);
            ImVec2 textPos = ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
            draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), idBuf);
        }
    }

    void GuiManager::drawRegexPlayground() {
        ImGui::Begin("Regex Playground", NULL, ImGuiWindowFlags_None);
        
        ImGui::InputText("Regex", regexBuffer, 256);
        ImGui::SameLine();
        if (ImGui::Button("Visualize")) {
            std::string r = regexBuffer;
            try {
                // Regen
                debugNFA = Automata::RegexParser::toNFA(Automata::RegexParser::toPostfix(r));
                debugDFA = Automata::RegexParser::toDFA(debugNFA, Automata::TOKEN_UNKNOWN);
                hasDebugData = true;
                
                // Clear positions to trigger re-layout
                nfaPositions.clear();
                dfaPositions.clear();
                
            } catch(...) {}
        }
        
        if (hasDebugData) {
            if (ImGui::BeginTabBar("Graphs")) {
                if (ImGui::BeginTabItem("NFA")) {
                    drawAutomaton(debugNFA.states, debugNFA.startStateId, "Thompson NFA", nfaPositions, true);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("DFA")) {
                    drawAutomaton(debugDFA.states, debugDFA.startStateId, "Optimized DFA", dfaPositions, false);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

}
