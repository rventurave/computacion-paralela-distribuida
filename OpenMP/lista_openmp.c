#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <omp.h>

/*
 * Laboratorio OpenMP: lista enlazada concurrente.
 *
 * Compilacion:
 *   gcc -O2 -Wall -Wextra -fopenmp lista_openmp.c -o lista_openmp
 *
 * Ejemplos:
 *   ./lista_openmp 10000 50000 4 global
 *   ./lista_openmp 10000 50000 4 fases
 *
 * Argumentos:
 *   1. Cantidad inicial de nodos
 *   2. Cantidad de operaciones
 *   3. Cantidad de hilos
 *   4. Estrategia: global o fases
 */

typedef struct Nodo {
    int dato;
    struct Nodo *siguiente;
} Nodo;

typedef enum {
    BUSCAR,
    ELIMINAR
} TipoOperacion;

typedef struct {
    TipoOperacion tipo;
    int valor;
} Operacion;

/* Generador pseudoaleatorio reproducible. */
static uint32_t siguiente_aleatorio(uint32_t *estado)
{
    *estado = (*estado * 1664525u) + 1013904223u;
    return *estado;
}

static Nodo *crear_lista(size_t n)
{
    Nodo *cabeza = NULL;
    Nodo *cola = NULL;

    for (size_t i = 0; i < n; ++i) {
        Nodo *nuevo = malloc(sizeof(*nuevo));
        if (nuevo == NULL) {
            fprintf(stderr, "Error: no se pudo reservar memoria.\n");
            while (cabeza != NULL) {
                Nodo *tmp = cabeza;
                cabeza = cabeza->siguiente;
                free(tmp);
            }
            return NULL;
        }

        nuevo->dato = (int)i;
        nuevo->siguiente = NULL;

        if (cabeza == NULL) {
            cabeza = nuevo;
        } else {
            cola->siguiente = nuevo;
        }
        cola = nuevo;
    }

    return cabeza;
}

static void liberar_lista(Nodo *cabeza)
{
    while (cabeza != NULL) {
        Nodo *tmp = cabeza;
        cabeza = cabeza->siguiente;
        free(tmp);
    }
}

static size_t contar_nodos(const Nodo *cabeza)
{
    size_t total = 0;
    while (cabeza != NULL) {
        ++total;
        cabeza = cabeza->siguiente;
    }
    return total;
}

/* Devuelve 1 si encuentra el valor y 0 en caso contrario. */
static int buscar_secuencial(const Nodo *cabeza, int valor)
{
    while (cabeza != NULL) {
        if (cabeza->dato == valor) {
            return 1;
        }
        cabeza = cabeza->siguiente;
    }
    return 0;
}

/*
 * Elimina el primer nodo que contiene valor.
 * Devuelve 1 si lo elimino y 0 si no estaba presente.
 */
static int eliminar_secuencial(Nodo **cabeza, int valor)
{
    Nodo *actual = *cabeza;
    Nodo *anterior = NULL;

    while (actual != NULL && actual->dato != valor) {
        anterior = actual;
        actual = actual->siguiente;
    }

    if (actual == NULL) {
        return 0;
    }

    if (anterior == NULL) {
        *cabeza = actual->siguiente;
    } else {
        anterior->siguiente = actual->siguiente;
    }

    free(actual);
    return 1;
}

/*
 * Genera las mismas operaciones para todas las pruebas.
 * Aproximadamente 80 % son busquedas y 20 % eliminaciones.
 * Algunas busquedas consultan valores que no existen.
 */
static Operacion *generar_operaciones(size_t cantidad, size_t n)
{
    Operacion *ops = malloc(cantidad * sizeof(*ops));
    if (ops == NULL && cantidad > 0) {
        return NULL;
    }

    uint32_t estado = 123456789u;

    for (size_t i = 0; i < cantidad; ++i) {
        uint32_t r = siguiente_aleatorio(&estado);

        if ((r % 5u) == 0u) {
            ops[i].tipo = ELIMINAR;
            ops[i].valor = (int)(siguiente_aleatorio(&estado) % n);
        } else {
            ops[i].tipo = BUSCAR;
            if ((r % 7u) == 0u) {
                ops[i].valor = (int)n + (int)(r % 100u);
            } else {
                ops[i].valor = (int)(siguiente_aleatorio(&estado) % n);
            }
        }
    }

    return ops;
}

/*
 * La estrategia secuencial "global" conserva el orden original.
 * La estrategia "fases" ejecuta todas las busquedas y luego
 * todas las eliminaciones, para poder compararla con esa variante.
 */
