#include <cpu.h>

char *g_nombre_config;
sem_t sem;

int main(int argc, char* argv[]) {

    /*******************/
    /****** INITS ******/
    /*******************/
	g_nombre_config = argv[1];

    sem_init(&sem,0,1);
    iniciar_logger();
    iniciar_config();
    iniciar_tlb();
    iniciar_conexiones();
    
    /*******************/
    /****** FREEs ******/
    /*******************/
    free_config();
    free_conexiones();
    free_logger();

    return EXIT_SUCCESS;
}

/*******************/
/****** INITS ******/
/*******************/
void iniciar_logger() {
    logger = log_create("./cfg/cpu.log", CPU, 1, LOG_LEVEL_INFO);
    if (logger == NULL) {
        exit(EXIT_FAILURE);
    }
}

void iniciar_config() {
    config_cpu = malloc(sizeof(t_config_cpu));

    char *path = string_from_format("%s%s%s", "./cfg/", g_nombre_config, ".config");
    //printf("path: %s\n",path);

    config = config_create(path);
    if (config == NULL) {
        perror("\nNo se pudo encontrar el path del config.\n");
        exit(EXIT_FAILURE);
    }

    config_cpu->ip_memoria = strdup(config_get_string_value(config, "IP_MEMORIA"));
    config_cpu->puerto_memoria = strdup(config_get_string_value(config, "PUERTO_MEMORIA"));
    config_cpu->puerto_escucha_dispatch = strdup(config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH"));
    config_cpu->puerto_escucha_interrupt = strdup(config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT"));
    config_cpu->cantidad_entradas_tlb = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    config_cpu->algoritmo_tlb = strdup(config_get_string_value(config, "ALGORITMO_TLB"));
    config_cpu->tam_max_pagina = config_get_int_value(config, "TAMANIO_MAX_PAGI");
}

void iniciar_tlb(){
    int cantidad_entradas =config_cpu->cantidad_entradas_tlb ;
    if(cantidad_entradas >= 0){
        tlb = list_create();
    }else
    {
        perror("no se pudo asignar la cantidad de entradas al TLB");
        exit(EXIT_FAILURE);
    }
}

void iniciar_conexiones() {
    
    iniciar_conexion_memoria();
    iniciar_server_dispatch();
    iniciar_server_interrupt();
    
    // creo que estaria necesitando mas que nada,  alguna especie de señal o momento en el que 
    // controle que no me llegue algun op_code de interrupt ya que ahi lo debo detener todo el proceso

    // Esperar a que los hilos terminen
    //pthread_join(hilo_dispatch, NULL);
    pthread_join(hilo_interrupt, NULL);

    //free(mensaje);
}

void iniciar_conexion_memoria(){
     // CONEXION MEMORIA
    memoria_fd = crear_conexion(IP_MEMORIA, config_cpu->puerto_memoria);
    if (memoria_fd == -1) {
        printf("Error en la conexion con MEMORIA.\n");
        exit(EXIT_FAILURE);
    }
}

void iniciar_server_dispatch(){
    // CONEXION KERNEL DISPATCH
    socket_cpu_dispatch = iniciar_servidor(IP_CPU, config_cpu->puerto_escucha_dispatch);
    if (socket_cpu_dispatch == -1) {
    	printf("Error al recibir KERNEL por DISPATCH.\n");
        exit(EXIT_FAILURE);
    }

    // escuchar_dispatch(); // ver si lo puedo mover esta funcion
    // CREO EL HILO PARA ESCUCHAR DISPATCH
    if(pthread_create(&hilo_dispatch,NULL,(void*)escuchar_dispatch,NULL)!=0){
    	printf("Error al crear hilo Dispatch.\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_dispatch);
}

void iniciar_server_interrupt(){
    // CONEXION KERNEL INTERRUPT
    socket_cpu_interrupt = iniciar_servidor(IP_CPU, config_cpu->puerto_escucha_interrupt);
    if (socket_cpu_interrupt == -1) {
    	printf("Error al recibir KERNEL por INTERRUPT.\n");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&hilo_interrupt,NULL,(void*)escuchar_interrupt,NULL)!=0){
    	printf(" Error al crear hilo Interrupt.\n");
        exit(EXIT_FAILURE);
    }
    //pthread_detach(hilo_interrupt);
}


void escuchar_dispatch() {
	int cod_op;
    // char* mensaje = malloc(sizeof(char*));
    dispatch_fd = esperar_cliente(socket_cpu_dispatch);
    if (dispatch_fd == -1) {
    	printf("Error en la conexion con el cliente\n");
    }
    printf("Conexion con KERNEL por DISPATCH establecida.\n");

    while((cod_op = recibir_operacion(dispatch_fd)) != -1){
        switch(cod_op) {
            case CONTEXTO_EJECUCION:
                t_contexto_ejecucion* contexto = recibir_contexto_cpu(dispatch_fd);
                //terminar de cargar los valores de registro de la lista de instrucciones al contexto

                inicio_ciclo_de_instrucciones(contexto);
                free_contexto_ejecucion(contexto);
                break;

            default:
                printf("escuchar_interrupt() - operación desconocida %d\n", cod_op);
            }
    }
    
} 

void escuchar_interrupt() {
    op_code cod_op;
    interrupt_fd = esperar_cliente(socket_cpu_interrupt);
    if (interrupt_fd == -1) {
    	printf("Error en la conexion con el cliente.\n");
    }
    printf("Conexion con KERNEL por INTERRUPT establecida.\n");

    //recibo datos del cliente
    while((cod_op = recibir_operacion(interrupt_fd)) != -1){
        switch(cod_op) {
            case INTERRUPCION:
                interrupt_flag = 1;
            break;
            case INTERRUPTED_BY_USER:
                interrupt_flag = 2;
            break;
            default:
                printf("escuchar_interrupt() - operación desconocida %d\n", cod_op);
        }
        t_list* list = recibir_paquete(interrupt_fd);
        list_destroy_and_destroy_elements(list, free);
    }
}

void inicio_ciclo_de_instrucciones(t_contexto_ejecucion* contexto){
	exec_flag = true;
    while(exec_flag) {
        if (interrupt_flag) {
            switch (interrupt_flag) {
                case 1:
                    printf("ENVIANDO INTERRUPCION DE QUANTUM \n");
                    exit_con_motivo(contexto, INTERRUPTED_BY_USER, FIN_QUANTUM);     
                break;
                case 2:
                    exit_con_motivo(contexto, INTERRUPTED_BY_USER, HANDLE_EXIT);
                break;
                default:
                    enviar_contexto_actualizado(contexto, true);
                break;
            }
            interrupt_flag = 0;
        } else {
            fetch(contexto); 
        }
    }
}

void fetch(t_contexto_ejecucion* contexto){
    t_paquete_a_memoria* paquete_pid_pc = malloc(sizeof(t_paquete_a_memoria));
    paquete_pid_pc->pid = contexto->pid;
    paquete_pid_pc->program_counter = contexto->program_counter;
    
    enviar_pid_pc_memoria(paquete_pid_pc,memoria_fd);

    mem_status status = recibir_mem_status(memoria_fd);
    if (status == MEM_EXITO) {
        op_code cod_op = recibir_operacion(memoria_fd);
        if (cod_op == INSTRUCCION) {
            log_info(logger, "PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->program_counter);

			t_list* paquete = recibir_paquete(memoria_fd);
			decode(paquete,contexto);
			list_destroy_and_destroy_elements(paquete, free);
        }
        else {
        	printf("fetch() - cod_op (%d) != INSTRUCCION\n", cod_op);
        }
    }
    else {
    	printf("fetch() - mem_status = %d\n", status);
        exec_flag = false;
    }
}

void decode(t_list *paquete, t_contexto_ejecucion *contexto)
{
    bool auto_incremento_pc = true;

    char *nro_linea = (char *)list_get(paquete, 0);
    log_info(logger,"PID: %d - Ejecutando: %s", contexto->pid, nro_linea);
    t_instruccion *instruccion = parsear_instruccion(nro_linea);
    int cod_instru = convert_instruction_a_codeop(nro_linea);
    // ToDo , llevarlo a otro lado esta inicializacion y plasmarlo dentro del switch-case
    char* reg1 = NULL;
    char* reg2 = NULL;
    char* reg3 = NULL;
    char* reg4 = NULL;
    char* reg5 = NULL;
    switch (cod_instru) {
        case SET:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            if (comparar_set_pc(instruccion, "PC")) {
                contexto->program_counter = (uint32_t)atoi(reg2);
                auto_incremento_pc=false;
            }
            else {
                set(contexto, reg1, reg2);
            }
            break;
        case SUM:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

            sum(contexto, reg1, reg2);
            break;
        case SUB:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

            sub(contexto, reg1, reg2);
            break;
        case JNZ:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

             // jnz(contexto, reg1, reg2);
            if (get_valor_registro(contexto, reg1) != 0) {
                contexto->program_counter = (uint32_t) atoi(reg2);
                auto_incremento_pc = false;
            }
            break;
        case IO_GEN_SLEEP:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

            enviar_contexto_actualizado(contexto, true);
            io_gen_sleep(reg1, atoi(reg2));

            break;
        case MOV_OUT:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

            mov_out(contexto, reg1, reg2);
            break;
        case MOV_IN:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);

            mov_in(contexto, reg1, reg2);
            break;
        case RESIZE:
            reg1 = list_get(instruccion->parametros, 0);

            resize(contexto,reg1);
            break;
        case COPY_STRING:
            reg1 = list_get(instruccion->parametros, 0);

            copy_string(contexto,reg1);
            break;
        case IO_STDIN_READ:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            reg3 = list_get(instruccion->parametros, 2);
            io_stdin_read(contexto,reg1, reg2, reg3 );
            break;
        case IO_STDOUT_WRITE:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            reg3 = list_get(instruccion->parametros, 2);
            io_stdin_write(contexto,reg1, reg2, reg3 );
            break;
        case WAIT:
            reg1 = list_get(instruccion->parametros, 0);
            instruction_wait(contexto,reg1);
            break;
        case SIGNAL:
            reg1 = list_get(instruccion->parametros, 0);
            instruction_signal(contexto,reg1);
            break;
        case IO_FS_CREATE:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            io_fs_create(contexto,reg1, reg2 );
            break;
        case IO_FS_DELETE:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            io_fs_delete(contexto,reg1, reg2 );
            break;
        case IO_FS_TRUNCATE:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            reg3 = list_get(instruccion->parametros, 2);
            io_fs_truncate(contexto,reg1, reg2,reg3);
            break;
        case IO_FS_WRITE:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            reg3 = list_get(instruccion->parametros, 2);
            reg4 = list_get(instruccion->parametros, 3);
            reg5 = list_get(instruccion->parametros, 4);
            io_fs_write(contexto,reg1, reg2, reg3, reg4, reg5);
            break;
        case IO_FS_READ:
            reg1 = list_get(instruccion->parametros, 0);
            reg2 = list_get(instruccion->parametros, 1);
            reg3 = list_get(instruccion->parametros, 2);
            reg4 = list_get(instruccion->parametros, 3);
            reg5 = list_get(instruccion->parametros, 4);
            io_fs_read(contexto,reg1, reg2, reg3, reg4, reg5);
            break;
        case EXIT_INSTRUCCION:
            exit_con_motivo(contexto, SUCCESS, HANDLE_EXIT);
            break;
        default:
            printf("Instrucción inválida %s\n", nro_linea);
            break;
    }

    if(auto_incremento_pc)
        ++contexto->program_counter;

    //list_destroy_and_destroy_elements(paquete, free);
    liberar_instruccion(instruccion);
}

instruction_code convert_instruction_a_codeop(char* nombre_instruccion){
    if (!strcmp("SET", nombre_instruccion)) return SET;
	if (!strcmp("SUM", nombre_instruccion)) return SUM;
	if (!strcmp("SUB", nombre_instruccion)) return SUB;
	if (!strcmp("JNZ", nombre_instruccion)) return JNZ;
	if (!strcmp("IO_GEN_SLEEP", nombre_instruccion)) return IO_GEN_SLEEP;
    if (!strcmp("MOV_IN", nombre_instruccion)) return MOV_IN;
    if (!strcmp("MOV_OUT", nombre_instruccion)) return MOV_OUT;
    if (!strcmp("RESIZE", nombre_instruccion)) return RESIZE;
    if (!strcmp("COPY_STRING", nombre_instruccion)) return COPY_STRING;
    if (!strcmp("IO_STDIN_READ", nombre_instruccion)) return IO_STDIN_READ;
    if (!strcmp("IO_STDOUT_WRITE", nombre_instruccion)) return IO_STDOUT_WRITE;
    if (!strcmp("IO_FS_CREATE", nombre_instruccion)) return IO_FS_CREATE;
    if (!strcmp("IO_FS_DELETE", nombre_instruccion)) return IO_FS_DELETE;
    if (!strcmp("IO_FS_TRUNCATE", nombre_instruccion)) return IO_FS_TRUNCATE;
    if (!strcmp("IO_FS_WRITE", nombre_instruccion)) return IO_FS_WRITE;
    if (!strcmp("IO_FS_READ", nombre_instruccion)) return IO_FS_READ;
    if (!strcmp("SIGNAL", nombre_instruccion)) return SIGNAL;
    if (!strcmp("WAIT", nombre_instruccion)) return WAIT;
    if (!strcmp("EXIT", nombre_instruccion)) return EXIT_INSTRUCCION;

	return -1;
}
bool comparar_set_pc(t_instruccion* instruccion, char * reg){
    return strcmp((char*)list_get(instruccion->parametros,0), reg) == 0;
}

t_instruccion* parsear_instruccion(char* linea) {
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    instruccion->parametros = list_create();
    char* token = strtok(linea, " ");
    //copio de lo que tomo del strtok con strdup
    instruccion->nombre_ins = strdup(token);

    // al ingresar el NULL sobre el mismo token, informo que 
    // continue sobre la misma linea hasta proximo " "
    //token = strtok(NULL, " ");
    //instruccion->nombre_reg1 = token ? strdup(token) : NULL;

    // idem aca, con el mismo token sigo la misma linea hasta el proximo ""
    //token = strtok(NULL, " ");
    //instruccion->nombre_reg2 = token ? strdup(token) : NULL;
    while ((token = strtok(NULL, " ")) != NULL) {
        list_add(instruccion->parametros, strdup(token));
    }
    return instruccion;
}
void set(t_contexto_ejecucion* contexto, char* reg, char * valor){
    setear_registro(contexto,reg, (uint32_t) atoi(valor));
    
}

void setear_registro(t_contexto_ejecucion* cont,char* reg , uint32_t valor){
    if(strcmp(reg, "AX")==0){
        cont->AX = valor;
    }else if(strcmp(reg, "BX")==0){
        cont->BX = valor;
    }else if(strcmp(reg, "CX")==0){
        cont->CX = valor;
    }else if(strcmp(reg, "DX")==0){
        cont->DX = valor;
    }else if(strcmp(reg, "EAX")==0){
        cont->EAX = valor;
    }else if(strcmp(reg, "EBX")==0){
        cont->EBX = valor;
    }else if(strcmp(reg, "ECX")==0){
        cont->ECX = valor;
    }else if(strcmp(reg, "EDX")==0){
        cont->EDX = valor;
    }else if(strcmp(reg, "DI")==0){
        cont->DI = valor;
    }else if(strcmp(reg, "SI")==0){
        cont->SI= valor;
    }else{
    	printf("Registro %s no válido.\n", reg);
    	return;
    }

    printf("Registro %s establecido a %d.\n", reg, valor);
}

void sum(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    // como tendre que sumar, elijo tipo de dato mas alto
    uint32_t valor_reg1 = get_valor_registro(contexto,reg1);
    uint32_t valor_reg2 = get_valor_registro(contexto,reg2);
    // entiendo yo, que se debe guardar
    // q en el lugar donde esta el 1er valor a sumar    
    // ver como devuelve el valor!
    uint32_t valor_resultado = valor_reg1 + valor_reg2;
    
    setear_registro(contexto, reg1, valor_resultado);

}

void sub(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    // como tendre que sumar, elijo tipo de dato mas alto
    uint32_t valor_reg1 = get_valor_registro(contexto,reg1);
    uint32_t valor_reg2 = get_valor_registro(contexto,reg2);
    // entiendo yo,  ue se debe guardar
    // q en el lugar donde esta el 1er valor a sumar    
    // ver como devuelve el valor!
    uint32_t valor_resultado = valor_reg1 - valor_reg2;

    setear_registro(contexto, reg1, valor_resultado);
}

void jnz(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    if (get_valor_registro(contexto, reg1) != 0)
    	contexto->program_counter = (uint32_t) atoi(reg2);
}

void io_gen_sleep(char* reg1, int reg2){
    enviar_io_gen_sleep(reg1, reg2, dispatch_fd);
}


uint32_t get_valor_registro(t_contexto_ejecucion* contexto, char* reg){
    if (strcmp(reg, "AX") == 0) {
        return contexto->AX;
    } else if (strcmp(reg, "BX") == 0) {
        return contexto->BX;
    } else if (strcmp(reg, "CX") == 0) {
        return contexto->CX;
    } else if (strcmp(reg, "DX") == 0) {
        return contexto->DX;
    } else if (strcmp(reg, "EAX") == 0) {
        return contexto->EAX;
    } else if (strcmp(reg, "EBX") == 0) {
        return contexto->EBX;
    } else if (strcmp(reg, "ECX") == 0) {
        return contexto->ECX;
    } else if (strcmp(reg, "EDX") == 0) {
        return contexto->EDX;
    }else if (strcmp(reg, "DI") == 0) {
        return contexto->DI;
    }else if (strcmp(reg, "SI") == 0) {
        return contexto->SI;
    }else {
        printf("Error: Registro desconocido '%s'\n", reg);
        return 0;
    }
}

void enviar_contexto_actualizado(t_contexto_ejecucion* contexto, bool incrementar_pc) {

    if(incrementar_pc){
        contexto->program_counter += 1;
    }

    enviar_contexto_ejecucion(contexto, dispatch_fd);

    exec_flag = false;
    interrupt_flag = 0; //para descartar una interrupción que quedó colgada
}

void exit_con_motivo(t_contexto_ejecucion* contexto, t_motivo_exit motivo, op_code codigo){
	contexto->motivo_exit = motivo;
    if(codigo == FIN_QUANTUM){
        enviar_contexto_actualizado(contexto, false);
    } else {
        enviar_contexto_actualizado(contexto, true);
    }
    //pasar a serializacion.c
    t_paquete* paquete = crear_paquete(codigo);
	enviar_paquete(paquete, dispatch_fd);
	eliminar_paquete(paquete);
}


t_mmu* traducir_direccion_de_memoria(uint32_t direccion_logica,  t_contexto_ejecucion* contexto, t_list* tlb){
    t_mmu* mmu = malloc(sizeof(t_mmu));

    // ver TAM_MAX memoria!
    mmu->numero_pagina = floor (direccion_logica / config_cpu->tam_max_pagina);
    //printf("dir logica: %d tamanio pagina: %d", direccion_logica, config_cpu->tam_max_pagina);
    mmu->desplazamiento = direccion_logica - ((mmu->numero_pagina) * config_cpu->tam_max_pagina);
    //printf("el nro de pagina total a enviar es: %d , y si desplazamiento es %d.\n", mmu->numero_pagina,  mmu->desplazamiento );
    //mmu->direccion_fisica = mmu->desplazamiento + mmu->numero_pagina;

    
    bool marco_asociado = buscar_existencia_marco_en_tlb(contexto->pid,mmu->numero_pagina ,tlb);

    if(marco_asociado){
        manejar_tlb_hit(tlb,mmu,contexto);
    }
    else{
        manejar_tlb_miss(tlb,mmu,contexto);
    }
    return mmu;
}
// especie de for para tener tantas direcciones fisicas ,
// traduzco, envio a memoria!

bool buscar_existencia_marco_en_tlb(int pid, int pagina, t_list* tlb){
    // list size del tlb, al ppio solo tendra 1 , pero se incrementara por cada miss!
    for (int i = 0; i < list_size(tlb); i++) {
        t_tlb* tabla_tlb = list_get(tlb, i);
        if (tabla_tlb->tlb_pid == pid && tabla_tlb->pagina == pagina) {
            return true;    
        }
    }return false;
}
void manejar_tlb_hit(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto){
	/*printf("TABLA TLB:::::\n");
	for(int j=0; j<list_size(tlb); j++)
		printf("PID %d | Pág %d | Marco %d\n", ((t_tlb*)list_get(tlb, j))->tlb_pid, ((t_tlb*)list_get(tlb, j))->pagina, ((t_tlb*)list_get(tlb, j))->frame);
*/
    for(int i = 0; i < list_size(tlb); i++){
        t_tlb* tabla_tlb = list_get(tlb, i);
        if (tabla_tlb->tlb_pid == contexto->pid && tabla_tlb->pagina == mmu->numero_pagina) {
            mmu->direccion_fisica = (tabla_tlb->frame * config_cpu->tam_max_pagina) + mmu->desplazamiento;

            //si el algoritmo de reemplazo es LRU, el Hit debe hacer reiniciar el timer para esta entrada
            if(!strcmp(config_cpu->algoritmo_tlb, LRU)) {
            	temporal_destroy(((t_tlb*) list_get(tlb, i))->tiempo_uso);
            	((t_tlb*) list_get(tlb, i))->tiempo_uso = temporal_create();
            }


            log_info(logger, "PID: %d - TLB HIT - Pagina: %d", tabla_tlb->tlb_pid, tabla_tlb->pagina);
        }
    }
    
}
/*void manejar_tlb_miss(t_list* tlb,t_mmu* mmu, t_contexto_ejecucion* contexto){
    enviar_pid_nro_pagina_a_memoria(mmu->numero_pagina, contexto->pid, memoria_fd);
    // recibir respuesta de memoria!
    op_code cod_op = recibir_operacion(memoria_fd);
    if(cod_op == MEM_STATUS){
        t_list* list_status = recibir_paquete(memoria_fd);
        int status = *(int*) list_get(list_status,0);
        if(status == 0){
            op_code cod_op2_marco= recibir_operacion(memoria_fd);
            if(cod_op2_marco == NUMERO_DE_MARCO){
                t_list* paquete = list_create();
                paquete  = recibir_paquete(memoria_fd);
                if(list_size(tlb) >= config_cpu->cantidad_entradas_tlb){
                    aplicar_algoritmo(tlb,mmu,paquete,contexto);
                    actualizar_tabla_tlb(tlb,mmu,paquete,contexto);
                    log_info(logger, "PID: %d - TLB MISS - Pagina: %d", contexto->pid, mmu->numero_pagina);
                }
                else{
                    actualizar_tabla_tlb(tlb , mmu ,paquete, contexto);
                    log_info(logger, "PID: %d - TLB MISS - Pagina: %d", contexto->pid, mmu->numero_pagina);
                }
                
                list_destroy(paquete);
            } 
        }list_destroy(list_status);
        
    }

}*/

void manejar_tlb_miss(t_list* tlb,t_mmu* mmu, t_contexto_ejecucion* contexto){
    enviar_pid_nro_pagina_a_memoria(mmu->numero_pagina, contexto->pid, memoria_fd);

    if(config_cpu->cantidad_entradas_tlb != 0)
    	log_info(logger, "PID: %d - TLB MISS - Pagina: %d", contexto->pid, mmu->numero_pagina);
    if(recibir_mem_status(memoria_fd) == MEM_EXITO){
        int cod_op = recibir_operacion(memoria_fd);
        t_list* paquete  = recibir_paquete(memoria_fd);

		if(cod_op == NUMERO_DE_MARCO){
			if(list_size(tlb) >= config_cpu->cantidad_entradas_tlb && config_cpu->cantidad_entradas_tlb != 0)
				aplicar_algoritmo(tlb,mmu,contexto);
			actualizar_tabla_tlb(tlb,mmu,paquete,contexto);
		}
		else
			printf("cod_op (%d) != NUMERO_DE_MARCO\n", cod_op);

		list_destroy_and_destroy_elements(paquete, free);
	}
}


void aplicar_algoritmo(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto){
    if (!strcmp(config_cpu->algoritmo_tlb, FIFO)){
        algoritmo_fifo(tlb, mmu, contexto);
    }else if (!strcmp(config_cpu->algoritmo_tlb, LRU)){
        algoritmo_lru(tlb,  mmu,  contexto);
    }
    else{
    	printf("Algoritmo TLB no definido, abortando...\n");
        exit(EXIT_FAILURE);
    }
}
void algoritmo_fifo(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto){
    if (!list_is_empty(tlb)) {
        t_tlb* entrada_tlb_a_eliminar = (t_tlb*)list_remove(tlb, 0);
        temporal_destroy(entrada_tlb_a_eliminar->tiempo_uso);
        free(entrada_tlb_a_eliminar);
    }
    //list_remove_element(tlb, list_get(tlb, 0));
}
void algoritmo_lru(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto){
    int indice_eliminar = buscar_indice_entrada_mas_antigua(tlb);
    t_tlb* entrada_tlb_a_eliminar = list_remove(tlb,indice_eliminar);
    temporal_destroy(entrada_tlb_a_eliminar->tiempo_uso);
    free(entrada_tlb_a_eliminar);
}
//obtengo cual es la entrada mas antigua!
int buscar_indice_entrada_mas_antigua(t_list* tlb){
    int indice_mas_antiguo = 0;
    t_tlb* entrada_tlb_mas_antigua = list_get(tlb, 0);
    for(int i = 1; i < list_size(tlb); i++){
        t_tlb* entrada_tlb_actual = list_get(tlb, i);
        if(temporal_gettime(entrada_tlb_mas_antigua->tiempo_uso)< temporal_gettime(entrada_tlb_actual->tiempo_uso)){
            entrada_tlb_mas_antigua = entrada_tlb_actual;
            indice_mas_antiguo = i; // para decir que se volvio la mas vieja para la busqueda
        }
    }
    return indice_mas_antiguo;
}

void actualizar_tabla_tlb(t_list* tlb , t_mmu* mmu , t_list* paquete, t_contexto_ejecucion* contexto){
    int marco_nuevo = *(int *) list_get(paquete,0); // traigo el marco, ya que asumo solo pasara este
    t_tlb* nueva_entrada_tabla_tlb = malloc(sizeof(t_tlb));
    nueva_entrada_tabla_tlb->tlb_pid = contexto->pid;
    nueva_entrada_tabla_tlb->pagina = mmu->numero_pagina;
    nueva_entrada_tabla_tlb->frame = marco_nuevo;
    //creo un tiempo para cada proceso, aunque lo use mas que nada en lru!
    nueva_entrada_tabla_tlb->tiempo_uso = temporal_create();
    list_add(tlb, nueva_entrada_tabla_tlb);

    //una vez actualizado debo guardar la dir fisica 
    mmu->direccion_fisica = (marco_nuevo * config_cpu->tam_max_pagina) + mmu->desplazamiento;

    log_info(logger, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d",
    		contexto->pid, mmu->numero_pagina, marco_nuevo);
}

void resize(t_contexto_ejecucion* contexto, char* tamanio_resize){
//  log_info(logger, "pase por debajo de la instruccion, antes de enviar a memoria!.\n");
    enviar_resize_a_memoria(contexto->pid,atoi(tamanio_resize),memoria_fd);

    int status = recibir_mem_status(memoria_fd);
    if(status == MEM_OUT_OF_MEMORY){
    	exit_con_motivo(contexto, OUT_OF_MEMORY, HANDLE_EXIT);
    	printf("Memoria: OUT OF MEMORY\n");
    }
    else if(status == MEM_EXITO){
    	//remover las entradas de tlb que ya no sean válidas
    	int maxpag = (int) ceil(atoi(tamanio_resize) / (float) config_cpu->tam_max_pagina) -1;
    	for(int i=0; i<list_size(tlb); i++) {
    		 t_tlb* reg = list_get(tlb, i);
    		 if(reg->tlb_pid == contexto->pid && reg->pagina > maxpag) {
    			 if(reg->tiempo_uso)
    				 temporal_destroy(reg->tiempo_uso);
    			 list_remove_and_destroy_element(tlb, i, free);
    		 }
    	}
    }
}

//retorna una lista de pares direccion-tamaño que representan accesos a memoria
t_list* obtener_accesos_a_memoria(t_contexto_ejecucion* contexto, int dir_logica, int tamanio) {
	t_list* accesos_a_memoria = list_create();

	int cantidad_accedida = 0;
	while(cantidad_accedida < tamanio) {
        sem_wait(&sem);
		t_mmu* mmu_reg1_fisica = traducir_direccion_de_memoria(dir_logica+cantidad_accedida,contexto,tlb);
        sem_post(&sem);
		int dir_inicio = mmu_reg1_fisica->direccion_fisica;
		free(mmu_reg1_fisica);

		if(dir_inicio < 0) {
			printf("Se recibió un marco o dirección inválida\n");
			break;
		}

		int frame_limite = (dir_inicio / config_cpu->tam_max_pagina) + 1;
		int dir_limite = (frame_limite * config_cpu->tam_max_pagina) - 1;

		//cantidad a acceder en esta iteración
		int cantidad_a_acceder = tamanio - cantidad_accedida;
		int cantidad_limite = (dir_limite - dir_inicio) + 1;
		if(cantidad_a_acceder > cantidad_limite) //no pasar el límite de la página
			cantidad_a_acceder = cantidad_limite;

		t_acceso_memoria* acceso_memoria = malloc(sizeof(t_acceso_memoria));
		acceso_memoria->direccion = dir_inicio;
		acceso_memoria->tamanio = cantidad_a_acceder;
		list_add(accesos_a_memoria, acceso_memoria);

		cantidad_accedida += cantidad_a_acceder;
	}

	return accesos_a_memoria;
}

/*
// ToDo no estan hechas las interfaces! asi que se prueba despues
void copy_string(t_contexto_ejecucion* contexto, char* tamanio){
    int size_to_copy = atoi(tamanio);
    if(size_to_copy < 0){
        perror("Tamaño invalido");
    }
    //copia de una direccion de memoria a otra!
    // de esto se encarga memoria!
    // especie de mov_in - mov_out! 
    
    // peticion 
    t_mmu* logi_si = traducir_direccion_de_memoria(contexto->SI, contexto,tlb);
    t_mmu* logi_di = traducir_direccion_de_memoria(contexto->DI, contexto,tlb);

    //revisar punteros void* -- capaz lo tenga que pasar a char*
    memcpy((char*)logi_si->direccion_fisica, (char*)logi_di->direccion_fisica, size_to_copy);
    
    free(logi_si);
    free(logi_di);
}
*/
void copy_string(t_contexto_ejecucion* contexto, char* tamanio){
    //verifoco si el tamaño no es menor a 0
    int size_to_copy = atoi(tamanio);
    if(size_to_copy < 0){
        perror("Tamaño invalido");
        return;
    }
    //primero necesito las dir logica de c/u
    //tomo la dir logica del SI , del valor
    uint32_t dir_logica_SI = get_valor_registro(contexto, "SI");
    //necesito la dir a donde va a copiar
    uint32_t dir_logica_DI = get_valor_registro(contexto, "DI");

    // buffer para almacenar segun tamaño
    void* buffer = malloc(size_to_copy);
    //debo leer SI, segun cierta cantidad de bytes, y se guarda en buffer
    //lectura_de_memoria(contexto, dir_logica_SI, buffer, size_to_copy);

    //enviar a memoria las direcciones (accesos) de a una
    int cantidad_accedida = 0;
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica_SI, size_to_copy);
    for(int i=0; i<list_size(accesos_a_memoria); i++) {
    	t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);

    	if(!lectura_de_memoria(buffer+cantidad_accedida,
    			contexto->pid,
    			acceso_memoria->direccion,
				acceso_memoria->tamanio,
				true))
    		break;

    	cantidad_accedida += acceso_memoria->tamanio;
    }

    list_destroy_and_destroy_elements(accesos_a_memoria, free);


    //escritura_en_memoria(contexto, dir_logica_DI, buffer, size_to_copy);

    //enviar a memoria las direcciones (accesos) de a una
    cantidad_accedida = 0;
   accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica_DI, size_to_copy);
    for(int i=0; i<list_size(accesos_a_memoria); i++) {
    	t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);

    	if(!escritura_en_memoria(buffer+cantidad_accedida,
				contexto->pid,
				acceso_memoria->direccion,
				acceso_memoria->tamanio,
				true))
    		break;

    	cantidad_accedida += acceso_memoria->tamanio;
    }

    list_destroy_and_destroy_elements(accesos_a_memoria, free);

    free(buffer);
}
/*
void mov_in(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    //necesito la direccion logica del reg2 - registro direccion del cual empezara el dato a ser leido
    uint32_t dir_logica_reg2 = get_valor_registro(contexto,reg2);
    t_mmu* mmu_reg2 = traducir_direccion_de_memoria(dir_logica_reg2,contexto,tlb);
    //ahora necesito el valor de la direccion fisica de esa tradccion 
    //uint32_t valor_reg2 = (uint32_t)mmu_reg2->direccion_fisica;
    //ahora necesito asignar ese valor al registro 1
    int tamanio_reg1 = get_tamanio_registro(reg1);
    //sprintf(valor_resultado_str, "%d", valor_reg2);
    log_info(logger, "valor dir fisica que envia a memoria %d.\n", mmu_reg2->direccion_fisica);
    enviar_pedido_lectura_a_memoria(contexto->pid, mmu_reg2->direccion_fisica, tamanio_reg1);
    op_code cod_op = recibir_operacion(memoria_fd);
    log_info(logger,"el codigo op es: %i: ", cod_op);
    if(cod_op == MEM_STATUS){
        //ToDo esta delegada y usada en mov_out, implemtar aca!!
        // colocar ese leido en registro contx
        //op_code cod_op = recibir_operacion(memoria_fd);
        t_list* status = recibir_paquete(memoria_fd);
        if(*(int*) list_get(status,0) == 0){
            op_code cod_op2 = recibir_operacion(memoria_fd);
                if(cod_op2 == DATOS_DE_MEMORIA){
                    // delegar en una funcion esta parte!
                     t_list* valor_leido = recibir_paquete(memoria_fd);
                      char* valor_resultado_str = malloc(sizeof(char) * 10);
                      sprintf(valor_resultado_str, "%d", *(int*) list_get(valor_leido,0));
                      log_info(logger,"el valor del registro recibido de memoria es %s.\n", valor_resultado_str);
                      setear_registro(contexto,reg1,valor_resultado_str);
                      log_info(logger, "el valor del cx: %d", contexto->CX);
                      list_destroy_and_destroy_elements(valor_leido,free);
                }
        }else{
            log_error(logger,"ocurrio un error a hora de lectuda de datos");
        }
        list_destroy(status);
    }
    
    free(mmu_reg2);

}
void enviar_pedido_lectura_a_memoria( int pid, int dir_fisica,  int tamanio){
    enviar_mov_in_a_memoria(pid, dir_fisica, tamanio, memoria_fd);
}
*/
void mov_in(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    int dir_logica_reg2 = (int)get_valor_registro(contexto,reg2);
    //valor_reg2 (uint32_t) nos dice la dirección donde leer
    //valor_reg1 (uint32_t) es el registro a escribir

    uint32_t valor_reg1=0;
    //lectura_de_memoria(contexto, dir_logica_reg2, &valor_reg1, get_tamanio_registro(reg1));

    //enviar a memoria las direcciones (accesos) de a una
    int cantidad_accedida = 0;
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica_reg2, (int) get_tamanio_registro(reg1));
    for(int i=0; i<list_size(accesos_a_memoria); i++) {
    	t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);

    	if(!lectura_de_memoria(((char *)&valor_reg1)+cantidad_accedida,
    			contexto->pid,
    			acceso_memoria->direccion,
				acceso_memoria->tamanio,
				false))
    		break;

    	cantidad_accedida += acceso_memoria->tamanio;
    }

    setear_registro(contexto, reg1, valor_reg1);

	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}


