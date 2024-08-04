#include "memoria.h"

//variables globales propias de Memoria
t_log* logger;
t_config_data* config;
t_list* g_descriptor_proceso_list;
sem_t contador_iniciar_proceso[MAX_PROC];

char *g_nombre_config;

void *g_espacio;
t_bitarray* g_frames_bitarray;
sem_t mx_memoria;
pthread_mutex_t mutex_bitarray;
pthread_mutex_t mutex_descriptor_list;

int main(int argc, char* argv[]) {

	g_nombre_config = argv[1];
	if (!iniciar_variables_memoria())
		return EXIT_FAILURE;
    
    //creamos el socket que va a esperar clientes
    int socket_servidor = iniciar_servidor(IP_MEMORIA, config->puerto_escucha);

    while (1) {
    	int conexion = esperar_cliente(socket_servidor);
    	printear(PRINT_INFO, "Se conectó un cliente, atendiendo conexión..");
    	crear_hilo_para_conexion(&procesar_conexion, conexion);
	}

    //log_info(logger, "Terminando módulo Memoria...");

    liberar_variables_memoria();

    return EXIT_SUCCESS;
}

t_log* iniciar_logger()
{
	t_log* nuevo_logger;
	nuevo_logger = log_create("./cfg/memoria.log", "Memoria", true, LOG_LEVEL_INFO);

	return nuevo_logger;
}

t_config_data* iniciar_config()
{
	t_config_data* config = malloc(sizeof(t_config_data));
	char *path = string_from_format("%s%s%s", "./cfg/", g_nombre_config, ".config");
	//printf("path: %s\n",path);

	config->tad = config_create(path);
	free(path);

	if (config->tad == NULL)
		return NULL;

	config->puerto_escucha = config_get_string_value(config->tad, "PUERTO_ESCUCHA");
    config->tam_memoria = config_get_int_value(config->tad, "TAM_MEMORIA");
    config->tam_pagina = config_get_int_value(config->tad, "TAM_PAGINA");
    config->path_instrucciones = config_get_string_value(config->tad, "PATH_INSTRUCCIONES");
    config->retardo_respuesta = config_get_int_value(config->tad, "RETARDO_RESPUESTA");

	return config;
}

bool iniciar_variables_memoria(void) {
	logger = iniciar_logger();
	if (logger == NULL) {
		printear(PRINT_ERROR, "logger == NULL\n");
		return false;
	}

	config = iniciar_config();
	if (config == NULL) {
		printear(PRINT_ERROR, "config == NULL");
		return false;
	}

	g_descriptor_proceso_list = list_create();

	//el espacio contiguo de memoria
	g_espacio = malloc(config->tam_memoria);

	size_t cantidad_frames = config->tam_memoria / config->tam_pagina;
	char* frames_bits = malloc(cantidad_frames / 8);
	memset(frames_bits, 0, cantidad_frames / 8);
	g_frames_bitarray = bitarray_create_with_mode(frames_bits, cantidad_frames / 8, LSB_FIRST);

	for (int i = 0; i < MAX_PROC; ++i)
		sem_init(&(contador_iniciar_proceso[i]), 0, 0);

	pthread_mutex_init(&mutex_bitarray, NULL);
	pthread_mutex_init(&mutex_descriptor_list, NULL);

	return true;
}

void liberar_variables_memoria(void) { //<-- necesario?
	/* FixMe: usar algo como 'free_descriptor' en lugar 'free' */
	list_destroy_and_destroy_elements(g_descriptor_proceso_list, free);

	free(g_espacio);
	free(g_frames_bitarray->bitarray);
	bitarray_destroy(g_frames_bitarray);

	config_destroy(config->tad);

	free(config);
	log_destroy(logger);
}

void crear_hilo_para_conexion(void *(*fn)(void *), int conexion) {
	void *arg = malloc(sizeof(int));
	*(int *) arg = conexion;

	pthread_t thread;
	if (pthread_create(&thread, NULL, fn, arg) != 0) {
		printear(PRINT_ERROR, "pthread_create fail");
		free(arg);
	}

	return;
}

