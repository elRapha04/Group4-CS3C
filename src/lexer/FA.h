#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <iostream>

// Token Types for the Lexer
enum TokenType {
    TOKEN_INVALID,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_OPERATOR_PLUS,
    TOKEN_OPERATOR_MINUS,
    TOKEN_OPERATOR_MULT,
    TOKEN_OPERATOR_DIV,
    TOKEN_OPERATOR_EQ,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_UNKNOWN,
    TOKEN_EOF
};

struct Token {
    TokenType type;
    std::string value;
    int position; 
    int line;
};

namespace Automata {

    struct Transition {
        char input; // '\0' for Epsilon
        int targetStateId;
    };

    struct State {
        int id;
        bool isFinal;
        std::vector<Transition> transitions;
        std::set<int> nfaStateIds; // For DFA debugging: which NFA states does this map to?
    };

    class NFA {
    public:
        std::vector<State> states;
        int startStateId;
        int finalStateId;

        NFA() : startStateId(0), finalStateId(0) {}
        
        int addState(bool isFinal = false) {
            State s;
            s.id = (int)states.size();
            s.isFinal = isFinal;
            states.push_back(s);
            return s.id;
        }

        void addTransition(int from, int to, char input) {
            if (from < states.size()) {
                states[from].transitions.push_back({input, to});
            }
        }
    };

    class DFA {
    public:
        std::vector<State> states;
        int startStateId;
        // Keep track of which token type this final state recognizes
        // If a state is final for multiple NFA sources (unlikely in simple lexer), priority rules apply
        std::map<int, TokenType> stateTokenMap; 

        DFA() : startStateId(0) {}
    };
}
