#pragma once
#include <chrono>
#include <thread>
#include <vector>
#include <tuple>
#include <utility>
#include <iostream>
#include <fstream>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")),
             hand(&board), logic(&board, &config)
    {
        // Создание и очистка журнала ("log.txt")
        std::ofstream fout(project_path + "log.txt", std::ios_base::trunc); // Открытие файла log.txt и очищение его содержимого
        fout.close();
    }

    // Основная функция запуска игры
    int play()
    {
        auto start = std::chrono::steady_clock::now(); // Измерение времени старта игры

        // Если включён режим повторения (replay), перезагружаем состояние игры
        if (is_replay)
        {
            logic = Logic(&board, &config);           // Создаём новый объект логика
            config.reload();                          // Загружаем новые настройки
            board.redraw();                           // Перерисовка доски
        }
        else
        {
            board.start_draw();                       // Начальная прорисовка доски
        }
        is_replay = false;                            // Выключение режима повторения

        int turn_num = -1;                            // Номер текущего хода (-1 для первого хода)
        bool is_quit = false;                         // Признак выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // Максимальная длина игры

        // Главный игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;                          // Обнуление серии удачных ударов
            logic.find_turns(turn_num % 2);           // Поиск всех доступных ходов для текущего игрока

            // Проверка наличия ходов
            if (logic.turns.empty())                  // Если ходов больше нет, прекращаем игру
                break;

            // Установка глубины поиска AI исходя из цвета игрока
            logic.Max_depth = config("Bot", std::string((turn_num % 2) ? "Black" : "White") + std::string("BotLevel"));

            // Выбор, кто ходит: человек или бот
            if (!config("Bot", std::string("Is") + std::string((turn_num % 2) ? "Black" : "White") + std::string("Bot"))) // Человеческий ход?
            {
                auto resp = player_turn(turn_num % 2); // Запрашиваем ход игрока

                // Анализ реакций игрока
                if (resp == Response::QUIT)           // Игрок вышел из игры
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)     // Игрок хочет сыграть заново
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)       // Игрок вернул ход обратно
                {
                    // Проверьте условие отмены хода и выполняйте откат
                    if (config("Bot", std::string("Is") + std::string((1 - turn_num % 2) ? "Black" : "White") + std::string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();              // Отмена предыдущего хода
                        --turn_num;                    // Уменьшаем счётчик ходов
                    }
                    if (!beat_series)                  // Если нет серийных ударов, уменьшаем ход дважды
                        --turn_num;

                    board.rollback();                  // Вторая отмена хода
                    --turn_num;
                    beat_series = 0;                  // Сброс серии ударов
                }
            }
            else                                      // Ход компьютера
                bot_turn(turn_num % 2);               // Передача хода боту
        }

        // Фиксация времени окончания игры
        auto end = std::chrono::steady_clock::now();
        std::ofstream fout(project_path + "log.txt", std::ios_base::app); // Сохраняем время игры в журнале
        fout << "Game time: " << static_cast<int>(std::chrono::duration<double, std::milli>(end - start).count()) << " ms\n";
        fout.close();

        // Логи финала игры
        if (is_replay)                                // Повтор игры
            return play();
        if (is_quit)                                  // Выход из игры
            return 0;
        int result = 2;                               // По умолчанию ничья
        if (turn_num == Max_turns)                    // Игра закончилась естественным путем (до предела ходов)
        {
            result = 0;                               // Ничья
        }
        else if (turn_num % 2)                        // Побеждает противоположный игрок
        {
            result = 1;                               // Чёрные победили
        }
        board.show_final(result);                     // Показ результатов игры
        auto response = hand.wait();                  // Ждать реакцию игрока после показа экрана победителя
        if (response == Response::REPLAY)             // Игрок решает продолжить
        {
            is_replay = true;
            return play();                            // Новая партия
        }
        return result;                                // Итоговый результат игры
    }

