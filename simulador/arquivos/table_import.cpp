#include "arquivos/table_import.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

#include "miniz.h"

namespace fsae::io {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n\"");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n\"");
    return s.substr(b, e - b + 1);
}

/// Número no formato com ponto ou (se `comma_decimal`) vírgula decimal. NaN se não for.
double to_number(std::string s, bool comma_decimal) {
    s = trim(s);
    if (s.empty()) return kNaN;
    if (comma_decimal && s.find(',') != std::string::npos) {
        if (s.find('.') != std::string::npos) {
            // "1.234,5": ponto como separador de milhar
            s.erase(std::remove(s.begin(), s.end(), '.'), s.end());
        }
        std::replace(s.begin(), s.end(), ',', '.');
    }
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || errno == ERANGE) return kNaN;
    return v;
}

std::string column_letter(std::size_t c) {
    std::string s;
    ++c;
    while (c > 0) {
        s.insert(s.begin(), static_cast<char>('A' + (c - 1) % 26));
        c = (c - 1) / 26;
    }
    return s;
}

DataTable build(const std::vector<std::vector<std::string>>& cells, bool comma_decimal) {
    std::size_t ncols = 0;
    for (const auto& r : cells) ncols = std::max(ncols, r.size());
    if (ncols == 0) throw std::runtime_error("tabela vazia");

    auto numeric_count = [&](const std::vector<std::string>& r) {
        std::size_t n = 0;
        for (const auto& c : r) n += std::isnan(to_number(c, comma_decimal)) ? 0 : 1;
        return n;
    };
    const std::size_t needed = ncols == 1 ? 1 : 2;
    std::size_t start = cells.size();
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (numeric_count(cells[i]) >= needed) {
            start = i;
            break;
        }
    }
    if (start == cells.size()) throw std::runtime_error("nenhuma linha com numeros encontrada");

    DataTable t;
    t.first_data_row = start;
    t.columns.resize(ncols);
    const std::vector<std::string>* header = nullptr;
    if (start > 0 && numeric_count(cells[start - 1]) < cells[start - 1].size()) header = &cells[start - 1];
    for (std::size_t c = 0; c < ncols; ++c) {
        std::string name = header && c < header->size() ? trim((*header)[c]) : std::string();
        t.columns[c] = name.empty() ? "Coluna " + column_letter(c) : name;
    }
    t.values.assign(ncols, {});
    for (std::size_t i = start; i < cells.size(); ++i) {
        const auto& r = cells[i];
        bool empty = true;
        for (const auto& c : r) empty = empty && trim(c).empty();
        if (empty) continue;
        for (std::size_t c = 0; c < ncols; ++c) {
            t.values[c].push_back(c < r.size() ? to_number(r[c], comma_decimal) : kNaN);
        }
    }
    return t;
}

// ---------------------------------------------------------------------------
// XLSX (zip com XML)
// ---------------------------------------------------------------------------

class Zip {
public:
    explicit Zip(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
        std::ostringstream ss;
        ss << in.rdbuf();
        data_ = ss.str();
        std::memset(&zip_, 0, sizeof zip_);
        if (!mz_zip_reader_init_mem(&zip_, data_.data(), data_.size(), 0)) {
            throw std::runtime_error(path.filename().string() + " nao e um arquivo .xlsx valido");
        }
    }
    ~Zip() { mz_zip_reader_end(&zip_); }
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;

    /// Conteúdo de um arquivo interno; vazio se não existir.
    std::string read(const std::string& name) {
        std::size_t size = 0;
        void* p = mz_zip_reader_extract_file_to_heap(&zip_, name.c_str(), &size, 0);
        if (!p) return {};
        std::string s(static_cast<const char*>(p), size);
        mz_free(p);
        return s;
    }

private:
    std::string data_;
    mz_zip_archive zip_;
};

