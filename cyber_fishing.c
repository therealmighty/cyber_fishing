#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdlib.h>

#define SAVE_PATH EXT_PATH("apps_data/cyber_fishing.save")

typedef enum { 
    StateSplash, StateWaiting, StateFishing, StateBite, StateCaught, 
    StateLost, StateShop, StateSell, StateIndex, 
    StateWorldShop, StatePrestige, StateDevMenu 
} FishingState;

typedef struct {
    FishingState current_state;
    int splash_timer;
    int fish_timer;
    int reaction_timer;
    uint32_t credits;
    uint32_t high_score;
    uint32_t buffer_lvl;
    uint32_t antenna_lvl;
    uint32_t lure_lvl;
    uint32_t inv[7];
    bool discovered[7];
    bool world_unlocked[5];
    int current_world;
    uint32_t core_ver;
    int shop_cursor;
    int dev_cursor;
    int index_cursor;
    int frame_count;
    int last_catch_idx;
    int cheat_step;
    FuriMessageQueue* event_queue;
} CyberFishApp;

const char* pkt_names[] = {"TCP_PKT", "UDP_STRM", "SQL_QRY", "SSL_KEY", "ROOT_HASH", "VOID_DATA", "SYS_GLITCH"};
const char* pkt_desc_a[] = {"Standard", "Fast, but", "Database", "Security", "System", "Data from", "FATAL"};
const char* pkt_desc_b[] = {"Transport", "Unreliable", "Request", "Layer", "Heart", "The Void", "ERROR"};
const uint32_t pkt_prices[] = {5, 15, 30, 75, 200, 1000, 5000};
const char* world_names[] = {"Cyber Dock", "Neon Forest", "Data City", "Void Sector", "Int. Kernel"};
const uint32_t world_costs[] = {0, 200, 600, 1500, 5000};
const char* prestige_icons[] = {"[X]", "[O]", "[#]", "[@]", "[&]", "[%]", "[*]", "[!]", "[?]", "[S]"};
const InputKey cheat_seq[] = {InputKeyUp, InputKeyUp, InputKeyDown, InputKeyDown, InputKeyLeft, InputKeyRight};

const NotificationSequence sequence_bite = {
    &message_vibro_on, &message_note_c6, &message_delay_50, &message_vibro_off, &message_note_e6, &message_delay_50, &message_sound_off, NULL
};

const NotificationSequence sequence_catch = {
    &message_note_c5, &message_delay_100, &message_note_e5, &message_delay_100, &message_note_g5, &message_delay_100, &message_note_c6, &message_delay_100, &message_delay_100, &message_sound_off, NULL
};

const NotificationSequence sequence_fail = {
    &message_vibro_off, &message_note_g4, &message_delay_100, &message_delay_100, &message_note_c4, &message_delay_100, &message_delay_100, &message_delay_100, &message_sound_off, NULL
};

const NotificationSequence sequence_boot = {
    &message_note_e5, &message_delay_50, &message_note_g5, &message_delay_50, &message_note_e6, &message_delay_100, &message_sound_off, NULL
};

void save_game(CyberFishApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->credits, sizeof(uint32_t));
        storage_file_write(file, &app->high_score, sizeof(uint32_t));
        storage_file_write(file, &app->buffer_lvl, sizeof(uint32_t));
        storage_file_write(file, &app->antenna_lvl, sizeof(uint32_t));
        storage_file_write(file, &app->lure_lvl, sizeof(uint32_t));
        storage_file_write(file, &app->current_world, sizeof(int));
        storage_file_write(file, &app->core_ver, sizeof(uint32_t));
        storage_file_write(file, app->inv, sizeof(uint32_t) * 7);
        storage_file_write(file, app->discovered, sizeof(bool) * 7);
        storage_file_write(file, app->world_unlocked, sizeof(bool) * 5);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void reset_game(CyberFishApp* app) {
    app->credits = 0; app->buffer_lvl = 1; app->antenna_lvl = 1; app->lure_lvl = 1; app->current_world = 0;
    for(int i=0; i<7; i++) { app->inv[i] = 0; app->discovered[i] = false; }
    for(int i=0; i<5; i++) app->world_unlocked[i] = (i == 0);
    save_game(app);
}