void* procesar_conexion(void *arg) {
	int conexion = *(int *) arg;

	op_code operacion;
	while ((operacion = recibir_operacion(conexion)) != -1) {
		t_list* valores = recibir_paquete(conexion);
		switch (operacion) {
		/*el Kernel me solicita crear un proceso en memoria*/
		case PETICION_INICIAR_PROCESO:
		{
			int pid = *(int *) list_get(valores, 0);
			char *nombre = (char *) list_get(valores, 1);
			atender_peticion_crear_proceso(conexion, pid, nombre);
			sem_post(&(contador_iniciar_proceso[pid]));
			break;
		}

		/*la CPU me pide una línea del archivo de instrucciones*/
		case PEDIR_INSTRUCCION:
		{
			int pid = *(int *) list_get(valores, 0);
			int nrolinea = *(int *) list_get(valores, 1);
			sem_wait(&(contador_iniciar_proceso[pid]));
			atender_pedido_de_instruccion(conexion, pid, nrolinea);
			sem_post(&(contador_iniciar_proceso[pid]));
			break;
		}

		/*el Kernel me solicita que libere un proceso de Memoria*/
		case PETICION_LIBERAR_PROCESO:
		{
			int pid = *(int *) list_get(valores, 0);
			atender_peticion_liberar_proceso(conexion, pid);
			//printf("\nDestruyendo un sem para el pid: %d\n", pid);
			sem_destroy(&(contador_iniciar_proceso[pid]));
			break;
		}

		/*la CPU o una interfaz de IO me pide que lea de memoria*/
		case LEER_DE_MEMORIA:
		{
			int pid = *(int *) list_get(valores, 0);
			int direccion = *(int *) list_get(valores, 1);
			int tamanio = *(int *) list_get(valores, 2);
			atender_lectura_de_memoria(conexion, pid, direccion, tamanio);
			break;
		}

		/*la CPU o una interfaz de IO me pide que escriba en memoria*/
		case ESCRIBIR_EN_MEMORIA:
		{
			
			int pid = *(int *) list_get(valores, 0);
			int direccion = *(int *) list_get(valores, 1);
			char *buffer = (char *) list_get(valores, 2);
			int tamanio = *(int *) list_get(valores, 3);
			atender_escritura_en_memoria(conexion, pid, direccion, buffer, tamanio);
			break;
		}

		/*la CPU me pide que reserve o remueva páginas de un proceso*/
		case RESIZE_PROCESO:
		{
			int pid = *(int *) list_get(valores, 0);
			int nuevo_tamanio = *(int *) list_get(valores, 1);
			atender_resize_proceso(conexion, pid, nuevo_tamanio);
			break;
		}

		/*la CPU me da un pid y pagina_id, le devuelvo el marco de esa página para ese proceso*/
		case ACCESO_A_TABLA_DE_PAGINAS:
		{
			int pid = *(int *) list_get(valores, 0);
			int pagina_id = *(int *) list_get(valores, 1);
			atender_acceso_a_tabla_de_paginas(conexion, pid, pagina_id);
			break;
		}

		default:
			printear(PRINT_WARNING, "Operación %d no válida...", operacion);
			break;
		}

		list_destroy_and_destroy_elements(valores, free);

		//aplicar el retardo para la próxima petición
		usleep((unsigned int) (config->retardo_respuesta * 1000));

	}
	printear(PRINT_INFO, "recibir_operacion == -1, terminando conexión %d.", conexion);

	free(arg);
	close(conexion);
	pthread_exit(NULL);
}


void atender_peticion_crear_proceso(int conexion, int pid, char* nombre) {
	//concatenar la ruta con el nombre de archivo recibido
	char *path = m_strcat(config->path_instrucciones, nombre);

	if (crear_descriptor_de_proceso(pid, path) == NULL)
		enviar_status_crear_proceso(conexion, 0);
	else
		enviar_status_crear_proceso(conexion, 1);

	free(path);
}


void atender_pedido_de_instruccion(int conexion, int pid, int nrolinea) {
	char *instruccion = obtener_instruccion(pid, nrolinea);

	printear(PRINT_INFO, "%d - %s", nrolinea, instruccion);

	if (instruccion != NULL) {
		enviar_op_status(conexion, MEM_EXITO);
		enviar_instruccion(conexion, instruccion);
	}
	else {
		//instrucción no encontrada
		enviar_op_status(conexion, MEM_ERROR);
	}
}


