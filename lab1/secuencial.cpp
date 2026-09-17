#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

double calcularSumaSecuencial(const vector<double>& A)
{
    double suma = 0.0;

    for (size_t i = 0; i < A.size(); ++i)
    {
        double B = A[i] * A[i] + 3.0 * A[i] + 1.0;

        suma += B;
    }

    return suma;
}

int main()
{
    const size_t N = 10'000'000;

    const int EJECUCIONES = 5;

    cout << "   IMPLEMENTACION SECUENCIAL\n";

    cout << "N = " << N << " elementos\n";
    cout << "Ejecuciones = " << EJECUCIONES << "\n\n";

    // 1. Generación del vector A

    vector<double> A(N);
    mt19937 generador(12345);

    uniform_real_distribution<double> distribucion(-100.0, 100.0);

    for (size_t i = 0; i < N; ++i)
    {
        A[i] = distribucion(generador);
    }

    cout << "Vector A generado correctamente.\n";

    // 2. Ejecuciones para medir el tiempo

    double sumaFinal = 0.0;
    double tiempoTotal = 0.0;

    for (int ejecucion = 1; ejecucion <= EJECUCIONES; ++ejecucion)
    {
        auto inicio = high_resolution_clock::now();

        sumaFinal = calcularSumaSecuencial(A);

        auto fin = high_resolution_clock::now();

        // Duración en milisegundos
        double tiempo =
            duration<double, milli>(fin - inicio).count();

        tiempoTotal += tiempo;

        cout << "Ejecucion " << ejecucion
             << ": " << fixed << setprecision(3)
             << tiempo << " ms\n";
      cout << "Suma final de la ejecucion ["<< ejecucion << "]: "<< setprecision(7) << sumaFinal << '\n';
    }

    // 3. Resultados

    double tiempoPromedio = tiempoTotal / EJECUCIONES;

    cout << "RESULTADOS\n";

    cout << setprecision(7);
    cout << "Suma final S = " << sumaFinal << "\n";

    cout << fixed << setprecision(3);
    cout << "Tiempo promedio = "<< tiempoPromedio << " ms\n";

    return 0;
}
