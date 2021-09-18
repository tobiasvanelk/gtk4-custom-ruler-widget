#include "crw-ruler.h"

/**
 * IDs for \c TEGRuler 's properties.
 */
typedef enum {
    PROP_0, // The first property is reserved by GTK

    PROP_DESIRED_WIDTH,
    PROP_DESIRED_HEIGHT,

    PROP_MAJOR_TICK_LENGTH,
    PROP_MIN_MAJOR_TICK_SPACING,

    // Being the element following the last property,
    // this will be equal to the number of properties
    N_PROPERTIES,

    // Overridden properties should apparently be separated from the
    // non-overridden properties, as far as I can tell?

    /* GtkOrientable */
    PROP_ORIENTATION,

} CrwRulerProperty;

// This object will contain the specification of the properties
// which is initialized in the class_init function.
static GParamSpec *props[N_PROPERTIES] = { NULL, };

/** The name with which all ruler widgets can be referred to in CSS. */
static const char* ruler_css_name = "ruler";

/**
 * The set of valid intervals between major ruler ticks is <br>
 * { x * 10^n | x ∈ \c ruler_valid_intervals AND n : int AND n >= 0 } <br>
 * \c ruler_valid_intervals is used when calculating an appropriate interval
 * depending on the size of the ruler widget and the given range to display.
 */
static const int ruler_valid_intervals[] = {1, 5, 10, 25, 50, 100};

/** The default minimum amount of pixels between each major tick. */
static const int default_min_major_tick_spacing = 80;

/** The minimum amount of pixels between each minor tick. */
static const int ruler_min_minor_tick_spacing = 5;

/** The maximum depth of subdivisions of each segment between major ticks. */
static const int ruler_max_tick_depth = 2;

static const int ruler_default_height = 25;

// TODO Have not figured yet out how to read this information from (CSS) style context
static const double FONT_SIZE = 11;
static const char *FONT_FAMILY = "sans-serif";

/**
 * The instance struct containing the member variables of the ruler.
 */
struct _CrwRuler
{
    GtkWidget parent_instance;

    /**
     * The desired width of the ruler in pixels.
     */
    int desired_width;
    /**
     * The desired height of the ruler in pixels.
     */
    int desired_height;

    /**
     * The lower limit of the range the ruler displays.
     */
    double lower_limit;
    /**
     * The upper limit of the range the ruler displays.
     */
    double upper_limit;

    /**
     * The interval in the ruler range to draw with.
     */
    int interval;

    /* DRAWING PROPERTIES */

    int tick_width;

    double major_tick_length_percent;

    /** The minimum amount of pixels between each major tick. */
    int min_major_tick_spacing;

    /**
     * The orientation of the ruler.
     */
    GtkOrientation orientation;

    // The ruler uses a strategy-like pattern with these pointers to functions
    // that implement the drawing of the different elements of the ruler
    // which depend on the orientation of the ruler

    void (* draw_outline) (CrwRuler *self, cairo_t *cr);

    void (* draw_tick) (CrwRuler *self, cairo_t *cr, double pos, double tick_length_percent, bool draw_label, const char* label);

    int (* pixel_spacing) (CrwRuler *self, double lower_limit, double upper_limit);
};

