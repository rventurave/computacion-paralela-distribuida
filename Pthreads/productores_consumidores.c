#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>

typedef struct {
    int *buffer;
    size_t capacidad;
    size_t entrada;
    size_t salida;
    size_t cantidad;

    size_t total;
    size_t siguiente;
    size_t producidos;
    size_t consumidos;

    size_t *frecuencias;

    pthread_mutex_t mutex;
    pthread_cond_t no_lleno;
    pthread_cond_t no_vacio;
} Buffer;

typedef struct {
    Buffer *buffer;
    int id;
} Argumentos;

void *productor(void *arg) {
    Argumentos *a = arg;
    Buffer *b = a->buffer;

    while (1) {
        pthread_mutex_lock(&b->mutex);

        if (b->siguiente >= b->total) {
            pthread_mutex_unlock(&b->mutex);
            break;
        }

        int valor = (int)(b->siguiente + 1);
        b->siguiente++;

        while (b->cantidad == b->capacidad) {
            pthread_cond_wait(&b->no_lleno, &b->mutex);
        }

        b->buffer[b->entrada] = valor;
        b->entrada = (b->entrada + 1) % b->capacidad;
        b->cantidad++;
        b->producidos++;

        pthread_cond_signal(&b->no_vacio);
        pthread_mutex_unlock(&b->mutex);
    }

    return NULL;
}

void *consumidor(void *arg) {
    Argumentos *a = arg;
    Buffer *b = a->buffer;

    while (1) {
        pthread_mutex_lock(&b->mutex);

        while (b->cantidad == 0 && b->consumidos < b->total) {
            pthread_cond_wait(&b->no_vacio, &b->mutex);
        }

        if (b->consumidos >= b->total) {
            pthread_mutex_unlock(&b->mutex);
            break;
        }

        int valor = b->buffer[b->salida];

        b->salida = (b->salida + 1) % b->capacidad;
        b->cantidad--;
        b->consumidos++;

        if (valor >= 1 && (size_t)valor <= b->total) {
            b->frecuencias[valor - 1]++;
        }

        pthread_cond_signal(&b->no_lleno);

        if (b->consumidos == b->total) {
            pthread_cond_broadcast(&b->no_vacio);
            pthread_cond_broadcast(&b->no_lleno);
        }

        pthread_mutex_unlock(&b->mutex);
    }

    return NULL;
}

int convertirPositivo(const char *texto, size_t *resultado) {
    char *fin;
    errno = 0;

    unsigned long long valor = strtoull(texto, &fin, 10);

    if (errno != 0 || texto == fin || *fin != '\0' ||
        valor == 0 || valor > SIZE_MAX) {
        return 0;
    }

    *resultado = (size_t)valor;
    return 1;
}