/*void lectura_de_memoria(t_contexto_ejecucion* contexto, int dir_logica, void *buf, int tamanio) {
	int cantidad_leida = 0; //lo que lleva leyendo

	while(cantidad_leida < tamanio) {
		t_mmu* mmu_reg2_fisica = traducir_direccion_de_memoria(dir_logica+cantidad_leida,contexto,tlb);
		int dir_inicio = mmu_reg2_fisica->direccion_fisica;
		free(mmu_reg2_fisica);

		if(dir_inicio < 0) {
			log_error(logger, "Lectura: Se recibió un marco o dirección inválida");
			break;
		}

		int frame_limite = (dir_inicio / TAMANIO_MAX) + 1;
		int dir_limite = (frame_limite * TAMANIO_MAX) - 1;

		//cantidad a leer en esta iteración
		int cantidad_a_leer = tamanio - cantidad_leida;
		int cantidad_limite = (dir_limite - dir_inicio) + 1;
		if(cantidad_a_leer > cantidad_limite) //no pasar el límite de la página
			cantidad_a_leer = cantidad_limite;

		if(!enviar_pedido_lectura_a_memoria(buf+cantidad_leida,contexto->pid,dir_inicio,cantidad_a_leer)) {
			log_error(logger, "Ocurrió un error a la hora de lectura de datos");
			break;
		}

		cantidad_leida += cantidad_a_leer;
	}
}*/

