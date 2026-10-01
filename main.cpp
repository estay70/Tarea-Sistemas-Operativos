#include "parser.hpp"
#include "gestor_dag.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

namespace {

volatile sig_atomic_t g_interrumpido = 0;

void manejarSigint(int) {
    g_interrumpido = 1;
}

void instalarManejadorSigint() {
    struct sigaction sa{};
    sa.sa_handler = manejarSigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // sin SA_RESTART: que waitpid() se interrumpa con EINTR
    sigaction(SIGINT, &sa, nullptr);
}

[[noreturn]] void correrActividad(const Actividad& act, int fd_in_read, int fd_out_write) {
    char buf[256];
    while (read(fd_in_read, buf, sizeof(buf)) > 0) { /* descartar */ }
    close(fd_in_read);

    usleep(static_cast<useconds_t>(act.tiempo_ms) * 1000);

    // Simulamos una falla aleatoria con 5% de probabilidad.
    static thread_local unsigned int semilla = static_cast<unsigned int>(getpid());
    bool exito = (rand_r(&semilla) % 100) >= 5;

    std::string msg = (exito ? "OK:" : "FAIL:") + act.nombre;
    write(fd_out_write, msg.data(), msg.size());
    close(fd_out_write);

    _exit(exito ? 0 : 1);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " plan.txt K\n";
        return 1;
    }

    std::string path = argv[1];
    int K = std::atoi(argv[2]);
    if (K < 1) {
        std::cerr << "K debe ser un entero positivo\n";
        return 1;
    }

    Grafo g;
    try {
        g = parsearPlan(path);
    } catch (const std::exception& e) {
        std::cerr << "Error al parsear el plan: " << e.what() << "\n";
        return 1;
    }

    GestorDag gestor;
    if (!gestor.inicializar(g)) {
        return 1; // GestorDag ya imprimió el motivo (ciclo detectado)
    }

    instalarManejadorSigint();

    std::unordered_map<std::string, std::string> mensajes; // id -> mensaje recibido al terminar
    std::unordered_map<pid_t, std::string> pidAId;
    std::unordered_map<std::string, int> fdOutDeId;
    int corriendo = 0;

    auto lanzar = [&](const std::string& id) {
        const Actividad& act = g.actividades.at(id);

        std::string entrada;
        for (const auto& dep : act.deps) {
            if (!entrada.empty()) entrada += "; ";
            entrada += mensajes[dep];
        }

        int fd_in[2], fd_out[2];
        if (pipe(fd_in) == -1 || pipe(fd_out) == -1) {
            std::cerr << "Error creando pipes para '" << id << "': " << std::strerror(errno) << "\n";
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error en fork para '" << id << "': " << std::strerror(errno) << "\n";
            close(fd_in[0]); close(fd_in[1]);
            close(fd_out[0]); close(fd_out[1]);
            return;
        }

        if (pid == 0) {
            close(fd_in[1]);
            close(fd_out[0]);
            correrActividad(act, fd_in[0], fd_out[1]); // nunca retorna
        }

        close(fd_in[0]);
        close(fd_out[1]);

        if (!entrada.empty()) write(fd_in[1], entrada.data(), entrada.size());
        close(fd_in[1]);

        gestor.marcarCorriendo(id);
        pidAId[pid] = id;
        fdOutDeId[id] = fd_out[0];
        corriendo++;
    };

    bool abortado_por_sigint = false;

    while (!gestor.todasTerminadas()) {
        if (g_interrumpido) { abortado_por_sigint = true; break; }

        for (const auto& id : gestor.nodosListos()) {
            if (corriendo >= K) break;
            lanzar(id);
        }

        if (corriendo == 0) break; // salvaguarda; no debería pasar con un DAG válido

        int status;
        pid_t pid = waitpid(-1, &status, 0);

        if (pid == -1) {
            if (errno == EINTR) {
                if (g_interrumpido) { abortado_por_sigint = true; break; }
                continue;
            }
            std::cerr << "Error en waitpid: " << std::strerror(errno) << "\n";
            break;
        }

        auto itId = pidAId.find(pid);
        if (itId == pidAId.end()) continue;
        std::string id = itId->second;
        pidAId.erase(itId);
        corriendo--;

        int fd_out_read = fdOutDeId[id];
        fdOutDeId.erase(id);

        std::string msg;
        char buf[256];
        ssize_t n;
        while ((n = read(fd_out_read, buf, sizeof(buf))) > 0) msg.append(buf, static_cast<size_t>(n));
        close(fd_out_read);
        mensajes[id] = msg;

        bool exito = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        gestor.marcarTerminada(g, id, exito);

        std::cout << (exito ? "[OK]    " : "[FALLO] ") << id << " (" << g.actividades.at(id).nombre << ")\n";
    }

    if (abortado_por_sigint) {
        std::cerr << "\n*** SIGINT recibido: abortando todas las actividades ***\n";
        for (const auto& [pid, id] : pidAId) kill(pid, SIGTERM);
        for (const auto& [pid, id] : pidAId) waitpid(pid, nullptr, 0);
        for (const auto& [id, fd] : fdOutDeId) close(fd);
    }

    std::cout << "\n=== Estado final ===\n";
    gestor.imprimirEstados();

    return abortado_por_sigint ? 130 : 0;
}