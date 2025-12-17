#include "Lexer.h"
#include <iostream>

namespace Automata {

    void Lexer::init() {
        // Order matters! 
        // More specific first if we just take first match, but best practice is Longest Match.
        // If length is equal, take first defined.
        
        // Operators
        addRule("\\+", TOKEN_OPERATOR_PLUS);
        addRule("-", TOKEN_OPERATOR_MINUS);
        addRule("\\*", TOKEN_OPERATOR_MULT);
        addRule("/", TOKEN_OPERATOR_DIV);
        addRule("=", TOKEN_OPERATOR_EQ);
        addRule("\\(", TOKEN_LPAREN);
        addRule("\\)", TOKEN_RPAREN);
        
        // Numbers: [0-9]+
        // Note: RegexParser implementation is basic.
        // We need to support ranges or multiple rules. 
        // Since my parser supports |, we can do 0|1|2... 
        // But that's verbose. Let's assume the preprocessor handles [0-9] shorthand or we manually expand.
        // For this assignment, I'll cheat slightly and make 'd' map to 0-9 in the regex preprocessing if strictly needed,
        // or just use a helper to generate "0|1|2|3|4|5|6|7|8|9".
        
        std::string digit = "(0|1|2|3|4|5|6|7|8|9)";
        std::string posInt = digit + digit + "*"; // d . d*
        addRule(posInt, TOKEN_NUMBER);
        
        // ID: [a-z]+
        // Simplified to just a,b,c,x,y for brevity or expand if needed.
        // Let's expand common charset.
        std::string alpha = "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)";
        std::string ident = alpha + "(" + alpha + "|" + digit + ")*"; 
        addRule(ident, TOKEN_IDENTIFIER);
    }

    void Lexer::addRule(std::string regex, TokenType type) {
        tokenDFAs.push_back(RegexParser::createDFA(regex, type));
    }

    std::vector<Token> Lexer::tokenize(std::string input) {
        std::vector<Token> output;
        int cursor = 0;
        int line = 1;
        
        while (cursor < input.length()) {
            // Skip whitespace
            if (isspace(input[cursor])) {
                if(input[cursor] == '\n') line++;
                cursor++;
                continue;
            }

            // Find longest match
            int bestLen = 0;
            TokenType bestType = TOKEN_INVALID;
            int bestDFAIndex = -1;
            
            for (int i = 0; i < tokenDFAs.size(); i++) {
                int lastFinal = -1;
                int lastIdx = -1;
                // We pass substring from cursor
                tokenDFAs[i].simulate(input.substr(cursor), lastFinal, lastIdx);
                
                if (lastFinal != -1) {
                    // lastIdx is the length of the match relative to substr start ?
                    // The simulate returns lastInputIndex which is index locally.
                    // If matched "abc", index is 3 (length).
                    int matchLen = lastIdx; 
                    if (matchLen > bestLen) {
                        bestLen = matchLen;
                        bestType = tokenDFAs[i].stateTokenMap[lastFinal];
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
                // Error / Unknown char
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