bool lectura_de_memoria(void *buffer, int pid,int dir_fisica, int tamanio, int is_string) {
    enviar_mov_in_a_memoria(pid, dir_fisica, tamanio, memoria_fd);

    if(recibir_mem_status(memoria_fd) == MEM_EXITO) {
    	op_code opc = recibir_operacion(memoria_fd);
		t_list* valores = recibir_paquete(memoria_fd);
		if(opc == DATOS_DE_MEMORIA) {
			memcpy(buffer, list_get(valores, 0), tamanio);

			if(is_string) {
				char *valor_str = string_new();
				string_n_append(&valor_str, buffer, tamanio);
				log_info(logger, "PID: %d - Acción: Leer - Dirección física: %d - Valor: %s", pid, dir_fisica, valor_str);
				free(valor_str);
			} else {
				int valor_truncado=0;
				memcpy(&valor_truncado, buffer, tamanio);
				log_info(logger, "PID: %d - Acción: Leer - Dirección física: %d - Valor: %d", pid, dir_fisica, valor_truncado);
			}
		}
		else {
			printf("opc (%d) != DATOS_DE_MEMORIA\n", opc);
		}

		list_destroy_and_destroy_elements(valores, free);
		return opc == DATOS_DE_MEMORIA;
    }
    else
    	printf("lectura_de_memoria() - mem_status != MEM_EXITO\n");

    return false;
}

