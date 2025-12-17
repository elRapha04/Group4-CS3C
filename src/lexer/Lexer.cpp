#include "Lexer.h"
#include <iostream>

namespace Automata {

    void Lexer::init() {
        // Simple manual definitions to avoid complex regex logic issues
        
        addRule("+", TOKEN_OPERATOR_PLUS);
        addRule("-", TOKEN_OPERATOR_MINUS);
        addRule("\\*", TOKEN_OPERATOR_MULT);
        addRule("/", TOKEN_OPERATOR_DIV);
        addRule("=", TOKEN_OPERATOR_EQ);
        addRule("\\(", TOKEN_LPAREN);
        addRule("\\)", TOKEN_RPAREN);
        
        // Numbers: 0-9 sequence
        // Explicitly simple: (0|1|2|3|4|5|6|7|8|9)(0|1|2|3|4|5|6|7|8|9)* 
        // We use a helper string
        std::string d = "(0|1|2|3|4|5|6|7|8|9)";
        addRule(d + d + "*", TOKEN_NUMBER);
        
        // ID: a-z sequence
        // We limit to a small set for safety if needed, but let's try full
        std::string a = "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)";
        addRule(a + "(" + a + "|" + d + ")*", TOKEN_IDENTIFIER);
    }

    void Lexer::addRule(std::string regex, TokenType type) {
        tokenDFAs.push_back(RegexParser::createDFA(regex, type));
    }

    std::vector<Token> Lexer::tokenize(std::string input) {
        std::vector<Token> output;
        int cursor = 0;
        int line = 1;
        
        while (cursor < (int)input.length()) {
            if (isspace(input[cursor])) {
                if(input[cursor] == '\n') line++;
                cursor++;
                continue;
            }

            int bestLen = 0;
            TokenType bestType = TOKEN_INVALID;
            
            for (auto& dfa : tokenDFAs) {
                int lastFinal = -1;
                int lastIdx = -1;
                dfa.simulate(input.substr(cursor), lastFinal, lastIdx);
                
                if (lastFinal != -1) {
                    if (lastIdx > bestLen) {
                        bestLen = lastIdx;
                        bestType = dfa.stateTokenMap[lastFinal];
                    }
                }
            }

            if (bestLen > 0) {
                Token t;
                t.type = bestType;
                t.value = input.substr(cursor, bestLen);
                t.position = cursor;
                t.line = line;
                output.push_back(t);
                cursor += bestLen;
            } else {
                Token t;
                t.type = TOKEN_UNKNOWN;
                t.value = std::string(1, input[cursor]);
                t.position = cursor;
                t.line = line;
                output.push_back(t);
                cursor++;
            }
        }
        
        Token eof; eof.type = TOKEN_EOF; eof.line = line;
        output.push_back(eof);
        return output;
    }
}
