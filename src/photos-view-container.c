/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012 Red Hat, Inc.
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

#include "gd-main-view.h"
#include "photos-item-manager.h"
#include "photos-load-more-button.h"
#include "photos-mode-controller.h"
#include "photos-selection-controller.h"
#include "photos-tracker-controller.h"
#include "photos-utils.h"
#include "photos-view-container.h"


struct _PhotosViewContainerPrivate
{
  GdMainView *view;
  GtkListStore *model;
  GtkWidget *load_more;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosSelectionController *sel_cntrlr;
  PhotosTrackerController *trk_cntrlr;
  gboolean disposed;
  gulong adjustment_changed_id;
  gulong adjustment_value_id;
  gulong scrollbar_visible_id;
};


G_DEFINE_TYPE (PhotosViewContainer, photos_view_container, GTK_TYPE_GRID);


static void
photos_view_container_view_changed (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;
  gboolean end = FALSE;
  gdouble page_size;
  gdouble upper;
  gdouble value;
  gint reveal_area_height = 32;

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));
  if (vscrollbar == NULL || !gtk_widget_get_visible (GTK_WIDGET (vscrollbar)))
    {
      photos_load_more_button_set_block (PHOTOS_LOAD_MORE_BUTTON (priv->load_more), TRUE);
      return;
    }

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  page_size = gtk_adjustment_get_page_size (vadjustment);
  upper = gtk_adjustment_get_upper (vadjustment);
  value = gtk_adjustment_get_value (vadjustment);

  /* Special case these values which happen at construction */
  if ((gint) value == 0 && (gint) upper == 1 && (gint) page_size == 1)
    end = FALSE;
  else
    end = !(value < (upper - page_size - reveal_area_height));

  photos_load_more_button_set_block (PHOTOS_LOAD_MORE_BUTTON (priv->load_more), !end);
}


static void
photos_view_container_connect_view (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  priv->adjustment_changed_id = g_signal_connect_swapped (vadjustment,
                                                          "changed",
                                                          G_CALLBACK (photos_view_container_view_changed),
                                                          self);
  priv->adjustment_value_id = g_signal_connect_swapped (vadjustment,
                                                        "value-changed",
                                                        G_CALLBACK (photos_view_container_view_changed),
                                                        self);

  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));
  priv->scrollbar_visible_id = g_signal_connect_swapped (vscrollbar,
                                                         "notify::visible",
                                                         G_CALLBACK (photos_view_container_view_changed),
                                                         self);

  photos_view_container_view_changed (self);
}


static void
photos_view_container_disconnect_view (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv = self->priv;
  GtkAdjustment *vadjustment;
  GtkWidget *vscrollbar;

  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->view));

  if (priv->adjustment_changed_id != 0)
    {
      g_signal_handler_disconnect (vadjustment, priv->adjustment_changed_id);
      priv->adjustment_changed_id = 0;
    }

  if (priv->adjustment_value_id != 0)
    {
      g_signal_handler_disconnect (vadjustment, priv->adjustment_value_id);
      priv->adjustment_value_id = 0;
    }

  if (priv->scrollbar_visible_id != 0)
    {
      g_signal_handler_disconnect (vscrollbar, priv->scrollbar_visible_id);
      priv->scrollbar_visible_id = 0;
    }
}


static void
photos_view_container_item_activated (GdMainView *main_view,
                                      const gchar * id,
                                      const GtkTreePath *path,
                                      gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  photos_base_manager_set_active_object_by_id (self->priv->item_mngr, id);
}


static void
photos_view_container_query_status_changed (PhotosTrackerController *trk_cntrlr,
                                            gboolean query_status,
                                            gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  PhotosViewContainerPrivate *priv = self->priv;

  if (!query_status)
    {
      priv->model = photos_item_manager_get_model (PHOTOS_ITEM_MANAGER (priv->item_mngr));
      gd_main_view_set_model (priv->view, GTK_TREE_MODEL (priv->model));
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, FALSE);
      /* TODO: update selection */
    }
  else
    {
      photos_selection_controller_freeze_selection (priv->sel_cntrlr, TRUE);
      priv->model = NULL;
      gd_main_view_set_model (priv->view, NULL);
    }
}


