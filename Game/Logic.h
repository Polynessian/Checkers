#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9; // Константа бесконечности для оценочной функции

class Logic
{
public:
    // Конструктор класса Logic
    //
    // Параметры:
    // - board: ссылка на игровую доску
    // - config: ссылка на объект конфигурации
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        // Инициализация генератора случайных чисел
        // Если NoRandom выключено, используем текущее время как seed
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        // Инициализация способа расчета очков и опции оптимизации
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    

private:
    // Применяет ход на виртуальной матрице доски
    //
    // Параметры:
    // - mtx: текущая матрица доски
    // - turn: ход для выполнения
    //
    // Возвращаемое значение:
    // новая матрица доски после выполнения хода
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1) // Если есть удар, удаляем захваченный элемент
            mtx[turn.xb][turn.yb] = 0;
        // Преобразование пешки в дамку при достижении края поля
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) ||
            (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещение фигуры на новое место
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Расчёт текущей оценки позиции
    //
    // Параметры:
    // - mtx: текущая матрица доски
    // - first_bot_color: определяет, чей ход считается первым (цвет бота)
    //
    // Возвращаемое значение:
    // числовая оценка текущей позиции
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        double white_pawns = 0, white_queens = 0, black_pawns = 0, black_queens = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                white_pawns += (mtx[i][j] == 1);      // Белые пешки
                white_queens += (mtx[i][j] == 3);    // Белые дамы
                black_pawns += (mtx[i][j] == 2);      // Черные пешки
                black_queens += (mtx[i][j] == 4);    // Черные дамы
                // Дополнительная стратегия подсчета очков с учётом позиционных факторов
                if (scoring_mode == "NumberAndPotential")
                {
                    white_pawns += 0.05 * (mtx[i][j] == 1) * (7 - i); // Для белых: близость к концу увеличивает ценность
                    black_pawns += 0.05 * (mtx[i][j] == 2) * (i);     // Для черных аналогично
                }
            }
        }
        // Меняем стороны, если текущая сторона — оппонент бота
        if (!first_bot_color)
        {
            std::swap(white_pawns, black_pawns);
            std::swap(white_queens, black_queens);
        }
        // Проверка на проигрыш одной из сторон
        if (white_pawns + white_queens == 0)
            return INF; // Проиграли белые
        if (black_pawns + black_queens == 0)
            return 0;   // Проиграли черные
        // Рассчитываем коэффициент влияния дамок
        int queen_coefficient = 4;
        if (scoring_mode == "NumberAndPotential")
            queen_coefficient = 5;
        // Формула расчёта относительной силы позиций
        return (black_pawns + black_queens * queen_coefficient) /
               (white_pawns + white_queens * queen_coefficient);
    }

   


public:
    // Находит доступные ходы для определенного цвета
    //
    // Параметры:
    // - color: цвет игрока
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board()); // Просто передаем текущую доску
    }

    // Находит доступные ходы из конкретной клетки
    //
    // Параметры:
    // - x, y: координаты клетки
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board()); // Так же передаем текущую доску
    }

private:
    // Основной метод для поиска доступных ходов
    //
    // Параметры:
    // - color: цвет игрока
    // - mtx: текущая  матрица доски
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> result_turns;
        bool found_beats = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx); // Находим возможные ходы из клетки (i,j)
                    if (have_beats && !found_beats)
                    {
                        found_beats = true;
                        result_turns.clear(); // Чистим предыдущие результаты, если нашли атаку
                    }
                    if ((found_beats && have_beats) || !found_beats)
                    {
                        result_turns.insert(result_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = result_turns; // Результаты помещаются в общий массив ходов
        std::shuffle(turns.begin(), turns.end(), rand_eng); // Случайная перестановка ходов
        have_beats = found_beats;
    }

    // Находит возможные ходы из определенной клетки
    //
    // Параметры:
    // - x, y: координаты клетки
    // - mtx: текущая матрица доски
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear(); // Очищаем ранее сохранённые ходы
        have_beats = false; // Пока нет никаких ударов
        POS_T piece_type = mtx[x][y]; // Тип фигуры на текущей клетке

        // Различные случаи проверок хода
        switch (piece_type)
        {
        case 1: // Белая простая фигура
        case 2: // Черная простая фигура
            // Проверка простых ходов (без взятий)
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T middle_x = (x + i) / 2, middle_y = (y + j) / 2;
                    if (mtx[i][j] || !mtx[middle_x][middle_y] || mtx[middle_x][middle_y] % 2 == piece_type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, middle_x, middle_y); // Добавляем ход с ударом
                }
            }
            break;
        default: // Дамка
            // Проверка вертикальных и горизонтальных ходов дамки
            for (POS_T dir_i = -1; dir_i <= 1; dir_i += 2)
            {
                for (POS_T dir_j = -1; dir_j <= 1; dir_j += 2)
                {
                    POS_T last_blocked_x = -1, last_blocked_y = -1;
                    for (POS_T i2 = x + dir_i, j2 = y + dir_j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += dir_i, j2 += dir_j)
                    {
                        if (mtx[i2][j2]) // Если встречена фигура
                        {
                            if (mtx[i2][j2] % 2 == piece_type % 2 || (last_blocked_x != -1 && last_blocked_x != i2))
                            {
                                break; // Невозможен дальнейший ход
                            }
                            last_blocked_x = i2;
                            last_blocked_y = j2;
                        }
                        if (last_blocked_x != -1 && last_blocked_x != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, last_blocked_x, last_blocked_y); // Добавляем удар
                        }
                    }
                }
            }
            break;
        }
        // Если были найдены удары, добавляем обычные ходы
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        // Проверка обычных ходов для простой фигуры или дамки
        switch (piece_type)
        {
        case 1: // Белая простая фигура
        case 2: // Черная простая фигура
            {
                POS_T dx = ((piece_type % 2) ? x - 1 : x + 1); // Направление хода вперед
                for (POS_T dy = y - 1; dy <= y + 1; dy += 2)
                {
                    if (dx < 0 || dx > 7 || dy < 0 || dy > 7 || mtx[dx][dy])
                        continue;
                    turns.emplace_back(x, y, dx, dy); // Добавляем обычный ход
                }
                break;
            }
        default: // Дамка
            for (POS_T di = -1; di <= 1; di += 2)
            {
                for (POS_T dj = -1; dj <= 1; dj += 2)
                {
                    for (POS_T i2 = x + di, j2 = y + dj; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += di, j2 += dj)
                    {
                        if (mtx[i2][j2])
                            break; // Нельзя пройти дальше фигуры
                        turns.emplace_back(x, y, i2, j2); // Добавляем обычный ход
                    }
                }
            }
            break;
        }
    }

public:
    // Массив доступных ходов
    vector<move_pos> turns;
    // Наличие обязательных ударов
    bool have_beats;
    // Максимальная глубина поиска
    int Max_depth;

private:
    // Генератор случайных чисел
    std::default_random_engine rand_eng;
    // Способ подсчета очков
    string scoring_mode;
    // Тип оптимизации (alpha-beta cutoff)
    string optimization;
    // Последовательность состояний
    vector<move_pos> next_move;
    // Следующее лучшее состояние
    vector<int> next_best_state;
    // Указатель на доску
    Board* board;
    // Указатель на конфигурацию
    Config* config;
};


