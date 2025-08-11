#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__ // Специфичные  включения библиотек для платформы Apple
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else // Универсальные библиотеки для Windows и Linux
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default; // Пустой конструктор по умолчанию
    Board(const unsigned int W, const unsigned int H) : W(W), H(H) {} // Конструктор с параметрами ширины и высоты окна

    // Метод рисования начальной доски
    int start_draw()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0) // Инициализация SDL
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }
        if (W == 0 || H == 0) // Если размеры окна не указаны, берем разрешение рабочего стола
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }
            W = min(dm.w, dm.h); // Берем минимальное разрешение экрана
            W -= W / 15; // Немного уменьшаем размер окна
            H = W; // Сохраняем соотношение сторон
        }
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE); // Создаем окно
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); // Создаем рендерер
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }
        // Загружаем текстуры досок и фигур
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }
        SDL_GetRendererOutputSize(ren, &W, &H); // Получаем фактические размеры окна
        make_start_mtx(); // Формируем начальную матрицу расположения фигур
        rerender(); // Рисуем первоначальную картину на экране
        return 0;
    }

    // Метод для перерисовки доски после сброса игры
    void redraw()
    {
        game_results = -1; // Сбрасываем результат игры
        history_mtx.clear(); // Очищаем историю ходов
        history_beat_series.clear(); // Очищаем историю сериальных ударов
        make_start_mtx(); // Восстанавливаем начальную позицию
        clear_active(); // Сбрасываем выделенные клетки
        clear_highlight(); // Сбрасываем подсветку клеток
    }

    // Метод для перемещения фигуры на новую позицию
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1) // Если был произведен удар
            mtx[turn.xb][turn.yb] = 0; // Удаляем фигуру, которая была сбита
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series); // Выполняем обычное перемещение
    }

    // Основной метод перемещения фигуры
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2]) // Если конечная позиция занята
            throw runtime_error("final position is not empty, can't move");
        if (!mtx[i][j]) // Если начальная позиция свободна
            throw runtime_error("begin position is empty, can't move");
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7)) // Если фигура дошла до крайней линии
            mtx[i][j] += 2; // Преобразуем фигуру в даму
        mtx[i2][j2] = mtx[i][j]; // Перемещаем фигуру на новую позицию
        drop_piece(i, j); // Убираем фигуру с старой позиции
        add_history(beat_series); // Добавляем ход в историю
    }

    // Метод удаления фигуры с указанной позиции
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0; // Обнуляется позиция
        rerender(); // Перерисовывается доска
    }

    // Метод превращения фигуры в даму
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2) // Если позиция пустая или уже дамка
            throw runtime_error("can't turn into queen in this position");
        mtx[i][j] += 2; // Повышаем ранг фигуры до дамы
        rerender(); // Перерисовываем доску
    }

    // Метод для получения текущей матрицы доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Метод выделения клеток (например, для подсветки возможных ходов)
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1; // Метим клетки как выделенные
        }
        rerender(); // Перерисовываем доску
    }

    // Метод очистки выделения клеток
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0); // Все клетки становятся невыделенными
        }
        rerender(); // Перерисовываем доску
    }

    // Метод установки активного состояния клетки
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender(); // Перерисовываем доску
    }

    // Метод сброса активного состояния клетки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender(); // Перерисовываем доску
    }

    // Метод проверки, выделена ли данная клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Метод отката последних ходов
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin())); // Берем последнюю серию ударов
        while (beat_series-- && history_mtx.size() > 1) // Пока есть ходы для отката
        {
            history_mtx.pop_back(); // Удаляем последний ход из истории
            history_beat_series.pop_back(); // Удаляем соответствующую серию ударов
        }
        mtx = *(history_mtx.rbegin()); // Восстанавливаем предыдущее состояние доски
        clear_highlight(); // Сбрасываем подсветку
        clear_active(); // Сбрасываем активное состояние
    }

    // Метод вывода финального результата игры
    void show_final(const int res)
    {
        game_results = res; // Записываем результат игры
        rerender(); // Перерисовываем доску
    }

    // Метод для изменения размеров окна
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H); // Получаем новые размеры окна
        rerender(); // Перерисовываем доску
    }

    // Метод завершения работы и освобождения ресурсов
    void quit()
    {
        SDL_DestroyTexture(board); // Освобождаем ресурсы текстур
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren); // Освобождаем рендерер
        SDL_DestroyWindow(win); // Освобождаем окно
        SDL_Quit(); // Завершаем работу SDL
    }

    // Деструктор для автоматического вызова метода quit()
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Метод добавления текущего состояния доски в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx); // Добавляем копию текущей матрицы
        history_beat_series.push_back(beat_series); // Добавляем количество серий ударов
    }

    // Метод формирования начальной матрицы расположения фигур
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0; // Инициализируем всю доску нулями
                if (i < 3 && (i + j) % 2 == 1) // Располагаем черные фигуры
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1) // Располагаем белые фигуры
                    mtx[i][j] = 1;
            }
        }
        add_history(); // Добавляем начальное состояние в историю
    }

    // Метод перерисовки всей сцены
    void rerender()
    {
        // Очищаем сцену
        SDL_RenderClear(ren);
        // Рисуем фон доски
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Рисуем фигуры
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j]) // Пропускаем пустые клетки
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120; // Высчитываем координаты фигуры
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 }; // Прямоугольник фигуры
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1) // Белая фигура
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2) // Черная фигура
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3) // Белая дама
                    piece_texture = w_queen;
                else // Черная дама
                    piece_texture = b_queen;
                SDL_RenderCopy(ren, piece_texture, NULL, &rect); // Рисуем фигуру
            }
        }

        // Рисуем подсветку клеток
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0); // Зеленый цвет подсветки
        const double scale = 2.5; // Масштабирование рендера
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j]) // Пропускаем невыделенные клетки
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell); // Рисуем прямоугольники подсветки
            }
        }

        // Рисуем активную клетку красным цветом
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1); // Возвращаем масштаб рендера к 1

        // Рисуем стрелки для возврата назад и перезапуска игры
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Рисуем финальную картинку победы или поражения
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;
            else if (game_results == 2)
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // Обновляем экран
        SDL_RenderPresent(ren);
        // Задержка и опрос событий (специально для Mac OS)
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Метод для печати исключений в лог-файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0; // Ширина окна
    int H = 0; // Высота окна
    // История матриц досок
    vector<vector<vector<POS_T>>> history_mtx;

