#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>
#include <extra_profiles/hid_profile.h>

#include "amitablet_ble_profile.h"
#include "amitablet_icons.h"

#define AMITABLET_BT_KEYS_FILE ".bt_hid.keys"
#define AMITABLET_SCROLL_STEP 1

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    bool connected;
    bool running;
    uint32_t bt_event_count;
    const char* last_input;
} AmiTabletApp;

#define AMITABLET_VERSION_LABEL "v1.0"

/* ------------------------------------------------------------------ */
/* V1.0 portrait UI (64x128 with ViewPortOrientationVerticalFlip).     */
/* Follows approved mockup 2, adapted to 64px width:                   */
/*   y  0..27  header icon (48x28, centered)                           */
/*   y 37      title "AmiTablet" (FontPrimary, centered)               */
/*   y 45      version label (FontSecondary, centered, discreet)       */
/*   y 46..56  BLE status badge (inverted rounded capsule)             */
/*   y 57..110 6 control rows, 9px each (framed pictogram + label)     */
/*   y 110     separator line                                         */
/*   y112..127 footer frame with exit hint (two lines)                 */
/* All coordinates below are in portrait space.                        */
/* ------------------------------------------------------------------ */

static void amitablet_draw_centered(Canvas* canvas, int32_t cx, int32_t y, const char* str) {
    uint16_t w = canvas_string_width(canvas, str);
    canvas_draw_str(canvas, cx - (int32_t)w / 2, y, str);
}

static void amitablet_draw_right_aligned(Canvas* canvas, int32_t x_right, int32_t y, const char* str) {
    uint16_t w = canvas_string_width(canvas, str);
    canvas_draw_str(canvas, x_right - (int32_t)w, y, str);
}

/* Pictograms below draw inside a 9x7 cell at (bx, by). */

static void amitablet_draw_picto_up(Canvas* canvas, int32_t bx, int32_t by) {
    int32_t cx = bx + 4;
    canvas_draw_line(canvas, cx, by + 1, cx - 2, by + 4);
    canvas_draw_line(canvas, cx, by + 1, cx + 2, by + 4);
    canvas_draw_line(canvas, cx - 2, by + 4, cx + 2, by + 4);
}

static void amitablet_draw_picto_down(Canvas* canvas, int32_t bx, int32_t by) {
    int32_t cx = bx + 4;
    canvas_draw_line(canvas, cx, by + 5, cx - 2, by + 2);
    canvas_draw_line(canvas, cx, by + 5, cx + 2, by + 2);
    canvas_draw_line(canvas, cx - 2, by + 2, cx + 2, by + 2);
}

static void amitablet_draw_picto_left(Canvas* canvas, int32_t bx, int32_t by) {
    int32_t cy = by + 3;
    canvas_draw_line(canvas, bx + 1, cy, bx + 4, cy - 2);
    canvas_draw_line(canvas, bx + 1, cy, bx + 4, cy + 2);
    canvas_draw_line(canvas, bx + 4, cy - 2, bx + 4, cy + 2);
}

static void amitablet_draw_picto_right(Canvas* canvas, int32_t bx, int32_t by) {
    int32_t cy = by + 3;
    canvas_draw_line(canvas, bx + 7, cy, bx + 4, cy - 2);
    canvas_draw_line(canvas, bx + 7, cy, bx + 4, cy + 2);
    canvas_draw_line(canvas, bx + 4, cy - 2, bx + 4, cy + 2);
}

static void amitablet_draw_picto_ok(Canvas* canvas, int32_t bx, int32_t by) {
    canvas_draw_circle(canvas, bx + 4, by + 3, 2);
    canvas_draw_dot(canvas, bx + 4, by + 3);
}

/* BACK return-arrow pictogram in a 9x7 cell at (bx, by).
   A "BACK" text capsule (as in mockup 2) does not fit next to the
   "Screenshot" label in 64px, so a return-arrow glyph is used. */
static void amitablet_draw_picto_back(Canvas* canvas, int32_t bx, int32_t by) {
    canvas_draw_line(canvas, bx + 6, by + 1, bx + 6, by + 4);
    canvas_draw_line(canvas, bx + 6, by + 4, bx + 2, by + 4);
    canvas_draw_line(canvas, bx + 2, by + 4, bx + 4, by + 2);
    canvas_draw_line(canvas, bx + 2, by + 4, bx + 4, by + 5);
}

/* Mini Bluetooth rune, 5x7 at (x, y). */
static void amitablet_draw_bt(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_line(canvas, x + 2, y, x + 2, y + 6);
    canvas_draw_line(canvas, x + 2, y, x + 4, y + 2);
    canvas_draw_line(canvas, x + 4, y + 2, x + 2, y + 3);
    canvas_draw_line(canvas, x + 2, y + 3, x + 4, y + 4);
    canvas_draw_line(canvas, x + 4, y + 4, x + 2, y + 6);
}

/* Confirmation tick, 6x5 at (x, y). */
static void amitablet_draw_check(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_line(canvas, x, y + 3, x + 2, y + 5);
    canvas_draw_line(canvas, x + 2, y + 5, x + 5, y + 1);
}

