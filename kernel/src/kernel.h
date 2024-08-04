#ifndef KERNEL_H_
#define KERNEL_H_

#define FIFO "FIFO"
#define RR "RR"
#define VRR "VRR"

#define PRINT_COMMANDS() do { \
    printf("EJECUTAR_SCRIPT [PATH_COMANDOS]\n"); \
    printf("INICIAR_PROCESO [PATH_INSTRUCCIONES]\n"); \
    printf("FINALIZAR_PROCESO [PID]\n"); \
    printf("DETENER_PLANIFICACION\n"); \
    printf("INICIAR_PLANIFICACION\n"); \
    printf("MULTIPROGRAMACION [VALOR]\n"); \
    printf("PROCESO_ESTADO\n"); \
} while(0)

#include <utils/utils.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>

typedef struct {
	char* puerto_escucha;
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    int quantum;
    char** recursos;
    int* instancias_recursos;
    int grado_multiprogramacion;
} t_config_kernel;

typedef enum {
    NEW,
    READY,
    EXEC,
    EXIT,
    BLOCK
} t_estado;

typedef enum {
    EJECUTAR_SCRIPT,
    INICIAR_PROCESO,
    FINALIZAR_PROCESO,
    DETENER_PLANIFICACION,
    INICIAR_PLANIFICACION,
    MULTIPROGRAMACION,
    PROCESO_ESTADO
} t_funcion;

typedef struct {
    int quantum;
    t_estado estado_actual;
    t_estado estado_anterior;
    t_contexto_ejecucion* contexto_ejecucion;
    t_instruccion_io* proxima_io;
} t_pcb;

typedef struct {
    int id;
    int instancias;
    char* nombre;
    t_list* cola_block_asignada;
    t_list* cola_en_uso;
    pthread_mutex_t mx_asignado;
} t_recurso;

typedef struct{
    int fd;
	t_list* pcbs;
    char* tipo_interfaz;
	char* nombreInterfaz;
} t_interfaz;

//int io_fd;
int socket_io;
int cpu_dispatch_fd;
int cpu_interrupt_fd;
int memoria_fd;
bool desalojado_fin_quantum;
bool flag_sleep;

t_log* logger;
t_config* config;
t_config_kernel* config_kernel;

t_list* cola_ready;
t_list* cola_ready_prioridad;
t_list* cola_exit;
t_list* cola_new;
t_list* cola_exec;
t_list* cola_block;

t_list* interfaces;
t_list* lista_recursos;
t_list* pcbs_totales;

sem_t sem_gr_multi_prog;
sem_t sem_planificador_largo;
sem_t sem_planificador_corto;
sem_t sem_new; 
sem_t sem_ready;
sem_t sem_exec;
sem_t sem_exit;
//sem_t sem_block;
//sem_t sem_block_recurso;
sem_t sem_quantum;

pthread_mutex_t mx_cola_ready;
pthread_mutex_t mx_cola_new;
pthread_mutex_t mx_cola_exit;
pthread_mutex_t mx_cola_exec;
pthread_mutex_t mx_cola_block;
pthread_mutex_t mx_cola_ready_prioridad;
pthread_mutex_t mx_planificacion;
pthread_mutex_t mx_sleep;

/*********************/
/******* INITS *******/
/*********************/

void iniciar_logger();
void iniciar_config();
void iniciar_conexiones();
void inicializar_colas();
void inicializar_semaforos();
void inicializar_mutex();
t_pcb* inicializar_pcb();
void iniciar_consola();
void ejecutar_consola(char* inp);
t_list* inicializar_recursos();

/************************/
/****** CPU Utils *******/
/************************/

void procesar_conexion_cpu_dispatch();
void handle_signal(t_pcb* pcb, char* recurso_asignado);
void handle_wait(t_pcb* pcb, char* recurso_asignado);
void handle_gen_sleep(t_pcb* pcb);
void handle_stdin_read(t_pcb* pcb);
void handle_stdout_write(t_pcb* pcb);
void handle_fs_create(t_pcb* pcb);
void handle_fs_delete(t_pcb* pcb);
void handle_fs_truncate(t_pcb* pcb);
void handle_fs_write(t_pcb* pcb);
void handle_fs_read(t_pcb* pcb);

/************************/
/****** E/S Utils *******/
/************************/

void escuchar_io();
void* recibir_io(void* entrada_salida_fd);

/***************************/
/****** Planificacion ******/
/***************************/

void iniciar_planificador_largo_plazo();
void new_to_ready();
void to_exit();

void iniciar_planificador_corto_plazo();
void ready_to_exec();
void dispatch_pcb(t_pcb* pcb);
void block_to_ready();

/***************************************/
/****** Algoritmos Planificacion *******/
/***************************************/

void push_pcb(t_list* cola, t_pcb*, pthread_mutex_t* mutex);
t_pcb* remover_pcb(t_list* cola, pthread_mutex_t* mutex);
void algoritmo_round_robin(t_pcb* pcb);
void algoritmo_virtual_round_robin();
void esperar_por_quantum(t_pcb* pcb);
void desalojo_pre_fin_quantum(t_pcb* pcb, t_temporal* temp);

/********************/
/****** Utils *******/
/********************/

t_funcion inp_to_funcion(char* inp);
void actualizar_pcb(t_pcb* pcb, t_contexto_ejecucion* contexto);
void actualizar_estado(t_pcb* pcb, t_estado estado);
char* estado_string(t_estado estado);
t_list* get_pids(t_list* lista);
bool admite_operacion(char* tipo_interfaz, instruction_code instruccion);
t_recurso* buscar_recurso(char* recurso);
void resource_not_found(char* recurso, t_pcb* pcb);
void pcb_to_exit(t_pcb* pcb, t_motivo_exit motivo);
void pcb_to_block(t_pcb* pcb);
t_interfaz* buscar_interfaz(char* interfaz);
t_pcb* buscar_pcb_por_id(int pid);
bool invalid_parameter(char* p);
void ejecutar_interfaz(t_interfaz* interfaz_encontrada, t_pcb* pcb, t_instruccion_io* parametros);
void ejecutar_script(char* path);
t_recurso* buscar_pcb_en_recurso(int pid);
t_pcb* remover_pcb_cola_block(t_list* cola, pthread_mutex_t* mutex, int i);
t_pcb* remover_pcb_interfaz(int pid);
int buscar_indice_en_cola(int pid, t_list* cola);
t_recurso* buscar_pcb_en_recurso_en_uso(int pid);

/**********************/
/******* Free's *******/
/**********************/

void free_pcb(t_pcb* pcb);
void free_recurso(t_recurso* recurso);
void free_logger();
void free_config();
void free_conexiones();

#endif /* KERNEL_H_ */