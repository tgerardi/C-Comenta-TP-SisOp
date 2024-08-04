#include <kernel.h>

pthread_t t_quantum;
char* g_nombre_config;

int main(int argc, char* argv[])
{
    pthread_t t_planificador_corto;
    g_nombre_config = argv[1];

    /*******************/
    /****** INITS ******/
    /*******************/

    iniciar_logger();
    iniciar_config();
    iniciar_conexiones();
    inicializar_colas();
    inicializar_mutex();
    inicializar_semaforos();

    /***************************/
    /****** PLANIFICACION ******/
    /***************************/

    iniciar_planificador_largo_plazo();

    pthread_create(&t_planificador_corto, NULL, (void*) iniciar_planificador_corto_plazo, NULL);
    pthread_detach(t_planificador_corto);

    /***************************/
    /********* CONSOLA *********/
    /***************************/

    iniciar_consola();

    /*******************/
    /****** FREEs ******/
    /*******************/

    free_logger();
    free_config();
    free_conexiones();

    return 0;
}

/*******************/
/****** INITS ******/
/*******************/

void iniciar_logger()
{
    logger = log_create("./cfg/kernel.log", KERNEL, true, LOG_LEVEL_INFO);
    if (logger == NULL) {
        exit(EXIT_FAILURE);
    }
}

