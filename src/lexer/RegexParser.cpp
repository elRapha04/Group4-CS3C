#include "RegexParser.h"
#include <stack>
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>

namespace Automata {

    int RegexParser::priority(char op) {
        if (op == '*') return 3;
        if (op == '.') return 2; // Explicit concatenation
        if (op == '|') return 1;
        return 0;
    }

    std::string RegexParser::preprocessRegex(const std::string& regex) {
        std::string res = "";
        for (int i = 0; i < regex.length(); i++) {
            char c1 = regex[i];
            if (i + 1 < regex.length()) {
                char c2 = regex[i+1];
                res += c1;
                // Add concatenation operator '.' between:
                // 1. Literal and Literal (a b)
                // 2. Literal and (
                // 3. * and Literal
                // 4. * and (
                // 5. ) and Literal
                // 6. ) and (
                bool isLiteral1 = (c1 != '(' && c1 != '|' && c1 != '.'); // * is a postfix op, so it behaves like a literal ending
                bool isLiteral2 = (c2 != ')' && c2 != '|' && c2 != '*' && c2 != '.');
                
                if (c1 != '(' && c1 != '|' && c2 != ')' && c2 != '|' && c2 != '*') {
                     // Simplified logic: insert concat if current is not open-group/union and next is not close-group/union/star
                     // But strictly: 
                     // a . b
                     // a . (
                     // * . a
                     // * . (
                     // ) . a
                     // ) . (
                     bool c1Valid = (isalnum(c1) || c1 == '*' || c1 == ')');
                     bool c2Valid = (isalnum(c2) || c2 == '(');
                     // Note: We are handling simplified regex. Escaping is not handled deeply here for simplicity.
                     if (c1Valid && c2Valid) {
                         res += '.';
                     }
                }
            } else {
                res += c1;
            }
        }
        return res;
    }

    std::string RegexParser::toPostfix(const std::string& infix) {
        std::string processed = preprocessRegex(infix);
        std::string postfix = "";
        std::stack<char> opStack;

        for (char c : processed) {
            if (isalnum(c)) {
                postfix += c;
            } else if (c == '(') {
                opStack.push(c);
            } else if (c == ')') {
                while (!opStack.empty() && opStack.top() != '(') {
                    postfix += opStack.top();
                    opStack.pop();
                }
                if (!opStack.empty()) opStack.pop();
            } else {
                while (!opStack.empty() && priority(opStack.top()) >= priority(c)) {
                    postfix += opStack.top();
                    opStack.pop();
                }
                opStack.push(c);
            }
        }
        while (!opStack.empty()) {
            postfix += opStack.top();
            opStack.pop();
        }
        return postfix;
    }

    NFA RegexParser::toNFA(const std::string& postfix) {
        std::stack<NFA> stack;

        for (char c : postfix) {
            if (isalnum(c)) { // Literal
                NFA n;
                int start = n.addState(false);
                int end = n.addState(true);
                n.addTransition(start, end, c);
                n.startStateId = start;
                n.finalStateId = end;
                stack.push(n);
            } else if (c == '.') { // Concatenation
                NFA n2 = stack.top(); stack.pop();
                NFA n1 = stack.top(); stack.pop();
                
                // Merge n1 and n2
                // n1.final -> n2.start (epsilon)
                // All states of n2 need to be offset by n1.states.size()
                
                // We actually copy states to a new NFA to be clean
                NFA res;
                int offset1 = 0;
                for(auto& s : n1.states) {
                    int id = res.addState(false); 
                    // Copy transitions
                }
                // Wait, simpler approach: Modify n1 to include n2
                // Merging logic is verbose, let's just append n2's states to n1
                
                int offset = n1.states.size();
                n1.states[n1.finalStateId].isFinal = false; // Old final is no longer final
                // Add epsilon transition from old final to n2 start
                
                // Copy n2 states
                for(const auto& s : n2.states) {
                    State newState = s;
                    newState.id += offset;
                    for(auto& t : newState.transitions) {
                        t.targetStateId += offset;
                    }
                    n1.states.push_back(newState);
                }
                
                n1.addTransition(n1.finalStateId, n2.startStateId + offset, '\0');
                n1.finalStateId = n2.finalStateId + offset;
                n1.states[n1.finalStateId].isFinal = true;
                
                stack.push(n1);

            } else if (c == '|') { // Union
                NFA n2 = stack.top(); stack.pop();
                NFA n1 = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                
                // Copy n1
                int offset1 = res.states.size();
                for(const auto& s : n1.states) {
                    State news = s; news.id += offset1;
                    for(auto& t : news.transitions) t.targetStateId += offset1;
                    res.states.push_back(news);
                }
                
                // Copy n2
                int offset2 = res.states.size();
                for(const auto& s : n2.states) {
                    State news = s; news.id += offset2;
                    for(auto& t : news.transitions) t.targetStateId += offset2;
                    res.states.push_back(news);
                }
                
                int final = res.addState(true);
                
                // Start -> n1.start, Start -> n2.start
                res.addTransition(start, n1.startStateId + offset1, '\0');
                res.addTransition(start, n2.startStateId + offset2, '\0');
                
                // n1.final -> Final, n2.final -> Final
                res.states[n1.finalStateId + offset1].isFinal = false;
                res.states[n2.finalStateId + offset2].isFinal = false;
                
                res.addTransition(n1.finalStateId + offset1, final, '\0');
                res.addTransition(n2.finalStateId + offset2, final, '\0');
                
                res.startStateId = start;
                res.finalStateId = final;
                
                stack.push(res);
                
            } else if (c == '*') { // Kleene Star
                NFA n = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                int final = res.addState(true);
                
                int offset = res.states.size(); // Current size is 2 (start, final) - wait, no
                // We add start/final first, so offset is 2? No, let's append N first then wrap
                
                // Better order:
                // NewStart -> OldStart
                // OldFinal -> OldStart
                // OldFinal -> NewFinal
                // NewStart -> NewFinal
                
                int oldStart = n.startStateId; 
                int oldFinal = n.finalStateId;
                
                int offsetN = 1; // We added 1 state (start) before N
                
                // Logic:
                // 1. Create structure with new start
                // 2. Append N
                // 3. Add new final
                // 4. Link
                
                // Reset res
                res = NFA();
                int newStart = res.addState(false);
                
                for(const auto& s : n.states) {
                    State news = s; news.id += 1;
                    for(auto& t : news.transitions) t.targetStateId += 1;
                    res.states.push_back(news);
                }
                
                int newFinal = res.addState(true);
                
                int shiftedOldStart = oldStart + 1;
                int shiftedOldFinal = oldFinal + 1;
                
                res.states[shiftedOldFinal].isFinal = false;
                
                res.addTransition(newStart, shiftedOldStart, '\0'); // 1
                res.addTransition(shiftedOldFinal, shiftedOldStart, '\0'); // 2 (Loop)
                res.addTransition(shiftedOldFinal, newFinal, '\0'); // 3
                res.addTransition(newStart, newFinal, '\0'); // 4 (Skip)
                
                res.startStateId = newStart;
                res.finalStateId = newFinal;
                
                stack.push(res);
            }
        }
        
        return stack.top();
    }