private:
    // Ход игрока
    Response player_turn(const bool color)
    {
        // Получаем список доступных ходов для текущего игрока
        std::vector<std::pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);       // Доступные клетки
        }
        board.highlight_cells(cells);                 // Подсветка возможных ходов на доске

        POS_T x = -1, y = -1;                         // Текущие координаты выбранной клетки
        move_pos pos{-1, -1, -1, -1};                // Текущий выбранный ход

        // Попытка выбрать первую клетку
        while (true)
        {
            auto resp = hand.get_cell();              // Просим руку выбрать клетку
            if (std::get<0>(resp) != Response::CELL) // Ответ отличается от выбора клетки
                return std::get<0>(resp);             // Игрок выбрал действие (QUIT, REPLAY и др.)

            std::pair<POS_T, POS_T> cell{std::get<1>(resp), std::get<2>(resp)}; // Получены координаты ячейки

            bool is_correct = false;                  // Был ли сделан правильный выбор?
            for (auto turn : logic.turns)
            {
                if ((turn.x == cell.first && turn.y == cell.second)) // Проверка начальной точки хода
                {
                    is_correct = true;                // Правильная позиция выбрана
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second}) // Полностью подходящий ход
                {
                    pos = turn;                       // Запоминаем ход
                    break;
                }
            }
            if (pos.x != -1)                          // Нашли подходящий ход
                break;
            if (!is_correct)                          // Неправильный выбор клетки
            {
                if (x != -1)                          // Есть предыдущая активная ячейка
                {
                    board.clear_active();             // Снимаем выделение активной ячейки
                    board.clear_highlight();          // Снимаем подсветку ходов
                    board.highlight_cells(cells);     // Освежаем подсвечивание доступных ходов
                }
                x = -1;                               // Сброс предыдущих координат
                y = -1;
                continue;
            }
            x = cell.first;                           // Фиксируем выбранную позицию
            y = cell.second;
            board.clear_highlight();                  // Удаляем подсветку остальных ходов
            board.set_active(x, y);                   // Активируем выбранную клетку
            std::vector<std::pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)       // Подсветка доступных направлений движения
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);            // Подсветка конечных пунктов выбранного направления
        }

        // Сделали первый ход
        board.clear_highlight();                      // Снимем подсветку
        board.clear_active();                         // Снимем активацию
        board.move_piece(pos, pos.xb != -1);          // Перемещаем фигуру согласно выбранному ходу

        // Продолжаем серией ударов, если возможно
        if (pos.xb == -1)                             // Обычный ход без захвата фигуры
            return Response::OK;

        beat_series = 1;                              // Включаем захватную серию
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);         // Найти следующие возможные удары

            // Если следующий удар невозможен, останавливаемся
            if (!logic.have_beats)
                break;

            std::vector<std::pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2); // Новые потенциальные ходы
            }
            board.highlight_cells(cells);             // Подсветка новых доступных ходов
            board.set_active(pos.x2, pos.y2);         // Активация последней занятой клетки

            // Получаем следующую клетку для хода
            while (true)
            {
                auto resp = hand.get_cell();          // Запрашиваем ввод следующей клетки
                if (std::get<0>(resp) != Response::CELL)
                    return std::get<0>(resp);         // Действие игрока отличалось от выбора клетки

                std::pair<POS_T, POS_T> cell{std::get<1>(resp), std::get<2>(resp)}; // Получаем координаты выбранной клетки

                bool is_correct = false;              // Является ли ход правильным?
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;            // Валидный ход найден
                        pos = turn;                  // Запоминаем новый ход
                        break;
                    }
                }
                if (!is_correct)                      // Если неверный ход, продолжаем ждать правильного ввода
                    continue;

                board.clear_highlight();              // Снимаем подсветку
                board.clear_active();                 // Снимаем активность предыдущей клетки
                beat_series += 1;                     // Увеличение серии ударов
                board.move_piece(pos, beat_series);   // Совершаем очередной удар
                break;
            }
        }
        return Response::OK;                          // Всё прошло успешно
    }

    // Ход компьютера
    void bot_turn(const bool color)
    {
        auto start = std::chrono::steady_clock::now(); // Время начала хода

        auto delay_ms = config("Bot", "BotDelayMS");  // Получаем установленную задержку для хода бота
        std::thread th(std::SDL_Delay, delay_ms);     // Поток ожидания (задержка для визуального эффекта)
        auto best_turns = logic.find_best_turns(color);// Нахождение лучших ходов для бота
        th.join();                                    // Синхронизация потока

        bool is_first = true;                         // Первый ход в серии
        for (auto turn : best_turns)
        {
            if (!is_first)                            // Пауза между последующими ходами в серии
            {
                std::SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);           // Следим за серией ударов
            board.move_piece(turn, beat_series);      // Осуществление хода
        }

        auto end = std::chrono::steady_clock::now();  // Время окончания хода
        std::ofstream fout(project_path + "log.txt", std::ios_base::app); // Добавляем время хода в журнал
        fout << "Bot turn time: " << static_cast<int>(std::chrono::duration<double, std::milli>(end - start).count()) << " ms\n";
        fout.close();
    }

private:
    Config config;                                   // Объект конфигурации
    Board board;                                     // Объект игровой доски
    Hand hand;                                       // Объект управления игроками
    Logic logic;                                     // Объект логики игры
    int beat_series;                                 // Количество подряд идущих удачных ударов
    bool is_replay = false;                          // Флаг режима повторения игры
};