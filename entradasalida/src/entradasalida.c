#include "entradasalida.h"

t_list* metadatas;
t_bitarray* bitarray;
char* bloques_mapped;
//void* bitarray_mapped;

int main(int argc, char* argv[])
{
    if(argc != 3){
        printf("Cantidad de parametros incorrecta");
        exit(EXIT_FAILURE);
    }

    nombreInterfaz = strdup(argv[1]);

    /*******************/
    /****** INITS ******/
    /*******************/
    iniciar_logger();
    iniciar_config(argv[2]);
    iniciar_interfaz();
    iniciar_conexiones();

    pthread_join(t_kernel, NULL);
    //pthread_join(t_memoria, NULL);

    /*******************/
    /****** FREEs ******/
    /*******************/
    free_logger();
    free_config();
    free_conexiones();

    return EXIT_SUCCESS;
}

void iniciar_logger()
{
    logger = log_create("./cfg/entradasalida.log", ENTRADASALIDA, 1, LOG_LEVEL_INFO);
    if (logger == NULL) {
        exit(EXIT_FAILURE);
    }
}

void iniciar_config(char* path)
{
    config_io = malloc(sizeof(t_config_io));
    config = config_create(path);
    if (config == NULL) {
        perror("\nNo se pudo encontrar el path del config.\n");
        exit(EXIT_FAILURE);
    }
    config_io->tipo_interfaz = strdup(config_get_string_value(config,"TIPO_INTERFAZ"));
}

void iniciar_conexiones()
{
    // CONEXION MEMORIA
    memoria_fd = crear_conexion(IP_MEMORIA, config_io->puerto_memoria);
    if (memoria_fd == -1) {
        log_error(logger, "\nError en la conexion con MEMORIA.\n");
        exit(EXIT_FAILURE);
    }
    printf("Conexion con MEMORIA establecida!");

    // CONEXION KERNEL
    kernel_fd = crear_conexion(IP_KERNEL,config_io->puerto_kernel);
    if (kernel_fd == -1) {
        log_error(logger, "\nError en la conexion con KERNEL.\n");
        exit(EXIT_FAILURE);
    }
    printf("Conexion con KERNEL establecida!");
    
    pthread_create(&t_kernel, NULL, (void*) procesar_conexion_kernel, NULL);
}


void iniciar_interfaz()
{
    t_interfaz interfaz = tipoInterfazToEnum(config_io->tipo_interfaz);

    switch(interfaz){
        case GENERICA:
            config_io->tiempo_unidad_trabajo = config_get_int_value(config,"TIEMPO_UNIDAD_TRABAJO");
            config_io->ip_kernel = strdup(config_get_string_value(config,"IP_KERNEL"));
            config_io->puerto_kernel = strdup(config_get_string_value(config,"PUERTO_KERNEL"));
            config_io->ip_memoria = strdup(config_get_string_value(config,"IP_MEMORIA"));
            config_io->puerto_memoria = strdup(config_get_string_value(config,"PUERTO_MEMORIA"));
            break;
        case STDIN:
            config_io->ip_kernel = strdup(config_get_string_value(config,"IP_KERNEL"));
            config_io->puerto_kernel = strdup(config_get_string_value(config,"PUERTO_KERNEL"));
            config_io->ip_memoria = strdup(config_get_string_value(config,"IP_MEMORIA"));
            config_io->puerto_memoria = strdup(config_get_string_value(config,"PUERTO_MEMORIA"));
        break;
        case STDOUT:
            config_io->ip_kernel = strdup(config_get_string_value(config,"IP_KERNEL"));
            config_io->puerto_kernel = strdup(config_get_string_value(config,"PUERTO_KERNEL"));
            config_io->ip_memoria = strdup(config_get_string_value(config,"IP_MEMORIA"));
            config_io->puerto_memoria = strdup(config_get_string_value(config,"PUERTO_MEMORIA"));
        break;
        case DIALFS:
            config_io->tiempo_unidad_trabajo = config_get_int_value(config,"TIEMPO_UNIDAD_TRABAJO");
            config_io->ip_kernel = strdup(config_get_string_value(config,"IP_KERNEL"));
            config_io->puerto_kernel = strdup(config_get_string_value(config,"PUERTO_KERNEL"));
            config_io->ip_memoria = strdup(config_get_string_value(config,"IP_MEMORIA"));
            config_io->puerto_memoria = strdup(config_get_string_value(config,"PUERTO_MEMORIA"));
            config_io->path_base_dialfs = strdup(config_get_string_value(config,"PATH_BASE_DIALFS"));
            config_io->block_size = config_get_int_value(config,"BLOCK_SIZE"); 
            config_io->block_count = config_get_int_value(config,"BLOCK_COUNT");
            config_io->retraso_compactacion = config_get_int_value(config,"RETRASO_COMPACTACION");
            metadatas = list_create();
            inicializar_fs();
        break;
        default:

            break;
    }
}