void iniciar_config()
{
    config_kernel = malloc(sizeof(t_config_kernel));

    config = config_create(string_from_format("%s%s%s", "./cfg/", g_nombre_config, ".config"));
    if (config == NULL) {
        perror("\nNo se pudo encontrar el path del config\n");
        exit(EXIT_FAILURE);
    }

    config_kernel->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
    config_kernel->ip_memoria = strdup(config_get_string_value(config, "IP_MEMORIA"));
    config_kernel->puerto_memoria = strdup(config_get_string_value(config, "PUERTO_MEMORIA"));
    config_kernel->ip_cpu = strdup(config_get_string_value(config, "IP_CPU"));
    config_kernel->puerto_cpu_dispatch = strdup(config_get_string_value(config, "PUERTO_CPU_DISPATCH"));
    config_kernel->puerto_cpu_interrupt = strdup(config_get_string_value(config, "PUERTO_CPU_INTERRUPT"));
    config_kernel->algoritmo_planificacion = strdup(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    config_kernel->quantum = config_get_int_value(config, "QUANTUM");
    config_kernel->recursos = config_get_array_value(config, "RECURSOS");
    config_kernel->instancias_recursos = string_to_int_array(config_get_array_value(config, "INSTANCIAS_RECURSOS"));
    config_kernel->grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
}

void iniciar_conexiones()
{
    pthread_t t_socket_io;
    pthread_t t_cpu_dispatch;
    //pthread_t t_cpu_interrupt;
    
    
    // CONEXION ENTRADA-SALIDA
    socket_io = iniciar_servidor(IP_KERNEL, config_kernel->puerto_escucha);
    if (socket_io == -1) {
        log_error(logger, "Error al recibir INTERFAZ E/S\n");
        exit(EXIT_FAILURE);
    }
    pthread_create(&t_socket_io, NULL, (void*) escuchar_io, NULL);
    pthread_detach(t_socket_io);

    // CONEXION CPU DISPATCH
    cpu_dispatch_fd = crear_conexion(IP_CPU, config_kernel->puerto_cpu_dispatch);
    if (cpu_dispatch_fd == -1) {
        log_error(logger, "Error en la conexion con CPU DISPATCH.");
        exit(EXIT_FAILURE);
    }
    pthread_create(&t_cpu_dispatch, NULL, (void*) procesar_conexion_cpu_dispatch, NULL);
    pthread_detach(t_cpu_dispatch);
    printf("Conexion con CPU DISPATCH establecida!");

    // CONEXION CPU INTERRUPT
    cpu_interrupt_fd = crear_conexion(IP_CPU, config_kernel->puerto_cpu_interrupt);
    if (cpu_interrupt_fd == -1) {
        log_error(logger, "Error en la conexion con CPU INTERRUPT.");
        exit(EXIT_FAILURE);
    }
    //pthread_create(&t_cpu_interrupt, NULL, (void*) procesar_conexion_cpu_interrupt, NULL);
    //pthread_detach(t_cpu_interrupt);
    printf("Conexion con CPU INTERRUPT establecida!");

    // CONEXION MEMORIA
    memoria_fd = crear_conexion(IP_MEMORIA, config_kernel->puerto_memoria);
    if (memoria_fd == -1) {
        log_error(logger, "Error en la conexion con MEMORIA.\n");
        exit(EXIT_FAILURE);
    }
    //pthread_create(&t_memoria, NULL, (void*) procesar_conexion_memoria, NULL);
    printf("Conexion con MEMORIA establecida!");

    
}

void inicializar_colas()
{
    cola_ready = list_create();
    cola_exit = list_create();
    cola_new = list_create();
    cola_exec = list_create();
    interfaces = list_create();
    cola_ready_prioridad = list_create();
    cola_block = list_create();
    lista_recursos = inicializar_recursos();
    pcbs_totales = list_create();
    flag_sleep = false;
}

t_list* inicializar_recursos()
{
    t_list* lista = list_create();
	int cantidad_recursos = string_array_size(config_kernel->recursos);
	for(int i = 0; i < cantidad_recursos; i++){
		char* str = (config_kernel->recursos)[i];
		t_recurso* recurso = malloc(sizeof(t_recurso));
		recurso->nombre = malloc(sizeof(char) * strlen(str) + 1);
		strcpy(recurso->nombre, str);
		t_list* cola_block1 = list_create();
        t_list* cola_uso = list_create();
		recurso->id = i;
		recurso->instancias = (config_kernel->instancias_recursos)[i];
		recurso->cola_block_asignada = cola_block1;
        recurso->cola_en_uso = cola_uso;
		pthread_mutex_init(&recurso->mx_asignado, NULL);
		list_add(lista, recurso);
	}
	return lista;
}

void inicializar_mutex()
{
    pthread_mutex_init(&mx_cola_ready, NULL);
    pthread_mutex_init(&mx_cola_new, NULL);
    pthread_mutex_init(&mx_cola_exit, NULL);
    pthread_mutex_init(&mx_cola_exec, NULL);
    pthread_mutex_init(&mx_cola_block, NULL);
    pthread_mutex_init(&mx_cola_ready_prioridad, NULL);
    pthread_mutex_init(&mx_sleep, NULL);
}

void inicializar_semaforos()
{
    sem_init(&sem_gr_multi_prog, 0, config_kernel->grado_multiprogramacion);
    sem_init(&sem_planificador_largo, 0, 1);
    sem_init(&sem_planificador_corto, 0, 1);
    sem_init(&sem_new, 0, 0);
    sem_init(&sem_ready, 0, 0);
    sem_init(&sem_exec, 0, 1);
    sem_init(&sem_exit, 0, 0);
    //sem_init(&sem_block, 0, 0);
    sem_init(&sem_quantum, 0, 0);
    //sem_init(&sem_block_recurso, 0, 0);
}

t_pcb* inicializar_pcb()
{
    static int pid = 0;
    t_pcb* pcb = malloc(sizeof(t_pcb));
    
    pcb->quantum = config_kernel->quantum;
    pcb->estado_actual = NEW;
    pcb->estado_anterior = NEW;

    t_contexto_ejecucion* contexto_ejecucion = malloc(sizeof(t_contexto_ejecucion));
    contexto_ejecucion->pid = pid++;
    contexto_ejecucion->program_counter = 0;
    contexto_ejecucion->AX = 0;
    contexto_ejecucion->BX = 0;
    contexto_ejecucion->CX = 0;
    contexto_ejecucion->DX = 0;
    contexto_ejecucion->EAX = 0;
    contexto_ejecucion->EBX = 0;
    contexto_ejecucion->ECX = 0;
    contexto_ejecucion->EDX = 0;
    contexto_ejecucion->SI = 0;
    contexto_ejecucion->DI = 0;

    pcb->contexto_ejecucion = contexto_ejecucion;

    t_instruccion_io* prox_io = malloc(sizeof(t_instruccion_io));
    prox_io->param1 = string_new();
    prox_io->param2 = -1;
    prox_io->param3 = -1;
    prox_io->param4 = -1;
    prox_io->lista_direcciones = list_create();

    pcb->proxima_io = prox_io;
    
    list_add(pcbs_totales, pcb);

    return pcb;
}

void iniciar_consola()
{
    char* inp = string_new();
    while (1) {
        PRINT_COMMANDS();
        inp = readline("INGRESE FUNCION: ");
        ejecutar_consola(inp);
    }
}

void ejecutar_consola(char* inp)
{
    char* nombre_funcion = string_new();
    char* parametro = string_new();
    char** lista_inp = string_split(inp," ");
    nombre_funcion = lista_inp[0];
    parametro = lista_inp[1];
    t_funcion funcion = inp_to_funcion(nombre_funcion);

    t_list* peticion_memoria = list_create();
    switch(funcion) {
        case EJECUTAR_SCRIPT:
            if (invalid_parameter(parametro)) break;
            ejecutar_script(parametro);
            break;
        case INICIAR_PROCESO:
            if (invalid_parameter(parametro)) break;
            t_pcb* pcb = inicializar_pcb();

            list_add(peticion_memoria, pcb->contexto_ejecucion->pid);
            list_add(peticion_memoria, parametro);
            enviar_peticion_memoria(peticion_memoria, memoria_fd);

            log_info(logger, "Se crea el proceso %d en NEW", pcb->contexto_ejecucion->pid);
            push_pcb(cola_new, pcb, &mx_cola_new);
            sem_post(&sem_new);
            break;
        case FINALIZAR_PROCESO:
            if (invalid_parameter(parametro)) break;
            int pid = atoi(parametro);
            t_pcb* pcb_a_finalizar;
            pcb_a_finalizar = buscar_pcb_por_id(pid);
            switch(pcb_a_finalizar->estado_actual){
                case EXEC:
                    enviar_interrupcion(cpu_interrupt_fd, INTERRUPTED_BY_USER);
                    sem_post(&sem_exec);
                    break;
                case READY:
                    pcb_to_exit(remover_pcb(cola_ready, &mx_cola_ready), INTERRUPTED_BY_USER);
                    break;
                case NEW:
                    pcb_to_exit(remover_pcb(cola_new, &mx_cola_new), INTERRUPTED_BY_USER);
                    break;
                case BLOCK:
                    int index = -1;
                    t_recurso* recurso = buscar_pcb_en_recurso(pid);
                    if (recurso && list_size(recurso->cola_block_asignada) > 0) {
                        index = buscar_indice_en_cola(pid, recurso->cola_block_asignada);

                        t_recurso* recurso_en_uso = buscar_pcb_en_recurso_en_uso(pid); 
                        int index2 = buscar_indice_en_cola(pid, recurso_en_uso->cola_en_uso);
                        remover_pcb_cola_block(recurso_en_uso->cola_en_uso, &recurso->mx_asignado, index2);

                        pcb_to_exit(remover_pcb_cola_block(recurso->cola_block_asignada, &(recurso->mx_asignado), index), INTERRUPTED_BY_USER);
                        recurso->instancias++;
                        handle_signal(NULL, recurso_en_uso->nombre);
                    } else {
                        pcb_to_exit(remover_pcb_interfaz(pid), INTERRUPTED_BY_USER);
                    }
                    break;
                case EXIT:
                    break;
            }
            break;
        case DETENER_PLANIFICACION:
            sem_wait(&sem_planificador_largo);
            sem_wait(&sem_planificador_corto);
            log_warning(logger, "Planificacion detenida...");
            break;
        case INICIAR_PLANIFICACION:
            sem_post(&sem_planificador_largo);
            sem_post(&sem_planificador_corto);
            log_warning(logger, "Planificacion iniciada...");
            break;
        case MULTIPROGRAMACION:
            if (invalid_parameter(parametro)) break;
            int new_multiprog = atoi(parametro);
            int old_multiprog = config_kernel->grado_multiprogramacion;
            if(new_multiprog >= old_multiprog){
                for(int j = 0; j < (new_multiprog - old_multiprog); j++ ){
                    sem_post(&sem_gr_multi_prog);
                }
            } else {
                for(int j = 0; j < (old_multiprog - new_multiprog); j++ ){
                    sem_wait(&sem_gr_multi_prog);
                }
            }
            //sem_destroy(&sem_gr_multi_prog);
            //sem_init(&sem_gr_multi_prog, 0, new_multiprog);
            log_warning(logger, "Nuevo grado %d", new_multiprog);
            break;
        case PROCESO_ESTADO:
            t_list* list_pids_new = get_pids(cola_new);
            char* pids_new = get_string_elements(list_pids_new);
            log_info(logger, "Estado: NEW - Procesos: [ %s ]", pids_new);

            t_list* list_pids_ready = get_pids(cola_ready);
            char* pids_ready = get_string_elements(list_pids_ready);
            log_info(logger, "Estado: READY - Procesos: [ %s ]", pids_ready);

            t_list* list_pids_exec = get_pids(cola_exec);
            char* pids_exec = get_string_elements(list_pids_exec);
            log_info(logger, "Estado: EXEC - Procesos: [ %s ]", pids_exec);

            t_list* list_pids_exit = get_pids(cola_exit);
            char* pids_exit = get_string_elements(list_pids_exit);
            log_info(logger, "Estado: EXIT - Procesos: [ %s ]", pids_exit);

            for(int g=0; g<list_size(cola_block); g++){
                
                t_interfaz* interfaz_block = list_get(cola_block, g);
                t_list* list_pids_block = get_pids(interfaz_block->pcbs);
                char* pids_block = get_string_elements(list_pids_block);
                log_info(logger, "Interfaz: %s ", interfaz_block->nombreInterfaz);
                log_info(logger, "Estado: BLOCK - Procesos: [ %s ]", pids_block);
                list_destroy(list_pids_block);
                free(pids_block);
            }

            list_destroy(list_pids_new);
            list_destroy(list_pids_ready);
            list_destroy(list_pids_exec);
            list_destroy(list_pids_exit);
            free(pids_new);
            free(pids_ready);
            free(pids_exit);
            free(pids_exec);
            break;
        case -1:
            printf("Funcion invalida, ingrese de nuevo\n");
            break;
        default:
            break;
    }
    list_destroy(peticion_memoria);
    //free(parametro);
    //free(nombre_funcion);
    if (!string_is_empty(inp)) free(inp);
    string_array_destroy(lista_inp);
}


void ejecutar_script(char* path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

	char *linea = NULL;
	while (getline(&linea, &(size_t) {0}, file) > 0) {
		linea[strcspn(linea, "\n")] = '\0';
        ejecutar_consola(linea);
		//linea = NULL;
	}

	free(linea);

    fclose(file);
}

/************************/
/****** CPU Utils *******/
/************************/

void procesar_conexion_cpu_dispatch()
{
    op_code cod_op;
    
    /*
     * Seteo de variables globales (semaforos y booleanos):
     *  post sem_quantum (instruccion e/s)
     *  false desalojado_fin_quantum (instruccion e/s)
     */ 

    while(cpu_dispatch_fd != -1) {
        cod_op = recibir_operacion(cpu_dispatch_fd);
        if(cod_op == -1) {
            log_error(logger, "El cliente se desconecto: %s \n",CPU);
        }
        if (cod_op == CONTEXTO_EJECUCION) {
            op_code cod_instruccion;
            t_contexto_ejecucion* contexto = recibir_contexto_cpu(cpu_dispatch_fd);
            t_pcb* pcb = remover_pcb(cola_exec, &mx_cola_exec);
            actualizar_pcb(pcb, contexto);
            cod_instruccion = recibir_operacion(cpu_dispatch_fd);
            //recv(cpu_dispatch_fd, &cod_instruccion, sizeof(op_code), 0);
            switch(cod_instruccion) {
                case I_WAIT:
                    char* recurso_wait = recibir_recurso(cpu_dispatch_fd);
                    handle_wait(pcb, recurso_wait);
                    free(recurso_wait);
                    break;
                case I_SIGNAL:
                    char* recurso_signal = recibir_recurso(cpu_dispatch_fd);
                    handle_signal(pcb, recurso_signal);
                    free(recurso_signal);
                    break;
                case IO_SLEEP:
                    handle_gen_sleep(pcb);
                    break;
                case SOLICITAR_IO_STDIN_READ:
                    handle_stdin_read(pcb);
                    break;
                case SOLICITAR_IO_STDOUT_WRITE:
                    handle_stdout_write(pcb);
                    break;
                case IO_CREATE:
                    handle_fs_create(pcb);
                    break;
                case IO_DELETE:
                    handle_fs_delete(pcb);
                    break;
                case IO_TRUNCATE:
                    handle_fs_truncate(pcb);
                    break;
                case IO_WRITE:
                    handle_fs_write(pcb);
                    break;
                case IO_READ:
                    handle_fs_read(pcb);
                    break;
                case HANDLE_EXIT:       
                    //ToDo: Si es interrupción por usuario entonces ponemos motivo de exit como INTERRUPTED_BY_USER
                    //quizás en otro case
                    // Recibir motivo exit
                    desalojado_fin_quantum = false;
                    pcb_to_exit(pcb, pcb->contexto_ejecucion->motivo_exit);
                    break;
                case FIN_QUANTUM:
                    log_info(logger, "PID: %d - Desalojado por fin de Quantum", pcb->contexto_ejecucion->pid);
                    if (!strcmp(config_kernel->algoritmo_planificacion, VRR)) {
                        pcb->quantum = config_kernel->quantum;
                    } 
                    actualizar_estado(pcb, READY);
                    push_pcb(cola_ready, pcb, &mx_cola_ready);
                    t_list* pid_list = get_pids(cola_ready);
                    char* pids = get_string_elements(pid_list);
                    log_info(logger, "Cola Ready %s: [%s]", config_kernel->algoritmo_planificacion, pids);
                    sem_post(&sem_ready);
                    sem_post(&sem_exec);
                    list_destroy(pid_list);
                    free(pids);
                break;
            }
        //list_destroy_and_destroy_elements(recibir_paquete(cpu_dispatch_fd),free);
        //sem_post(&sem_planificador_corto);
        }
    }
}

void handle_signal(t_pcb* pcb, char* recurso_asignado)
{
    t_recurso* recurso = malloc(sizeof(t_recurso));
    recurso = buscar_recurso(recurso_asignado);
    if (!recurso) {
        resource_not_found(recurso_asignado, pcb);
        return;
    }

    ++recurso->instancias;

    if (pcb) {
        int index = buscar_indice_en_cola(pcb->contexto_ejecucion->pid, recurso->cola_en_uso);
        remover_pcb_cola_block(recurso->cola_en_uso, &recurso->mx_asignado, index);
        push_pcb(cola_exec, pcb, &mx_cola_exec);
        enviar_contexto_ejecucion(pcb->contexto_ejecucion, cpu_dispatch_fd);
    }

    if (recurso->instancias == 0) {
        //En caso de que corresponda, desbloquea al primer proceso de la cola de bloqueados
        //Se devuelve la ejecucion al proceso que peticiona el signal
        t_pcb* pcb_desbloquear = remover_pcb(recurso->cola_block_asignada, &recurso->mx_asignado);

        actualizar_estado(pcb_desbloquear, READY);
        push_pcb(cola_ready, pcb_desbloquear, &mx_cola_ready);
        t_list* pid_list = get_pids(cola_ready);
        char* pids = get_string_elements(pid_list);
        log_info(logger, "Cola Ready %s: [%s]", config_kernel->algoritmo_planificacion, pids);
        list_destroy(pid_list);
        free(pids);
        push_pcb(recurso->cola_en_uso, pcb_desbloquear, &recurso->mx_asignado);
        if(list_is_empty(cola_exec))
            sem_wait(&sem_exec);
        sem_post(&sem_ready);
        //push_pcb(cola_block, pcb, &mx_cola_block);
        //sem_post(&sem_block_recurso);
    }

    sem_post(&sem_quantum);
}

void handle_wait(t_pcb* pcb, char* recurso_asignado)
{
    t_recurso* recurso = malloc(sizeof(t_recurso));
    recurso = buscar_recurso(recurso_asignado);
	if (!recurso) {
        resource_not_found(recurso_asignado, pcb);
        return;
    }

    --recurso->instancias;
    if (recurso->instancias < 0) {
        actualizar_estado(pcb, BLOCK);
        log_info(logger,"PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, recurso_asignado);
        push_pcb(recurso->cola_block_asignada, pcb, &recurso->mx_asignado);
        sem_post(&sem_exec);
    } else {
        push_pcb(recurso->cola_en_uso, pcb, &recurso->mx_asignado);
        push_pcb(cola_exec, pcb, &mx_cola_exec);
        enviar_contexto_ejecucion(pcb->contexto_ejecucion, cpu_dispatch_fd);
    }
    sem_post(&sem_quantum);
}

void handle_gen_sleep(t_pcb* pcb)
{
    desalojado_fin_quantum = false;

    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_GEN_SLEEP)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }

    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);

    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = NULL;
    parametros->param2 = *(int*) list_get(paquete, 1);
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->lista_direcciones = list_create();
    parametros->prox_io = IO_SLEEP;

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_stdin_read(t_pcb* pcb)
{
    // 1ro: Desalojo pcb
    desalojado_fin_quantum = false;

    // 2do: Recibo nombre interfaz, realizo las validaciones y bloqueo el pcb
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz *interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_STDIN_READ)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }

    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);

    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    // 3ro: Recibo los parametros correspondientes
    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = string_new();
    parametros->param2 = -1;
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->lista_direcciones = list_create();

    int cantidad = *(int*)list_get(paquete, 1);
	for(int i = 2; i < 2 + (2 * cantidad) ; i++) {
        uint32_t* dir_tam = (uint32_t*) list_get(paquete, i);
        list_add(parametros->lista_direcciones, dir_tam);
	}

    parametros->prox_io = IO_STDIN_READ;

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_stdout_write(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = (char*) list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_STDOUT_WRITE)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }    
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);

    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = string_new();
    parametros->param2 = -1;
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->lista_direcciones = list_create();

    int cantidadWrite = *(int*)list_get(paquete, 1);
	for(int i = 2; i < 2 + (2 * cantidadWrite) ; i++) {
        uint32_t* dir_tam = (uint32_t*) list_get(paquete, i);
        list_add(parametros->lista_direcciones, dir_tam);
	}

    parametros->prox_io = IO_STDOUT_WRITE;

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_fs_create(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_FS_CREATE)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);

     if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = (char*)list_get(paquete, 1);
    parametros->param2 = -1;
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->prox_io = IO_FS_CREATE;
    parametros->lista_direcciones = list_create();

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_fs_delete(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_FS_DELETE)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);
    
    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = (char*)list_get(paquete, 1);
    parametros->param2 = -1;
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->prox_io = IO_FS_DELETE;
    parametros->lista_direcciones = list_create();

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_fs_truncate(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_FS_TRUNCATE)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);
    
    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = (char*)list_get(paquete, 1);
    parametros->param2 = *(int*)list_get(paquete, 2);
    parametros->param3 = -1;
    parametros->param4 = -1;
    parametros->prox_io = IO_FS_TRUNCATE;
    parametros->lista_direcciones = list_create();

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_fs_write(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_FS_WRITE)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);
    
    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = (char*)list_get(paquete, 1);
    parametros->param2 = *(int*)list_get(paquete, 2);
    parametros->param3 = *(int*)list_get(paquete, 3);
    parametros->param4 = -1;
    parametros->prox_io = IO_FS_WRITE;
    parametros->lista_direcciones = list_create();

	for(int i = 4; i < 4 + (2 * parametros->param2) ; i++) {
        uint32_t* dir_tam = (uint32_t*) list_get(paquete, i);
        list_add(parametros->lista_direcciones, dir_tam);
	}

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

