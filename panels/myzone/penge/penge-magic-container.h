/*
 *
 * Copyright (C) 2009, Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
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
 *
 */

#ifndef _PENGE_MAGIC_CONTAINER
#define _PENGE_MAGIC_CONTAINER

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define PENGE_TYPE_MAGIC_CONTAINER penge_magic_container_get_type()

#define PENGE_MAGIC_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PENGE_TYPE_MAGIC_CONTAINER, PengeMagicContainer))

#define PENGE_MAGIC_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), PENGE_TYPE_MAGIC_CONTAINER, PengeMagicContainerClass))

#define PENGE_IS_MAGIC_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PENGE_TYPE_MAGIC_CONTAINER))

#define PENGE_IS_MAGIC_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), PENGE_TYPE_MAGIC_CONTAINER))

#define PENGE_MAGIC_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), PENGE_TYPE_MAGIC_CONTAINER, PengeMagicContainerClass))

typedef struct {
  ClutterActor parent;
} PengeMagicContainer;

typedef struct {
  ClutterActorClass parent_class;
} PengeMagicContainerClass;

GType penge_magic_container_get_type (void);

ClutterActor *penge_magic_container_new (void);
void penge_magic_container_set_minimum_child_size (PengeMagicContainer *pmc,
                                                   gfloat               width,
                                                   gfloat               height);

G_END_DECLS

#endif /* _PENGE_MAGIC_CONTAINER */

/* penge-magic-container.c */

