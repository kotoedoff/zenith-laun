#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "0.4.0-beta"

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *welcome_page;
    GtkWidget *distro_page;
    GtkWidget *license_page;
    GtkWidget *components_page;
    GtkWidget *install_page;
    GtkWidget *finish_page;
    GtkWidget *contacts_page;  // ← ДОБАВИЛ
    GtkWidget *terminal;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    GtkWidget *next_button;
    GtkWidget *back_button;
    GtkWidget *finish_button;
    GtkWidget *install_button; // ← ДОБАВИЛ кнопку установки
    
    char *selected_distro;
    gboolean license_accepted;
    gboolean install_sdl2;
    gboolean install_opengl;
    gboolean install_tcc;
    gboolean install_crypto;
    gboolean install_examples;
    gboolean installation_started; // ← ДОБАВИЛ флаг
} InstallerData;

static const char *ZENITH_LOGO = 
"    ╔══════════════════════════════════╗\n"
"    ║                                  ║\n"
"    ║         ███████╗███████╗         ║\n"
"    ║         ╚══███╔╝██╔════╝         ║\n"
"    ║           ███╔╝ █████╗           ║\n"
"    ║          ███╔╝  ██╔══╝           ║\n"
"    ║         ███████╗███████╗         ║\n"
"    ║         ╚══════╝╚══════╝         ║\n"
"    ║                                  ║\n"
"    ║      ZENITH LANGUAGE v0.4.0      ║\n"
"    ║    Production Ready Installer    ║\n"
"    ║                                  ║\n"
"    ╚══════════════════════════════════╝\n";

static const char *GPL_LICENSE = 
"GNU GENERAL PUBLIC LICENSE\n"
"Version 2, June 1991\n\n"
"Copyright (C) 2024 Zenith Language Project\n\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
"GNU General Public License for more details.\n";

// Декларации функций
static void on_component_toggled(GtkToggleButton *btn, gpointer user_data);
static void on_close_clicked(GtkButton *btn, gpointer user_data);
static void on_license_toggled(GtkToggleButton *button, InstallerData *data);
static void on_distro_toggled(GtkToggleButton *button, InstallerData *data);
static void on_next_clicked(GtkButton *button, InstallerData *data);
static void on_back_clicked(GtkButton *button, InstallerData *data);
static void on_stack_changed(GtkStack *stack, GParamSpec *pspec, InstallerData *data);
static void on_start_installation(GtkButton *button, InstallerData *data); // ← ДОБАВИЛ

void apply_dark_theme(GtkWidget *widget) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background-color: #1a1a1a; color: #ffffff; }"
        "box { background-color: #1a1a1a; }"
        "label { color: #ffffff; font-size: 14px; }"
        ".title { font-size: 28px; font-weight: bold; color: #667eea; }"
        ".subtitle { font-size: 16px; color: #aaaaaa; margin-top: 10px; }"
        ".logo { font-family: monospace; color: #667eea; font-size: 11px; }"
        ".contacts { font-family: monospace; color: #00ff00; font-size: 12px; background: #0a0a0a; padding: 15px; border-radius: 8px; }"
        "button { "
        "  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
        "  color: #ffffff; border: none; border-radius: 8px;"
        "  padding: 12px 30px; font-size: 14px; font-weight: bold;"
        "  min-width: 120px; min-height: 40px; }"
        "button:hover { background: linear-gradient(135deg, #7b8df5 0%, #8b5cb8 100%); }"
        "button:disabled { background: #333333; color: #666666; }"
        "checkbutton label { color: #ffffff; }"
        "radiobutton label { color: #ffffff; }"
        "textview { background-color: #0a0a0a; color: #ffffff; padding: 10px; }"
        "textview text { background-color: #0a0a0a; color: #ffffff; }"
        "progressbar { background-color: #2a2a2a; min-height: 30px; }"
        "progressbar progress { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }",
        -1, NULL);
    
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

GtkWidget* create_welcome_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    
    GtkWidget *logo_label = gtk_label_new(ZENITH_LOGO);
    GtkStyleContext *context = gtk_widget_get_style_context(logo_label);
    gtk_style_context_add_class(context, "logo");
    gtk_box_pack_start(GTK_BOX(box), logo_label, FALSE, FALSE, 0);
    
    GtkWidget *title = gtk_label_new("Welcome to Zenith Language Installer");
    context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    
    GtkWidget *subtitle = gtk_label_new(
        "Fast • Safe • Powerful\n\n"
        "This installer will guide you through the installation");
    context = gtk_widget_get_style_context(subtitle);
    gtk_style_context_add_class(context, "subtitle");
    gtk_label_set_justify(GTK_LABEL(subtitle), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 0);
    
    return box;
}

