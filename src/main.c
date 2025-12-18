#include <glib.h>
#include <adwaita.h>
#include <webkit/webkit.h>
#include "gtk/gtk.h"

#define HOMEPAGE_URL "https://www.google.com/"

typedef struct {
    AdwTabView *tab_view;
    GtkEntry   *url_entry;
    GtkButton  *back;
    GtkButton  *forward;
    GtkWindow  *window;
} AppData;

static gboolean looks_like_uri(const gchar *text) {
    if (!text || !*text)
        return FALSE;

    static GRegex *uri_regex = NULL;
    if (!uri_regex) {
        uri_regex = g_regex_new(
            "^(http[s]?://)?(www\\.)?([a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,63}(/\\S*)?$",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL
        );
    }

    return g_regex_match(uri_regex, text, 0, NULL);
}

static WebKitWebView *get_current_web_view(AppData *app_data) {
    AdwTabPage *page = adw_tab_view_get_selected_page(app_data->tab_view);
    return page ? WEBKIT_WEB_VIEW(adw_tab_page_get_child(page)) : NULL;
}

static void update_navigation_buttons(AppData *app_data) {
    WebKitWebView *web_view = get_current_web_view(app_data);
    if (web_view) {
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->back), webkit_web_view_can_go_back(web_view));
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->forward), webkit_web_view_can_go_forward(web_view));
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->back), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(app_data->forward), FALSE);
    }
}

static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, AppData *app_data) {
    WebKitWebView *current = get_current_web_view(app_data);
    if (current == web_view) {
        if (load_event == WEBKIT_LOAD_FINISHED) {
            const gchar *uri = webkit_web_view_get_uri(web_view);
            if (uri && g_strcmp0(uri, HOMEPAGE_URL) == 0)
                gtk_editable_set_text(GTK_EDITABLE(app_data->url_entry), "");
            else if (uri)
                gtk_editable_set_text(GTK_EDITABLE(app_data->url_entry), uri);
        }
        update_navigation_buttons(app_data);
    }
}

static void on_url_activate(GtkEntry *entry, AppData *app_data) {
    WebKitWebView *web_view = get_current_web_view(app_data);
    if (!web_view)
        return;

    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    gboolean is_uri = looks_like_uri(text);

    gchar *full_url;
    if (is_uri) {
        if (!(g_str_has_prefix(text, "http://") || g_str_has_prefix(text, "https://")))
            full_url = g_strdup_printf("https://%s", text);
        else
            full_url = g_strdup(text);
    } else {
        full_url = g_strdup_printf("https://google.com/search?q=%s", text);
    }

    webkit_web_view_load_uri(web_view, full_url);
    g_free(full_url);
}

static void on_back_clicked(GtkButton *button, AppData *app_data) {
    WebKitWebView *web_view = get_current_web_view(app_data);
    if (web_view)
        webkit_web_view_go_back(web_view);
}

static void on_forward_clicked(GtkButton *button, AppData *app_data) {
    WebKitWebView *web_view = get_current_web_view(app_data);
    if (web_view)
        webkit_web_view_go_forward(web_view);
}

static void on_title_changed(WebKitWebView *web_view, GParamSpec *pspec, AppData *app_data) {
    GListModel *pages = G_LIST_MODEL(adw_tab_view_get_pages(app_data->tab_view));
    guint n_pages = g_list_model_get_n_items(pages);

    for (guint i = 0; i < n_pages; ++i) {
        AdwTabPage *page = g_list_model_get_item(pages, i);
        if (adw_tab_page_get_child(page) == GTK_WIDGET(web_view)) {
            const gchar *title = webkit_web_view_get_title(web_view);
            if (title && *title)
                adw_tab_page_set_title(page, title);
            else
                adw_tab_page_set_title(page, "New Tab");
            g_object_unref(page);
            break;
        }
        g_object_unref(page);
    }
}

static AdwTabPage *on_create_window(AdwTabView *tab_view, AppData *app_data) {
    GtkWidget *web_view = webkit_web_view_new();
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), "https://www.google.com/");
    
    g_signal_connect(web_view, "load-changed", G_CALLBACK(on_load_changed), app_data);
    g_signal_connect(web_view, "notify::title", G_CALLBACK(on_title_changed), app_data);

    AdwTabPage *page = adw_tab_view_append(tab_view, web_view);
    adw_tab_page_set_title(page, "New Tab");
    
    return page;
}

static void on_new_tab_clicked(GtkButton *button, AppData *app_data) {
    on_create_window(app_data->tab_view, app_data);
}

static void on_tab_changed(AdwTabView *tab_view, GParamSpec *pspec, AppData *app_data) {
    WebKitWebView *web_view = get_current_web_view(app_data);
    if (web_view) {
        const gchar *uri = webkit_web_view_get_uri(web_view);
        if (uri && g_strcmp0(uri, HOMEPAGE_URL) == 0)
            gtk_editable_set_text(GTK_EDITABLE(app_data->url_entry), "");
        else if (uri)
            gtk_editable_set_text(GTK_EDITABLE(app_data->url_entry), uri);
    }
    update_navigation_buttons(app_data);
}

static void on_page_detached(AdwTabView *tab_view, AdwTabPage *page, gint position, AppData *app_data) {
    if (adw_tab_view_get_n_pages(tab_view) == 0)
        gtk_window_close(app_data->window);
}