void mov_out(t_contexto_ejecucion* contexto, char* reg1, char* reg2){
    int valor_reg2 = (int) get_valor_registro(contexto,reg2);

    int dir_logica_reg1 = (int)get_valor_registro(contexto,reg1);

    //enviar a memoria las direcciones (accesos) de a una
    int cantidad_accedida = 0;
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica_reg1, get_tamanio_registro(reg2));
    for(int i=0; i<list_size(accesos_a_memoria); i++) {
    	t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);

    	if(!escritura_en_memoria(((char *)&valor_reg2)+cantidad_accedida,
				contexto->pid,
				acceso_memoria->direccion,
				acceso_memoria->tamanio,
				false))
    		break;

    	cantidad_accedida += acceso_memoria->tamanio;
    }

	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}

/*void escritura_en_memoria(t_contexto_ejecucion* contexto, int dir_logica, void *buf, int tamanio) {
	int cantidad_escrita = 0; //lo que lleva escribiendo

	while(cantidad_escrita < tamanio) {
		t_mmu* mmu_reg1_fisica = traducir_direccion_de_memoria(dir_logica+cantidad_escrita,contexto,tlb);
		int dir_inicio = mmu_reg1_fisica->direccion_fisica;
		free(mmu_reg1_fisica);

		if(dir_inicio < 0) {
			log_error(logger, "Escritura: Se recibió un marco o dirección inválida");
			break;
		}

		int frame_limite = (dir_inicio / TAMANIO_MAX) + 1;
		int dir_limite = (frame_limite * TAMANIO_MAX) - 1;

		//cantidad a escribir en esta iteración
		int cantidad_a_escribir = tamanio - cantidad_escrita;
		int cantidad_limite = (dir_limite - dir_inicio) + 1;
		if(cantidad_a_escribir > cantidad_limite) //no pasar el límite de la página
			cantidad_a_escribir = cantidad_limite;


		if(!enviar_valor_escritura_a_memoria(buf+cantidad_escrita,dir_inicio,contexto->pid,cantidad_a_escribir))
			break;

		cantidad_escrita += cantidad_a_escribir;
	//	log_info(logger, "PID: %d - Acción: Escribir - Dirección física: %d - Valor: %d")
	}
}*/