typedef void (*AmiTabletPictoFn)(Canvas* canvas, int32_t bx, int32_t by);

static void amitablet_draw_callback(Canvas* canvas, void* context) {
    AmiTabletApp* app = context;

    canvas_clear(canvas);

    /* Header: tablet-and-stylus icon, title, discreet version. */
    canvas_draw_icon(canvas, 8, 0, &I_amitablet_logo);

    canvas_set_font(canvas, FontPrimary);
    amitablet_draw_centered(canvas, 32, 37, "AmiTablet");

    canvas_set_font(canvas, FontSecondary);
    amitablet_draw_centered(canvas, 32, 45, AMITABLET_VERSION_LABEL);

    /* BLE status badge: inverted rounded capsule (mockup 2) with BT
       symbol, status text, and a tick when linked. Text is measured so
       nothing can overflow the 64px width. */
    const char* ble_text = app->connected ? "BLE: OK" : "BLE: WAIT";
    uint16_t ble_w = canvas_string_width(canvas, ble_text);
    /* Group: [bt 5px][gap 2][text][gap 2 + check 6px when connected]. */
    int32_t group_w = 7 + (int32_t)ble_w + (app->connected ? 8 : 0);
    bool ble_compact = (group_w <= 54);
    int32_t gx = ble_compact ? (3 + (58 - group_w) / 2) : 3;
    canvas_draw_rbox(canvas, 3, 46, 58, 11, 3);
    canvas_invert_color(canvas);
    if(ble_compact) {
        amitablet_draw_bt(canvas, gx, 47);
        canvas_draw_str(canvas, gx + 7, 54, ble_text);
        if(app->connected) {
            amitablet_draw_check(canvas, gx + 7 + (int32_t)ble_w + 2, 47);
        }
    } else {
        /* Fallback: centered text only, guaranteed to fit. */
        amitablet_draw_centered(canvas, 32, 54, ble_text);
    }
    canvas_invert_color(canvas);

    /* Main controls list: 6 rows, framed pictogram left + label right. */
    static const AmiTabletPictoFn pictos[6] = {
        amitablet_draw_picto_up,
        amitablet_draw_picto_down,
        amitablet_draw_picto_left,
        amitablet_draw_picto_right,
        amitablet_draw_picto_ok,
        amitablet_draw_picto_back,
    };
    static const char* const labels[6] = {
        "Scroll up",
        "Scroll down",
        "Back",
        "Forward",
        "Right-click",
        "Screenshot",
    };
    for(int i = 0; i < 6; i++) {
        int32_t ry = 57 + i * 9;
        canvas_draw_rframe(canvas, 2, ry, 9, 7, 1);
        pictos[i](canvas, 2, ry);
        amitablet_draw_right_aligned(canvas, 62, ry + 7, labels[i]);
    }

    /* Footer: separator + framed exit hint (two lines to fit 64px). */
    canvas_draw_line(canvas, 6, 110, 58, 110);
    canvas_draw_rframe(canvas, 4, 112, 56, 16, 2);
    amitablet_draw_centered(canvas, 32, 120, "Hold BACK");
    amitablet_draw_centered(canvas, 32, 126, "to exit");

    /* V0.9 diagnostic state is intentionally not drawn over the final UI.
       It is logged to the console to preserve the V1.0 design. */
}

static void amitablet_input_callback(InputEvent* input_event, void* context) {
    AmiTabletApp* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static void amitablet_bt_status_callback(BtStatus status, void* context) {
    AmiTabletApp* app = context;
    app->connected = (status == BtStatusConnected);
    app->bt_event_count++;
    FURI_LOG_I(
        "AmiTablet",
        "BT event #%lu: status=%d connected=%d",
        (unsigned long)app->bt_event_count,
        (int)status,
        app->connected ? 1 : 0);

    if(app->view_port) {
        view_port_update(app->view_port);
    }
}

static void amitablet_make_identity(AmiTabletBleProfileParams* params) {
    memset(params, 0, sizeof(AmiTabletBleProfileParams));

    strlcpy(params->name, "AmiTablet", sizeof(params->name));

    const uint8_t* flipper_mac = furi_hal_version_get_ble_mac();
    memcpy(params->mac, flipper_mac, sizeof(params->mac));

    /* Stable, dedicated BLE identity for AmiTablet.
       Keep it deterministic so Windows can reuse the bond on later launches. */
    params->mac[0] ^= 0x02;
    params->mac[2] ^= 0xA5;

    params->bonding = true;
    params->pairing = GapPairingPinCodeVerifyYesNo;
}

static void amitablet_ble_init(AmiTabletApp* app) {
    app->bt = furi_record_open(RECORD_BT);

    furi_hal_bt_stop_advertising();
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(AMITABLET_BT_KEYS_FILE));

    AmiTabletBleProfileParams params;
    amitablet_make_identity(&params);

    app->ble_profile =
        bt_profile_start(app->bt, amitablet_ble_profile, (FuriHalBleProfileParams)&params);
    furi_check(app->ble_profile);

    bt_set_status_changed_callback(app->bt, amitablet_bt_status_callback, app);
    furi_hal_bt_start_advertising();
}

