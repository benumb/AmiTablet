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
} AmiTabletApp;

static void amitablet_draw_callback(Canvas* canvas, void* context) {
    AmiTabletApp* app = context;

    canvas_clear(canvas);

    /* Exact bitmap asset derived from the same AmiTablet pictogram
       used for the app-list icon. */
    canvas_draw_icon(canvas, 8, 1, &I_amitablet_logo);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 38, "AmiTablet");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 43, 38, "v0.6");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 50, app->connected ? "BLE: OK" : "BLE: WAIT");
    canvas_draw_str(canvas, 4, 62, "UP    Scroll");
    canvas_draw_str(canvas, 4, 72, "DOWN  Scroll");
    canvas_draw_str(canvas, 4, 82, "LEFT  Back");
    canvas_draw_str(canvas, 4, 92, "RIGHT Fwd");
    canvas_draw_str(canvas, 4, 102, "OK    R-click");
    canvas_draw_str(canvas, 4, 118, "Hold BACK");
    canvas_draw_str(canvas, 4, 127, "to exit");
}

static void amitablet_input_callback(InputEvent* input_event, void* context) {
    AmiTabletApp* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static void amitablet_bt_status_callback(BtStatus status, void* context) {
    AmiTabletApp* app = context;
    app->connected = (status == BtStatusConnected);

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

static void amitablet_handle_input(AmiTabletApp* app, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeLong) {
            app->running = false;
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
