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

        // Persist collapse state
        static bool isLeftCollapsed = false;
        static bool isRightCollapsed = false;

        // Dynamic widths based on collapse state
        // If collapsed, reserve small width (e.g. 50px) for the vertical title bar or just the bar itself.
        // ImGui collapsed window is just the title bar height usually.
        // We will reserve 30px for collapsed width to indicate presence.
        float w1 = isLeftCollapsed ? 30.0f : w * 0.20f;
        float w3 = isRightCollapsed ? 30.0f : w * 0.30f;
        float w2 = w - w1 - w3; // Middle takes remaining space
        
        float h1 = h * 0.5f;

        // Flags: Remove NoCollapse to allow user interaction
        ImGuiWindowFlags panFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize; 
        // Note: NoCollapse removed.

        // -- Column 1: Source & Tokens --
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        if (!isLeftCollapsed) {
            ImGui::SetNextWindowSize(ImVec2(w1, h1), ImGuiCond_Always);
        }
        
        // We handle Source Code collapse as the master for "Left Panel"
        // Actually, we have 2 windows in Left Panel.
        // If "Source Code" is collapsed, we treat the whole left column as collapsed?
        // Or we let them collapse individually?
        // User asked for "Left panels".
        // Let's link them: if one collapses, we consider the column collapsed for layout purposes?
        // Better: Collapsing Source Code collapses the width. Tokens will follow.
        
        // Window 1: Source Code
        ImGui::Begin("Source Code##Editor", NULL, panFlags);
        isLeftCollapsed = ImGui::IsWindowCollapsed();
        
        // Draw Source Code Content
        if (!isLeftCollapsed) {
             ImGui::InputTextMultiline("##source", codeBuffer, IM_ARRAYSIZE(codeBuffer), ImVec2(-FLT_MIN, -30));
             if (ImGui::Button("Compile & Run", ImVec2(-FLT_MIN, 0))) {
                sourceCode = std::string(codeBuffer);
                if (!sourceCode.empty()) {
                    tokens = lexer.tokenize(sourceCode);
                    pda.loadInput(tokens);
                    parserStepIndex = 0;
                }
             }
        }
        ImGui::End();

        // Window 2: Tokens
        // If Left Column is collapsed, we might hide Tokens or keep it small?
        // If Source is collapsed, w1 is 30. Tokens window at size (30, h) is useless.
        // So we should force Tokens to match state.
        // We can't easily force collapse via API without SetNextWindowCollapsed.
        // But we can just hide it or render it small.
        // Let's SetNextWindowCollapsed based on isLeftCollapsed?
        ImGui::SetNextWindowCollapsed(isLeftCollapsed, ImGuiCond_Always);
        
        ImGui::SetNextWindowPos(ImVec2(x, y + h1), ImGuiCond_Always);
        if (!isLeftCollapsed) {
            ImGui::SetNextWindowSize(ImVec2(w1, h - h1), ImGuiCond_Always);
        }
        
        ImGui::Begin("Tokens##Table", NULL, panFlags);
        if (!isLeftCollapsed) {
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
        }
        ImGui::End();

        // -- Column 2: Automata Playground --
        ImGui::SetNextWindowPos(ImVec2(x + w1, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w2, h), ImGuiCond_Always);
        drawRegexPlayground();

        // -- Column 3: PDA --
        ImGui::SetNextWindowPos(ImVec2(x + w1 + w2, y), ImGuiCond_Always);
        if (!isRightCollapsed) {
            ImGui::SetNextWindowSize(ImVec2(w3, h), ImGuiCond_Always);
        }
        
        ImGui::Begin("Syntactic Analysis (PDA)##View", NULL, panFlags);
        isRightCollapsed = ImGui::IsWindowCollapsed();
        
        if (!isRightCollapsed) {
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
                      pda.inputTokens = tokens; 
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
                                  if (i == step.stackSnapshot.size() - 1) ImGui::TextColored(ImVec4(0,1,0,1), "[TOP] %s", val.c_str());
                                  else ImGui::Text("      %s", val.c_str());
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
        }
        ImGui::End();
    }

    void GuiManager::drawCodeEditor() {
         // Moved inline to renderUI to share collapse state
    }

    void GuiManager::drawTokenTable() {
         // Moved inline
    }
    
    void GuiManager::drawParserView() {
         // Moved inline
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
        
        // Init positions with BFS Layered Layout (Ranked)
        if (positions.empty() && !states.empty()) {
             // 1. Build Adjacency and ID Map
             std::map<int, std::vector<int>> adj;
             for(const auto& s : states) {
                 for(const auto& t : s.transitions) {
                     adj[s.id].push_back(t.targetStateId);
                 }
             }
             
             // 2. BFS for Depth
             std::map<int, int> depths;
             std::queue<int> q;
             std::set<int> visited;
             
             q.push(startId);
             visited.insert(startId);
             depths[startId] = 0;
             
             int maxDepth = 0;
             
             while(!q.empty()) {
                 int u = q.front(); q.pop();
                 maxDepth = std::max(maxDepth, depths[u]);
                 
                 if (adj.find(u) != adj.end()) {
                     for(int v : adj[u]) {
                         if (visited.find(v) == visited.end()) {
                             visited.insert(v);
                             depths[v] = depths[u] + 1;
                             q.push(v);
                         }
                     }
                 }
             }

             // Handle disconnected components (orphans) - assign them to depth 0 or maxDepth+1
             for(const auto& s : states) {
                 if (visited.find(s.id) == visited.end()) {
                     depths[s.id] = 0; 
                 }
             }
             
             // 3. Group by Depth
             std::map<int, std::vector<int>> levels;
             for(auto const& [id, d] : depths) {
                 levels[d].push_back(id);
             }
             
             // 4. Assign Positions
             // X spacing: 80 - 100
             // Y spacing: 60 - 80
             float xSpacing = 100.0f;
             float ySpacing = 80.0f;
             
             // Center mapping
             float totalWidth = (maxDepth + 1) * xSpacing;
             float startX = (canvas_sz.x - totalWidth) * 0.5f;
             if (startX < 50.0f) startX = 50.0f;
             
             for(auto const& [d, nodeIds] : levels) {
                 float x = startX + d * xSpacing;
                 float totalHeight = nodeIds.size() * ySpacing;
                 float startY = (canvas_sz.y - totalHeight) * 0.5f + canvas_p.y + 20.0f; // +20 margin
                 
                 for(int i=0; i<(int)nodeIds.size(); i++) {
                     int id = nodeIds[i];
                     positions[id] = ImVec2(canvas_p.x + x, startY + i * ySpacing);
                 }
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
             
             // Group transitions by target state using Set for unique labels
             std::map<int, std::set<std::string>> targetLabels;
             for (const auto& t : s.transitions) {
                 if (positions.find(t.targetStateId) == positions.end()) continue;
                 std::string label = (t.input == '\0') ? "eps" : std::string(1, t.input);
                 targetLabels[t.targetStateId].insert(label);
             }
             
             // Render aggregated links
             for (auto const& [tid, labels] : targetLabels) {
                 ImVec2 p2 = positions[tid];
                 
                 // Construct Label String
                 std::string labelStr = "";
                 for(const auto& l : labels) {
                     if (!labelStr.empty()) labelStr += ", ";
                     labelStr += l;
                 }
                 
                 // Self-loop
                 if (s.id == tid) {
                     float loopH = nodeRadius * 3.5f;
                     float loopW = nodeRadius * 2.5f;
                     
                     draw_list->AddBezierCubic(
                         ImVec2(p1.x - loopW * 0.5f, p1.y - nodeRadius),
                         ImVec2(p1.x - loopW, p1.y - loopH),
                         ImVec2(p1.x + loopW, p1.y - loopH),
                         ImVec2(p1.x + loopW * 0.5f, p1.y - nodeRadius),
                         IM_COL32(200,200,200,255), 2.0f
                     );
                     draw_list->AddText(ImVec2(p1.x - 10, p1.y - loopH - 12), IM_COL32(255,255,0,255), labelStr.c_str());
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
                 draw_list->AddText(mid, IM_COL32(255, 255, 0, 255), labelStr.c_str());
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