void load_game(CyberFishApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &app->credits, sizeof(uint32_t));
        storage_file_read(file, &app->high_score, sizeof(uint32_t));
        storage_file_read(file, &app->buffer_lvl, sizeof(uint32_t));
        storage_file_read(file, &app->antenna_lvl, sizeof(uint32_t));
        storage_file_read(file, &app->lure_lvl, sizeof(uint32_t));
        storage_file_read(file, &app->current_world, sizeof(int));
        storage_file_read(file, &app->core_ver, sizeof(uint32_t));
        storage_file_read(file, app->inv, sizeof(uint32_t) * 7);
        storage_file_read(file, app->discovered, sizeof(bool) * 7);
        storage_file_read(file, app->world_unlocked, sizeof(bool) * 5);
    } else { app->core_ver = 1; reset_game(app); }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void draw_logo(Canvas* canvas, int f) {
    canvas_draw_line(canvas, 64, 10, 64, 40);
    canvas_draw_line(canvas, 64, 40, 50, 40);
    canvas_draw_line(canvas, 50, 40, 50, 30);
    canvas_draw_line(canvas, 50, 30, 55, 35);
    canvas_draw_circle(canvas, 64, 10, 2);
    canvas_draw_circle(canvas, 50, 30, 2);
    if(f % 10 < 5) {
        canvas_draw_line(canvas, 68, 15, 75, 15);
        canvas_draw_line(canvas, 45, 35, 40, 35);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, "CYBER_FISH");
}

void draw_world_bg(Canvas* canvas, CyberFishApp* app) {
    int f = app->frame_count;
    if(app->current_world == 0) {
        canvas_draw_line(canvas, 0, 45, 55, 45);
        canvas_draw_line(canvas, 10, 45, 10, 60);
        canvas_draw_line(canvas, 40, 45, 40, 60);
        for(int i=0; i<4; i++) {
            int x = (f * 2 + (i * 30)) % 128;
            canvas_draw_dot(canvas, x, 52 + (i%3));
        }
    } else if(app->current_world == 1) {
        for(int i=0; i<60; i+=15) {
            canvas_draw_line(canvas, i, 45, i+5, 20);
            canvas_draw_line(canvas, i+5, 20, i+10, 45);
        }
        for(int i=0; i<10; i++) {
            int rx = (i * 21) % 128;
            int ry = (f + (i * 7)) % 45;
            canvas_draw_dot(canvas, rx, ry);
        }
        canvas_draw_line(canvas, 0, 45, 55, 45);
    } else if(app->current_world == 2) {
        for(int i=0; i<128; i+=16) {
            int x_off = (f % 16);
            canvas_draw_line(canvas, i - x_off, 45, (i - x_off) - 10, 64);
        }
        canvas_draw_frame(canvas, 5, 20, 10, 25);
        canvas_draw_frame(canvas, 25, 10, 15, 35);
        canvas_draw_line(canvas, 0, 45, 55, 45);
    } else if(app->current_world == 3) {
        int pulse = (f % 20) / 2;
        canvas_draw_circle(canvas, 100, 20, pulse);
        for(int i=0; i<15; i++) {
            canvas_draw_dot(canvas, (i*17)%128, (i*9)%45);
        }
        canvas_draw_line(canvas, 0, 45, 55, 45);
    } else if(app->current_world == 4) {
        int shake = (f % 2 == 0) ? 1 : -1;
        for(int i=0; i<40; i+=8) {
            canvas_draw_str(canvas, (i*3)%60 + shake, (i + f)%45, (i%2==0)?"0":"1");
        }
        canvas_draw_line(canvas, 0, 45, 55, 45);
    }
}

