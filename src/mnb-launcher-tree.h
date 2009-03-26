/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2009 Intel Corp.
 *
 * Author: Robert Staudinger <robertx.staudinger@intel.com>
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

#ifndef MNB_LAUNCHER_TREE_H
#define MNB_LAUNCHER_TREE_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * MnbLauncherMonitor to monitor menu changes.
 */
typedef struct MnbLauncherMonitor_ MnbLauncherMonitor;

typedef void (*MnbLauncherMonitorFunction) (MnbLauncherMonitor *monitor,
                                            gpointer            user_data);

void mnb_launcher_monitor_free (MnbLauncherMonitor *monitor);

/*
 * MnbLauncherTree represents the entire menu hierarchy.
 */

typedef struct MnbLauncherTree_ MnbLauncherTree;

MnbLauncherTree *     mnb_launcher_tree_create          (void);
GSList *              mnb_launcher_tree_list_entries    (MnbLauncherTree            *tree);
MnbLauncherMonitor *  mnb_launcher_tree_create_monitor  (MnbLauncherTree            *tree,
                                                         MnbLauncherMonitorFunction  monitor_function,
                                                         gpointer                    user_data);
void                  mnb_launcher_tree_free_entries    (GSList                     *entries);
void                  mnb_launcher_tree_free            (MnbLauncherTree            *tree);

/*
 * MnbLauncherDirectory represents a "folder" item in the main menu.
 */
typedef struct {
  gchar   *name;
  GSList  *entries;
} MnbLauncherDirectory;

/*
 * MnbLauncherEntry represents a "launcher" item in the main menu.
 */
typedef struct MnbLauncherEntry_ MnbLauncherEntry;

MnbLauncherEntry *  mnb_launcher_entry_create                 (const gchar *desktop_file_path);
void                mnb_launcher_entry_free                   (MnbLauncherEntry *entry);

const gchar *       mnb_launcher_entry_get_name               (MnbLauncherEntry *entry);
gchar *             mnb_launcher_entry_get_exec               (MnbLauncherEntry *entry);
gchar *             mnb_launcher_entry_get_icon               (MnbLauncherEntry *entry);
gchar *             mnb_launcher_entry_get_comment            (MnbLauncherEntry *entry);
const gchar *       mnb_launcher_entry_get_desktop_file_path  (MnbLauncherEntry *entry);

/*
 * Utils.
 */

gchar * mnb_launcher_utils_get_last_used (const gchar *executable);

G_END_DECLS

#endif /* MNB_LAUNCHER_TREE_H */
