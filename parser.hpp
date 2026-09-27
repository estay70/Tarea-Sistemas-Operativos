#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
using namespace std;

struct Actividad{ //Datos de la actividad
    string id;
    string nombre;
    int tiempo_ms;
    vector<string> deps;
};

struct Grafo{ //Datos del grafo
    unordered_map<string, Actividad> actividades;
    unordered_map<string, vector<string>> dependencias;
    unordered_map<std::string, int> act_restantes;

};