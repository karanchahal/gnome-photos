/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014, 2015 Red Hat, Inc.
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

#include "photos-item-manager.h"
#include "photos-offset-favorites-controller.h"
#include "photos-query-builder.h"
#include "photos-search-context.h"
#include "photos-tracker-favorites-controller.h"
#include "photos-utils.h"


struct _PhotosTrackerFavoritesControllerPrivate
{
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosOffsetController *offset_cntrlr;
};


G_DEFINE_TYPE_WITH_CODE (PhotosTrackerFavoritesController,
                         photos_tracker_favorites_controller,
                         PHOTOS_TYPE_TRACKER_CONTROLLER,
                         G_ADD_PRIVATE (PhotosTrackerFavoritesController)
                         photos_utils_ensure_extension_points ();
                         g_io_extension_point_implement (PHOTOS_TRACKER_CONTROLLER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "favorites",
                                                         0));


static void
photos_tracker_favorites_controller_col_active_changed (PhotosTrackerFavoritesController *self)
{
  PhotosWindowMode mode;

  mode = photos_mode_controller_get_window_mode (self->priv->mode_cntrlr);
  if (mode != PHOTOS_WINDOW_MODE_FAVORITES)
    return;

  photos_tracker_controller_refresh_for_object (PHOTOS_TRACKER_CONTROLLER (self));
}


static PhotosOffsetController *
photos_tracker_favorites_controller_get_offset_controller (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerFavoritesController *self = PHOTOS_TRACKER_FAVORITES_CONTROLLER (trk_cntrlr);
  return g_object_ref (self->priv->offset_cntrlr);
}


static PhotosQuery *
photos_tracker_favorites_controller_get_query (PhotosTrackerController *trk_cntrlr)
{
  PhotosTrackerFavoritesController *self = PHOTOS_TRACKER_FAVORITES_CONTROLLER (trk_cntrlr);
  PhotosTrackerFavoritesControllerPrivate *priv = self->priv;
  GApplication *app;
  PhotosBaseItem *collection;
  PhotosSearchContextState *state;
  gint flags;

  collection = photos_item_manager_get_active_collection (PHOTOS_ITEM_MANAGER (priv->item_mngr));
  if (collection != NULL)
    flags = PHOTOS_QUERY_FLAGS_NONE;
  else
    flags = PHOTOS_QUERY_FLAGS_FAVORITES;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  return photos_query_builder_global_query (state, flags, priv->offset_cntrlr);
}


static GObject *
photos_tracker_favorites_controller_constructor (GType type,
                                                guint n_construct_params,
                                                GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (photos_tracker_favorites_controller_parent_class)->constructor (type,
                                                                                             n_construct_params,
                                                                                             construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
photos_tracker_favorites_controller_dispose (GObject *object)
{
  PhotosTrackerFavoritesController *self = PHOTOS_TRACKER_FAVORITES_CONTROLLER (object);
  PhotosTrackerFavoritesControllerPrivate *priv = self->priv;

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);
  g_clear_object (&priv->offset_cntrlr);

  G_OBJECT_CLASS (photos_tracker_favorites_controller_parent_class)->dispose (object);
}


static void
photos_tracker_favorites_controller_init (PhotosTrackerFavoritesController *self)
{
  PhotosTrackerFavoritesControllerPrivate *priv;
  GApplication *app;
  PhotosSearchContextState *state;

  self->priv = photos_tracker_favorites_controller_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->item_mngr = g_object_ref (state->item_mngr);
  g_signal_connect_swapped (priv->item_mngr,
                            "active-collection-changed",
                            G_CALLBACK (photos_tracker_favorites_controller_col_active_changed),
                            self);

  priv->mode_cntrlr = g_object_ref (state->mode_cntrlr);
  priv->offset_cntrlr = photos_offset_favorites_controller_dup_singleton ();
}


static void
photos_tracker_favorites_controller_class_init (PhotosTrackerFavoritesControllerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  PhotosTrackerControllerClass *tracker_controller_class = PHOTOS_TRACKER_CONTROLLER_CLASS (class);

  object_class->constructor = photos_tracker_favorites_controller_constructor;
  object_class->dispose = photos_tracker_favorites_controller_dispose;
  tracker_controller_class->get_offset_controller = photos_tracker_favorites_controller_get_offset_controller;
  tracker_controller_class->get_query = photos_tracker_favorites_controller_get_query;
}


PhotosTrackerController *
photos_tracker_favorites_controller_dup_singleton (void)
{
  return g_object_new (PHOTOS_TYPE_TRACKER_FAVORITES_CONTROLLER, "mode", PHOTOS_WINDOW_MODE_FAVORITES, NULL);
}
