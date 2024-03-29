/*
 * Pidgin-wishmaster - unlocks fancy pidgin nick completion features
 * Copyright (C) 2010 kawaii.neko
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gln_intl.h"

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include <pidgin.h>
#include <version.h>
#include <debug.h>
#include <util.h>
#include <privacy.h>
#include <gtkimhtml.h>
#include <gdk/gdkkeysyms.h>

/* for pidgin_create_prpl_icon */
#include <gtkutils.h>

#include <string.h>

#define PLUGIN_ID "pidgin-wishmaster"

#undef SMARTTAB

#ifdef SMARTTAB
typedef struct _PidginAutocomplete PidginAutocomplete;
struct _PidginAutocomplete
{
	gboolean	tabbing;
	GList		*talked;			/* persons talked to you (or you talked to) */
	GList		*said;				/* recently talked persons */
	GList		**active;			/* active autocomplete list */
	GList		*position;			/* position in autocomplete list */
	char		*hint;
};

static PidginAutocomplete *
pidgin_autocomplete_new()
{
	return g_malloc0(sizeof(PidginAutocomplete));
}

static void
pidgin_autocomplete_free(PidginAutocomplete *ac)
{
	if (ac) {
		g_list_foreach(ac->talked, (GFunc)g_free, NULL);
		g_list_foreach(ac->said, (GFunc)g_free, NULL);
		g_free(ac->hint);
		g_free(ac);
	}
}

static GList *
pidgin_autocomplete_pop_entry(GList *list, const char *name)
{
	GList *entry = g_list_find_custom(list, name, (GCompareFunc)strcmp);
	if (entry) {
		if (entry != list) {
			/* Bring existing entry to head */
			list = g_list_remove_link(list, entry);
			entry->next = list;
			list->prev = entry;
			return entry;
		}
		return list;
	} else {
		return g_list_prepend(list, g_strdup(name));
	}
}

static void
pidgin_autocomplete_set_hint(PidginAutocomplete *ac, const char *hint)
{
	if (ac->hint)
		g_free(ac->hint);
	ac->hint = hint ? g_strdup(hint) : NULL;
}

static void
pidgin_autocomplete_add_said(PidginAutocomplete *ac, const char *name)
{
	ac->said = pidgin_autocomplete_pop_entry(ac->said, name);
}

static void
pidgin_autocomplete_add_talked(PidginAutocomplete *ac, const char *name)
{
	ac->talked = pidgin_autocomplete_pop_entry(ac->talked, name);
}

static void
pidgin_autocomplete_remove_user(PidginAutocomplete *ac, const char *name)
{
	GList *entry = g_list_find_custom(ac->said, name, (GCompareFunc)strcmp);
	if (entry) {
		if (ac->position == entry)
			ac->position = entry->prev;
		g_free(entry->data);
		ac->said = g_list_delete_link(ac->said, entry);
	}
	entry = g_list_find_custom(ac->talked, name, (GCompareFunc)strcmp);
	if (entry) {
		if (ac->position == entry)
			ac->position = entry->prev;
		g_free(entry->data);
		ac->talked = g_list_delete_link(ac->talked, entry);
	}
}

static void
pidgin_autocomplete_update_user(PidginAutocomplete *ac, const char *old_name, const char *new_name)
{
	GList *entry = g_list_find_custom(ac->said, old_name, (GCompareFunc)strcmp);
	if (entry) {
		g_free(entry->data);
		entry->data = g_strdup(new_name);
	}
	entry = g_list_find_custom(ac->talked, old_name, (GCompareFunc)strcmp);
	if (entry) {
		g_free(entry->data);
		entry->data = g_strdup(new_name);
	}
}
#endif

static PurplePluginPrefFrame *
get_plugin_pref_frame (PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new ();

	ppref = purple_plugin_pref_new_with_name_and_label (
                            "/plugins/gtk/tabby/click_on_nick",
                            _("Click on nick to insert it into conversation window"));
	purple_plugin_pref_frame_add (frame, ppref);

	return frame;
}