void handle_fs_read(t_pcb* pcb)
{
    desalojado_fin_quantum = false;
    t_list* paquete = recibir_paquete(cpu_dispatch_fd);
    char* nombre_interfaz = list_get(paquete, 0);
    t_interfaz* interfaz_encontrada = buscar_interfaz(nombre_interfaz);
    if (!interfaz_encontrada || !admite_operacion(interfaz_encontrada->tipo_interfaz, IO_FS_READ)) {
        pcb_to_exit(pcb, INVALID_INTERFACE);
        list_destroy(paquete);
        free(nombre_interfaz);
        return;
    }
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion->pid, nombre_interfaz);
   
    if(!flag_sleep){
        sem_post(&sem_quantum);
        flag_sleep = true;
    }

    pcb_to_block(pcb);

    t_instruccion_io* parametros = malloc(sizeof(t_instruccion_io));
    parametros->param1 = (char*)list_get(paquete, 1);
    parametros->param2 = *(int*)list_get(paquete, 2);
    parametros->param3 = *(int*)list_get(paquete, 3);
    parametros->param4 = -1;
    parametros->prox_io = IO_FS_READ;
    parametros->lista_direcciones = list_create();

    for(int i = 4; i < 4 + (2 * parametros->param2) ; i++) {
        uint32_t* dir_tam = (uint32_t*) list_get(paquete, i);
        list_add(parametros->lista_direcciones, dir_tam);
	}

    ejecutar_interfaz(interfaz_encontrada, pcb, parametros);

    list_destroy(paquete);
    free(nombre_interfaz);
}