bool escritura_en_memoria(void *buffer, int pid,int dir_fisica, int tamanio, int is_string) {
    enviar_mov_out_a_memoria(buffer,dir_fisica,pid, tamanio, memoria_fd);

    if(recibir_mem_status(memoria_fd) != MEM_EXITO) {
    	printf("escritura_en_memoria() - mem_status != MEM_EXITO\n");
		return false;
    }

    if(is_string) {
		char *valor_str = string_new();
		string_n_append(&valor_str, buffer, tamanio);
		log_info(logger, "PID: %d - Acción: Escribir - Dirección física: %d - Valor: %s", pid, dir_fisica, valor_str);
		free(valor_str);
	} else {
		int valor_truncado=0;
		memcpy(&valor_truncado, buffer, tamanio);
		log_info(logger, "PID: %d - Acción: Escribir - Dirección física: %d - Valor: %d", pid, dir_fisica, valor_truncado);
	}

    return true;
}


int get_tamanio_registro(char* reg){
    if(strcmp(reg, "AX") == 0 || strcmp(reg, "BX") ==0 || strcmp(reg, "CX") ==0 ||strcmp(reg, "DX")==0){
        return 1;
    }
    return 4;
}


void io_stdin_read(t_contexto_ejecucion* contexto,char* nom_interfaz, char* reg2, char* reg3) {
    int dir_logica = (int) get_valor_registro(contexto,reg2);
    int tamanio = (int) get_valor_registro(contexto,reg3);
    
	//enviar a kernel todas las direcciones (accesos) de una
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica, tamanio);
	enviar_stdin_read_kernel(contexto, nom_interfaz, accesos_a_memoria);
	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}


