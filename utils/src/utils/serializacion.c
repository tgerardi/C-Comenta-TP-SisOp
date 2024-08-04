#include <utils/serializacion.h>

/***********************************************************************************/
/**************************** SEND & SERIALIZACION *********************************/
/***********************************************************************************/


void enviar_contexto_ejecucion(t_contexto_ejecucion* contexto_ejecucion, int fd_modulo) {
	t_paquete* paquete = crear_paquete(CONTEXTO_EJECUCION);
	serializar_contexto_ejecucion(paquete, contexto_ejecucion);
	enviar_paquete(paquete, fd_modulo);
	eliminar_paquete(paquete);
}

void serializar_contexto_ejecucion(t_paquete* paquete, t_contexto_ejecucion* contexto_ejecucion) {
	agregar_a_paquete(paquete, &(contexto_ejecucion->pid), sizeof(int));
	agregar_a_paquete(paquete, &(contexto_ejecucion->program_counter), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->motivo_exit), sizeof(t_motivo_exit));
	agregar_a_paquete(paquete, &(contexto_ejecucion->AX), sizeof(uint8_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->BX), sizeof(uint8_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->CX), sizeof(uint8_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->DX), sizeof(uint8_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->EAX), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->EBX), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->ECX), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->EDX), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->SI), sizeof(uint32_t));
	agregar_a_paquete(paquete, &(contexto_ejecucion->DI), sizeof(uint32_t));
}

void enviar_pid_pc_memoria(t_paquete_a_memoria* pid_pc, int fd_modulo) {
	t_paquete* paquete = crear_paquete(PEDIR_INSTRUCCION);
	serializar_pid_pc(paquete, pid_pc);
	enviar_paquete(paquete, fd_modulo);
	free_pid_pc(pid_pc); 
	eliminar_paquete(paquete); 
}
void serializar_pid_pc(t_paquete* paquete, t_paquete_a_memoria* pid_pc) {
	agregar_a_paquete(paquete, &(pid_pc->pid), sizeof(int));
	agregar_a_paquete(paquete, &(pid_pc->program_counter), sizeof(uint32_t));
	return;
}

char* enviar_peticion_memoria_direcciones(int direccion_fisicaWrite, int tamanio_direccionWrite, int pidIOWrite, int fd)
{
	t_paquete* paquete = crear_paquete(LEER_DE_MEMORIA); 
	agregar_a_paquete(paquete, &pidIOWrite, sizeof(int));
	agregar_a_paquete(paquete, &direccion_fisicaWrite, sizeof(int));
	agregar_a_paquete(paquete, &tamanio_direccionWrite, sizeof(int));
	enviar_paquete(paquete, fd);
	mem_status status = recibir_mem_status(fd);

	op_code cod_op;
	char* datosBloque = malloc(tamanio_direccionWrite);
    cod_op = recibir_operacion(fd);
	    if(cod_op == DATOS_DE_MEMORIA){
		t_list* paqueteRecibido = recibir_paquete(fd);
		memcpy(datosBloque, (char*)list_get(paqueteRecibido, 0), tamanio_direccionWrite);
		list_destroy_and_destroy_elements(paqueteRecibido, free);
    }
	eliminar_paquete(paquete);

	return datosBloque;
}

void guardar_en_direcciones(char* texto, int dir, int tam, int pid, int fd_memoria){
	t_paquete* paquete = crear_paquete(ESCRIBIR_EN_MEMORIA);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, &dir, sizeof(int));
	agregar_a_paquete(paquete, texto, strlen(texto)+1);
	agregar_a_paquete(paquete, &tam, sizeof(int));
	enviar_paquete(paquete,fd_memoria);
	eliminar_paquete(paquete);
}

void enviar_peticion_memoria(t_list* peticion, int fd) {
	t_paquete* paquete = crear_paquete(PETICION_INICIAR_PROCESO); 
	serializar_peticion_memoria(paquete, peticion);
	enviar_paquete(paquete, fd);
	eliminar_paquete(paquete);
}

void serializar_peticion_memoria(t_paquete* paquete, t_list* peticion) {
	int pid = (int)list_get(peticion,0);
	char* path = (char*)list_get(peticion,1);
	printf("PID ENVIADO: %d", pid);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, path, strlen(path)+1);
}