/***************************/
/******** E/S Utils ********/
/***************************/

void escuchar_io()
{
    pthread_t t_entrada_salida;
    int entrada_salida_fd;
    while(1) {
        entrada_salida_fd = esperar_cliente(socket_io);
        pthread_create(&t_entrada_salida, NULL, (void*) recibir_io, &entrada_salida_fd);
        pthread_detach(t_entrada_salida);
    }
}

void* recibir_io(void* entrada_salida_fd)
{
    int fd = *(int*)entrada_salida_fd;
    if (fd == -1) {
        log_error(logger, "Error en la conexion con el cliente\n");
        return NULL;
    }
    log_info(logger, "Conexion con E/S establecida\n");

    t_interfaz* interfaz = malloc(sizeof(t_interfaz));
    op_code cod_op;
    cod_op = recibir_operacion(fd);
    if(cod_op == NOMBREINTERFAZ){
        t_list* lista = recibirNombreInterfaz(fd);
        char* nombre = (char*)list_get(lista,0);
        char* tipo = (char*)list_get(lista,1);
        interfaz->nombreInterfaz = nombre;
        interfaz->tipo_interfaz = tipo;
        interfaz->fd = fd;
        interfaz->pcbs = list_create();
        list_destroy(lista);
        pthread_mutex_lock(&mx_cola_block);
        list_add(interfaces, interfaz);
        list_add(cola_block, interfaz);
        pthread_mutex_unlock(&mx_cola_block);
    }

    while ((cod_op = recibir_operacion(fd)) != -1) {
        if (cod_op == FIN_INTERFAZ) {
            t_pcb* pcb = malloc(sizeof(pcb));
            t_pcb* pcb_to_ready = malloc(sizeof(pcb));
            pcb_to_ready = (t_pcb*) list_remove(interfaz->pcbs, 0);
            actualizar_estado(pcb_to_ready, READY);

            if (!strcmp(config_kernel->algoritmo_planificacion, VRR)) {
                if(pcb_to_ready->quantum <= 0){
                    pcb_to_ready->quantum = config_kernel->quantum;
                    push_pcb(cola_ready, pcb_to_ready, &mx_cola_ready);
                    t_list* pid_list = get_pids(cola_ready);
                    char* pids = get_string_elements(pid_list);
                    log_info(logger, "Cola Ready %s: [%s]", config_kernel->algoritmo_planificacion, pids);
                    list_destroy(pid_list);
                    free(pids);
                } else {
                    push_pcb(cola_ready_prioridad, pcb_to_ready, &mx_cola_ready_prioridad);
                    t_list* pid_list = get_pids(cola_ready_prioridad);
                    char* pids = get_string_elements(pid_list);
                    log_info(logger, "Cola Ready Prioridad %s: [%s]", config_kernel->algoritmo_planificacion, pids);
                    list_destroy(pid_list);
                    free(pids);
                }
            } else {
                push_pcb(cola_ready, pcb_to_ready, &mx_cola_ready);
                t_list* pid_list = get_pids(cola_ready);
                char* pids = get_string_elements(pid_list);
                log_info(logger, "Cola Ready %s: [%s]", config_kernel->algoritmo_planificacion, pids);
                list_destroy(pid_list);
                free(pids);
            }
            sem_post(&sem_ready);


            if (list_size(interfaz->pcbs) > 0 ) {
                //Si hay pcbs en espera entonces ejecuta el primero
                pcb = list_get(interfaz->pcbs, 0);
                enviar_instruccion_io(pcb->contexto_ejecucion->pid ,interfaz->nombreInterfaz, pcb->proxima_io, interfaz->fd);
            } 
        }
    }
    if (list_remove_element(interfaces, interfaz) && list_remove_element(cola_block, interfaz)) {
        log_info(logger, "%s removida de la lista de interfaces y de la cola de blocked.\n", interfaz->nombreInterfaz);
    } else {
        log_error(logger, "Error al quitar %s de la lista de interfaces.\n", interfaz->nombreInterfaz);
    }

    log_warning(logger, "Se desonecto E/S.\n");
}