static void on_menu_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    const gchar *action_name = g_action_get_name(G_ACTION(action));

    if (g_strcmp0(action_name, "about") == 0) {
        adw_show_about_dialog(
            GTK_WIDGET(app_data->window),
            "application-name", "Webwatcher",
            "version", "2.0",
            "developer-name", "xrc2",
            "license-type", GTK_LICENSE_BSD_3,
            "comments", "Webwatcher rewritten as a libadwaita/WebKitGTK application.",
            NULL
        );
    } else if (g_strcmp0(action_name, "quit") == 0) {
        gtk_window_close(app_data->window);
    }
}

static void activate_cb(GtkApplication* app) {
    GtkWidget *window = adw_application_window_new(app);
    GtkWidget *toolbar_view = adw_toolbar_view_new();
    
    AppData *app_data = g_new0(AppData, 1);
    app_data->window = GTK_WINDOW(window);
    
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(header_box, "toolbar");
    
    AdwTabView *tab_view = adw_tab_view_new();
    app_data->tab_view = tab_view;
    
    g_signal_connect(tab_view, "page-detached", G_CALLBACK(on_page_detached), app_data);
    
    AdwTabBar *tab_bar = adw_tab_bar_new();
    adw_tab_bar_set_view(tab_bar, tab_view);
    adw_tab_bar_set_autohide(tab_bar, FALSE);
    adw_tab_bar_set_expand_tabs(tab_bar, FALSE);
    
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(controls_box, GTK_ALIGN_CENTER);
    
    GtkWidget *new_tab = gtk_button_new_from_icon_name("tab-new-symbolic");
    gtk_widget_add_css_class(new_tab, "flat");
    g_signal_connect(new_tab, "clicked", G_CALLBACK(on_new_tab_clicked), app_data);
    
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    gtk_widget_add_css_class(menu_button, "flat");
    
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "About Webwatcher", "app.about");
    g_menu_append(menu, "Quit", "app.quit");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
    
    GSimpleAction *about_action = g_simple_action_new("about", NULL);
    GSimpleAction *quit_action = g_simple_action_new("quit", NULL);
    g_signal_connect(about_action, "activate", G_CALLBACK(on_menu_action), app_data);
    g_signal_connect(quit_action, "activate", G_CALLBACK(on_menu_action), app_data);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(about_action));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(quit_action));
    
    GtkWidget *window_controls = gtk_window_controls_new(GTK_PACK_END);
    
    gtk_box_append(GTK_BOX(controls_box), new_tab);
    gtk_box_append(GTK_BOX(controls_box), menu_button);
    gtk_box_append(GTK_BOX(controls_box), window_controls);
    
    gtk_box_append(GTK_BOX(header_box), GTK_WIDGET(tab_bar));
    gtk_box_append(GTK_BOX(header_box), controls_box);
    gtk_widget_set_hexpand(GTK_WIDGET(tab_bar), TRUE);
    
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(nav_box, "toolbar");
    gtk_widget_set_margin_start(nav_box, 6);
    gtk_widget_set_margin_end(nav_box, 6);
    gtk_widget_set_margin_top(nav_box, 6);
    gtk_widget_set_margin_bottom(nav_box, 6);
    
    GtkWidget *back = gtk_button_new_from_icon_name("go-previous-symbolic");
    GtkWidget *forward = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_add_css_class(back, "flat");
    gtk_widget_add_css_class(forward, "flat");
    app_data->back = GTK_BUTTON(back);
    app_data->forward = GTK_BUTTON(forward);

    gtk_widget_set_sensitive(back, FALSE);
    gtk_widget_set_sensitive(forward, FALSE);
    
    GtkWidget *url_entry = gtk_entry_new();
    app_data->url_entry = GTK_ENTRY(url_entry);
    gtk_widget_set_hexpand(url_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(url_entry), "Search Google or type a URL");
    
    gtk_box_append(GTK_BOX(nav_box), back);
    gtk_box_append(GTK_BOX(nav_box), forward);
    gtk_box_append(GTK_BOX(nav_box), url_entry);
    
    g_signal_connect(url_entry, "activate", G_CALLBACK(on_url_activate), app_data);
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_clicked), app_data);
    g_signal_connect(forward, "clicked", G_CALLBACK(on_forward_clicked), app_data);
    g_signal_connect(tab_view, "notify::selected-page", G_CALLBACK(on_tab_changed), app_data);
    
    GtkWidget *web_view = webkit_web_view_new();
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), "https://www.google.com/");
    g_signal_connect(web_view, "load-changed", G_CALLBACK(on_load_changed), app_data);
    g_signal_connect(web_view, "notify::title", G_CALLBACK(on_title_changed), app_data);
    
    AdwTabPage *page = adw_tab_view_append(tab_view, web_view);
    adw_tab_page_set_title(page, "New Tab");
    
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_box);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), nav_box);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), GTK_WIDGET(tab_view));
    
    gtk_window_set_title(GTK_WINDOW(window), "Webwatcher");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), toolbar_view);
    gtk_window_present(GTK_WINDOW(window));
    gtk_editable_set_text(GTK_EDITABLE(url_entry), "");
}

int main(int argc, char *argv[]) {
    g_autoptr(AdwApplication) app = NULL;

    app = adw_application_new("org.mobren.webwatcher", 0);
    g_signal_connect(app, "activate", G_CALLBACK(activate_cb), NULL);

    return g_application_run(G_APPLICATION(app), argc, argv);
}