void enviar_io_gen_sleep(char* interfaz, int cantidad, int fd_modulo) {
	t_paquete* paquete = crear_paquete(IO_SLEEP);
	agregar_a_paquete(paquete, interfaz, strlen(interfaz)+1);
	agregar_a_paquete(paquete, &cantidad, sizeof(int));
	enviar_paquete(paquete, fd_modulo);
	eliminar_paquete(paquete); 
}

void enviarFinInstruccion(int fd){
	t_paquete* paquete = crear_paquete(FIN_INTERFAZ); 
	enviar_paquete(paquete, fd);
	eliminar_paquete(paquete);
}

void enviar_resize_a_memoria(int pid, int tamanio_memoria, int fd_memoria){
	t_paquete* paquete = crear_paquete(RESIZE_PROCESO);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, &tamanio_memoria, sizeof(int));
	enviar_paquete(paquete, fd_memoria);
	eliminar_paquete(paquete); 

}

void enviar_pid_nro_pagina_a_memoria(int pagina, int pid, int fd_memoria){
	t_paquete* paquete = crear_paquete(ACCESO_A_TABLA_DE_PAGINAS);
	agregar_a_paquete(paquete,&(pid), sizeof(int));
	agregar_a_paquete(paquete,&(pagina), sizeof(int));
	enviar_paquete(paquete,fd_memoria);
	eliminar_paquete(paquete);
}
void enviar_mov_out_a_memoria(char* valor_reg2, int direccion_fisica, int pid, int tamanio, int fd_memoria){
	t_paquete* paquete = crear_paquete(ESCRIBIR_EN_MEMORIA);
	//agregar a paquete el pid, dir fisica, buffer de datos
	// el tamaño de la escritura! como parametro tambien!
	agregar_a_paquete(paquete,&(pid), sizeof(int));
	agregar_a_paquete(paquete,&(direccion_fisica), sizeof(int));
	agregar_a_paquete(paquete,valor_reg2, tamanio);
	agregar_a_paquete(paquete,&(tamanio), sizeof(int));
	enviar_paquete(paquete,fd_memoria);
	eliminar_paquete(paquete);
}
void enviar_mov_in_a_memoria(int pid, int dir_fisica, int tamanio, int memoria_fd ){
	t_paquete* paquete = crear_paquete(LEER_DE_MEMORIA);
	agregar_a_paquete(paquete,&(pid), sizeof(int));
	agregar_a_paquete(paquete,&(dir_fisica), sizeof(int));
	agregar_a_paquete(paquete,&(tamanio), sizeof(int));
	enviar_paquete(paquete,memoria_fd);
	eliminar_paquete(paquete);
}

void enviar_a_kernel_stdin_read(char* nombre_interfaz, t_list* lista_direcciones, int dispatch_fd){
	t_paquete* paquete = crear_paquete(SOLICITAR_IO_STDIN_READ);
	agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz)+1);

	int cantidad_direcciones = list_size(lista_direcciones);
	agregar_a_paquete(paquete, &cantidad_direcciones, sizeof(int));

	for(int i=0; i<cantidad_direcciones; i++) {
		t_acceso_memoria* acceso_memoria = list_get(lista_direcciones, i);
		agregar_a_paquete(paquete, &(acceso_memoria->direccion), sizeof(int));
		agregar_a_paquete(paquete, &(acceso_memoria->tamanio), sizeof(int));
	}

	enviar_paquete(paquete,dispatch_fd);
	eliminar_paquete(paquete);
}

void enviar_a_kernel_stduot_write(char* nombre_interfaz, t_list* lista_direcciones, int dispatch_fd){
	t_paquete* paquete = crear_paquete(SOLICITAR_IO_STDOUT_WRITE);
	agregar_a_paquete(paquete, nombre_interfaz, strlen(nombre_interfaz)+1);

	int cantidad_direcciones = list_size(lista_direcciones);
	agregar_a_paquete(paquete, &cantidad_direcciones, sizeof(int));

	t_acceso_memoria* acceso_memoria = malloc(sizeof(t_acceso_memoria));
	for(int i=0; i<cantidad_direcciones; i++) {
		acceso_memoria = (t_acceso_memoria*) list_get(lista_direcciones, i);
		agregar_a_paquete(paquete, &(acceso_memoria->direccion), sizeof(uint32_t));
		agregar_a_paquete(paquete, &(acceso_memoria->tamanio), sizeof(uint32_t));
	}


	enviar_paquete(paquete, dispatch_fd);
	eliminar_paquete(paquete);
}