/***************************/
/****** Planificacion ******/
/***************************/

void iniciar_planificador_largo_plazo()
{
    pthread_t t_new_to_ready;
    pthread_t t_to_exit;
    pthread_create(&t_new_to_ready, NULL, (void*) new_to_ready, NULL);
    pthread_detach(t_new_to_ready);
    pthread_create(&t_to_exit, NULL, (void*) to_exit, NULL);
    pthread_detach(t_to_exit);
}

void new_to_ready()
{
    while(1) {
        sem_wait(&sem_new);
        sem_wait(&sem_gr_multi_prog);
        sem_wait(&sem_planificador_largo);

        pthread_mutex_lock(&mx_cola_ready);

        t_pcb* pcb = remover_pcb(cola_new, &mx_cola_new);
        actualizar_estado(pcb, READY);
        list_add(cola_ready, pcb);
        t_list* pid_list = get_pids(cola_ready);
        char* pids = get_string_elements(pid_list);
        log_info(logger, "Cola Ready %s: [%s]", config_kernel->algoritmo_planificacion, pids);
        list_destroy(pid_list);
        free(pids);

        pthread_mutex_unlock(&mx_cola_ready);

        sem_post(&sem_planificador_largo);
        sem_post(&sem_ready);
    }

}

void to_exit()
{
    while(1) {
        sem_wait(&sem_exit);

        t_pcb* pcb = remover_pcb(cola_exit, &mx_cola_exit);
        char* motivo = get_motivo_exit(pcb->contexto_ejecucion->motivo_exit);
        log_info(logger, "Finaliza el proceso %d - Motivo %s", pcb->contexto_ejecucion->pid, motivo);
        
        // Recibir ok de memoria: enviar_op_status()
        enviar_fin_proceso(pcb->contexto_ejecucion->pid, memoria_fd);
        free_pcb(pcb);
        sem_post(&sem_gr_multi_prog);
    }
}

