#pragma once
#include <stack>
#include <vector>
#include <string>
#include "../lexer/FA.h" // For TokenType
#include "Grammar.h"

namespace Automata {

    struct ParseStep {
        std::vector<Symbol> stackSnapshot;
        Token currentInput;
        std::string actionDesc; // "Reduce Expr -> Term Expr'"
    };

    class PDA {
    public:
        std::vector<Symbol> parseStack; // Using vector as stack to easily iterate for drawing
        std::vector<Token> inputTokens;
        std::vector<ParseStep> history;
        int currentTokenIndex;
        bool isError;
        bool isSuccess;

        PDA(); // Sets up grammar
        
        void loadInput(const std::vector<Token>& tokens);
        void reset();
        
        // Executes one step of LL(1) parsing
        // Returns true if step was taken, false if finished/error
        bool step(); 
        
    private:
        // LL(1) Table lookup helper
        // Returns the production index or -1 if error
        int getProduction(const std::string& nonTerminal, TokenType lookahead);
        
        // Grammar hardcoded for simplicity of this assignment
        // Expr -> Term ExprPrime
        // ExprPrime -> + Term ExprPrime | - Term ExprPrime | eps
        // Term -> Factor TermPrime
        // TermPrime -> * Factor TermPrime | / Factor TermPrime | eps
        // Factor -> ( Expr ) | num | id
    };

}
