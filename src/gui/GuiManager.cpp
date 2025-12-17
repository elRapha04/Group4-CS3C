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
        
        // Empty Init
        memset(codeBuffer, 0, sizeof(codeBuffer));
        memset(regexBuffer, 0, sizeof(regexBuffer));
        
        hasDebugData = false;
        
        // Pre-clear maps
        nfaPositions.clear();
        dfaPositions.clear();
        
        // Optimize Text Size
        ImGui::GetIO().FontGlobalScale = 1.6f;
        
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
            case Automata::TOKEN_LBRACE: return "L_BRACE";
            case Automata::TOKEN_RBRACE: return "R_BRACE";
            case Automata::TOKEN_UNKNOWN: return "UNKNOWN";
            case Automata::TOKEN_EOF: return "EOF";
            default: return "???";
        }
    }

    void GuiManager::renderUI() {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float w = viewport->Size.x;
        float h = viewport->Size.y;
        float x = viewport->Pos.x;
        float y = viewport->Pos.y;

        // 3-Column Layout:
        // Col 1 (20%): Source Code (Top 50%) + Tokens (Bottom 50%)
        // Col 2 (50%): Regex Playground (Automata)
        // Col 3 (30%): PDA
        
        float w1 = w * 0.20f;
        float w2 = w * 0.50f;
        float w3 = w * 0.30f;
        
        // Ensure total width <= w due to float rounding
        if (w1 + w2 + w3 > w) w3 = w - w1 - w2;

        float h1 = h * 0.5f;

        // Flags: Rigid layout to prevent overlap as requested.
        // User wants "fitting in screen" and "no overlap". 
        // We sacrifice custom adjustability for layout stability because manual window management is fragile.
        ImGuiWindowFlags panFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        // -- Column 1 --
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w1, h1), ImGuiCond_Always);
        drawCodeEditor(); 

        ImGui::SetNextWindowPos(ImVec2(x, y + h1), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w1, h - h1), ImGuiCond_Always);
        drawTokenTable();

        // -- Column 2 --
        ImGui::SetNextWindowPos(ImVec2(x + w1, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w2, h), ImGuiCond_Always);
        drawRegexPlayground();

        // -- Column 3 --
        ImGui::SetNextWindowPos(ImVec2(x + w1 + w2, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w3, h), ImGuiCond_Always);
        drawParserView();
    }

    void GuiManager::drawCodeEditor() {
        ImGui::Begin("Source Code##Editor", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::InputTextMultiline("##source", codeBuffer, IM_ARRAYSIZE(codeBuffer), ImVec2(-FLT_MIN, -30)); // Leave room for button
        
        if (ImGui::Button("Compile & Run", ImVec2(-FLT_MIN, 0))) {
            sourceCode = std::string(codeBuffer);
            if (!sourceCode.empty()) {
                tokens = lexer.tokenize(sourceCode);
                pda.loadInput(tokens);
                // Reset PDA on compile
                parserStepIndex = 0;
            }
        }
        ImGui::End();
    }

    void GuiManager::drawTokenTable() {
        ImGui::Begin("Tokens##Table", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTable("TokenTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for (const auto& t : tokens) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", getTokenName(t.type)); 
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", t.value.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    void GuiManager::drawParserView() {
        ImGui::Begin("Syntactic Analysis (PDA)##View", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        
        if (pda.inputTokens.empty()) {
             ImGui::TextWrapped("Compile code to load PDA.");
        } else {
             if (ImGui::Button("Step Forward")) {
                 pda.step();
                 parserStepIndex = pda.history.size() - 1;
             }
             ImGui::SameLine();
             if (ImGui::Button("Run All")) {
                 while(pda.step());
                 parserStepIndex = pda.history.size() - 1;
             }
             ImGui::SameLine();
             if (ImGui::Button("Reset")) {
                 pda.reset();
                 pda.inputTokens = tokens; // Reload tokens
                 parserStepIndex = 0;
             }
             
             ImGui::Separator();
             
             if (!pda.history.empty()) {
                 ImGui::SliderInt("History", &parserStepIndex, 0, (int)pda.history.size() - 1);
                 
                 if (parserStepIndex >= 0 && parserStepIndex < (int)pda.history.size()) {
                     const auto& step = pda.history[parserStepIndex];
                     
                     ImGui::TextColored(ImVec4(1, 1, 0, 1), "Action: %s", step.actionDesc.c_str());
                     ImGui::Text("Input: %s", step.currentInput.value.c_str());
                     
                     ImGui::Separator();
                     ImGui::Text("Stack (Top is Top):");
                     
                     if (ImGui::BeginChild("StackView", ImVec2(0, 0), true)) {
                         for (int i = (int)step.stackSnapshot.size() - 1; i >= 0; i--) {
                             std::string val = step.stackSnapshot[i].value;
                             if (i == step.stackSnapshot.size() - 1) {
                                 ImGui::TextColored(ImVec4(0,1,0,1), "[TOP] %s", val.c_str());
                             } else {
                                 ImGui::Text("      %s", val.c_str());
                             }
                         }
                         ImGui::EndChild();
                     }
                 }
             } else {
                 ImGui::Text("Ready.");
                 ImGui::Text("Next Input: %s", (pda.currentTokenIndex < pda.inputTokens.size()) ? pda.inputTokens[pda.currentTokenIndex].value.c_str() : "End");
             }
             
             if (pda.isSuccess) ImGui::TextColored(ImVec4(0,1,0,1), "RESULT: VALID");
             if (pda.isError) ImGui::TextColored(ImVec4(1,0,0,1), "RESULT: INVALID");
        }
        
        ImGui::End();
    }

    void GuiManager::drawAutomaton(const std::vector<Automata::State>& states, int startId, const char* label, std::map<int, ImVec2>& positions, bool isNFA) {
        ImGui::Text("%s (%d states)", label, (int)states.size());
        ImGui::TextDisabled("Drag nodes to rearrange.");
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_p = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        
        ImGui::InvisibleButton("canvas", canvas_sz);
        
        // Init positions
        if (positions.empty() && !states.empty()) {
            ImVec2 center = ImVec2(canvas_p.x + canvas_sz.x * 0.5f, canvas_p.y + canvas_sz.y * 0.5f);
            float radius = std::min(canvas_sz.x, canvas_sz.y) * 0.35f;
            int count = (int)states.size();
            for(int i=0; i<count; i++) {
                float angle = (float)i / (float)count * 6.28f;
                // Place Start node at Left (Pi)
                if (states[i].id == startId) angle = 3.14f;
                
                positions[states[i].id] = ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius);
            }
        }

        float nodeRadius = 20.0f;
        
        // Drag Logic
        if (ImGui::IsMouseDragging(0) && draggedNodeId != -1) {
             if (isDraggingNFA == isNFA) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                positions[draggedNodeId].x += delta.x;
                positions[draggedNodeId].y += delta.y;
             }
        }
        if (!ImGui::IsMouseDown(0)) draggedNodeId = -1;

        // Draw Links
        for (const auto& s : states) {
             if (positions.find(s.id) == positions.end()) continue;
             ImVec2 p1 = positions[s.id];
             
             for (const auto& t : s.transitions) {
                 if (positions.find(t.targetStateId) != positions.end()) {
                     ImVec2 p2 = positions[t.targetStateId];
                     
                     // Helper for self-loop
                     if (s.id == t.targetStateId) {
                         // Make loop larger and easier to see
                         float loopH = nodeRadius * 3.5f;
                         float loopW = nodeRadius * 2.0f;
                         
                         draw_list->AddBezierCubic(
                             ImVec2(p1.x - loopW * 0.5f, p1.y - nodeRadius),      // Start (Left-ish top)
                             ImVec2(p1.x - loopW, p1.y - loopH),                  // Control 1 (Far Left up)
                             ImVec2(p1.x + loopW, p1.y - loopH),                  // Control 2 (Far Right up)
                             ImVec2(p1.x + loopW * 0.5f, p1.y - nodeRadius),      // End (Right-ish top)
                             IM_COL32(200,200,200,255), 2.0f
                         );
                         char l[2] = { t.input == '\0' ? 'E' : t.input, '\0' };
                         draw_list->AddText(ImVec2(p1.x - 5, p1.y - loopH - 10), IM_COL32(255,255,0,255), l);
                         continue;
                     }

                     float angle = atan2(p2.y - p1.y, p2.x - p1.x);
                     ImVec2 start = ImVec2(p1.x + cos(angle)*nodeRadius, p1.y + sin(angle)*nodeRadius);
                     ImVec2 end = ImVec2(p2.x - cos(angle)*nodeRadius, p2.y - sin(angle)*nodeRadius);
                     
                     draw_list->AddLine(start, end, IM_COL32(200, 200, 200, 255), 2.0f);
                     
                     float arrowLen = 10.0f;
                     ImVec2 arrow1 = ImVec2(end.x - cos(angle + 0.5)*arrowLen, end.y - sin(angle + 0.5)*arrowLen);
                     ImVec2 arrow2 = ImVec2(end.x - cos(angle - 0.5)*arrowLen, end.y - sin(angle - 0.5)*arrowLen);
                     draw_list->AddTriangleFilled(end, arrow1, arrow2, IM_COL32(200, 200, 200, 255));

                     ImVec2 mid = ImVec2((start.x+end.x)*0.5f, (start.y+end.y)*0.5f);
                     mid.y -= 15.0f; 
                     char l[2] = { t.input == '\0' ? 'E' : t.input, '\0' };
                     draw_list->AddText(mid, IM_COL32(255, 255, 0, 255), l);
                 }
             }
        }

        // Draw Nodes
        for (const auto& s : states) {
            if (positions.find(s.id) == positions.end()) continue;
            ImVec2 center = positions[s.id];
            
            ImVec2 mouse = ImGui::GetMousePos();
            float dist = sqrt(pow(mouse.x - center.x, 2) + pow(mouse.y - center.y, 2));
            if (dist <= nodeRadius && ImGui::IsMouseClicked(0)) {
                draggedNodeId = s.id;
                isDraggingNFA = isNFA;
            }
            
            ImU32 col = (s.isFinal) ? IM_COL32(0, 180, 0, 255) : IM_COL32(100, 100, 200, 255);
            if (s.id == startId) col = IM_COL32(180, 180, 0, 255);
            
            draw_list->AddCircleFilled(center, nodeRadius, col);
            draw_list->AddCircle(center, nodeRadius, IM_COL32(255, 255, 255, 255), 0, 2.0f);
            
            char idBuf[16]; snprintf(idBuf, 16, "%d", s.id);
            ImVec2 txtSz = ImGui::CalcTextSize(idBuf);
            draw_list->AddText(ImVec2(center.x - txtSz.x*0.5f, center.y - txtSz.y*0.5f), IM_COL32(255,255,255,255), idBuf);
        }
    }

    void GuiManager::drawRegexPlayground() {
        ImGui::Begin("Automata Visualization##Playground", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        
        ImGui::InputText("Regex", regexBuffer, 256);
        ImGui::SameLine();
        if (ImGui::Button("Visualize")) {
            std::string r = regexBuffer;
            if (!r.empty()) {
                try {
                    debugNFA = Automata::RegexParser::toNFA(Automata::RegexParser::toPostfix(r));
                    debugNFA.optimize(); // Clean up!
                    
                    debugDFA = Automata::RegexParser::toDFA(debugNFA, Automata::TOKEN_UNKNOWN);
                    debugDFA.optimize(); // Clean up!
                    
                    hasDebugData = true;
                    nfaPositions.clear();
                    dfaPositions.clear();
                } catch(...) {}
            }
        }
        
        if (hasDebugData) {
            if (ImGui::BeginTabBar("Graphs")) {
                if (ImGui::BeginTabItem("NFA")) {
                    drawAutomaton(debugNFA.states, debugNFA.startStateId, "Thompson NFA (Optimized)", nfaPositions, true);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("DFA")) {
                    drawAutomaton(debugDFA.states, debugDFA.startStateId, "Deterministic FA (Optimized)", dfaPositions, false);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        } else {
            ImGui::Text("Enter a regex and click Visualize.");
        }
        
        ImGui::End();
    }

}
