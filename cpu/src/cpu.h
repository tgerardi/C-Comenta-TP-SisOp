#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>
#include <pthread.h>


#define FIFO "FIFO"
#define LRU "LRU"

//crear alguna variable especie bandera, flag en la que cambie de valor 
// cuando cae / toca en dispatch o interrupt 
// si esta en false: es por que no hay interrumcion, osea -> dispatch
// si es true -> es interrupcion y se debe atender(atender, desaolojar, actualizar contexto y devolverle a kernel)
// recorda que debe ser constante el cambio de booleano de la bandera en cada escucha
//extern bool interrupt_flag;
extern int interrupt_flag;
pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER;
extern bool exec_flag;


typedef struct {
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha_dispatch;
    char* puerto_escucha_interrupt;
    int cantidad_entradas_tlb;
    char* algoritmo_tlb;
    int tam_max_pagina;
} t_config_cpu;

typedef struct{
    int numero_pagina;
    int desplazamiento;
    int direccion_fisica;
    int tiempo;
} t_mmu;

typedef struct {
    int tlb_pid;
    int pagina;
    int frame;

    t_temporal* tiempo_uso;
}t_tlb;

//t_tlb* tlb;
t_list* tlb;

//int TAMANIO_MAX = 16;

int dispatch_fd;
int interrupt_fd;
int socket_cpu_dispatch;
int socket_cpu_interrupt;
int memoria_fd;


t_log* logger;
t_config* config;
t_config_cpu* config_cpu;

pthread_t hilo_dispatch;
pthread_t hilo_interrupt;

bool exec_flag = true;
//bool interrupt_flag = false;
int interrupt_flag = 0;
/*******************/
/****** INITS ******/
/*******************/
void iniciar_logger();
void iniciar_config();
void iniciar_conexiones();
void iniciar_tlb();

void escuchar_dispatch();
void escuchar_interrupt();

void iniciar_conexion_memoria();
void iniciar_server_dispatch();
void iniciar_server_interrupt();
void escuchar_dispatch();
void escuchar_interrupt();
void inicio_ciclo_de_instrucciones(t_contexto_ejecucion* contexto);
void enviar_pid_pc_memoria(t_paquete_a_memoria* paquete_pid_pc,int memoria_fd);
void fetch(t_contexto_ejecucion* contexto);
void decode(t_list* paquete, t_contexto_ejecucion* contexto);
instruction_code convert_instruction_a_codeop(char* nro_linea);
t_instruccion* parsear_instruccion(char* nro_linea);
instruction_code convert_instruction_a_codeop(char* nro_linea);
void setear_registro(t_contexto_ejecucion* contexto, char* nomn_reg1, uint32_t val);
void set(t_contexto_ejecucion* contexto, char* nomn_reg1, char* val);
void sub(t_contexto_ejecucion* contexto, char* nomn_reg1, char* nomn_reg2);
void sum(t_contexto_ejecucion* contexto, char* nomn_reg1, char* nomn_reg2);
void jnz(t_contexto_ejecucion* contexto, char* nomn_reg1, char* val);
uint32_t get_valor_registro(t_contexto_ejecucion* contexto, char* reg);
void io_gen_sleep(char* nombre_reg1, int nombre_reg2);
void enviar_io_gen_sleep(char* reg1, int reg2, int dispatch_fd);
bool comparar_set_pc(t_instruccion* ins, char* pc);
void enviar_contexto_actualizado(t_contexto_ejecucion* contexto, bool incrementar_pc);
void exit_con_motivo(t_contexto_ejecucion* contexto, t_motivo_exit motivo, op_code codigo);
void resize(t_contexto_ejecucion* contexto, char* nombre_reg1);
void copy_string(t_contexto_ejecucion* contexto, char* nombre_reg1);
void io_stdin_read(t_contexto_ejecucion* contexto,char* nom_interfaz, char* reg2, char* reg3);
void io_stdin_write(t_contexto_ejecucion* contexto,char* nom_interfaz, char* reg2, char* reg3);
bool buscar_existencia_marco_en_tlb(int pid,int numero_pagina , t_list* tlb);
void manejar_tlb_hit(t_list* tlb, t_mmu* mmu,t_contexto_ejecucion* contexto);
void manejar_tlb_miss(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto);
void actualizar_tabla_tlb(t_list* tlb , t_mmu* mmu, t_list* paquete, t_contexto_ejecucion* contexto);
//void copiar_pid_mmu(t_contexto_ejecucion* contexto, t_mmu* mmu);
void mov_in(t_contexto_ejecucion* contexto, char* reg1, char* reg2);
//void lectura_de_memoria(t_contexto_ejecucion* contexto, int dir_logica, void *buf, int tamanio);
void mov_out(t_contexto_ejecucion* contexto, char* reg1, char* reg2);
//void escritura_en_memoria(t_contexto_ejecucion* contexto, int dir_logica, void *buf, int tamanio);
//void lectura_de_memoria(t_contexto_ejecucion* contexto, int dir_logica, void *buf, int tamanio);
void instruction_wait(t_contexto_ejecucion* contexto, char* recurso);
void instruction_signal(t_contexto_ejecucion* contexto, char* recurso);
void io_fs_create(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo);
void io_fs_delete(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo);
void io_fs_truncate(t_contexto_ejecucion* contexto, char* interfaz, char* nom_archivo, char* reg3);
void io_fs_write(t_contexto_ejecucion* contexto, char* interfaz,char* nom_archivo, char* reg3, char* reg4, char* reg5);
void io_fs_read(t_contexto_ejecucion* contexto, char* interfaz,char* nom_archivo, char* reg3, char* reg4, char* reg5);
bool escritura_en_memoria(void *buffer, int pid,int dir_fisica, int tamanio, int is_string);
void aplicar_algoritmo(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto);
void algoritmo_fifo(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto);
void algoritmo_lru(t_list* tlb, t_mmu* mmu, t_contexto_ejecucion* contexto);
bool lectura_de_memoria(void *buffer, int pid,int dir_fisica, int tamanio, int is_string);
void enviar_stdin_read_kernel(t_contexto_ejecucion* contexto, char* nom_interfaz, t_list* lista_accesos_a_memoria);
void enviar_stdout_write_kernel(t_contexto_ejecucion* contexto, char* nom_interfaz, t_list* lista_accesos_a_memoria);
int get_tamanio_registro(char* reg1);
int buscar_indice_entrada_mas_antigua(t_list* tlb);
/********************/
/****** Free's ******/
/********************/
void free_logger();
void free_config();
void free_conexiones();
void liberar_instruccion(t_instruccion* instruccion);

#endif
