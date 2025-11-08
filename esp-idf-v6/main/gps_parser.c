/**
 * @file gps_parser.c
 * @brief NMEA GPS парсер для UM980 GNSS модуля
 *
 * Поддерживаемые сообщения:
 * - GNS: координаты, altitude, satellites, fix quality
 * - GST: точность (lat/lon/vert accuracy)
 * - GGA: точный fix quality (RTK Fixed/Float)
 * - GSV: видимые спутники по системам
 * - GSA: используемые спутники
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "common.h"

static const char *TAG = "GPS";

// Буфер для парсинга NMEA (статический для экономии стека)
#define NMEA_PARSE_BUFFER_SIZE 256
static char nmea_parse_buffer[NMEA_PARSE_BUFFER_SIZE];

// Буфер для входящих NMEA строк
#define NMEA_LINE_BUFFER_SIZE 256
static char nmea_line_buffer[NMEA_LINE_BUFFER_SIZE];
static size_t nmea_line_pos = 0;

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

/**
 * @brief Преобразование DDMM.MMM в десятичные градусы
 */
static double convert_to_decimal_degrees(double ddmm) {
    int degrees = (int)(ddmm / 100.0);
    double minutes = ddmm - (degrees * 100.0);
    return degrees + minutes / 60.0;
}

/**
 * @brief Получить ранг режима позиционирования
 */
static int get_mode_rank(char mode) {
    switch (mode) {
        case 'R': return 6; // RTK integer (fixed)
        case 'F': return 5; // RTK float
        case 'P': return 4; // High precision
        case 'D': return 3; // Differential
        case 'A': return 2; // Autonomous
        case 'M': return 1; // Manual input
        case 'S': return 0; // Simulator
        default:  return -1; // N / unknown
    }
}

/**
 * @brief Разбить NMEA строку на поля (modifies input!)
 * @param nmea Входная NMEA строка (будет изменена!)
 * @param fields Массив указателей на поля
 * @param max_fields Максимальное количество полей
 * @return Количество найденных полей
 */
static int split_fields(char *nmea, char *fields[], int max_fields) {
    int count = 0;
    char *p = nmea;

    while (*p && count < max_fields) {
        fields[count++] = p;

        // Ищем запятую или звёздочку (начало checksum)
        char *c = p;
        while (*c && *c != ',' && *c != '*') {
            c++;
        }

        // Если нашли разделитель
        if (*c) {
            char separator = *c;
            *c = '\0';  // Терминируем текущее поле
            p = c + 1;  // Переходим к следующему полю

            // Если нашли звёздочку - это конец данных
            if (separator == '*') {
                break;
            }
        } else {
            // Конец строки
            break;
        }
    }

    return count;
}

/**
 * @brief Получить строковое представление типа фикса
 */