    // Helper for Epsilon Closure
    std::set<int> epsilonClosure(const NFA& nfa, std::set<int> states) {
        std::stack<int> stack;
        for(int s : states) stack.push(s);
        
        while(!stack.empty()) {
            int u = stack.top(); stack.pop();
            for(const auto& t : nfa.states[u].transitions) {
                if(t.input == '\0') {
                    if(states.find(t.targetStateId) == states.end()) {
                        states.insert(t.targetStateId);
                        stack.push(t.targetStateId);
                    }
                }
            }
        }
        return states;
    }

    // Helper for Move
    std::set<int> move(const NFA& nfa, const std::set<int>& states, char c) {
        std::set<int> result;
        for(int s : states) {
            for(const auto& t : nfa.states[s].transitions) {
                if(t.input == c) {
                    result.insert(t.targetStateId);
                }
            }
        }
        return result;
    }

    DFA RegexParser::toDFA(const NFA& nfa, TokenType type) {
        DFA dfa;
        
        // Initial state = E-Closure(NFA start)
        std::set<int> startSet; 
        startSet.insert(nfa.startStateId);
        startSet = epsilonClosure(nfa, startSet);
        
        std::vector<std::set<int>> dfaSets;
        dfaSets.push_back(startSet);
        
        int startId = dfa.states.size(); // 0
        State startState;
        startState.id = startId;
        startState.nfaStateIds = startSet;
        // Check if final
        if(startSet.count(nfa.finalStateId)) {
            startState.isFinal = true;
            dfa.stateTokenMap[startId] = type;
        } else {
            startState.isFinal = false;
        }
        dfa.states.push_back(startState);
        
        std::queue<int> q;
        q.push(startId);
        
        // Alphabet inference (usually we pass this, but we can scan the NFA)
        std::set<char> alphabet;
        for(const auto& s : nfa.states) 
            for(const auto& t : s.transitions) 
                if(t.input != '\0') alphabet.insert(t.input);
                
        while(!q.empty()) {
            int currentDfaId = q.front(); q.pop();
            std::set<int> currentSet = dfa.states[currentDfaId].nfaStateIds;
            
            for(char c : alphabet) {
                std::set<int> nextSet = epsilonClosure(nfa, move(nfa, currentSet, c));
                if(nextSet.empty()) continue;
                
                // Check if this set exists
                int existingId = -1;
                for(int i=0; i<dfa.states.size(); i++) {
                    if(dfa.states[i].nfaStateIds == nextSet) {
                        existingId = i;
                        break;
                    }
                }
                
                if(existingId == -1) {
                    // Create new state
                    State newState;
                    newState.id = dfa.states.size();
                    newState.nfaStateIds = nextSet;
                    newState.isFinal = (nextSet.count(nfa.finalStateId) > 0);
                    if(newState.isFinal) dfa.stateTokenMap[newState.id] = type;
                    
                    dfa.states.push_back(newState);
                    existingId = newState.id;
                    q.push(existingId);
                }
                
                // Add transition
                dfa.states[currentDfaId].transitions.push_back({c, existingId});
            }
        }
        
        return dfa;
    }
    
    DFA RegexParser::createDFA(const std::string& regex, TokenType type) {
         return toDFA(toNFA(toPostfix(regex)), type);
    }

}