#ifdef SMARTTAB
#define PURPLE_AUTOCOMPLETE_GET(conv) \
	((PidginAutocomplete*)purple_conversation_get_data(conv, "tabby-autocomplete"))

#define PURPLE_AUTOCOMPLETE_SET(conv, data) \
	purple_conversation_set_data(conv, "tabby-autocomplete", data)

static void
received_chat_msg_cb(PurpleAccount *account, char *sender, char *message,
							PurpleConversation *conv, PurpleMessageFlags flags)
{
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	if (ac && sender && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT
			&& strcmp(sender, conv->u.chat->nick) != 0) {
		if (flags & PURPLE_MESSAGE_NICK)
			pidgin_autocomplete_pop_entry(ac->talked, sender);
		if (!(flags & PURPLE_MESSAGE_ERROR))
			pidgin_autocomplete_pop_entry(ac->said, sender);
	}
}

static void
chat_buddy_joined(PurpleConversation *conv, const char *name,
							PurpleConvChatBuddyFlags flags,
							gboolean new_arrival)
{
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	if (ac && name && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT
			&& strcmp(name, conv->u.chat->nick) != 0)
		pidgin_autocomplete_pop_entry(ac->said, name);
}

static void
chat_buddy_left(PurpleConversation *conv, const char *name,
							const char *reason)
{
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	if (ac)
		pidgin_autocomplete_remove_user(ac, name);
}
#endif

static gboolean
insert_nick_do(PidginConversation *gtkconv, const char *who)
{
	PurpleConversation *conv = gtkconv->active_conv;
	char *tmp, *nick = purple_conversation_get_data(conv, "tabby-last");
	if (who) {
		g_free(nick);
		nick = g_strdup(who);
		purple_conversation_set_data(conv, "tabby-last", nick);
	}
	if (!nick)
		return FALSE;
	tmp = g_strdup_printf("%s: ", nick);
	gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, tmp, -1);
	g_free(tmp);
	gtk_widget_grab_focus(gtkconv->entry);
	return TRUE;
}

static gint
button_press(GtkWidget *widget, GdkEventButton *event,
				PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	gchar *who;
	int x, y;

	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = account->gc;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(gtkchat->list),
								  event->x, event->y, &path, &column, &x, &y);

	if (path == NULL)
		return FALSE;

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list))), path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(gtkchat->list),
							 path, NULL, FALSE);
	gtk_widget_grab_focus(GTK_WIDGET(gtkchat->list));

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	if (event->button == 1) {
		insert_nick_do(gtkconv, who);
		g_free(who);
		gtk_tree_path_free(path);
		return TRUE;
	}

	g_free(who);
	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean
entry_key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
	switch (event->keyval) {
		case GDK_r:
			if (event->state & GDK_CONTROL_MASK)
				printf("C-r\n");
			break;
		case GDK_Tab:
		case GDK_KP_Tab:
		case GDK_ISO_Left_Tab: {
			GtkTextIter start_buffer, cursor;
			PidginConversation *gtkconv = (PidginConversation*)data;
			gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
			gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
					gtk_text_buffer_get_insert(gtkconv->entry_buffer));
			/* we're at the start of entry */
			if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
				return insert_nick_do(gtkconv, NULL);
			}
	   }
	}
	
	return FALSE;
}

#ifdef SMARTTAB
static PidginAutocomplete*
#else
static void
#endif
conversation_attach(PurpleConversation *conv)
{
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT && !conv->u.chat->left) {
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
		PidginChatPane *gtkchat = gtkconv->u.chat;
#ifdef SMARTTAB
		PidginAutocomplete *ac = pidgin_autocomplete_new();
		PURPLE_AUTOCOMPLETE_SET(conv, ac);
#endif
		g_signal_connect(G_OBJECT(gtkchat->list), "button-release-event",
						G_CALLBACK(button_press), gtkconv);
		g_signal_connect(G_OBJECT(gtkconv->entry), "key-release-event",
						G_CALLBACK(entry_key_press_cb), gtkconv);
#ifdef SMARTTAB
		return ac;
#endif
	}
