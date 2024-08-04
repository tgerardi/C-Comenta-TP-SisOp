#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>
#include <commons/bitarray.h>
#include <stdarg.h>

typedef struct {
	t_config* tad;
    char* puerto_escucha;
    int tam_memoria;
    int tam_pagina;
    char* path_instrucciones;
    int retardo_respuesta;
} t_config_data;

t_log* iniciar_logger(void);
t_config_data* iniciar_config(void);

bool iniciar_variables_memoria(void);
void liberar_variables_memoria(void);

void* procesar_conexion(void *);
void crear_hilo_para_conexion(void *(*fn)(void *), int conexion);


//estructura para guardar cierta información de un proceso
typedef struct {
	int pid;	//id del proceso
	char *path;	//path al archivo de instrucciones
	t_list* instrucciones;	//su lista de instrucciones (strings)
	t_list* tabla_de_paginas; //cada elemento es un t_pagina
} t_descriptor_proceso;

void atender_peticion_crear_proceso(int conexion, int pid, char* nombre);
void atender_pedido_de_instruccion(int conexion, int pid, int nrolinea);
void atender_peticion_liberar_proceso(int conexion, int pid);
void atender_lectura_de_memoria(int conexion, int pid, int direccion, int tamanio);
void atender_escritura_en_memoria(int conexion, int pid, int direccion, char *buffer, int tamanio);
void atender_resize_proceso(int conexion, int pid, int nuevo_tamanio);
void atender_acceso_a_tabla_de_paginas(int conexion, int pid, int pagina_id);


typedef struct { //dejo esto como struct por las dudas
	int marco;
} t_pagina;

t_descriptor_proceso* crear_descriptor_de_proceso(int pid, char *path);
t_list* generar_lista_de_instrucciones(char *path);
char* obtener_instruccion(int pid, int nrolinea);
t_descriptor_proceso* buscar_descriptor_por_pid(int pid);
void liberar_descriptor_de_proceso(t_descriptor_proceso* descriptor_proceso);
void crear_tabla_de_paginas(t_descriptor_proceso* proceso);
void liberar_tabla_de_paginas(t_descriptor_proceso* proceso);
bool leer_de_proceso(t_descriptor_proceso* proceso, int direccion, void *buffer, int cantidad);
t_pagina* pagina_de_direccion(t_descriptor_proceso* proceso, int direccion);
bool escribir_en_proceso(t_descriptor_proceso* proceso, int direccion, void *buffer, int cantidad);
bool reduccion_de_proceso(t_descriptor_proceso* proceso, int nuevo_tamanio);
bool ampliacion_de_proceso(t_descriptor_proceso* proceso, int nuevo_tamanio);
int cantidad_paginas_de_proceso(t_descriptor_proceso* proceso);
int tamanio_en_memoria(t_descriptor_proceso* proceso);
void reservar_paginas_para(t_descriptor_proceso* proceso, int cantidad);
void remover_paginas_para(t_descriptor_proceso* proceso, int cantidad);
int encontrar_marco_libre();
int cantidad_marcos_libres();
void setear_marco_ocupado(int id);
void setear_marco_disponible(int id);
int marco_de_pagina(t_descriptor_proceso* proceso, int pagina_id);


void enviar_instruccion(int conexion, char *instruccion);
void enviar_status_crear_proceso(int conexion, mem_status status);
void enviar_datos_de_memoria(int conexion, void *buffer, size_t len);
void enviar_op_status(int conexion, mem_status status);
void enviar_numero_de_marco(int conexion, int marco);


//pasar a utils??
typedef enum {
	PRINT_INFO,
	PRINT_WARNING,
	PRINT_ERROR
} printear_type;

void printear(printear_type type, char *template, ...);
char* m_strcat(char *to, char *from);

#endif
