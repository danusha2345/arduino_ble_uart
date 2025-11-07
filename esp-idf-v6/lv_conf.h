/**
 * @file lv_conf.h
 * @brief LVGL v9.4.0 Configuration
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_INCLUDE_SIMPLE 1

/* ====================
   Цветовые настройки
   ==================== */
#define LV_COLOR_DEPTH 16

/* ====================
   Память
   ==================== */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64 * 1024U)  // 64KB для буферов LVGL

/* ====================
   Отладка и мониторинг
   ==================== */
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1

/* ====================
   Логирование
   ==================== */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF 1

/* ====================
   Виджеты
   ==================== */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1
#define LV_USE_BAR 1
#define LV_USE_SLIDER 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_ROLLER 0
#define LV_USE_TEXTAREA 0

/* ====================
   Шрифты
   ==================== */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1

#define LV_FONT_MONTSERRAT_20_COMPRESSED 1
#define LV_FONT_DEJAVU_18_PERSIAN_HEBREW 1

/* ====================
   Анимация
   ==================== */
#define LV_USE_ANIMATION 1
#define LV_ANIM_DEFAULT_DURATION 200  // ms

/* ====================
   Темы
   ==================== */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1

/* ====================
   Отрисовка
   ==================== */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
#define LV_DRAW_SW_GRADIENT_CACHE_SIZE 0

/* ====================
   Оптимизация для ESP32
   ==================== */
#define LV_TICK_CUSTOM 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0

/* ====================
   Дисплей
   ==================== */
#define LV_DPI_DEF 130

#endif /* LV_CONF_H */
