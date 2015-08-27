/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


#include "config.h"

#include <math.h>

#include <cairo.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gegl-gtk-view.h"
#include "photos-icons.h"
#include "photos-tool.h"
#include "photos-tool-crop.h"
#include "photos-utils.h"


typedef enum
{
  PHOTOS_TOOL_CROP_LOCATION_NONE,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT,
  PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_CENTER,
  PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE,
  PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT,
  PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT,
  PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE
} PhotosToolCropLocation;

struct _PhotosToolCrop
{
  PhotosTool parent_instance;
  GeglRectangle bbox_scaled;
  GeglRectangle bbox_source;
  GtkListStore *model;
  GtkWidget *combo_box;
  GtkWidget *view;
  PhotosToolCropLocation location;
  cairo_surface_t *surface;
  gboolean grabbed;
  gdouble crop_height;
  gdouble crop_height0;
  gdouble crop_width;
  gdouble crop_width0;
  gdouble crop_x;
  gdouble crop_x0;
  gdouble crop_y;
  gdouble crop_y0;
  gdouble event_x0;
  gdouble event_y0;
};

struct _PhotosToolCropClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolCrop, photos_tool_crop, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "crop",
                                                         100));


typedef enum
{
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL,
  PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN
} PhotosToolCropAspectRatioType;

typedef struct _PhotosToolCropConstraint PhotosToolCropConstraint;

struct _PhotosToolCropConstraint
{
  PhotosToolCropAspectRatioType aspect_ratio_type;
  const gchar *name;
  guint basis_height;
  guint basis_width;
};

enum
{
  CONSTRAINT_COLUMN_ASPECT_RATIO = 0,
  CONSTRAINT_COLUMN_NAME = 1,
  CONSTRAINT_COLUMN_BASIS_HEIGHT = 2,
  CONSTRAINT_COLUMN_BASIS_WIDTH = 3
};

static PhotosToolCropConstraint CONSTRAINTS[] =
{
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY, N_("Free"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL, N_("Original Size"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN, N_("Screen"), 0, 0 },
  { PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS, N_("Square"), 1, 1 }
};


static gdouble
photos_tool_crop_calculate_aspect_ratio (PhotosToolCrop *self)
{
  gdouble ret_val = 1.0;
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (self->combo_box));

  switch (CONSTRAINTS[active].aspect_ratio_type)
    {
    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY:
      if (self->crop_height > 0.0 && self->crop_width > 0.0)
        ret_val = self->crop_width / self->crop_height;
      else
        g_assert_not_reached ();
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_BASIS:
      ret_val = (gdouble) CONSTRAINTS[active].basis_width / CONSTRAINTS[active].basis_height;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_ORIGINAL:
      ret_val = (gdouble) self->bbox_source.width / self->bbox_source.height;
      break;

    case PHOTOS_TOOL_CROP_ASPECT_RATIO_SCREEN:
      {
        GdkScreen *screen;
        gint height;
        gint width;

        screen = gdk_screen_get_default ();
        height = gdk_screen_get_height (screen);
        width = gdk_screen_get_width (screen);
        ret_val = (gdouble) width / height;
        break;
      }

    default:
      g_assert_not_reached ();
    }

  return ret_val;
}


