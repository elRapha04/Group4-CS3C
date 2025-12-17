#pragma once
#include <string>
#include <vector>
#include "FA.h"

namespace Automata {
    
    class RegexParser {
    public:
        // Main pipeline: Regex String -> Postfix -> NFA -> DFA
        static DFA createDFA(const std::string& regex, TokenType type);

        // Individual steps (public for visualization access)
        static std::string preprocessRegex(const std::string& regex); // Adds explicit concatenation '.'
        static std::string toPostfix(const std::string& infix);
        static NFA toNFA(const std::string& postfix);
        static DFA toDFA(const NFA& nfa, TokenType type);
        
    private:
        static int priority(char op);
    };

}
