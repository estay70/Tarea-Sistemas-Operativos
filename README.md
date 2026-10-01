# Planificador Dieciochero — Tarea 1, Sistemas Operativos

Simulador y planificador de actividades para las Fiestas Patrias del señor Loyola,
modelado como un DAG (Grafo Acíclico Dirigido) donde cada actividad corre en su
propio proceso, con un límite de concurrencia configurable y propagación de
mensajes entre actividades dependientes vía pipes.

## Integrantes

- [Martín Estay] 
- [Diego Peñailillo] 

## Estructura del proyecto

```
parser.hpp       -> Declaración de Actividad, Grafo y parsearPlan()
parser1.cpp      -> Implementación del parseo de plan.txt
gestor_dag.hpp   -> Declaración de la clase GestorDag y el enum Estado
gestor_dag.cpp   -> Implementación de la máquina de estados del DAG
main.cpp         -> Orquestador: fork(), pipes, límite K, manejo de SIGINT
README.md        -> Este archivo
```

## Compilación

Según lo exigido en la rúbrica:

```bash
g++ -Wall -Wextra -std=c++17 -lpthread -o planificador parser1.cpp gestor_dag.cpp main.cpp
```

> Nota: no usamos hilos ni mecanismos de sincronización de hilos en ninguna
> parte del programa (todo el control de concurrencia vive en el proceso padre,
> vía `fork()`/`waitpid()`). El flag `-lpthread` se incluye solo porque la
> rúbrica lo pide como parte del comando de compilación estándar del curso.

## Ejecución

```bash
./planificador plan.txt K
```

- `plan.txt`: archivo de texto con las actividades a simular (ver formato abajo).
- `K`: entero positivo, número máximo de actividades que pueden correr
  simultáneamente (como procesos).

Ejemplo:
```bash
./planificador plan.txt 2
```

## Formato de `plan.txt`

Cada línea describe una actividad:

```
ID_Actividad : Nombre_Actividad : tiempo_ms : [Dependencia1, Dependencia2, ...]
```

- `tiempo_ms` puede dejarse vacío; en ese caso se asigna aleatoriamente un
  valor entre 100 y 5000 ms.
- Las dependencias se aceptan con o sin corchetes, separadas por coma.
- Una actividad sin dependencias simplemente deja el último campo vacío.

Ejemplo:
```
1 : prender_carbon : 500 :
2 : comprar_carne : 1200 :
3 : comprar_pan : 300 :
4 : asar_longaniza : 800 : 1, 2
5 : armar_choripan : 250 : 3, 4
6 : servir_mesa : 100 : 5
```

## Funciones implementadas

### `parser.hpp` / `parser1.cpp`

- **`struct Actividad`**: representa un nodo del DAG (id, nombre, tiempo en ms,
  lista de ids de los que depende).
- **`struct Grafo`**: contiene el mapa de actividades por id, el grafo inverso
  (quién depende de quién) y el conteo de dependencias pendientes por nodo.
- **`Grafo parsearPlan(const string& path)`**: lee `plan.txt` línea por línea,
  separa los campos por `:`, limpia espacios en blanco (`trim`), genera un
  tiempo aleatorio en `[100, 5000]` ms cuando el campo viene vacío
  (`tiempoAleatorio`, con `std::mt19937`), y parsea la lista de dependencias
  aceptando corchetes opcionales (`parsearDeps`). Arma la estructura `Grafo`
  completa, incluyendo el grafo inverso (`dependencias`) y el conteo inicial
  de dependencias pendientes por nodo (`act_restantes`).

  Lanza `std::runtime_error` (con el número de línea y el motivo) ante:
  - una línea con menos de 3 campos (formato inválido),
  - id o nombre vacíos,
  - `tiempo_ms` no numérico o negativo,
  - un id de actividad duplicado,
  - una dependencia que referencia un id que no existe en el plan.

  **Nota importante:** `parsearPlan` *no* detecta ciclos de dependencias —
  esa validación ocurre más adelante, en `GestorDag::inicializar()`, mediante
  un ordenamiento topológico (algoritmo de Kahn). Si el plan tiene un ciclo,
  el parseo se completa sin error, pero `inicializar()` lo detecta y hace
  fallar la inicialización del gestor.

