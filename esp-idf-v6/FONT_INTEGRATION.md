# Интеграция кастомного шрифта - Инструкция

## После создания файла шрифта

Когда у тебя будет файл `roboto_condensed_20.c`, я автоматически выполню следующие действия:

### 1. Обновлю main/CMakeLists.txt

Добавлю:
```cmake
set(COMPONENT_SRCS
    "main.c"
    "ble_service.c"
    "wifi_service.c"
    "display_manager.c"
    "gps_parser.c"
    "fonts/roboto_condensed_20.c"  # <-- Новый шрифт
)
```

### 2. Добавлю объявление в display_manager.c

```c
// В начале файла, после includes:
LV_FONT_DECLARE(roboto_condensed_20);
```

### 3. Заменю шрифт в create_ui()

```c
// Было:
lv_obj_set_style_text_font(label_line[i], &lv_font_montserrat_16, 0);

// Станет:
lv_obj_set_style_text_font(label_line[i], &roboto_condensed_20, 0);
```

---

## Быстрая команда для копирования файла

Если файл скачан в `~/Downloads/`:

```bash
cp ~/Downloads/roboto_condensed_20.c ~/arduino_ble_uart/esp-idf-v6/main/fonts/
ls -la ~/arduino_ble_uart/esp-idf-v6/main/fonts/
```

Проверь что файл появился, затем дай мне знать!

---

## Размеры памяти (примерно)

**Минимальный набор (ASCII 0x20-0x7E):**
- ~15-20 KB Flash

**С кириллицей (0x20-0x7E, 0x400-0x45F):**
- ~25-30 KB Flash

**4-bit BPP** использует в 2 раза меньше памяти чем 8-bit,
но качество почти такое же.

---

## Если хочешь попробовать разные варианты

Можно создать несколько шрифтов с разными настройками:

1. `roboto_condensed_20.c` - основной (Roboto Condensed)
2. `ubuntu_condensed_20.c` - альтернатива (Ubuntu Condensed)
3. `fira_condensed_20.c` - современный вариант (Fira Sans Condensed)

Тогда можно будет быстро переключаться между ними и выбрать лучший!