void render_callback(Canvas* canvas, void* ctx) {
    CyberFishApp* app = ctx;
    if(app->current_state == StateSplash) {
        canvas_clear(canvas);
        draw_logo(canvas, app->frame_count);
        return;
    }
    bool invert = (app->current_world == 4 && app->current_state == StateBite && (app->frame_count % 4 < 2));
    if(invert) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_clear(canvas);
    }
    char buf[64];
    if(app->current_state == StateDevMenu) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "ADMIN_TERMINAL.sh");
        canvas_set_font(canvas, FontSecondary);
        const char* options[] = {"Add 1000 Credits", "Unlock All Worlds", "Max Everything", "Wipe All Progress"};
        for(int i=0; i<4; i++) canvas_draw_str(canvas, 12, 25 + (i*9), options[i]);
        canvas_draw_str(canvas, 2, 25 + (app->dev_cursor * 9), ">");
    } else if(app->current_state == StateIndex) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 5, 12, "NET_INDEX");
        canvas_set_font(canvas, FontSecondary);
        for(int i=0; i<7; i++) {
            snprintf(buf, sizeof(buf), "%s", app->discovered[i] ? pkt_names[i] : "???");
            canvas_draw_str(canvas, 12, 22 + (i*6), buf);
        }
        canvas_draw_line(canvas, 65, 15, 65, 50); 
        if(app->discovered[app->index_cursor]) {
            canvas_draw_str(canvas, 70, 30, pkt_desc_a[app->index_cursor]);
            canvas_draw_str(canvas, 70, 40, pkt_desc_b[app->index_cursor]);
        } else {
            canvas_draw_str(canvas, 70, 35, "LOCKED");
        }
        canvas_draw_line(canvas, 5, 52, 120, 52);
        canvas_draw_str(canvas, 5, 62, "CORES:");
        for(uint32_t i=0; i < (app->core_ver-1) && i < 10; i++) {
            canvas_draw_str(canvas, 40 + (i*8), 62, prestige_icons[i]);
        }
    } else if(app->current_state == StatePrestige) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 5, 12, "SYSTEM REFORMAT");
        canvas_set_font(canvas, FontSecondary);
        bool all_found = true;
        for(int i=0; i<7; i++) if(!app->discovered[i]) all_found = false;
        if(!all_found) {
            canvas_draw_str(canvas, 10, 30, "Index Incomplete...");
            canvas_draw_str(canvas, 10, 40, "Scan all packets first.");
        } else {
            snprintf(buf, sizeof(buf), "Current: v%lu", app->core_ver);
            canvas_draw_str(canvas, 10, 25, buf);
            canvas_draw_str(canvas, 10, 35, "REFORMAT? [OK]");
            canvas_draw_str(canvas, 10, 45, "(Reset for 25% Bonus)");
        }
    } else if(app->current_state == StateShop || app->current_state == StateWorldShop || app->current_state == StateSell) {
        canvas_set_font(canvas, FontPrimary);
        if(app->current_state == StateShop) canvas_draw_str(canvas, 5, 12, "HARDWARE");
        else if(app->current_state == StateWorldShop) canvas_draw_str(canvas, 5, 12, "TRAVEL");
        else canvas_draw_str(canvas, 5, 12, "MARKET");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Cr: %lu", app->credits);
        canvas_draw_str(canvas, 80, 12, buf);
        if(app->current_state == StateShop) {
            snprintf(buf, sizeof(buf), "Buff L%lu (50c)", app->buffer_lvl);
            canvas_draw_str(canvas, 10, 25, buf);
            snprintf(buf, sizeof(buf), "Ant  L%lu (80c)", app->antenna_lvl);
            canvas_draw_str(canvas, 10, 33, buf);
            snprintf(buf, sizeof(buf), "Lure L%lu (60c)", app->lure_lvl);
            canvas_draw_str(canvas, 10, 41, buf);
            canvas_draw_str(canvas, 2, 25 + (app->shop_cursor * 8), ">");
        } else if(app->current_state == StateWorldShop) {
            for(int i=0; i<5; i++) {
                snprintf(buf, sizeof(buf), "%s %s", world_names[i], app->world_unlocked[i] ? "[OK]" : "[LOK]");
                canvas_draw_str(canvas, 10, 25 + (i*7), buf);
            }
            canvas_draw_str(canvas, 2, 25 + (app->shop_cursor * 7), ">");
        } else {
            snprintf(buf, sizeof(buf), "Sell %s: %luc", pkt_names[app->shop_cursor], (uint32_t)(pkt_prices[app->shop_cursor] * (1.0 + (app->core_ver-1)*0.25)));
            canvas_draw_str(canvas, 10, 30, buf);
            snprintf(buf, sizeof(buf), "Stock: %lu", app->inv[app->shop_cursor]);
            canvas_draw_str(canvas, 10, 40, buf);
            canvas_draw_str(canvas, 2, 30, ">");
        }
    } else {
        draw_world_bg(canvas, app);
        canvas_draw_circle(canvas, 45, 35, 3);
        canvas_draw_line(canvas, 45, 38, 45, 43);
        int rod_y = (app->current_state == StateBite && (app->frame_count % 2)) ? 22 : 25;
        canvas_draw_line(canvas, 45, 38, 70, rod_y);
        canvas_draw_line(canvas, 70, rod_y, 70, 50);
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "v%lu | C:%lu | %s", app->core_ver, app->credits, world_names[app->current_world]);
        canvas_draw_str(canvas, 2, 10, buf);
        if(app->current_state == StateWaiting) {
            canvas_draw_str(canvas, 60, 22, "UP:Shop DN:Sell");
            canvas_draw_str(canvas, 60, 32, "LT:Index OK:Go");
        } else if(app->current_state == StateCaught) {
            snprintf(buf, sizeof(buf), "+ %s", pkt_names[app->last_catch_idx]);
            canvas_draw_str(canvas, 60, 30, buf);
        } else if(app->current_state == StateLost) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 60, 30, "LOST PKT!");
        } else if(app->current_state == StateBite) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 75, 30, "BITE!");
        }
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t cyber_fishing_app(void* p) {
    UNUSED(p);
    CyberFishApp* app = malloc(sizeof(CyberFishApp));
    app->current_state = StateSplash;
    app->splash_timer = 20; 
    app->cheat_step = 0;
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    load_game(app);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, input_callback, app->event_queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_boot);
    InputEvent event;
    int vib_rarity = 0;
    bool running = true;
    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            if(app->current_state == StateWaiting && (event.type == InputTypeShort || event.type == InputTypeLong)) {
                if(event.key == cheat_seq[app->cheat_step]) {
                    app->cheat_step++;
                    if(app->cheat_step == 6) {
                        app->current_state = StateDevMenu;
                        app->dev_cursor = 0;
                        app->cheat_step = 0;
                        notification_message(notifications, &sequence_success);
                    }
                } else if(event.key != InputKeyOk) {
                    app->cheat_step = 0;
                }
            }
            if(event.type == InputTypeShort) {
                if(event.key == InputKeyBack) {
                    if(app->current_state == StateWaiting || app->current_state == StateSplash) {
                        running = false;
                    } else {
                        app->current_state = StateWaiting;
                    }
                } else if(app->current_state == StateDevMenu) {
                    if(event.key == InputKeyDown) app->dev_cursor = (app->dev_cursor + 1) % 4;
                    else if(event.key == InputKeyUp) app->dev_cursor = (app->dev_cursor - 1 + 4) % 4;
                    else if(event.key == InputKeyOk) {
                        if(app->dev_cursor == 0) app->credits += 1000;
                        else if(app->dev_cursor == 1) { for(int i=0; i<5; i++) app->world_unlocked[i] = true; }
                        else if(app->dev_cursor == 2) {
                            app->credits = 50000; app->buffer_lvl = 8; app->antenna_lvl = 8; app->lure_lvl = 8;
                            for(int i=0; i<5; i++) app->world_unlocked[i] = true;
                            for(int i=0; i<7; i++) app->discovered[i] = true;
                        } else if(app->dev_cursor == 3) {
                            app->core_ver = 1; reset_game(app);
                        }
                        furi_delay_ms(100);
                        save_game(app);
                        notification_message(notifications, &sequence_blink_green_100);
                    }
                } else if(app->current_state == StateIndex) {
                    if(event.key == InputKeyDown) app->index_cursor = (app->index_cursor + 1) % 7;
                    else if(event.key == InputKeyUp) app->index_cursor = (app->index_cursor - 1 + 7) % 7;
                } else if(app->current_state == StatePrestige) {
                    bool all_found = true;
                    for(int i=0; i<7; i++) if(!app->discovered[i]) all_found = false;
                    if(all_found && event.key == InputKeyOk) {
                        app->core_ver++;
                        reset_game(app);
                        app->current_state = StateWaiting;
                        notification_message(notifications, &sequence_success);
                    }
                } else if(app->current_state == StateShop) {
                    if(event.key == InputKeyRight) {
                        app->current_state = StateWorldShop;
                        app->shop_cursor = 0;
                    } else if(event.key == InputKeyLeft) {
                        app->current_state = StatePrestige;
                    } else if(event.key == InputKeyDown) app->shop_cursor = (app->shop_cursor + 1) % 3;
                    else if(event.key == InputKeyUp) app->shop_cursor = (app->shop_cursor - 1 + 3) % 3;
                    else if(event.key == InputKeyOk) {
                        if(app->shop_cursor == 0 && app->credits >= 50) { app->credits -= 50; app->buffer_lvl++; }
                        else if(app->shop_cursor == 1 && app->credits >= 80) { app->credits -= 80; app->antenna_lvl++; }
                        else if(app->shop_cursor == 2 && app->credits >= 60) { app->credits -= 60; app->lure_lvl++; }
                        save_game(app);
                    }
                } else if(app->current_state == StateWorldShop) {
                    if(event.key == InputKeyLeft) {
                        app->current_state = StateShop;
                        app->shop_cursor = 0;
                    } else if(event.key == InputKeyDown) app->shop_cursor = (app->shop_cursor + 1) % 5;
                    else if(event.key == InputKeyUp) app->shop_cursor = (app->shop_cursor - 1 + 5) % 5;
                    else if(event.key == InputKeyOk) {
                        if(!app->world_unlocked[app->shop_cursor] && app->credits >= world_costs[app->shop_cursor]) {
                            app->credits -= world_costs[app->shop_cursor];
                            app->world_unlocked[app->shop_cursor] = true;
                        } else if(app->world_unlocked[app->shop_cursor]) {
                            app->current_world = app->shop_cursor;
                            app->current_state = StateWaiting;
                        }
                        save_game(app);
                    }
                } else if(app->current_state == StateSell) {
                    if(event.key == InputKeyDown) app->shop_cursor = (app->shop_cursor + 1) % 7;
                    else if(event.key == InputKeyUp) app->shop_cursor = (app->shop_cursor - 1 + 7) % 7;
                    else if(event.key == InputKeyOk && app->inv[app->shop_cursor] > 0) {
                        app->inv[app->shop_cursor]--;
                        app->credits += (uint32_t)(pkt_prices[app->shop_cursor] * (1.0 + (app->core_ver-1)*0.25));
                        save_game(app);
                    }
                } else if(app->current_state == StateWaiting) {
                    if(event.key == InputKeyUp) { app->current_state = StateShop; app->shop_cursor = 0; }
                    else if(event.key == InputKeyDown) { app->current_state = StateSell; app->shop_cursor = 0; }
                    else if(event.key == InputKeyLeft) { app->current_state = StateIndex; app->index_cursor = 0; }
                    else if(event.key == InputKeyOk) {
                        app->current_state = StateFishing;
                        app->fish_timer = (rand() % 40) + 10 - (app->lure_lvl * 2) - (app->current_world * 3);
                    }
                } else if(app->current_state == StateBite && event.key == InputKeyOk) {
                    app->inv[vib_rarity]++;
                    app->discovered[vib_rarity] = true;
                    app->last_catch_idx = vib_rarity;
                    app->current_state = StateCaught;
                    notification_message(notifications, &sequence_catch);
                    save_game(app);
                } else if((app->current_state == StateCaught || app->current_state == StateLost) && event.key == InputKeyOk) {
                    app->current_state = StateWaiting;
                }
            }
        }
        if(app->current_state == StateSplash) {
            app->splash_timer--;
            if(app->splash_timer <= 0) app->current_state = StateWaiting;
        } else if(app->current_state == StateFishing) {
            app->fish_timer--;
            if(app->fish_timer <= 0) {
                app->current_state = StateBite;
                notification_message(notifications, &sequence_bite);
                app->reaction_timer = 20 + (app->buffer_lvl * 5) - (app->current_world * 3);
                int r = rand() % 100 + (app->antenna_lvl * 4) + (app->current_world * 20);
                vib_rarity = (r > 180) ? 6 : (r > 150) ? 5 : (r > 120) ? 4 : (r > 90) ? 3 : (r > 70) ? 2 : (r > 40) ? 1 : 0;
            }
        } else if(app->current_state == StateBite) {
            app->reaction_timer--;
            if(app->reaction_timer <= 0) {
                app->current_state = StateLost;
                notification_message(notifications, &sequence_fail);
            }
        }
        app->frame_count++;
        view_port_update(view_port);
    }
    notification_message(notifications, &sequence_reset_vibro);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
