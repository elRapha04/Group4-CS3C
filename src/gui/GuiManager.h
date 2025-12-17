#pragma once
#include <string>
#include <vector>
#include <map>
#include "imgui.h"
#include "../lexer/Lexer.h"
#include "../parser/PDA.h"

namespace GUI {
    
    class GuiManager {
    public:
        GuiManager();
        ~GuiManager();
        
        bool init();
        void run(); 
        void shutdown();
        void renderUI();

    private:
        // Core Logic Instances
        Automata::Lexer lexer;
        Automata::PDA pda;

        // Code Editor State
        std::string sourceCode;
        std::vector<Automata::Token> tokens;
        
        // Regex Playground State
        Automata::NFA debugNFA;
        Automata::DFA debugDFA;
        bool hasDebugData;
        
        // Visual State
        std::map<int, ImVec2> nfaPositions;
        std::map<int, ImVec2> dfaPositions;
        int draggedNodeId; // ID of node currently being dragged
        bool isDraggingNFA; // True if NFA, False if DFA
        
        // Parser Visualization State
        int parserStepIndex; 

        // Internal Helpers
        void drawCodeEditor();
        void drawTokenTable();
        void drawParserView();
        void drawRegexPlayground();
        
        // Helper utils
        const char* getTokenName(Automata::TokenType t);
        void drawAutomaton(const std::vector<Automata::State>& states, int startId, const char* label, std::map<int, ImVec2>& positions, bool isNFA);
    };
}
