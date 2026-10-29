#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#define CATEGORIAS 10
#define MAX_HILOS 8

typedef struct {
    const int *datos;
    size_t inicio;
    size_t fin;
    size_t *histograma;
    pthread_mutex_t *mutex;
} Argumentos;

double obtenerTiempo(struct timespec inicio, struct timespec fin) {
    return (double)(fin.tv_sec - inicio.tv_sec) +
           (double)(fin.tv_nsec - inicio.tv_nsec) / 1e9;
}

int leerTamanos(const char *archivo, size_t **tamanos,
                size_t *cantidad) {
    FILE *f = fopen(archivo, "r");

    if (f == NULL) {
        perror("Error al abrir data.txt");
        return 0;
    }

    size_t capacidad = 10;
    *cantidad = 0;
    *tamanos = malloc(capacidad * sizeof(size_t));

    if (*tamanos == NULL) {
        perror("Error al reservar memoria");
        fclose(f);
        return 0;
    }

    unsigned long long n;

    while (fscanf(f, "%llu", &n) == 1) {
        if (n == 0 || n > SIZE_MAX / sizeof(int) || n > INT_MAX) {
            fprintf(stderr, "Tamano invalido: %llu\n", n);
            free(*tamanos);
            *tamanos = NULL;
            fclose(f);
            return 0;
        }

        if (*cantidad == capacidad) {
            if (capacidad > SIZE_MAX / 2 / sizeof(size_t)) {
                fprintf(stderr, "Demasiados tamanos en el archivo.\n");
                free(*tamanos);
                *tamanos = NULL;
                fclose(f);
                return 0;
            }

            capacidad *= 2;

            size_t *temp = realloc(
                *tamanos, capacidad * sizeof(size_t)
            );

            if (temp == NULL) {
                perror("Error al ampliar memoria");
                free(*tamanos);
                *tamanos = NULL;
                fclose(f);
                return 0;
            }

            *tamanos = temp;
        }

        (*tamanos)[(*cantidad)++] = (size_t)n;
    }

    if (ferror(f)) {
        fprintf(stderr, "Error al leer data.txt.\n");
        free(*tamanos);
        *tamanos = NULL;
        fclose(f);
        return 0;
    }

    fclose(f);

    if (*cantidad == 0) {
        fprintf(stderr, "data.txt no contiene tamanos validos.\n");
        free(*tamanos);
        *tamanos = NULL;
        return 0;
    }

    return 1;
}

void generarDatos(int *datos, size_t N) {
    for (size_t i = 0; i < N; i++) {
        datos[i] = rand() % 100;
    }
}

void histogramaSecuencial(const int *datos, size_t N,
                          size_t *histograma) {
    for (size_t i = 0; i < N; i++) {
        histograma[datos[i] / 10]++;
    }
}

int verificarHistograma(const size_t *histograma, size_t N) {
    size_t total = 0;

    for (int i = 0; i < CATEGORIAS; i++) {
        total += histograma[i];
    }

    return total == N;
}

void *histogramaConMutex(void *arg) {
    Argumentos *a = (Argumentos *)arg;

    for (size_t i = a->inicio; i < a->fin; i++) {
        int categoria = a->datos[i] / 10;

        pthread_mutex_lock(a->mutex);
        a->histograma[categoria]++;
        pthread_mutex_unlock(a->mutex);
    }

    return NULL;
}

void *histogramaPrivado(void *arg) {
    Argumentos *a = (Argumentos *)arg;

    for (size_t i = a->inicio; i < a->fin; i++) {
        int categoria = a->datos[i] / 10;
        a->histograma[categoria]++;
    }

    return NULL;
}

int histogramasIguales(const size_t *a, const size_t *b) {
    for (int i = 0; i < CATEGORIAS; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }

    return 1;
}