void atender_peticion_liberar_proceso(int conexion, int pid) {
	t_descriptor_proceso* proceso = buscar_descriptor_por_pid(pid);
	if (proceso == NULL) {
		log_error(logger, "Error: pid: %d no encontrado", pid);
		enviar_op_status(conexion, MEM_PID_INVALIDO);
	}
	else {
		liberar_descriptor_de_proceso(proceso);
		enviar_op_status(conexion, MEM_EXITO);
	}
}


void atender_lectura_de_memoria(int conexion, int pid, int direccion, int tamanio) {
	printear(PRINT_INFO, "Lectura de memoria - pid %d - direccion %d - tamaño %d", pid, direccion, tamanio);

	t_descriptor_proceso* proceso = buscar_descriptor_por_pid(pid);
	if (proceso == NULL) {
		printear(PRINT_ERROR, "Error: proceso %d no encontrado", pid);
		enviar_op_status(conexion, MEM_PID_INVALIDO);
	}
	else {
		void *buffer = malloc(tamanio);

		if (leer_de_proceso(proceso, direccion, buffer, tamanio)) {
			enviar_op_status(conexion, MEM_EXITO);
			enviar_datos_de_memoria(conexion, buffer, tamanio);
		}
		else {
			enviar_op_status(conexion, MEM_DIR_INVALIDA);
		}

		free(buffer);
	}
}


void atender_escritura_en_memoria(int conexion, int pid, int direccion, char *buffer, int tamanio) {
	printear(PRINT_INFO, "Escritura de memoria - pid %d - direccion %d buffer %X - tamaño %d", pid, direccion, buffer, tamanio);

	t_descriptor_proceso* proceso = buscar_descriptor_por_pid(pid);
	if (proceso == NULL) {
		printear(PRINT_ERROR, "Error: proceso %d no encontrado", pid);
		enviar_op_status(conexion, MEM_PID_INVALIDO);
	}
	else {
		if (escribir_en_proceso(proceso, direccion, buffer, tamanio))
			enviar_op_status(conexion, MEM_EXITO);
		else
			enviar_op_status(conexion, MEM_DIR_INVALIDA);
	}
}


void atender_resize_proceso(int conexion, int pid, int nuevo_tamanio) {
	t_descriptor_proceso* proceso = buscar_descriptor_por_pid(pid);
	if (proceso == NULL) {
		printear(PRINT_ERROR, "Error: proceso %d no encontrado", pid);
		enviar_op_status(conexion, MEM_PID_INVALIDO);
	}
	else {
		int tamanio_actual = tamanio_en_memoria(proceso);

		if (nuevo_tamanio < tamanio_actual) {
			//reducir el proceso hasta 'nuevo_tamanio'
			if (reduccion_de_proceso(proceso, nuevo_tamanio))
				enviar_op_status(conexion, MEM_EXITO);
			else
				enviar_op_status(conexion, MEM_ERROR);
		}
		else {
			//ampliar el proceso hasta 'nuevo_tamanio'
			if (ampliacion_de_proceso(proceso, nuevo_tamanio))
				enviar_op_status(conexion, MEM_EXITO);
			else
				enviar_op_status(conexion, MEM_OUT_OF_MEMORY);
		}
	}
}


void atender_acceso_a_tabla_de_paginas(int conexion, int pid, int pagina_id) {
	t_descriptor_proceso* proceso = buscar_descriptor_por_pid(pid);
	if (proceso == NULL) {
		printear(PRINT_ERROR, "Error: proceso %d no encontrado", pid);
		enviar_op_status(conexion, MEM_PID_INVALIDO);
	}
	else {
		enviar_op_status(conexion, MEM_EXITO);
		enviar_numero_de_marco(conexion, marco_de_pagina(proceso, pagina_id));
	}
}


t_descriptor_proceso* crear_descriptor_de_proceso(int pid, char *path) {
	//TEST: ver qué se está abriendo
	printear(PRINT_INFO, "Estoy por abrir el archivo: %s  PID: %d", path, pid);
	t_list* instrucciones = generar_lista_de_instrucciones(path);
	if (instrucciones == NULL)
		return NULL;

	t_descriptor_proceso* descriptor_proceso = malloc(sizeof(t_descriptor_proceso));
	descriptor_proceso->pid = pid;
	descriptor_proceso->path = strdup(path);
	descriptor_proceso->instrucciones = instrucciones;

	crear_tabla_de_paginas(descriptor_proceso);

	//agregamos el descriptor creado a la lista global
	pthread_mutex_lock(&mutex_descriptor_list);
	list_add(g_descriptor_proceso_list, descriptor_proceso);
	pthread_mutex_unlock(&mutex_descriptor_list);

	return descriptor_proceso;
}