void enviar_wait_kernel(char* reg, int dispatch){
	t_paquete* paquete = crear_paquete(I_WAIT);
	agregar_a_paquete(paquete,reg, strlen(reg)+1);
	enviar_paquete(paquete,dispatch);
	eliminar_paquete(paquete);
}
void enviar_signal_kernel(char* reg, int dispatch){
	t_paquete* paquete = crear_paquete(I_SIGNAL);
	agregar_a_paquete(paquete,reg, strlen(reg)+1);
	enviar_paquete(paquete,dispatch);
	eliminar_paquete(paquete);
}
void enviar_io_fs_create_a_kernel(char* nomb_interfaz, char* nombr_archivo, int dispatch){
	t_paquete* paquete = crear_paquete(IO_CREATE);
	agregar_a_paquete(paquete,nomb_interfaz, strlen(nomb_interfaz)+1);
	agregar_a_paquete(paquete,nombr_archivo, strlen(nombr_archivo)+1);
	enviar_paquete(paquete,dispatch);
	eliminar_paquete(paquete);
}
void enviar_io_fs_delete_a_kernel(char* nomb_interfaz, char* nombr_archivo, int dispatch){
	t_paquete* paquete = crear_paquete(IO_DELETE);
	agregar_a_paquete(paquete,nomb_interfaz, strlen(nomb_interfaz)+1);
	agregar_a_paquete(paquete,nombr_archivo, strlen(nombr_archivo)+1);
	enviar_paquete(paquete,dispatch);
	eliminar_paquete(paquete);
}
void enviar_io_fs_truncate_a_kernel(char* nomb_interfaz, char* nombr_archivo, uint32_t tamanio, int dispatch){
	t_paquete* paquete = crear_paquete(IO_TRUNCATE);
	agregar_a_paquete(paquete,nomb_interfaz, strlen(nomb_interfaz)+1);
	agregar_a_paquete(paquete,nombr_archivo, strlen(nombr_archivo)+1);
	agregar_a_paquete(paquete,&(tamanio), sizeof(uint32_t));
	enviar_paquete(paquete,dispatch);
	eliminar_paquete(paquete);
}

void enviar_io_fs_write_a_kernel(char* nomb_interfaz, char* nombr_archivo, t_list* accesos, uint32_t puntero, int dispatch_fd)
{
	t_paquete* paquete = crear_paquete(IO_WRITE);
	agregar_a_paquete(paquete, nomb_interfaz, strlen(nomb_interfaz)+1);
	agregar_a_paquete(paquete, nombr_archivo, strlen(nombr_archivo)+1);

	int cantidad_direcciones = list_size(accesos);
	agregar_a_paquete(paquete, &cantidad_direcciones, sizeof(int));
	agregar_a_paquete(paquete, &puntero, sizeof(uint32_t));

	t_acceso_memoria* acceso_memoria = malloc(sizeof(t_acceso_memoria));
	for(int i = 0; i < cantidad_direcciones; i++) {
		acceso_memoria = (t_acceso_memoria*) list_get(accesos, i);
		agregar_a_paquete(paquete, &(acceso_memoria->direccion), sizeof(uint32_t));
		agregar_a_paquete(paquete, &(acceso_memoria->tamanio), sizeof(uint32_t));
	}

	enviar_paquete(paquete, dispatch_fd);
	eliminar_paquete(paquete);
}


//void enviar_io_fs_write_a_kernel(char* nomb_interfaz, char* nombr_archivo, int direccion, int tamanio, uint32_t puntero_archivo, int dispatch_fd){
//	t_paquete* paquete = crear_paquete(IO_WRITE);
//	agregar_a_paquete(paquete,nomb_interfaz, strlen(nomb_interfaz)+1);
//	agregar_a_paquete(paquete,nombr_archivo, strlen(nombr_archivo)+1);
//
//	agregar_a_paquete(paquete, &direccion, sizeof(int));
//	agregar_a_paquete(paquete, &tamanio, sizeof(int));
//
//	agregar_a_paquete(paquete,&puntero_archivo, sizeof(uint32_t));
//	enviar_paquete(paquete,dispatch_fd);
//	eliminar_paquete(paquete);
//}

