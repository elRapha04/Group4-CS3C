#pragma once
#include <string>
#include <vector>
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
        
        // Parser Visualization State
        int parserStepIndex; 

        // Internal Helpers
        void drawCodeEditor();
        void drawTokenTable();
        void drawParserView();
        void drawRegexPlayground(); // New!
    };
}
