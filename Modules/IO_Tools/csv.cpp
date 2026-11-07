#include "IO_Tools/csv.hpp"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace fsae::io {

namespace {

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> out;
    std::string field;
    for (char ch : line) {
        if (ch == delimiter) {
            out.push_back(field);
            field.clear();
        } else if (ch != '\r') {
            field.push_back(ch);
        }
    }
    out.push_back(field);
    return out;
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

double parse_double(const std::string& text, const std::filesystem::path& path,
                    std::size_t line_no) {
    const std::string t = trim(text);
    if (t.empty()) {
        throw std::runtime_error(path.string() + ":" + std::to_string(line_no) +
                                 ": campo vazio");
    }
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(t.c_str(), &end);
    if (end != t.c_str() + t.size()) {
        throw std::runtime_error(path.string() + ":" + std::to_string(line_no) +
                                 ": numero invalido '" + t + "'");
    }
    return value;
}

std::ifstream open(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
    return in;
}

}  // namespace

CsvTable::CsvTable(std::vector<std::string> header, std::vector<std::vector<double>> columns)
    : header_(std::move(header)), columns_(std::move(columns)) {
    if (header_.size() != columns_.size()) {
        throw std::invalid_argument("CsvTable: cabecalho e colunas com tamanhos diferentes");
    }
}

std::size_t CsvTable::rows() const noexcept {
    return columns_.empty() ? 0 : columns_.front().size();
}

bool CsvTable::has_column(std::string_view name) const noexcept {
    for (const auto& h : header_) {
        if (h == name) return true;
    }
    return false;
}

std::size_t CsvTable::column_index(std::string_view name) const {
    for (std::size_t i = 0; i < header_.size(); ++i) {
        if (header_[i] == name) return i;
    }
    throw std::out_of_range("Coluna nao encontrada: " + std::string(name));
}

const std::vector<double>& CsvTable::column(std::string_view name) const {
    return columns_[column_index(name)];
}

const std::vector<double>& CsvTable::column(std::size_t index) const {
    return columns_.at(index);
}

CsvTable read_numeric_csv(const std::filesystem::path& path, char delimiter) {
    auto in = open(path);
    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error(path.string() + ": arquivo vazio");

    std::vector<std::string> header;
    for (const auto& h : split(line, delimiter)) header.push_back(trim(h));
    std::vector<std::vector<double>> columns(header.size());

    std::size_t line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (trim(line).empty()) continue;
        const auto fields = split(line, delimiter);
        if (fields.size() != header.size()) {
            throw std::runtime_error(path.string() + ":" + std::to_string(line_no) + ": esperado " +
                                     std::to_string(header.size()) + " campos, encontrado " +
                                     std::to_string(fields.size()));
        }
        for (std::size_t i = 0; i < fields.size(); ++i) {
            columns[i].push_back(parse_double(fields[i], path, line_no));
        }
    }
    return CsvTable(std::move(header), std::move(columns));
}

std::map<std::string, double> read_key_value_csv(const std::filesystem::path& path,
                                                 char delimiter) {
    auto in = open(path);
    std::string line;
    std::getline(in, line);  // cabeçalho
    std::map<std::string, double> out;
    std::size_t line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (trim(line).empty()) continue;
        const auto fields = split(line, delimiter);
        if (fields.size() != 2) {
            throw std::runtime_error(path.string() + ":" + std::to_string(line_no) +
                                     ": esperado key,value");
        }
        out[trim(fields[0])] = parse_double(fields[1], path, line_no);
    }
    return out;
}

}  // namespace fsae::io
