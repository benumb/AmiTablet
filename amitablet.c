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

#define AMITABLET_VERSION_LABEL "v1.1"

/* ------------------------------------------------------------------ */
/* V1.1 portrait UI (64x128 with ViewPortOrientationVerticalFlip).     */
/* Pixel-faithful reproduction of image.png: every text line, the      */
/* control circle and the central diamond are bitmaps extracted from   */
/* the 64x128 reference; frames use standard canvas primitives.        */
/* Composed lines ("v1.1", "BT-WAIT") reuse the reference glyphs.      */
/* Layout (portrait space):                                            */
/*   y  0,34   blank margins                                           */
/*   y  1..33  header frame: AMITABLET, version, BT badge              */
/*   y 35..114 black main block with white control circle              */
/*   y115,116  blank gap                                               */
/*   y117..126 footer frame: HOLD BACK TO EXIT                         */
/*   y127      blank margin                                            */
/* ------------------------------------------------------------------ */

/* Blits 1-bit data with the current canvas color. */
static void amitablet_blt(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint8_t w,
    uint8_t h,
    const uint8_t* data) {
    uint8_t nb = (uint8_t)((w + 7) / 8);
    for(uint8_t r = 0; r < h; r++) {
        for(uint8_t c = 0; c < w; c++) {
            if(data[r * nb + c / 8] & (0x80 >> (c % 8))) {
                canvas_draw_box(canvas, x + c, y + r, 1, 1);
            }
        }
    }
}

static const uint8_t amitablet_bmp_title[] = {0x73, 0xE5, 0xF3, 0xCE, 0x41, 0xE7, 0xC0, 0x8A, 0xA4, 0x42, 0x4A, 0x41, 0x01, 0x00, 0x8A, 0xA4, 0x42, 0x4A, 0x41, 0x01, 0x00, 0xFA, 0xA4, 0x43, 0xCF, 0x41, 0xC1, 0x00, 0x8A, 0x24, 0x42, 0x49, 0x41, 0x01, 0x00, 0x8A, 0x24, 0x42, 0x4F, 0x79, 0xE1, 0x00}; /* AMITABLET 50x6 at (7,6) */
static const uint8_t amitablet_bmp_v11[] = {0xA4, 0x20, 0xA4, 0x20, 0xA4, 0x20, 0x45, 0x20}; /* v1.1 12x4 at (50,14) */
static const uint8_t amitablet_bmp_btok[] = {0xF0, 0x06, 0x50, 0x97, 0x09, 0x50, 0x92, 0x09, 0x60, 0xFA, 0x69, 0x50, 0x8A, 0x09, 0x50, 0xFA, 0x06, 0x50}; /* BT-OK 20x6 at (22,22) */
static const uint8_t amitablet_bmp_btwait[] = {0xF0, 0x08, 0x9C, 0x80, 0x97, 0x08, 0xA2, 0xB8, 0x92, 0x08, 0xA2, 0x90, 0xFA, 0x6A, 0xBE, 0x90, 0x8A, 0x0A, 0xA2, 0x90, 0xFA, 0x05, 0x22, 0x90}; /* BT-WAIT 30x6 at (17,22) */
static const uint8_t amitablet_bmp_up[] = {0x97, 0x00, 0x94, 0x80, 0x97, 0x00, 0x94, 0x00, 0x64, 0x00}; /* UP 9x5 at (28,51) */
static const uint8_t amitablet_bmp_sc1[] = {0x63, 0x63, 0x24, 0x84, 0x54, 0xA4, 0x64, 0x64, 0xA4, 0x14, 0x54, 0xA4, 0x63, 0x53, 0x36}; /* SCROLL 23x5 at (20,58) */
static const uint8_t amitablet_bmp_ctrlL[] = {0x6E, 0x98, 0x84, 0x94, 0x84, 0x98, 0x84, 0x94, 0x64, 0xD4}; /* CTRL 14x5 at (6,68) */
static const uint8_t amitablet_bmp_ctrlR[] = {0x6E, 0x98, 0x84, 0x94, 0x84, 0x98, 0x84, 0x94, 0x64, 0xD4}; /* CTRL 14x5 at (44,68) */
static const uint8_t amitablet_bmp_plusL[] = {0x40, 0xE0, 0x40}; /* + 3x3 at (11,75) */
static const uint8_t amitablet_bmp_plusR[] = {0x40, 0xE0, 0x40}; /* + 3x3 at (50,75) */
static const uint8_t amitablet_bmp_c[] = {0x60, 0x80, 0x80, 0x80, 0x60}; /* C 3x5 at (11,80) */
static const uint8_t amitablet_bmp_v[] = {0xA0, 0xA0, 0xA0, 0xA0, 0x40}; /* V 3x5 at (50,80) */
static const uint8_t amitablet_bmp_sc2[] = {0x63, 0x63, 0x24, 0x84, 0x54, 0xA4, 0x64, 0x64, 0xA4, 0x14, 0x54, 0xA4, 0x63, 0x53, 0x36}; /* SCROLL 23x5 at (20,88) */
static const uint8_t amitablet_bmp_down[] = {0xC6, 0x45, 0x20, 0xA9, 0x45, 0xA0, 0xA9, 0x55, 0x60, 0xA9, 0x55, 0x20, 0xC6, 0x29, 0x20}; /* DOWN 19x5 at (22,95) */
static const uint8_t amitablet_bmp_foot[] = {0xA6, 0x4C, 0x62, 0x35, 0x39, 0x8D, 0x5D, 0xD0, 0xA9, 0x4A, 0x55, 0x45, 0x12, 0x49, 0x48, 0x90, 0xE9, 0x4A, 0x65, 0x46, 0x12, 0x4C, 0x88, 0x90, 0xA9, 0x4A, 0x57, 0x45, 0x12, 0x49, 0x48, 0x90, 0xA6, 0x6C, 0x65, 0x35, 0x11, 0x8D, 0x5C, 0x90}; /* HOLD BACK TO EXIT 60x5 at (3,119) */

