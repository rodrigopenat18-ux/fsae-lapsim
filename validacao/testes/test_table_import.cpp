#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "arquivos/table_import.hpp"
#include "carro/car.hpp"
#include "support/reference.hpp"

using fsae::io::parse_delimited_table;
using fsae::test::reference_path;

TEST_CASE("CSV com virgula e cabecalho") {
    const auto t = parse_delimited_table("rpm,torque\n1000,40.5\n2000,45\n");
    REQUIRE(t.cols() == 2);
    REQUIRE(t.rows() == 2);
    CHECK(t.columns[0] == "rpm");
    CHECK(t.columns[1] == "torque");
    CHECK(t.values[1][0] == 40.5);
}

TEST_CASE("CSV brasileiro: ponto e virgula e virgula decimal") {
    const auto t = parse_delimited_table("Rotação;Torque (N.m)\r\n4499;43,8\r\n5000;45,25\r\n1.234,5;10\r\n");
    REQUIRE(t.rows() == 3);
    CHECK(t.columns[0] == "Rotação");
    CHECK(t.values[1][0] == 43.8);
    CHECK(t.values[1][1] == 45.25);
    CHECK(t.values[0][2] == 1234.5);
}

TEST_CASE("texto colado do Excel (tabulacao), linhas de titulo e vazias") {
    const auto t = parse_delimited_table(
        "Dados do motor\t\t\n\nRPM\tTorque\tObs\n4499\t43,8\tmedido\n\n5000\t45\tmedido\n");
    REQUIRE(t.cols() == 3);
    REQUIRE(t.rows() == 2);
    CHECK(t.columns[1] == "Torque");
    CHECK(t.values[0][1] == 5000.0);
    CHECK(std::isnan(t.values[2][0]));  // coluna de texto
    CHECK(t.first_data_row == 3);
}

TEST_CASE("sem cabecalho: colunas A, B...") {
    const auto t = parse_delimited_table("1 2\n3 4\n");
    CHECK(t.columns[0] == "Coluna A");
    CHECK(t.columns[1] == "Coluna B");
    CHECK(t.values[1][1] == 4.0);
}

TEST_CASE("erros de tabela") {
    CHECK_THROWS_WITH((void)parse_delimited_table("so texto\naqui tambem\n"), doctest::Contains("nenhuma linha"));
    CHECK_THROWS_WITH((void)parse_delimited_table(""), doctest::Contains("vazia"));
}

TEST_CASE("Excel .xlsx escrito pelo MATLAB (import/engine.xlsx)") {
    const auto path = reference_path("import/engine.xlsx");
    const auto sheets = fsae::io::xlsx_sheet_names(path);
    REQUIRE(sheets.size() >= 2);
    CHECK(sheets[0] == "Torque");
    CHECK(sheets[1] == "Marchas");

    const auto spec = fsae::CarSpec::matlab_default();
    const auto t = fsae::io::read_xlsx_table(path, 0);
    REQUIRE(t.cols() == 3);
    CHECK(t.columns[0] == "RPM");
    CHECK(t.columns[1] == "Torque [N.m]");
    CHECK(t.columns[2] == "Observação");
    REQUIRE(t.rows() == spec.engine_rpm.size());
    for (std::size_t i = 0; i < t.rows(); ++i) {
        CHECK(t.values[0][i] == spec.engine_rpm[i]);
        CHECK(t.values[1][i] == spec.engine_torque_nm[i]);
    }

    const auto g = fsae::io::read_table_file(path, 1);
    REQUIRE(g.rows() == spec.gear_ratios.size());
    CHECK(g.columns[1] == "Relação");
    for (std::size_t i = 0; i < g.rows(); ++i) {
        CHECK(g.values[1][i] == spec.gear_ratios[i]);
        CHECK(g.values[2][i] == spec.upshift_rpm[i]);
    }
    CHECK_THROWS((void)fsae::io::read_xlsx_table(path, 5));
    CHECK_THROWS_WITH((void)fsae::io::read_table_file("x.xls"), doctest::Contains(".xls antigo"));
}