void iniciar_planificador_corto_plazo()
{
    while (1) {
        ready_to_exec();
    }
}

void ready_to_exec()
{
    sem_wait(&sem_ready);
    sem_wait(&sem_exec);
    sem_wait(&sem_planificador_corto);
    flag_sleep = false;

    if (!strcmp(config_kernel->algoritmo_planificacion, FIFO)) {
        dispatch_pcb(remover_pcb(cola_ready, &mx_cola_ready));
    } else if (!strcmp(config_kernel->algoritmo_planificacion, RR)) {
        algoritmo_round_robin(remover_pcb(cola_ready, &mx_cola_ready));
    } else if (!strcmp(config_kernel->algoritmo_planificacion, VRR)) {
        algoritmo_virtual_round_robin();
    } else {
        log_error(logger, "Algoritmo de planificacion no definido, abortando...");
        exit(EXIT_FAILURE);
    }
    sem_post(&sem_planificador_corto);
}

void dispatch_pcb(t_pcb* pcb)
{
    actualizar_estado(pcb, EXEC);
    push_pcb(cola_exec, pcb, &mx_cola_exec);
    enviar_contexto_ejecucion(pcb->contexto_ejecucion, cpu_dispatch_fd);
}

/***************************************/
/****** Algoritmos Planificacion *******/
/***************************************/

/***** FIFO ****/

void push_pcb(t_list* cola, t_pcb* pcb, pthread_mutex_t* mutex)
{
    pthread_mutex_lock(mutex);
    list_add(cola, pcb);
    pthread_mutex_unlock(mutex);
}

t_pcb* remover_pcb(t_list* cola, pthread_mutex_t* mutex)
{
    t_pcb* pcb;
    pthread_mutex_lock(mutex);
    pcb = list_remove(cola, 0);
    pthread_mutex_unlock(mutex);
    return pcb;
}

t_pcb* remover_pcb_cola_block(t_list* cola, pthread_mutex_t* mutex, int i)
{
    t_pcb* pcb;
    pthread_mutex_lock(mutex);
    pcb = list_remove(cola, i);
    pthread_mutex_unlock(mutex);
    return pcb;
}

/***** RR *****/

void algoritmo_round_robin(t_pcb* pcb)
{
    dispatch_pcb(pcb);
    pthread_create(&t_quantum, NULL, (void*) esperar_por_quantum, (void*) pcb);
    pthread_detach(t_quantum);
    desalojo_pre_fin_quantum(pcb, NULL);

}

/***** VRR *****/

void algoritmo_virtual_round_robin()
{
    t_pcb* pcb;

    if(list_size(cola_ready_prioridad) > 0)
        pcb = remover_pcb(cola_ready_prioridad, &mx_cola_ready_prioridad);
    else 
        pcb = remover_pcb(cola_ready, &mx_cola_ready);
    
    dispatch_pcb(pcb);
    t_temporal* temp = temporal_create();
    pthread_create(&t_quantum, NULL, (void*) esperar_por_quantum, (void*) pcb);
    pthread_detach(t_quantum);
    desalojo_pre_fin_quantum(pcb, temp);
}

