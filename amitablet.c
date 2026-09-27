#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
} AmiTabletApp;

static void amitablet_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "AmiTablet");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 25, "UP    Scroll up");
    canvas_draw_str(canvas, 2, 34, "DOWN  Scroll down");
    canvas_draw_str(canvas, 2, 43, "LEFT  Back");
    canvas_draw_str(canvas, 67, 43, "RIGHT Forward");
    canvas_draw_str(canvas, 2, 53, "OK    Right click");
    canvas_draw_str(canvas, 2, 63, "BACK  Exit");
}

static void amitablet_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);

    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, input_event, FuriWaitForever);
}

int32_t amitablet_app(void* p) {
    UNUSED(p);

    AmiTabletApp app = {
        .input_queue = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .view_port = view_port_alloc(),
        .gui = furi_record_open(RECORD_GUI),
    };

    view_port_draw_callback_set(app.view_port, amitablet_draw_callback, NULL);
    view_port_input_callback_set(app.view_port, amitablet_input_callback, app.input_queue);

    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    bool running = true;

    while(running) {
        InputEvent event;

        if(furi_message_queue_get(app.input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if((event.key == InputKeyBack) &&
               ((event.type == InputTypeShort) || (event.type == InputTypeLong))) {
                running = false;
            }

            /*
             * V0.1 UI scaffold only.
             *
             * Next step:
             * - UP/DOWN: Bluetooth HID mouse wheel
             * - LEFT/RIGHT: navigation back/forward
             * - OK: Bluetooth HID right click
             *
             * HID implementation will be based on Momentum's current Bluetooth
             * HID API so we do not guess or depend on outdated symbols.
             */
        }
    }

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}
