#include "gestor_dag.hpp"
#include <queue>
#include <iostream>

bool GestorDag::inicializar(const Grafo& g) {
    restantes_.clear();
    estados_.clear();

    for (const auto& [id, act] : g.actividades) {
        restantes_[id] = static_cast<int>(act.deps.size());
        estados_[id] = Estado::PENDIENTE;
    }

    std::unordered_map<std::string, int> grados = restantes_;
    std::queue<std::string> cola;
    for (const auto& [id, grado] : grados) {
        if (grado == 0) cola.push(id);
    }

    int procesados = 0;
    while (!cola.empty()) {
        std::string actual = cola.front();
        cola.pop();
        procesados++;

        auto it = g.dependencias.find(actual);
        if (it == g.dependencias.end()) continue;
        for (const auto& hijo : it->second) {
            grados[hijo]--;
            if (grados[hijo] == 0) cola.push(hijo);
        }
    }

    if (procesados != static_cast<int>(g.actividades.size())) {
        std::cerr << "Error: el plan contiene un ciclo de dependencias.\n";
        return false;
    }

    for (auto& [id, r] : restantes_) {
        if (r == 0) estados_[id] = Estado::LISTA;
    }

    return true;
}

std::vector<std::string> GestorDag::nodosListos() const {
    std::vector<std::string> resultado;
    for (const auto& [id, estado] : estados_) {
        if (estado == Estado::LISTA) resultado.push_back(id);
    }
    return resultado;
}

Estado GestorDag::estadoDe(const std::string& id) const {
    auto it = estados_.find(id);
    if (it == estados_.end()) return Estado::PENDIENTE;
    return it->second;
}

void GestorDag::marcarCorriendo(const std::string& id) {
    estados_[id] = Estado::CORRIENDO;
}

std::vector<std::string> GestorDag::marcarTerminada(const Grafo& g,
                                                      const std::string& id,
                                                      bool exito) {
    std::vector<std::string> nuevosListos;

    if (!exito) {
        estados_[id] = Estado::FALLIDA;
        abortarDescendencia(g, id);
        return nuevosListos;
    }

    estados_[id] = Estado::HECHA;

    auto it = g.dependencias.find(id);
    if (it == g.dependencias.end()) return nuevosListos;

    for (const auto& hijoId : it->second) {
        if (estados_[hijoId] == Estado::ABORTADA) continue;
        restantes_[hijoId]--;
        if (restantes_[hijoId] == 0) {
            estados_[hijoId] = Estado::LISTA;
            nuevosListos.push_back(hijoId);
        }
    }

    return nuevosListos;
}

void GestorDag::abortarDescendencia(const Grafo& g, const std::string& id) {
    auto it = g.dependencias.find(id);
    if (it == g.dependencias.end()) return;

    for (const auto& hijoId : it->second) {
        if (estados_[hijoId] == Estado::ABORTADA || estados_[hijoId] == Estado::HECHA) {
            continue;
        }
        estados_[hijoId] = Estado::ABORTADA;
        abortarDescendencia(g, hijoId);
    }
}

bool GestorDag::todasTerminadas() const {
    for (const auto& [id, estado] : estados_) {
        if (estado == Estado::PENDIENTE || estado == Estado::LISTA ||
            estado == Estado::CORRIENDO) {
            return false;
        }
    }
    return true;
}

void GestorDag::imprimirEstados() const {
    auto nombre = [](Estado e) {
        switch (e) {
            case Estado::PENDIENTE: return "PENDIENTE";
            case Estado::LISTA:     return "LISTA";
            case Estado::CORRIENDO: return "CORRIENDO";
            case Estado::HECHA:     return "HECHA";
            case Estado::FALLIDA:   return "FALLIDA";
            case Estado::ABORTADA:  return "ABORTADA";
        }
        return "?";
    };
    for (const auto& [id, estado] : estados_) {
        std::cout << "  " << id << " -> " << nombre(estado) << "\n";
    }
}