static const char* get_fix_type_string(int quality) {
    switch(quality) {
        case 0: return "NO FIX";
        case 1: return "GPS";
        case 2: return "DGPS";
        case 3: return "PPS";
        case 4: return "RTK Fix";
        case 5: return "RTK Flt";
        case 7: return "MANUAL";
        case 8: return "SIMUL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Парсинг времени из формата hhmmss.ss
 * @param time_str Строка времени в формате hhmmss.ss
 * @param hour Указатель для записи часов
 * @param minute Указатель для записи минут
 * @param second Указатель для записи секунд
 * @return true если парсинг успешен
 */
static bool parse_time(const char *time_str, int *hour, int *minute, int *second) {
    if (!time_str || strlen(time_str) < 6) {
        return false;
    }

    // Парсим hhmmss из первых 6 символов
    char hh[3] = {time_str[0], time_str[1], '\0'};
    char mm[3] = {time_str[2], time_str[3], '\0'};
    char ss[3] = {time_str[4], time_str[5], '\0'};

    *hour = atoi(hh);
    *minute = atoi(mm);
    *second = atoi(ss);

    // Проверка валидности
    if (*hour < 0 || *hour > 23 || *minute < 0 || *minute > 59 || *second < 0 || *second > 59) {
        return false;
    }

    return true;
}

/**
 * @brief Вычисление смещения часового пояса от долготы
 * @param longitude Долгота в десятичных градусах
 * @return Смещение в минутах
 */
static int estimate_timezone_offset_from_longitude(double longitude) {
    // Каждые 15 градусов долготы = 1 час = 60 минут
    // Положительная долгота (восток) дает положительное смещение
    int offset_hours = (int)round(longitude / 15.0);

    // Ограничиваем от -12 до +14 часов
    if (offset_hours < -12) offset_hours = -12;
    if (offset_hours > 14) offset_hours = 14;

    return offset_hours * 60;  // Возвращаем в минутах
}

// ==================================================
// ПАРСЕРЫ NMEA СООБЩЕНИЙ
// ==================================================

/**
 * @brief Парсер GST для точности позиционирования
 * Формат: $GNGST,hhmmss.ss,rms,major,minor,orient,lat_err,lon_err,alt_err*cs
 */
static void parse_gst(const char *nmea) {
    strncpy(nmea_parse_buffer, nmea, sizeof(nmea_parse_buffer) - 1);
    nmea_parse_buffer[sizeof(nmea_parse_buffer) - 1] = '\0';

    char *fields[32];
    int n = split_fields(nmea_parse_buffer, fields, 32);
    if (n < 9) return;

    // Field 7: Lat accuracy (метры)
    if (fields[6] && *fields[6]) {
        float val = atof(fields[6]);
        if (val > 0.0f && val < 100.0f) {
            g_gps_data.lat_accuracy = val;
        }
    }

    // Field 8: Lon accuracy (метры)
    if (fields[7] && *fields[7]) {
        float val = atof(fields[7]);
        if (val > 0.0f && val < 100.0f) {
            g_gps_data.lon_accuracy = val;
        }
    }

    // Field 9: Alt accuracy (метры)
    if (fields[8] && *fields[8]) {
        float val = atof(fields[8]);
        if (val > 0.0f && val < 100.0f) {
            g_gps_data.vert_accuracy = val;
        }
    }

    g_gps_data.last_gst_update = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief Парсер GNS для координат и основных данных
 * Формат: $GNGNS,hhmmss.ss,lat,N/S,lon,E/W,mode,numSV,HDOP,alt,sep,age,stnID*cs
 */
static void parse_gns(const char *nmea) {
    strncpy(nmea_parse_buffer, nmea, sizeof(nmea_parse_buffer) - 1);
    nmea_parse_buffer[sizeof(nmea_parse_buffer) - 1] = '\0';

    char *fields[32];
    int n = split_fields(nmea_parse_buffer, fields, 32);
    if (n < 11) return;

    // Field 1: Time UTC (hhmmss.ss)
    if (fields[1] && *fields[1]) {
        int hour, minute, second;
        if (parse_time(fields[1], &hour, &minute, &second)) {
            g_gps_data.hour = hour;
            g_gps_data.minute = minute;
            g_gps_data.second = second;
            g_gps_data.time_valid = true;
        }
    }

    // Field 3: Latitude (DDMM.MMM)
    if (fields[2] && fields[3] && *fields[2] && *fields[3]) {
        double lat = convert_to_decimal_degrees(atof(fields[2]));
        if (fields[3][0] == 'S') lat = -lat;
        g_gps_data.latitude = lat;
        g_gps_data.last_update = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    // Field 5: Longitude (DDDMM.MMM)
    if (fields[4] && fields[5] && *fields[4] && *fields[5]) {
        double lon = convert_to_decimal_degrees(atof(fields[4]));
        if (fields[5][0] == 'W') lon = -lon;
        g_gps_data.longitude = lon;

        // Вычисляем timezone offset от долготы
        g_gps_data.timezone_offset_minutes = estimate_timezone_offset_from_longitude(lon);
    }

    // Field 7: Mode indicators (GPS,GLONASS,Galileo,BDS,QZSS,NavIC)
    if (fields[6] && *fields[6]) {
        // Определяем длину поля (максимум 6 систем)
        size_t modes_len = 0;
        while (fields[6][modes_len] && fields[6][modes_len] != ',' &&
               fields[6][modes_len] != '*') modes_len++;
        if (modes_len > 6) modes_len = 6;

        char best_mode = 'N';
        int best_rank = -1;
        bool has_valid_fix = false;

        for (size_t i = 0; i < modes_len; i++) {
            char mode = fields[6][i];
            int rank = get_mode_rank(mode);
            if (mode == 'A' || mode == 'D' || mode == 'P' || mode == 'F' || mode == 'R') {
                has_valid_fix = true;
            }
            if (rank > best_rank) {
                best_rank = rank;
                best_mode = mode;
            }
        }

        // Устанавливаем fixQuality по лучшему режиму
        switch (best_mode) {
            case 'A': g_gps_data.fix_quality = 1; break;
            case 'D': g_gps_data.fix_quality = 2; break;
            case 'P': g_gps_data.fix_quality = 3; break;
            case 'R': g_gps_data.fix_quality = 4; break;
            case 'F': g_gps_data.fix_quality = 5; break;
            case 'M': g_gps_data.fix_quality = 7; break;
            case 'S': g_gps_data.fix_quality = 8; break;
            default:  g_gps_data.fix_quality = 0; break;
        }

        g_gps_data.valid = has_valid_fix;
    }

    // Field 8: Number of satellites (только из GNGNS, не из GPGNS/GLGNS/etc)
    if (strncmp(nmea, "$GNGNS", 6) == 0) {
        if (fields[7] && *fields[7]) {
            g_gps_data.satellites = atoi(fields[7]);
        }
    }

    // Field 10: Altitude (метры)
    if (n > 9 && fields[9] && *fields[9]) {
        g_gps_data.altitude = atof(fields[9]);
    }
}

/**
 * @brief Парсер GGA для точного определения типа фикса
 * Формат: $GNGGA,hhmmss.ss,lat,N/S,lon,E/W,quality,numSV,hdop,alt,M,sep,M,age,stnID*cs
 * GGA имеет точное поле quality indicator (приоритетнее GNS)
 */
static void parse_gga(const char *nmea) {
    // Парсим только GNGGA (комбинированное)
    if (strncmp(nmea, "$GNGGA", 6) != 0) return;

    strncpy(nmea_parse_buffer, nmea, sizeof(nmea_parse_buffer) - 1);
    nmea_parse_buffer[sizeof(nmea_parse_buffer) - 1] = '\0';

    char *fields[15];
    int n = split_fields(nmea_parse_buffer, fields, 15);

    if (n < 7) return;

    // Field 6: Quality indicator
    // 0 = Fix not available, 1 = GPS, 2 = DGPS, 3 = PPS,
    // 4 = RTK Fixed, 5 = RTK Float, 7 = Manual, 8 = Simulator
    if (fields[6] && *fields[6]) {
        int quality = atoi(fields[6]);

        if (quality >= 0 && quality <= 8) {
            g_gps_data.fix_quality = quality;

            // Устанавливаем valid для качественных фиксов
            if (quality >= 1 && quality <= 5) {
                g_gps_data.valid = true;
            } else {
                g_gps_data.valid = false;
            }
        }
    }
}

/**
 * @brief Парсер GSV для видимых спутников
 * Формат: $GPGSV,totalMsg,msgNum,totalSats,...*cs
 */
static void parse_gsv(const char *nmea) {
    strncpy(nmea_parse_buffer, nmea, sizeof(nmea_parse_buffer) - 1);
    nmea_parse_buffer[sizeof(nmea_parse_buffer) - 1] = '\0';

    char *fields[32];
    int n = split_fields(nmea_parse_buffer, fields, 32);
    if (n < 4) return;

    int total = atoi(fields[3]); // поле 3 = общее число видимых спутников
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (strncmp(nmea, "$GPGSV", 6) == 0) {
        g_sat_data.gps.visible = total;
        g_sat_data.gps.lastUpdate = now;
    } else if (strncmp(nmea, "$GLGSV", 6) == 0) {
        g_sat_data.glonass.visible = total;
        g_sat_data.glonass.lastUpdate = now;
    } else if (strncmp(nmea, "$GAGSV", 6) == 0) {
        g_sat_data.galileo.visible = total;
        g_sat_data.galileo.lastUpdate = now;
    } else if (strncmp(nmea, "$GBGSV", 6) == 0) {
        g_sat_data.beidou.visible = total;
        g_sat_data.beidou.lastUpdate = now;
    }
}

/**
 * @brief Парсер GSA для используемых спутников
 * Формат: $GPGSA,mode,fixType,sv1,...,sv12,PDOP,HDOP,VDOP*cs
 * Формат GNGSA: $GNGSA,mode,fixType,sv1,...,sv12,PDOP,HDOP,VDOP,systemID*cs
 */
static void parse_gsa(const char *nmea) {
    strncpy(nmea_parse_buffer, nmea, sizeof(nmea_parse_buffer) - 1);
    nmea_parse_buffer[sizeof(nmea_parse_buffer) - 1] = '\0';

    char *fields[32];
    int n = split_fields(nmea_parse_buffer, fields, 32);
    if (n < 4) return;

    // Считаем количество используемых спутников (поля 3-14)
    int used = 0;
    for (int i = 3; i <= 14 && i < n; i++) {
        if (fields[i] && *fields[i] && atoi(fields[i]) > 0) {
            used++;
        }
    }

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (strncmp(nmea, "$GPGSA", 6) == 0) {
        g_sat_data.gps.used = used;
        g_sat_data.gps.lastUpdate = now;
    } else if (strncmp(nmea, "$GLGSA", 6) == 0) {
        g_sat_data.glonass.used = used;
        g_sat_data.glonass.lastUpdate = now;
    } else if (strncmp(nmea, "$GAGSA", 6) == 0) {
        g_sat_data.galileo.used = used;
        g_sat_data.galileo.lastUpdate = now;
    } else if (strncmp(nmea, "$GBGSA", 6) == 0) {
        g_sat_data.beidou.used = used;
        g_sat_data.beidou.lastUpdate = now;
    } else if (strncmp(nmea, "$GNGSA", 6) == 0) {
        // GNGSA - комбинированное сообщение с System ID в поле 18
        // System ID: 1=GPS, 2=GLONASS, 3=Galileo, 4=BeiDou, 5=QZSS, 6=NavIC
        if (n > 18 && fields[18] && *fields[18]) {
            int system_id = atoi(fields[18]);

            switch (system_id) {
                case 1:  // GPS
                    g_sat_data.gps.used = used;
                    g_sat_data.gps.lastUpdate = now;
                    break;
                case 2:  // GLONASS
                    g_sat_data.glonass.used = used;
                    g_sat_data.glonass.lastUpdate = now;
                    break;
                case 3:  // Galileo
                    g_sat_data.galileo.used = used;
                    g_sat_data.galileo.lastUpdate = now;
                    break;
                case 4:  // BeiDou
                    g_sat_data.beidou.used = used;
                    g_sat_data.beidou.lastUpdate = now;
                    break;
                case 5:  // QZSS
                    g_sat_data.qzss.used = used;
                    g_sat_data.qzss.lastUpdate = now;
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown GNGSA System ID: %d", system_id);
                    break;
            }
        }
    }
}

/**
 * @brief Универсальный диспетчер NMEA сообщений
 */
static void parse_nmea(const char *nmea) {
    // Проверяем что это GNSS сообщение
    if (strncmp(nmea, "$GP", 3) != 0 && strncmp(nmea, "$GA", 3) != 0 &&
        strncmp(nmea, "$GL", 3) != 0 && strncmp(nmea, "$GB", 3) != 0 &&
        strncmp(nmea, "$GQ", 3) != 0 && strncmp(nmea, "$GN", 3) != 0) {
        return;
    }

    // Диспетчеризация по типу сообщения
    if (strstr(nmea, "GSV")) parse_gsv(nmea);
    else if (strstr(nmea, "GSA")) parse_gsa(nmea);
    else if (strstr(nmea, "GST")) parse_gst(nmea);
    else if (strstr(nmea, "GGA")) parse_gga(nmea);  // GGA первым для fixQuality
    else if (strstr(nmea, "GNS")) parse_gns(nmea);  // GNS для координат
}

/**
 * @brief Проверка таймаутов для данных спутников
 */
static void check_satellite_timeouts(void) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    const uint32_t TIMEOUT_MS = 5000; // 5 секунд

    if (now - g_sat_data.gps.lastUpdate > TIMEOUT_MS) {
        g_sat_data.gps.visible = 0;
        g_sat_data.gps.used = 0;
    }
    if (now - g_sat_data.glonass.lastUpdate > TIMEOUT_MS) {
        g_sat_data.glonass.visible = 0;
        g_sat_data.glonass.used = 0;
    }
    if (now - g_sat_data.galileo.lastUpdate > TIMEOUT_MS) {
        g_sat_data.galileo.visible = 0;
        g_sat_data.galileo.used = 0;
    }
    if (now - g_sat_data.beidou.lastUpdate > TIMEOUT_MS) {
        g_sat_data.beidou.visible = 0;
        g_sat_data.beidou.used = 0;
    }
}

// ==================================================
// ФУНКЦИЯ ПАРСИНГА ПО БАЙТАМ (вызывается из UART task)
// ==================================================

/**
 * @brief Парсинг GPS данных побайтно (вызывается из UART task)
 * Эта функция НЕ читает из g_ble_tx_buffer - данные передаются напрямую!
 * g_ble_tx_buffer используется только для СКВОЗНОЙ передачи BLE/WiFi
 */
void gps_parse_byte(uint8_t byte) {
    // Собираем NMEA строку
    if (byte == '\n') {
        // Конец строки
        nmea_line_buffer[nmea_line_pos] = '\0';

        // Парсим если строка не пустая и начинается с '$'
        if (nmea_line_pos > 0 && nmea_line_buffer[0] == '$') {
            parse_nmea(nmea_line_buffer);
        }

        nmea_line_pos = 0;
    }
    else if (byte != '\r') {
        // Добавляем символ в буфер (игнорируем '\r')
        if (nmea_line_pos < NMEA_LINE_BUFFER_SIZE - 1) {
            nmea_line_buffer[nmea_line_pos++] = byte;
        } else {
            // Переполнение - сбрасываем буфер
            nmea_line_pos = 0;
        }
    }
}

/**
 * @brief Задача мониторинга GPS (таймауты и логирование)
 */
void gps_parser_task(void *pvParameters) {
    ESP_LOGI(TAG, "GPS Parser task started on core %d", xPortGetCoreID());

    uint32_t last_timeout_check = 0;

    while (1) {
        // Проверяем таймауты раз в секунду
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_timeout_check > 1000) {
            check_satellite_timeouts();
            last_timeout_check = now;

            // GPS статус выводится на дисплей, логи не нужны
            // static uint32_t last_log = 0;
            // if (now - last_log > 5000) {
            //     ESP_LOGI(TAG, "GPS: %.6f,%.6f alt=%.1f sat=%d fix=%s acc=%.2f/%.2f/%.2f",
            //              g_gps_data.latitude, g_gps_data.longitude, g_gps_data.altitude,
            //              g_gps_data.satellites, get_fix_type_string(g_gps_data.fix_quality),
            //              g_gps_data.lat_accuracy, g_gps_data.lon_accuracy, g_gps_data.vert_accuracy);
            //     last_log = now;
            // }
        }

        // Задержка 1 секунда
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
