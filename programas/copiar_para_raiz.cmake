# Copia um executável recém-compilado para a raiz do repositório, ao lado da pasta dados/,
# para que dê para abrir com um duplo clique. Os arquivos copiados ficam fora do Git
# (.gitignore). Se o programa estiver aberto, a cópia falha só com um aviso: a compilação
# não é interrompida.
#
# Uso: cmake -DSRC=<executável> -DDST_DIR=<pasta> -P copiar_para_raiz.cmake

get_filename_component(name "${SRC}" NAME)
file(COPY_FILE "${SRC}" "${DST_DIR}/${name}" RESULT result ONLY_IF_DIFFERENT)
if(result)
    message(WARNING "Nao foi possivel copiar ${name} para ${DST_DIR} (o programa esta aberto?). "
                    "Feche-o e compile de novo para atualizar a copia.")
endif()
