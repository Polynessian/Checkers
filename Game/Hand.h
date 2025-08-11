#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для работы с действиями рук пользователей (обработка событий мышью и клавишей)
class Hand
{
public:
    // Конструктор принимает ссылку на игровую доску
    Hand(Board *board) : board(board)
    {}

    // Метод получает событие нажатия клавиши мыши и возвращает соответствующий отклик
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent; // Экземпляр события SDL
        Response resp = Response::OK; // Изначально считаем ответ успешным
        int x = -1, y = -1; // Хранятся координаты мыши
        int xc = -1, yc = -1; // Внутренние координаты клетки на доске

        // Цикл опроса событий оконной системы
        while (true)
        {
            if (SDL_PollEvent(&windowEvent)) // Если произошло какое-нибудь событие
            {
                switch (windowEvent.type) // Обрабатываем разные типы событий
                {
                case SDL_QUIT: // Нажата кнопка закрытия окна
                    resp = Response::QUIT; // Выставляем команду выхода
                    break;

                case SDL_MOUSEBUTTONDOWN: // Нажата кнопка мыши
                    x = windowEvent.motion.x; // Забираем координату X
                    y = windowEvent.motion.y; // Забираем координату Y
                    xc = int(y / (board->H / 10) - 1); // Переводим пиксельные координаты в индексы доски
                    yc = int(x / (board->W / 10) - 1);

                    // Проверяем, какая область была нажата
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK; // Команда отступления (вернуться назад)
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY; // Команда перезапуска игры
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL; // Пользователь указал клетку на доске
                    }
                    else
                    {
                        xc = -1; // Инвалидные координаты
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT: // Изменился размер окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // Пересчитываем размеры окна
                        break;
                    }
                }
                if (resp != Response::OK) // Если возникла любая реакция кроме OK, прерываем цикл
                    break;
            }
        }
        return {resp, xc, yc}; // Возвращаем отклик и координаты клетки
    }

    // Метод ожидает пользовательского ввода и интерпретирует его
    Response wait() const
    {
        SDL_Event windowEvent; // Объект события SDL
        Response resp = Response::OK; // Первоначально устанавливаем успех

        // Цикл опроса событий оконной системы
        while (true)
        {
            if (SDL_PollEvent(&windowEvent)) // Произошло событие
            {
                switch (windowEvent.type) // Обрабатываем разные типы событий
                {
                case SDL_QUIT: // Нажата кнопка закрытия окна
                    resp = Response::QUIT; // Сообщаем о завершении игры
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: // Изменилась ширина окна
                    board->reset_window_size(); // Нужно обновить размеры окна
                    break;

                case SDL_MOUSEBUTTONDOWN: // Нажата кнопка мыши
                    int x = windowEvent.motion.x; // Получаем координаты клика
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1); // Приводим к индексам доски
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY; // Переключение на перезапуск игры
                    break;
                }
                if (resp != Response::OK) // Если произошел какой-то иной отклик, выходим из цикла
                    break;
            }
        }
        return resp; // Возвращаем полученный отклик
    }

private:
    Board *board; // Указатель на игровую доску
};