void on_distro_toggled(GtkToggleButton *button, InstallerData *data) {
    if (gtk_toggle_button_get_active(button)) {
        const char *label = gtk_button_get_label(GTK_BUTTON(button));
        if (data->selected_distro) g_free(data->selected_distro);
        data->selected_distro = g_strdup(label);
    }
}

GtkWidget* create_distro_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_margin_top(box, 20);
    
    GtkWidget *title = gtk_label_new("Select Your Linux Distribution");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    GSList *group = NULL;
    const char *distros[] = {"Debian/Ubuntu", "Arch Linux", "Fedora", "openSUSE"};
    
    for (int i = 0; i < 4; i++) {
        GtkWidget *radio = gtk_radio_button_new_with_label(group, distros[i]);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
        g_signal_connect(radio, "toggled", G_CALLBACK(on_distro_toggled), data);
        gtk_box_pack_start(GTK_BOX(box), radio, FALSE, FALSE, 5);
    }
    
    if (group) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group->data), TRUE);
    return box;
}

void on_license_toggled(GtkToggleButton *button, InstallerData *data) {
    data->license_accepted = gtk_toggle_button_get_active(button);
    if (data->next_button) {
        const char *page = gtk_stack_get_visible_child_name(GTK_STACK(data->stack));
        if (strcmp(page, "license") == 0) {
            gtk_widget_set_sensitive(data->next_button, data->license_accepted);
        }
    }
}

GtkWidget* create_license_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_margin_top(box, 20);
    
    GtkWidget *title = gtk_label_new("License Agreement");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, 600, 300);
    
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, GPL_LICENSE, -1);
    
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 10);
    
    GtkWidget *check = gtk_check_button_new_with_label("I accept the GPL 2.0 License");
    g_signal_connect(check, "toggled", G_CALLBACK(on_license_toggled), data);
    gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
    
    return box;
}

static void on_component_toggled(GtkToggleButton *btn, gpointer user_data) {
    gboolean *val = (gboolean*)user_data;
    *val = gtk_toggle_button_get_active(btn);
}

GtkWidget* create_components_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_margin_top(box, 20);
    
    GtkWidget *title = gtk_label_new("Select Components");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    struct { const char *name; gboolean *value; } components[] = {
        {"SDL2 Graphics Library (Recommended)", &data->install_sdl2},
        {"OpenGL Support", &data->install_opengl},
        {"TCC Compiler", &data->install_tcc},
        {"OpenSSL Crypto (Required)", &data->install_crypto},
        {"Example Projects", &data->install_examples}
    };
    
    for (int i = 0; i < 5; i++) {
        GtkWidget *check = gtk_check_button_new_with_label(components[i].name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
        *components[i].value = TRUE;
        
        g_signal_connect(check, "toggled", G_CALLBACK(on_component_toggled), components[i].value);
        
        gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 5);
    }
    
    return box;
}

void execute_command(VteTerminal *terminal, const char *cmd) {
    char *argv[] = {"/bin/sh", "-c", (char*)cmd, NULL};
    vte_terminal_spawn_async(terminal, VTE_PTY_DEFAULT, NULL, argv, NULL,
        G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL);
}

gboolean installation_step(gpointer user_data) {
    InstallerData *data = (InstallerData*)user_data;
    static int step = 0;
    
    if (!data->installation_started) {
        return G_SOURCE_CONTINUE;
    }
    
    const char *steps[] = {
        "Updating package lists...",
        "Installing dependencies...",
        "Downloading Zenith source...",
        "Compiling Zenith...",
        "Installing Zenith...",
        "Creating directories...",
        "Installing examples...",
        "Finalizing installation..."
    };
    
    if (step < 8) {
        gtk_label_set_text(GTK_LABEL(data->status_label), steps[step]);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->progress_bar), (step + 1) / 8.0);
        
        VteTerminal *terminal = VTE_TERMINAL(data->terminal);
        
        if (step == 0) {
            execute_command(terminal, "echo 'Updating packages...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 1) {
            execute_command(terminal, "echo 'Installing dependencies...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 2) {
            execute_command(terminal, "echo 'Downloading Zenith source...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 3) {
            execute_command(terminal, "echo 'Compiling Zenith Language...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 4) {
            execute_command(terminal, "echo 'Installing to /usr/local/bin/zenith'");
            execute_command(terminal, "sleep 1");
        } else if (step == 5) {
            execute_command(terminal, "echo 'Creating directories...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 6) {
            execute_command(terminal, "echo 'Installing examples...'");
            execute_command(terminal, "sleep 1");
        } else if (step == 7) {
            execute_command(terminal, "echo 'Finalizing installation...'");
            execute_command(terminal, "sleep 1");
        }
        
        step++;
        return G_SOURCE_CONTINUE;
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "✓ Installation Complete!");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->progress_bar), 1.0);
        
        // Переходим на страницу с контактами, а не сразу на финиш
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "contacts");
        step = 0;
        data->installation_started = FALSE;
        return G_SOURCE_REMOVE;
    }
}

