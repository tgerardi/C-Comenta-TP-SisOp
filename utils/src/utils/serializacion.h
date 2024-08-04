#ifndef SERIALIZACION_H_
#define SERIALIZACION_H_

#include <utils/utils.h>

typedef enum {
    PAQUETE,
    CONTEXTO_EJECUCION,
	LISTA_INSTRUCCIONES,
	PETICION_INICIAR_PROCESO,
	PEDIR_INSTRUCCION,
	INSTRUCCION,
	ESPERAR_IO_SLEEP,
	FIN_INTERFAZ,
	CREAR_PROCESO_STATUS,
	RESIZE_MEMORIA,
	ACCESO_TABLA_PAGINA,
	RESPUESTA_ACCESO_TABLA_PAGINA,
	PEDIDO_ESCRITURA_A_MEMORIA,
	NUMERO_DE_MARCO,
	MEM_STATUS,
	DATOS_DE_MEMORIA,
	PETICION_LIBERAR_PROCESO,
	LEER_DE_MEMORIA,
	ESCRIBIR_EN_MEMORIA,
	RESIZE_PROCESO,
	ACCESO_A_TABLA_DE_PAGINAS,
	SOLICITAR_IO_STDIN_READ,
	SOLICITAR_IO_STDOUT_WRITE,
	NOMBREINTERFAZ,
	I_WAIT,
	INTERRUPCION,
	I_SIGNAL,
	IO_SLEEP,
	IO_CREATE,
	IO_DELETE,
	IO_TRUNCATE,
	IO_WRITE,
	IO_READ,
	HANDLE_EXIT,
	FIN_QUANTUM
} op_code;

typedef enum {
	MEM_EXITO,
	MEM_ERROR,
	MEM_PID_INVALIDO,
	MEM_DIR_INVALIDA,
	MEM_OUT_OF_MEMORY
} mem_status;

typedef struct {
    char *param1;
    int param2;
    int param3;
    int param4;
    op_code prox_io;
	t_list* lista_direcciones;
} t_instruccion_io;

typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;


typedef struct {
	uint32_t direccion;
	uint32_t tamanio;
} t_acceso_memoria;

/***********************************************************************************/
/******************************* SERIALIZACION *************************************/
/***********************************************************************************/

void enviar_contexto_ejecucion(t_contexto_ejecucion* contexto_ejecucion, int fd_modulo);
void serializar_contexto_ejecucion(t_paquete* paquete, t_contexto_ejecucion* contexto_ejecucion);
void enviar_pid_pc_memoria(t_paquete_a_memoria* pid_pc, int fd_modulo);
void serializar_pid_pc(t_paquete* paquete, t_paquete_a_memoria* pid_pc);
void enviar_peticion_memoria(t_list* peticion, int fd);
void serializar_peticion_memoria(t_paquete* paquete, t_list* peticion);
void serializar_io_gen(t_paquete* paquete, t_list* io_gen_sleep);
void enviar_io_gen_sleep_a_kernel(t_list* io_gen_sleep, int fd_modulo);
void enviarFinInstruccion(int fd);
void enviar_resize_a_memoria(int pid,int tamanio_memoria, int fd_memoria);
void enviar_pid_nro_pagina_a_memoria(int pagina, int pid, int fd_memoria);
void enviar_mov_out_a_memoria(char* valor_reg2, int direccion_fisica, int pid, int tamanio, int fd_memoria);
void enviar_mov_in_a_memoria( int pid, int dir_fisica, int tamanio, int memoria_fd );
void enviar_a_kernel_stdin_read(char* nombre_interfaz, t_list* lista_accesos_a_memoria, int dispatch_fd);
void enviar_a_kernel_stduot_write(char* nombre_interfaz, t_list* lista_accesos_a_memoria, int dispatch_fd);
void enviar_wait_kernel(char* reg, int dispatch);
void enviar_signal_kernel(char* reg, int dispatch);
void enviar_io_fs_create_a_kernel(char* nomb_interfaz, char* nombr_archivo, int dispatch);
void enviar_io_fs_delete_a_kernel(char* nomb_interfaz, char* nombr_archivo, int dispatch);
void enviar_io_fs_truncate_a_kernel(char* nomb_interfaz, char* nombr_archivo, uint32_t tamanio, int dispatch);
void enviar_io_fs_write_a_kernel(char* nomb_interfaz, char* nombr_archivo, t_list* accesos, uint32_t puntero, int dispatch_fd);
void enviar_io_fs_read_a_kernel(char* nomb_interfaz, char* nombr_archivo, t_list* accesos, uint32_t puntero, int dispatch_fd);
void enviar_instruccion_io(int pid, char* interfaz, t_instruccion_io* parametros, int fd_modulo);
void enviar_fin_proceso(int pid, int fd);
void enviar_interrupcion(int fd, t_motivo_exit motivo);
void enviarNombreInterfaz(int kernel_fd, char* nombreInterfaz, char* tipo_interfaz);
void guardar_en_direcciones(char* texto,int dir, int tam, int pid, int fd_memoria);
char* enviar_peticion_memoria_direcciones(int direccion_fisicaWrite, int tamanio_direccionWrite, int pidIOWrite, int fd);
void enviar_io_read_a_memoria(char* datosBloque, int dir_fisica_a_memoria , int  memoria_fd, int tamanio_a_leer, int pid);
char* enviar_io_write_a_memoria(int dir_fisica_a_memoria, int tam_a_escribir, int memoria_fd, int pid);
	
/***********************************************************************************/
/****************************** DESERIALIZACION ************************************/
/***********************************************************************************/

t_contexto_ejecucion* recibir_contexto_cpu(int dispatch_fd);
t_contexto_ejecucion* deserializar_pcb(t_list * paquete_pcb);
t_list* recibirNombreInterfaz(int fd);
char* recibir_recurso(int fd);
t_list* recibir_direccionesOValores(int fd);
mem_status recibir_mem_status(int memoria_fd);
t_list* recibir_io_create(int kernel_fd);
char* recibir_io_delete(int kernel_fd);
t_list* recibir_io_truncate(int kernel_fd);
t_list* recibir_io_write(int kernel_fd);

/***********************************************************************************/
/******************************** TP 0 UTILS ***************************************/
/***********************************************************************************/

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void eliminar_paquete(t_paquete* paquete);
t_paquete* crear_paquete(op_code codigo_op);
t_list* recibir_paquete(int socket_cliente);
void crear_buffer(t_paquete *paquete);
void enviar_mensaje(char* mensaje, int socket_cliente);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
void* recibir_buffer(int* size, int socket_cliente);
char* recibir_mensaje(int socket_cliente);
op_code recibir_operacion(int socket_cliente);

#endif
