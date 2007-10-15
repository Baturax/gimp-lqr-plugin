/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * (implementation based on the GIMP Plug-in Template by Michael Natterer)
 * All Rights Reserved.
 *
 * This plugin implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>

#include <libgimp/gimp.h>

#include "lqr.h"
#include "lqr_gradient.h"
#include "lqr_raster.h"
#include "lqr_external.h"

#include "main.h"
#include "render.h"
#include "plugin-intl.h"


#define MEMCHECK(x) if ((x) == FALSE) { g_message(_("Not enough memory")); return FALSE; }

gboolean
render (gint32 image_ID,
        GimpDrawable * drawable,
        PlugInVals * vals,
        PlugInImageVals * image_vals, PlugInDrawableVals * drawable_vals,
        PlugInColVals * col_vals)
{
  LqrRaster *rasta;
  gint32 mask_ID;
  gint32 layer_ID;
  gchar layer_name[LQR_MAX_NAME_LENGTH];
  gchar new_layer_name[LQR_MAX_NAME_LENGTH];
  gint old_width, old_height;
  gint x_off, y_off, aux_x_off, aux_y_off;
  GimpRGB color_start, color_end;
#ifdef __LQR_CLOCK__
  double clock1, clock2, clock3;
#endif //CLOCK

  if (drawable_vals->layer_ID) {
	  layer_ID = drawable_vals->layer_ID;
  } else {
	  layer_ID = gimp_image_get_active_layer(image_ID);
  }


  if (!gimp_drawable_is_layer (layer_ID))
    {
      g_message (_("Error: it seems that the selected layer "
                   "is no longer valid"));
      return FALSE;
    }

  if (vals->pres_layer_ID != 0)
    {
      if (!gimp_drawable_is_layer (vals->pres_layer_ID))
        {
          g_message (_("Error: it seems that the preservation "
                       "features layer is no longer valid"));
          return FALSE;
        }
    }

  if (vals->disc_layer_ID != 0)
    {
      if (!gimp_drawable_is_layer (vals->disc_layer_ID))
        {
          g_message (_("Error: it seems that the discard features "
                       "layer is no longer valid"));
          return FALSE;
        }
    }

  if (gimp_layer_is_floating_sel (layer_ID))
    {
      gimp_floating_sel_to_layer (layer_ID);
    }

  drawable = gimp_drawable_get (layer_ID);

  snprintf (layer_name, LQR_MAX_NAME_LENGTH, "%s",
            gimp_drawable_get_name (drawable->drawable_id));

  if (gimp_selection_is_empty (image_ID) == FALSE)
    {
      gimp_selection_save (image_ID);
      gimp_selection_none (image_ID);
      gimp_image_unset_active_channel (image_ID);
    }

  if (vals->output_seams)
    {
      gimp_image_convert_rgb (image_ID);
    }


  if (vals->new_layer == TRUE)
    {
      snprintf (new_layer_name, LQR_MAX_NAME_LENGTH, "%s LqR", layer_name);
      layer_ID = gimp_layer_copy (drawable->drawable_id);
      gimp_image_add_layer (image_ID, layer_ID, -1);
      drawable = gimp_drawable_get (layer_ID);
      gimp_drawable_set_name (layer_ID, new_layer_name);
      gimp_drawable_set_visible (layer_ID, FALSE);
    }

  mask_ID = gimp_layer_get_mask (drawable->drawable_id);
  if (mask_ID != -1)
    {
      gimp_layer_remove_mask (drawable->drawable_id, vals->mask_behavior);
    }

  /* unset lock alpha */
  /* gimp_layer_set_lock_alpha(drawable->drawable_id, FALSE); */
  gimp_layer_set_preserve_trans (drawable->drawable_id, FALSE);

#ifdef __LQR_CLOCK__
  clock1 = (double) clock() / CLOCKS_PER_SEC;
  printf ("[ begin: clock: %g ]\n", clock1);
#endif // __LQR_CLOCK__

  if (vals->resize_aux_layers == TRUE)
    {
      gimp_drawable_offsets (drawable->drawable_id, &x_off, &y_off);
      old_width = gimp_drawable_width (drawable->drawable_id);
      old_height = gimp_drawable_height (drawable->drawable_id);
      if (vals->pres_layer_ID != 0)
        {
          gimp_drawable_offsets (vals->pres_layer_ID, &aux_x_off, &aux_y_off);
          gimp_layer_resize (vals->pres_layer_ID, old_width, old_height,
                             aux_x_off - x_off, aux_y_off - y_off);
        }
      if (vals->disc_layer_ID != 0)
        {
          gimp_drawable_offsets (vals->disc_layer_ID, &aux_x_off, &aux_y_off);
          gimp_layer_resize (vals->disc_layer_ID, old_width, old_height,
                             aux_x_off - x_off, aux_y_off - y_off);
        }
    }

  gimp_rgba_set (&color_start, col_vals->r1, col_vals->g1, col_vals->b1, 1);
  gimp_rgba_set (&color_end, col_vals->r2, col_vals->g2, col_vals->b2, 1);

  rasta =
    lqr_raster_new (image_ID, drawable, layer_name, vals->pres_layer_ID,
                       vals->pres_coeff, vals->disc_layer_ID,
                       vals->disc_coeff, vals->grad_func, vals->rigidity,
                       vals->resize_aux_layers, vals->output_seams,
                       color_start, color_end);
  MEMCHECK (rasta != NULL);

  MEMCHECK (lqr_raster_resize (rasta, vals->new_width, vals->new_height));

  if (vals->resize_canvas == TRUE)
    {
      gimp_image_resize (image_ID, vals->new_width, vals->new_height, 0, 0);
      gimp_layer_resize_to_image_size (layer_ID);
    }
  else
    {
      gimp_layer_resize (layer_ID, vals->new_width, vals->new_height, 0, 0);
      x_off = 0;
      y_off = 0;
    }
  drawable = gimp_drawable_get (layer_ID);

#ifdef __LQR_CLOCK__
  clock2 = (double) clock() / CLOCKS_PER_SEC;
  printf ("[ resized: clock : %g (%g) ]\n", clock2, clock2 - clock1);
  fflush (stdout);
#endif // __LQR_CLOCK__


  MEMCHECK (lqr_external_writeimage (rasta, drawable));

  if (vals->resize_aux_layers == TRUE)
    {
      if (vals->pres_layer_ID != 0)
        {
          gimp_layer_resize (vals->pres_layer_ID, vals->new_width,
                             vals->new_height, x_off, y_off);
          MEMCHECK (lqr_external_writeimage (rasta->pres_raster,
                                   gimp_drawable_get (vals->pres_layer_ID)));
        }
      if (vals->disc_layer_ID != 0)
        {
          gimp_layer_resize (vals->disc_layer_ID, vals->new_width,
                             vals->new_height, x_off, y_off);
          MEMCHECK (lqr_external_writeimage (rasta->disc_raster,
                                   gimp_drawable_get (vals->disc_layer_ID)));
        }
    }

  lqr_raster_destroy (rasta);

#ifdef __LQR_CLOCK__
  clock3 = (double) clock() / CLOCKS_PER_SEC;
  printf ("[ finish: clock: %g (%g) ]\n", clock3, clock3 - clock2 );
#endif // __LQR_CLOCK__

  gimp_drawable_set_visible (layer_ID, TRUE);
  gimp_image_set_active_layer (image_ID, layer_ID);

  return TRUE;
}