int ejecutarParalelo(
    const int *datos,
    size_t N,
    int numHilos,
    int usarMutex,
    size_t *resultado,
    double *tiempo
) {
    pthread_t hilos[MAX_HILOS];
    Argumentos args[MAX_HILOS];
    size_t privados[MAX_HILOS][CATEGORIAS] = {{0}};
    pthread_mutex_t mutex;

    if (usarMutex && pthread_mutex_init(&mutex, NULL) != 0) {
        fprintf(stderr, "Error al inicializar el mutex.\n");
        return 0;
    }

    struct timespec inicio, fin;
    clock_gettime(CLOCK_MONOTONIC, &inicio);

    int creados = 0;

    for (int t = 0; t < numHilos; t++) {
        args[t].datos = datos;

        args[t].inicio =
            (N / (size_t)numHilos) * (size_t)t
            + ((N % (size_t)numHilos) * (size_t)t)
            / (size_t)numHilos;

        args[t].fin =
            (N / (size_t)numHilos) * (size_t)(t + 1)
            + ((N % (size_t)numHilos) * (size_t)(t + 1))
            / (size_t)numHilos;

        args[t].histograma =
            usarMutex ? resultado : privados[t];

        args[t].mutex = usarMutex ? &mutex : NULL;

        void *(*funcion)(void *) =
            usarMutex ? histogramaConMutex : histogramaPrivado;

        if (pthread_create(&hilos[t], NULL, funcion, &args[t]) != 0) {
            fprintf(stderr, "Error al crear el hilo %d.\n", t);
            break;
        }

        creados++;
    }

    int correcto = (creados == numHilos);

    for (int t = 0; t < creados; t++) {
        if (pthread_join(hilos[t], NULL) != 0) {
            fprintf(stderr, "Error al esperar el hilo %d.\n", t);
            correcto = 0;
        }
    }

    if (!correcto) {
        if (usarMutex) {
            pthread_mutex_destroy(&mutex);
        }

        return 0;
    }

    if (!usarMutex) {
        for (int t = 0; t < numHilos; t++) {
            for (int c = 0; c < CATEGORIAS; c++) {
                resultado[c] += privados[t][c];
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &fin);
    *tiempo = obtenerTiempo(inicio, fin);

    if (usarMutex) {
        pthread_mutex_destroy(&mutex);
    }

    return 1;
}

int main(void) {
    size_t *tamanos = NULL;
    size_t cantidad = 0;

    if (!leerTamanos("data.txt", &tamanos, &cantidad)) {
        return EXIT_FAILURE;
    }

    srand((unsigned int)time(NULL));

    const int configuraciones[] = {1, 2, 4, 8};

    printf("\n");
    printf("===============================================================================================\n");
    printf("                         HISTOGRAMA SECUENCIAL Y PARALELO\n");
    printf("===============================================================================================\n");

    printf("%-12s %-10s %-18s %-18s %-18s %-15s\n",
           "Tamano (N)", "Hilos", "Secuencial (s)",
           "Mutex (s)", "Privado (s)", "Verificacion");

    printf("-----------------------------------------------------------------------------------------------\n");

    for (size_t prueba = 0; prueba < cantidad; prueba++) {
        size_t N = tamanos[prueba];
        int *datos = malloc(N * sizeof(int));

        if (datos == NULL) {
            fprintf(stderr,
                    "No se pudo reservar memoria para N=%zu.\n", N);
            free(tamanos);
            return EXIT_FAILURE;
        }

        generarDatos(datos, N);

        size_t secuencial[CATEGORIAS] = {0};

        struct timespec inicio, fin;

        clock_gettime(CLOCK_MONOTONIC, &inicio);
        histogramaSecuencial(datos, N, secuencial);
        clock_gettime(CLOCK_MONOTONIC, &fin);

        double tiempoSecuencial = obtenerTiempo(inicio, fin);

        for (int k = 0; k < 4; k++) {
            int numHilos = configuraciones[k];

            size_t conMutex[CATEGORIAS] = {0};
            size_t combinado[CATEGORIAS] = {0};

            double tiempoMutex = 0.0;
            double tiempoPrivado = 0.0;

            if (!ejecutarParalelo(
                    datos, N, numHilos, 1,
                    conMutex, &tiempoMutex)) {
                free(datos);
                free(tamanos);
                return EXIT_FAILURE;
            }

            if (!ejecutarParalelo(
                    datos, N, numHilos, 0,
                    combinado, &tiempoPrivado)) {
                free(datos);
                free(tamanos);
                return EXIT_FAILURE;
            }

            int correcto =
                histogramasIguales(secuencial, conMutex) &&
                histogramasIguales(secuencial, combinado) &&
                verificarHistograma(secuencial, N) &&
                verificarHistograma(conMutex, N) &&
                verificarHistograma(combinado, N);

            if (!correcto) {
                fprintf(stderr,
                        "\nERROR: verificacion incorrecta para N=%zu "
                        "con %d hilos.\n",
                        N, numHilos);

                free(datos);
                free(tamanos);
                return EXIT_FAILURE;
            }

            printf("%-12zu %-10d %-18.9f %-18.9f %-18.9f %-15s\n",
                   N, numHilos, tiempoSecuencial,
                   tiempoMutex, tiempoPrivado,
                   correcto ? "SI" : "NO");
        }

        printf("-----------------------------------------------------------------------------------------------\n");
        free(datos);
    }

    free(tamanos);

    printf("\nPruebas finalizadas correctamente.\n");

    return EXIT_SUCCESS;
}