#ifdef SMARTTAB
	return NULL;
#endif
}

static void
conversation_detach(PurpleConversation *conv)
{
	char *nick = purple_conversation_get_data(conv, "tabby-last");
	if (nick) {
		g_free(nick);
		purple_conversation_set_data(conv, "tabby-last", NULL);
	}
#ifdef SMARTTAB
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	PURPLE_AUTOCOMPLETE_SET(conv, NULL);
	pidgin_autocomplete_free(ac);
#endif
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT && !conv->u.chat->left) {
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
		PidginChatPane *gtkchat = gtkconv->u.chat;
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkchat->list),
						G_CALLBACK(button_press), gtkconv);
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtkconv->entry),
						G_CALLBACK(entry_key_press_cb), gtkconv);
	}
}

#ifdef SMARTTAB
static void (*old_rename)(PurpleConversation *conv, const char *old_name,
				 const char *new_name, const char *new_alias) = NULL;

static void
chat_buddy_rename(PurpleConversation *conv, const char *old_name,
							const char *new_name, const char *new_alias)
{
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	if (ac && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT
			&& strcmp(old_name, conv->u.chat->nick) != 0)
		pidgin_autocomplete_update_user(ac, old_name, new_name);
	if (old_rename)
		old_rename(conv, old_name, new_name, new_alias);
}
#endif

static gboolean tag_hack(GtkTextTag *tag, GObject *imhtml,
		GdkEvent *event, GtkTextIter *arg2, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS && ((GdkEventButton*)event)->button == 1) {
		PurpleConversation *conv = data;
		char *buddyname;

		/* strlen("BUDDY " or "HILIT ") == 6 */
		g_return_val_if_fail((tag->name != NULL)
				&& (strlen(tag->name) > 6), FALSE);

		buddyname = (tag->name) + 6;

		insert_nick_do(PIDGIN_CONVERSATION(conv), buddyname);
		return TRUE;
	}

	return FALSE;
}

static void
chat_hack(PurpleAccount *account, const char *who, char *displaying,
						PurpleConversation *conv, PurpleMessageFlags flags)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	GtkTextTag *buddytag;
	gchar *str;
	gboolean highlight = (flags & PURPLE_MESSAGE_NICK);
	GtkTextBuffer *buffer = GTK_IMHTML(gtkconv->imhtml)->text_buffer;

	str = g_strdup_printf(highlight ? "HILIT %s" : "BUDDY %s", who);

	buddytag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(buffer), str);

	if (buddytag) {
		if (!g_object_get_data(G_OBJECT(buddytag), "ntfy")) {
			g_object_set_data(G_OBJECT(buddytag), "ntfy", "1");
			g_signal_connect(G_OBJECT(buddytag), "event",
					G_CALLBACK(tag_hack), conv);
		}
	}
	g_free(str);
}

static gboolean
plugin_load (PurplePlugin *plugin)
{
	void *conv_handle;

	conv_handle = purple_conversations_get_handle ();

	/* Attach to all conversations */
	GList *convs;
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next) {
		PurpleConversation *conv = (PurpleConversation*)convs->data;
#ifdef SMARTTAB
		PidginAutocomplete *ac;
		if ((ac = conversation_attach(conv))) {
			GList *u;
			for (u = conv->u.chat->in_room; u != NULL; u = u->next) {
				PurpleConvChatBuddy *b = (PurpleConvChatBuddy*)u->data;
				ac->said = g_list_insert_sorted(ac->said, g_strdup(b->name),
						(GCompareFunc)g_utf8_collate);
			}
		}
#else
		conversation_attach(conv);
#endif
	}

#ifdef SMARTTAB
	PurpleConversationUiOps *ops = pidgin_conversations_get_conv_ui_ops();
	old_rename = ops->chat_rename_user;
	ops->chat_rename_user = chat_buddy_rename;
