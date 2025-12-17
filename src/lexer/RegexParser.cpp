#include "RegexParser.h"
#include <stack>
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>

namespace Automata {

    int RegexParser::priority(char op) {
        if (op == '*') return 3;
        if (op == '+') return 3; // Same priority as star
        if (op == '.') return 2;
        if (op == '|') return 1;
        return 0;
    }

    bool isSpecial(char c) {
        return c == '*' || c == '+' || c == '|' || c == '.' || c == '(' || c == ')';
    }

    std::string RegexParser::preprocessRegex(const std::string& regex) {
        std::string res = "";
        for (int i = 0; i < (int)regex.length(); i++) {
            char c1 = regex[i];
            
            if (c1 == '\\') {
                // Escape: Copy \ and next char
                res += c1;
                i++;
                if (i < (int)regex.length()) res += regex[i];
                
                // If next is start of something, add dot
                if (i + 1 < (int)regex.length()) {
                    char next = regex[i+1];
                    bool nextIsOp = (next == '*' || next == '|' || next == ')' || next == '.');
                    if (!nextIsOp || next == '(') {
                        res += '.';
                    }
                }
                continue;
            }

            if (i + 1 < (int)regex.length()) {
                char c2 = regex[i+1];
                res += c1;

                bool c1IsLiteral = !isSpecial(c1);
                bool c1IsStar = (c1 == '*');
                bool c1IsPlus = (c1 == '+');
                bool c1IsClose = (c1 == ')');
                
                bool c2IsLiteral = !isSpecial(c2);
                bool c2IsOpen = (c2 == '(');
                bool c2IsEscape = (c2 == '\\');

                if ((c1IsLiteral || c1IsStar || c1IsPlus || c1IsClose) && (c2IsLiteral || c2IsOpen || c2IsEscape)) {
                    res += '.';
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

        for (int i = 0; i < (int)processed.length(); i++) {
            char c = processed[i];
            
            if (c == '\\') {
                postfix += c;
                i++;
                if (i < (int)processed.length()) postfix += processed[i];
            }
            else if (isalnum(c)) {
                 postfix += c;
            }
            else if (!isSpecial(c)) { 
                postfix += c;
            } 
            else if (c == '(') {
                opStack.push(c);
            } 
            else if (c == ')') {
                while (!opStack.empty() && opStack.top() != '(') {
                    postfix += opStack.top();
                    opStack.pop();
                }
                if (!opStack.empty()) opStack.pop();
            } 
            else {
                while (!opStack.empty() && priority(opStack.top()) >= priority(c)) {
                    postfix += opStack.top();
                    opStack.pop();
                }
                opStack.push(c);
            }
        }
        
        // Pop remaining operators
        while (!opStack.empty()) {
            // Discard opening parentheses that were unmatched
            if (opStack.top() != '(') {
                postfix += opStack.top();
            }
            opStack.pop();
        }
        return postfix;
    }

    NFA RegexParser::toNFA(const std::string& postfix) {
        std::stack<NFA> stack;

        for (int i = 0; i < (int)postfix.length(); i++) {
            char c = postfix[i];

            if (c == '\\') {
                i++;
                char lit = (i < (int)postfix.length()) ? postfix[i] : '\\';
                NFA n;
                int start = n.addState(false);
                int end = n.addState(true);
                n.addTransition(start, end, lit);
                n.startStateId = start;
                n.finalStateId = end;
                stack.push(n);
            }
            else if (!isSpecial(c)) { 
                NFA n;
                int start = n.addState(false);
                int end = n.addState(true);
                n.addTransition(start, end, c);
                n.startStateId = start;
                n.finalStateId = end;
                stack.push(n);
            } 
            else if (c == '.') { 
                if (stack.size() < 2) continue;
                NFA n2 = stack.top(); stack.pop();
                NFA n1 = stack.top(); stack.pop();
                
                NFA res = n1; // Use copy, logic below is essentially append
                // But full deep copy needed for safety due to index shifting
                res = NFA(); // Clear
                
                int start = res.addState(false); // Dummy start? No, reuse n1 start?
                // Proper merge:
                // New logic to ensure clean state IDs
                
                // Copy N1
                int offset1 = res.states.size(); // 0
                for(const auto& s : n1.states) {
                    State news = s; news.id += offset1;
                    for(auto& t : news.transitions) t.targetStateId += offset1;
                    res.states.push_back(news);
                }
                
                // Copy N2
                int offset2 = res.states.size();
                for(const auto& s : n2.states) {
                    State news = s; news.id += offset2;
                    for(auto& t : news.transitions) t.targetStateId += offset2;
                    res.states.push_back(news);
                }
                
                // Link N1 Final to N2 Start (Epsilon)
                int n1FinalMapped = n1.finalStateId + offset1;
                int n2StartMapped = n2.startStateId + offset2;
                
                res.states[n1FinalMapped].isFinal = false;
                res.addTransition(n1FinalMapped, n2StartMapped, '\0');
                
                res.startStateId = n1.startStateId + offset1;
                res.finalStateId = n2.finalStateId + offset2;
                res.states[res.finalStateId].isFinal = true;
                
                stack.push(res);

            } else if (c == '|') { 
                if (stack.size() < 2) continue;
                NFA n2 = stack.top(); stack.pop();
                NFA n1 = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                
                int offset1 = res.states.size();
                for(const auto& s : n1.states) {
                    State news = s; news.id += offset1;
                    for(auto& t : news.transitions) t.targetStateId += offset1;
                    res.states.push_back(news);
                }
                
                int offset2 = res.states.size();
                for(const auto& s : n2.states) {
                    State news = s; news.id += offset2;
                    for(auto& t : news.transitions) t.targetStateId += offset2;
                    res.states.push_back(news);
                }
                
                int final = res.addState(true);
                
                res.addTransition(start, n1.startStateId + offset1, '\0');
                res.addTransition(start, n2.startStateId + offset2, '\0');
                
                res.states[n1.finalStateId + offset1].isFinal = false;
                res.states[n2.finalStateId + offset2].isFinal = false;
                
                res.addTransition(n1.finalStateId + offset1, final, '\0');
                res.addTransition(n2.finalStateId + offset2, final, '\0');
                
                res.startStateId = start;
                res.finalStateId = final;
                
                stack.push(res);
                
            } else if (c == '*') { 
                if (stack.empty()) continue;
                NFA n = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                
                int offset = res.states.size(); 
                for(const auto& s : n.states) {
                    State news = s; news.id += offset;
                    for(auto& t : news.transitions) t.targetStateId += offset;
                    res.states.push_back(news);
                }
                
                int final = res.addState(true);
                
                int oldStart = n.startStateId + offset;
                int oldFinal = n.finalStateId + offset;
                
                res.states[oldFinal].isFinal = false;
                
                res.addTransition(start, oldStart, '\0');
                res.addTransition(oldFinal, oldStart, '\0');
                res.addTransition(oldFinal, final, '\0');
                res.addTransition(start, final, '\0');
                
                res.startStateId = start;
                res.finalStateId = final;
                
                stack.push(res);
            } else if (c == '+') {
                 if (stack.empty()) continue;
                 NFA n = stack.top(); stack.pop();
                 
                 NFA res;
                 int start = res.addState(false);
                 
                 int offset = res.states.size(); 
                 for(const auto& s : n.states) {
                     State news = s; news.id += offset;
                     for(auto& t : news.transitions) t.targetStateId += offset;
                     res.states.push_back(news);
                 }
                 
                 int final = res.addState(true);
                 
                 int oldStart = n.startStateId + offset;
                 int oldFinal = n.finalStateId + offset;
                 
                 res.states[oldFinal].isFinal = false; // Internalize
                 
                 // One or more: Must go through N at least once.
                 res.addTransition(start, oldStart, '\0'); // Enter N
                 res.addTransition(oldFinal, final, '\0'); // Exit N
                 res.addTransition(oldFinal, oldStart, '\0'); // Loop back
                 
                 res.startStateId = start;
                 res.finalStateId = final;
                 
                 stack.push(res);
            }
        }
        
        if (stack.empty()) {
            NFA empty;
            empty.addState(true); // Accept Epsilon
            return empty;
        }
        return stack.top();
    }

    std::set<int> epsilonClosure(const NFA& nfa, std::set<int> states) {
        std::stack<int> stack;
        for(int s : states) stack.push(s);
        
        while(!stack.empty()) {
            int u = stack.top(); stack.pop();
            if (u >= 0 && u < (int)nfa.states.size()) {
                for(const auto& t : nfa.states[u].transitions) {
                    if(t.input == '\0') {
                        if(states.find(t.targetStateId) == states.end()) {
                            states.insert(t.targetStateId);
                            stack.push(t.targetStateId);
                        }
                    }
                }
            }
        }
        return states;
    }

    std::set<int> move(const NFA& nfa, const std::set<int>& states, char c) {
        std::set<int> result;
        for(int s : states) {
            if (s >= 0 && s < (int)nfa.states.size()) {
                for(const auto& t : nfa.states[s].transitions) {
                    if(t.input == c) {
                        result.insert(t.targetStateId);
                    }
                }
            }
        }
        return result;
    }

    DFA RegexParser::toDFA(const NFA& nfa, TokenType type) {
        DFA dfa;
        if (nfa.states.empty()) return dfa;

        std::set<int> startSet; 
        startSet.insert(nfa.startStateId);
        startSet = epsilonClosure(nfa, startSet);
        
        int startId = dfa.states.size(); // 0
        State startState;
        startState.id = startId;
        startState.nfaStateIds = startSet;
        if(startSet.count(nfa.finalStateId)) {
            startState.isFinal = true;
            dfa.stateTokenMap[startId] = type;
        } else {
            startState.isFinal = false;
        }
        dfa.states.push_back(startState);
        
        std::queue<int> q;
        q.push(startId);
        
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
                
                int existingId = -1;
                for(int i=0; i<(int)dfa.states.size(); i++) {
                    if(dfa.states[i].nfaStateIds == nextSet) {
                        existingId = i;
                        break;
                    }
                }
                
                if(existingId == -1) {
                    State newState;
                    newState.id = (int)dfa.states.size();
                    newState.nfaStateIds = nextSet;
                    newState.isFinal = (nextSet.count(nfa.finalStateId) > 0);
                    if(newState.isFinal) dfa.stateTokenMap[newState.id] = type;
                    
                    dfa.states.push_back(newState);
                    existingId = newState.id;
                    q.push(existingId);
                }
                
                dfa.states[currentDfaId].transitions.push_back({c, existingId});
            }
        }
        
        return dfa;
    }
    
    DFA RegexParser::createDFA(const std::string& regex, TokenType type) {
         return toDFA(toNFA(toPostfix(regex)), type);
    }

}
