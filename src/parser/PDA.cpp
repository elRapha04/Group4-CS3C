#include "PDA.h"
#include <iostream>
#include <algorithm>

namespace Automata {

    PDA::PDA() {
        reset();
    }

    void PDA::reset() {
        parseStack.clear();
        inputTokens.clear();
        history.clear();
        currentTokenIndex = 0;
        isError = false;
        isSuccess = false;
        
        // Initial Stack: [EOF, Expr] (Symbol order: Top is last element)
        // We simulate a stack, so 'push' adds to back.
        // Stack bottom is index 0.
        parseStack.push_back({TERMINAL, "EOF"}); // End marker
        parseStack.push_back({NON_TERMINAL, "Statement"});
    }

    void PDA::loadInput(const std::vector<Token>& tokens) {
        reset();
        inputTokens = tokens;
        // Ensure tokens end with EOF if not present, though Lexer adds it.
    }

    // Helper to map Token Type to Grammar Terminal String
    std::string tokenToTerminal(TokenType t) {
        switch(t) {
            case TOKEN_IDENTIFIER: return "id";
            case TOKEN_NUMBER: return "num";
            case TOKEN_OPERATOR_PLUS: return "+";
            case TOKEN_OPERATOR_MINUS: return "-";
            case TOKEN_OPERATOR_MULT: return "*";
            case TOKEN_OPERATOR_DIV: return "/";
            case TOKEN_OPERATOR_EQ: return "="; // Assignment not in simple expression grammar usually, but user asked for 'x = ...'
            case TOKEN_LPAREN: return "(";
            case TOKEN_RPAREN: return ")";
            case TOKEN_LBRACE: return "{";
            case TOKEN_RBRACE: return "}";
            case TOKEN_EOF: return "EOF";
            default: return "";
        }
    }

    // ... (This function is helper, need to update PDA::step Logic for F)

    // In PDA::step around line 215 (F expansion)
    // We need to target the block handling F -> (E) | {E} | num | id
    // I can't easily target multiple disparate blocks with one replace, so I'll assume current context
    
    // ... skipping to PDA::step implementation ... 


    // For the user request: x = 10 + 20
    // The grammar provided:
    // Expr -> Expr + Term ...
    // This implies Expr is "Arithmetic Expression" value-wise.
    // "x = 10" is an Assignment.
    // I should augment the grammar to support Assignment if I want to parse "x = 10 + 20".
    // New Start: Stmt -> id = Expr | Expr
    // This keeps it simple and backward compatible.
    
    // Augmented Grammar (LL1 Friendly):
    // Stmt -> id Stmt' | Expr (Wait, id starts Expr too (Factor -> id))
    // This is a conflict: First(Stmt) = {id, num, (}
    // Stmt -> AssignmentOrExpr
    // AssignmentOrExpr -> id RestOfAssignmentOrExpr | num ... | ( ...
    // Let's just wrap it:
    // Program -> id = Expr | Expr (If lookahead after id is =, it's assignment)
    // But LL(1) only looks at 1 token.
    // id could be Factor or LHS.
    // We can do: Stmt -> Expr | id = Expr ?? NO
    
    // Let's use:
    // S -> id = Expr | Expr
    // But conflict on 'id'.
    // If next token is '=', it is assignment.
    // Factor -> id
    
    // We will stick to the provided grammar for Expr, and just add specific handling for "x = ..." at top level?
    // User sample: "x = 10 + 20".
    // I will add: S -> id = Expr | Expr (We can cheat LL(1) by looking ahead 2 tokens if needed, but let's try to be pure)
    
    // Workaround:
    // S -> id S_Prime | OtherExpr
    // S_Prime -> = Expr | Term_Prime Expr_Prime
    // OtherExpr -> num Term_Prime Expr_Prime | ( Expr ) Term_Prime Expr_Prime
    
    // Refined Grammar:
    // S -> E
    // E -> T E'
    // E' -> + T E' | - T E' | epsilon
    // T -> F T'
    // T' -> * F T' | / F T' | epsilon
    // F -> ( E ) | num | id
    //
    // AND to support assignment: "S -> id = E" vs "S -> E"
    // Since 'id' is in First(E), we have a conflict.
    // I will simply assume the user input is ALWAYS E, unless it starts with "id =".
    // Actually, I'll extend the grammar slightly effectively.
    // S -> id Rest | num T' E' | ( E ) T' E'
    // Rest -> = E | T' E'
    
