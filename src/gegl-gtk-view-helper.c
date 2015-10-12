/* This file is part of GEGL-GTK
 *
 * GEGL-GTK is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL-GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL-GTK; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2003, 2004, 2006 Øyvind Kolås <pippin@gimp.org>
 * Copyright (C) 2011 Jon Nordby <jononor@gmail.com>
 * Copyright (C) 2015 Red Hat, Inc.
 */

#include "config.h"

#include <math.h>

#include <babl/babl.h>

#include "gegl-gtk-view-helper.h"
#include "photos-debug.h"


G_DEFINE_TYPE(ViewHelper, view_helper, G_TYPE_OBJECT)


enum {
    SIGNAL_REDRAW_NEEDED,
    SIGNAL_SIZE_CHANGED,
    N_SIGNALS
};

static guint view_helper_signals[N_SIGNALS] = { 0 };


static void
finalize(GObject *gobject);
void
trigger_redraw(ViewHelper *self, GeglRectangle *redraw_rect);


static void
view_helper_class_init(ViewHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = finalize;

    /* Emitted when a redraw is needed, with the area that needs redrawing. */
    view_helper_signals[SIGNAL_REDRAW_NEEDED] = g_signal_new("redraw-needed",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__BOXED,
            G_TYPE_NONE, 1,
            GEGL_TYPE_RECTANGLE);

    /* Emitted when the size of the view changes, with the new size. */
    view_helper_signals[SIGNAL_SIZE_CHANGED] = g_signal_new("size-changed",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__BOXED,
            G_TYPE_NONE, 1,
            GEGL_TYPE_RECTANGLE);
}

static void
view_helper_init(ViewHelper *self)
{
    GdkRectangle invalid_gdkrect = {0, 0, -1, -1};

    self->node        = NULL;
    self->x           = 0;
    self->y           = 0;
    self->scale       = 1.0;
    self->autoscale_policy = GEGL_GTK_VIEW_AUTOSCALE_CONTENT;
    self->block = FALSE;

    self->widget_allocation = invalid_gdkrect;
}

static void
finalize(GObject *gobject)
{
    ViewHelper *self = VIEW_HELPER(gobject);

    if (self->node)
        g_object_unref(self->node);
}

/* Transform a rectangle from model to view coordinates. */
static void
model_rect_to_view_rect(ViewHelper *self, GeglRectangle *rect)
{
    GeglRectangle temp;

    temp.x = self->scale * (rect->x) - self->x;
    temp.y = self->scale * (rect->y) - self->y;
    temp.width = ceil(self->scale * rect->width);
    temp.height = ceil(self->scale * rect->height);

    *rect = temp;
}

static void
update_autoscale(ViewHelper *self)
{
    GdkRectangle viewport = self->widget_allocation;
    GeglRectangle bbox;

    if (!self->node || viewport.width < 0 || viewport.height < 0)
        return;

    bbox = gegl_node_get_bounding_box(self->node);
    if (bbox.width < 0 || bbox.height < 0)
        return;

    if (self->autoscale_policy == GEGL_GTK_VIEW_AUTOSCALE_WIDGET) {
        /* Request widget size change */
        /* XXX: Should we reset scale/x/y here? */
        model_rect_to_view_rect(self, &bbox);
        g_signal_emit(self, view_helper_signals[SIGNAL_SIZE_CHANGED],
                      0, &bbox, NULL);

    } else if (self->autoscale_policy == GEGL_GTK_VIEW_AUTOSCALE_CONTENT) {
        /* Calculate and set scaling factor to make the content fit inside */
        float scale = 1.0;

        if (bbox.width > viewport.width || bbox.height > viewport.height) {
            float width_ratio = bbox.width / (float)viewport.width;
            float height_ratio = bbox.height / (float)viewport.height;
            float max_ratio = width_ratio >= height_ratio ? width_ratio : height_ratio;

            scale = 1.0 / max_ratio;

            bbox.width = (gint) (scale * bbox.width + 0.5);
            bbox.height = (gint) (scale * bbox.height + 0.5);
        }

        self->scale = scale;

        /* At this point, viewport is definitely bigger than bbox. */
        self->x = (bbox.width - viewport.width) / 2.0;
        self->y = (bbox.height - viewport.height) / 2.0;
    }

}

/* When the GeglNode has been computed,
 * find out if the size of the vie changed and
 * emit the "size-changed" signal to notify view
 * find out which area in the view was computed and emit the
 * "redraw-needed" signal to notify it that a redraw is needed */
static void
computed_event(GeglNode      *node,
               GeglRectangle *rect,
               ViewHelper    *self)
{
    update_autoscale(self);
}