/* White control circle silhouette: horizontal spans for rows 46..104. */
static const uint8_t amitablet_circle[][2] = {
    {26,37}, {22,41}, {20,43}, {18,45}, {16,47}, {15,48}, {13,50}, {12,51}, {11,52}, {10,53}, {9,54}, {9,54}, {8,55}, {7,56}, {7,56}, {6,57}, {6,57}, {5,58}, {5,58}, {4,59}, {4,59}, {4,59}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {3,60}, {4,59}, {4,59}, {4,59}, {5,58}, {5,58}, {6,57}, {6,57}, {7,56}, {7,56}, {8,55}, {9,54}, {9,54}, {10,53}, {11,52}, {12,51}, {13,50}, {15,48}, {16,47}, {18,45}, {20,43}, {22,41}};

/* Central diamond + ticks: black runs (y, x0, x1). */
static const uint8_t amitablet_diamond[][3] = {
    {66,31,32}, {67,30,33}, {69,30,33}, {70,29,34}, {71,28,35}, {72,27,36}, {73,27,36}, {74,24,24}, {74,26,37}, {74,39,39}, {75,24,24}, {75,26,37}, {75,39,39}, {76,27,36}, {77,27,36}, {78,28,35}, {79,29,34}, {80,30,33}, {82,30,33}, {83,31,32}};

/* Old V1.0 pictograms removed: V1.1 draws mockup-extracted bitmaps. */

