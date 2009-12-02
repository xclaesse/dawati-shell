 /*
 * Copyright (C) 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _PENGE_PEOPLE_PLACEHOLDER_TILE
#define _PENGE_PEOPLE_PLACEHOLDER_TILE

#include <glib-object.h>
#include <mx/mx.h>

G_BEGIN_DECLS

#define PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE penge_people_placeholder_tile_get_type()

#define PENGE_PEOPLE_PLACEHOLDER_TILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE, PengePeoplePlaceholderTile))

#define PENGE_PEOPLE_PLACEHOLDER_TILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE, PengePeoplePlaceholderTileClass))

#define PENGE_IS_PEOPLE_PLACEHOLDER_TILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE))

#define PENGE_IS_PEOPLE_PLACEHOLDER_TILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE))

#define PENGE_PEOPLE_PLACEHOLDER_TILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), PENGE_TYPE_PEOPLE_PLACEHOLDER_TILE, PengePeoplePlaceholderTileClass))

typedef struct {
  MxButton parent;
} PengePeoplePlaceholderTile;

typedef struct {
  MxButtonClass parent_class;
} PengePeoplePlaceholderTileClass;

GType penge_people_placeholder_tile_get_type (void);

ClutterActor *penge_people_placeholder_tile_new (void);

G_END_DECLS

#endif /* _PENGE_PEOPLE_PLACEHOLDER_TILE */