private:
    SDL_Window *win = nullptr; // Указатель на окно
    SDL_Renderer *ren = nullptr; // Рендерер
    // Текстуры изображений
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;
    // Пути к изображениям
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png"; // Фоновая текстура доски
    const string piece_white_path = textures_path + "piece_white.png"; // Белый солдат
    const string piece_black_path = textures_path + "piece_black.png"; // Черный солдат
    const string queen_white_path = textures_path + "queen_white.png"; // Белая дама
    const string queen_black_path = textures_path + "queen_black.png"; // Черная дама
    const string back_path = textures_path + "arrow_left.png"; // Стрелка назад
    const string replay_path = textures_path + "refresh.png"; // Кнопка перезапуска
    const string draw_path = textures_path + "draw.png"; // Картинка ничьей
    const string white_path = textures_path + "winner_white.png"; // Картинка выигрыша белых
    const string black_path = textures_path + "winner_black.png"; // Картинка выигрыша черных
    // Матрица состояния игры
    vector<vector<POS_T>> mtx(8, vector<POS_T>(8));
    // История серий ударов
    vector<int> history_beat_series;
    // Выделенные клетки
    vector<vector<bool>> is_highlighted_(8, vector<bool>(8, false));
    // Активная клетка
    POS_T active_x = -1, active_y = -1;
    // Финал игры
    int game_results = -1;
};