t_list* generar_lista_de_instrucciones(char *path) {
	FILE *fp = fopen(path, "r");

	if (fp == NULL) {
		printear(PRINT_ERROR, "fp == NULL");
		return NULL;
	}

	//esta lista contendrá cada renglón del archivo de pseudocódigo
	t_list* instrucciones = list_create();

	char *linea = NULL;
	while (getline(&linea, &(size_t) {0}, fp) > 0) {
		//remover el '\n' del string leído si tiene
		linea[strcspn(linea, "\n")] = '\0';

		//agregar a la lista la referencia al string
		list_add(instrucciones, linea);

		linea = NULL;
	}

	free(linea);

	//TEST: printear todas las lineas
	//for (int i=0; i<list_size(instrucciones); i++)
		//printear(PRINT_INFO, "%d - %s", i, (char *) list_get(instrucciones, i));

	fclose(fp);

	return instrucciones;
}


char* obtener_instruccion(int pid, int nrolinea) {
	t_descriptor_proceso* descriptor_proceso = buscar_descriptor_por_pid(pid);
	if (descriptor_proceso == NULL) {
		printear(PRINT_ERROR, "descriptor_proceso == NULL - pid: %d", pid);
		return NULL;
	}

	if (list_size(descriptor_proceso->instrucciones) < nrolinea + 1)
		return NULL;

	return (char *) list_get(descriptor_proceso->instrucciones, nrolinea);
}


t_descriptor_proceso* buscar_descriptor_por_pid(int pid) {
	t_descriptor_proceso* proc = NULL;
	pthread_mutex_lock(&mutex_descriptor_list);
	for (int i = 0; i < list_size(g_descriptor_proceso_list); i++) {
		t_descriptor_proceso* proceso = list_get(g_descriptor_proceso_list, i);
		if (proceso->pid == pid) {
			proc = proceso;
			break;
		}
	}
	pthread_mutex_unlock(&mutex_descriptor_list);

	return proc;
}


void liberar_descriptor_de_proceso(t_descriptor_proceso* descriptor_proceso) {
	printear(PRINT_INFO, "FINALIZANDO pid: %d - path: %s", descriptor_proceso->pid, descriptor_proceso->path);

	free(descriptor_proceso->path);
	list_destroy_and_destroy_elements(descriptor_proceso->instrucciones, free);

	liberar_tabla_de_paginas(descriptor_proceso);

	list_remove_element(g_descriptor_proceso_list, descriptor_proceso);

	free(descriptor_proceso);
}


void crear_tabla_de_paginas(t_descriptor_proceso* proceso) {
	log_info(logger, "PID: %d - Tamaño: %d", proceso->pid, 0);

	proceso->tabla_de_paginas = list_create();
}


void liberar_tabla_de_paginas(t_descriptor_proceso* proceso) {
	log_info(logger, "PID: %d - Tamaño: %d", proceso->pid, cantidad_paginas_de_proceso(proceso));

	//setear los marcos que ocupaban como disponibles
	for (int id = 0; id < cantidad_paginas_de_proceso(proceso); id++) {
		t_pagina* pagina = list_get(proceso->tabla_de_paginas, id);

		setear_marco_disponible(pagina->marco);
	}

	list_destroy_and_destroy_elements(proceso->tabla_de_paginas, free);
}


bool leer_de_proceso(t_descriptor_proceso* proceso, int direccion, void *buffer, int cantidad) {
	log_info(logger, "PID: %d - Accion: LEER - Direccion fisica: %d - Tamaño: %d", proceso->pid, direccion, cantidad);

	t_pagina* pagina = pagina_de_direccion(proceso, direccion);
	if (pagina == NULL) {
		printear(PRINT_ERROR, "Error: la dirección %dd no es válida para pid: %d", direccion, proceso->pid);
		return false;
	}

	int cantidad_maxima = ((pagina->marco + 1) * config->tam_pagina) - direccion;
	if (cantidad > cantidad_maxima) {
		printear(PRINT_WARNING, "Se intentó leer %d bytes de memoria que no es del proceso %d",
				cantidad - cantidad_maxima, proceso->pid);

		cantidad = cantidad_maxima;
	}
	
	memcpy(buffer, g_espacio + direccion, (size_t) cantidad);
	return true;
}

