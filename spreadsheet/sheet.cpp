#include "sheet.h"
#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

using namespace std::literals;

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Неверная позиция");
    }

    if (auto existing_cell = position_cell_.find(pos); existing_cell == position_cell_.end() || ThisCell(pos)->IsEmpty()) {
        auto new_cell = std::make_unique<Cell>(*this);
        SaveCell(std::move(new_cell), pos);
    }

    position_cell_.at(pos)->Set(std::move(text));
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Неверная позиция");
    }

    auto cell_it = position_cell_.find(pos);
    return (cell_it != position_cell_.end()) ? cell_it->second.get() : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Неверная позиция");
    }

    auto cell_it = position_cell_.find(pos);
    return (cell_it != position_cell_.end()) ? cell_it->second.get() : nullptr;
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Неверная позиция");
    }

    if (GetCell(pos)) {
        if (!ThisCell(pos)->IsReferenced()) {
            position_cell_.erase(pos);
        }
        else {
            ThisCell(pos)->Clear();
        }

        auto& row_set = row_cell_.at(pos.row);
        row_set.erase(pos.col);
        if (row_set.empty()) {
            row_cell_.erase(pos.row);
        }

        auto& column_set = col_cell_.at(pos.col);
        column_set.erase(pos.row);
        if (column_set.empty()) {
            col_cell_.erase(pos.col);
        }
    }
}

Size Sheet::GetPrintableSize() const {
    Size size{ 0, 0 };

    if (!row_cell_.empty()) {
        size.rows = row_cell_.rbegin()->first + 1;
    }
    if (!col_cell_.empty()) {
        size.cols = col_cell_.rbegin()->first + 1;
    }

    return size;
}

void PrintValue(std::ostream& output, double value) {
    output << value;
}

void PrintValue(std::ostream& output, const std::string& value) {
    output << value;
}

void PrintValue(std::ostream& output, FormulaError value) {
    output << value;
}

void Sheet::PrintValues(std::ostream& output) const {
    Size size = GetPrintableSize();

    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            Position pos{ row, col };
            if (auto cell_ptr = GetCell(pos)) {
                std::visit([&output](const auto& value) { PrintValue(output, value); }, cell_ptr->GetValue());
            }

            if (col < size.cols - 1) {
                output << '\t';
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    Size size = GetPrintableSize();

    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            Position pos{ row, col };
            if (auto cell_ptr = GetCell(pos)) {
                output << cell_ptr->GetText();
            }

            if (col < size.cols - 1) {
                output << '\t';
            }
        }
        output << '\n';
    }
}

const Cell* Sheet::ThisCell(Position pos) const {
    return position_cell_.at(pos).get();
}

Cell* Sheet::ThisCell(Position pos) {
    return position_cell_.at(pos).get();
}

Cell* Sheet::GetOrCreate(Position pos) {
    if (position_cell_.find(pos) == position_cell_.end()) {
        SetCell(pos, "");
    }
    return ThisCell(pos);
}

CellInterface::Value Sheet::GetValue(Position pos) const {
    if (!pos.IsValid()) {
        throw FormulaError(FormulaError::Category::Ref);
    }

    return position_cell_.count(pos) == 0 ? 0.0 : ThisCell(pos)->GetValue();
}

void Sheet::SaveCell(std::unique_ptr<Cell> cell, Position pos) {
    position_cell_[pos] = std::move(cell);
    row_cell_[pos.row].insert(pos.col);
    col_cell_[pos.col].insert(pos.row);
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