int main(int argc, char *argv[]) {
    size_t numProductores = 2;
    size_t numConsumidores = 2;
    size_t capacidad = 5;
    size_t total = 1000;

    if (argc != 1 && argc != 5) {
        fprintf(stderr,
                "Uso: %s [productores consumidores capacidad total]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 5) {
        if (!convertirPositivo(argv[1], &numProductores) ||
            !convertirPositivo(argv[2], &numConsumidores) ||
            !convertirPositivo(argv[3], &capacidad) ||
            !convertirPositivo(argv[4], &total)) {
            fprintf(stderr, "Todos los argumentos deben ser positivos.\n");
            return EXIT_FAILURE;
        }
    }

    if (total > INT32_MAX ||
        numProductores > SIZE_MAX / sizeof(pthread_t) ||
        numConsumidores > SIZE_MAX / sizeof(pthread_t) ||
        total > SIZE_MAX / sizeof(size_t) ||
        numProductores > SIZE_MAX / sizeof(Argumentos) ||
        numConsumidores > SIZE_MAX / sizeof(Argumentos) ||
        capacidad > SIZE_MAX / sizeof(int)) {
        fprintf(stderr, "Los valores son demasiado grandes.\n");
        return EXIT_FAILURE;
    }

    Buffer b = {0};

    b.capacidad = capacidad;
    b.total = total;

    b.buffer = malloc(capacidad * sizeof(int));
    b.frecuencias = calloc(total, sizeof(size_t));

    pthread_t *hilosProductores =
        malloc(numProductores * sizeof(pthread_t));
    pthread_t *hilosConsumidores =
        malloc(numConsumidores * sizeof(pthread_t));

    Argumentos *argsProductores =
        malloc(numProductores * sizeof(Argumentos));
    Argumentos *argsConsumidores =
        malloc(numConsumidores * sizeof(Argumentos));

    if (!b.buffer || !b.frecuencias ||
        !hilosProductores || !hilosConsumidores ||
        !argsProductores || !argsConsumidores) {
        fprintf(stderr, "Error al reservar memoria.\n");

        free(b.buffer);
        free(b.frecuencias);
        free(hilosProductores);
        free(hilosConsumidores);
        free(argsProductores);
        free(argsConsumidores);

        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&b.mutex, NULL) != 0) {
        fprintf(stderr, "Error al inicializar el mutex.\n");
        return EXIT_FAILURE;
    }

    if (pthread_cond_init(&b.no_lleno, NULL) != 0) {
        fprintf(stderr, "Error al inicializar no_lleno.\n");
        pthread_mutex_destroy(&b.mutex);
        return EXIT_FAILURE;
    }

    if (pthread_cond_init(&b.no_vacio, NULL) != 0) {
        fprintf(stderr, "Error al inicializar no_vacio.\n");
        pthread_cond_destroy(&b.no_lleno);
        pthread_mutex_destroy(&b.mutex);
        return EXIT_FAILURE;
    }

    size_t productoresCreados = 0;
    size_t consumidoresCreados = 0;

    for (size_t i = 0; i < numConsumidores; i++) {
        argsConsumidores[i] = (Argumentos){&b, (int)i};

        if (pthread_create(&hilosConsumidores[i], NULL,
                           consumidor, &argsConsumidores[i]) != 0) {
            fprintf(stderr, "Error al crear consumidor.\n");
            break;
        }

        consumidoresCreados++;
    }

    if (consumidoresCreados != numConsumidores) {
        pthread_mutex_lock(&b.mutex);
        b.total = 0;
        pthread_cond_broadcast(&b.no_vacio);
        pthread_cond_broadcast(&b.no_lleno);
        pthread_mutex_unlock(&b.mutex);

        for (size_t i = 0; i < consumidoresCreados; i++) {
            pthread_join(hilosConsumidores[i], NULL);
        }

        pthread_cond_destroy(&b.no_vacio);
        pthread_cond_destroy(&b.no_lleno);
        pthread_mutex_destroy(&b.mutex);

        free(b.buffer);
        free(b.frecuencias);
        free(hilosProductores);
        free(hilosConsumidores);
        free(argsProductores);
        free(argsConsumidores);

        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < numProductores; i++) {
        argsProductores[i] = (Argumentos){&b, (int)i};

        if (pthread_create(&hilosProductores[i], NULL,
                           productor, &argsProductores[i]) != 0) {
            fprintf(stderr, "Error al crear productor.\n");
            break;
        }

        productoresCreados++;
    }

    for (size_t i = 0; i < productoresCreados; i++) {
        pthread_join(hilosProductores[i], NULL);
    }

    pthread_mutex_lock(&b.mutex);
    pthread_cond_broadcast(&b.no_vacio);
    pthread_cond_broadcast(&b.no_lleno);
    pthread_mutex_unlock(&b.mutex);

    for (size_t i = 0; i < consumidoresCreados; i++) {
        pthread_join(hilosConsumidores[i], NULL);
    }

    int correcto = productoresCreados == numProductores &&
                   b.producidos == total &&
                   b.consumidos == total &&
                   b.cantidad == 0;

    for (size_t i = 0; i < total; i++) {
        if (b.frecuencias[i] != 1) {
            correcto = 0;
            break;
        }
    }

    printf("\n============================================\n");
    printf(" PRODUCTORES Y CONSUMIDORES\n");
    printf("============================================\n");
    printf("Productores:          %zu\n", numProductores);
    printf("Consumidores:         %zu\n", numConsumidores);
    printf("Capacidad del buffer: %zu\n", capacidad);
    printf("Elementos esperados:  %zu\n", total);
    printf("Elementos producidos: %zu\n", b.producidos);
    printf("Elementos consumidos: %zu\n", b.consumidos);
    printf("Elementos restantes:  %zu\n", b.cantidad);
    printf("Verificacion:         %s\n",
           correcto ? "CORRECTA" : "INCORRECTA");
    printf("============================================\n");

    pthread_cond_destroy(&b.no_vacio);
    pthread_cond_destroy(&b.no_lleno);
    pthread_mutex_destroy(&b.mutex);

    free(b.buffer);
    free(b.frecuencias);
    free(hilosProductores);
    free(hilosConsumidores);
    free(argsProductores);
    free(argsConsumidores);

    return correcto ? EXIT_SUCCESS : EXIT_FAILURE;
}