void io_stdin_write(t_contexto_ejecucion* contexto,char* nom_interfaz, char* reg2, char* reg3) {
    int dir_logica = (int) get_valor_registro(contexto,reg2);
    int tamanio = (int) get_valor_registro(contexto,reg3);

    //enviar a kernel todas las direcciones (accesos) de una
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica, tamanio);
	enviar_stdout_write_kernel(contexto, nom_interfaz, accesos_a_memoria);
	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}


void enviar_stdin_read_kernel(t_contexto_ejecucion* contexto, char* nom_interfaz, t_list* lista_accesos_a_memoria){
	enviar_contexto_actualizado(contexto, true);
    enviar_a_kernel_stdin_read(nom_interfaz, lista_accesos_a_memoria, dispatch_fd);
}


void enviar_stdout_write_kernel(t_contexto_ejecucion* contexto, char* nom_interfaz, t_list* lista_accesos_a_memoria){
	enviar_contexto_actualizado(contexto, true);
    enviar_a_kernel_stduot_write(nom_interfaz, lista_accesos_a_memoria, dispatch_fd);
}


void instruction_wait(t_contexto_ejecucion* contexto, char* recurso){
	enviar_contexto_actualizado(contexto, true);
    enviar_wait_kernel(recurso, dispatch_fd);
}
void instruction_signal(t_contexto_ejecucion* contexto, char* recurso){
	enviar_contexto_actualizado(contexto, true);
    enviar_signal_kernel(recurso,dispatch_fd);
}
void io_fs_create(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo){
	enviar_contexto_actualizado(contexto, true);
    enviar_io_fs_create_a_kernel(interfaz, nom_archivo,dispatch_fd);
}
void io_fs_delete(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo){
	enviar_contexto_actualizado(contexto, true);
    enviar_io_fs_delete_a_kernel(interfaz,nom_archivo,dispatch_fd);
}
void io_fs_truncate(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo, char* reg3){
	enviar_contexto_actualizado(contexto, true);

    uint32_t tamanio_reg3 = get_valor_registro(contexto,reg3);
    enviar_io_fs_truncate_a_kernel(interfaz,nom_archivo,tamanio_reg3,dispatch_fd);
}
void io_fs_write(t_contexto_ejecucion* contexto, char* interfaz,char* nom_archivo, char* reg3, char* reg4, char* reg5){
    enviar_contexto_actualizado(contexto, true);
	int dir_logica = (int) get_valor_registro(contexto,reg3);
    t_mmu* mmu_reg3 = traducir_direccion_de_memoria(dir_logica,contexto,tlb);
    free(mmu_reg3);

    uint32_t tamanio_reg4 = get_valor_registro(contexto,reg4);
    uint32_t tamanio_reg5 = get_valor_registro(contexto,reg5);


    //enviar a kernel las direcciones (accesos) de a una
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica, (int) tamanio_reg4);
    //for(int i=0; i<list_size(accesos_a_memoria); i++) {
    //	t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);
    //	enviar_io_fs_write_a_kernel(interfaz, nom_archivo, acceso_memoria->direccion, acceso_memoria->tamanio,tamanio_reg5,dispatch_fd);
    //}
    enviar_io_fs_write_a_kernel(interfaz, nom_archivo, accesos_a_memoria, tamanio_reg5, dispatch_fd);

	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}