#endif

	purple_signal_connect (conv_handle, "conversation-created", plugin,
						PURPLE_CALLBACK(conversation_attach), NULL);
	purple_signal_connect (conv_handle, "deleting-conversation", plugin,
						PURPLE_CALLBACK(conversation_detach), NULL);
#ifdef SMARTTAB
	purple_signal_connect (conv_handle, "received-chat-msg", plugin,
						PURPLE_CALLBACK(received_chat_msg_cb), NULL);
	purple_signal_connect (conv_handle, "chat-buddy-joined", plugin,
						PURPLE_CALLBACK(chat_buddy_joined), NULL);
	purple_signal_connect (conv_handle, "chat-buddy-left", plugin,
						PURPLE_CALLBACK(chat_buddy_left), NULL);
#endif
	purple_signal_connect (pidgin_conversations_get_handle(),
						"displayed-chat-msg", plugin,
						PURPLE_CALLBACK(chat_hack), NULL);

	return TRUE;
}

static gboolean
plugin_unload (PurplePlugin *plugin)
{
	void *conv_handle;

	conv_handle = purple_conversations_get_handle ();

	purple_signal_disconnect (conv_handle, "conversation-created", plugin,
							PURPLE_CALLBACK(conversation_attach));
	purple_signal_disconnect (conv_handle, "deleting-conversation", plugin,
							PURPLE_CALLBACK(conversation_detach));
#ifdef SMARTTAB
	purple_signal_disconnect (conv_handle, "received-chat-msg", plugin,
							PURPLE_CALLBACK(received_chat_msg_cb));
	purple_signal_disconnect (conv_handle, "chat-buddy-joined", plugin,
							PURPLE_CALLBACK(chat_buddy_joined));
	purple_signal_disconnect (conv_handle, "chat-buddy-left", plugin,
							PURPLE_CALLBACK(chat_buddy_left));
#endif
	purple_signal_disconnect (pidgin_conversations_get_handle(),
							"displayed-chat-msg", plugin,
							PURPLE_CALLBACK(chat_hack));

	GList *convs;
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next)
		conversation_detach((PurpleConversation*)convs->data);

#ifdef SMARTTAB
	PurpleConversationUiOps *ops = pidgin_conversations_get_conv_ui_ops();
	ops->chat_rename_user = old_rename;
	old_rename = NULL;
#endif

	return TRUE;
}

static PurplePluginUiInfo prefs_info = {
    NULL, //get_plugin_pref_frame,
    0,						/* page num (Reserved) */
    NULL					/* frame (Reserved) */
};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,										/* api version */
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_STANDARD,									/* type */
    0,														/* ui requirement */
    0,														/* flags */
    NULL,													/* dependencies */
    PURPLE_PRIORITY_DEFAULT,									/* priority */
    
    PLUGIN_ID,												/* id */
    NULL,													/* name */
    VERSION,												/* version */
    NULL,													/* summary */
    NULL,													/* description */
    
    "kawaii neko <kawaii.neko@pochta.ru>",		/* author */
    "http://radioanon.ru",		/* homepage */
    
    plugin_load,			/* load */
    plugin_unload,			/* unload */
    NULL,					/* destroy */
    NULL,					/* ui info */
    NULL,					/* extra info */
    &prefs_info				/* prefs info */
};

static void
init_plugin (PurplePlugin *plugin)
{
#if 0
	bindtextdomain (PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
#endif
	info.name = _("Wishmaster");
	info.summary = _("Better nick autocomplete in MUC.");
	info.description = _("Wishmaster:\nAllows insert nicks in chat by clicking on them.");

	purple_prefs_add_none ("/plugins/gtk/wishmaster");
	purple_prefs_add_bool ("/plugins/gtk/wishmaster/click_on_nick", TRUE);
}

PURPLE_INIT_PLUGIN(notify, init_plugin, info)

