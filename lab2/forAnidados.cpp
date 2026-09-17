#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void inicializar(double *A, double *x, double *y, int n) {
    for (int i = 0; i < n; i++) {
        x[i] = 1.0;
        y[i] = 0.0;

        for (int j = 0; j < n; j++)
            A[i * n + j] = 1.0;
    }
}

void por_filas(double *A, double *x, double *y, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            y[i] += A[i * n + j] * x[j];
}

void por_columnas(double *A, double *x, double *y, int n) {
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            y[i] += A[i * n + j] * x[j];
}

int main() {
    int tamanos[] = {64, 256, 512, 1024};

    for (int k = 0; k < 4; k++) {
        int n = tamanos[k];

        double *A = new double[n * n];
        double *x = new double[n];
        double *y = new double[n];

        inicializar(A, x, y, n);

        clock_t inicio = clock();
        por_filas(A, x, y, n);
        clock_t fin = clock();

        double tiempo_filas = (double)(fin - inicio) / CLOCKS_PER_SEC;

        inicializar(A, x, y, n);

        inicio = clock();
        por_columnas(A, x, y, n);
        fin = clock();

        double tiempo_columnas = (double)(fin - inicio) / CLOCKS_PER_SEC;

        printf("N = %d | Filas: %.5f s | Columnas: %.5f s\n",
               n, tiempo_filas, tiempo_columnas);

        delete[] A;
        delete[] x;
        delete[] y;
    }

    return 0;
}
