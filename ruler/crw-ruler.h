#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CRW_TYPE_RULER crw_ruler_get_type()
G_DECLARE_FINAL_TYPE(CrwRuler, crw_ruler, CRW, RULER, GtkWidget)

struct _CrwRulerClass
{
    GtkWidgetClass parent_class;
};

/**
 * Creates a new ruler.
 * @param orientation The orientation of the ruler.
 * @return The new \c CrwRuler.
 */
GtkWidget *crw_ruler_new(GtkOrientation orientation);

/**
 * Sets the range that the ruler will display.
 * @param ruler
 * @param lower_limit The lower limit of the range. Must be smaller than \p upper_limit.
 * @param upper_limit The upper limit of the range. Must be greater than \p lower_limit.
 */
void crw_ruler_set_range(CrwRuler *ruler, double lower_limit, double upper_limit);

/**
 * Returns the lower limit of the range of a ruler.
 * @param self
 * @return The lower limit of the range.
 */
double crw_ruler_get_lower_limit(CrwRuler *ruler);

/**
 * Returns the upper limit of the range of a ruler.
 * @param self
 * @return The upper limit of the range.
 */
double crw_ruler_get_upper_limit(CrwRuler *ruler);

/**
 * Sets the desired width of a ruler.
 *
 * \remark When the self is not set to expand horizontally using \c Gtk.Widget:hexpand,
 * the self will attempt to maintain the given width.
 * @param self
 * @param width The desired width.
 */
void crw_ruler_set_desired_width(CrwRuler *ruler, int width);

/**
 * Sets the desired height of a ruler.
 *
 * \remark When the self is not set to expand vertically using \c Gtk.Widget:vexpand,
 * the self will attempt to maintain the given height.
 * @param self
 * @param height The desired height.
 */
void crw_ruler_set_desired_height(CrwRuler *ruler, int height);

/**
 * Sets the length of the major ticks.
 * @param self
 * @param length_percent The length of the major ticks,
 *      expressed as a fraction of the height of a horizontal ruler or the width of a vertical ruler.
 */
void crw_ruler_set_major_tick_length(CrwRuler *self, double length_percent);

/**
 * Sets the length of the major ticks.
 * @param self
 * @param min_spacing The minimum spacing in pixels between major ruler ticks.
 */
void crw_ruler_set_min_major_tick_spacing(CrwRuler *self, int min_spacing);

G_END_DECLS