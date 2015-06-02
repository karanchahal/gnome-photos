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

#include "photos-tool.h"
#include "photos-utils.h"


G_DEFINE_ABSTRACT_TYPE (PhotosTool, photos_tool, G_TYPE_OBJECT);


static void
photos_tool_default_draw (PhotosTool *self, cairo_t *cr, GdkRectangle *rect)
{
}


static gboolean
photos_tool_default_left_click_event (PhotosTool *self, GdkEventButton *event)
{
  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_default_left_unclick_event (PhotosTool *self, GdkEventButton *event)
{
  return GDK_EVENT_PROPAGATE;
}


static gboolean
photos_tool_default_motion_event (PhotosTool *self, GdkEventMotion *event)
{
  return GDK_EVENT_PROPAGATE;
}


static void
photos_tool_init (PhotosTool *self)
{
}


static void
photos_tool_class_init (PhotosToolClass *class)
{
  class->draw = photos_tool_default_draw;
  class->left_click_event = photos_tool_default_left_click_event;
  class->left_unclick_event = photos_tool_default_left_unclick_event;
  class->motion_event = photos_tool_default_motion_event;
}


void
photos_tool_activate (PhotosTool *self, PhotosBaseItem *item, GeglGtkView *view)
{
  PHOTOS_TOOL_GET_CLASS (self)->activate (self, item, view);
}


void
photos_tool_draw (PhotosTool *self, cairo_t *cr, GdkRectangle *rect)
{
  return PHOTOS_TOOL_GET_CLASS (self)->draw (self, cr, rect);
}


const gchar *
photos_tool_get_icon_name (PhotosTool *self)
{
  return PHOTOS_TOOL_GET_CLASS (self)->icon_name;
}


const gchar *
photos_tool_get_name (PhotosTool *self)
{
  return PHOTOS_TOOL_GET_CLASS (self)->name;
}


GtkWidget *
photos_tool_get_widget (PhotosTool *self)
{
  return PHOTOS_TOOL_GET_CLASS (self)->get_widget (self);
}


gboolean
photos_tool_left_click_event (PhotosTool *self, GdkEventButton *event)
{
  return PHOTOS_TOOL_GET_CLASS (self)->left_click_event (self, event);
}


gboolean
photos_tool_left_unclick_event (PhotosTool *self, GdkEventButton *event)
{
  return PHOTOS_TOOL_GET_CLASS (self)->left_unclick_event (self, event);
}


gboolean
photos_tool_motion_event (PhotosTool *self, GdkEventMotion *event)
{
  return PHOTOS_TOOL_GET_CLASS (self)->motion_event (self, event);
}