/***** QUANTUM *****/

void esperar_por_quantum(t_pcb* pcb)
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    usleep(pcb->quantum * 1000);
    pthread_testcancel();

    if(!flag_sleep){
        desalojado_fin_quantum = true;
        flag_sleep = true;
        sem_post(&sem_quantum);
    }
}

void desalojo_pre_fin_quantum(t_pcb* pcb, t_temporal* temp)
{
    sem_wait(&sem_quantum);
    pthread_cancel(t_quantum);

    int tiempo_transcurrido = 0;
    if (!desalojado_fin_quantum) {
        
       // t_pcb* pcb = remover_pcb(cola_exec, &mx_cola_exec);
        if (!strcmp(config_kernel->algoritmo_planificacion, VRR)) {
            temporal_stop(temp);
            tiempo_transcurrido = (int)temporal_gettime(temp);
            if (tiempo_transcurrido < pcb->quantum) {
                pcb->quantum -= tiempo_transcurrido;
            } 
            temporal_destroy(temp);
        }
    } else {
        if (!strcmp(config_kernel->algoritmo_planificacion, VRR)) {
            temporal_destroy(temp);
        }
        enviar_interrupcion(cpu_interrupt_fd, INTERRUPCION);
    }
    desalojado_fin_quantum = false;
    
}

/********************/
/****** Utils *******/
/********************/

t_funcion inp_to_funcion(char* inp)
{
    if(!strcmp("EJECUTAR_SCRIPT", inp)) return EJECUTAR_SCRIPT;
    if(!strcmp("INICIAR_PROCESO", inp)) return INICIAR_PROCESO;
    if(!strcmp("FINALIZAR_PROCESO", inp)) return FINALIZAR_PROCESO;
    if(!strcmp("DETENER_PLANIFICACION", inp)) return DETENER_PLANIFICACION;
    if(!strcmp("INICIAR_PLANIFICACION", inp)) return INICIAR_PLANIFICACION;
    if(!strcmp("MULTIPROGRAMACION", inp)) return MULTIPROGRAMACION;
    if(!strcmp("PROCESO_ESTADO", inp)) return PROCESO_ESTADO;
    return -1;
}

void actualizar_estado(t_pcb* pcb, t_estado estado)
{
    if (pcb->estado_actual != estado) {
        pcb->estado_anterior = pcb->estado_actual;
        pcb->estado_actual = estado;
        char* estado_nuevo = strdup(estado_string(pcb->estado_actual));
        char* estado_anterior = strdup(estado_string(pcb->estado_anterior));
        log_info(logger, "PID: %d - Estado Anterior: %s - Estado Actual: %s", pcb->contexto_ejecucion->pid, estado_anterior, estado_nuevo);
        free(estado_nuevo);
        free(estado_anterior);
    }
}

char* estado_string(t_estado estado)
{
    switch (estado) {
    case NEW:
        return "NEW";
        break;
    case READY:
        return "READY";
        break;
    case BLOCK:
        return "BLOCK";
        break;
    case EXEC:
        return "EXEC";
        break;
    case EXIT:
        return "EXIT";
        break;
    default:
        return "UNKNOWN";
        break;
    }
}

t_list* get_pids(t_list* lista)
{
    t_list* pid_list = list_create();
    for (int i = 0; i < list_size(lista); ++i) {
        t_pcb* pcb = list_get(lista, i);
        list_add(pid_list, &(pcb->contexto_ejecucion->pid));
    }
    return pid_list;
}

void actualizar_pcb(t_pcb* pcb, t_contexto_ejecucion* contexto)
{
	free(pcb->contexto_ejecucion);
    pcb->contexto_ejecucion = contexto;
}

bool admite_operacion(char* tipo_interfaz, instruction_code instruccion)
{
    if (!strcmp(tipo_interfaz, "GENERICA")) {
        return instruccion == IO_GEN_SLEEP;
    } 
    if (!strcmp(tipo_interfaz, "STDIN")) {
        return instruccion == IO_STDIN_READ;
    }
    if (!strcmp(tipo_interfaz, "STDOUT")) {
        return instruccion == IO_STDOUT_WRITE;
    }
    if (!strcmp(tipo_interfaz, "DIALFS")) {
        return (instruccion == IO_FS_CREATE) 
            || (instruccion == IO_FS_DELETE)
            || (instruccion == IO_FS_TRUNCATE)
            || (instruccion == IO_FS_WRITE)
            || (instruccion == IO_FS_READ);
    }
    return true;
}

t_recurso* buscar_recurso(char* recurso)
{
    int len = list_size(lista_recursos);
    t_recurso* recurso_a_buscar;
    for (int i = 0; i < len; ++i) {
        recurso_a_buscar = (t_recurso*) list_get(lista_recursos, i);
        if (!strcmp(recurso_a_buscar->nombre, recurso))
            return recurso_a_buscar;
    }
    return NULL;
}

void resource_not_found(char* recurso, t_pcb* pcb)
{
    log_error(logger, "No existe el recurso solicitado: %s", recurso);
    pcb->contexto_ejecucion->motivo_exit = INVALID_RESOURCE;
    actualizar_estado(pcb, EXIT);
    push_pcb(cola_exit, pcb, &mx_cola_exit);
    sem_post(&sem_exit);
    sem_post(&sem_exec);
}

void pcb_to_exit(t_pcb* pcb, t_motivo_exit motivo) 
{
    actualizar_estado(pcb, EXIT);
    pcb->contexto_ejecucion->motivo_exit = motivo;
    push_pcb(cola_exit, pcb, &mx_cola_exit);
    if(!flag_sleep){
        flag_sleep = true;
        sem_post(&sem_quantum);
    }
    sem_post(&sem_exit);
    sem_post(&sem_exec);
}

