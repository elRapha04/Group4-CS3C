#pragma once
#include <vector>
#include <string>
#include "FA.h"
#include "RegexParser.h"

namespace Automata {

    class Lexer {
    private:
        std::vector<DFA> tokenDFAs;
        // Priority order matters for resolving conflicts (e.g. keywords over identifiers)
        
    public:
        // Initialize with default patterns
        void init();
        
        // Add a specific regex rule
        void addRule(std::string regex, TokenType type);
        
        // Tokenize a full input string
        std::vector<Token> tokenize(std::string input);
        
        // Helper to merge multiple DFAs into one combined NFA/DFA (Optional for visualization)
        // For actual lexing, running independent DFAs or a combined DFA is a choice. 
        // We will run 'longest match' across all patterns.
    };

}
