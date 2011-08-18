/* $Id$
 *
 * Buzztard
 * Copyright (C) 2011 Buzztard team <buzztard-devel@lists.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef BT_SETTINGS_PAGE_SHORTCUTS_H
#define BT_SETTINGS_PAGE_SHORTCUTS_H

#include <glib.h>
#include <glib-object.h>

#define BT_TYPE_SETTINGS_PAGE_SHORTCUTS            (bt_settings_page_shortcuts_get_type ())
#define BT_SETTINGS_PAGE_SHORTCUTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BT_TYPE_SETTINGS_PAGE_SHORTCUTS, BtSettingsPageShortcuts))
#define BT_SETTINGS_PAGE_SHORTCUTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), BT_TYPE_SETTINGS_PAGE_SHORTCUTS, BtSettingsPageShortcutsClass))
#define BT_IS_SETTINGS_PAGE_SHORTCUTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BT_TYPE_SETTINGS_PAGE_SHORTCUTS))
#define BT_IS_SETTINGS_PAGE_SHORTCUTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), BT_TYPE_SETTINGS_PAGE_SHORTCUTS))
#define BT_SETTINGS_PAGE_SHORTCUTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BT_TYPE_SETTINGS_PAGE_SHORTCUTS, BtSettingsPageShortcutsClass))

/* type macros */

typedef struct _BtSettingsPageShortcuts BtSettingsPageShortcuts;
typedef struct _BtSettingsPageShortcutsClass BtSettingsPageShortcutsClass;
typedef struct _BtSettingsPageShortcutsPrivate BtSettingsPageShortcutsPrivate;

/**
 * BtSettingsPageShortcuts:
 *
 * the root window for the editor application
 */
struct _BtSettingsPageShortcuts {
  GtkTable parent;
  
  /*< private >*/
  BtSettingsPageShortcutsPrivate *priv;
};
/* structure of the settings-dialog class */
struct _BtSettingsPageShortcutsClass {
  GtkTableClass parent;
  
};

/* used by SETTINGS_PAGE_SHORTCUTS_TYPE */
GType bt_settings_page_shortcuts_get_type(void) G_GNUC_CONST;

#endif // BT_SETTINGS_PAGE_SHORTCUTS_H