### `gestor_dag.hpp` / `gestor_dag.cpp`

Lleva el progreso de ejecución del DAG, separado de su estructura fija:

- **`enum class Estado`**: `PENDIENTE`, `LISTA`, `CORRIENDO`, `HECHA`,
  `FALLIDA`, `ABORTADA` — ciclo de vida de cada actividad.
- **`inicializar(const Grafo&)`**: construye el estado inicial de todas las
  actividades y valida que el grafo sea efectivamente un DAG (sin ciclos),
  usando un algoritmo de ordenamiento topológico (Kahn).
- **`nodosListos()`**: retorna los ids de las actividades que ya pueden
  lanzarse (sin dependencias pendientes).
- **`marcarCorriendo(id)`**: marca una actividad como actualmente en ejecución.
- **`marcarTerminada(g, id, exito)`**: registra el resultado de una actividad.
  Si tuvo éxito, decrementa el contador de dependencias pendientes de sus
  dependientes directos y marca como `LISTA` a los que queden en cero. Si
  falló, marca la actividad como `FALLIDA` y aborta recursivamente **solo**
  la rama que dependía de ella.
- **`abortarDescendencia(g, id)`**: propaga el estado `ABORTADA` a todos los
  descendientes (directos e indirectos) de una actividad fallida.
- **`todasTerminadas()`**: indica si ya no queda ninguna actividad pendiente,
  lista o corriendo.
- **`imprimirEstados()`**: imprime el estado final de cada actividad (usado al
  terminar el programa o al ser interrumpido).

### `main.cpp`

- **`correrActividad(act, fd_in_read, fd_out_write)`**: código que ejecuta el
  **proceso hijo** de una actividad. Lee el "insumo" enviado por el padre a
  través del pipe de entrada, simula el trabajo con `usleep(tiempo_ms)`, y
  escribe un mensaje de resultado acotado (tamaño fijo, no acumulativo) por el
  pipe de salida. Para poder probar el aislamiento de errores sin depender de
  fallas reales del sistema, cada actividad tiene una probabilidad fija del
  **5%** de "fallar" simuladamente (`rand_r` con semilla propia por proceso,
  derivada de su `pid`, para que cada hijo tenga una secuencia de números
  aleatorios independiente). Si falla, el proceso termina con código de
  salida 1 (`_exit(1)`); si tiene éxito, con código 0.
- **`lanzar(id)`** (lambda dentro de `main`): crea los dos pipes de la
  actividad (entrada/salida), hace `fork()`, y en el proceso padre le pasa el
  insumo ya concatenado de sus dependencias (que ya terminaron, porque la
  actividad está `LISTA`).
- **Loop principal**: mientras no estén todas terminadas, lanza actividades
  listas hasta llenar el cupo `K`, y espera (`waitpid` bloqueante) a que algún
  hijo termine para actualizar el `GestorDag` y repetir.
- **`manejarSigint` / `instalarManejadorSigint`**: capturan `SIGINT` (Ctrl+C)
  seteando una bandera `volatile sig_atomic_t`. Al detectarla, el loop
  principal mata (`SIGTERM`) a todos los procesos hijos activos, espera que
  terminen (`waitpid`) para no dejar zombies, e imprime el estado final antes
  de salir con código 130.

## Decisiones de diseño

- **Un solo proceso orquestador, sin hilos**: toda la lógica de "qué lanzar,
  cuándo y cuántos" vive en el proceso padre. Esto evita cualquier necesidad
  de sincronización entre hilos (prohibida por el enunciado) porque solo hay
  un flujo de control decidiendo el estado compartido (`GestorDag`); los
  procesos hijos son "tontos" y no comparten memoria con el padre ni entre sí.