    bool PDA::step() {
        if (isError || isSuccess) return false;
        if (parseStack.empty()) {
            isSuccess = (currentTokenIndex >= inputTokens.size() - 1); // Only EOF left
            return false;
        }

        Symbol top = parseStack.back();
        Token currentToken = inputTokens[currentTokenIndex];
        std::string currentVal = tokenToTerminal(currentToken.type);

        // Snapshot
        ParseStep stepRecord;
        stepRecord.stackSnapshot = parseStack;
        stepRecord.currentInput = currentToken;

        if (top.type == TERMINAL) {
            if (top.value == "epsilon") {
                parseStack.pop_back();
                stepRecord.actionDesc = "Skip Empty (epsilon)";
                history.push_back(stepRecord);
                return true;
            }
            if (top.value == currentVal) {
                parseStack.pop_back();
                if (top.value != "EOF") {
                    currentTokenIndex++;
                } else {
                    isSuccess = true;
                }
                stepRecord.actionDesc = "Match Terminal '" + top.value + "'";
                history.push_back(stepRecord);
                return true;
            } else {
                isError = true;
                stepRecord.actionDesc = "Error: Expected '" + top.value + "', but found '" + currentVal + "'";
                history.push_back(stepRecord);
                return false;
            }
        } 
        else { 
            // Non-Terminal Expansion
            parseStack.pop_back();
            std::vector<Symbol> production;
            std::string ruleName;

            // --- LL(1) Table Logic ---
            // Renamed Non-Terminals for Clarity:
            // S -> Statement
            // E -> Expr
            // E' -> Expr_Rest (Handles + or -)
            // T -> Term
            // T' -> Term_Rest (Handles * or /)
            // F -> Factor
            
            if (top.value == "S" || top.value == "Statement") {
                if (currentToken.type == TOKEN_IDENTIFIER) {
                     if (currentTokenIndex + 1 < inputTokens.size() && 
                         inputTokens[currentTokenIndex + 1].type == TOKEN_OPERATOR_EQ) {
                         // S -> id = E
                         production = {{TERMINAL, "id"}, {TERMINAL, "="}, {NON_TERMINAL, "Expr"}};
                         ruleName = "Statement -> Assignment (id = Expr)";
                     } else {
                         // S -> E
                         production = {{NON_TERMINAL, "Expr"}};
                         ruleName = "Statement -> Expression";
                     }
                } else {
                    // S -> E
                    production = {{NON_TERMINAL, "Expr"}};
                    ruleName = "Statement -> Expression";
                }
            }
            else if (top.value == "E" || top.value == "Expr") {
                // E -> T E'
                if (currentVal == "id" || currentVal == "num" || currentVal == "(" || currentVal == "{") {
                    production = {{NON_TERMINAL, "Term"}, {NON_TERMINAL, "Expr_Rest"}};
                    ruleName = "Expr -> Term + Rest";
                } else {
                    isError = true;
                }
            }
            else if (top.value == "E'" || top.value == "Expr_Rest") {
                // E' -> + T E' | - T E' | epsilon
                if (currentVal == "+") {
                    production = {{TERMINAL, "+"}, {NON_TERMINAL, "Term"}, {NON_TERMINAL, "Expr_Rest"}};
                    ruleName = "Expr_Rest -> Add (+ Term ...)";
                } else if (currentVal == "-") {
                    production = {{TERMINAL, "-"}, {NON_TERMINAL, "Term"}, {NON_TERMINAL, "Expr_Rest"}};
                    ruleName = "Expr_Rest -> Subtract (- Term ...)";
                } else if (currentVal == ")" || currentVal == "}" || currentVal == "EOF") {
                    production = {{TERMINAL, "epsilon"}};
                    ruleName = "Expr_Rest -> End (epsilon)";
                } else {
                    isError = true;
                }
            }
            else if (top.value == "T" || top.value == "Term") {
                // T -> F T'
                if (currentVal == "id" || currentVal == "num" || currentVal == "(" || currentVal == "{") {
                    production = {{NON_TERMINAL, "Factor"}, {NON_TERMINAL, "Term_Rest"}};
                    ruleName = "Term -> Factor * Rest";
                } else {
                    isError = true;
                }
            }
            else if (top.value == "T'" || top.value == "Term_Rest") {
                // T' -> * F T' | / F T' | epsilon
                if (currentVal == "*") {
                    production = {{TERMINAL, "*"}, {NON_TERMINAL, "Factor"}, {NON_TERMINAL, "Term_Rest"}};
                    ruleName = "Term_Rest -> Mult (* Factor ...)";
                } else if (currentVal == "/") {
                    production = {{TERMINAL, "/"}, {NON_TERMINAL, "Factor"}, {NON_TERMINAL, "Term_Rest"}};
                    ruleName = "Term_Rest -> Div (/ Factor ...)";
                } else if (currentVal == "+" || currentVal == "-" || currentVal == ")" || currentVal == "}" || currentVal == "EOF") {
                    production = {{TERMINAL, "epsilon"}};
                    ruleName = "Term_Rest -> End (epsilon)";
                } else {
                    isError = true;
                }
            }
            else if (top.value == "F" || top.value == "Factor") {
                // F -> ( E ) | { E } | num | id
                if (currentVal == "(") {
                    production = {{TERMINAL, "("}, {NON_TERMINAL, "Expr"}, {TERMINAL, ")"}};
                    ruleName = "Factor -> Group ( Expr )";
                } else if (currentVal == "{") {
                     production = {{TERMINAL, "{"}, {NON_TERMINAL, "Statement"}, {TERMINAL, "}"}};
                     ruleName = "Factor -> Block { Statement }";
                } else if (currentVal == "num") {
                    production = {{TERMINAL, "num"}};
                    ruleName = "Factor -> Number";
                } else if (currentVal == "id") {
                    production = {{TERMINAL, "id"}};
                    ruleName = "Factor -> Identifier";
                } else {
                    isError = true;
                }
            }
            else {
                isError = true;
                ruleName = "Unknown Symbol " + top.value;
            }

            if (!isError) {
                // Push RHS in Reverse
                for (int i = production.size() - 1; i >= 0; i--) {
                    if (production[i].value != "epsilon") {
                        parseStack.push_back(production[i]);
                    }
                }
                stepRecord.actionDesc = ruleName;
                history.push_back(stepRecord);
                return true;
            } else {
                 if (top.value == ")" || top.value == "(") {
                     stepRecord.actionDesc = "Error: Mismatched Parentheses. Expected " + top.value;
                 } else if (top.value == "}" || top.value == "{") {
                     stepRecord.actionDesc = "Error: Mismatched Block. Expected " + top.value;
                 } else {
                     stepRecord.actionDesc = "Stack Error: Cannot expand " + top.value + " with input '" + currentVal + "'";
                 }
                 history.push_back(stepRecord);
                 return false;
            }
        }
        return false;
    }

}
