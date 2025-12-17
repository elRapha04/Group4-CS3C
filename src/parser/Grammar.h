#pragma once
#include <string>
#include <vector>
#include <iostream>

namespace Automata {

    enum SymbolType {
        NON_TERMINAL,
        TERMINAL
    };

    struct Symbol {
        SymbolType type;
        std::string value; // e.g. "Expr", "Term" or "+"
        
        bool operator==(const Symbol& other) const {
            return type == other.type && value == other.value;
        }
    };

    struct Production {
        std::string lhs;
        std::vector<Symbol> rhs;
    };
    
    // Helper to print Symbols
    inline std::string toString(const Symbol& s) {
        return (s.type == NON_TERMINAL ? "<" + s.value + ">" : "'" + s.value + "'");
    }
}
