
#include <adwaita.h>

#include <libportal/portal.h>
#include <libportal-gtk4/portal-gtk4.h>

#include "arrow.h"
#include "shape.h"
#include "point.h"

#define N_ARROWS 3

typedef struct {
    GtkWidget* window;
    GtkDrawingArea* area;

    // Background; Loaded image as a cairo surface, we load via GdkTexture and download the pixels once
    cairo_surface_t* image;
    int              image_w;
    int              image_h;

    // Shapes
    GList* shapes;  
    Shape* selected_shape;

    //Drag-to-draw state, widget coordinates.
    double   drag_start_x, drag_start_y;
    gboolean dragging;

    // App and Window states
    gboolean    screenshot_in_progress;

} AppData;


// --------------------------------------------------------------------------------------------------------------------
//  Drawing the app 
// --------------------------------------------------------------------------------------------------------------------

static void get_image_transform(AppData* app, double* scale, double* off_x, double* off_y) {
    if (app->image == NULL) {
        *scale = 1.0; *off_x = 0.0; *off_y = 0.0;
        return;
    }

    int width  = gtk_widget_get_width(GTK_WIDGET(app->area));
    int height = gtk_widget_get_height(GTK_WIDGET(app->area));

    *scale = MIN((double)width  / app->image_w,
                 (double)height / app->image_h);
    *off_x = (width  - app->image_w * *scale) / 2.0;
    *off_y = (height - app->image_h * *scale) / 2.0;
}

static Point widget_to_image(AppData* app, double wx, double wy) {
    double scale, off_x, off_y;
    get_image_transform(app, &scale, &off_x, &off_y);
    return point_new((wx - off_x) / scale, (wy - off_y) / scale);
}

static void on_draw(GtkDrawingArea* /*area*/, cairo_t* cr, int width, int height, gpointer user_data) {
    AppData* app = user_data;

    // Background image (scaled to fit, centered, letterboxed)
    if (app->image != NULL) {
        double scale  = MIN((double)width  / app->image_w,
                            (double)height / app->image_h);
        double draw_w = app->image_w * scale;
        double draw_h = app->image_h * scale;
        double off_x  = (width  - draw_w) / 2.0;
        double off_y  = (height - draw_h) / 2.0;

        cairo_save(cr);
        cairo_translate(cr, off_x, off_y);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, app->image, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }

    // Draw all elements on top of the background
    for (GList* l = app->shapes; l != NULL; l = l->next) {
        Shape* s = (Shape*)l->data;
        s->on_draw(s, cr);
    }    
}


// --------------------------------------------------------------------------------------------------------------------
//  Drag to draw elements 
//  Click selection detection
// --------------------------------------------------------------------------------------------------------------------

static void on_drag_begin(GtkGestureDrag* /*gesture*/, double x, double y, gpointer user_data) {
    AppData* app = user_data;
    app->drag_start_x = x;
    app->drag_start_y = y;

    Point tmp = widget_to_image(app, x, y);
    printf("Clicked into image at : %f x %f\n", tmp.x, tmp.y);
}

static void on_drag_update(GtkGestureDrag* /*gesture*/, double dx, double dy, gpointer user_data) {
    AppData* app = user_data;

    // No dragging/creating if no background image
    if(app->image == NULL) {
        return;
    }

    if(app->dragging == false) {
        // check to see if we should start dragging a NEW shape or a SELECTED shape
        if(gtk_drag_check_threshold(GTK_WIDGET(app->area), app->drag_start_x, app->drag_start_y, app->drag_start_x+dx, app->drag_start_y+dy)) {

            if(app->selected_shape != NULL) {
                // check if we are close enough of a control point? 
                if(app->selected_shape->is_handle_hit(app->selected_shape, app->drag_start_x, app->drag_start_y)) {
                    printf("drag update, close to handle, selected handle: %d\n", app->selected_shape->dragging_point);
                    app->dragging = true;
                } else {
                    // click away, deselect
                    app->selected_shape->is_showing_handles = false;
                    app->selected_shape = NULL;
                }

            } else {
                // start a new shape with the current selected shape type.
                Shape* arrow = arrow_new(point_new(app->drag_start_x, app->drag_start_y), point_new(app->drag_start_x+dx, app->drag_start_y+dy));
                arrow->dragging_point = 1;
                app->shapes = g_list_append(app->shapes, arrow);
                app->selected_shape = arrow;
                app->dragging = true;
            }
        }
    } 
    
    if(app->dragging) {
        app->selected_shape->points[app->selected_shape->dragging_point] = point_new(app->drag_start_x+dx, app->drag_start_y+dy);
        gtk_widget_queue_draw(GTK_WIDGET(app->area));
    }
}

