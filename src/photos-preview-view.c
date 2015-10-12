/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2013, 2014, 2015 Red Hat, Inc.
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

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "gegl-gtk-view.h"
#include "photos-base-item.h"
#include "photos-base-manager.h"
#include "photos-mode-controller.h"
#include "photos-operation-insta-common.h"
#include "photos-preview-nav-buttons.h"
#include "photos-preview-view.h"
#include "photos-search-context.h"


struct _PhotosPreviewViewPrivate
{
  GeglNode *node;
  GtkWidget *overlay;
  GtkWidget *stack;
  GtkWidget *view;
  PhotosBaseManager *item_mngr;
  PhotosModeController *mode_cntrlr;
  PhotosPreviewNavButtons *nav_buttons;
};

enum
{
  PROP_0,
  PROP_OVERLAY
};


G_DEFINE_TYPE_WITH_PRIVATE (PhotosPreviewView, photos_preview_view, GTK_TYPE_SCROLLED_WINDOW);


static GtkWidget *photos_preview_view_create_view (PhotosPreviewView *self);


static void
photos_preview_view_draw_background (PhotosPreviewView *self, cairo_t *cr, GdkRectangle *rect, gpointer user_data)
{
  GtkStyleContext *context;
  GtkStateFlags flags;
  GtkWidget *view = GTK_WIDGET (user_data);

  context = gtk_widget_get_style_context (view);
  flags = gtk_widget_get_state_flags (view);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, flags);
  gtk_render_background (context, cr, 0, 0, rect->width, rect->height);
  gtk_style_context_restore (context);
}


static GtkWidget *
photos_preview_view_get_invisible_view (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GList *children;
  GList *l;
  GtkWidget *current_view;
  GtkWidget *next_view = NULL;

  current_view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  children = gtk_container_get_children (GTK_CONTAINER (priv->stack));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *view = GTK_WIDGET (l->data);

      if (current_view != view)
        {
          next_view = view;
          break;
        }
    }

  g_list_free (children);
  return next_view;
}


static void
photos_preview_view_nav_buttons_activated (PhotosPreviewView *self, PhotosPreviewAction action)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GtkWidget *current_view;
  GtkWidget *new_view;
  GtkWidget *next_view;
  gint position;

  if (action == PHOTOS_PREVIEW_ACTION_NONE)
    return;

  switch (action)
    {
    case PHOTOS_PREVIEW_ACTION_NEXT:
      position = 0;
      break;

    case PHOTOS_PREVIEW_ACTION_PREVIOUS:
      position = -1;
      break;

    case PHOTOS_PREVIEW_ACTION_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  current_view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  gtk_container_child_set (GTK_CONTAINER (priv->stack), current_view, "position", position, NULL);

  next_view = photos_preview_view_get_invisible_view (self);
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), next_view);

  gtk_container_remove (GTK_CONTAINER (priv->stack), current_view);

  new_view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), new_view);
}