static void ejecutar_secuencial(
    Nodo **cabeza,
    const Operacion *ops,
    size_t cantidad,
    int por_fases)
{
    if (!por_fases) {
        for (size_t i = 0; i < cantidad; ++i) {
            if (ops[i].tipo == BUSCAR) {
                (void)buscar_secuencial(*cabeza, ops[i].valor);
            } else {
                (void)eliminar_secuencial(cabeza, ops[i].valor);
            }
        }
    } else {
        for (size_t i = 0; i < cantidad; ++i) {
            if (ops[i].tipo == BUSCAR) {
                (void)buscar_secuencial(*cabeza, ops[i].valor);
            }
        }
        for (size_t i = 0; i < cantidad; ++i) {
            if (ops[i].tipo == ELIMINAR) {
                (void)eliminar_secuencial(cabeza, ops[i].valor);
            }
        }
    }
}

/*
 * Estrategia 1: bloqueo global.
 * Cada operacion completa se protege con un solo omp_lock_t.
 * Esto es seguro, aunque limita el paralelismo efectivo.
 */
static void ejecutar_paralelo_global(
    Nodo **cabeza,
    const Operacion *ops,
    size_t cantidad,
    int num_hilos)
{
    omp_lock_t bloqueo;
    omp_init_lock(&bloqueo);

    #pragma omp parallel for num_threads(num_hilos) schedule(dynamic, 32)
    for (size_t i = 0; i < cantidad; ++i) {
        omp_set_lock(&bloqueo);

        if (ops[i].tipo == BUSCAR) {
            (void)buscar_secuencial(*cabeza, ops[i].valor);
        } else {
            (void)eliminar_secuencial(cabeza, ops[i].valor);
        }

        omp_unset_lock(&bloqueo);
    }

    omp_destroy_lock(&bloqueo);
}

/*
 * Estrategia 2: ejecucion por fases.
 * Fase 1: busquedas concurrentes de solo lectura.
 * Barrera implicita al terminar el primer parallel for.
 * Fase 2: eliminaciones protegidas por un bloqueo global.
 *
 * Se usa un bloqueo en la fase de eliminacion para asegurar
 * que solo un hilo modifique/recorra la lista a la vez.
 */
static void ejecutar_paralelo_fases(
    Nodo **cabeza,
    const Operacion *ops,
    size_t cantidad,
    int num_hilos)
{
    #pragma omp parallel for num_threads(num_hilos) schedule(static)
    for (size_t i = 0; i < cantidad; ++i) {
        if (ops[i].tipo == BUSCAR) {
            (void)buscar_secuencial(*cabeza, ops[i].valor);
        }
    }

    omp_lock_t bloqueo;
    omp_init_lock(&bloqueo);

    #pragma omp parallel for num_threads(num_hilos) schedule(dynamic, 32)
    for (size_t i = 0; i < cantidad; ++i) {
        if (ops[i].tipo == ELIMINAR) {
            omp_set_lock(&bloqueo);
            (void)eliminar_secuencial(cabeza, ops[i].valor);
            omp_unset_lock(&bloqueo);
        }
    }

    omp_destroy_lock(&bloqueo);
}

/*
 * Comprueba que la lista contiene exactamente los valores iniciales
 * que no aparecen como objetivo de ninguna operacion de eliminacion.
 * Dado que todas las eliminaciones se ejecutan antes de terminar,
 * el orden de las operaciones no cambia este conjunto final.
 */
static int verificar_lista(
    const Nodo *cabeza,
    const unsigned char *eliminado,
    size_t n)
{
    const Nodo *actual = cabeza;

    for (size_t i = 0; i < n; ++i) {
        if (eliminado[i]) {
            continue;
        }

        if (actual == NULL || actual->dato != (int)i) {
            return 0;
        }
        actual = actual->siguiente;
    }

    return actual == NULL;
}

static void imprimir_uso(const char *programa)
{
    fprintf(stderr,
        "Uso: %s [nodos] [operaciones] [hilos] [global|fases]\n"
        "Ejemplo: %s 10000 50000 4 global\n",
        programa, programa);
}

