/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Intel Corp.
 *
 * Author: Thomas Wood <thomas@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _MNB_NOTIFICATION_CLUSTER
#define _MNB_NOTIFICATION_CLUSTER

#include <glib-object.h>
#include <clutter/clutter.h>
#include <mx/mx.h>

#include "moblin-netbook-notify-store.h"

G_BEGIN_DECLS

#define MNB_TYPE_NOTIFICATION_CLUSTER mnb_notification_cluster_get_type()

#define MNB_NOTIFICATION_CLUSTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MNB_TYPE_NOTIFICATION_CLUSTER, MnbNotificationCluster))

#define MNB_NOTIFICATION_CLUSTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MNB_TYPE_NOTIFICATION_CLUSTER, MnbNotificationClusterClass))

#define MNB_IS_NOTIFICATION_CLUSTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MNB_TYPE_NOTIFICATION_CLUSTER))

#define MNB_IS_NOTIFICATION_CLUSTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MNB_TYPE_NOTIFICATION_CLUSTER))

#define MNB_NOTIFICATION_CLUSTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MNB_TYPE_NOTIFICATION_CLUSTER, MnbNotificationClusterClass))

typedef struct _MnbNotificationClusterPrivate MnbNotificationClusterPrivate;

typedef struct {
  MxTable parent;
  /*< private >*/
  MnbNotificationClusterPrivate *priv;
} MnbNotificationCluster;

typedef struct {
  MxTableClass parent_class;

  void (*sync_input_region) (MnbNotificationCluster *cluster);

} MnbNotificationClusterClass;

GType mnb_notification_cluster_get_type (void);

ClutterActor* mnb_notification_cluster_new (void);

void
mnb_notification_cluster_set_store (MnbNotificationCluster    *self,
                                    MoblinNetbookNotifyStore  *notify_store);

G_END_DECLS

#endif /* _MNB_NOTIFICATION_CLUSTER */


