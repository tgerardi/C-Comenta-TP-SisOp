#ifndef UTILS_H_
#define UTILS_H_

#define KERNEL "KERNEL"
#define CPU "CPU"
#define MEMORIA "MEMORIA"
#define ENTRADASALIDA "ENTRADASALIDA"
#define MAX_PROC 100

#define IP_KERNEL "192.168.1.102"
#define IP_MEMORIA "192.168.1.103"
#define IP_CPU "192.168.1.104"
#define IP_ENTRADASALIDA "192.168.1.104"
#define IP_ENTRADASALIDA "192.168.1.101"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <readline/readline.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <utils/conexion.h>
#include <math.h>
#include <commons/temporal.h>
#include <sys/mman.h>

typedef enum {
	SUCCESS,
	INVALID_RESOURCE,
	INVALID_INTERFACE,
	OUT_OF_MEMORY,
	INTERRUPTED_BY_USER
} t_motivo_exit;

typedef struct { 
	int pid;
	uint32_t program_counter;
	t_motivo_exit motivo_exit;
	uint8_t AX;
	uint8_t BX;
	uint8_t CX;
	uint8_t DX;
	uint32_t EAX;
	uint32_t EBX;
	uint32_t ECX;
	uint32_t EDX;
	uint32_t SI;
	uint32_t DI;
} t_contexto_ejecucion;

typedef enum {
	SET,
	SUM,
	SUB,
	JNZ,
	IO_GEN_SLEEP,
	MOV_IN,
	MOV_OUT,
	RESIZE,
	COPY_STRING,
	IO_STDIN_READ,
	IO_STDOUT_WRITE,
	IO_FS_CREATE,
	IO_FS_DELETE,
	IO_FS_TRUNCATE,
	IO_FS_WRITE,
	IO_FS_READ,
	SIGNAL,
	WAIT,
	EXIT_INSTRUCCION
} instruction_code;

typedef struct {
	char* nombre_ins;
	//char* nombre_reg1;
	//char* nombre_reg2;
	t_list* parametros;
} t_instruccion;

typedef struct {
	int pid;
	uint32_t program_counter;
} t_paquete_a_memoria;

int *string_to_int_array(char **array_de_strings);
char* get_string_elements(t_list* lista);
char* get_motivo_exit(t_motivo_exit motivo);
void free_contexto_ejecucion(t_contexto_ejecucion* contexto_ejecucion);
void free_pid_pc(t_paquete_a_memoria* pid_cpu);
void liberar_conexion(int socket);

#endif
