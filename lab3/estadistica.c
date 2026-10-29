#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#define MAX_HILOS 8

typedef struct {
    const int *datos;
    size_t inicio;
    size_t fin;
    int64_t suma;
    int minimo;
    int maximo;
} Trabajo;

typedef struct {
    int64_t suma;
    int minimo;
    int maximo;
    double promedio;
} Resultado;

double obtenerTiempo(void) {
    struct timespec tiempo;
    clock_gettime(CLOCK_MONOTONIC, &tiempo);
    return (double)tiempo.tv_sec + (double)tiempo.tv_nsec / 1e9;
}

int resultadosIguales(Resultado a, Resultado b) {
    return a.suma == b.suma &&
           a.minimo == b.minimo &&
           a.maximo == b.maximo &&
           fabs(a.promedio - b.promedio) < 1e-9;
}

Resultado secuencial(const int *datos, size_t n) {
    Resultado resultado = {0, INT_MAX, INT_MIN, 0.0};

    for (size_t i = 0; i < n; i++) {
        resultado.suma += datos[i];

        if (datos[i] < resultado.minimo)
            resultado.minimo = datos[i];

        if (datos[i] > resultado.maximo)
            resultado.maximo = datos[i];
    }

    resultado.promedio = (double)resultado.suma / (double)n;

    return resultado;
}

void *calcularParcial(void *argumento) {
    Trabajo *trabajo = (Trabajo *)argumento;

    trabajo->suma = 0;
    trabajo->minimo = INT_MAX;
    trabajo->maximo = INT_MIN;

    for (size_t i = trabajo->inicio; i < trabajo->fin; i++) {
        int valor = trabajo->datos[i];

        trabajo->suma += valor;

        if (valor < trabajo->minimo)
            trabajo->minimo = valor;

        if (valor > trabajo->maximo)
            trabajo->maximo = valor;
    }

    return NULL;
}

Resultado paralelo(const int *datos, size_t n, int cantidad) {
    Resultado resultado = {0, INT_MAX, INT_MIN, 0.0};

    pthread_t *hilos = malloc((size_t)cantidad * sizeof(pthread_t));
    Trabajo *trabajos = malloc((size_t)cantidad * sizeof(Trabajo));

    if (hilos == NULL || trabajos == NULL) {
        perror("Error al reservar memoria");
        free(hilos);
        free(trabajos);
        exit(EXIT_FAILURE);
    }

    size_t base = n / (size_t)cantidad;
    size_t resto = n % (size_t)cantidad;
    int creados = 0;

    for (int i = 0; i < cantidad; i++) {
        trabajos[i].datos = datos;
        trabajos[i].inicio = base * (size_t)i +
            (resto * (size_t)i) / (size_t)cantidad;
        trabajos[i].fin = base * (size_t)(i + 1) +
            (resto * (size_t)(i + 1)) / (size_t)cantidad;

        int error = pthread_create(
            &hilos[i], NULL, calcularParcial, &trabajos[i]
        );

        if (error != 0) {
            fprintf(stderr, "Error al crear hilo: %s\n", strerror(error));

            for (int j = 0; j < creados; j++)
                pthread_join(hilos[j], NULL);

            free(hilos);
            free(trabajos);
            exit(EXIT_FAILURE);
        }

        creados++;
    }

    for (int i = 0; i < creados; i++) {
        int error = pthread_join(hilos[i], NULL);

        if (error != 0) {
            fprintf(stderr, "Error al esperar hilo: %s\n", strerror(error));
            free(hilos);
            free(trabajos);
            exit(EXIT_FAILURE);
        }

        resultado.suma += trabajos[i].suma;

        if (trabajos[i].minimo < resultado.minimo)
            resultado.minimo = trabajos[i].minimo;

        if (trabajos[i].maximo > resultado.maximo)
            resultado.maximo = trabajos[i].maximo;
    }

    resultado.promedio = (double)resultado.suma / (double)n;

    free(hilos);
    free(trabajos);

    return resultado;
}

void ejecutarAnalisis(size_t n) {
    if (n == 0 || n > SIZE_MAX / sizeof(int) ||
        n > (size_t)(INT64_MAX / 999)) {
        printf("Tamano no valido: %zu\n", n);
        return;
    }

    int *datos = malloc(n * sizeof(int));

    if (datos == NULL) {
        perror("Error al reservar memoria para los datos");
        return;
    }

    unsigned int semilla = 12345;

    for (size_t i = 0; i < n; i++) {
        semilla = semilla * 1103515245u + 12345u;
        datos[i] = (int)((semilla >> 16) % 1000);
    }

    double inicio = obtenerTiempo();
    Resultado referencia = secuencial(datos, n);
    double tiempoSecuencial = obtenerTiempo() - inicio;

    printf("\n========================================\n");
    printf("TAMANO N = %zu\n", n);
    printf("========================================\n");
    printf("RESULTADO SECUENCIAL\n");
    printf("Suma: %" PRId64 "\n", referencia.suma);
    printf("Minimo: %d\n", referencia.minimo);
    printf("Maximo: %d\n", referencia.maximo);
    printf("Promedio: %.4f\n", referencia.promedio);
    printf("Tiempo secuencial: %.9f segundos\n", tiempoSecuencial);

    printf("\n%-8s %-16s %-12s %-14s %-10s\n",
           "Hilos", "Tiempo (s)", "Speedup", "Eficiencia", "Correcto");

    int cantidades[] = {1, 2, 4, 8};

    for (size_t i = 0; i < sizeof(cantidades) / sizeof(cantidades[0]); i++) {
        int cantidad = cantidades[i];

        inicio = obtenerTiempo();
        Resultado resultado = paralelo(datos, n, cantidad);
        double tiempoParalelo = obtenerTiempo() - inicio;

        double speedup = tiempoParalelo > 0.0
            ? tiempoSecuencial / tiempoParalelo
            : 0.0;

        double eficiencia = speedup / (double)cantidad;

        printf("%-8d %-16.9f %-12.4f %-14.4f %-10s\n",
               cantidad,
               tiempoParalelo,
               speedup,
               eficiencia,
               resultadosIguales(referencia, resultado) ? "SI" : "NO");
    }

    free(datos);
}

void procesarArchivo(const char *nombreArchivo) {
    FILE *archivo = fopen(nombreArchivo, "r");

    if (archivo == NULL) {
        perror("No se pudo abrir data.txt");
        return;
    }

    char linea[256];
    size_t numeroLinea = 0;

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        numeroLinea++;

        char *inicio = linea;

        while (isspace((unsigned char)*inicio))
            inicio++;

        if (*inicio == '\0' || *inicio == '#')
            continue;

        errno = 0;
        char *fin;
        uintmax_t valor = strtoumax(inicio, &fin, 10);

        while (isspace((unsigned char)*fin))
            fin++;

        if (errno != 0 || inicio == fin || *fin != '\0' ||
            valor == 0 || valor > SIZE_MAX) {
            printf("Linea %zu invalida: %s", numeroLinea, linea);
            continue;
        }

        ejecutarAnalisis((size_t)valor);
    }

    if (ferror(archivo))
        perror("Error al leer data.txt");

    fclose(archivo);
}

int main(void) {
    procesarArchivo("data.txt");
    return 0;
}