// Define the type CrwRuler, which extends GtkWidget and implements GtkOrientable
G_DEFINE_TYPE_WITH_CODE(CrwRuler, crw_ruler, GTK_TYPE_WIDGET,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


// Forward declare any necessary functions

static int crw_ruler_calculate_interval(int ruler_width, int min_size_segment, double range_size);

static void crw_ruler_switch_draw_strategy(CrwRuler *self, GtkOrientation orientation);

static int crw_ruler_first_tick(double range_lower, int interval);

static void crw_ruler_update_interval(CrwRuler *self);


// ======================================
// ===== PROPERTY GETTERS / SETTERS =====

void crw_ruler_set_range(CrwRuler *self, double lower_limit, double upper_limit)
{
    g_return_if_fail(lower_limit < upper_limit);

    self->lower_limit = lower_limit;
    self->upper_limit = upper_limit;

    crw_ruler_update_interval(self);
}

double crw_ruler_get_lower_limit(CrwRuler *self)
{
    return self->lower_limit;
}

double crw_ruler_get_upper_limit(CrwRuler *self)
{
    return self->upper_limit;
}

void crw_ruler_set_desired_width(CrwRuler *self, int width)
{
    g_return_if_fail(width >= 0);

    if (self->desired_width == width)
    {
        return;
    }

    self->desired_width = width;
    gtk_widget_queue_resize(GTK_WIDGET(self));
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DESIRED_WIDTH]);
}

int crw_ruler_get_desired_width(CrwRuler *self)
{
    return self->desired_width;
}

void crw_ruler_set_desired_height(CrwRuler *self, int height)
{
    g_return_if_fail(height >= 0);

    if (self->desired_height == height)
    {
        return;
    }

    self->desired_height = height;
    gtk_widget_queue_resize(GTK_WIDGET(self));
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DESIRED_HEIGHT]);
}

int crw_ruler_get_desired_height(CrwRuler *self)
{
    return self->desired_height;
}

/**
 * Sets the orientation of a ruler.
 * @param self
 * @param orientation The orientation of the ruler.
 * @return True if the orientation has been changed from its previous value.
 */
static bool crw_ruler_set_orientation(CrwRuler *self, GtkOrientation orientation)
{
    if (gtk_orientable_get_orientation(GTK_ORIENTABLE(self)) != orientation)
    {
        self->orientation = orientation;
        crw_ruler_switch_draw_strategy(self, self->orientation);

        return true;
    }
    return false;
}

/**
 * Returns the orientation of a ruler.
 * @param self
 * @return The orientation of ruler.
 */
static int crw_ruler_get_orientation(CrwRuler *self)
{
    return self->orientation;
}

void crw_ruler_set_major_tick_length(CrwRuler *self, double length_percent)
{
    self->major_tick_length_percent = length_percent;

    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAJOR_TICK_LENGTH]);
}

void crw_ruler_set_min_major_tick_spacing(CrwRuler *self, int min_spacing)
{
    self->min_major_tick_spacing = min_spacing;

    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MIN_MAJOR_TICK_SPACING]);
}

