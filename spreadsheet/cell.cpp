// cell.cpp
#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <utility>
#include <algorithm>
#include <memory>

#include "sheet.h"

Cell::Cell(Sheet& sheet) : impl_(std::make_unique<EmptyImpl>()), sheet_(&sheet) {}

void Cell::Set(std::string text) {
    const std::string& current_text = impl_->GetText();
    if (text == current_text) {
        return;
    }

    RemoveDependence();

    if (text.empty()) {
        impl_ = std::make_unique<EmptyImpl>();
    }
    else if (text[0] == FORMULA_SIGN && text.size() > 1) {
        std::unique_ptr<Impl> temp_impl;
        try {
            temp_impl = std::make_unique<FormulaImpl>(text.substr(1), *sheet_);
        }
        catch (const std::exception&) {
            throw FormulaException("incorrect formula syntax");
        }

        auto referenced_cells = dynamic_cast<FormulaImpl*>(temp_impl.get())->GetReferencedCells();
        if (!referenced_cells.empty()) {
            std::vector<const Cell*> cells_to_check;
            for (const auto& pos : referenced_cells) {
                cells_to_check.push_back(sheet_->GetOrCreate(pos));
            }
            CheckDependencies(cells_to_check);
        }

        if (!referenced_cells.empty()) {
            for (const auto& pos : referenced_cells) {
                sheet_->ThisCell(pos)->AddRefference(this);
            }
        }

        std::swap(impl_, temp_impl);
    }
    else {
        impl_ = std::make_unique<TextImpl>(std::move(text));
    }

    ClearCache();
}

void Cell::Clear() {
    Set({});
}

Cell::Value Cell::GetValue() const {
    try {
        return impl_->GetValue();
    }
    catch (const FormulaError& err) {
        return err;
    }
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    if (auto* form = dynamic_cast<FormulaImpl*>(impl_.get())) {
        return form->GetReferencedCells();
    }
    return {};
}

bool Cell::IsReferenced() const {
    return !referring_cells_.empty();
}

bool Cell::IsEmpty() const {
    return dynamic_cast<EmptyImpl*>(impl_.get()) != nullptr;
}

void Cell::AddRefference(Cell* cell) {
    referring_cells_.push_back(cell);
}

void Cell::DeleteRefference(Cell* cell) {
    referring_cells_.erase(std::remove(referring_cells_.begin(), referring_cells_.end(), cell), referring_cells_.end());

}

void Cell::ClearCache() {
    if (ChacheIsFull()) {
        impl_->ClearCache();
        for (auto* cell : referring_cells_) {
            cell->ClearCache();
        }
    }
}

void Cell::RemoveDependence() {
    for (const auto& pos : GetReferencedCells()) {
        if (auto* cell_ptr = sheet_->ThisCell(pos)) {
            cell_ptr->DeleteRefference(this);
        }
    }
}

void Cell::RecursedCheck(const Cell* start_pos, std::unordered_set<const Cell*>& visited_cells) const {
    if (visited_cells.count(this)) {
        return;
    }

    if (this == start_pos) {
        throw CircularDependencyException("circular dependency detected");
    }

    visited_cells.insert(this);

    for (const auto& pos : GetReferencedCells()) {
        if (const auto* cell = sheet_->ThisCell(pos)) {
            cell->RecursedCheck(start_pos, visited_cells);
        }
    }
}

void Cell::CheckDependencies(const std::vector<const Cell*>& referenced_cells) const {
    if (std::find(referenced_cells.begin(), referenced_cells.end(), static_cast<const Cell*>(this)) != referenced_cells.end()) {
        throw CircularDependencyException("circular dependency detected");
    }


    std::unordered_set<const Cell*> checked_cells;
    for (const auto* cell : referenced_cells) {
        cell->RecursedCheck(this, checked_cells);
    }
}

bool Cell::ChacheIsFull() const {
    return impl_->ChacheIsFull();
}

// РЕАЛИЗАЦИЯ EmptyImpl

CellInterface::Value Cell::EmptyImpl::GetValue() const {
    return FormulaError(FormulaError::Category::Value);
}

std::string Cell::EmptyImpl::GetText() const {
    return std::string();
}

void Cell::EmptyImpl::Clear() {}

// РЕАЛИЗАЦИЯ TextImpl

Cell::TextImpl::TextImpl(std::string text) : text_(std::move(text)) {}

CellInterface::Value Cell::TextImpl::GetValue() const {
    if (text_.find(ESCAPE_SIGN) == 0) {
        return text_.substr(1);
    }
    return text_;
}

std::string Cell::TextImpl::GetText() const {
    return text_;
}

void Cell::TextImpl::Clear() {
    text_.clear();
}

// РЕАЛИЗАЦИЯ FormulaImpl

Cell::FormulaImpl::FormulaImpl(std::string text, const Sheet& sheet)
    : formula_(ParseFormula(std::move(text))), sheet_(sheet) {}


std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    return formula_->GetReferencedCells();
}

CellInterface::Value Cell::FormulaImpl::GetValue() const {
    if (ChacheIsFull()) {
        return GetCacheValue();
    }
    else {
        CellInterface::Value value;
        auto result = formula_->Evaluate(sheet_);
        if (std::holds_alternative<double>(result)) {
            value = std::get<double>(result);
            SetCacheValue(value);
            return value;
        }
        else {
            value = std::get<FormulaError>(result);
            SetCacheValue(value);
            return value;
        }
    }
}


std::string Cell::FormulaImpl::GetText() const {
    return std::string(1, FORMULA_SIGN) + formula_->GetExpression();
}

void Cell::FormulaImpl::Clear() {
    formula_ = ParseFormula("");
}

void Cell::FormulaImpl::ClearCache() {
    cache_.reset();
}

bool Cell::FormulaImpl::ChacheIsFull() const {
    return cache_.has_value();
}

CellInterface::Value Cell::FormulaImpl::GetCacheValue() const {
    return *cache_;
}

void Cell::FormulaImpl::SetCacheValue(Value value) const {
    cache_ = std::move(value);
}