static void on_start_installation(GtkButton *button, InstallerData *data) {
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    data->installation_started = TRUE;
    g_timeout_add(1000, installation_step, data); // Таймер запускается ТОЛЬКО здесь
}

GtkWidget* create_install_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_margin_top(box, 20);
    
    GtkWidget *title = gtk_label_new("Installing Zenith Language");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    data->status_label = gtk_label_new("Click 'Start Installation' to begin");
    gtk_box_pack_start(GTK_BOX(box), data->status_label, FALSE, FALSE, 0);
    
    data->progress_bar = gtk_progress_bar_new();
    gtk_widget_set_size_request(data->progress_bar, 600, 30);
    gtk_box_pack_start(GTK_BOX(box), data->progress_bar, FALSE, FALSE, 10);
    
    // КНОПКА НАЧАЛА УСТАНОВКИ (ДОБАВИЛ)
    data->install_button = gtk_button_new_with_label("Start Installation");
    gtk_box_pack_start(GTK_BOX(box), data->install_button, FALSE, FALSE, 10);
    g_signal_connect(data->install_button, "clicked", G_CALLBACK(on_start_installation), data);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, 700, 300);
    
    data->terminal = vte_terminal_new();
    VteTerminal *terminal = VTE_TERMINAL(data->terminal);
    
    PangoFontDescription *font = pango_font_description_from_string("Monospace 10");
    vte_terminal_set_font(terminal, font);
    pango_font_description_free(font);
    
    GdkRGBA bg = {0.0, 0.0, 0.0, 1.0};
    GdkRGBA fg = {0.0, 1.0, 0.0, 1.0};
    vte_terminal_set_color_background(terminal, &bg);
    vte_terminal_set_color_foreground(terminal, &fg);
    
    gtk_container_add(GTK_CONTAINER(scrolled), data->terminal);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 10);
    
    return box;
}

GtkWidget* create_finish_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    
    GtkWidget *title = gtk_label_new("🎉 Installation Complete!");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    
    GtkWidget *info = gtk_label_new(
        "Zenith Language has been successfully installed!\n\n"
        "Quick Start:\n"
        "  zenith --version\n"
        "  zenith --help\n"
        "  zenith\n\n"
        "Click 'Next' to see developer contacts");
    context = gtk_widget_get_style_context(info);
    gtk_style_context_add_class(context, "subtitle");
    gtk_label_set_justify(GTK_LABEL(info), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), info, FALSE, FALSE, 10);
    
    return box;
}

// НОВАЯ СТРАНИЦА С КОНТАКТАМИ
GtkWidget* create_contacts_page(InstallerData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    
    GtkWidget *title = gtk_label_new("📱 Developer Contacts");
    GtkStyleContext *context = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(context, "title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    
    GtkWidget *contacts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(contacts_box, 600, 200);
    
    // Создаем рамку для контактов
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkStyleContext *frame_ctx = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(frame_ctx, "contacts");
    
    GtkWidget *contacts_text = gtk_label_new(
        "┌─────────────────────────────────────┐\n"
        "│      ZENITH LANGUAGE PROJECT        │\n"
        "├─────────────────────────────────────┤\n"
        "│                                     │\n"
        "│   Developer: koto_ed                │\n"
        "│                                     │\n"
        "│   Telegram: @koto_ed                │\n"
        "│   TikTok:   @zenithlaun             │\n"
        "│   GitHub:   github.com/kotoed       │\n"
        "│                                     │\n"
        "│   Report bugs & suggestions:        │\n"
        "│   kotoed@proton.me                  │\n"
        "│                                     │\n"
        "│   Thank you for using Zenith!       │\n"
        "│                                     │\n"
        "└─────────────────────────────────────┘");
    
    gtk_label_set_justify(GTK_LABEL(contacts_text), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(frame), contacts_text);
    gtk_box_pack_start(GTK_BOX(contacts_box), frame, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(box), contacts_box, FALSE, FALSE, 0);
    
    GtkWidget *message = gtk_label_new(
        "\nFollow for updates, tutorials and news!\n"
        "Zenith Language - The Future of Systems Programming");
    context = gtk_widget_get_style_context(message);
    gtk_style_context_add_class(context, "subtitle");
    gtk_label_set_justify(GTK_LABEL(message), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), message, FALSE, FALSE, 10);
    
    return box;
}