static void on_drag_end(GtkGestureDrag* /*gesture*/, double /*dx*/, double /*dy*/, gpointer user_data) {
    AppData* app = user_data;

    // No dragging/creating if no background image
    if(app->image == NULL) {
        return;
    }
    
    if(app->dragging) {
        // we are done creating/modifying the shape
        app->dragging = false;

        // We undo the shape selection, only on new creation, 
        // If it is showing its handles, it was selected manually.
        if(app->selected_shape->is_showing_handles == false) {
            app->selected_shape = NULL;
        }
    } else {
        // We were not dragging, so lets find out if we clicked a shape.

        // Start by deselecting all the shapes
        app->selected_shape = NULL;
        for (GList* l = app->shapes; l != NULL; l = l->next) {
            Shape* s = (Shape*)l->data;
            s->is_showing_handles = false;
        }    

        // Now check if we are in a shape ... and select it if it is the case
        for (GList* l = app->shapes; l != NULL; l = l->next) {
            Shape* s = (Shape*)l->data;
            if(s->is_hit(s, app->drag_start_x, app->drag_start_y)) {
                s->is_showing_handles = true;
                app->selected_shape = s;
                break;
            }
        }    
    }

    gtk_widget_queue_draw(GTK_WIDGET(app->area));
}


// --------------------------------------------------------------------------------------------------------------------
//  Background handling 
// --------------------------------------------------------------------------------------------------------------------

static void set_background_from_file(GFile* file, gpointer user_data) {
    AppData* app = user_data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GdkTexture) texture = gdk_texture_new_from_file(file, &error);

    if (texture == NULL) {
        g_printerr("Could not load image: %s\n", error ? error->message : "unknown error");
        return;
    }

    int w = gdk_texture_get_width(texture);
    int h = gdk_texture_get_height(texture);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    gdk_texture_download(texture, cairo_image_surface_get_data(surface), cairo_image_surface_get_stride(surface));
    cairo_surface_mark_dirty(surface);

    g_clear_pointer(&app->image, cairo_surface_destroy);
    app->image = surface;
    app->image_w = w;
    app->image_h = h;

    gtk_widget_queue_draw(GTK_WIDGET(app->area));
}


// --------------------------------------------------------------------------------------------------------------------
//  Screenshot 
// --------------------------------------------------------------------------------------------------------------------

static void on_screenshot_done(GObject* source, GAsyncResult* res, gpointer user_data) {
    AppData* app = user_data;
    app->screenshot_in_progress = false;

    // The window was hidden to take the screenshot, show it again
    gtk_widget_set_visible(GTK_WIDGET(app->window), true);

    XdpPortal* portal = XDP_PORTAL (source);
    g_autoptr(GError) error = NULL;

    g_autofree char* uri = xdp_portal_take_screenshot_finish (portal, res, &error);
    if(uri == NULL) {
        g_warning("Screenshot failed: %s", error->message);
        return;
    }

    g_autoptr(GFile) file = g_file_new_for_uri (uri);

    set_background_from_file(file, user_data);

    // Delete the screenshot file from the disk
    // We will save ours later
    g_file_delete(file, NULL, NULL);
}

