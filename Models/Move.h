#pragma once
#include <stdlib.h>

// Тип для хранения координат  (целочисленный тип размером 8 бит)
typedef int8_t POS_T;

// Структура для представления хода
struct move_pos
{
    POS_T x, y;             // Исходные координаты (начало хода)
    POS_T x2, y2;           // Целевые координаты (конец хода)
    POS_T xb = -1, yb = -1; // Координаты фигуры, которую били (если совершался удар)

    // Конструктор простого хода без удара
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2)
    {}
    
    // Конструктор полного хода с возможностью удара
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {}

    // Оператор сравнения на равенство
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Оператор проверки неравенства (через отрицание равенства)
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};