void enviar_io_fs_read_a_kernel(char* nomb_interfaz, char* nombr_archivo, t_list* accesos, uint32_t puntero, int dispatch_fd){
	t_paquete* paquete = crear_paquete(IO_READ);
	agregar_a_paquete(paquete,nomb_interfaz, strlen(nomb_interfaz)+1);
	agregar_a_paquete(paquete,nombr_archivo, strlen(nombr_archivo)+1);

	int cantidad_direcciones = list_size(accesos);
	agregar_a_paquete(paquete, &cantidad_direcciones, sizeof(int));
	agregar_a_paquete(paquete, &puntero, sizeof(uint32_t));

	t_acceso_memoria* acceso_memoria = malloc(sizeof(t_acceso_memoria));
	for(int i = 0; i < cantidad_direcciones; i++) {
		acceso_memoria = (t_acceso_memoria*) list_get(accesos, i);
		agregar_a_paquete(paquete, &(acceso_memoria->direccion), sizeof(uint32_t));
		agregar_a_paquete(paquete, &(acceso_memoria->tamanio), sizeof(uint32_t));
	}

	enviar_paquete(paquete,dispatch_fd);
	eliminar_paquete(paquete);
}

void enviarNombreInterfaz(int fd, char* nombreInterfaz, char* tipo_interfaz){

    t_paquete* paquete = crear_paquete(NOMBREINTERFAZ);
	agregar_a_paquete(paquete, nombreInterfaz, strlen(nombreInterfaz)+1);
	agregar_a_paquete(paquete, tipo_interfaz, strlen(tipo_interfaz)+1);
	enviar_paquete(paquete, fd);
	eliminar_paquete(paquete);
}

void enviar_interrupcion(int fd, t_motivo_exit motivo)
{
	t_paquete* paquete = crear_paquete(motivo);
	enviar_paquete(paquete,fd);
	eliminar_paquete(paquete);
}

void enviar_instruccion_io(int pid, char* interfaz, t_instruccion_io* parametros, int fd_modulo)
{
	t_paquete* paquete = crear_paquete(parametros->prox_io);
	agregar_a_paquete(paquete, interfaz, strlen(interfaz)+1);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	printf("\nenviando un pid %d\n", pid);

	if(parametros->param1!=NULL)
		agregar_a_paquete(paquete, parametros->param1 ? parametros->param1 : NULL, strlen(parametros->param1)+1);
	else
		agregar_a_paquete(paquete, NULL, 0);

	agregar_a_paquete(paquete, (parametros->param2 != -1) ? &(parametros->param2) : 0, (parametros->param2 != -1) ? sizeof(int) : 0);
	agregar_a_paquete(paquete, (parametros->param3 != -1) ? &(parametros->param3) : 0, (parametros->param3 != -1) ? sizeof(int) : 0);
	agregar_a_paquete(paquete, (parametros->param4 != -1) ? &(parametros->param4) : 0, (parametros->param4 != -1) ? sizeof(int) : 0);

	if(parametros->lista_direcciones != NULL) {
		uint32_t param;
		for(int i=0; i < list_size(parametros->lista_direcciones) ; i++) {
			param = *(uint32_t*)list_get(parametros->lista_direcciones, i);
			agregar_a_paquete(paquete, &param, sizeof(uint32_t));
		}
	}
	enviar_paquete(paquete, fd_modulo);
	//free(parametros->param1);
	//free(parametros->param2);
	//free(parametros->param3);
	//free(parametros->param4);
	//list_destroy(parametros->lista_direcciones);
	//free(parametros);
	eliminar_paquete(paquete); 
}

void enviar_fin_proceso(int pid, int fd)
{
	t_paquete* paquete = crear_paquete(PETICION_LIBERAR_PROCESO);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	enviar_paquete(paquete,fd);
	eliminar_paquete(paquete);
}