//obtener la página que contiene a esta direccion
t_pagina* pagina_de_direccion(t_descriptor_proceso* proceso, int direccion) {
	for (int id = 0; id < cantidad_paginas_de_proceso(proceso); id++) {
		t_pagina* pagina = list_get(proceso->tabla_de_paginas, id);
		if (pagina->marco == direccion / config->tam_pagina)
			return pagina;
	}

	return NULL;
}


bool escribir_en_proceso(t_descriptor_proceso* proceso, int direccion, void *buffer, int cantidad) {
	log_info(logger, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d - Tamaño: %d", proceso->pid, direccion, cantidad);

	t_pagina* pagina = pagina_de_direccion(proceso, direccion);
	if (pagina == NULL) {
		printear(PRINT_ERROR, "Error: la dirección %dd no es válida para pid: %d", direccion, proceso->pid);
		return false;
	}
	
	int cantidad_maxima = ((pagina->marco + 1) * config->tam_pagina) - direccion;
	if (cantidad > cantidad_maxima) {
		printear(PRINT_WARNING, "Se intentó escribir %d bytes en memoria que no es del proceso %d",
				cantidad - cantidad_maxima, proceso->pid);

		cantidad = cantidad_maxima;
	}

	memcpy(g_espacio + direccion, buffer, (size_t) cantidad);
	return true;
}


bool reduccion_de_proceso(t_descriptor_proceso* proceso, int nuevo_tamanio) {
	int tamanio_actual = tamanio_en_memoria(proceso);
	log_info(logger, "PID: %d - Tamaño Actual: %d - Tamaño a Reducir: %d", proceso->pid,
			tamanio_actual, tamanio_actual - nuevo_tamanio);

	//cantidad de páginas a remover del proceso
	int cantidad_paginas = cantidad_paginas_de_proceso(proceso) - (int) ceil(nuevo_tamanio / (float) config->tam_pagina);
	if (cantidad_paginas_de_proceso(proceso) - cantidad_paginas < 0) {
		printear(PRINT_ERROR, "Se intentó remover %d páginas de pid: %d pero tiene %d",
				cantidad_paginas, proceso->pid, cantidad_paginas_de_proceso(proceso));

		return false;
	}

	remover_paginas_para(proceso, cantidad_paginas);
	return true;
}


bool ampliacion_de_proceso(t_descriptor_proceso* proceso, int nuevo_tamanio) {
	int tamanio_actual = tamanio_en_memoria(proceso);
	log_info(logger, "PID: %d - Tamaño Actual: %d - Tamaño a Ampliar: %d", proceso->pid,
			tamanio_actual, nuevo_tamanio - tamanio_actual);

	//cantidad de páginas a agregar al proceso
	int cantidad_paginas = (int) ceil(nuevo_tamanio / (float) config->tam_pagina) - cantidad_paginas_de_proceso(proceso);
	if (cantidad_marcos_libres() < cantidad_paginas) {
		printear(PRINT_ERROR, "Se intentó agregar %d páginas para pid: %d pero hay %d marcos disponibles",
				cantidad_paginas, proceso->pid, cantidad_marcos_libres());

		return false;
	}

	reservar_paginas_para(proceso, cantidad_paginas);
	return true;
}


int cantidad_paginas_de_proceso(t_descriptor_proceso* proceso) {
	return list_size(proceso->tabla_de_paginas);
}

int tamanio_en_memoria(t_descriptor_proceso* proceso) {
	return cantidad_paginas_de_proceso(proceso) * config->tam_pagina;
}

void reservar_paginas_para(t_descriptor_proceso* proceso, int cantidad) {
	printear(PRINT_INFO, "Reservando %d páginas para pid: %d", cantidad, proceso->pid);
	for (int i = 0; i < cantidad; i++) {
		t_pagina* pagina = malloc(sizeof(t_pagina));
		//pagina->id = list_size(proceso->tabla_de_paginas);
		pagina->marco = encontrar_marco_libre();

		//agregar la referencia en la tabla
		list_add(proceso->tabla_de_paginas, pagina);

		//el marco estará ocupado
		setear_marco_ocupado(pagina->marco);
	}
}

void remover_paginas_para(t_descriptor_proceso* proceso, int cantidad) {
	printear(PRINT_INFO, "Removiendo %d páginas de pid: %d", cantidad, proceso->pid);
	for (int i = 0; i < cantidad; i++) {
		//obtener el último id en la tabla de páginas
		int id_ultima_pagina = cantidad_paginas_de_proceso(proceso) - 1;

		//remover la referencia de la lista
		t_pagina *pagina = list_remove(proceso->tabla_de_paginas, id_ultima_pagina);

		//setear el marco como disponible
		setear_marco_disponible(pagina->marco);

		free(pagina);
	}
}

int encontrar_marco_libre() {
	int marco = -1;

	pthread_mutex_lock(&mutex_bitarray);
	for (int i = 0; i < bitarray_get_max_bit(g_frames_bitarray); i++) {
		if (bitarray_test_bit(g_frames_bitarray, i) == 0) {
			marco = i;
			break;
		}
	}
	pthread_mutex_unlock(&mutex_bitarray);

	return marco;
}

int cantidad_marcos_libres() {
	int contador = 0;
	pthread_mutex_lock(&mutex_bitarray);
	for (int i = 0; i < bitarray_get_max_bit(g_frames_bitarray); i++) {
		if (bitarray_test_bit(g_frames_bitarray, i) == 0)
			contador++;
	}
	pthread_mutex_unlock(&mutex_bitarray);

	return contador;
}

void setear_marco_ocupado(int id) {
	pthread_mutex_lock(&mutex_bitarray);
	bitarray_set_bit(g_frames_bitarray, id);
	pthread_mutex_unlock(&mutex_bitarray);
}

void setear_marco_disponible(int id) {
	pthread_mutex_lock(&mutex_bitarray);
	bitarray_clean_bit(g_frames_bitarray, id);
	pthread_mutex_unlock(&mutex_bitarray);
}

int marco_de_pagina(t_descriptor_proceso* proceso, int pagina_id) {
	//devolver -1 si el id de página es inválido
	if (cantidad_paginas_de_proceso(proceso) < pagina_id + 1)
		return -1;

	t_pagina *pagina = list_get(proceso->tabla_de_paginas, pagina_id);
	log_info(logger, "PID %d - Página %d - Marco %d", proceso->pid, pagina_id, pagina->marco);

	return pagina->marco;
}


void enviar_instruccion(int conexion, char *instruccion) {
	t_paquete* paquete = crear_paquete(INSTRUCCION);
	agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_status_crear_proceso(int conexion, mem_status status) {
	t_paquete* paquete = crear_paquete(CREAR_PROCESO_STATUS);
	agregar_a_paquete(paquete, &status, sizeof(int));
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_datos_de_memoria(int conexion, void *buffer, size_t len) {
	t_paquete* paquete = crear_paquete(DATOS_DE_MEMORIA);
	agregar_a_paquete(paquete, buffer, len);
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_op_status(int conexion, mem_status status) {
	t_paquete* paquete = crear_paquete(MEM_STATUS);
	agregar_a_paquete(paquete, &status, sizeof(int));
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void enviar_numero_de_marco(int conexion, int marco) {
	t_paquete* paquete = crear_paquete(NUMERO_DE_MARCO);
	agregar_a_paquete(paquete, &marco, sizeof(int));
	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}


void printear(printear_type type, char *template, ...) {
	static char *reset_color = "\x1b[0m";
	static char *print_color[3] = {"\x1b[0m", "\x1b[33m", "\x1b[31m"};

	size_t n = snprintf(NULL, 0, "%s%s%s\n", print_color[type], template, reset_color);
	char *s = malloc(n + 1);
	snprintf(s, n + 1, "%s%s%s\n", print_color[type], template, reset_color);

	va_list ap;
	va_start(ap, template);
	vprintf(s, ap);
	fflush(stdout);
	va_end(ap);

	free(s);
}

char* m_strcat(char *to, char *from) {
	char *s = malloc(strlen(to) + strlen(from) + 1);
	return strcat(strcpy(s, to), from);
}
