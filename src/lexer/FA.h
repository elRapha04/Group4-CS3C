#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <iostream>
#include <algorithm>
#include <queue>

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
        TOKEN_LBRACE,
        TOKEN_RBRACE,
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
        
        bool operator<(const Transition& other) const {
            if (input != other.input) return input < other.input;
            return targetStateId < other.targetStateId;
        }
        bool operator==(const Transition& other) const {
            return input == other.input && targetStateId == other.targetStateId;
        }
    };

    struct State {
        int id;
        bool isFinal;
        std::vector<Transition> transitions;
        std::set<int> nfaStateIds; // For DFA debugging
    };

    class AutomatonBase {
    public:
        std::vector<State> states;
        int startStateId;
        int finalStateId; // Only for NFA typically, but base keeps it valid

        AutomatonBase() : startStateId(0), finalStateId(0) {}

        int addState(bool isFinal = false) {
            State s;
            s.id = (int)states.size();
            s.isFinal = isFinal;
            states.push_back(s);
            return s.id;
        }

        void addTransition(int from, int to, char input) {
            if (from < (int)states.size()) {
                states[from].transitions.push_back({input, to});
            }
        }

        void optimize() {
            if (states.empty()) return;

            // 1. Remove Duplicate Transitions
            for(auto& s : states) {
                std::sort(s.transitions.begin(), s.transitions.end());
                s.transitions.erase(std::unique(s.transitions.begin(), s.transitions.end()), s.transitions.end());
            }

            // 2. Remove Unreachable States (BFS)
            std::set<int> reachable;
            std::queue<int> q;
            
            if (startStateId < (int)states.size()) {
                q.push(startStateId);
                reachable.insert(startStateId);
            }

            while(!q.empty()) {
                int u = q.front(); q.pop();
                if (u >= (int)states.size()) continue;

                for(const auto& t : states[u].transitions) {
                    if (reachable.find(t.targetStateId) == reachable.end()) {
                        reachable.insert(t.targetStateId);
                        q.push(t.targetStateId);
                    }
                }
            }

            // Rebuild States
            std::vector<State> newStates;
            std::map<int, int> oldToNew;
            
            // Should valid states strictly be reachable? Yes.
            // But we must preserve startStateId at index 0? Not necessarily, but cleaner.
            
            // Map Start First
            if (reachable.count(startStateId)) {
                oldToNew[startStateId] = 0;
                State newStart = states[startStateId];
                newStart.id = 0;
                newStates.push_back(newStart);
            } else {
                // Empty automaton or invalid start?
                states.clear();
                return; 
            }

            for(int i=0; i<(int)states.size(); i++) {
                if (i == startStateId) continue;
                if (reachable.count(i)) {
                    int newId = (int)newStates.size();
                    oldToNew[i] = newId;
                    State s = states[i];
                    s.id = newId;
                    newStates.push_back(s);
                }
            }
            
            // Update Transitions
            for(auto& s : newStates) {
                for(auto& t : s.transitions) {
                    if (oldToNew.count(t.targetStateId)) {
                        t.targetStateId = oldToNew[t.targetStateId];
                    } else {
                        // Should not happen if logic is correct
                        t.targetStateId = 0; // fallback
                    }
                }
            }

            // Update Metadata
            startStateId = oldToNew[startStateId];
            if (oldToNew.count(finalStateId)) finalStateId = oldToNew[finalStateId];
            else finalStateId = -1; // Final state was unreachable?

            states = newStates;
        }
    };

    class NFA : public AutomatonBase {};

    class DFA : public AutomatonBase {
    public:
        // Keep track of which token type this final state recognizes
        std::map<int, TokenType> stateTokenMap; 

        // Simulates the input on this DFA.
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
        
        void optimize() {
            if (states.empty()) return;

            // 1. Remove Duplicate Transitions
            for(auto& s : states) {
                std::sort(s.transitions.begin(), s.transitions.end());
                s.transitions.erase(std::unique(s.transitions.begin(), s.transitions.end()), s.transitions.end());
            }

            // 2. Remove Unreachable States (BFS)
            std::set<int> reachable;
            std::queue<int> q;
            
            if (startStateId < (int)states.size()) {
                q.push(startStateId);
                reachable.insert(startStateId);
            }

            while(!q.empty()) {
                int u = q.front(); q.pop();
                if (u >= (int)states.size()) continue;

                for(const auto& t : states[u].transitions) {
                    if (reachable.find(t.targetStateId) == reachable.end()) {
                        reachable.insert(t.targetStateId);
                        q.push(t.targetStateId);
                    }
                }
            }

            std::vector<State> newStates;
            std::map<int, int> oldToNew;
            std::map<int, TokenType> newTokenMap;
            
            if (reachable.count(startStateId)) {
                oldToNew[startStateId] = 0;
                State newStart = states[startStateId];
                newStart.id = 0;
                newStates.push_back(newStart);
                
                if (stateTokenMap.count(startStateId)) {
                    newTokenMap[0] = stateTokenMap[startStateId];
                }
            } else {
                states.clear();
                return;
            }

            for(int i=0; i<(int)states.size(); i++) {
                if (i == startStateId) continue;
                if (reachable.count(i)) {
                    int newId = (int)newStates.size();
                    oldToNew[i] = newId;
                    State s = states[i];
                    s.id = newId;
                    newStates.push_back(s);
                    
                    if (stateTokenMap.count(i)) {
                        newTokenMap[newId] = stateTokenMap[i];
                    }
                }
            }
            
            for(auto& s : newStates) {
                for(auto& t : s.transitions) {
                    if (oldToNew.count(t.targetStateId)) {
                        t.targetStateId = oldToNew[t.targetStateId];
                    } else {
                        t.targetStateId = 0;
                    }
                }
            }

            startStateId = oldToNew[startStateId];
            if (oldToNew.count(finalStateId)) finalStateId = oldToNew[finalStateId];
            else finalStateId = -1;

            states = newStates;
            stateTokenMap = newTokenMap;
        }
    };
}