t_interfaz tipoInterfazToEnum(char* tipoInterfaz)
{
    if(!strcmp("GENERICA", tipoInterfaz)) return GENERICA;
    if(!strcmp("STDIN", tipoInterfaz)) return STDIN;
    if(!strcmp("STDOUT", tipoInterfaz)) return STDOUT;
    if(!strcmp("DIALFS", tipoInterfaz)) return DIALFS;
    return -1;
}

void procesar_conexion_kernel()
{
    enviarNombreInterfaz(kernel_fd, nombreInterfaz, config_io->tipo_interfaz);
    //ToDo: logguear operación a realizar con pid del proceso

    while (true) {
        op_code cod_op = recibir_operacion(kernel_fd);
        switch(cod_op){
            case IO_SLEEP:
            {
                t_list* lista_sleep = list_create();
                lista_sleep = recibir_paquete(kernel_fd);
                int pidsleep = *(int*) list_get(lista_sleep,1);
                log_info(logger,"PID: %d - Operacion: IO_GEN_SLEEP", pidsleep);
	            int cantidad_unidad_trabajo = *(int*) list_get(lista_sleep,3);
                usleep(cantidad_unidad_trabajo * config_io->tiempo_unidad_trabajo * 1000);
                enviarFinInstruccion(kernel_fd);
                list_destroy(lista_sleep);
                break;
            }
            case IO_STDIN_READ:
            {
                t_list* list = list_create();
                list = recibir_paquete(kernel_fd);
                //list = recibir_direccionesOValores(kernel_fd); 
                int pidIORead = *(int*) list_get(list, 1);
                log_info(logger,"PID: %d - Operacion: IO_STDIN_READ", pidIORead);
                char* texto = readline("INGRESAR TEXTO A GUARDAR: ");
                uint32_t direccion_fisica;
                uint32_t tamanio_direccion;
                int k1 = 6;
                int cantidad_direcciones = (list_size(list) - 6);
                int j = 0;
                for( int i=0;i < (cantidad_direcciones / 2); i++) {
                    direccion_fisica = *(uint32_t*)list_get(list,k1);
                    k1++;
                    tamanio_direccion = *(uint32_t*)list_get(list,k1);
                    k1++;
                    char* texto_recortado = string_new();
                    texto_recortado = string_substring(texto, j , tamanio_direccion);
                    guardar_en_direcciones(texto_recortado , direccion_fisica, tamanio_direccion, pidIORead , memoria_fd);
                    j += tamanio_direccion;
                }
                enviarFinInstruccion(kernel_fd);
                break;
            }
            case IO_STDOUT_WRITE:
            {
                t_list* listWrite = list_create();
                char* respuesta = string_new();
                uint32_t direccion_fisicaWrite;
                uint32_t tamanio_direccionWrite;
                //listWrite = recibir_direccionesOValores(kernel_fd);

                int k2 = 6;
                listWrite = recibir_paquete(kernel_fd);
                int pidIOWrite = *(int*) list_get(listWrite, 1);
                log_info(logger,"PID: %d - Operacion: IO_STDOUT_WRITE", pidIOWrite);
                int cantidad_direcciones_write = (list_size(listWrite) - 6);
                for (int i = 0; i < (cantidad_direcciones_write / 2); i++) {
                    direccion_fisicaWrite = *(uint32_t*)list_get(listWrite, k2);
                    k2++;
                    tamanio_direccionWrite = *(uint32_t*)list_get(listWrite, k2);
                    k2++;

                    respuesta = enviar_peticion_memoria_direcciones(direccion_fisicaWrite, tamanio_direccionWrite, pidIOWrite, memoria_fd);
                    
                    printf("%s", string_substring(respuesta, 0 , tamanio_direccionWrite));
                }
                printf("\n");
                enviarFinInstruccion(kernel_fd);           
                break;
            }
            case IO_FS_CREATE:
            {
                char* nombre_archivo = string_new();
                t_list* listcreate = recibir_paquete(kernel_fd);
                int pid_fs_create = *(int*)list_get(listcreate, 1);
                nombre_archivo = (char*)list_get(listcreate, 2);
                log_info(logger,"PID: %d - Operacion: IO_FS_CREATE", pid_fs_create);
                log_info(logger,"PID: %d - Crear Archivo: %s ", pid_fs_create, nombre_archivo);
                int index = buscar_espacio();
                if(index != -1){
                    levantar_metadata(nombre_archivo,index);
                }else{
                    log_error(logger,"No hay espacio disponible en el FS.");
                }           
                usleep(config_io->tiempo_unidad_trabajo * 1000);
                list_destroy(listcreate);
                enviarFinInstruccion(kernel_fd);
                break;
            }
            case IO_FS_DELETE:
            {

                char* nombre_archivo_delete = string_new();
                t_list* lista_delete = list_create();
                lista_delete = recibir_paquete(kernel_fd);
                nombre_archivo_delete = (char*)list_get(lista_delete , 2);
                int pid_delete = *(int*)list_get(lista_delete , 1);
                log_info(logger,"PID: %d - Operacion: IO_FS_DELETE", pid_delete);
                log_info(logger,"PID: %d - Eliminar Archivo: %s ", pid_delete, nombre_archivo_delete);
                t_metadata* metadata = buscarArchivo(nombre_archivo_delete);
                if(metadata == NULL){
                    metadata = levantar_metadata_nuevamente(nombre_archivo_delete);
                }
                eliminarArchivo(metadata); 
                usleep(config_io->tiempo_unidad_trabajo * 1000);
                enviarFinInstruccion(kernel_fd);
                list_destroy(lista_delete);
                break;
            }
            case IO_FS_TRUNCATE:
            {
                char* nombre_archivo_truncar = string_new();
                int tamanio_truncar;
                t_list* paquete = recibir_paquete(kernel_fd);
                int pid_truncate = *(int*)list_get(paquete,1);
                nombre_archivo_truncar = (char*)list_get(paquete,2);
                tamanio_truncar = *(int*)list_get(paquete,3);
                log_info(logger,"PID: %d - Operacion: IO_FS_TRUNCATE", pid_truncate);
                log_info(logger,"PID: %d - Truncar Archivo: %s - Tamaño: %d ", pid_truncate, nombre_archivo_truncar, tamanio_truncar);
                truncar(nombre_archivo_truncar,tamanio_truncar, pid_truncate);
                usleep(config_io->tiempo_unidad_trabajo * 1000);
                list_destroy(paquete);
                enviarFinInstruccion(kernel_fd);
                break;
            }
            case IO_FS_WRITE:
            {
                //t_list* paqueteWrite = recibir_io_write(kernel_fd);
                t_list* paqueteWrite = recibir_paquete(kernel_fd);
                int pidWrite = *(int*)list_get(paqueteWrite,1);
                log_info(logger,"PID: %d - Operacion: IO_FS_WRITE", pidWrite);
                char* nombre_archivo_write = (char*)list_get(paqueteWrite,2);
                int cant_dir = *(int*)list_get(paqueteWrite, 3);
                int puntero_inicio_write = *(int*)list_get(paqueteWrite,4);
                int dir_fisica_fs;
                int tam_fs;
                int x = 6;
                int z = 6;
                int tamanio_total = 0;
                for (int i = 0; i < cant_dir; i++) {
                    x++;
                    tamanio_total = *(uint32_t*)list_get(paqueteWrite, x);
                    x++;
                    tamanio_total += tamanio_total;
                }
                log_info(logger,"PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d ", pidWrite, nombre_archivo_write,tamanio_total,puntero_inicio_write);

                for (int i = 0; i < cant_dir; i++) {
                    dir_fisica_fs = *(uint32_t*)list_get(paqueteWrite, z);
                    z++;
                    tam_fs = *(uint32_t*)list_get(paqueteWrite, z);
                    ++z;
                    io_escribir(nombre_archivo_write, puntero_inicio_write, tam_fs, dir_fisica_fs, pidWrite);
                    puntero_inicio_write = puntero_inicio_write + tam_fs;
                }

                usleep(config_io->tiempo_unidad_trabajo * 1000);
                list_destroy(paqueteWrite);
                enviarFinInstruccion(kernel_fd);
                break;
            }
            case IO_FS_READ:
            {
                t_list* paqueteRead = recibir_paquete(kernel_fd);
                int pidread= *(int*)list_get(paqueteRead,1);
                log_info(logger,"PID: %d - Operacion: IO_FS_READ", pidread);
                char* nombre_archivo_read = (char*)list_get(paqueteRead,2);
                int cant_dir_read = *(int*)list_get(paqueteRead,3);
                int puntero_inicio = *(int*)list_get(paqueteRead,4);
                int dir_fisica_fs_read;
                int tam_fs_read;
                int z2 = 6;

                int y = 6;
                int tamanio_total = 0;
                for (int i = 0; i < cant_dir_read; i++) {
                    y++;
                    tamanio_total = *(uint32_t*)list_get(paqueteRead, y);
                    y++;
                    tamanio_total += tamanio_total;
                }
                log_info(logger,"PID: %d - Leer Archivo: %s - Tamaño a Leer: %d - Puntero Archivo: %d ", pidread, nombre_archivo_read,tamanio_total,puntero_inicio);

                for (int i = 0; i < cant_dir_read; i++) {
                    dir_fisica_fs_read = *(uint32_t*)list_get(paqueteRead, z2);
                    z2++;
                    tam_fs_read = *(uint32_t*)list_get(paqueteRead, z2);
                    ++z2;
                    io_leer(nombre_archivo_read, puntero_inicio, tam_fs_read, dir_fisica_fs_read, pidread);
                    puntero_inicio = puntero_inicio + tam_fs_read;
                }

                usleep(config_io->tiempo_unidad_trabajo * 1000);
                list_destroy(paqueteRead);
                enviarFinInstruccion(kernel_fd);
                break;
            }
            default:
                break;
        }
    }
}

