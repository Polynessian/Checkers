#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
public:
    /// Конструктор, инициализирует объект класса загрузкой начальных настроек
    Config()
    {
        reload();
    }

    /// Функция reload() обновляет конфигурацию, считывая её из файла settings.json
    ///
    /// Эта функция открывает указанный файл конфигурации и записывает его содержание
    /// в переменную-член 'config'. После изменения настроек в файле эта функция должна
    /// быть вызвана для обновления внутренних  данных.
    void reload()
    {
        std::ifstream fin(project_path + "settings.json"); // Открываем файл настроек
        fin >> config;                                     // Читаем данные в переменную config
        fin.close();                                       // Закрываем файл
    }

    /// Операция обращения к классу Config как к функции с двумя аргументами
    ///
    /// Позволяет обращаться к объекту класса Config как к функции, передавая два аргумента:
    /// setting_dir (каталог параметра) и setting_name (название конкретного параметра),
    /// возвращая соответствующее значение из JSON-конфигурации.
    auto operator()(const std::string& setting_dir, const std::string& setting_name) const
    {
        return config[setting_dir][setting_name]; // Возвращает значение параметра из JSON
    }

private:
    json config; ///< Переменная хранит данные конфигурации в виде JSON
};