static gboolean do_take_screenshot(gpointer user_data) {
    g_autoptr(GError) error = NULL;

    XdpPortal* portal = xdp_portal_initable_new(&error);
    
    if(error != NULL) {
        g_warning ("Screenshot system initialization failed: %s", error->message);
        return G_SOURCE_REMOVE;
    }

    xdp_portal_take_screenshot (portal,
                                NULL,
                                XDP_SCREENSHOT_FLAG_INTERACTIVE, //XDP_SCREENSHOT_FLAG_NONE,
                                NULL,
                                on_screenshot_done,
                                user_data);

    return G_SOURCE_REMOVE;
}

static void on_window_unmapped(GtkWidget* /*widget*/, gpointer user_data) {
    AppData* app = user_data;

    if(app->screenshot_in_progress) {
        // Window is unmapped, but GNOME Shell's fade-out animation is still playing give it a moment
        g_timeout_add (300, do_take_screenshot, user_data);
    }
}

static void on_screenshot_clicked(GtkButton* /*btn*/, gpointer user_data) {
    AppData* app = user_data;
    app->screenshot_in_progress = true;

    // Hide the app before taking a screenshot
    gtk_widget_set_visible(GTK_WIDGET(app->window), false);

    // The screenshot will happen from on_window_unmapped, once the window is hidden
}


// --------------------------------------------------------------------------------------------------------------------
//  File opening 
// --------------------------------------------------------------------------------------------------------------------

static void on_file_chosen(GObject* source, GAsyncResult* result, gpointer user_data) {
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source);

    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file == NULL) {
        g_clear_error(&error);
        return;
    }

    set_background_from_file(file, user_data);
}

static void on_open_clicked(GtkButton* /*btn*/, gpointer user_data) {
    AppData* app = user_data;

    g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choose an image");

    g_autoptr(GtkFileFilter)filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/webp");
    gtk_file_filter_add_mime_type(filter, "image/bmp");

    g_autoptr(GListStore)filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dialog, filter);

    gtk_file_dialog_open(dialog, GTK_WINDOW(app->window), NULL, on_file_chosen, user_data);
}


// --------------------------------------------------------------------------------------------------------------------
//  Keyboard Shortcuts and Menu Items
// --------------------------------------------------------------------------------------------------------------------

// 1. Define action callbacks for when items are clicked
static void on_menu_open(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    g_print("Open clicked!\n");
}

static void on_menu_save(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    g_print("Save clicked!\n");
}

static void on_menu_save_as(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    g_print("Save As clicked!\n");
}

// Ctrl+o shortcut callback
static gboolean on_open_shortcut(GtkWidget* /*widget*/, GVariant* /*args*/, gpointer user_data) {
    on_open_clicked(NULL, user_data);
    return true;
}

// Del shortcut callback
static gboolean on_delete_shortcut(GtkWidget* /*widget*/, GVariant* /*args*/, gpointer user_data) {
    AppData* app = user_data;
    if(app->selected_shape != NULL) {
        for (GList* l = app->shapes; l != NULL; l = l->next) {
            Shape* s = (Shape*)l->data;
            if(s == app->selected_shape) {
                app->shapes = g_list_remove_link(app->shapes, l);   // Remove element from list, returns the head
                shape_free(s);                                       // Free the shape and its data
                g_list_free(l);                                      // Free the list element itself
                app->selected_shape = NULL;
                gtk_widget_queue_draw(GTK_WIDGET(app->area));
                return true;
            }
        }    
    }
    return false;
}


// --------------------------------------------------------------------------------------------------------------------
//  Application teardown 
// --------------------------------------------------------------------------------------------------------------------

static void main_window_free(gpointer data) {
    AppData* app = data;

    for (GList* l = app->shapes; l != NULL; l = l->next) {
        shape_free((Shape*)l->data);
    }    
    g_list_free(app->shapes);

    g_clear_pointer(&app->image, cairo_surface_destroy);
    g_free(app);
}


// --------------------------------------------------------------------------------------------------------------------
//  Main window creation 
// --------------------------------------------------------------------------------------------------------------------