static void crw_ruler_set_property(GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    CrwRuler *self = CRW_RULER(object);

    switch ((CrwRulerProperty) property_id)
    {
        case PROP_DESIRED_WIDTH:
            crw_ruler_set_desired_width(self, g_value_get_int(value));
            break;

        case PROP_DESIRED_HEIGHT:
            crw_ruler_set_desired_height(self, g_value_get_int(value));
            break;

        case PROP_ORIENTATION:
        {
            bool prop_changed = crw_ruler_set_orientation(self, g_value_get_enum(value));
            if (prop_changed)
            {
                g_object_notify_by_pspec(G_OBJECT(self), pspec);
            }
            break;
        }

        case PROP_MAJOR_TICK_LENGTH:
            crw_ruler_set_major_tick_length(self, g_value_get_double(value));
            break;

        case PROP_MIN_MAJOR_TICK_SPACING:
            crw_ruler_set_min_major_tick_spacing(self, g_value_get_int(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void crw_ruler_get_property(GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    CrwRuler *self = CRW_RULER(object);

    switch ((CrwRulerProperty) property_id)
    {
        case PROP_DESIRED_WIDTH:
            g_value_set_int(value, crw_ruler_get_desired_width(self));
            break;

        case PROP_DESIRED_HEIGHT:
            g_value_set_int(value, crw_ruler_get_desired_height(self));
            break;

        case PROP_ORIENTATION:
            g_value_set_enum(value, crw_ruler_get_orientation(self));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

// =============================
// ===== DRAWING FUNCTIONS =====

/** To draw proper 1px wide lines, we must offset our positions by 0.5. */
const double LINE_COORD_OFFSET = 0.5;

const double LABEL_OFFSET = 4;

const double LABEL_ALIGN = 0.65;

const double TEXT_ANCHOR = 0.5;


int crw_ruler_range_to_draw_pos(double lower_limit, double upper_limit, double pos, double allocated_size)
{
    double range_size = upper_limit - lower_limit;
    double scale = allocated_size / range_size;
    return (int)round(scale * (pos - lower_limit));
}

void crw_ruler_draw_outline(CrwRuler *self, cairo_t *cr)
{
    self->draw_outline(self, cr);
}

void crw_ruler_draw_outline_horizontal(CrwRuler *self, cairo_t *cr)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));
    int height = gtk_widget_get_height(GTK_WIDGET(self));

    cairo_set_line_width(cr, self->tick_width);
    double DRAW_OFFSET = self->tick_width * LINE_COORD_OFFSET;

    // Draw line along left side of ruler
    cairo_move_to(cr, DRAW_OFFSET, 0);
    cairo_line_to(cr, DRAW_OFFSET, height);

    // Draw line along right side of ruler
    cairo_move_to(cr, width - DRAW_OFFSET, 0);
    cairo_line_to(cr, width - DRAW_OFFSET, height);

    // Draw line along bottom of the ruler
    cairo_move_to(cr, 0, height - DRAW_OFFSET);
    cairo_line_to(cr, width, height - DRAW_OFFSET);

    // Render all lines
    cairo_stroke(cr);
}

void crw_ruler_draw_outline_vertical(CrwRuler *self, cairo_t *cr)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));
    int height = gtk_widget_get_height(GTK_WIDGET(self));

    cairo_set_line_width(cr, self->tick_width);
    double DRAW_OFFSET = self->tick_width * LINE_COORD_OFFSET;

    // Draw line along top side of ruler
    cairo_move_to(cr, 0, DRAW_OFFSET);
    cairo_line_to(cr, width, DRAW_OFFSET);

    // Draw line along bottom side of ruler
    cairo_move_to(cr, 0, height - DRAW_OFFSET);
    cairo_line_to(cr, width, height - DRAW_OFFSET);

    // Draw line along bottom of the ruler
    cairo_move_to(cr, width - DRAW_OFFSET, 0);
    cairo_line_to(cr, width - DRAW_OFFSET, height);

    // Render all lines
    cairo_stroke(cr);
}

void crw_ruler_draw_tick(CrwRuler *self, cairo_t *cr, double pos, double tick_length_percent, bool draw_label, const char* label)
{
    return self->draw_tick(self, cr, pos, tick_length_percent, draw_label, label);
}

void crw_ruler_draw_tick_horizontal(CrwRuler *self, cairo_t *cr, double pos, double tick_length_percent, bool draw_label, const char* label)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));
    int height = gtk_widget_get_height(GTK_WIDGET(self));

    int draw_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, pos, width);
    double tick_length = round(height * tick_length_percent);

    const double DRAW_OFFSET = cairo_get_line_width(cr) * LINE_COORD_OFFSET;

    cairo_move_to(cr, draw_pos + DRAW_OFFSET, height);
    cairo_line_to(cr, draw_pos + DRAW_OFFSET, height - tick_length);
    cairo_stroke(cr);

    // Draw label along tick
    if (draw_label)
    {
        cairo_text_extents_t textExtents;
        cairo_text_extents(cr, label, &textExtents);
        // Draw label, vertically centered on the tick line
        cairo_move_to(cr,
                      draw_pos + LABEL_OFFSET,
                      height - LABEL_ALIGN * tick_length + TEXT_ANCHOR * textExtents.height);
        cairo_show_text(cr, label);
    }
}