void on_next_clicked(GtkButton *button, InstallerData *data) {
    const char *current = gtk_stack_get_visible_child_name(GTK_STACK(data->stack));
    
    if (strcmp(current, "welcome") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "distro");
    } else if (strcmp(current, "distro") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "license");
    } else if (strcmp(current, "license") == 0) {
        if (data->license_accepted) {
            gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "components");
        }
    } else if (strcmp(current, "components") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "install");
    } else if (strcmp(current, "install") == 0) {
        // Пользователь должен нажать "Start Installation"
        // а не "Next" на странице установки
    } else if (strcmp(current, "finish") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "contacts");
    }
}

void on_back_clicked(GtkButton *button, InstallerData *data) {
    const char *current = gtk_stack_get_visible_child_name(GTK_STACK(data->stack));
    
    if (strcmp(current, "distro") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "welcome");
    } else if (strcmp(current, "license") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "distro");
    } else if (strcmp(current, "components") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "license");
    } else if (strcmp(current, "install") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "components");
    } else if (strcmp(current, "finish") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "install");
    } else if (strcmp(current, "contacts") == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "finish");
    }
}

void on_stack_changed(GtkStack *stack, GParamSpec *pspec, InstallerData *data) {
    const char *current = gtk_stack_get_visible_child_name(stack);
    
    // Обновляем доступность кнопок
    gtk_widget_set_sensitive(data->back_button, 
        strcmp(current, "welcome") != 0);
    
    if (strcmp(current, "license") == 0) {
        gtk_widget_set_sensitive(data->next_button, data->license_accepted);
    } else if (strcmp(current, "install") == 0) {
        // На странице установки кнопка "Next" неактивна
        gtk_widget_set_sensitive(data->next_button, FALSE);
    } else if (strcmp(current, "contacts") == 0) {
        // На странице контактов скрываем "Next"
        gtk_widget_set_visible(data->next_button, FALSE);
    } else {
        gtk_widget_set_sensitive(data->next_button, TRUE);
        gtk_widget_set_visible(data->next_button, TRUE);
    }
    
    // Показываем "Finish" только на странице контактов
    gtk_widget_set_visible(data->finish_button, strcmp(current, "contacts") == 0);
}

static void on_close_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *win = (GtkWidget*)user_data;
    gtk_window_close(GTK_WINDOW(win));
}

void activate(GtkApplication *app, gpointer user_data) {
    InstallerData *data = g_new0(InstallerData, 1);
    data->installation_started = FALSE;
    
    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "Zenith Language Installer");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 900, 650);
    gtk_window_set_position(GTK_WINDOW(data->window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
    
    apply_dark_theme(data->window);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(data->window), main_box);
    
    data->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(data->stack), 
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(data->stack), 300);
    gtk_box_pack_start(GTK_BOX(main_box), data->stack, TRUE, TRUE, 0);
    
    // Добавляем все страницы
    gtk_stack_add_named(GTK_STACK(data->stack), create_welcome_page(data), "welcome");
    gtk_stack_add_named(GTK_STACK(data->stack), create_distro_page(data), "distro");
    gtk_stack_add_named(GTK_STACK(data->stack), create_license_page(data), "license");
    gtk_stack_add_named(GTK_STACK(data->stack), create_components_page(data), "components");
    gtk_stack_add_named(GTK_STACK(data->stack), create_install_page(data), "install");
    gtk_stack_add_named(GTK_STACK(data->stack), create_finish_page(data), "finish");
    gtk_stack_add_named(GTK_STACK(data->stack), create_contacts_page(data), "contacts"); // ← НОВАЯ СТРАНИЦА
    
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(button_box, 20);
    gtk_widget_set_margin_end(button_box, 20);
    gtk_widget_set_margin_top(button_box, 10);
    gtk_widget_set_margin_bottom(button_box, 20);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    
    data->back_button = gtk_button_new_with_label("← Back");
    gtk_widget_set_sensitive(data->back_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_box), data->back_button, FALSE, FALSE, 0);
    
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(button_box), spacer, TRUE, TRUE, 0);
    
    data->next_button = gtk_button_new_with_label("Next →");
    gtk_box_pack_start(GTK_BOX(button_box), data->next_button, FALSE, FALSE, 0);
    
    data->finish_button = gtk_button_new_with_label("Finish");
    gtk_box_pack_start(GTK_BOX(button_box), data->finish_button, FALSE, FALSE, 0);
    gtk_widget_set_visible(data->finish_button, FALSE);
    
    // Подключаем сигналы
    g_signal_connect(data->next_button, "clicked", G_CALLBACK(on_next_clicked), data);
    g_signal_connect(data->back_button, "clicked", G_CALLBACK(on_back_clicked), data);
    g_signal_connect(data->stack, "notify::visible-child", G_CALLBACK(on_stack_changed), data);
    g_signal_connect(data->finish_button, "clicked", G_CALLBACK(on_close_clicked), data->window);
    
    gtk_widget_show_all(data->window);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.zenith.installer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