static void
photos_view_container_selection_mode_changed (PhotosSelectionController *sel_cntrlr,
                                              gboolean mode,
                                              gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  gd_main_view_set_selection_mode (self->priv->view, mode);
}


static void
photos_view_container_selection_mode_request (GdMainView *main_view, gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  photos_selection_controller_set_selection_mode (self->priv->sel_cntrlr, TRUE);
}


static void
photos_view_container_view_selection_changed (GdMainView *main_view, gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);
  PhotosViewContainerPrivate *priv = self->priv;
  GList *selected_urns;
  GList *selection;

  selection = gd_main_view_get_selection (main_view);
  selected_urns = photos_utils_get_urns_from_paths (selection, GTK_TREE_MODEL (priv->model));
  photos_selection_controller_set_selection (priv->sel_cntrlr, selected_urns);

  if (selection != NULL)
    g_list_free_full (selection, (GDestroyNotify) gtk_tree_path_free);
}


static void
photos_view_container_window_mode_changed (PhotosModeController *mode_cntrlr,
                                           PhotosWindowMode mode,
                                           PhotosWindowMode old_mode,
                                           gpointer user_data)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (user_data);

  if (mode == PHOTOS_WINDOW_MODE_OVERVIEW)
    photos_view_container_connect_view (self);
  else
    photos_view_container_disconnect_view (self);
}


static void
photos_view_container_dispose (GObject *object)
{
  PhotosViewContainer *self = PHOTOS_VIEW_CONTAINER (object);
  PhotosViewContainerPrivate *priv = self->priv;

  if (!priv->disposed)
    {
      photos_view_container_disconnect_view (self);
      priv->disposed = TRUE;
    }

  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->sel_cntrlr);
  g_clear_object (&priv->trk_cntrlr);

  G_OBJECT_CLASS (photos_view_container_parent_class)->dispose (object);
}


static void
photos_view_container_init (PhotosViewContainer *self)
{
  PhotosViewContainerPrivate *priv;
  gboolean status;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            PHOTOS_TYPE_VIEW_CONTAINER,
                                            PhotosViewContainerPrivate);
  priv = self->priv;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  priv->view = gd_main_view_new (GD_MAIN_VIEW_ICON);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->view));

  priv->load_more = photos_load_more_button_new ();
  gtk_container_add (GTK_CONTAINER (self), priv->load_more);

  gtk_widget_show_all (GTK_WIDGET (self));

  g_signal_connect (priv->view, "item-activated", G_CALLBACK (photos_view_container_item_activated), self);
  g_signal_connect (priv->view,
                    "selection-mode-request",
                    G_CALLBACK (photos_view_container_selection_mode_request),
                    self);
  g_signal_connect (priv->view,
                    "view-selection-changed",
                    G_CALLBACK (photos_view_container_view_selection_changed),
                    self);

  priv->item_mngr = photos_item_manager_new ();

  priv->sel_cntrlr = photos_selection_controller_new ();
  g_signal_connect (priv->sel_cntrlr,
                    "selection-mode-changed",
                    G_CALLBACK (photos_view_container_selection_mode_changed),
                    self);
  photos_view_container_selection_mode_changed (priv->sel_cntrlr,
                                                photos_selection_controller_get_selection_mode (priv->sel_cntrlr),
                                                self);

  priv->mode_cntrlr = photos_mode_controller_new ();
  g_signal_connect (priv->mode_cntrlr,
                    "window-mode-changed",
                    G_CALLBACK (photos_view_container_window_mode_changed),
                    self);

  priv->trk_cntrlr = photos_tracker_controller_new ();
  g_signal_connect (priv->trk_cntrlr,
                    "query-status-changed",
                    G_CALLBACK (photos_view_container_query_status_changed),
                    self);
  photos_tracker_controller_start (priv->trk_cntrlr);

  status = photos_tracker_controller_get_query_status (priv->trk_cntrlr);
  photos_view_container_query_status_changed (priv->trk_cntrlr, status, self);
}


static void
photos_view_container_class_init (PhotosViewContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_view_container_dispose;

  g_type_class_add_private (class, sizeof (PhotosViewContainerPrivate));
}


GtkWidget *
photos_view_container_new (void)
{
  return g_object_new (PHOTOS_TYPE_VIEW_CONTAINER, NULL);
}