int main(int argc, char **argv)
{
    size_t n = 10000;
    size_t cantidad = 50000;
    int num_hilos = 4;
    int por_fases = 0;

    if (argc > 5) {
        imprimir_uso(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc > 1) {
        char *fin = NULL;
        unsigned long long valor = strtoull(argv[1], &fin, 10);
        if (fin == argv[1] || *fin != '\0' || valor == 0 ||
            valor > (unsigned long long)INT32_MAX) {
            imprimir_uso(argv[0]);
            return EXIT_FAILURE;
        }
        n = (size_t)valor;
    }

    if (argc > 2) {
        char *fin = NULL;
        unsigned long long valor = strtoull(argv[2], &fin, 10);
        if (fin == argv[2] || *fin != '\0' ||
            valor > (unsigned long long)SIZE_MAX / sizeof(Operacion)) {
            imprimir_uso(argv[0]);
            return EXIT_FAILURE;
        }
        cantidad = (size_t)valor;
    }

    if (argc > 3) {
        char *fin = NULL;
        long valor = strtol(argv[3], &fin, 10);
        if (fin == argv[3] || *fin != '\0' || valor < 1 || valor > 1024) {
            imprimir_uso(argv[0]);
            return EXIT_FAILURE;
        }
        num_hilos = (int)valor;
    }

    if (argc > 4) {
        if (strcmp(argv[4], "global") == 0) {
            por_fases = 0;
        } else if (strcmp(argv[4], "fases") == 0) {
            por_fases = 1;
        } else {
            imprimir_uso(argv[0]);
            return EXIT_FAILURE;
        }
    }

    Operacion *ops = generar_operaciones(cantidad, n);
    if (ops == NULL && cantidad > 0) {
        fprintf(stderr, "Error: no se pudo reservar memoria para operaciones.\n");
        return EXIT_FAILURE;
    }

    unsigned char *eliminado = calloc(n, sizeof(*eliminado));
    if (eliminado == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria para verificacion.\n");
        free(ops);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < cantidad; ++i) {
        if (ops[i].tipo == ELIMINAR &&
            ops[i].valor >= 0 && (size_t)ops[i].valor < n) {
            eliminado[ops[i].valor] = 1;
        }
    }

    Nodo *lista_secuencial = crear_lista(n);
    Nodo *lista_paralela = crear_lista(n);

    if (lista_secuencial == NULL || lista_paralela == NULL) {
        liberar_lista(lista_secuencial);
        liberar_lista(lista_paralela);
        free(eliminado);
        free(ops);
        return EXIT_FAILURE;
    }

    double inicio = omp_get_wtime();
    ejecutar_secuencial(&lista_secuencial, ops, cantidad, por_fases);
    double tiempo_secuencial = omp_get_wtime() - inicio;

    inicio = omp_get_wtime();
    if (por_fases) {
        ejecutar_paralelo_fases(&lista_paralela, ops, cantidad, num_hilos);
    } else {
        ejecutar_paralelo_global(&lista_paralela, ops, cantidad, num_hilos);
    }
    double tiempo_paralelo = omp_get_wtime() - inicio;

    int correcto_secuencial = verificar_lista(lista_secuencial, eliminado, n);
    int correcto_paralelo = verificar_lista(lista_paralela, eliminado, n);

    size_t nodos_secuencial = contar_nodos(lista_secuencial);
    size_t nodos_paralelo = contar_nodos(lista_paralela);

    double speedup = (tiempo_paralelo > 0.0)
        ? tiempo_secuencial / tiempo_paralelo : 0.0;
    double eficiencia = speedup / (double)num_hilos;

    printf("=== Lista enlazada con OpenMP ===\n");
    printf("Nodos iniciales:       %zu\n", n);
    printf("Operaciones:            %zu\n", cantidad);
    printf("Hilos solicitados:      %d\n", num_hilos);
    printf("Estrategia:             %s\n", por_fases ? "fases" : "global");
    printf("Tiempo secuencial:      %.6f s\n", tiempo_secuencial);
    printf("Tiempo paralelo:        %.6f s\n", tiempo_paralelo);
    printf("Speedup:                %.4f\n", speedup);
    printf("Eficiencia:             %.4f (%.2f%%)\n",
           eficiencia, eficiencia * 100.0);
    printf("Nodos finales secuencial: %zu\n", nodos_secuencial);
    printf("Nodos finales paralelo:   %zu\n", nodos_paralelo);
    printf("Verificacion secuencial:  %s\n",
           correcto_secuencial ? "CORRECTA" : "INCORRECTA");
    printf("Verificacion paralela:    %s\n",
           correcto_paralelo ? "CORRECTA" : "INCORRECTA");

    liberar_lista(lista_secuencial);
    liberar_lista(lista_paralela);
    free(eliminado);
    free(ops);

    return (correcto_secuencial && correcto_paralelo &&
            nodos_secuencial == nodos_paralelo)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