ViewHelper *
view_helper_new(void)
{
    return VIEW_HELPER(g_object_new(VIEW_HELPER_TYPE, NULL));
}

/* Draw the view of the GeglNode to the provided cairo context,
 * taking into account transformations et.c.
 * @rect the bounding box of the area to draw in view coordinates
 *
 * For instance called by widget during the draw/expose */
void
view_helper_draw(ViewHelper *self, cairo_t *cr, GdkRectangle *rect)
{
    cairo_surface_t *surface = NULL;
    guchar          *buf = NULL;
    GeglRectangle   roi;
    gint            stride;
    gint64          end;
    gint64          start;

    roi.x = self->x + rect->x;
    roi.y = self->y + rect->y;
    roi.width  = rect->width;
    roi.height = rect->height;

    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, roi.width);
    buf = g_malloc0(stride * roi.height);

    start = g_get_monotonic_time ();

    gegl_node_blit(self->node,
                   self->scale,
                   &roi,
                   babl_format("cairo-ARGB32"),
                   (gpointer)buf,
                   GEGL_AUTO_ROWSTRIDE,
                   GEGL_BLIT_CACHE | (self->block ? 0 : GEGL_BLIT_DIRTY));

    end = g_get_monotonic_time ();
    photos_debug (PHOTOS_DEBUG_GEGL, "Node Blit: %" G_GINT64_FORMAT, end - start);

    surface = cairo_image_surface_create_for_data(buf,
              CAIRO_FORMAT_ARGB32,
              roi.width, roi.height,
              stride);
    cairo_set_source_surface(cr, surface, rect->x, rect->y);
    cairo_paint(cr);

    cairo_surface_destroy(surface);
    g_free(buf);

}

void
view_helper_set_allocation(ViewHelper *self, GdkRectangle *allocation)
{
    self->widget_allocation = *allocation;
    update_autoscale(self);
}

void
trigger_redraw(ViewHelper *self, GeglRectangle *redraw_rect)
{
    GeglRectangle invalid_rect = {0, 0, -1, -1}; /* Indicates full redraw */

    if (!redraw_rect) {
        redraw_rect = &invalid_rect;
    }

    g_signal_emit(self, view_helper_signals[SIGNAL_REDRAW_NEEDED],
                  0, redraw_rect, NULL);
}

void
view_helper_set_node(ViewHelper *self, GeglNode *node)
{
    if (self->node == node)
        return;

    if (self->node) {
        g_signal_handler_disconnect (self->node, self->computed_id);
        g_object_unref(self->node);
    }

    if (node) {
        g_object_ref(node);
        self->node = node;

        self->computed_id = g_signal_connect_object(self->node, "computed",
                                                    G_CALLBACK(computed_event),
                                                    self, 0);

        update_autoscale(self);

    } else {
        self->node = NULL;
        self->computed_id = 0;
    }
}

GeglNode *
view_helper_get_node(ViewHelper *self)
{
    return self->node;
}

void
view_helper_set_scale(ViewHelper *self, float scale)
{
    if (self->scale == scale)
        return;

    self->scale = scale;
    update_autoscale(self);
    trigger_redraw(self, NULL);
}

float
view_helper_get_scale(ViewHelper *self)
{
    return self->scale;
}

float
view_helper_get_x(ViewHelper *self)
{
    return self->x;
}

float
view_helper_get_y(ViewHelper *self)
{
    return self->y;
}

void view_helper_get_transformation(ViewHelper *self, GeglMatrix3 *matrix)
{
    /* XXX: Below gives the right result, but is it really the
     * way we want transformations to work? */

    matrix->coeff [0][0] = self->scale; /* xx */
    matrix->coeff [0][1] = 0.0; /* xy */
    matrix->coeff [0][2] = -self->x; /* x0 */

    matrix->coeff [1][0] = 0.0; /* yx */
    matrix->coeff [1][1] = self->scale; /* yy */
    matrix->coeff [1][2] = -self->y; /* y0 */

    matrix->coeff [2][0] = 0.0;
    matrix->coeff [2][1] = 0.0;
    matrix->coeff [2][2] = 1.0;
}

void
view_helper_set_autoscale_policy(ViewHelper *self, GeglGtkViewAutoscale autoscale)
{
    if (self->autoscale_policy == autoscale)
        return;

    self->autoscale_policy = autoscale;
    update_autoscale(self);
}

GeglGtkViewAutoscale
view_helper_get_autoscale_policy(ViewHelper *self)
{
    return self->autoscale_policy;
}