std::string xml_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const auto semi = s.find(';', i);
        if (semi == std::string::npos) {
            out += s[i];
            continue;
        }
        const std::string ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            const unsigned long cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')
                                         ? std::strtoul(ent.c_str() + 2, nullptr, 16)
                                         : std::strtoul(ent.c_str() + 1, nullptr, 10);
            // UTF-8
            if (cp < 0x80) out += static_cast<char>(cp);
            else if (cp < 0x800) {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        } else {
            out += s.substr(i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

/// Valor do atributo `name="..."` na tag (texto entre '<' e '>').
std::string attr(const std::string& tag, const std::string& name) {
    std::size_t pos = 0;
    while ((pos = tag.find(name + "=", pos)) != std::string::npos) {
        if (pos == 0 || tag[pos - 1] == ' ' || tag[pos - 1] == '\t' || tag[pos - 1] == '\n' ||
            tag[pos - 1] == '\r' || tag[pos - 1] == ':') {
            const std::size_t q = pos + name.size() + 1;
            if (q < tag.size() && (tag[q] == '"' || tag[q] == '\'')) {
                const auto e = tag.find(tag[q], q + 1);
                if (e != std::string::npos) return xml_decode(tag.substr(q + 1, e - q - 1));
            }
        }
        pos += name.size();
    }
    return {};
}

/// Texto de todos os <t>...</t> dentro de `xml` (concatenados).
std::string all_text(const std::string& xml) {
    std::string out;
    std::size_t pos = 0;
    while ((pos = xml.find("<t", pos)) != std::string::npos) {
        const char next = pos + 2 < xml.size() ? xml[pos + 2] : '\0';
        if (next != '>' && next != ' ') {
            pos += 2;
            continue;
        }
        const auto gt = xml.find('>', pos);
        if (gt == std::string::npos) break;
        if (xml[gt - 1] == '/') {
            pos = gt;
            continue;
        }
        const auto end = xml.find("</t>", gt);
        if (end == std::string::npos) break;
        out += xml_decode(xml.substr(gt + 1, end - gt - 1));
        pos = end + 4;
    }
    return out;
}

std::vector<std::string> shared_strings(Zip& z) {
    std::vector<std::string> out;
    const std::string xml = z.read("xl/sharedStrings.xml");
    std::size_t pos = 0;
    while ((pos = xml.find("<si", pos)) != std::string::npos) {
        const auto end = xml.find("</si>", pos);
        if (end == std::string::npos) break;
        out.push_back(all_text(xml.substr(pos, end - pos)));
        pos = end + 5;
    }
    return out;
}

struct SheetRef {
    std::string name;
    std::string path;  ///< dentro do zip, ex.: xl/worksheets/sheet1.xml
};

std::vector<SheetRef> sheet_refs(Zip& z) {
    const std::string wb = z.read("xl/workbook.xml");
    if (wb.empty()) throw std::runtime_error("arquivo .xlsx sem xl/workbook.xml");
    const std::string rels = z.read("xl/_rels/workbook.xml.rels");
    std::map<std::string, std::string> target;
    std::size_t pos = 0;
    while ((pos = rels.find("<Relationship", pos)) != std::string::npos) {
        const auto gt = rels.find('>', pos);
        const std::string tag = rels.substr(pos, gt - pos);
        target[attr(tag, "Id")] = attr(tag, "Target");
        pos = gt;
    }
    std::vector<SheetRef> out;
    pos = 0;
    while ((pos = wb.find("<sheet ", pos)) != std::string::npos) {
        const auto gt = wb.find('>', pos);
        const std::string tag = wb.substr(pos, gt - pos);
        SheetRef s;
        s.name = attr(tag, "name");
        std::string t = target[attr(tag, "id")];  // r:id
        if (t.empty()) t = "worksheets/sheet" + std::to_string(out.size() + 1) + ".xml";
        s.path = !t.empty() && t[0] == '/' ? t.substr(1) : "xl/" + t;
        out.push_back(s);
        pos = gt;
    }
    if (out.empty()) throw std::runtime_error("arquivo .xlsx sem planilhas");
    return out;
}

/// Coluna (base 0) da referência "AB12".
std::size_t ref_column(const std::string& ref) {
    std::size_t c = 0;
    for (char ch : ref) {
        if (ch < 'A' || ch > 'Z') break;
        c = c * 26 + static_cast<std::size_t>(ch - 'A' + 1);
    }
    return c == 0 ? 0 : c - 1;
}

}  // namespace

DataTable table_from_cells(const std::vector<std::vector<std::string>>& cells) {
    return build(cells, false);
}

DataTable parse_delimited_table(const std::string& text) {
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string l;
        while (std::getline(in, l)) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            lines.push_back(l);
        }
    }
    // BOM UTF-8
    if (!lines.empty() && lines[0].rfind("\xEF\xBB\xBF", 0) == 0) lines[0] = lines[0].substr(3);

    std::size_t tabs = 0, semis = 0, commas = 0;
    for (std::size_t i = 0; i < std::min<std::size_t>(lines.size(), 50); ++i) {
        tabs += static_cast<std::size_t>(std::count(lines[i].begin(), lines[i].end(), '\t'));
        semis += static_cast<std::size_t>(std::count(lines[i].begin(), lines[i].end(), ';'));
        commas += static_cast<std::size_t>(std::count(lines[i].begin(), lines[i].end(), ','));
    }
    char delim = ',';
    if (tabs > 0 && tabs >= semis) delim = '\t';
    else if (semis > 0) delim = ';';
    else if (commas == 0) delim = ' ';
    const bool comma_decimal = delim != ',';

    std::vector<std::vector<std::string>> cells;
    for (const auto& l : lines) {
        std::vector<std::string> row;
        if (delim == ' ') {
            std::istringstream ss(l);
            std::string f;
            while (ss >> f) row.push_back(f);
        } else {
            std::string f;
            for (char ch : l) {
                if (ch == delim) {
                    row.push_back(f);
                    f.clear();
                } else {
                    f += ch;
                }
            }
            row.push_back(f);
        }
        cells.push_back(row);
    }
    return build(cells, comma_decimal);
}

