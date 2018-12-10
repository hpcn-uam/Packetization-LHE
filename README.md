(old)

Si el fichero no tiene en la cabecera LHE el campo 1st block ID, hay dos opciones:

1. Compilar el programa con NOT_FIRST_BLOCK_ID definido a 1, por ejemplo con

    make CEXTRAFLAGS=-DNOT_FIRST_BLOCK_ID

2. Ejecutar el programa add_first_block_id 

    # compilar el programa
    make add_first_block_id.out
    # ejecutarlo
    ./add_first_block_id.out original original_fixed