static GMenuModel* create_menu_model() {
    GMenu* main_menu = g_menu_new();
    GMenu* file_section = g_menu_new();

    // Add individual items to the submenu (Label, Action Name)
    // Note: "win." matches the target scope prefix of the window actions
    g_menu_append(file_section, "Open", "win.open");
    g_menu_append(file_section, "Save", "win.save");
    g_menu_append(file_section, "Save As...", "win.save-as");
    g_menu_append_section(main_menu, NULL, G_MENU_MODEL(file_section));

    g_object_unref(file_section);
    return G_MENU_MODEL(main_menu);
}

static GtkWidget* main_window_new(AdwApplication* adw_app) {
    AppData* app = g_new0(AppData, 1);

    app->window = adw_application_window_new(GTK_APPLICATION(adw_app));
    gtk_window_set_title(GTK_WINDOW(app->window), "Regarde Moi Ca");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 640);

    // Tie the RegardeMoiCa's lifetime to the window
    g_object_set_data_full(G_OBJECT(app->window), "regardemoica-state", app, main_window_free);

    // Layout: ToolbarView with a HeaderBar on top
    GtkWidget* toolbar_view = adw_toolbar_view_new();

    // Application Header Bar
    GtkWidget* header = adw_header_bar_new();
    
    GtkWidget* open_btn = gtk_button_new_with_label("Open Image");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), app);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), open_btn);

    GtkWidget* screenshot_btn = gtk_button_new_with_label("Screen Shot");
    gtk_widget_add_css_class(screenshot_btn, "suggested-action");
    g_signal_connect(screenshot_btn, "clicked", G_CALLBACK(on_screenshot_clicked), app);
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), screenshot_btn);

    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);


    // Create the Menu Button (The Burger Menu)
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    gtk_widget_set_tooltip_text(menu_button, "Main Menu");    

    // g_autoptr(GMenuModel) menu_model = create_menu_model();
    GMenuModel* menu_model = create_menu_model();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), menu_model);

    // Pack the burger menu on the right edge (end) of the header bar
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), menu_button);


    // Drawing area: paints the image, then the arrows on top
    GtkWidget* area = gtk_drawing_area_new();
    app->area = GTK_DRAWING_AREA(area);
    gtk_widget_set_hexpand(area, TRUE);
    gtk_widget_set_vexpand(area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), on_draw, app, NULL);

    // Drag gesture so the user can draw their own arrow
    GtkGesture* drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin",  G_CALLBACK(on_drag_begin),  app);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), app);
    g_signal_connect(drag, "drag-end",    G_CALLBACK(on_drag_end),    app);
    gtk_widget_add_controller(area, GTK_EVENT_CONTROLLER(drag));

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), area);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(app->window), toolbar_view);

    // Ctrl+O shortcut
    GtkShortcut* shortcutCTRLO = gtk_shortcut_new(gtk_shortcut_trigger_parse_string("<Control>o"), gtk_callback_action_new(on_open_shortcut, app, NULL));
    GtkShortcut* shortcutDEL = gtk_shortcut_new(gtk_shortcut_trigger_parse_string("Delete"), gtk_callback_action_new(on_delete_shortcut, app, NULL));
    GtkEventController* controller = gtk_shortcut_controller_new();
    gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(controller), GTK_SHORTCUT_SCOPE_GLOBAL);
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(controller), shortcutCTRLO);
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(controller), shortcutDEL);
    gtk_widget_add_controller(app->window, controller);

    // Hiding of the main window, to take a screenshot
    g_signal_connect(app->window, "unmap", G_CALLBACK(on_window_unmapped), app);    

    return app->window;
}


// --------------------------------------------------------------------------------------------------------------------
//  Application start 
// --------------------------------------------------------------------------------------------------------------------

static void on_activate(GApplication* gapp, gpointer /*user_data*/) {
    GtkWindow* win = gtk_application_get_active_window(GTK_APPLICATION(gapp));
    
    if (win == NULL) {
        win = GTK_WINDOW(main_window_new(ADW_APPLICATION(gapp)));
    }
    
    gtk_window_present(win);
}

int main(int argc, char** argv) {
    g_autoptr(AdwApplication) app = adw_application_new("com.pallaire.regardemoica", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
