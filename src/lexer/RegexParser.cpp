#include "RegexParser.h"
#include <stack>
#include <queue>
#include <set>
#include <algorithm>
#include <iostream>
#include <map>

namespace Automata {

    int RegexParser::priority(char op) {
        if (op == '*') return 3;
        if (op == '+') return 3; 
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
                res += c1;
                i++;
                if (i < (int)regex.length()) res += regex[i];
                
                // Implicit concatenation after escaped char?
                // If next is literal or (, add dot
                if (i + 1 < (int)regex.length()) {
                    char next = regex[i+1];
                    // If next is not an operator (OR it is open paren), insert .
                    bool nextIsOp = (next == '*' || next == '+' || next == '|' || next == ')' || next == '.');
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
            else if (isalnum(c) || !isSpecial(c)) {
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
        
        while (!opStack.empty()) {
            if (opStack.top() != '(') {
                postfix += opStack.top();
            }
            opStack.pop();
        }
        return postfix;
    }

    // Helper to merge NFA states into a new NFA and return the offset
    int mergeNFA(NFA& target, const NFA& source) {
        int offset = target.states.size();
        for (const auto& s : source.states) {
            State newS = s;
            newS.id += offset;
            for (auto& t : newS.transitions) {
                t.targetStateId += offset;
            }
            target.states.push_back(newS);
        }
        return offset;
    }

    NFA RegexParser::toNFA(const std::string& postfix) {
        std::stack<NFA> stack;

        for (int i = 0; i < (int)postfix.length(); i++) {
            char c = postfix[i];

            if (c == '\\') {
                // Literal
                i++;
                char lit = (i < (int)postfix.length()) ? postfix[i] : '\\';
                
                NFA n;
                int s1 = n.addState(false);
                int s2 = n.addState(true);
                n.addTransition(s1, s2, lit);
                n.startStateId = s1;
                n.finalStateId = s2;
                stack.push(n);
            }
            else if (!isSpecial(c)) { 
                // Literal
                NFA n;
                int s1 = n.addState(false);
                int s2 = n.addState(true);
                n.addTransition(s1, s2, c);
                n.startStateId = s1;
                n.finalStateId = s2;
                stack.push(n);
            } 
            else if (c == '.') { 
                // Concatenation: AB
                if (stack.size() < 2) continue;
                NFA B = stack.top(); stack.pop();
                NFA A = stack.top(); stack.pop();
                
                NFA res = A; 
                // Merge B into res
                int offset = mergeNFA(res, B);
                
                // Connect Accept(A) -> Start(B) with Epsilon
                // A.finalStateId is valid in res
                // B.startStateId becomes B.startStateId + offset
                
                res.states[res.finalStateId].isFinal = false; // A's final is no longer final
                res.addTransition(res.finalStateId, B.startStateId + offset, '\0');
                
                res.finalStateId = B.finalStateId + offset; // New final is B's final
                res.states[res.finalStateId].isFinal = true;
                
                stack.push(res);
            } 
            else if (c == '|') { 
                // Union: A|B
                if (stack.size() < 2) continue;
                NFA B = stack.top(); stack.pop();
                NFA A = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                
                int offsetA = mergeNFA(res, A);
                int offsetB = mergeNFA(res, B);
                
                int final = res.addState(true);
                
                // eps from NewStart -> Start(A)
                res.addTransition(start, A.startStateId + offsetA, '\0');
                // eps from NewStart -> Start(B)
                res.addTransition(start, B.startStateId + offsetB, '\0');
                
                // eps from Accept(A) -> NewFinal
                res.states[A.finalStateId + offsetA].isFinal = false;
                res.addTransition(A.finalStateId + offsetA, final, '\0');
                
                // eps from Accept(B) -> NewFinal
                res.states[B.finalStateId + offsetB].isFinal = false;
                res.addTransition(B.finalStateId + offsetB, final, '\0');
                
                res.startStateId = start;
                res.finalStateId = final;
                
                stack.push(res);
            } 
            else if (c == '*') { 
                // Kleene Star: A*
                if (stack.empty()) continue;
                NFA A = stack.top(); stack.pop();
                
                NFA res;
                int start = res.addState(false);
                int offset = mergeNFA(res, A);
                int final = res.addState(true);
                
                int A_start = A.startStateId + offset;
                int A_final = A.finalStateId + offset;
                
                res.states[A_final].isFinal = false;
                
                // 1. eps: NewStart -> A_Start
                res.addTransition(start, A_start, '\0');
                // 2. eps: NewStart -> NewFinal (0 occurrences)
                res.addTransition(start, final, '\0');
                // 3. eps: A_Final -> NewFinal
                res.addTransition(A_final, final, '\0');
                // 4. eps: A_Final -> A_Start (Loop)
                res.addTransition(A_final, A_start, '\0');
                
                res.startStateId = start;
                res.finalStateId = final;
                
                stack.push(res);
            }
            else if (c == '+') {
                 // Plus: A+
                 // Similar to *, but no NewStart -> NewFinal epsilon
                 if (stack.empty()) continue;
                 NFA A = stack.top(); stack.pop();
                 
                 NFA res;
                 int start = res.addState(false);
                 int offset = mergeNFA(res, A);
                 int final = res.addState(true);
                 
                 int A_start = A.startStateId + offset;
                 int A_final = A.finalStateId + offset;
                 
                 res.states[A_final].isFinal = false;
                 
                 // 1. eps: NewStart -> A_Start (Must go in at least once)
                 res.addTransition(start, A_start, '\0');
                 // 2. eps: A_Final -> NewFinal
                 res.addTransition(A_final, final, '\0');
                 // 3. eps: A_Final -> A_Start (Loop)
                 res.addTransition(A_final, A_start, '\0');
                 
                 res.startStateId = start;
                 res.finalStateId = final;
                 
                 stack.push(res);
            }
        }
        
        if (stack.empty()) {
            NFA empty;
            empty.addState(true); 
            return empty;
        }
        
        NFA result = stack.top();
        result.optimize(); // Ensure no unreachable states
        return result;
    }

    std::set<int> epsilonClosure(const NFA& nfa, const std::set<int>& states) {
        std::set<int> closure = states;
        std::stack<int> stack;
        for(int s : states) stack.push(s);
        
        while(!stack.empty()) {
            int u = stack.top(); stack.pop();
            if (u >= 0 && u < (int)nfa.states.size()) {
                for(const auto& t : nfa.states[u].transitions) {
                    if(t.input == '\0') {
                        if(closure.find(t.targetStateId) == closure.end()) {
                            closure.insert(t.targetStateId);
                            stack.push(t.targetStateId);
                        }
                    }
                }
            }
        }
        return closure;
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

        // 1. Initial State = E-Closure(NFA Start)
        std::set<int> startSet; 
        startSet.insert(nfa.startStateId);
        startSet = epsilonClosure(nfa, startSet);
        
        int startId = dfa.states.size(); // 0
        State startState;
        startState.id = startId;
        startState.nfaStateIds = startSet; // Track subset
        
        if(startSet.count(nfa.finalStateId)) {
            startState.isFinal = true;
            dfa.stateTokenMap[startId] = type;
        } else {
            startState.isFinal = false;
        }
        dfa.states.push_back(startState);
        dfa.startStateId = startId;
        
        std::queue<int> q;
        q.push(startId);
        
        // Find Alphabet
        std::set<char> alphabet;
        for(const auto& s : nfa.states) 
            for(const auto& t : s.transitions) 
                if(t.input != '\0') alphabet.insert(t.input);
                
        // Subset Construction
        while(!q.empty()) {
            int currentDfaId = q.front(); q.pop();
            std::set<int> currentSet = dfa.states[currentDfaId].nfaStateIds;
            
            for(char c : alphabet) {
                // move(T, a) -> closure
                std::set<int> nextSet = epsilonClosure(nfa, move(nfa, currentSet, c));
                
                if(nextSet.empty()) continue; // No transition
                
                // Check if existing
                int targetId = -1;
                for(const auto& s : dfa.states) {
                    if(s.nfaStateIds == nextSet) {
                        targetId = s.id;
                        break;
                    }
                }
                
                if(targetId == -1) {
                    // New DFA state
                    State newState;
                    newState.id = (int)dfa.states.size();
                    newState.nfaStateIds = nextSet;
                    newState.isFinal = (nextSet.count(nfa.finalStateId) > 0);
                    if(newState.isFinal) dfa.stateTokenMap[newState.id] = type;
                    
                    targetId = newState.id;
                    dfa.states.push_back(newState);
                    q.push(targetId);
                }
                
                dfa.addTransition(currentDfaId, targetId, c);
            }
        }
        
        dfa.optimize(); // Ensure clean result (remove unreachable etc)
        return dfa;
    }
    
    DFA RegexParser::createDFA(const std::string& regex, TokenType type) {
         return toDFA(toNFA(toPostfix(regex)), type);
    }

}