void crw_ruler_draw_tick_vertical(CrwRuler *self, cairo_t *cr, double pos, double tick_length_percent, bool draw_label, const char* label)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));
    int height = gtk_widget_get_height(GTK_WIDGET(self));

    int draw_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, pos, height);
    double tick_length = round(width * tick_length_percent);

    const double DRAW_OFFSET = cairo_get_line_width(cr) * LINE_COORD_OFFSET;

    cairo_move_to(cr, width, draw_pos + DRAW_OFFSET);
    cairo_line_to(cr, width - tick_length, draw_pos + DRAW_OFFSET);
    cairo_stroke(cr);

    // Draw label along tick
    if (draw_label)
    {
        cairo_text_extents_t textExtents;
        cairo_text_extents(cr, label, &textExtents);

        cairo_save(cr);

        // Draw label, vertically centered on the tick line
        cairo_move_to(cr,
                      width - LABEL_ALIGN * tick_length + TEXT_ANCHOR * textExtents.height,
                      draw_pos + LABEL_OFFSET + textExtents.width);
        cairo_rotate(cr, -M_PI / 2);
        cairo_show_text(cr, label);

        cairo_restore(cr);
    }
}

int crw_ruler_range_pixel_spacing(CrwRuler *self, double lower_pos, double upper_pos)
{
    return self->pixel_spacing(self, lower_pos, upper_pos);
}

int crw_ruler_pixel_spacing_horizontal(CrwRuler *self, double lower_pos, double upper_pos)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));

    int lower_pixel_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, lower_pos, width);
    int upper_pixel_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, upper_pos, width);

    return upper_pixel_pos - lower_pixel_pos;
}

int crw_ruler_pixel_spacing_vertical(CrwRuler *self, double lower_pos, double upper_pos)
{
    int height = gtk_widget_get_height(GTK_WIDGET(self));

    int lower_pixel_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, lower_pos, height);
    int upper_pixel_pos = crw_ruler_range_to_draw_pos(self->lower_limit, self->upper_limit, upper_pos, height);

    return upper_pixel_pos - lower_pixel_pos;
}

/**
 * Draws the minor ticks for a given range.
 * @param self
 * @param cr Cairo context to draw to.
 * @param lower The lower limit of the range.
 * @param upper The upper limit of the range.
 * @param depth The depth of the recursion.
 * @param tick_length_percent The length of the ticks to draw.
 */
static void crw_ruler_draw_minor_ticks(CrwRuler *self,
                                       cairo_t *cr,
                                       double lower,
                                       double upper,
                                       int depth,
                                       double tick_length_percent)
{
    if (depth > ruler_max_tick_depth - 1)
    {
        return;
    }

    // Check that there is enough space between minor tick and edges of the limit when drawn
    if (crw_ruler_range_pixel_spacing(self, lower, upper) < ruler_min_minor_tick_spacing)
    {
        return;
    }

    // Draw tick in middle of range
    double tick_pos = lower + (upper - lower) / 2;
    crw_ruler_draw_tick(self, cr, tick_pos, tick_length_percent, false, NULL);

    // Recursively draw minor ticks between lower limit, tick position and upper limit
    crw_ruler_draw_minor_ticks(self, cr, lower, tick_pos, depth + 1, 0.5 * tick_length_percent);
    crw_ruler_draw_minor_ticks(self, cr, tick_pos, upper, depth + 1, 0.5 * tick_length_percent);
}

static void crw_ruler_draw_ticks(CrwRuler *self, cairo_t *cr)
{
    int first_tick = crw_ruler_first_tick(self->lower_limit, self->interval);

    int pos = first_tick;
    // Move pos over the ruler range
    while (pos < self->upper_limit)
    {
        char *str_format = "%d";
        int buffer_size = snprintf(NULL, 0, str_format, pos);
        char *label_str = malloc(buffer_size + 1);
        snprintf(label_str, buffer_size + 1, str_format, pos);

        crw_ruler_draw_tick(self, cr, pos, self->major_tick_length_percent, true, label_str);
        free(label_str);

        // Draw minor ticks between major ticks
        crw_ruler_draw_minor_ticks(self, cr, pos, pos + self->interval, 0, 0.5 * self->major_tick_length_percent);

        pos += self->interval;
    }
}

