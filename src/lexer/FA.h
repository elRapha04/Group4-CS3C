#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <iostream>

namespace Automata {

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
        std::map<int, TokenType> stateTokenMap; 

        DFA() : startStateId(0) {}
        
        // Simulates the input on this DFA.
        // Returns the final state reached, or -1 if stuck.
        // Updates lastFinalState and lastInputIndex to track the longest match prefix.
        int simulate(const std::string& input, int& lastFinalState, int& lastInputIndex) {
            int currentState = startStateId;
            lastFinalState = -1;
            lastInputIndex = -1;

            if (states.empty()) return -1;

            // Check if initial state is final (empty match)
            if (states[currentState].isFinal) {
                lastFinalState = currentState;
                lastInputIndex = 0;
            }

            for (int i = 0; i < (int)input.length(); i++) {
                char c = input[i];
                bool found = false;
                
                for (const auto& t : states[currentState].transitions) {
                    if (t.input == c) {
                        currentState = t.targetStateId;
                        found = true;
                        break;
                    }
                }

                if (!found) break; // Dead end

                if (states[currentState].isFinal) {
                    lastFinalState = currentState;
                    lastInputIndex = i + 1; // Length is index + 1
                }
            }
            
            return currentState;
        }
    };
}
