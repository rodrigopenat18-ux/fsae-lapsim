function [segments] = trackExcel()
% Esta função lê um arquivo .csv de dados de pista e o formata em uma
% matriz para uso em softwares de simulação como o OptimumLap.
%
% FORMATO DO ARQUIVO .CSV ESPERADO (sem cabeçalho):
% - Delimitador: Ponto e vírgula (;)
% - Separador decimal: Ponto (.)
% - Coluna 1: Tipo de segmento (Left, Right, Straight)
% - Coluna 2: Comprimento (m)
% - Coluna 3: Raio (m)
%
% FORMATO DA MATRIZ DE SAÍDA 'segments':
% Coluna 1: Index (1, 2, 3, ...)
% Coluna 2: Comprimento (m)
% Coluna 3: Raio (m)
% Coluna 4: Slope (sempre 0)
% Coluna 5: Type (0=Straight, 1=Left, 2=Right)

% --- Solicita o nome do arquivo ao usuário ---
prompt = 'Digite o nome do arquivo .csv (ex: dados_pista.csv): ';
filename = input(prompt, 's');

% Garante que a extensão .csv esteja presente
if ~endsWith(filename, '.csv', 'IgnoreCase', true)
    filename = [filename, '.csv'];
end

% --- Leitura e Processamento do Arquivo ---
try
    % Especifica as opções de importação para um arquivo separado por ';'
    % e com os tipos de dados corretos (texto, número, número).
    opts = delimitedTextImportOptions('NumVariables', 3);
    opts.Delimiter = ';';
    opts.VariableNames = {'TypeStr', 'Length', 'Radius'};
    opts.VariableTypes = {'string', 'double', 'double'};
    
    % Usa readtable para ler os dados de forma robusta.
    T = readtable(filename, opts);
    
    numRows = height(T);
    % Pré-aloca a matriz de saída para melhor performance.
    segments = zeros(numRows, 5);

    % Itera sobre cada linha da tabela lida.
    for i = 1:numRows
        % Coluna 1: Index
        segments(i, 1) = i;

        % Coluna 2: Comprimento (lido diretamente da tabela)
        segments(i, 2) = T.Length(i);

        % Coluna 3: Raio (lido diretamente da tabela)
        segments(i, 3) = T.Radius(i);

        % Coluna 4: Slope
        segments(i, 4) = 0;

        % Coluna 5: Tipo de Curva (convertendo texto para número)
        typeStr = lower(T.TypeStr(i)); % Converte para minúsculas para ser mais robusto
        switch typeStr
            case "left"
                segments(i, 5) = 1;
            case "right"
                segments(i, 5) = 2;
            case "straight"
                segments(i, 5) = 0;
            otherwise
                warning('Tipo de segmento não reconhecido na linha %d: "%s". Usando NaN.', i, T.TypeStr(i));
                segments(i, 5) = NaN; % Marca como um valor inválido
        end
    end
    
    fprintf('Arquivo "%s" processado com sucesso. %d segmentos carregados.\n', filename, numRows);

catch ME
    % Captura e exibe um erro se o arquivo não for encontrado ou houver outro problema.
    error('Falha ao ler o arquivo "%s". Verifique o nome, se está na pasta correta e se o formato (separado por ";") corresponde.\nDetalhes do erro: %s', filename, ME.message);
end

end