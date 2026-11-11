#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspsysmem.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "intraFont.h"
#include "oslib.h"

PSP_MODULE_INFO("NexusOS", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define SCR_W 480
#define SCR_H 272
#define MAX_GAMES 100

// Состояния системы
#define STATE_MENU 0
#define STATE_GAMES 1
int current_state = STATE_MENU;

static intraFont *font = NULL;
static OSL_IMAGE *bg_card = NULL; 

// Цвета (Космос)
#define C_BG        0xFF0B0F19 
#define C_TOP_BAR   0xFF131A26 
#define C_TEXT_MAIN 0xFFFFFFFF
#define C_TEXT_DIM  0xFF8B9BB4
#define C_CARD_PURP 0xFF9C27B0 
#define C_CARD_BLUE 0xFF2196F3 
#define C_CARD_RED  0xFFE53935 
#define C_CARD_GRAY 0xFF1E293B 
#define C_HIGHLIGHT 0xFF00E5FF // Неоновый голубой

typedef struct {
    float x, y, w, h;
    unsigned int color;
    char title[32];
    char sub[32];
    int action; 
} Card;

// Меню
Card main_cards[4] = {
    {20, 50, 210, 90, C_CARD_BLUE, "Игры", "Запуск ISO/CSO", 1},
    {240, 50, 220, 90, C_CARD_PURP, "Настройки", "Система", 0},
    {20, 150, 210, 80, C_CARD_GRAY, "Инфо", "Мониторинг", 0},
    {240, 150, 220, 80, C_CARD_RED, "Выход", "В XMB (Меню)", 3}
};

// Сканер игр
char game_list[MAX_GAMES][128];
int game_count = 0;
int selected_game = 0;
int game_scroll = 0;

int target_card = 0;
float cursor_x = 20.0f, cursor_y = 50.0f;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Сканирование папки ISO
void scan_games() {
    game_count = 0;
    SceUID d = sceIoDopen("ms0:/ISO");
    if (d < 0) return;
    SceIoDirent ent;
    while (sceIoDread(d, &ent) > 0 && game_count < MAX_GAMES) {
        if (ent.d_stat.st_mode & FIO_S_IFDIR) continue;
        int len = strlen(ent.d_name);
        if (len > 4) {
            char *ext = &ent.d_name[len-4];
            // Проверка расширений (ISO, CSO)
            if (strcasecmp(ext, ".iso") == 0 || strcasecmp(ext, ".cso") == 0) {
                snprintf(game_list[game_count], 128, "%s", ent.d_name);
                game_count++;
            }
        }
    }
    sceIoDclose(d);
}

// Запуск игры через ARK-5 Inferno
void launch_game() {
    if (game_count == 0) return;
    char path[256];
    snprintf(path, sizeof(path), "ms0:/ISO/%s", game_list[selected_game]);
    
    // Вызов перехватывается ARK-5, игра запускается без распаковки
    sceKernelDelayThread(200000); 
    sceKernelLoadExec(path, NULL);
}

void draw_icon(int type, float x, float y) {
    // Векторные иконки
    if (type == 1) { // Игры
        oslDrawRect(x, y, x+20, y+20, C_TEXT_MAIN);
        oslDrawFillRect(x+4, y+4, x+16, y+16, C_TEXT_MAIN);
    } else if (type == 3) { // Выход
        oslDrawLine(x, y, x+20, y+20, C_TEXT_MAIN);
        oslDrawLine(x+20, y, x, y+20, C_TEXT_MAIN);
    } else if (type == 0) { // Настройки/Инфо
        oslDrawFillRect(x+8, y, x+12, y+20, C_TEXT_MAIN);
        oslDrawFillRect(x, y+8, x+20, y+12, C_TEXT_MAIN);
    }
}

void draw_card(Card *c) {
    if (bg_card) oslDrawImageColor(c->x, c->y, c->color, bg_card);
    else oslDrawFillRect(c->x, c->y, c->x + c->w, c->y + c->h, c->color);
    
    draw_icon(c->action, c->x + 15, c->y + 15);
    
    intraFontSetStyle(font, 0.9f, C_TEXT_MAIN, 0, INTRAFONT_ALIGN_LEFT, 0);
    intraFontPrint(font, c->x + 45, c->y + 15, c->title);
    intraFontSetStyle(font, 0.6f, 0xCCFFFFFF, 0, INTRAFONT_ALIGN_LEFT, 0);
    intraFontPrint(font, c->x + 45, c->y + 45, c->sub);
}

void render_menu() {
    for (int i = 0; i < 4; i++) draw_card(&main_cards[i]);
    
    // Плавная анимация курсора
    cursor_x = lerp(cursor_x, main_cards[target_card].x, 0.3f);
    cursor_y = lerp(cursor_y, main_cards[target_card].y, 0.3f);
    
    // Неоновая рамка
    oslDrawRect(cursor_x, cursor_y, cursor_x + main_cards[target_card].w, cursor_y + main_cards[target_card].h, C_HIGHLIGHT);
    oslDrawRect(cursor_x-2, cursor_y-2, cursor_x + main_cards[target_card].w+2, cursor_y + main_cards[target_card].h+2, C_HIGHLIGHT);
    
    intraFontSetStyle(font, 0.6f, C_TEXT_DIM, 0, INTRAFONT_ALIGN_CENTER, 0);
    intraFontPrint(font, SCR_W/2, SCR_H - 15, "X - Выбрать | O - Выход в XMB | START - Разгон 333МГц");
}

void render_games() {
    intraFontSetStyle(font, 0.8f, C_TEXT_MAIN, 0, INTRAFONT_ALIGN_LEFT, 0);
    intraFontPrint(font, 20, 50, "Игры на карте памяти:");
    
    if (game_count == 0) {
        intraFontSetStyle(font, 0.7f, C_CARD_RED, 0, INTRAFONT_ALIGN_LEFT, 0);
        intraFontPrint(font, 20, 90, "Папка ms0:/ISO пуста или не найдена...");
    } else {
        for (int i = 0; i < 5; i++) {
            int idx = game_scroll + i;
            if (idx >= game_count) break;
            int y = 80 + (i * 30);
            
            if (idx == selected_game) {
                oslDrawFillRect(10, y-2, SCR_W-10, y+22, C_CARD_GRAY);
                oslDrawFillRect(10, y-2, 15, y+22, C_HIGHLIGHT);
            }
            intraFontSetStyle(font, 0.7f, (idx == selected_game) ? C_TEXT_MAIN : C_TEXT_DIM, 0, INTRAFONT_ALIGN_LEFT, 0);
            intraFontPrint(font, 25, y, game_list[idx]);
        }
    }
    
    intraFontSetStyle(font, 0.6f, C_TEXT_DIM, 0, INTRAFONT_ALIGN_CENTER, 0);
    intraFontPrint(font, SCR_W/2, SCR_H - 15, "X - Запустить | O - Назад");
}

void render() {
    oslStartDrawing();
    oslClearScreen(C_BG);

    // Верхняя шторка (Живой мониторинг)
    oslDrawFillRect(0, 0, SCR_W, 36, C_TOP_BAR);
    ScePspDateTime t; sceRtcGetCurrentClockLocalTime(&t);
    char time_str[64], bat_str[64], mem_str[64], cpu_str[64];
    
    sprintf(time_str, "%02d:%02d", t.hour, t.minute);
    sprintf(bat_str, "%d%%", scePowerGetBatteryLifePercent());
    
    // Свободная оперативка
    int free_mem = sceKernelTotalFreeMemSize() / 1024; 
    sprintf(mem_str, "RAM: %d KB", free_mem);
    
    // Текущая частота процессора
    int cpu;
    scePowerGetCpuClockFrequencyInt(&cpu);
    sprintf(cpu_str, "CPU: %d MHz", cpu);
    
    intraFontSetStyle(font, 0.6f, C_TEXT_DIM, 0, INTRAFONT_ALIGN_LEFT, 0);
    intraFontPrint(font, 15, 12, time_str);
    intraFontPrint(font, 70, 12, bat_str);
    intraFontPrint(font, 150, 12, cpu_str);
    intraFontPrint(font, 300, 12, mem_str);

    // Отрисовка контента
    if (current_state == STATE_MENU) render_menu();
    else if (current_state == STATE_GAMES) render_games();

    oslEndDrawing();
    oslSyncFrame();
}

int handle_input() {
    SceCtrlData pad;
    static unsigned int old = 0;
    sceCtrlReadBufferPositive(&pad, 1);
    unsigned int pressed = pad.Buttons & ~old;
    old = pad.Buttons;

    if (current_state == STATE_MENU) {
        // Навигация по сетке 2х2
        if (pressed & PSP_CTRL_UP) target_card = (target_card >= 2) ? target_card - 2 : target_card;
        if (pressed & PSP_CTRL_DOWN) target_card = (target_card < 2) ? target_card + 2 : target_card;
        if (pressed & PSP_CTRL_LEFT) target_card = (target_card % 2 == 1) ? target_card - 1 : target_card;
        if (pressed & PSP_CTRL_RIGHT) target_card = (target_card % 2 == 0) ? target_card + 1 : target_card;

        if (pressed & PSP_CTRL_CROSS) {
            if (main_cards[target_card].action == 1) {
                scan_games();
                current_state = STATE_GAMES;
            } else if (main_cards[target_card].action == 3) {
                sceKernelExitGame(); // Выход в стандартное меню
            }
        }
    } else if (current_state == STATE_GAMES) {
        if (pressed & PSP_CTRL_UP && selected_game > 0) selected_game--;
        if (pressed & PSP_CTRL_DOWN && selected_game < game_count - 1) selected_game++;
        
        // Скроллинг списка
        if (selected_game < game_scroll) game_scroll = selected_game;
        if (selected_game >= game_scroll + 5) game_scroll = selected_game - 4;

        if (pressed & PSP_CTRL_CROSS) launch_game();
        if (pressed & PSP_CTRL_CIRCLE) current_state = STATE_MENU;
    }

    // Глобальные хоткеи
    if (pressed & PSP_CTRL_CIRCLE && current_state == STATE_MENU) return -1;
    if (pressed & PSP_CTRL_START) scePowerSetClockFrequency(333, 333, 166); // Хоткей разгона
    if (pressed & PSP_CTRL_SELECT) scePowerSetClockFrequency(222, 222, 111); // Сброс частоты

    return 0;
}

int main() {
    // Экономия батареи в меню
    scePowerSetClockFrequency(222, 222, 111); 
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    oslInit(0);
    oslInitGfx(OSL_PF_8888, 1);
    
    intraFontInit();
    font = intraFontLoad("flash0:/font/ltn0.pgf", INTRAFONT_CACHE_ASCII);
    intraFontSetEncoding(font, INTRAFONT_STRING_UTF8);

    // Ищем текстуру скруглений (опционально)
    bg_card = oslLoadImageFilePNG("card_bg.png", OSL_IN_RAM, OSL_PF_8888);

    int running = 1;
    while (running) {
        render();
        if (handle_input() < 0) running = 0; 
        sceKernelDelayThread(1000); 
    }

    if (bg_card) oslDeleteImage(bg_card);
    intraFontUnload(font);
    intraFontShutdown();
    oslEndGfx();
    oslQuit();
    sceKernelExitGame();
    return 0;
}