- **Sin busy-waiting**: el padre usa `waitpid(-1, &status, 0)` **bloqueante**
  para esperar a que termine algún hijo, en vez de hacer polling con
  `WNOHANG` en un loop. Esto evita que el proceso consuma CPU innecesariamente
  mientras espera.
- **Dos pipes por actividad**: uno de entrada (el padre escribe el insumo ya
  recolectado de las dependencias, el hijo lo lee al iniciar) y uno de salida
  (el hijo escribe su resultado antes de salir, el padre lo lee tras detectar
  que el hijo terminó). El padre cierra siempre los extremos que no usa
  inmediatamente después del `fork()`, para evitar bloqueos por descriptores
  de archivo abiertos de más.
- **Mensajes acotados**: el mensaje que cada actividad propaga contiene solo
  información sobre sí misma (ej. `"OK:nombre_actividad"`), nunca reenvía el
  contenido completo que recibió. Esto evita que el tamaño del mensaje crezca
  sin control en cadenas largas de dependencias (importante con la carga de
  estrés de hasta 10.000 actividades) y evita un posible deadlock de pipe si
  el mensaje llegara a superar el buffer del sistema (~64KB en Linux).
- **Aislamiento de errores vía `GestorDag`**: la propagación de fallas no
  depende de que el proceso padre revise el DAG completo cada vez — el estado
  `ABORTADA` se propaga recursivamente solo por el grafo inverso
  (`dependientes`/`dependencias`), tocando únicamente los nodos realmente
  afectados.
- **Aleatoriedad independiente por proceso**: cada actividad usa `rand_r()`
  con una semilla propia derivada de su `pid` (en vez de `rand()` global), para
  que los números aleatorios de cada proceso hijo sean independientes entre
  sí — como cada actividad corre en un proceso separado (memoria separada por
  `fork()`), usar el generador global `rand()` sin resembrar podría producir
  la misma secuencia de "éxito/fracaso" en procesos con el mismo estado
  heredado del padre.
- **Control de concurrencia sin semáforos ni memoria compartida**: como solo
  el proceso padre decide cuándo lanzar una nueva actividad, basta con un
  contador entero local (`corriendo`) en su propio espacio de memoria — no se
  necesita ningún primitivo de sincronización entre procesos para respetar
  el límite `K`.

## Pruebas realizadas

- **Límite de concurrencia (K)**: verificado contando procesos hijos activos
  con `ps -ef` mientras corre un plan con 6 actividades independientes y
  tiempos largos — nunca se observaron más de `K` procesos hijos simultáneos.
- **SIGINT**: interrumpido el programa a mitad de ejecución con Ctrl+C;
  confirmado que todos los procesos hijos activos terminan (`pgrep` no
  muestra procesos huérfanos ni zombies después de la interrupción).
- **Carga de estrés**: ejecutado un plan generado con 10.000 actividades
  encadenadas (con algo de ramificación aleatoria) y `K=50`; las 10.000
  terminaron en estado `HECHA`, sin cuelgues, en un tiempo acorde a la
  longitud de la cadena de dependencias.
- **Aislamiento de errores**: probado con un plan de 16 actividades organizado
  en 4 ramas independientes (cada una de 4 niveles: raíz → hijo → nieto), con
  la probabilidad de falla del 5% activa. En una corrida donde falló un nodo
  intermedio (`hijo_b1`), el resultado fue:
  - `hijo_b1` → `FALLIDA`
  - `hijo_b2` (dependía directamente de `hijo_b1`) → `ABORTADA`, nunca se
    llegó a lanzar su proceso
  - `nieto_b` (dependía de `hijo_b2`) → `ABORTADA` en cascada, aunque
    dependía solo indirectamente del nodo que falló
  - Las otras 3 ramas completas (`raiz_a`/`c`/`d` y toda su descendencia)
    terminaron `HECHA` sin verse afectadas en absoluto.

  Esto confirma que la falla de una actividad solo aborta la rama que
  depende de ella (directa o transitivamente), y el resto del plan continúa
  con normalidad — tal como exige el punto 4.1 del enunciado.
