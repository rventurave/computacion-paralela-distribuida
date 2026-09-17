#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <thread>
#include <algorithm>

using namespace std;
using namespace std::chrono;


// ============================================================
// Calcula la suma parcial de un bloque del vector
// ============================================================

void calcularSumaParcial(
    const vector<double>& A,
    size_t inicio,
    size_t fin,
    double& sumaParcial)
{
    sumaParcial = 0.0;

    for (size_t i = inicio; i < fin; ++i)
    {
        double B = A[i] * A[i]
                 + 3.0 * A[i]
                 + 1.0;

        sumaParcial += B;
    }
}


// ============================================================
// Procesamiento paralelo
// ============================================================

double calcularSumaParalela(
    const vector<double>& A,
    int cantidadThreads)
{
    vector<double> sumasParciales(
        cantidadThreads,
        0.0
    );

    vector<thread> threads;

    size_t N = A.size();

    size_t elementosPorThread =
        N / cantidadThreads;


    for (int i = 0; i < cantidadThreads; ++i)
    {
        size_t inicio =
            i * elementosPorThread;

        size_t fin;

        // El último thread procesa los elementos restantes
        if (i == cantidadThreads - 1)
        {
            fin = N;
        }
        else
        {
            fin = inicio + elementosPorThread;
        }


        threads.emplace_back(
            calcularSumaParcial,
            cref(A),
            inicio,
            fin,
            ref(sumasParciales[i])
        );
    }


    // Esperar a todos los threads
    for (auto& t : threads)
    {
        t.join();
    }


    // Combinar las sumas parciales
    double sumaFinal = 0.0;

    for (int i = 0;
         i < cantidadThreads;
         ++i)
    {
        sumaFinal += sumasParciales[i];
    }

    return sumaFinal;
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // --------------------------------------------------------
    // Configuración
    // --------------------------------------------------------

    const int EJECUCIONES = 1;

    const int MAX_THREADS_LAB = 32;


    // Tamaños solicitados por el laboratorio
    vector<size_t> tamanos =
    {
        1'000'000,
        10'000'000,
        100'000'000
    };


    // --------------------------------------------------------
    // Detectar hilos disponibles
    // --------------------------------------------------------

    unsigned int hilosHardware =
        thread::hardware_concurrency();


    if (hilosHardware == 0)
    {
        cout << "No se pudo detectar la cantidad de "
             << "hilos disponibles.\n";

        return 1;
    }


    int limiteThreads =
        min(
            static_cast<int>(hilosHardware),
            MAX_THREADS_LAB
        );


    // --------------------------------------------------------
    // Información del sistema
    // --------------------------------------------------------

    cout << "\n";
    cout << "============================================================\n";
    cout << "       EXPERIMENTO: TAMANO DEL PROBLEMA Y PARALELISMO\n";
    cout << "============================================================\n\n";


    cout << "Hilos logicos detectados : "
         << hilosHardware
         << "\n";

    cout << "Maximo permitido : "
         << MAX_THREADS_LAB
         << "\n";

    cout << "Maximo utilizado : "
         << limiteThreads
         << "\n";

    cout << "Ejecuciones por prueba : "
         << EJECUCIONES
         << "\n\n";


    // --------------------------------------------------------
    // Determinar threads a utilizar
    // --------------------------------------------------------

    vector<int> listaThreads;


    for (int threads = 1;
         threads <= MAX_THREADS_LAB;
         threads *= 2)
    {
        if (threads <= limiteThreads)
        {
            listaThreads.push_back(threads);
        }
    }


    // Si el límite no es potencia de 2,
    // agregarlo como última prueba
    if (!listaThreads.empty() &&
        listaThreads.back() != limiteThreads)
    {
        listaThreads.push_back(limiteThreads);
    }


    // ========================================================
    // EXPERIMENTO PARA CADA TAMAÑO
    // ========================================================

    for (size_t N : tamanos)
    {
        cout << "\n";
        cout << "============================================================\n";

        cout << "                N = "
             << N
             << " elementos\n";

        cout << "============================================================\n";


        // ----------------------------------------------------
        // Generar vector
        // ----------------------------------------------------

        cout << "Generando vector...\n";


        vector<double> A(N);


        mt19937 generador(12345);

        uniform_real_distribution<double>
            distribucion(-100.0, 100.0);


        for (size_t i = 0; i < N; ++i)
        {
            A[i] =
                distribucion(generador);
        }


        cout << "Vector generado correctamente.\n\n";


        // ----------------------------------------------------
        // Variables de resultados
        // ----------------------------------------------------

        vector<double> tiemposPromedio;

        vector<double> speedups;

        vector<double> eficiencias;


        double tiempoUnThread = 0.0;


        // ----------------------------------------------------
        // Ejecutar con diferentes cantidades de threads
        // ----------------------------------------------------

        for (int cantidadThreads : listaThreads)
        {
            double tiempoTotal = 0.0;

            double sumaFinal = 0.0;


            cout << "Threads: "
                 << cantidadThreads
                 << "\n";


            // ------------------------------------------------
            // Repetir ejecución
            // ------------------------------------------------

            for (int ejecucion = 1;
                 ejecucion <= EJECUCIONES;
                 ++ejecucion)
            {
                auto inicio =
                    high_resolution_clock::now();


                sumaFinal =
                    calcularSumaParalela(
                        A,
                        cantidadThreads
                    );


                auto fin =
                    high_resolution_clock::now();


                double tiempo =
                    duration<double, milli>(
                        fin - inicio
                    ).count();


                tiempoTotal += tiempo;


                cout << "  Ejecucion "
                     << ejecucion
                     << ": "
                     << fixed
                     << setprecision(3)
                     << tiempo
                     << " ms\n";
            }


            // ------------------------------------------------
            // Tiempo promedio
            // ------------------------------------------------

            double tiempoPromedio =
                tiempoTotal / EJECUCIONES;


            tiemposPromedio.push_back(
                tiempoPromedio
            );


            // ------------------------------------------------
            // Tiempo base
            // ------------------------------------------------

            if (cantidadThreads == 1)
            {
                tiempoUnThread =
                    tiempoPromedio;
            }


            // ------------------------------------------------
            // Speedup
            // ------------------------------------------------

            double speedup =
                tiempoUnThread /
                tiempoPromedio;


            speedups.push_back(
                speedup
            );


            // ------------------------------------------------
            // Eficiencia
            // ------------------------------------------------

            double eficiencia =
                speedup /
                cantidadThreads;


            eficiencias.push_back(
                eficiencia
            );


            cout << "  Tiempo promedio: "
                 << fixed
                 << setprecision(3)
                 << tiempoPromedio
                 << " ms\n";

            cout << "  Suma final: "
                 << setprecision(7)
                 << sumaFinal
                 << "\n\n";
        }


        // ====================================================
        // TABLA DE RESULTADOS
        // ====================================================

        cout << "\n";
        cout << "------------------------------------------------------------\n";

        cout << "Resultados para N = "
             << N
             << "\n";

        cout << "------------------------------------------------------------\n";


        cout << left
             << setw(12)
             << "Threads"

             << setw(18)
             << "Tiempo (s)"

             << setw(15)
             << "Speedup"

             << setw(15)
             << "Eficiencia"

             << "\n";


        cout << "------------------------------------------------------------\n";


        for (size_t i = 0;
             i < listaThreads.size();
             ++i)
        {
            double tiempoSegundos =
                tiemposPromedio[i] / 1000.0;


            cout << left
                 << setw(12)
                 << listaThreads[i]

                 << setw(18)
                 << fixed
                 << setprecision(6)
                 << tiempoSegundos

                 << setw(15)
                 << setprecision(3)
                 << speedups[i]

                 << setw(15)
                 << setprecision(3)
                 << eficiencias[i]

                 << "\n";
        }


        cout << "------------------------------------------------------------\n";


        // Liberar memoria antes del siguiente tamaño
        A.clear();
        A.shrink_to_fit();
    }


    // ========================================================
    // FINAL
    // ========================================================

    cout << "\n";
    cout << "============================================================\n";
    cout << "              EXPERIMENTO FINALIZADO\n";
    cout << "============================================================\n";


    cout << "\nFormulas utilizadas:\n\n";

    cout << "Speedup:\n";
    cout << "Sp = T1 / Tp\n\n";

    cout << "Eficiencia:\n";
    cout << "Ep = Sp / p\n\n";


    return 0;
}