static void amitablet_draw_callback(Canvas* canvas, void* context) {
    AmiTabletApp* app = context;
    size_t i;

    canvas_clear(canvas);

    /* Header frame. */
    canvas_draw_rframe(canvas, 1, 1, 62, 33, 2);
    canvas_draw_rframe(canvas, 2, 2, 60, 31, 1);

    /* Title + version (mockup bitmaps). */
    amitablet_blt(canvas, 7, 6, 50, 6, amitablet_bmp_title);
    amitablet_blt(canvas, 50, 14, 12, 4, amitablet_bmp_v11);

    /* BLE badge: filled capsule, white state text. */
    canvas_draw_rbox(canvas, 15, 21, 34, 8, 3);
    canvas_set_color(canvas, ColorWhite);
    if(app->connected) {
        amitablet_blt(canvas, 22, 22, 20, 6, amitablet_bmp_btok);
    } else {
        amitablet_blt(canvas, 17, 22, 30, 6, amitablet_bmp_btwait);
    }
    canvas_set_color(canvas, ColorBlack);

    /* Black main block. */
    canvas_draw_box(canvas, 1, 36, 62, 78);
    canvas_draw_line(canvas, 2, 35, 61, 35);
    canvas_draw_line(canvas, 2, 114, 61, 114);

    /* White control circle. */
    canvas_set_color(canvas, ColorWhite);
    for(i = 0; i < sizeof(amitablet_circle) / sizeof(amitablet_circle[0]); i++) {
        canvas_draw_line(
            canvas, amitablet_circle[i][0], (int32_t)(46 + i), amitablet_circle[i][1], (int32_t)(46 + i));
    }
    canvas_set_color(canvas, ColorBlack);

    /* Circle labels (mockup bitmaps, black on white). */
    amitablet_blt(canvas, 28, 51, 9, 5, amitablet_bmp_up);
    amitablet_blt(canvas, 20, 58, 23, 5, amitablet_bmp_sc1);
    amitablet_blt(canvas, 6, 68, 14, 5, amitablet_bmp_ctrlL);
    amitablet_blt(canvas, 44, 68, 14, 5, amitablet_bmp_ctrlR);
    amitablet_blt(canvas, 11, 75, 3, 3, amitablet_bmp_plusL);
    amitablet_blt(canvas, 50, 75, 3, 3, amitablet_bmp_plusR);
    amitablet_blt(canvas, 11, 80, 3, 5, amitablet_bmp_c);
    amitablet_blt(canvas, 50, 80, 3, 5, amitablet_bmp_v);
    amitablet_blt(canvas, 20, 88, 23, 5, amitablet_bmp_sc2);
    amitablet_blt(canvas, 22, 95, 19, 5, amitablet_bmp_down);

    /* Central diamond + ticks (right click). */
    for(i = 0; i < sizeof(amitablet_diamond) / sizeof(amitablet_diamond[0]); i++) {
        canvas_draw_line(
            canvas,
            amitablet_diamond[i][1],
            amitablet_diamond[i][0],
            amitablet_diamond[i][2],
            amitablet_diamond[i][0]);
    }

    /* Footer frame + exit hint. */
    canvas_draw_rframe(canvas, 1, 117, 62, 9, 1);
    canvas_draw_line(canvas, 3, 126, 60, 126);
    amitablet_blt(canvas, 3, 119, 60, 5, amitablet_bmp_foot);

    /* V0.9 diagnostic state is intentionally not drawn over the final UI.
       It is logged to the console to preserve the V1.1 design. */
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
        ble_profile_hid_kb_release_all(app->ble_profile);
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

/* LEFT: Ctrl+C (copy). The HID profile packs modifiers in the high byte,
   so a single press call sets Ctrl+C together; release C, then Ctrl. */
static void amitablet_copy_press(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_kb_press(
            app->ble_profile, (uint16_t)(KEY_MOD_LEFT_CTRL | HID_KEYBOARD_C));
    }
}

static void amitablet_copy_release(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_kb_release(app->ble_profile, HID_KEYBOARD_C);
        ble_profile_hid_kb_release(app->ble_profile, KEY_MOD_LEFT_CTRL);
    }
}

/* RIGHT: Ctrl+V (paste). Same press/release sequence as copy. */
static void amitablet_paste_press(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_kb_press(
            app->ble_profile, (uint16_t)(KEY_MOD_LEFT_CTRL | HID_KEYBOARD_V));
    }
}

static void amitablet_paste_release(AmiTabletApp* app) {
    if(app->ble_profile && app->connected) {
        ble_profile_hid_kb_release(app->ble_profile, HID_KEYBOARD_V);
        ble_profile_hid_kb_release(app->ble_profile, KEY_MOD_LEFT_CTRL);
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
            amitablet_copy_press(app);
        } else if(event->type == InputTypeRelease) {
            amitablet_copy_release(app);
        }
        break;

    case InputKeyRight:
        if(event->type == InputTypePress) {
            amitablet_paste_press(app);
        } else if(event->type == InputTypeRelease) {
            amitablet_paste_release(app);
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