void pcb_to_block(t_pcb* pcb)
{
    actualizar_estado(pcb, BLOCK);
    //push_pcb(cola_block, pcb, &mx_cola_block);
    sem_post(&sem_exec);
}

t_interfaz* buscar_interfaz(char* interfaz)
{
    int _buscar_interfaz(t_interfaz* interfaz_a_buscar){
        return !strcmp(interfaz_a_buscar->nombreInterfaz, interfaz);
    };

    return (void*) list_find(interfaces, (void*) _buscar_interfaz);
}

t_pcb* buscar_pcb_por_id(int pid)
{
    int _buscar_pcb(t_pcb* pcb_a_buscar){
        return pcb_a_buscar->contexto_ejecucion->pid == pid;
    };

    return (void*) list_find(pcbs_totales, (void*) _buscar_pcb);
}

bool invalid_parameter(char* p)
{
    return string_is_empty(p) || !p;
}

void ejecutar_interfaz(t_interfaz* interfaz_encontrada, t_pcb* pcb, t_instruccion_io* parametros)
{
    if (!list_is_empty(interfaz_encontrada->pcbs))
        pcb->proxima_io = parametros;
    else
        enviar_instruccion_io(pcb->contexto_ejecucion->pid ,interfaz_encontrada->nombreInterfaz, parametros, interfaz_encontrada->fd);

    list_add(interfaz_encontrada->pcbs, pcb);
}


t_pcb* remover_pcb_interfaz(int pid)
{
    t_pcb* pcb_to_remove;
    int i = 0;
    int _buscar_pcb_interfaz(t_interfaz* interfaz) {
        int _buscar_pcb_pcbs(t_pcb* pcb){
            if (pcb->contexto_ejecucion->pid == pid /*&& list_remove_element(interfaz->pcbs, pcb)*/){
                pcb_to_remove = (t_pcb*) list_get(interfaz->pcbs, i);
            }
            ++i;
        };
        return (void *)list_find(interfaz->pcbs, (void *)_buscar_pcb_pcbs);
    };

    (void *)list_find(cola_block, (void *)_buscar_pcb_interfaz);

    return pcb_to_remove;
}

t_recurso* buscar_pcb_en_recurso(int pid)
{ 
    t_recurso* recurso_encontrado = malloc(sizeof(t_recurso));
    int _buscar_pcb_recursos(t_recurso* recurso) {
        int _buscar_cola(t_pcb* pcb) {
            if (pcb->contexto_ejecucion->pid == pid)
                recurso_encontrado = recurso;
        };
        list_iterate(recurso->cola_block_asignada, (void*)_buscar_cola);
    };

    list_find(lista_recursos, (void*)_buscar_pcb_recursos);

    return recurso_encontrado;
}

/*t_recurso* buscar_pcb_en_recurso_en_uso(int pid)
{ 
    t_recurso* recurso_encontrado = malloc(sizeof(t_recurso));
    int _buscar_pcb_recursos_uso(t_recurso* recurso) {
        int _buscar_cola_uso(t_pcb* pcb) {
            if (pcb->contexto_ejecucion->pid == pid){
                recurso_encontrado = recurso;
            }
                
        };
        list_iterate(recurso->cola_en_uso, (void*)_buscar_cola_uso);
    };

    list_find(lista_recursos, (void*)_buscar_pcb_recursos_uso);

    return recurso_encontrado;
}
*/

t_recurso* buscar_pcb_en_recurso_en_uso(int pid)
{ 
    t_recurso* recurso_encontrado = malloc(sizeof(t_recurso));
    int _buscar_pcb_recursos_uso(t_recurso* recurso) {
        int _buscar_cola_uso(t_pcb* pcb) {
            if (pcb->contexto_ejecucion->pid == pid){
                recurso_encontrado = recurso;
                return true;
            }
                
        };
        list_find(recurso->cola_en_uso, (void*)_buscar_cola_uso);
    };

    list_find(lista_recursos, (void*)_buscar_pcb_recursos_uso);

    return recurso_encontrado;
}

int buscar_indice_en_cola(int pid, t_list* cola)
{
    int index = 0;
    int _buscar_pcb_cola(t_pcb* pcb) {
        if (pcb->contexto_ejecucion->pid == pid){
            return NULL;
        }
            ++index;
    }

    list_iterate(cola, (void*)_buscar_pcb_cola);

    return index;
}

/********************/
/****** Free's ******/
/********************/

void free_pcb(t_pcb *pcb)
{
    free_contexto_ejecucion(pcb->contexto_ejecucion);
    free(pcb->proxima_io->param1);
    if (!list_is_empty(pcb->proxima_io->lista_direcciones)) list_destroy(pcb->proxima_io->lista_direcciones);
    free(pcb->proxima_io);
    free(pcb);
}

void free_recurso(t_recurso* recurso)
{
    free(recurso->nombre);
    free(recurso);
}

void free_logger()
{
    log_destroy(logger);
}

void free_config()
{
    free(config_kernel->puerto_escucha);
    free(config_kernel->ip_memoria);
    free(config_kernel->puerto_memoria);
    free(config_kernel->ip_cpu);
    free(config_kernel->puerto_cpu_dispatch);
    free(config_kernel->puerto_cpu_interrupt);
    free(config_kernel->algoritmo_planificacion);
    free(config_kernel->recursos);
    free(config_kernel);
    config_destroy(config);
}

void free_conexiones()
{
    liberar_conexion(socket_io);
    liberar_conexion(cpu_dispatch_fd);
    liberar_conexion(cpu_interrupt_fd);
    liberar_conexion(memoria_fd);
}
