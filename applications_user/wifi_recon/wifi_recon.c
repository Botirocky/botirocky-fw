/*
 * WiFi Scanner — Flipper Zero app
 *
 * A Flipper rádiója nem tud WiFi-t szkennelni, ezért egy külső ESP32 végzi a
 * tényleges scant és UART-on (GPIO 13=TX / 14=RX) küldi az eredményt. Ez az app
 * fogadja, parse-olja és a képernyőn listázza a hálózatokat RSSI szerint.
 *
 * Soros protokoll (ASCII, sorvégjel \n):
 *   START\n                                   - egy scan-köteg eleje
 *   AP\t<rssi>\t<ch>\t<enc>\t<bssid>\t<ssid>\n - egy hálózat
 *   END\n                                      - a köteg vége (ekkor frissül a lista)
 * A Flipper OK gombra 's\n'-t küld vissza: azonnali újrascan kérése.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <notification/notification_messages.h>

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "WifiScanner"

#define MAX_NETWORKS      48
#define SSID_MAX_LEN      33 // 32 + null
#define BSSID_STR_LEN     18 // "AA:BB:CC:DD:EE:FF\0"
#define UART_BAUD         115200
#define RX_STREAM_SIZE    512
#define LINE_BUF_LEN      160
#define REFRESH_INTERVAL_MS 250
#define STALE_TIMEOUT_MS  15000 // ha ennyi ideig nincs adat, "no signal"

// ─── Adatszerkezetek ────────────────────────────────────────────────────────

typedef struct {
    char ssid[SSID_MAX_LEN];
    char bssid[BSSID_STR_LEN];
    int8_t rssi;
    uint8_t channel;
    uint8_t enc; // ESP32 wifi_auth_mode_t (0..8)
} WifiAp;

typedef enum {
    WifiScannerViewList,
    WifiScannerViewDetail,
} WifiScannerViewId;

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRx = (1 << 1),
} WorkerEvent;

typedef struct {
    // GUI
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* list_view;
    View* detail_view;
    NotificationApp* notifications;

    // UART
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker;

    // Megjelenített lista (mutex védi, a GUI olvassa)
    WifiAp networks[MAX_NETWORKS];
    uint8_t network_count;
    FuriMutex* mutex;

    // A worker thread saját, épülő köteg-buffere (csak a worker írja)
    WifiAp build[MAX_NETWORKS];
    uint8_t build_count;

    // Állapot
    uint8_t selected_index;
    uint32_t last_packet_ms;
    uint32_t batches_received;

    // Timer a periodikus újrarajzoláshoz
    FuriTimer* refresh_timer;
} WifiScannerApp;

// ─── Helperek ───────────────────────────────────────────────────────────────

static int8_t rssi_clamp(int rssi) {
    if(rssi > 0) return 0;
    if(rssi < -100) return -100;
    return (int8_t)rssi;
}

// Jelerősség: 0-4 sáv RSSI alapján
static uint8_t rssi_bars(int8_t rssi) {
    if(rssi >= -55) return 4;
    if(rssi >= -67) return 3;
    if(rssi >= -80) return 2;
    if(rssi >= -90) return 1;
    return 0;
}

// ESP32 wifi_auth_mode_t -> rövid címke
static const char* enc_name(uint8_t enc) {
    switch(enc) {
    case 0: return "Open";
    case 1: return "WEP";
    case 2: return "WPA";
    case 3: return "WPA2";
    case 4: return "WPA/2";
    case 5: return "WPA2-EAP";
    case 6: return "WPA3";
    case 7: return "WPA2/3";
    case 8: return "WAPI";
    default: return "?";
    }
}

// ─── Sor-parser (a worker threaden fut) ──────────────────────────────────────

// Egy tab-elválasztott mezőt vág le a *p elejéről, a maradékot visszaadja *p-ben.
// Üres mezőt is kezel. A visszatérési érték a mező eleje (mindig null-terminált).
static char* take_field(char** p) {
    char* start = *p;
    if(start == NULL) return (char*)"";
    char* tab = strchr(start, '\t');
    if(tab) {
        *tab = '\0';
        *p = tab + 1;
    } else {
        *p = NULL;
    }
    return start;
}

static void parse_ap_line(WifiScannerApp* app, char* line) {
    // line a "AP\t" után kezdődik: <rssi>\t<ch>\t<enc>\t<bssid>\t<ssid>
    char* p = line;
    char* f_rssi = take_field(&p);
    char* f_ch = take_field(&p);
    char* f_enc = take_field(&p);
    char* f_bssid = take_field(&p);
    char* f_ssid = (p != NULL) ? p : (char*)""; // maradék az SSID (tabot nem tartalmaz)

    if(app->build_count >= MAX_NETWORKS) return;
    WifiAp* ap = &app->build[app->build_count];
    memset(ap, 0, sizeof(WifiAp));

    ap->rssi = rssi_clamp(atoi(f_rssi));
    ap->channel = (uint8_t)atoi(f_ch);
    ap->enc = (uint8_t)atoi(f_enc);
    strncpy(ap->bssid, f_bssid, BSSID_STR_LEN - 1);

    if(f_ssid[0] == '\0') {
        strncpy(ap->ssid, "<hidden>", SSID_MAX_LEN - 1);
    } else {
        strncpy(ap->ssid, f_ssid, SSID_MAX_LEN - 1);
    }

    app->build_count++;
}

// RSSI szerint csökkenő rendezés (beszúrásos — max MAX_NETWORKS elem)
static void sort_by_rssi(WifiAp* arr, uint8_t count) {
    for(uint8_t i = 1; i < count; i++) {
        WifiAp key = arr[i];
        int j = (int)i - 1;
        while(j >= 0 && arr[j].rssi < key.rssi) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static void commit_batch(WifiScannerApp* app) {
    sort_by_rssi(app->build, app->build_count);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(app->networks, app->build, sizeof(WifiAp) * app->build_count);
    app->network_count = app->build_count;
    if(app->selected_index >= app->network_count && app->network_count > 0) {
        app->selected_index = app->network_count - 1;
    }
    furi_mutex_release(app->mutex);

    app->last_packet_ms = furi_get_tick();
    app->batches_received++;
    notification_message(app->notifications, &sequence_blink_blue_10);
}

static void parse_line(WifiScannerApp* app, char* line) {
    if(strcmp(line, "START") == 0) {
        app->build_count = 0;
    } else if(strcmp(line, "END") == 0) {
        commit_batch(app);
    } else if(strncmp(line, "AP\t", 3) == 0) {
        parse_ap_line(app, line + 3);
    }
    // ismeretlen sorokat csendben eldobjuk
}

// ─── UART RX callback (ISR-kontextus!) ───────────────────────────────────────

static void uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    WifiScannerApp* app = (WifiScannerApp*)context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(app->worker), WorkerEvtRx);
    }
}

// ─── Worker thread: bájtfolyam -> sorok -> parse ─────────────────────────────

static int32_t uart_worker(void* context) {
    WifiScannerApp* app = (WifiScannerApp*)context;
    char line[LINE_BUF_LEN];
    size_t pos = 0;
    uint8_t buf[64];

    while(true) {
        uint32_t events =
            furi_thread_flags_wait(WorkerEvtStop | WorkerEvtRx, FuriFlagWaitAny, FuriWaitForever);

        if(events & WorkerEvtStop) break;

        if(events & WorkerEvtRx) {
            size_t len;
            do {
                len = furi_stream_buffer_receive(app->rx_stream, buf, sizeof(buf), 0);
                for(size_t i = 0; i < len; i++) {
                    char c = (char)buf[i];
                    if(c == '\n' || c == '\r') {
                        if(pos > 0) {
                            line[pos] = '\0';
                            parse_line(app, line);
                            pos = 0;
                        }
                    } else if(pos < LINE_BUF_LEN - 1) {
                        line[pos++] = c;
                    } else {
                        pos = 0; // túl hosszú sor: eldobjuk
                    }
                }
            } while(len > 0);
        }
    }
    return 0;
}

// ─── Lista nézet rajzolása ───────────────────────────────────────────────────

static void list_draw_callback(Canvas* canvas, void* model) {
    WifiScannerApp* app = *(WifiScannerApp**)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    bool stale = (furi_get_tick() - app->last_packet_ms) > STALE_TIMEOUT_MS;

    char header[32];
    if(app->batches_received == 0) {
        snprintf(header, sizeof(header), "WiFi: waiting ESP...");
    } else if(stale) {
        snprintf(header, sizeof(header), "WiFi: no signal");
    } else {
        snprintf(header, sizeof(header), "WiFi - %u APs", app->network_count);
    }
    canvas_draw_str(canvas, 0, 10, header);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    canvas_set_font(canvas, FontSecondary);

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(app->network_count == 0) {
        canvas_draw_str(canvas, 8, 34, "No networks yet.");
        canvas_draw_str(canvas, 8, 46, "Check ESP32 + wiring.");
        furi_mutex_release(app->mutex);
        return;
    }

    uint8_t visible = 4;
    uint8_t start = 0;
    if(app->selected_index >= visible) {
        start = app->selected_index - visible + 1;
    }

    for(uint8_t i = 0; i < visible && (start + i) < app->network_count; i++) {
        uint8_t idx = start + i;
        WifiAp* ap = &app->networks[idx];
        uint8_t y = 24 + i * 12;

        if(idx == app->selected_index) {
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        // Jelsávok (jobb oldalon)
        uint8_t bars = rssi_bars(ap->rssi);
        for(uint8_t b = 0; b < 4; b++) {
            uint8_t bx = 119 + b * 3;
            uint8_t bh = 2 + b * 2;
            uint8_t by = y - bh + 1;
            if(b < bars) {
                canvas_draw_box(canvas, bx, by, 2, bh);
            } else {
                canvas_draw_frame(canvas, bx, by, 2, bh);
            }
        }

        // SSID (levágva, hogy elférjen a sávok előtt)
        char display[20];
        snprintf(display, sizeof(display), "%s", ap->ssid);
        canvas_draw_str(canvas, 2, y, display);

        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(app->mutex);

    canvas_draw_str(canvas, 0, 63, "OK:Info  Back:Exit");
}

// ─── Detail nézet rajzolása ──────────────────────────────────────────────────

static void detail_draw_callback(Canvas* canvas, void* model) {
    WifiScannerApp* app = *(WifiScannerApp**)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Network Info");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(app->selected_index < app->network_count) {
        WifiAp* ap = &app->networks[app->selected_index];
        canvas_set_font(canvas, FontSecondary);

        char l[48];
        snprintf(l, sizeof(l), "SSID: %s", ap->ssid);
        canvas_draw_str(canvas, 0, 25, l);
        snprintf(l, sizeof(l), "BSSID: %s", ap->bssid);
        canvas_draw_str(canvas, 0, 36, l);
        snprintf(l, sizeof(l), "RSSI: %d dBm  Ch: %u", ap->rssi, ap->channel);
        canvas_draw_str(canvas, 0, 47, l);

        const char* quality[] = {"Very Weak", "Weak", "Fair", "Good", "Excellent"};
        snprintf(
            l, sizeof(l), "Sec: %s  %s", enc_name(ap->enc), quality[rssi_bars(ap->rssi)]);
        canvas_draw_str(canvas, 0, 58, l);
    }

    furi_mutex_release(app->mutex);
}

// ─── Input kezelők ───────────────────────────────────────────────────────────

static bool list_input_callback(InputEvent* event, void* context) {
    WifiScannerApp* app = (WifiScannerApp*)context;

    if(event->type != InputTypePress && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->selected_index > 0) app->selected_index--;
        furi_mutex_release(app->mutex);
        with_view_model(app->list_view, void* m, { UNUSED(m); }, true);
        return true;

    case InputKeyDown:
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->selected_index + 1 < app->network_count) app->selected_index++;
        furi_mutex_release(app->mutex);
        with_view_model(app->list_view, void* m, { UNUSED(m); }, true);
        return true;

    case InputKeyOk: {
        bool has = false;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        has = app->network_count > 0;
        furi_mutex_release(app->mutex);
        if(has) {
            view_dispatcher_switch_to_view(app->view_dispatcher, WifiScannerViewDetail);
        }
        // Újrascan kérése az ESP-től
        const char req = 's';
        furi_hal_serial_tx(app->serial, (const uint8_t*)&req, 1);
        return true;
    }

    case InputKeyBack:
        view_dispatcher_stop(app->view_dispatcher);
        return true;

    default:
        break;
    }
    return false;
}

static bool detail_input_callback(InputEvent* event, void* context) {
    WifiScannerApp* app = (WifiScannerApp*)context;
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiScannerViewList);
        return true;
    }
    return false;
}

// ─── Refresh timer: periodikus újrarajzolás ──────────────────────────────────

static void refresh_timer_callback(void* context) {
    WifiScannerApp* app = (WifiScannerApp*)context;
    // Csak triggereljük az újrarajzolást (a model maga az app pointer, nem változik)
    with_view_model(app->list_view, void* m, { UNUSED(m); }, true);
}

// ─── Alloc / Free ────────────────────────────────────────────────────────────

static WifiScannerApp* wifi_scanner_app_alloc(void) {
    WifiScannerApp* app = malloc(sizeof(WifiScannerApp));
    memset(app, 0, sizeof(WifiScannerApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Worker thread
    app->worker = furi_thread_alloc_ex("WifiScannerWorker", 2048, uart_worker, app);
    furi_thread_start(app->worker);

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Lista nézet — a model egy WifiScannerApp* pointert tárol
    app->list_view = view_alloc();
    view_set_draw_callback(app->list_view, list_draw_callback);
    view_set_input_callback(app->list_view, list_input_callback);
    view_set_context(app->list_view, app);
    view_allocate_model(app->list_view, ViewModelTypeLocking, sizeof(WifiScannerApp*));
    with_view_model(
        app->list_view, WifiScannerApp** m, { *m = app; }, true);
    view_dispatcher_add_view(app->view_dispatcher, WifiScannerViewList, app->list_view);

    // Detail nézet
    app->detail_view = view_alloc();
    view_set_draw_callback(app->detail_view, detail_draw_callback);
    view_set_input_callback(app->detail_view, detail_input_callback);
    view_set_context(app->detail_view, app);
    view_allocate_model(app->detail_view, ViewModelTypeLocking, sizeof(WifiScannerApp*));
    with_view_model(
        app->detail_view, WifiScannerApp** m, { *m = app; }, true);
    view_dispatcher_add_view(app->view_dispatcher, WifiScannerViewDetail, app->detail_view);

    // UART megszerzése és indítása
    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(app->serial) {
        furi_hal_serial_init(app->serial, UART_BAUD);
        furi_hal_serial_async_rx_start(app->serial, uart_rx_callback, app, false);
    } else {
        FURI_LOG_E(TAG, "USART foglalt, nem sikerult megszerezni");
    }

    // Refresh timer
    app->refresh_timer = furi_timer_alloc(refresh_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->refresh_timer, REFRESH_INTERVAL_MS);

    return app;
}

static void wifi_scanner_app_free(WifiScannerApp* app) {
    furi_timer_stop(app->refresh_timer);
    furi_timer_free(app->refresh_timer);

    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
    }

    // Worker leállítása
    furi_thread_flags_set(furi_thread_get_id(app->worker), WorkerEvtStop);
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, WifiScannerViewList);
    view_dispatcher_remove_view(app->view_dispatcher, WifiScannerViewDetail);
    view_free(app->list_view);
    view_free(app->detail_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_stream_buffer_free(app->rx_stream);
    furi_mutex_free(app->mutex);
    free(app);
}

// ─── Belépési pont ───────────────────────────────────────────────────────────

int32_t wifi_recon_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "WiFi Scanner indul");

    WifiScannerApp* app = wifi_scanner_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, WifiScannerViewList);
    view_dispatcher_run(app->view_dispatcher);

    wifi_scanner_app_free(app);
    FURI_LOG_I(TAG, "WiFi Scanner kilep");
    return 0;
}