void io_fs_read(t_contexto_ejecucion* contexto, char* interfaz,char* nom_archivo, char* reg3, char* reg4, char* reg5){

    enviar_contexto_actualizado(contexto, true);
	int dir_logica = (int) get_valor_registro(contexto,reg3);
    t_mmu* mmu_reg3 = traducir_direccion_de_memoria(dir_logica,contexto,tlb);
    free(mmu_reg3);

    uint32_t tamanio_reg4 = get_valor_registro(contexto,reg4);
    uint32_t tamanio_reg5 = get_valor_registro(contexto,reg5);


    //enviar a kernel las direcciones (accesos) de a una
    t_list* accesos_a_memoria = obtener_accesos_a_memoria(contexto, dir_logica, (int) tamanio_reg4);
   /* for(int i=0; i<list_size(accesos_a_memoria); i++) {
        enviar_contexto_actualizado(contexto, true);
        t_acceso_memoria* acceso_memoria = list_get(accesos_a_memoria, i);
        enviar_io_fs_read_a_kernel(interfaz, nom_archivo, acceso_memoria->direccion, acceso_memoria->tamanio,tamanio_reg5,dispatch_fd);
    }*/
    enviar_io_fs_read_a_kernel(interfaz, nom_archivo, accesos_a_memoria, tamanio_reg5,dispatch_fd);
	list_destroy_and_destroy_elements(accesos_a_memoria, free);
}


/*******************/
/****** FREEs ******/
/*******************/
void free_logger() {
    log_destroy(logger);
}

void free_config() {
    free(config_cpu->ip_memoria);
    free(config_cpu->puerto_memoria);
    free(config_cpu->puerto_escucha_dispatch); 
    free(config_cpu->puerto_escucha_interrupt);
    free(config_cpu->algoritmo_tlb);
    free(config_cpu);
    config_destroy(config);
}

void free_conexiones() {
    liberar_conexion(memoria_fd);
    liberar_conexion(socket_cpu_dispatch);
    liberar_conexion(socket_cpu_interrupt);
}

void liberar_instruccion(t_instruccion* instruccion){
    free(instruccion->nombre_ins);
    list_destroy_and_destroy_elements(instruccion->parametros, free);
    free(instruccion);
}