void enviar_io_read_a_memoria(char* datosBloque, int dir_fisica_a_memoria , int memoria_fd, int tamanio_a_leer, int pid){
	t_paquete* paquete = crear_paquete(ESCRIBIR_EN_MEMORIA);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, &dir_fisica_a_memoria, sizeof(int));
	agregar_a_paquete(paquete, datosBloque, strlen(datosBloque)+1);
	agregar_a_paquete(paquete, &tamanio_a_leer, sizeof(int));
	enviar_paquete(paquete, memoria_fd);
	eliminar_paquete(paquete);
}

char* enviar_io_write_a_memoria(int dir_fisica_a_memoria, int tam_a_escribir, int memoria_fd, int pid){

	t_paquete* paquete = crear_paquete(LEER_DE_MEMORIA);
	agregar_a_paquete(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, &dir_fisica_a_memoria, sizeof(int));
	agregar_a_paquete(paquete, &tam_a_escribir, sizeof(int));
	enviar_paquete(paquete, memoria_fd);
	mem_status status = recibir_mem_status(memoria_fd);

	op_code cod_op;
	char* datosBloque = malloc(tam_a_escribir);
    cod_op = recibir_operacion(memoria_fd);
    if(cod_op == DATOS_DE_MEMORIA){
		t_list* paqueteRecibido = recibir_paquete(memoria_fd);
		memcpy(datosBloque, (char*)list_get(paqueteRecibido, 0), tam_a_escribir);
		list_destroy_and_destroy_elements(paqueteRecibido, free);
    }
	eliminar_paquete(paquete);

	return datosBloque;
}

/***********************************************************************************/
/*************************** RECV & DESERIALIZACION ********************************/
/***********************************************************************************/
t_contexto_ejecucion* recibir_contexto_cpu(int fd){
	t_list* paquete = recibir_paquete(fd);
	t_contexto_ejecucion* contexto = deserializar_pcb(paquete);
	list_destroy(paquete);
	return contexto;
}
//deserializo el pcb , creando mi otro pcb para agregar el registro
// ver como hacer esto!
t_contexto_ejecucion* deserializar_pcb(t_list * paquete_pcb) {
	t_contexto_ejecucion* pcb_cpu = malloc(sizeof(t_contexto_ejecucion));
	int* pid = list_get(paquete_pcb,0);
	pcb_cpu->pid = *pid;
	free(pid);

	uint32_t * pc = list_get(paquete_pcb,1);
	pcb_cpu->program_counter = *pc;
	free(pc);

	t_motivo_exit* motivo = list_get(paquete_pcb,2);
	pcb_cpu->motivo_exit = *motivo;
	free(motivo);

	uint8_t * ax = list_get(paquete_pcb,3);
	pcb_cpu->AX = *ax;
	free(ax);

	uint8_t * bx = list_get(paquete_pcb,4);
	pcb_cpu->BX = *bx;
	free(bx);

	uint8_t * cx = list_get(paquete_pcb,5);
	pcb_cpu->CX = *cx;
	free(cx);

	uint8_t * dx = list_get(paquete_pcb,6);
	pcb_cpu->DX = *dx;
	free(dx);

	uint8_t * eax = list_get(paquete_pcb,7);
	pcb_cpu->EAX = *eax;
	free(eax);

	uint32_t * ebx = list_get(paquete_pcb,8);
	pcb_cpu->EBX = *ebx;
	free(ebx);

	uint32_t * ecx = list_get(paquete_pcb,9);
	pcb_cpu->ECX= *ecx;
	free(ecx);

	uint32_t * edx = list_get(paquete_pcb,10);
	pcb_cpu->EDX = *edx;
	free(edx);

	uint32_t * si = list_get(paquete_pcb,11);
	pcb_cpu->SI = *si;
	free(si);

	uint32_t * di = list_get(paquete_pcb,12);
	pcb_cpu->DI = *di;
	free(di);

	return pcb_cpu;
}

t_list* recibirNombreInterfaz(int fd)
{
	t_list* paquete = recibir_paquete(fd);
	char* nombre = (char*)list_get(paquete, 0);
	char* tipo = (char*)list_get(paquete, 1);
	t_list* lista = list_create();
	list_add(lista, nombre);
	list_add(lista, tipo);
	list_destroy(paquete);
	return lista;
}

char* recibir_recurso(int fd)
{
	t_list* paquete = recibir_paquete(fd);
	char* recurso = list_get(paquete, 0);
	list_destroy(paquete);
	return recurso;
}

