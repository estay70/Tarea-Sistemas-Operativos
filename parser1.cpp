#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>
#include <functional>
using namespace std;
namespace {


string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}


vector<string> split(const string& s, char delim) {
    vector<string> out;
    stringstream ss(s);
    string token;
    while (getline(ss, token, delim)){ 
        out.push_back(token);
    }
    return out;             
}


int tiempoAleatorio() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int> dist(100, 5000);
    return dist(gen);
}


vector<string> parsearDeps(const string& campo) {
    string limpio = trim(campo);
    if (!limpio.empty() && limpio.front() == '[') limpio.erase(0, 1);
    if (!limpio.empty() && limpio.back() == ']') limpio.pop_back();

    vector<string> deps;
    for (const auto& d : split(limpio, ',')) {
        string dep = trim(d);
        if (!dep.empty()) deps.push_back(dep);
    }
    return deps;
}

} // namespace

Grafo parsearPlan(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        throw runtime_error("No se pudo abrir el archivo: " + path);
    }

    Grafo g;
    string linea;
    int num_linea = 0;

    while (getline(file, linea)) {
        num_linea++;
        string limpia = trim(linea);
        if (limpia.empty() || limpia[0] == '#') continue; // línea vacía o comentario

        auto campos = split(limpia, ':');
        if (campos.size() < 3) {
            throw runtime_error(
                "Línea " + to_string(num_linea) + " mal formada (se esperaban al menos 3 campos): " + linea);
        }

        Actividad act;
        act.id = trim(campos[0]);
        act.nombre = trim(campos[1]);
        string tiempo_str = trim(campos[2]);

        if (act.id.empty() || act.nombre.empty()) {
            throw runtime_error("Línea " + to_string(num_linea) + ": id o nombre vacío: " + linea);
        }

        if (tiempo_str.empty()) {
            act.tiempo_ms = tiempoAleatorio();
        } else {
            try {
                act.tiempo_ms = stoi(tiempo_str);
            } catch (...) {
                throw runtime_error(
                    "Línea " + to_string(num_linea) + ": tiempo_ms inválido: '" + tiempo_str + "'");
            }
            if (act.tiempo_ms < 0) {
                throw runtime_error("Línea " + to_string(num_linea) + ": tiempo_ms negativo");
            }
        }

        
        string campo_deps;
        if (campos.size() >= 4) {
            for (size_t i = 3; i < campos.size(); ++i) {
                if (i > 3) campo_deps += ":";
                campo_deps += campos[i];
            }
        }
        act.deps = parsearDeps(campo_deps);

        if (g.actividades.count(act.id)) {
            throw runtime_error("Línea " + to_string(num_linea) + ": id duplicado '" + act.id + "'");
        }

        g.actividades[act.id] = act;
    }

    
    for (const auto& [id, act] : g.actividades) {
        g.act_restantes[id] = static_cast<int>(act.deps.size());
        for (const auto& dep : act.deps) {
            if (!g.actividades.count(dep)) {
                throw runtime_error(
                    "Actividad '" + id + "' depende de '" + dep + "', que no existe en el plan.");
            }
            g.dependencias[dep].push_back(id);
        }
    }

    return g;
}