#pragma once

#include <filesystem>
#include <string>

#include "fsae/vehicle/car.hpp"

namespace fsae::io {

/// Lê a especificação do carro de um arquivo JSON (formato em data/cars/default.json).
/// É rigoroso: campo faltando, campo desconhecido (ex.: erro de digitação) ou tipo
/// errado geram std::runtime_error com o caminho do campo.
CarSpec load_car_spec(const std::filesystem::path& path);

/// Igual a load_car_spec, a partir do texto JSON.
CarSpec parse_car_spec(const std::string& json_text, const std::string& source = "<texto>");

/// Lê e converte para unidades SI (Car::from_spec).
Car load_car(const std::filesystem::path& path);

/// Serializa a especificação em JSON formatado.
std::string car_spec_to_json(const CarSpec& spec);

/// Grava a especificação num arquivo JSON.
void save_car_spec(const CarSpec& spec, const std::filesystem::path& path);

}  // namespace fsae::io