static void crw_ruler_switch_draw_strategy(CrwRuler *self, GtkOrientation orientation)
{
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        self->draw_outline = crw_ruler_draw_outline_horizontal;
        self->draw_tick = crw_ruler_draw_tick_horizontal;
        self->pixel_spacing = crw_ruler_pixel_spacing_horizontal;
    }
    else
    {
        self->draw_outline = crw_ruler_draw_outline_vertical;
        self->draw_tick = crw_ruler_draw_tick_vertical;
        self->pixel_spacing = crw_ruler_pixel_spacing_vertical;
    }
}

/**
 * Updates the interval using the current range and allocated size
 * and queues the ruler for a redraw.
 * @param self
 */
static void crw_ruler_update_interval(CrwRuler *self)
{
    GtkOrientation orientation = gtk_orientable_get_orientation(GTK_ORIENTABLE(self));
    int ruler_size;
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        ruler_size = gtk_widget_get_width(GTK_WIDGET(self));
    }
    else
    {
        ruler_size = gtk_widget_get_height(GTK_WIDGET(self));
    }

    if (ruler_size > 0)
    {
        self->interval = crw_ruler_calculate_interval(
                ruler_size,
                self->min_major_tick_spacing,
                self->upper_limit - self->lower_limit);
    }

    // Queue the ruler to be redrawn now that the interval has possibly changed
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

/**
 * Calculates the largest interval between major ruler ticks such that the pixel spacing between
 * the major ticks is at least \p min_size_segment.
 * \remark The calculation is generic for both vertical and horizontal rulers, but is framed as for
 * a horizontal ruler.
 * @param ruler_width The allocated width for the ruler. Must be larger than 0.
 * @param min_size_segment The minimum space in pixels between major ruler ticks.
 * @param range_size The total size of the range. Must be larger than 0.
 * @return An appropriate interval.
 */
static int crw_ruler_calculate_interval(int ruler_width, int min_size_segment, double range_size)
{
    g_return_val_if_fail(ruler_width > 0, 1);
    g_return_val_if_fail(min_size_segment > 0, 1);
    g_return_val_if_fail(range_size > 0, 1);

    double max_num_segments = fmax(1, floor((double)ruler_width/min_size_segment));
    double smallest_interval = ceil(range_size / max_num_segments);
    double interval_magnitude = fmax(0, ceil(log10(smallest_interval)) - 1);

    int interval = 1;
    int n_valid_intervals = sizeof(ruler_valid_intervals) / sizeof(ruler_valid_intervals[0]);
    for (int i = 0; i < n_valid_intervals && interval < smallest_interval; i++)
    {
        interval = (int)(ruler_valid_intervals[i] * pow(10, interval_magnitude));
    }
    return interval;
}

/**
 * Returns a number x such that x is a multiple of \p interval and is smaller than \p range_lower.
 * @param range_lower The lower limit of the range.
 * @param interval The interval of the ruler.
 * @return A number x such that x is a multiple of \p interval and is smaller than \p range_lower.
 */
static int crw_ruler_first_tick(double range_lower, int interval)
{
    return (int)floor(range_lower / interval) * interval;
}


// ==============================
// ===== OVERRIDDEN METHODS =====

static void crw_ruler_measure(GtkWidget *widget,
                              GtkOrientation orientation,
                              int for_size,
                              int *minimum_size,
                              int *natural_size,
                              int *minimum_baseline,
                              int *natural_baseline)
{
    CrwRuler *self = CRW_RULER(widget);

    *minimum_size = 1;

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        *natural_size = self->desired_width;
    }
    else
    {
        *natural_size = self->desired_height;
    }
}

