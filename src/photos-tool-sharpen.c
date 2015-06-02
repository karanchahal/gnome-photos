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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "photos-icons.h"
#include "photos-tool.h"
#include "photos-tool-sharpen.h"
#include "photos-utils.h"


struct _PhotosToolSharpen
{
  PhotosTool parent_instance;
  GAction *sharpen;
  GtkWidget *scale;
  guint value_changed_id;
};

struct _PhotosToolSharpenClass
{
  PhotosToolClass parent_class;
};


G_DEFINE_TYPE_WITH_CODE (PhotosToolSharpen, photos_tool_sharpen, PHOTOS_TYPE_TOOL,
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TOOL_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "sharpen",
                                                         300));


static const gdouble SHARPEN_SCALE_MAXIMUM = 10.0;
static const gdouble SHARPEN_SCALE_MINIMUM = 0.0;
static const gdouble SHARPEN_SCALE_STEP = 0.5;


static gboolean
photos_tool_sharpen_value_changed_timeout (gpointer user_data)
{
  PhotosToolSharpen *self = PHOTOS_TOOL_SHARPEN (user_data);
  GVariant *parameter;
  gdouble value;

  value = gtk_range_get_value (GTK_RANGE (self->scale));
  parameter = g_variant_new_double (value);
  g_action_activate (self->sharpen, parameter);

  self->value_changed_id = 0;
  return G_SOURCE_REMOVE;
}


static void
photos_tool_sharpen_value_changed (PhotosToolSharpen *self)
{
  if (self->value_changed_id != 0)
    return;

  self->value_changed_id = g_timeout_add (150, photos_tool_sharpen_value_changed_timeout, self);
}


static void
photos_tool_sharpen_activate (PhotosTool *tool, PhotosBaseItem *item, GeglGtkView *view)
{
  PhotosToolSharpen *self = PHOTOS_TOOL_SHARPEN (tool);
  gdouble value;

  if (!photos_base_item_operation_get (item, "gegl:unsharp-mask", "scale", &value, NULL))
    value = SHARPEN_SCALE_MINIMUM;

  value = CLAMP (value, SHARPEN_SCALE_MINIMUM, SHARPEN_SCALE_MAXIMUM);

  g_signal_handlers_block_by_func (self->scale, photos_tool_sharpen_value_changed, self);
  gtk_range_set_value (GTK_RANGE (self->scale), value);
  g_signal_handlers_unblock_by_func (self->scale, photos_tool_sharpen_value_changed, self);
}


static GtkWidget *
photos_tool_sharpen_get_widget (PhotosTool *tool)
{
  PhotosToolSharpen *self = PHOTOS_TOOL_SHARPEN (tool);
  return self->scale;
}


static void
photos_tool_sharpen_dispose (GObject *object)
{
  PhotosToolSharpen *self = PHOTOS_TOOL_SHARPEN (object);

  g_clear_object (&self->scale);

  G_OBJECT_CLASS (photos_tool_sharpen_parent_class)->dispose (object);
}


static void
photos_tool_sharpen_finalize (GObject *object)
{
  PhotosToolSharpen *self = PHOTOS_TOOL_SHARPEN (object);

  if (self->value_changed_id != 0)
    g_source_remove (self->value_changed_id);

  G_OBJECT_CLASS (photos_tool_sharpen_parent_class)->dispose (object);
}


static void
photos_tool_sharpen_init (PhotosToolSharpen *self)
{
  GApplication *app;

  app = g_application_get_default ();
  self->sharpen = g_action_map_lookup_action (G_ACTION_MAP (app), "sharpen-current");

  self->scale = g_object_ref_sink (gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                             SHARPEN_SCALE_MINIMUM,
                                                             SHARPEN_SCALE_MAXIMUM,
                                                             SHARPEN_SCALE_STEP));
  gtk_scale_set_draw_value (GTK_SCALE (self->scale), FALSE);
  g_signal_connect_swapped (self->scale, "value-changed", G_CALLBACK (photos_tool_sharpen_value_changed), self);
}


static void
photos_tool_sharpen_class_init (PhotosToolSharpenClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosToolClass *tool_class = PHOTOS_TOOL_CLASS (class);

  tool_class->icon_name = PHOTOS_ICON_IMAGE_SHARPEN_SYMBOLIC;
  tool_class->name = _("Sharpen");

  object_class->dispose = photos_tool_sharpen_dispose;
  object_class->finalize = photos_tool_sharpen_finalize;
  tool_class->activate = photos_tool_sharpen_activate;
  tool_class->get_widget = photos_tool_sharpen_get_widget;
}