t_list* recibir_direccionesOValores(int fd)
{
	t_list* paquete = recibir_paquete(fd);
	char* nombre_interfaz = (char*)list_get(paquete, 0);
	int pid = *(int*)list_get(paquete, 1);
	t_list* list = list_create();
	list_add(list, &pid);
	int cantidad = (list_size(paquete) - 6);
	list_add(list, &cantidad);
	uint32_t param;
	for(int i=0; i < cantidad ; i++) {
		param = *(uint32_t*) list_get(paquete, i+2);
        list_add(list, &param);
	}
	list_destroy(paquete);
	return list;
}

mem_status recibir_mem_status(int memoria_fd) {
	int status = -1;
	op_code opc = recibir_operacion(memoria_fd);
	if(opc != MEM_STATUS)
		printf("opc != MEM_STATUS\n");
	else {
		t_list* valores = recibir_paquete(memoria_fd);
		status = *(int *) list_get(valores, 0);
		list_destroy_and_destroy_elements(valores, free);
	}

	return status;
}


t_list* recibir_io_create(int kernel_fd){

	t_list* paquete = recibir_paquete(kernel_fd);
	char* nombre_archivo = (char*)list_get(paquete, 2);
	int pid = *(int*)list_get(paquete, 1);
	t_list* list = list_create();
	list_add(list, nombre_archivo);
	list_add(list, &pid);
	list_destroy(paquete);
	return list;
}

char* recibir_io_delete(kernel_fd){

	t_list* paquete = recibir_paquete(kernel_fd);
	char* nombre_archivo = (char*)list_get(paquete, 2);
	list_destroy(paquete);
	return nombre_archivo;
}

t_list* recibir_io_truncate(kernel_fd){

	t_list* paquete = recibir_paquete(kernel_fd);
	char* nombre_interfaz = (char*)list_get(paquete, 0);
	int pid = *(int*)list_get(paquete, 1);
	t_list* list = list_create();
	list_add(list, list_get(paquete, 2));
	list_add(list, list_get(paquete, 3));
	list_add(list, &pid);
	list_destroy(paquete);
	return list;
}

t_list* recibir_io_write(kernel_fd){

	t_list* paquete = recibir_paquete(kernel_fd);
	char* nombre_interfaz = (char*)list_get(paquete, 0);
	int pid = *(int*)list_get(paquete, 1);

	t_list* list = list_create();
	list_add(list, list_get(paquete, 2)); //nombre archivo
	list_add(list, list_get(paquete, 3)); //dir fisica 
	list_add(list, list_get(paquete, 4)); // tamanio 
	list_add(list, list_get(paquete, 5)); //puntero
	list_add(list, &pid);

	list_destroy(paquete);

	return list;
}



/***********************************************************************************/
/******************************** TP 0 UTILS ***************************************/
/***********************************************************************************/
t_paquete* crear_paquete(op_code codigo_op) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = codigo_op;
    crear_buffer(paquete);
    return paquete;
}

void crear_buffer(t_paquete *paquete) {
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;
}

void* serializar_paquete(t_paquete* paquete, int bytes) {
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio) {
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    paquete->buffer->size += tamanio + sizeof(int);
}

void eliminar_paquete(t_paquete* paquete) {
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente) {
	int bytes = paquete->buffer->size + sizeof(int) + sizeof(op_code);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

op_code recibir_operacion(int socket_cliente) {
    op_code cod_op;
    if (recv(socket_cliente, &cod_op, (u_int32_t) sizeof(int), MSG_WAITALL) > 0)
        return cod_op;
    else {
		printf("Error en el codigo de operación.");
        close(socket_cliente);
        return -1;
    };
}

t_list* recibir_paquete(int socket_cliente){
	int size;
	int desplazamiento = 0;
	void* buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size){
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void* recibir_buffer(int* size, int socket_cliente) {
    void* buffer;

    recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
    buffer = malloc(*size);
    recv(socket_cliente, buffer, *size, MSG_WAITALL);

    return buffer;
}

/*void enviar_mensaje(char* mensaje, int socket_cliente) {
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}*/

char* recibir_mensaje(int socket_cliente) {
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	return buffer;
}
