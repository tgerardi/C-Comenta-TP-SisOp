#include <utils/utils.h>

int *string_to_int_array(char **array_de_strings)
{
    int count = string_array_size(array_de_strings);
    int *numbers = malloc(sizeof(int) * count);
    for (int i = 0; i < count; i++)
    {
        int num = atoi(array_de_strings[i]);
        numbers[i] = num;
    }
    return numbers;
}

char* get_string_elements(t_list* lista)
{
    char* str = string_new();
    for (int i = 0; i < list_size(lista); i++) {
        int *num = (int*) list_get(lista, i);
        if (i < list_size(lista) - 1) {
            string_append_with_format(&str, " %d ", *num);
        } else {
            string_append_with_format(&str, " %d ", *num);
        }
    }
    return str;
}

char* get_motivo_exit(t_motivo_exit motivo)
{
    switch (motivo) {
        case SUCCESS:
            return "SUCCESS";
        case INVALID_RESOURCE:
            return "INVALID_RESOURCE";
        case INVALID_INTERFACE:
            return "INVALID_INTERFACE";
        case OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case INTERRUPTED_BY_USER:
            return "INTERRUPTED_BY_USER";
        default:
            break;
    }
    return NULL;
}

/*void free_contexto_ejecucion(t_contexto_ejecucion* contexto_ejecucion){
	free(contexto_ejecucion->AX);
	free(contexto_ejecucion->BX);
	free(contexto_ejecucion->CX);
	free(contexto_ejecucion->DX);
	free(contexto_ejecucion->EAX);
	free(contexto_ejecucion->EBX);
	free(contexto_ejecucion->ECX);
	free(contexto_ejecucion->EDX);
	free(contexto_ejecucion->SI);
	free(contexto_ejecucion->DI);
	free(contexto_ejecucion);
	contexto_ejecucion = NULL;
}*/

void free_contexto_ejecucion(t_contexto_ejecucion* contexto)
{
    free(contexto);
}

void free_pid_pc(t_paquete_a_memoria* pid_cpu)
{
    free(pid_cpu);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}