static void crw_ruler_size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
    crw_ruler_update_interval(CRW_RULER(widget));

    // Call parent class size_allocate
    GTK_WIDGET_CLASS(crw_ruler_parent_class)->size_allocate(widget, width, height, baseline);
}

static void crw_ruler_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    CrwRuler* self = CRW_RULER(widget);

    // Create a draw strategy object if we haven't already
    crw_ruler_switch_draw_strategy(self, self->orientation);

    GtkStyleContext *context = gtk_widget_get_style_context(widget);

    GtkAllocation *allocation = &((GtkAllocation) {0, 0, 0, 0});
    gtk_widget_get_allocation(widget, allocation);
    GtkBorder *padding = &((GtkBorder) {0, 0, 0, 0});
    gtk_style_context_get_padding(context, padding);

    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    // Create a cairo context that we can use to draw to
    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0, 0, width, height));

    // Render the background according to the style context
    gtk_render_background(context, cr,
                          padding->left,
                          padding->right,
                          allocation->width,
                          allocation->height);

    // Retrieve the foreground color from the style context
    GdkRGBA color = {0, 0, 0, 1};
    gtk_style_context_get_color(context, &color);

    // Setup cairo context
    cairo_set_line_width(cr, 1);
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);

    cairo_select_font_face(cr, FONT_FAMILY, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);

    crw_ruler_draw_outline(self, cr);
    crw_ruler_draw_ticks(self, cr);

    cairo_destroy(cr);
}

static void crw_ruler_unrealize(GtkWidget *widget)
{
    // Call base unrealize function
    GTK_WIDGET_CLASS(crw_ruler_parent_class)->unrealize(widget);
}

// ================================
// ===== CLASS INITIALIZATION =====

static void crw_ruler_class_init(CrwRulerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    // Assign getter and setter function for properties
    object_class->set_property = crw_ruler_set_property;
    object_class->get_property = crw_ruler_get_property;

    // Override virtual functions in parent
    widget_class->measure = crw_ruler_measure;
    widget_class->size_allocate = crw_ruler_size_allocate;
    widget_class->snapshot = crw_ruler_snapshot;
    widget_class->unrealize = crw_ruler_unrealize;

    props[PROP_DESIRED_WIDTH] =
            g_param_spec_int("desired-width",
                             "Desired width",
                             "Desired width of the ruler.",
                             1, G_MAXINT, ruler_default_height,
                             G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_CONSTRUCT);

    props[PROP_DESIRED_HEIGHT] =
            g_param_spec_int("desired-height",
                             "Desired height",
                             "Desired height of the ruler.",
                             1, G_MAXINT, ruler_default_height,
                             G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_CONSTRUCT);

    props[PROP_MAJOR_TICK_LENGTH] =
            g_param_spec_double("major-tick-length",
                                "Major tick length",
                                "The length of the major ticks as a fraction of the ruler height.",
                                0.1, 1, 0.8,
                                G_PARAM_WRITABLE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_CONSTRUCT);

    props[PROP_MIN_MAJOR_TICK_SPACING] =
            g_param_spec_int("min-major-tick-spacing",
                             "Minimum major tick spacing",
                             "The length of the major ticks as a fraction of the ruler height.",
                             1, G_MAXINT, default_min_major_tick_spacing,
                             G_PARAM_WRITABLE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_CONSTRUCT);

    // Override orientation property of GtkOrientable
    g_object_class_override_property(object_class, PROP_ORIENTATION, "orientation");

    g_object_class_install_properties(object_class, N_PROPERTIES, props);

    // Set css name for class
    gtk_widget_class_set_css_name(widget_class, ruler_css_name);
}

static void crw_ruler_init(CrwRuler *self)
{
    self->lower_limit = 0;
    self->upper_limit = 10;
    self->tick_width = 1;

    crw_ruler_switch_draw_strategy(self, self->orientation);
}

GtkWidget *crw_ruler_new(GtkOrientation orientation)
{
    return g_object_new(CRW_TYPE_RULER, "orientation", orientation, NULL);
}