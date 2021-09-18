#include <gtk/gtk.h>
#include <crw-ruler.h>


static void update_ruler(GtkAdjustment *adjustment, gpointer ruler)
{
    double lower = gtk_adjustment_get_value(adjustment);
    double upper = lower + gtk_adjustment_get_page_size(adjustment);
    crw_ruler_set_range(CRW_RULER(ruler), lower, upper);
}

static void activate (GtkApplication *app, gpointer user_data)
{
    GError *error = NULL;

    // We have to call get_type before using GtkBuilder, otherwise there's a chance
    // it won't recognize the CrwRuler class
    crw_ruler_get_type();

    GtkBuilder *builder = gtk_builder_new();
    if (gtk_builder_add_from_file (builder, "builder.ui", &error) == 0)
    {
        g_printerr ("Error loading file: %s\n", error->message);
        g_clear_error (&error);
        return;
    }

    GObject *window = gtk_builder_get_object(builder, "window");
    gtk_window_set_application(GTK_WINDOW(window), app);

    GObject *hruler = gtk_builder_get_object(builder, "hruler");
    GObject *vruler = gtk_builder_get_object(builder, "vruler");

    GObject *scrollwindow = gtk_builder_get_object(builder, "scrollwindow");
    GtkWidget *picture = gtk_picture_new_for_filename ("central-park-nyc.jpg");
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), false);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollwindow), GTK_WIDGET(picture));

    // Connect signals to update rulers when the image is scrolled
    GtkAdjustment *hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrollwindow));
    g_signal_connect(hadjustment, "changed", G_CALLBACK(update_ruler), hruler);
    g_signal_connect(hadjustment, "value-changed", G_CALLBACK(update_ruler), hruler);
    GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrollwindow));
    g_signal_connect(vadjustment, "changed", G_CALLBACK(update_ruler), vruler);
    g_signal_connect(vadjustment, "value-changed", G_CALLBACK(update_ruler), vruler);

    // Add simple CSS to window which will also style the ruler
    const char* style = "window { background-color: #282a36; color: #f8f8f2; } .titlebar { color: #000; }";

    GtkCssProvider *styleProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(styleProvider, style, -1);
    gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(styleProvider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_show (GTK_WIDGET(window));

    g_object_unref(builder);
}

int main (int argc, char **argv)
{
    GtkApplication *app;
    int status;

    app = gtk_application_new ("crw.ruler.demoapp", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}