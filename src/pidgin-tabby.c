/*
 * Pidgin-libnotify - Provides a libnotify interface for Pidgin
 * Copyright (C) 2005-2007 Duarte Henriques
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

/* for pidgin_create_prpl_icon */
#include <gtkutils.h>

#include <string.h>

#define PLUGIN_ID "pidgin-tabby"

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

/* do NOT g_free() the string returned by this function */
static gchar *
best_name (PurpleBuddy *buddy)
{
	if (buddy->alias) {
		return buddy->alias;
	} else if (buddy->server_alias) {
		return buddy->server_alias;
	} else {
		return buddy->name;
	}
}

/* you must g_free the returned string
 * num_chars is utf-8 characters */
static gchar *
truncate_escape_string (const gchar *str,
						int num_chars)
{
	gchar *escaped_str;

	if (g_utf8_strlen (str, num_chars*2+1) > num_chars) {
		gchar *truncated_str;
		gchar *str2;

		/* allocate number of bytes and not number of utf-8 chars */
		str2 = g_malloc ((num_chars-1) * 2 * sizeof(gchar));

		g_utf8_strncpy (str2, str, num_chars-2);
		truncated_str = g_strdup_printf ("%s..", str2);
		escaped_str = g_markup_escape_text (truncated_str, strlen (truncated_str));
		g_free (str2);
		g_free (truncated_str);
	} else {
		escaped_str = g_markup_escape_text (str, strlen (str));
	}

	return escaped_str;
}

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

static PidginAutocomplete*
conversation_attach(PurpleConversation *conv)
{
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT && !conv->u.chat->left) {
		PidginAutocomplete *ac = pidgin_autocomplete_new();
		PURPLE_AUTOCOMPLETE_SET(conv, ac);
		return ac;
	}
	return NULL;
}

static void
conversation_detach(PurpleConversation *conv)
{
	PidginAutocomplete *ac = PURPLE_AUTOCOMPLETE_GET(conv);
	PURPLE_AUTOCOMPLETE_SET(conv, NULL);
	pidgin_autocomplete_free(ac);
}

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

static gboolean
plugin_load (PurplePlugin *plugin)
{
	void *conv_handle, *blist_handle, *conn_handle;

	conv_handle = purple_conversations_get_handle ();
	blist_handle = purple_blist_get_handle ();
	conn_handle = purple_connections_get_handle();

	/* Attach to all conversations */
	GList *convs;
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next) {
		PidginAutocomplete *ac;
		PurpleConversation *conv = (PurpleConversation*)convs->data;
		if ((ac = conversation_attach(conv))) {
			GList *u;
			for (u = conv->u.chat->in_room; u != NULL; u = u->next) {
				PurpleConvChatBuddy *b = (PurpleConvChatBuddy*)u->data;
				ac->said = g_list_insert_sorted(ac->said, g_strdup(b->name),
						(GCompareFunc)g_utf8_collate);
			}
		}
	}

	PurpleConversationUiOps *ops = pidgin_conversations_get_conv_ui_ops();
	old_rename = ops->chat_rename_user;
	ops->chat_rename_user = chat_buddy_rename;

	purple_signal_connect (conv_handle, "conversation-created", plugin,
						PURPLE_CALLBACK(conversation_attach), NULL);
	purple_signal_connect (conv_handle, "deleting-conversation", plugin,
						PURPLE_CALLBACK(conversation_detach), NULL);
	purple_signal_connect (conv_handle, "received-chat-msg", plugin,
						PURPLE_CALLBACK(received_chat_msg_cb), NULL);
	purple_signal_connect (conv_handle, "chat-buddy-joined", plugin,
						PURPLE_CALLBACK(chat_buddy_joined), NULL);
	purple_signal_connect (conv_handle, "chat-buddy-left", plugin,
						PURPLE_CALLBACK(chat_buddy_left), NULL);

	return TRUE;
}

static gboolean
plugin_unload (PurplePlugin *plugin)
{
	void *conv_handle, *blist_handle, *conn_handle;

	conv_handle = purple_conversations_get_handle ();
	blist_handle = purple_blist_get_handle ();
	conn_handle = purple_connections_get_handle();

	purple_signal_disconnect (conv_handle, "conversation-created", plugin,
							PURPLE_CALLBACK(conversation_attach));
	purple_signal_disconnect (conv_handle, "deleting-conversation", plugin,
							PURPLE_CALLBACK(conversation_detach));
	purple_signal_disconnect (conv_handle, "received-chat-msg", plugin,
							PURPLE_CALLBACK(received_chat_msg_cb));
	purple_signal_disconnect (conv_handle, "chat-buddy-joined", plugin,
							PURPLE_CALLBACK(chat_buddy_joined));
	purple_signal_disconnect (conv_handle, "chat-buddy-left", plugin,
							PURPLE_CALLBACK(chat_buddy_left));

	GList *convs;
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next)
		conversation_detach((PurpleConversation*)convs->data);

	PurpleConversationUiOps *ops = pidgin_conversations_get_conv_ui_ops();
	ops->chat_rename_user = old_rename;
	old_rename = NULL;

	return TRUE;
}

static PurplePluginUiInfo prefs_info = {
    get_plugin_pref_frame,
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
	info.name = _("A Tabby");
	info.summary = _("Better nick autocomplete in MUC.");
	info.description = _("Pidgin-tabby:\nAllows insert nicks in chat by clicking on them.");

	purple_prefs_add_none ("/plugins/gtk/tabby");
	purple_prefs_add_bool ("/plugins/gtk/tabby/click_on_nick", TRUE);
}

PURPLE_INIT_PLUGIN(notify, init_plugin, info)