void inicializar_fs(){

    levantar_bloques();
    levantar_bitmap();

}

void levantar_bloques(){

    int pathlen = strlen(config_io->path_base_dialfs) + 1;
    char* path = malloc(pathlen);
    strcpy(path,config_io->path_base_dialfs);
    strcat(path,"/bloques.dat");
    FILE* archivo = fopen(path,"a+");

    if(archivo == NULL)
    {
        log_error(logger,"Error al abrir el archivo de bloques.");
        exit(EXIT_FAILURE);
    }
    
    int fd = fileno(archivo);
    int error = ftruncate(fd,config_io->block_size * config_io->block_count);

    if(error == -1)
    {
        log_error(logger,"Error al truncar el archivo de bloques.");
    }

    bloques_mapped = mmap(NULL, config_io->block_size * config_io->block_count, PROT_WRITE, MAP_SHARED, fd, 0);

    fclose(archivo);
    
}

void levantar_bitmap(){

    int pathlen = strlen(config_io->path_base_dialfs) + 1;
    char* path = malloc(pathlen);   
    strcpy(path,config_io->path_base_dialfs);
    strcat(path,"/bitmap.dat");
    FILE* archivo = fopen(path,"a+");

    if(archivo == NULL)
    {
        log_error(logger,"Error al abrir el archivo de bitmap.");
        exit(EXIT_FAILURE);
    }
    
    int fd = fileno(archivo);
    int error = ftruncate(fd,config_io->block_count/8);

    if(error == -1)
    {
        log_error(logger,"Error al truncar el archivo de bitmap.");
    }

    char* mapped = malloc(config_io->block_count/8);


    bitarray = bitarray_create_with_mode(mapped,config_io->block_count/8, LSB_FIRST);

    bitarray->bitarray = mmap(NULL, config_io->block_count/8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(msync(bitarray->bitarray, config_io->block_count, MS_SYNC) == -1){
        log_error(logger,"Error al sincronizar el archivo de bitmap.");
        exit(EXIT_FAILURE);
    }

    fclose(archivo);
} 


int buscar_espacio(){

    int index = 0;
    int aux = -1;
    for(index;bitarray_get_max_bit(bitarray);index++){
        if(!bitarray_test_bit(bitarray,index))
        {
            aux = 1;
            break;
        }
    }
    if(aux == -1)
    {
        return aux;
    }else{return index;}

}

void levantar_metadata(char* nombre_archivo, int bloque_inicial){

    int pathlen = strlen(config_io->path_base_dialfs) + 1;
    char* path = malloc(pathlen + strlen(nombre_archivo) + 6);   
    strcpy(path,config_io->path_base_dialfs);
    strcat(path, "/");
    strcat(path, nombre_archivo);
    //strcat(path, ".txt");
    FILE* archivo = fopen(path,"a+");

    if(archivo == NULL)
    {
        log_error(logger,"Error al abrir el archivo de metadata.");
        exit(EXIT_FAILURE);
    }

    int fd = fileno(archivo);
    int error = ftruncate(fd, config_io->block_size * config_io->block_count);

    if(error == -1)
    {
        log_error(logger,"Error al truncar el archivo de metadata.");
    }

    t_config* config = malloc(sizeof(t_config));
    config = config_create(path);

    if(config_keys_amount(config) > 0){
        bloque_inicial = config_get_int_value(config, "BLOQUE_INICIAL");
    }

    config_set_value(config, "BLOQUE_INICIAL", string_itoa(bloque_inicial));
    config_set_value(config, "TAMANIO_ARCHIVO", "0");  
    config_save_in_file(config, path);

    bitarray_set_bit(bitarray,bloque_inicial);
    msync(bitarray->bitarray, config_io->block_count, MS_SYNC);

    t_metadata* metadata = malloc(sizeof(t_metadata));
    metadata->nombre = string_new();
    strcpy(metadata->nombre,nombre_archivo);
    metadata->config = config;
    list_add(metadatas, metadata);

    fclose(archivo);
}

void compactarBitarray(){

    //Por cada espacio disponible muevo el proximo archivo a ese espacio
    //PROBAR PORQUE NO SE SI ESTÁ BIEN

    int free_block = 0;

    for (int i = 0; i < bitarray_get_max_bit(bitarray); i++){

        if(bitarray_test_bit(bitarray, i)){

            while(free_block < i && bitarray_test_bit(bitarray, free_block)){
                free_block ++;
            }

            if(free_block < i){

                bitarray_set_bit(bitarray, free_block);
                bitarray_clean_bit(bitarray, i);

                moverDatosBloques(i, free_block);

                t_metadata* archivo = buscarArchivoPorBloqueInicial(i);
                if(archivo != NULL){
                    config_set_value(archivo->config ,"BLOQUE_INICIAL", string_itoa(free_block));
                    config_save(archivo->config);
                }
                free_block++;
            }
        }
    }
}




void compactar(t_metadata* archivo, int tamanio_anterior){

    usleep(config_io->retraso_compactacion * 1000);
    int bloque_inicial = config_get_int_value(archivo->config, "BLOQUE_INICIAL");
    int cantidad_bloques_nuevo = config_get_int_value(archivo->config, "TAMANIO_ARCHIVO");
    //Sacar del bitarray el archivo a truncar
    if(tamanio_anterior == 0){
        cleanBitarray(bloque_inicial, bloque_inicial + tamanio_anterior);
    } else {
        cleanBitarray(bloque_inicial, bloque_inicial + tamanio_anterior - 1);
    }

    //Por cada espacio disponible muevo el proximo archivo a ese espacio
    compactarBitarray();
    //Verifico al final del bitarray si hay espacio disponible
    int primer_cero = buscar_espacio();
    int i = primer_cero;
        bool hayEspacio = true;
        for(i ; i < primer_cero + cantidad_bloques_nuevo; i++){
            if(bitarray_test_bit(bitarray, i)){
                hayEspacio = false;
            }
        }
    if(!hayEspacio){
        printf("No hay espacio");
        return NULL;
    }
    //Agrego el archivo a truncar al final del bitarray
    setBitarray(primer_cero, primer_cero + cantidad_bloques_nuevo - 1);
    config_set_value(archivo->config,"BLOQUE_INICIAL", string_itoa(primer_cero));
    config_save(archivo->config);
}

void moverDatosBloques(int bloqueOrigen, int bloqueDestino){

    char* origen = bloques_mapped + (bloqueOrigen * config_io->block_size);
    char* destino = bloques_mapped + (bloqueDestino * config_io->block_size);

    memmove(destino, origen, config_io->block_size);
    memset(origen, 0, config_io->block_size);
    msync(bloques_mapped, config_io->block_size * config_io->block_count, MS_SYNC);
}


t_metadata* buscarArchivoPorBloqueInicial(int bloque){

    int _buscar_archivo_por_bloque(t_metadata* archivo_a_buscar){
        int bloque_inicial = config_get_int_value(archivo_a_buscar->config, "BLOQUE_INICIAL");     
        return bloque == bloque_inicial;
    };

    return (void*)list_find(metadatas, (void*)_buscar_archivo_por_bloque);
}

t_metadata* buscarArchivoPorBloque(int bloque){

    int _buscar_archivo_por_bloque(t_metadata* archivo_a_buscar){
        int bloque_inicial = config_get_int_value(archivo_a_buscar->config, "BLOQUE_INICIAL");
        int tamanio = config_get_int_value(archivo_a_buscar->config, "TAMANIO_ARCHIVO");       
        if(bloque >= bloque_inicial && bloque <= bloque_inicial + tamanio){
            return true;
        }
    };

    return (void*)list_find(metadatas, (void*)_buscar_archivo_por_bloque);
}


t_metadata* buscarArchivo(char* nombre_archivo){

    int _buscar_archivo(t_metadata* archivo_a_buscar){
        return !strcmp(archivo_a_buscar->nombre, nombre_archivo);
    };

    return (void*)list_find(metadatas, (void*)_buscar_archivo);

}

int buscarIndex_Archivo (char* nombre_archivo){

    int index=0;

    void _buscar_archivo(t_metadata* archivo_a_buscar){
        if(!strcmp(archivo_a_buscar->nombre, nombre_archivo)){
            return;
        }
        index++;
    };

    list_iterate(metadatas, _buscar_archivo);
    return index;

}

void eliminarArchivo(t_metadata* metadata){

    t_config* config = metadata->config;
    int bloque_inicial = config_get_int_value(config,"BLOQUE_INICIAL");
    int tamanio = config_get_int_value(config,"TAMANIO_ARCHIVO");
    int bloque_final = bloque_inicial + tamanio - 1;
    //eliminar del bloques.dat y clean de los bits de ocupado
    eliminarEspacio(bloque_inicial, tamanio);
    cleanBitarray(bloque_inicial, bloque_final);
    deleteFile(metadata->nombre);

    //eliminar config y mapped y sacar de la lista
    if(list_remove_element(metadatas, metadata)){
        config_destroy(metadata->config);
        free(metadata->nombre);
        free(metadata);
    }

}

void deleteFile(char* nombre){

    int pathlen = strlen(config_io->path_base_dialfs) + 1;
    char* path = malloc(pathlen + strlen(nombre));   
    strcpy(path,config_io->path_base_dialfs);
    strcat(path, "/");
    strcat(path, nombre);

    if(remove(path) != 0){
        printf("No se pudo borrar el archivo");
    }
}


void eliminarEspacio(int bloque_inicial,int tamanio){

    memset((char*)bloques_mapped + bloque_inicial * config_io->block_size , 0, tamanio * config_io->block_size);
    msync(bloques_mapped, config_io->block_size * config_io->block_count, MS_SYNC);
}

void cleanBitarray(int bloque_inicial,int bloque_final){

    int index = bloque_inicial;
    for(index; index<=bloque_final; index++){
        bitarray_clean_bit(bitarray, index);
    }
    msync(bitarray->bitarray, config_io->block_count, MS_SYNC);
}

void setBitarray(int bloque_inicial,int bloque_final){

    int index = bloque_inicial;
    for(index; index<=bloque_final; index++){
        bitarray_set_bit(bitarray, index);
    }
    msync(bitarray->bitarray, config_io->block_count, MS_SYNC);
}


void truncar(char* nombre_archivo_truncar,int tamanio_truncar, int pid){

    t_metadata* metadata = buscarArchivo(nombre_archivo_truncar);
    if(metadata == NULL){
        metadata = levantar_metadata_nuevamente(nombre_archivo_truncar);
    }
    int bloque_inicial = config_get_int_value(metadata->config, "BLOQUE_INICIAL");
    int tamanio_anterior = config_get_int_value(metadata->config, "TAMANIO_ARCHIVO");
    int cantidad_bloques = ceil((float)tamanio_truncar / config_io->block_size);
    config_set_value(metadata->config,"TAMANIO_ARCHIVO", string_itoa(cantidad_bloques));
    config_save(metadata->config);
    
    if(cantidad_bloques < tamanio_anterior){
        cleanBitarray(bloque_inicial + cantidad_bloques, bloque_inicial + tamanio_anterior - 1);
        eliminarEspacio(bloque_inicial + cantidad_bloques, tamanio_anterior - cantidad_bloques);
    }
    if(cantidad_bloques > tamanio_anterior){
        //verifico si hay espacio disponible
        int i = bloque_inicial + tamanio_anterior;
        bool hayEspacio = true;
        for(i ; i < bloque_inicial + cantidad_bloques; i++){
            if(bitarray_test_bit(bitarray, i)){
                hayEspacio = false;
            }
        }
        //si no hay espacio entonces ejecuto el compactar
        if(!hayEspacio){
            log_info(logger, "PID: %d - Inicio Compactación.", pid);
            compactar(metadata, tamanio_anterior);
            log_info(logger, "PID: %d - Fin Compactación.", pid);
        } else {
            setBitarray(bloque_inicial + tamanio_anterior + 1, bloque_inicial + cantidad_bloques - 1);
        }
    }
}

void io_leer(char* nombre_archivo_leer, int puntero_inicio, int tam_a_leer, int dir_fisica_a_memoria, int pid){

    t_metadata* metadata = buscarArchivo(nombre_archivo_leer);
    if(metadata == NULL){
        metadata = levantar_metadata_nuevamente(nombre_archivo_leer);
    }
    int bloque_inicial = config_get_int_value(metadata->config, "BLOQUE_INICIAL");
    char* datosBloque = string_new();
    int primer_byte =  bloque_inicial * config_io->block_size + puntero_inicio;
    datosBloque = obtenerDatosBloque(primer_byte, primer_byte + tam_a_leer);
    enviar_io_read_a_memoria(datosBloque, dir_fisica_a_memoria , memoria_fd, tam_a_leer, pid);
}

void io_escribir(char* nombre_archivo_escribir, int puntero_inicio, int tam_a_escribir, int dir_fisica_a_memoria, int pid){
   
    t_metadata* metadata = buscarArchivo(nombre_archivo_escribir);
    if(metadata == NULL){
        metadata = levantar_metadata_nuevamente(nombre_archivo_escribir);
    }
    char* datosBloque = string_new();
    datosBloque = enviar_io_write_a_memoria(dir_fisica_a_memoria, tam_a_escribir, memoria_fd, pid);
    int bloque_inicial = config_get_int_value(metadata->config, "BLOQUE_INICIAL");
    int primer_byte =  bloque_inicial * config_io->block_size + puntero_inicio;
    int ultimo_byte = primer_byte + tam_a_escribir;
    escribirDatosBloque(datosBloque, primer_byte, ultimo_byte);
    free(datosBloque);
}


void escribirDatosBloque(char* datosBloque, int inicio, int fin){
    memmove(bloques_mapped + inicio, datosBloque, fin - inicio);
}


char* obtenerDatosBloque(int inicio, int fin){

    char* datosBloque = string_new();
    datosBloque = string_substring(bloques_mapped, inicio, fin - inicio);
    return datosBloque;
}

t_metadata* levantar_metadata_nuevamente(char* nombre_archivo){

    int pathlen = strlen(config_io->path_base_dialfs) + 1;
    char* path = malloc(pathlen + strlen(nombre_archivo) + 6);   
    strcpy(path,config_io->path_base_dialfs);
    strcat(path, "/");
    strcat(path, nombre_archivo);
    FILE* archivo = fopen(path,"a+");

    if(archivo == NULL)
    {
        log_error(logger,"Error al abrir el archivo de metadata.");
        exit(EXIT_FAILURE);
    }

    int fd = fileno(archivo);

    t_config* config = config_create(path);

    t_metadata* metadata = malloc(sizeof(t_metadata));
    metadata->nombre = string_new();
    strcpy(metadata->nombre,nombre_archivo);
    metadata->config = config;
    list_add(metadatas, metadata);

    fclose(archivo);

    return metadata;
}



/********************/
/****** Free's ******/
/********************/
void free_logger() {
    log_destroy(logger);
}

void free_config() {
    free(config_io->tipo_interfaz);
    free(config_io->ip_kernel);
    free(config_io->puerto_kernel);
    free(config_io->ip_memoria);
    free(config_io->puerto_memoria);
    free(config_io->path_base_dialfs);
    free(config_io);
    config_destroy(config);
}

void free_conexiones() {
    liberar_conexion(memoria_fd);
    liberar_conexion(kernel_fd);
}