static void amitablet_ble_deinit(AmiTabletApp* app) {
    if(!app->bt) return;

    if(app->ble_profile) {
        ble_profile_hid_mouse_release_all(app->ble_profile);
        ble_profile_hid_consumer_key_release_all(app->ble_profile);
    }

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    furi_hal_bt_stop_advertising();
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));

    app->ble_profile = NULL;
    app->connected = false;

    furi_record_close(RECORD_BT);
    app->bt = NULL;
}

static void amitablet_scroll(AmiTabletApp* app, int8_t delta) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_mouse_scroll(app->ble_profile, delta);
    }
}

static void amitablet_right_click_press(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_mouse_press(app->ble_profile, HID_MOUSE_BTN_RIGHT);
    }
}

static void amitablet_right_click_release(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_mouse_release(app->ble_profile, HID_MOUSE_BTN_RIGHT);
    }
}

static void amitablet_consumer_press(AmiTabletApp* app, uint16_t key) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_consumer_key_press(app->ble_profile, key);
    }
}

static void amitablet_consumer_release(AmiTabletApp* app, uint16_t key) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_consumer_key_release(app->ble_profile, key);
    }
}

/* BACK short press: tap the HID keyboard Print Screen key so Windows
   takes a normal full-screen screenshot. Press + release happen together
   here, so no key can ever stick. */
static void amitablet_screenshot(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_kb_press(app->ble_profile, HID_KEYBOARD_PRINT_SCREEN);
        ble_profile_hid_kb_release(app->ble_profile, HID_KEYBOARD_PRINT_SCREEN);
    }
}

static const char* amitablet_input_key_name(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return "UP";
    case InputKeyDown:
        return "DOWN";
    case InputKeyLeft:
        return "LEFT";
    case InputKeyRight:
        return "RIGHT";
    case InputKeyOk:
        return "OK";
    case InputKeyBack:
        return "BACK";
    default:
        return "OTHER";
    }
}

static void amitablet_log_input(AmiTabletApp* app, const InputEvent* event) {
    app->last_input = amitablet_input_key_name(event->key);
    FURI_LOG_I(
        "AmiTablet",
        "INPUT key=%s(%d) type=%d connected=%d",
        app->last_input,
        (int)event->key,
        (int)event->type,
        app->connected ? 1 : 0);
}

static void amitablet_handle_input(AmiTabletApp* app, const InputEvent* event) {
    amitablet_log_input(app, event);

    if(event->key == InputKeyBack) {
        /* Short BACK fires on release and only for short presses: screenshot.
           A long BACK produces Long (no Short), so it exits cleanly without
           taking a screenshot. Nothing is sent on the initial press. */
        if(event->type == InputTypeLong) {
            app->running = false;
        } else if(event->type == InputTypeShort) {
            amitablet_screenshot(app);
        }
        return;
    }

    switch(event->key) {
    case InputKeyUp:
        if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
            amitablet_scroll(app, AMITABLET_SCROLL_STEP);
        }
        break;

    case InputKeyDown:
        if((event->type == InputTypePress) || (event->type == InputTypeRepeat)) {
            amitablet_scroll(app, -AMITABLET_SCROLL_STEP);
        }
        break;

    case InputKeyLeft:
        if(event->type == InputTypePress) {
            amitablet_consumer_press(app, HID_CONSUMER_AC_BACK);
        } else if(event->type == InputTypeRelease) {
            amitablet_consumer_release(app, HID_CONSUMER_AC_BACK);
        }
        break;

    case InputKeyRight:
        if(event->type == InputTypePress) {
            amitablet_consumer_press(app, HID_CONSUMER_AC_FORWARD);
        } else if(event->type == InputTypeRelease) {
            amitablet_consumer_release(app, HID_CONSUMER_AC_FORWARD);
        }
        break;

    case InputKeyOk:
        if(event->type == InputTypePress) {
            amitablet_right_click_press(app);
        } else if(event->type == InputTypeRelease) {
            amitablet_right_click_release(app);
        }
        break;

    default:
        break;
    }
}

int32_t amitablet_app(void* p) {
    UNUSED(p);

    AmiTabletApp* app = malloc(sizeof(AmiTabletApp));
    memset(app, 0, sizeof(AmiTabletApp));

    app->running = true;
    app->bt_event_count = 0;
    app->last_input = "NONE";
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_set_orientation(app->view_port, ViewPortOrientationVerticalFlip);

    view_port_draw_callback_set(app->view_port, amitablet_draw_callback, app);
    view_port_input_callback_set(app->view_port, amitablet_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    amitablet_ble_init(app);
    view_port_update(app->view_port);

    while(app->running) {
        InputEvent event;

        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            amitablet_handle_input(app, &event);
        }
    }

    amitablet_ble_deinit(app);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);

    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}
