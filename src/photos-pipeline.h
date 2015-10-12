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

#ifndef PHOTOS_PIPELINE_H
#define PHOTOS_PIPELINE_H

#include <stdarg.h>

#include <gegl.h>

G_BEGIN_DECLS

#define PHOTOS_TYPE_PIPELINE (photos_pipeline_get_type ())

#define PHOTOS_PIPELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   PHOTOS_TYPE_PIPELINE, PhotosPipeline))

#define PHOTOS_PIPELINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   PHOTOS_TYPE_PIPELINE, PhotosPipelineClass))

#define PHOTOS_IS_PIPELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   PHOTOS_TYPE_PIPELINE))

#define PHOTOS_IS_PIPELINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   PHOTOS_TYPE_PIPELINE))

#define PHOTOS_PIPELINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   PHOTOS_TYPE_PIPELINE, PhotosPipelineClass))

typedef struct _PhotosPipeline      PhotosPipeline;
typedef struct _PhotosPipelineClass PhotosPipelineClass;

GType                  photos_pipeline_get_type          (void) G_GNUC_CONST;

PhotosPipeline        *photos_pipeline_new               (GeglNode *parent);

void                   photos_pipeline_add               (PhotosPipeline *self,
                                                          const gchar *operation,
                                                          const gchar *first_property_name,
                                                          va_list ap);

gboolean               photos_pipeline_get               (PhotosPipeline *self,
                                                          const gchar *operation,
                                                          const gchar *first_property_name,
                                                          va_list ap) G_GNUC_WARN_UNUSED_RESULT;

GeglNode              *photos_pipeline_get_graph         (PhotosPipeline *self);

GeglNode              *photos_pipeline_get_output        (PhotosPipeline *self);

GeglProcessor         *photos_pipeline_new_processor     (PhotosPipeline *self);

void                   photos_pipeline_redo              (PhotosPipeline *self);

void                   photos_pipeline_reset             (PhotosPipeline *self);

gboolean               photos_pipeline_undo              (PhotosPipeline *self);

G_END_DECLS

#endif /* PHOTOS_PIPELINE_H */
