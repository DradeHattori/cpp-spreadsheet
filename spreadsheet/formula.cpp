#include "formula.h"
#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include "sheet.h"

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << "#ARITHM!";
}

namespace {
    class Formula : public FormulaInterface {
    public:
        explicit Formula(std::string expression)
            : ast_(ParseFormulaAST(std::move(expression))) {}

        Value Evaluate(const SheetInterface& sheet) const override {
            const Sheet* sheet_ptr = dynamic_cast<const Sheet*>(&sheet);

            auto get_value = [&sheet_ptr](Position pos) {
                return sheet_ptr->GetValue(pos);
                };

            try {
                return ast_.Execute(get_value);
            }
            catch (const FormulaError& error) {
                return error;
            }
        }

        std::string GetExpression() const override {
            std::ostringstream output;
            ast_.PrintFormula(output);
            return output.str();
        }

        std::vector<Position> GetReferencedCells() const override {
            std::set<Position> unique_cells(ast_.GetCells().begin(), ast_.GetCells().end());
            return { unique_cells.begin(), unique_cells.end() };
        }

    private:
        FormulaAST ast_;
    };
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    }
    catch (...) {
        throw FormulaException("expression error: " + expression);
    }
}