static GtkWidget *
photos_preview_view_create_view (PhotosPreviewView *self)
{
  GtkStyleContext *context;
  GtkWidget *view;

  view = GTK_WIDGET (gegl_gtk_view_new ());
  gtk_widget_add_events (view, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  context = gtk_widget_get_style_context (view);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (context, "content-view");
  g_signal_connect_swapped (view, "draw-background", G_CALLBACK (photos_preview_view_draw_background), self);

  /* It has to be visible to become the visible child of priv->stack. */
  gtk_widget_show (view);

  return view;
}


static void
photos_preview_view_process (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (user_data);
  PhotosPreviewViewPrivate *priv = self->priv;
  GError *error = NULL;
  GtkWidget *view;
  PhotosBaseItem *item = PHOTOS_BASE_ITEM (source_object);

  photos_base_item_process_finish (item, res, &error);
  if (error != NULL)
    {
      g_warning ("Unable to process item: %s", error->message);
      g_error_free (error);
    }

  view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  gtk_widget_queue_draw (view);
}


static void
photos_preview_view_insta (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  PhotosOperationInstaPreset preset;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
  if (item == NULL)
    return;

  preset = (PhotosOperationInstaPreset) g_variant_get_int16 (parameter);
  photos_base_item_operation_add (item, "photos:insta-filter", "preset", preset, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
}


static void
photos_preview_view_sharpen (PhotosPreviewView *self, GVariant *parameter)
{
  PhotosBaseItem *item;
  gdouble scale;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
  if (item == NULL)
    return;

  scale = g_variant_get_double (parameter);
  photos_base_item_operation_add (item, "gegl:unsharp-mask", "scale", scale, NULL);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
}


static void
photos_preview_view_undo (PhotosPreviewView *self)
{
  PhotosBaseItem *item;

  item = PHOTOS_BASE_ITEM (photos_base_manager_get_active_object (self->priv->item_mngr));
  if (item == NULL)
    return;

  photos_base_item_operation_undo (item);
  photos_base_item_process_async (item, NULL, photos_preview_view_process, self);
}


static void
photos_preview_view_window_mode_changed (PhotosPreviewView *self, PhotosWindowMode mode, PhotosWindowMode old_mode)
{
  PhotosPreviewViewPrivate *priv = self->priv;

  switch (mode)
    {
    case PHOTOS_WINDOW_MODE_COLLECTIONS:
    case PHOTOS_WINDOW_MODE_EDIT:
    case PHOTOS_WINDOW_MODE_FAVORITES:
    case PHOTOS_WINDOW_MODE_OVERVIEW:
    case PHOTOS_WINDOW_MODE_SEARCH:
      photos_preview_nav_buttons_hide (priv->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_PREVIEW:
      photos_preview_nav_buttons_show (priv->nav_buttons);
      break;

    case PHOTOS_WINDOW_MODE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}


static void
photos_preview_view_dispose (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  g_clear_object (&priv->node);
  g_clear_object (&priv->item_mngr);
  g_clear_object (&priv->mode_cntrlr);

  G_OBJECT_CLASS (photos_preview_view_parent_class)->dispose (object);
}


static void
photos_preview_view_constructed (GObject *object)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  G_OBJECT_CLASS (photos_preview_view_parent_class)->constructed (object);

  /* Add the stack to the scrolled window after the default
   * adjustments have been created.
   */
  gtk_container_add (GTK_CONTAINER (self), priv->stack);

  priv->nav_buttons = photos_preview_nav_buttons_new (self, GTK_OVERLAY (priv->overlay));
  g_signal_connect_swapped (priv->nav_buttons,
                            "activated",
                            G_CALLBACK (photos_preview_view_nav_buttons_activated),
                            self);

  gtk_widget_show_all (GTK_WIDGET (self));
}


static void
photos_preview_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosPreviewView *self = PHOTOS_PREVIEW_VIEW (object);
  PhotosPreviewViewPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_OVERLAY:
      priv->overlay = GTK_WIDGET (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_preview_view_init (PhotosPreviewView *self)
{
  PhotosPreviewViewPrivate *priv;
  GAction *action;
  GApplication *app;
  GtkStyleContext *context;
  GtkWidget *view;
  PhotosSearchContextState *state;

  self->priv = photos_preview_view_get_instance_private (self);
  priv = self->priv;

  app = g_application_get_default ();
  state = photos_search_context_get_state (PHOTOS_SEARCH_CONTEXT (app));

  priv->item_mngr = g_object_ref (state->item_mngr);

  priv->mode_cntrlr = photos_mode_controller_dup_singleton ();
  g_signal_connect_object (priv->mode_cntrlr,
                           "window-mode-changed",
                           G_CALLBACK (photos_preview_view_window_mode_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self), GTK_SHADOW_IN);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "documents-scrolledwin");

  priv->stack = gtk_stack_new ();

  view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), view);
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), view);

  view = photos_preview_view_create_view (self);
  gtk_container_add (GTK_CONTAINER (priv->stack), view);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "insta-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_insta), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "sharpen-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_sharpen), self, G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (app), "undo-current");
  g_signal_connect_object (action, "activate", G_CALLBACK (photos_preview_view_undo), self, G_CONNECT_SWAPPED);
}


static void
photos_preview_view_class_init (PhotosPreviewViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = photos_preview_view_constructed;
  object_class->dispose = photos_preview_view_dispose;
  object_class->set_property = photos_preview_view_set_property;

  g_object_class_install_property (object_class,
                                   PROP_OVERLAY,
                                   g_param_spec_object ("overlay",
                                                        "GtkOverlay object",
                                                        "The stack overlay widget",
                                                        GTK_TYPE_OVERLAY,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}


GtkWidget *
photos_preview_view_new (GtkOverlay *overlay)
{
  return g_object_new (PHOTOS_TYPE_PREVIEW_VIEW, "overlay", overlay, NULL);
}


void
photos_preview_view_load_next (PhotosPreviewView *self)
{
  photos_preview_nav_buttons_next (self->priv->nav_buttons);
}


void
photos_preview_view_load_previous (PhotosPreviewView *self)
{
  photos_preview_nav_buttons_previous (self->priv->nav_buttons);
}


void
photos_preview_view_set_model (PhotosPreviewView *self, GtkTreeModel *model, GtkTreePath *current_path)
{
  PhotosPreviewViewPrivate *priv = self->priv;

  photos_preview_nav_buttons_set_model (self->priv->nav_buttons, model, current_path);
  photos_preview_nav_buttons_show (priv->nav_buttons);
}


void
photos_preview_view_set_node (PhotosPreviewView *self, GeglNode *node)
{
  PhotosPreviewViewPrivate *priv = self->priv;
  GtkWidget *view;

  if (priv->node == node)
    return;

  view = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
  g_clear_object (&priv->node);

  if (node == NULL)
    {
      gtk_container_remove (GTK_CONTAINER (priv->stack), view);

      view = photos_preview_view_create_view (self);
      gtk_container_add (GTK_CONTAINER (priv->stack), view);
    }
  else
    {
      priv->node = g_object_ref (node);

      /* Steals the reference to the GeglNode. */
      gegl_gtk_view_set_node (GEGL_GTK_VIEW (view), g_object_ref (priv->node));
      gtk_widget_queue_draw (view);
    }
}