static void
photos_tool_crop_set_crop (PhotosToolCrop *self, gdouble event_x, gdouble event_y)
{
  gdouble delta_x;
  gdouble delta_y;

  delta_x = event_x - self->event_x0;
  delta_y = event_y - self->event_y0;

  switch (self->location)
    {
    case PHOTOS_TOOL_CROP_LOCATION_NONE:
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT:
      self->crop_height = self->crop_height0 + delta_y;
      self->crop_width = self->crop_width0 - delta_x;
      self->crop_x = self->crop_x0 + delta_x;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT:
      self->crop_height = self->crop_height0 + delta_y;
      self->crop_width = self->crop_width0 + delta_x;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE:
      self->crop_height = self->crop_height0 + delta_y;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_CENTER:
      self->crop_x = self->crop_x0 + delta_x;
      self->crop_y = self->crop_y0 + delta_y;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE:
      self->crop_width = self->crop_width0 - delta_x;
      self->crop_x = self->crop_x0 + delta_x;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE:
      self->crop_width = self->crop_width0 + delta_x;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT:
      self->crop_height = self->crop_height0 - delta_y;
      self->crop_width = self->crop_width0 - delta_x;
      self->crop_x = self->crop_x0 + delta_x;
      self->crop_y = self->crop_y0 + delta_y;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT:
      self->crop_height = self->crop_height0 - delta_y;
      self->crop_width = self->crop_width0 + delta_x;
      self->crop_y = self->crop_y0 + delta_y;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE:
      self->crop_height = self->crop_height0 - delta_y;
      self->crop_y = self->crop_y0 + delta_y;
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_tool_crop_set_cursor (PhotosToolCrop *self)
{
  GdkCursor *cursor = NULL;
  GdkCursorType cursor_type;
  GdkDisplay *display;
  GdkWindow *window;

  window = gtk_widget_get_window (self->view);

  switch (self->location)
    {
    case PHOTOS_TOOL_CROP_LOCATION_NONE:
      goto set_cursor;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT:
      cursor_type = GDK_BOTTOM_LEFT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT:
      cursor_type = GDK_BOTTOM_RIGHT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE:
      cursor_type = GDK_BOTTOM_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_CENTER:
      cursor_type = GDK_FLEUR;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE:
      cursor_type = GDK_LEFT_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE:
      cursor_type = GDK_RIGHT_SIDE;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT:
      cursor_type = GDK_TOP_LEFT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT:
      cursor_type = GDK_TOP_RIGHT_CORNER;
      break;

    case PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE:
      cursor_type = GDK_TOP_SIDE;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_for_display (display, cursor_type);

 set_cursor:
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}


static void
photos_tool_crop_set_location (PhotosToolCrop *self, gdouble cur_x, gdouble cur_y)
{
  const gdouble edge_fuzz = 12.0;
  gdouble crop_x;
  gdouble crop_y;
  gdouble x;
  gdouble y;

  self->location = PHOTOS_TOOL_CROP_LOCATION_NONE;

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  crop_x = self->crop_x - x;
  crop_y = self->crop_y - y;

  if (cur_x > crop_x - edge_fuzz
      && cur_y > crop_y - edge_fuzz
      && cur_x < crop_x + self->crop_width + edge_fuzz
      && cur_y < crop_y + self->crop_height + edge_fuzz)
    {
      if (cur_x < crop_x + edge_fuzz && cur_y < crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_LEFT;
      else if (cur_x > crop_x + self->crop_width - edge_fuzz && cur_y < crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_RIGHT;
      else if (cur_x > crop_x + self->crop_width - edge_fuzz && cur_y > crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_RIGHT;
      else if (cur_x < crop_x + edge_fuzz && cur_y > crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_LEFT;
      else if (cur_y < crop_y + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_TOP_SIDE;
      else if (cur_x > crop_x + self->crop_width - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_RIGHT_SIDE;
      else if (cur_y > crop_y + self->crop_height - edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_BOTTOM_SIDE;
      else if (cur_x < crop_x + edge_fuzz)
        self->location = PHOTOS_TOOL_CROP_LOCATION_LEFT_SIDE;
      else
        self->location = PHOTOS_TOOL_CROP_LOCATION_CENTER;
    }
}


static void
photos_tool_crop_surface_create (PhotosToolCrop *self)
{
  GdkWindow *window;
  gfloat scale;

  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  window = gtk_widget_get_window (self->view);
  scale = gegl_gtk_view_get_scale (GEGL_GTK_VIEW (self->view));
  self->bbox_scaled.height = (gint) (scale * self->bbox_source.height + 0.5);
  self->bbox_scaled.width = (gint) (scale * self->bbox_source.width + 0.5);
  self->surface = gdk_window_create_similar_surface (window,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     self->bbox_scaled.width,
                                                     self->bbox_scaled.height);
}


static void
photos_tool_crop_surface_draw (PhotosToolCrop *self)
{
  cairo_t *cr;
  const gdouble handle_offset = 3.0;
  const gdouble handle_radius = 8.0;
  gdouble one_third_x;
  gdouble one_third_y;

  cr = cairo_create (self->surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
  cairo_paint (cr);

  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_rectangle (cr, self->crop_x, self->crop_y, self->crop_width, self->crop_height);
  cairo_fill (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, 0.25, 0.507, 0.828, 1.0);

  cairo_new_sub_path (cr);
  cairo_arc (cr, self->crop_x - handle_offset, self->crop_y - handle_offset, handle_radius, 0.0, 2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             self->crop_x + self->crop_width + handle_offset,
             self->crop_y - handle_offset,
             handle_radius,
             0.0,
             2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             self->crop_x + self->crop_width + handle_offset,
             self->crop_y + self->crop_height + handle_offset,
             handle_radius,
             0.0,
             2.0 * M_PI);
  cairo_fill (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             self->crop_x - handle_offset,
             self->crop_y + self->crop_height + handle_offset,
             handle_radius,
             0.0,
             2.0 * M_PI);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 1.0);
  cairo_set_line_width (cr, 0.5);
  one_third_x = self->crop_width / 3.0;
  one_third_y = self->crop_height / 3.0;

  cairo_move_to (cr, self->crop_x + one_third_x, self->crop_y);
  cairo_line_to (cr, self->crop_x + one_third_x, self->crop_y + self->crop_height);
  cairo_stroke (cr);

  cairo_move_to (cr, self->crop_x + 2.0 * one_third_x, self->crop_y);
  cairo_line_to (cr, self->crop_x + 2.0 * one_third_x, self->crop_y + self->crop_height);
  cairo_stroke (cr);

  cairo_move_to (cr, self->crop_x, self->crop_y + one_third_y);
  cairo_line_to (cr, self->crop_x + self->crop_width, self->crop_y + one_third_y);
  cairo_stroke (cr);

  cairo_move_to (cr, self->crop_x, self->crop_y + 2.0 * one_third_y);
  cairo_line_to (cr, self->crop_x + self->crop_width, self->crop_y + 2.0 * one_third_y);
  cairo_stroke (cr);

  cairo_destroy (cr);
}


static void
photos_tool_crop_surface_change_constraint (PhotosToolCrop *self)
{
  gdouble aspect_ratio;
  gdouble crop_center_x;
  gdouble crop_center_y;
  gdouble old_area;

  crop_center_x = self->crop_x + self->crop_width / 2;
  crop_center_y = self->crop_y + self->crop_height / 2;
  old_area = self->crop_height * self->crop_width;

  aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self);
  self->crop_height = sqrt (old_area / aspect_ratio);
  self->crop_width = sqrt (old_area * aspect_ratio);
  self->crop_x = crop_center_x - self->crop_width / 2;
  self->crop_y = crop_center_y - self->crop_height / 2;

  photos_tool_crop_surface_draw (self);
}


static void
photos_tool_crop_surface_reset (PhotosToolCrop *self)
{
  gdouble crop_aspect_ratio;
  gdouble aspect_ratio;

  aspect_ratio = (gdouble) self->bbox_source.width / self->bbox_source.height;
  crop_aspect_ratio = photos_tool_crop_calculate_aspect_ratio (self);

  if (crop_aspect_ratio < aspect_ratio)
    {
      self->crop_height = 0.7 * self->bbox_scaled.height;
      self->crop_width = self->crop_height * crop_aspect_ratio;
    }
  else
    {
      self->crop_width = 0.7 * self->bbox_scaled.width;
      self->crop_height = self->crop_width / crop_aspect_ratio;
    }

  self->crop_x = ((gdouble) self->bbox_scaled.width - self->crop_width) / 2.0;
  self->crop_y = ((gdouble) self->bbox_scaled.height - self->crop_height) / 2.0;

  photos_tool_crop_surface_draw (self);
}


static void
photos_tool_crop_changed (PhotosToolCrop *self)
{
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (self->combo_box));
  if (CONSTRAINTS[active].aspect_ratio_type == PHOTOS_TOOL_CROP_ASPECT_RATIO_ANY)
    return;

  photos_tool_crop_surface_change_constraint (self);
  gtk_widget_queue_draw (self->view);
}


static void
photos_tool_crop_size_allocate (PhotosToolCrop *self, GdkRectangle *allocation)
{
  gdouble crop_height_ratio;
  gdouble crop_width_ratio;
  gdouble crop_x_ratio;
  gdouble crop_y_ratio;

  crop_height_ratio = self->crop_height / (gdouble) self->bbox_scaled.height;
  crop_width_ratio = self->crop_width / (gdouble) self->bbox_scaled.width;
  crop_x_ratio = self->crop_x / (gdouble) self->bbox_scaled.width;
  crop_y_ratio = self->crop_y / (gdouble) self->bbox_scaled.height;

  photos_tool_crop_surface_create (self);

  self->crop_height = crop_height_ratio * (gdouble) self->bbox_scaled.height;
  self->crop_width = crop_width_ratio * (gdouble) self->bbox_scaled.width;
  self->crop_x = crop_x_ratio * (gdouble) self->bbox_scaled.width;
  self->crop_y = crop_y_ratio * (gdouble) self->bbox_scaled.height;

  photos_tool_crop_surface_draw (self);
}


static void
photos_tool_crop_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  if (!photos_base_item_get_bbox_source (item, &self->bbox_source))
    g_assert_not_reached ();

  self->view = GTK_WIDGET (view);
  g_signal_connect_swapped (self->view, "size-allocate", G_CALLBACK (photos_tool_crop_size_allocate), self);

  photos_tool_crop_surface_create (self);
  photos_tool_crop_surface_reset (self);
}


static void
photos_tool_crop_draw (PhotosTool *tool, cairo_t *cr, GdkRectangle *rect)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  gdouble x;
  gdouble y;

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  x = -x;

  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  y = -y;

  cairo_save (cr);
  cairo_set_source_surface (cr, self->surface, x, y);
  cairo_paint (cr);
  cairo_restore (cr);
}


static GtkWidget *
photos_tool_crop_get_widget (PhotosTool *tool)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  return self->combo_box;
}


static gboolean
photos_tool_crop_left_click_event (PhotosTool *tool, GdkEventButton *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  self->grabbed = TRUE;
  self->crop_height0 = self->crop_height;
  self->crop_width0 = self->crop_width;
  self->crop_x0 = self->crop_x;
  self->crop_y0 = self->crop_y;
  self->event_x0 = event->x;
  self->event_y0 = event->y;

  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_crop_left_unclick_event (PhotosTool *tool, GdkEventButton *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);

  self->grabbed = FALSE;
  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_crop_motion_event (PhotosTool *tool, GdkEventMotion *event)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (tool);
  const gdouble edge_fuzz = 12.0;
  gdouble crop_x;
  gdouble crop_y;
  gdouble x;
  gdouble y;

  x = (gdouble) gegl_gtk_view_get_x (GEGL_GTK_VIEW (self->view));
  y = (gdouble) gegl_gtk_view_get_y (GEGL_GTK_VIEW (self->view));
  crop_x = self->crop_x - x;
  crop_y = self->crop_y - y;

  if (self->grabbed)
    {
      photos_tool_crop_set_crop (self, event->x, event->y);
      photos_tool_crop_surface_draw (self);
      gtk_widget_queue_draw (self->view);
    }
  else
    {
      photos_tool_crop_set_location (self, event->x, event->y);
      photos_tool_crop_set_cursor (self);
    }

  return GDK_EVENT_STOP;
}


static void
photos_tool_crop_dispose (GObject *object)
{
  PhotosToolCrop *self = PHOTOS_TOOL_CROP (object);

  g_clear_object (&self->model);
  g_clear_object (&self->combo_box);
  g_clear_pointer (&self->surface, (GDestroyNotify) cairo_surface_destroy);

  G_OBJECT_CLASS (photos_tool_crop_parent_class)->dispose (object);
}


static void
photos_tool_crop_init (PhotosToolCrop *self)
{
  GtkCellRenderer *renderer;
  guint i;

  self->model = gtk_list_store_new (4, G_TYPE_INT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

  for (i = 0; i < G_N_ELEMENTS (CONSTRAINTS); i++)
    {
      GtkTreeIter iter;

      gtk_list_store_append (self->model, &iter);
      gtk_list_store_set (self->model,
                          &iter,
                          CONSTRAINT_COLUMN_ASPECT_RATIO, CONSTRAINTS[i].aspect_ratio_type,
                          CONSTRAINT_COLUMN_NAME, CONSTRAINTS[i].name,
                          CONSTRAINT_COLUMN_BASIS_HEIGHT, CONSTRAINTS[i].basis_height,
                          CONSTRAINT_COLUMN_BASIS_WIDTH, CONSTRAINTS[i].basis_width,
                          -1);
    }

  self->combo_box = g_object_ref_sink (gtk_combo_box_new_with_model (GTK_TREE_MODEL (self->model)));
  gtk_combo_box_set_active (GTK_COMBO_BOX (self->combo_box), 1);
  g_signal_connect_swapped (self->combo_box, "changed", G_CALLBACK (photos_tool_crop_changed), self);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->combo_box), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->combo_box), renderer, "text", CONSTRAINT_COLUMN_NAME);
}


static void
photos_tool_crop_class_init (PhotosToolCropClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_CROP_SYMBOLIC;
  tool_class->name = _("Crop");

  object_class->dispose = photos_tool_crop_dispose;
  tool_class->activate = photos_tool_crop_activate;
  tool_class->draw = photos_tool_crop_draw;
  tool_class->get_widget = photos_tool_crop_get_widget;
  tool_class->left_click_event = photos_tool_crop_left_click_event;
  tool_class->left_unclick_event = photos_tool_crop_left_unclick_event;
  tool_class->motion_event = photos_tool_crop_motion_event;
}