std::vector<std::string> xlsx_sheet_names(const std::filesystem::path& path) {
    Zip z(path);
    std::vector<std::string> names;
    for (const auto& s : sheet_refs(z)) names.push_back(s.name);
    return names;
}

DataTable read_xlsx_table(const std::filesystem::path& path, std::size_t sheet) {
    Zip z(path);
    const auto sheets = sheet_refs(z);
    if (sheet >= sheets.size()) throw std::runtime_error("planilha inexistente no arquivo");
    const std::string xml = z.read(sheets[sheet].path);
    if (xml.empty()) throw std::runtime_error("planilha \"" + sheets[sheet].name + "\" vazia ou ausente");
    const auto strings = shared_strings(z);

    std::vector<std::vector<std::string>> cells;
    std::size_t pos = 0;
    std::size_t implicit_row = 0;
    while ((pos = xml.find("<row", pos)) != std::string::npos) {
        const auto row_gt = xml.find('>', pos);
        if (row_gt == std::string::npos) break;
        const std::string row_tag = xml.substr(pos, row_gt - pos);
        const std::string rattr = attr(row_tag, "r");
        const std::size_t r = rattr.empty() ? implicit_row : static_cast<std::size_t>(std::strtoul(rattr.c_str(), nullptr, 10) - 1);
        implicit_row = r + 1;
        const bool row_empty = xml[row_gt - 1] == '/';
        const auto row_end = row_empty ? row_gt : xml.find("</row>", row_gt);
        if (row_end == std::string::npos) break;
        if (cells.size() <= r) cells.resize(r + 1);
        auto& row = cells[r];

        std::size_t cpos = row_gt;
        std::size_t implicit_col = 0;
        while ((cpos = xml.find("<c", cpos)) != std::string::npos && cpos < row_end) {
            const char next = xml[cpos + 2];
            if (next != ' ' && next != '>' && next != '/') {
                cpos += 2;
                continue;
            }
            const auto cgt = xml.find('>', cpos);
            const std::string ctag = xml.substr(cpos, cgt - cpos);
            const std::string ref = attr(ctag, "r");
            const std::size_t c = ref.empty() ? implicit_col : ref_column(ref);
            implicit_col = c + 1;
            std::string value;
            if (xml[cgt - 1] != '/') {
                const auto cend = xml.find("</c>", cgt);
                const std::string inner = xml.substr(cgt + 1, cend - cgt - 1);
                const std::string type = attr(ctag, "t");
                if (type == "inlineStr") {
                    value = all_text(inner);
                } else {
                    const auto v0 = inner.find("<v>");
                    const auto v1 = inner.find("</v>");
                    if (v0 != std::string::npos && v1 != std::string::npos) {
                        value = xml_decode(inner.substr(v0 + 3, v1 - v0 - 3));
                    }
                    if (type == "s") {
                        const auto idx = static_cast<std::size_t>(std::strtoul(value.c_str(), nullptr, 10));
                        value = idx < strings.size() ? strings[idx] : std::string();
                    }
                }
                cpos = cend;
            } else {
                cpos = cgt;
            }
            if (row.size() <= c) row.resize(c + 1);
            row[c] = value;
        }
        pos = row_end;
    }
    return build(cells, false);
}

DataTable read_table_file(const std::filesystem::path& path, std::size_t sheet) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".xlsx" || ext == ".xlsm") return read_xlsx_table(path, sheet);
    if (ext == ".xls") {
        throw std::runtime_error("formato .xls antigo nao suportado: salve como .xlsx ou .csv no Excel");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_delimited_table(ss.str());
}

}  // namespace fsae::io
