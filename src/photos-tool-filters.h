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

#ifndef PHOTOS_TOOL_FILTERS_H
#define PHOTOS_TOOL_FILTERS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_TOOL_FILTERS (photos_tool_filters_get_type ())

#define PHOTOS_TOOL_FILTERS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_TOOL_FILTERS, PhotosToolFilters))

#define PHOTOS_IS_TOOL_FILTERS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_TOOL_FILTERS))

typedef struct _PhotosToolFilters      PhotosToolFilters;
typedef struct _PhotosToolFiltersClass PhotosToolFiltersClass;

GType               photos_tool_filters_get_type           (void) G_GNUC_CONST;

G_END_DECLS

#endif /* PHOTOS_TOOL_FILTERS_H */
