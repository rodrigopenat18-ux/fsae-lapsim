#pragma once

namespace fsae {

/// Correções do modelo em relação à versão original do simulador.
///
/// Todas desligadas = modelo original (resultados idênticos aos da versão em MATLAB).
/// Cada uma pode ser ligada sozinha para medir o efeito no tempo de volta. Detalhes, efeito
/// medido e a numeração em docs/problemas_conhecidos.md (o item 5 de lá não precisa de
/// correção; o mapa do motor é o item 6).
struct ModelOptions {
    /// 1. Inclinação do segmento em radianos também no cálculo da velocidade de curva
    ///    (o original fazia atan(slope/100), tratando o valor como porcentagem).
    bool slope_in_radians = false;

    /// 2. Se o downforce cresce mais rápido que a força centrípeta necessária
    ///    (m − μy·K·R ≤ 0), a curva não tem limite de aderência (o original zerava a
    ///    velocidade). O teste de tombamento vai até a velocidade máxima do carro.
    bool unbounded_corner_grip = false;

    /// 3. A reta termina exatamente no fim do segmento (o último passo é interpolado) e o
    ///    tempo de cada volta é interpolado no ponto em que a distância cruza k × volta.
    bool exact_segment_end = false;

    /// 4. Só reduz a marcha se a rotação na marcha de baixo não passar do rpm de troca dela.
    bool downshift_rev_check = false;

    /// 5. Mapas do motor limitados à faixa medida: torque 0 acima da rotação máxima
    ///    (corte do limitador), torque do primeiro ponto abaixo da mínima; BSFC constante
    ///    fora da faixa.
    bool limit_engine_map = false;

    /// Modelo original (tudo desligado).
    static constexpr ModelOptions original() noexcept { return {}; }

    /// Todas as correções pontuais ligadas.
    static constexpr ModelOptions corrected() noexcept {
        ModelOptions o;
        o.slope_in_radians = true;
        o.unbounded_corner_grip = true;
        o.exact_segment_end = true;
        o.downshift_rev_check = true;
        o.limit_engine_map = true;
        return o;
    }
};

}  // namespace fsae
