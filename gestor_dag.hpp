#ifndef GESTOR_DAG_HPP
#define GESTOR_DAG_HPP

#include "parser.hpp"
#include <unordered_map>
#include <vector>
#include <string>

enum class Estado {
    PENDIENTE,   // esperando que terminen sus dependencias
    LISTA,       // ya puede correr, esperando cupo (K)
    CORRIENDO,   // tiene un proceso hijo activo
    HECHA,       // terminó OK
    FALLIDA,     // terminó con error
    ABORTADA     // no se ejecutó porque una dependencia falló
};

class GestorDag {
public:
    bool inicializar(const Grafo& g);
    std::vector<std::string> nodosListos() const;
    Estado estadoDe(const std::string& id) const;
    void marcarCorriendo(const std::string& id);

    std::vector<std::string> marcarTerminada(const Grafo& g,
                                              const std::string& id,
                                              bool exito);

    bool todasTerminadas() const;
    void imprimirEstados() const;

private:
    std::unordered_map<std::string, int> restantes_;
    std::unordered_map<std::string, Estado> estados_;

    void abortarDescendencia(const Grafo& g, const std::string& id);
};

#endif // GESTOR_DAG_HPP
