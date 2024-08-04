#ifndef ENTRADASALIDA_H_
#define ENTRADASALIDA_H_

#include <fcntl.h>
#include <utils/utils.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>

typedef enum{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
} t_interfaz;

typedef struct{
    char* tipo_interfaz;
    int tiempo_unidad_trabajo;
    char* ip_kernel;
    char* puerto_kernel;
    char* ip_memoria;
    char* puerto_memoria;
    char* path_base_dialfs;
    int block_size;
    int block_count;
    int retraso_compactacion;
} t_config_io;

typedef struct{
    char* nombre;
    t_config* config;
} t_metadata;

char* nombreInterfaz;

int memoria_fd;
int kernel_fd;

t_log* logger;
t_config* config;
t_config_io* config_io;

pthread_t t_kernel;
pthread_t t_memoria;

void procesar_conexion_kernel();
t_interfaz tipoInterfazToEnum(char* tipoInterfaz);
//void compactarBitarray();
void levantar_metadata(char* nombre_archivo, int bloque_inicial);
int buscar_espacio();
void levantar_bitmap();
void levantar_bloques();
void inicializar_fs();
void eliminarArchivo(t_metadata* metadata);
int buscarIndex_Archivo (char* nombre_archivo);
t_metadata* buscarArchivo(char* nombre_archivo);
void eliminarEspacio(int bloque_inicial,int tamanio);
void cleanBitarray(int bloque_inicial,int bloque_final);
void deleteFile(char* nombre);
void truncar(char* nombre_archivo_truncar,int tamanio_truncar, int pid);
void setBitarray(int bloque_inicial,int bloque_final);
void compactar(t_metadata* archivo, int tamanio_anterior);
void compactarBitarray();
void moverDatosMetadata(t_metadata* metadata, int bloque_viejo, int bloque_nuevo);
t_metadata* buscarArchivoPorBloqueInicial(int bloque);
t_metadata* buscarArchivoPorBloque(int bloque);
void io_leer(char* nombre_archivo_leer, int puntero_inicio, int tam_a_leer, int dir_fisica_a_memoria, int pid);
char* obtenerDatosBloque(int inicio, int fin);
void io_escribir(char* nombre_archivo_escribir, int puntero_inicio, int tam_a_escribir, int dir_fisica_a_memoria, int pid);
void escribirDatosBloque(char* datosBloque ,int inicio, int fin);
void moverDatosBloques(int bloqueOrigen, int bloqueDestino);
t_metadata* levantar_metadata_nuevamente(char* nombre_archivo);

/*********************/
/******* INITS *******/
/*********************/
void iniciar_logger();
void iniciar_config();
void iniciar_conexiones();
void iniciar_interfaz();

/*********************/
/******* FREEs *******/
/*********************/
void free_logger();
void free_config();
void free_conexiones();

#endif