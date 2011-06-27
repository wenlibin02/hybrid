#include <glib.h>
#include "util.h"
#include "config.h"
#include "account.h"
#include "gtkutils.h"
#include "notify.h"
#include "blist.h"

GSList *account_list = NULL;

extern HybridBlist *blist;

static void hybrid_account_icon_save(HybridAccount *account);
static void account_set_icon(HybridAccount *account, const guchar *icon_data,
		gint icon_data_len, const gchar *icon_crc);

static void load_blist_from_disk(HybridAccount *account);

void
hybrid_account_init(void)
{
	gchar *account_file;
	gchar *config_path;
	xmlnode *root;
	xmlnode *node;
	HybridAccount *account;
	gboolean flush = FALSE;

	gchar *username;
	gchar *protoname;
	gchar *password;
	gchar *value;
	gchar *icon_data;
	gsize icon_data_len;
	gchar *icon_path;
	gchar *crc;
	HybridModule *module;

	config_path = hybrid_config_get_path();
	account_file = g_strdup_printf("%s/accounts.xml", config_path);

	if (!(root = xmlnode_root_from_file(account_file))) {
		const gchar *root_name = "<accounts></accounts>";
		hybrid_debug_info("account", "accounts.xml doesn't exist or"
				"in bad format, create a new one.");
		root = xmlnode_root(root_name, strlen(root_name));
		flush ^= 1;
	}

	if ((node = xmlnode_child(root))) {

		while (node) {
			/*
			 * The name of the current node must be 'account', and
			 * it must has two nodes with the name 'user' and 'proto'
			 * respectively, otherwise the node is invalid. 
			 */
			if (g_strcmp0(node->name, "account") ||
				!xmlnode_has_prop(node, "user") ||
				!xmlnode_has_prop(node, "proto")) {
				hybrid_debug_error("account", "accounts.xml is in bad format,"
						"please try to remove ~/.config/hybrid/accounts.xml,"
						"and then restart hybrid :)");
				xmlnode_free(root);
				g_free(config_path);
				return;
			}

			username = xmlnode_prop(node, "user");
			protoname = xmlnode_prop(node, "proto");

			module = hybrid_module_find(protoname);

			account = hybrid_account_create(module);
			hybrid_account_set_username(account, username);

			/* load password */
			if (xmlnode_has_prop(node, "pass")) {
				password = xmlnode_prop(node, "pass");
				hybrid_account_set_password(account, password);
				g_free(password);
			}

			g_free(username);
			g_free(protoname);

			/* load the icon data. */
			if (xmlnode_has_prop(node, "icon")) {
				value = xmlnode_prop(node, "icon");
				if (*value != '\0') {
					icon_path = g_strdup_printf("%s/icons/%s",
							config_path, value);
					g_file_get_contents(icon_path, &icon_data,
							&icon_data_len, NULL);
					g_free(icon_path);

					if (xmlnode_has_prop(node, "crc")) {
						crc = xmlnode_prop(node, "crc");

					} else {
						crc = NULL;
					}

					account_set_icon(account, (guchar*)icon_data,
							icon_data_len, crc);

					g_free(crc);
					g_free(icon_data);

				}

				g_free(value);
			}

			/* load the nickname */
			if (xmlnode_has_prop(node, "nam")) {
				value = xmlnode_prop(node, "name");
				if (*value != '\0') {
					hybrid_account_set_nickname(account, value);
				}

				g_free(value);
			}

			account_list = g_slist_append(account_list, account);
				
			node = node->next;
		}
	}

	if (flush) {
		xmlnode_save_file(root, account_file);
	}

	g_free(config_path);
	xmlnode_free(root);
}

HybridAccount*
hybrid_account_get(const gchar *proto_name,	const gchar *username)
{
	HybridAccount *account;
	HybridModule *module;
	GSList *pos;

	for (pos = account_list; pos; pos = pos->next) {
		account = (HybridAccount*)pos->data;

		if (g_strcmp0(account->username, username) == 0 &&
			g_strcmp0(account->proto->info->name, proto_name) == 0) {

			return account;
		}
	}

	/* Well, the account we're finding doesn't exist, then we create one. */

	if (!(module = hybrid_module_find(proto_name))) {
		hybrid_debug_error("account", "create account error,"
				"invalid protocal name.");
		return NULL;
	}

	account = hybrid_account_create(module);
	hybrid_account_set_username(account, username);

	account_list = g_slist_append(account_list, account);

	return account;
}

void
hybrid_account_update(HybridAccount *account)
{
	gchar *account_file;
	gchar *config_path;
	xmlnode *root;
	xmlnode *node;
	gchar *username;
	gchar *protoname;

	config_path = hybrid_config_get_path();
	account_file = g_strdup_printf("%s/accounts.xml", config_path);
	g_free(config_path);

	if (!(root = xmlnode_root_from_file(account_file))) {
		hybrid_debug_error("account", "save account information failed,"
				"invalid accounts.xml");
		g_free(account_file);
		return;
	}

	if ((node = xmlnode_child(root))) {

		while (node) {
			if (g_strcmp0(node->name, "account") ||
				!xmlnode_has_prop(node, "user") ||
				!xmlnode_has_prop(node, "proto")) {
				hybrid_debug_error("account", 
						"invalid node found in accounts.xml");
				node = xmlnode_next(node);

				continue;
			}

			username = xmlnode_prop(node, "user");
			protoname = xmlnode_prop(node, "proto");

			if (g_strcmp0(username, account->username) == 0 &&
				g_strcmp0(protoname, account->proto->info->name) == 0) {
				goto update_node;
			}
			
			node = xmlnode_next(node);
		}
	}
	
	/* Node not found, create a new one. */
	node = xmlnode_new_child(root, "account");
update_node:

	if (xmlnode_has_prop(node, "proto")) {
		xmlnode_set_prop(node, "proto", account->proto->info->name);

	} else {
		xmlnode_new_prop(node, "proto", account->proto->info->name);
	}

	if (xmlnode_has_prop(node, "user")) {
		xmlnode_set_prop(node, "user", account->username);

	} else {
		xmlnode_new_prop(node, "user", account->username);
	}

	if (xmlnode_has_prop(node, "pass")) {
		xmlnode_set_prop(node, "pass", account->password);

	} else {
		xmlnode_new_prop(node, "pass", account->password);
	}

	if (xmlnode_has_prop(node, "icon")) {
		xmlnode_set_prop(node, "icon", account->icon_name);

	} else {
		xmlnode_new_prop(node, "icon", account->icon_name);
	}

	if (xmlnode_has_prop(node, "name")) {
		xmlnode_set_prop(node, "name", account->nickname);

	} else {
		xmlnode_new_prop(node, "name", account->nickname);
	}

	if (xmlnode_has_prop(node, "crc")) {
		xmlnode_set_prop(node, "crc", account->icon_crc);

	} else {
		xmlnode_new_prop(node, "crc", account->icon_crc);
	}

	xmlnode_save_file(root, account_file);

	g_free(account_file);
	xmlnode_free(root);
}

void
hybrid_account_remove(const gchar *protoname, const gchar *username)
{
	gchar *account_file;
	gchar *config_path;
	HybridAccount *account;
	xmlnode *root;
	xmlnode *node;
	GSList *pos;
	gchar *user_name;
	gchar *proto_name;

	/* Remove the revelent node from accounts.xml */
	config_path = hybrid_config_get_path();
	account_file = g_strdup_printf("%s/accounts.xml", config_path);
	g_free(config_path);

	if (!(root = xmlnode_root_from_file(account_file))) {
		hybrid_debug_error("account", "remove account information failed,"
				"invalid accounts.xml");
		g_free(account_file);
		return;
	}

	if ((node = xmlnode_child(root))) {

		while (node) {
			if (g_strcmp0(node->name, "account") ||
				!xmlnode_has_prop(node, "user") ||
				!xmlnode_has_prop(node, "proto")) {
				hybrid_debug_error("account", 
						"invalid node found in accounts.xml");
				node = xmlnode_next(node);

				continue;
			}

			user_name = xmlnode_prop(node, "user");
			proto_name = xmlnode_prop(node, "proto");

			if (g_strcmp0(user_name, username) == 0 &&
				g_strcmp0(proto_name, proto_name) == 0) {
				xmlnode_remove_node(node);
				xmlnode_save_file(root, account_file);
				break;
			}
			
			node = xmlnode_next(node);
		}
	}

	g_free(account_file);
	xmlnode_free(root);

	/* Remove the account from the global account list. */
	for (pos = account_list; pos; pos = pos->next) {
		account = (HybridAccount*)pos->data;

		if (g_strcmp0(account->username, username) == 0 &&
			g_strcmp0(account->proto->info->name, protoname) == 0) {

			account_list = g_slist_remove(account_list, account);
			hybrid_account_destroy(account);

			break;
		}
	}
}

HybridAccount*
hybrid_account_create(HybridModule *proto)
{
	extern HybridConfig *global_config;
	g_return_val_if_fail(proto != NULL, NULL);

	HybridAccount *ac = g_new0(HybridAccount, 1);
	
	ac->buddy_list = g_hash_table_new(g_str_hash, g_str_equal);
	ac->group_list = g_hash_table_new(g_str_hash, g_str_equal);
	ac->config = global_config;
	ac->proto = proto;

	return ac;
}

void
hybrid_account_destroy(HybridAccount *account)
{
	if (account) {
		g_free(account->username);
		g_free(account->password);
		g_free(account->nickname);
		g_free(account->icon_data);
		g_free(account->icon_crc);
		g_free(account->icon_name);
		g_free(account);
	}
}

gpointer
hybrid_account_get_protocol_data(HybridAccount *account)
{
	return account->protocol_data;
}

void
hybrid_account_set_protocol_data(HybridAccount *account,
		gpointer protocol_data)
{
	account->protocol_data = protocol_data;
}

void 
hybrid_account_set_username(HybridAccount *account, const gchar *username)
{
	g_return_if_fail(account != NULL);

	g_free(account->username);

	account->username = g_strdup(username);
}

void 
hybrid_account_set_password(HybridAccount *account, const gchar *password)
{
	g_return_if_fail(account != NULL);

	g_free(account->password);

	account->password = g_strdup(password);
}

void
hybrid_account_set_state(HybridAccount *account, gint state)
{
	gchar *menu_name;
	GdkPixbuf *presence_pixbuf;
	GtkWidget *presence_image;

	g_return_if_fail(account != NULL);

	account->state = state;

	/* 
	 * This function will do something more, that is to set
	 * the icon and name of the account menu to make to 
	 * show the current state .
	 */
	menu_name = g_strdup_printf("%s (%s)", account->username, 
					hybrid_get_presence_name(state));
	presence_pixbuf = hybrid_create_presence_pixbuf(state, 16);
	presence_image = gtk_image_new_from_pixbuf(presence_pixbuf);

	gtk_menu_item_set_label(GTK_MENU_ITEM(account->account_menu), menu_name);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(account->account_menu),
					presence_image);

	g_free(menu_name);
	g_object_unref(presence_pixbuf);
}

void
hybrid_account_set_nickname(HybridAccount *account, const gchar *nickname)
{
	g_return_if_fail(account != NULL);

	g_free(account->nickname);

	account->nickname = g_strdup(nickname);
}

const gchar*
hybrid_account_get_checksum(HybridAccount *account)
{
	return account->icon_crc;
}

void
hybrid_account_set_icon(HybridAccount *account, const guchar *icon_data,
		gint icon_data_len, const gchar *icon_crc)
{
	g_return_if_fail(account != NULL);

	/* Just set the attribute. */
	account_set_icon(account, icon_data, icon_data_len, icon_crc);

	/* Save the icon the local fs. */
	hybrid_account_icon_save(account);

	/* Save to the local xml cache file. */
	hybrid_account_update(account);
}

void
hybrid_account_close(HybridAccount *account)
{
	GHashTableIter hash_iter;
	HybridGroup *group;
	HybridBuddy *buddy;
	HybridModule *module;
	gpointer key;
	GtkTreeModel *model;

	g_return_if_fail(account != NULL);

	/*
	 * Set the account' state to be offline, it'll also
	 * change the appearance of the account menus 
	 */
	hybrid_account_set_state(account, HYBRID_STATE_OFFLINE);

	/*
	 * Set the account's state to be closed.
	 */
	hybrid_account_set_connection_status(account, HYBRID_CONNECTION_CLOSED);

	/*
	 * Remove the keep alive event source.
	 */
	g_source_remove(account->keep_alive_source);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(blist->treeview));

	g_hash_table_iter_init(&hash_iter, account->group_list);
	while (g_hash_table_iter_next(&hash_iter, &key, (gpointer*)&group)) {
		/*
		 * Remove the group in blist, it will remove the
		 * buddies belongs to these groups, free the GUI
		 * resource automaticly.
		 */
		gtk_tree_store_remove(GTK_TREE_STORE(model), &group->iter);

		/* Free the memory of the HybridGroups in hashtable */
		hybrid_blist_group_destroy(group);
	}
	/*
	 * Note that we only remove all the item from the hashtable
	 * but not destroy it, so is the buddy_list hashtable 
	 */
	g_hash_table_remove_all(account->group_list);

	g_hash_table_iter_init(&hash_iter, account->buddy_list);
	while (g_hash_table_iter_next(&hash_iter, &key, (gpointer*)&buddy)) {
		/* Free the memory of the HybridBuddys in hashtable */
		hybrid_blist_buddy_destroy(buddy);
	}
	
	g_hash_table_remove_all(account->buddy_list);

	/*
	 * There's NO need to destroy the account here, we need it
	 * in the account management panel, or to restart it again.
	 */
	module = account->proto;

	if (module->info->close) {
		module->info->close(account);
	}
}

void
hybrid_account_error_reason(HybridAccount *account, const gchar *reason)
{
	HybridNotify *notify;

	g_return_if_fail(account != NULL);
	g_return_if_fail(reason != NULL);

	/* Sure, we should close the account first. */
	hybrid_account_close(account);

	/* Popup an notification box. */
	notify = hybrid_notify_create(account, NULL);
	hybrid_notify_set_text(notify, reason);
}

static gboolean
keep_alive_cb(HybridAccount *account)
{
	HybridModule *module;

	g_return_val_if_fail(account != NULL, FALSE);

	module = account->proto;

	if (module->info->keep_alive) {
		return module->info->keep_alive(account);
	}

	return TRUE;
}

void
hybrid_account_set_connection_status(HybridAccount *account,
		HybridConnectionStatusType status)
{
	if (account->connect_state != HYBRID_CONNECTION_CONNECTED &&
		status == HYBRID_CONNECTION_CONNECTED) {

		/* 
		 * It's necessary to set the status value here, because if
		 * connection status is not HYBRID_CONNECTION_CONNECTED, 
		 * hybrid_blist_add_buddy() just return without doing anything,
		 * which will cause load_blist_from_disk() fail to add buddies.
		 */
		account->connect_state = status;

		/* here we load the blist from the local cache file. */
		load_blist_from_disk(account);

		/*
		 * Now we start keep alive thread, the keep alive hook function
		 * was called every 20s.
		 */
		account->keep_alive_source = 
			g_timeout_add_seconds(20, (GSourceFunc)keep_alive_cb, account);
	}

	account->connect_state = status;
}


const gchar*
hybrid_get_presence_name(gint presence_state)
{
	/* The human readable presence names. */
	const gchar *presence_names[] = {
		N_("Offline"),
		N_("Invisible"),
		N_("Busy"),
		N_("Away"),
		N_("Online")
	};

	return presence_names[presence_state];
}

/**
 * You may ask why we need a new set_buddy_icon function since
 * we alreay have hybrid_blist_set_buddy_icon() defined. The reason
 * is that hybrid_blist_set_buddy_icon() should be used only when the
 * buddy changed its icon from the server, and it will stored the new
 * data into the disk, which would take much IO resouce, and even make the
 * UI stuck for a while when data is large, so we need a new function
 * here to just load the icon from disk, but not store it back again.
 */
static void
blist_set_buddy_icon(HybridBuddy *buddy,
	const guchar *icon_data, gsize len, const gchar *crc)
{
	GdkPixbuf *pixbuf = NULL;

	g_return_if_fail(buddy != NULL);

	g_free(buddy->icon_crc);
	g_free(buddy->icon_data);

	buddy->icon_data = NULL;
	buddy->icon_data_length = len;
	buddy->icon_crc = g_strdup(crc);

	if (icon_data != NULL) {
		buddy->icon_data = g_memdup(icon_data, len);
	}

	/* set the portrait */
	gint scale_size = 32;

	if (buddy->icon_data && buddy->icon_data_length != 0) {
		pixbuf = hybrid_create_round_pixbuf(buddy->icon_data, 
				buddy->icon_data_length, scale_size);
	}

	/* Here the status if offline. */
	if (pixbuf) {
		gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

		gtk_tree_store_set(blist->treemodel, &buddy->iter,
				HYBRID_BLIST_BUDDY_ICON, pixbuf, -1);

		g_object_unref(pixbuf);
	}
}

static void
load_blist_from_disk(HybridAccount *account)
{
	HybridConfig *config;
	HybridBlistCache *cache;
	HybridGroup *group;
	HybridBuddy *buddy;
	xmlnode *root;
	xmlnode *node;
	xmlnode *group_node;
	xmlnode *buddy_node;
	xmlnode *account_node;
	gchar *id;
	gchar *name;
	gchar *value;
	guchar *icon_data;
	gsize icon_data_len;


	g_return_if_fail(account != NULL);

	config = account->config;
	cache = config->blist_cache;
	root = cache->root;

	if (!(node = xmlnode_find(root, "accounts"))) {
		hybrid_debug_error("account", 
			"can't find node named 'buddies' in blist.xml");
		return;
	}

	for (account_node = xmlnode_child(node); account_node;
			account_node = account_node->next) {
		if (g_strcmp0(account_node->name, "account")) {
			hybrid_debug_error("account", 
				"invalid blist.xml");
			return;
		}

		if (!xmlnode_has_prop(account_node, "proto") ||
			!xmlnode_has_prop(account_node, "username")) {
			hybrid_debug_error("account", 
				"invalid account node in blist.xml");
			continue;
		}
		name = xmlnode_prop(account_node, "username");
		value = xmlnode_prop(account_node, "proto");

		if (g_strcmp0(name, account->username) == 0 &&
			g_strcmp0(value, account->proto->info->name) == 0) {
			node = xmlnode_find(account_node, "buddies");
			g_free(name);
			g_free(value);
			goto account_found;
		}
		
		g_free(name);
		g_free(value);
	}

	hybrid_debug_error("account", "can't find 'account' node for this account");

	return;

account_found:

	group_node = xmlnode_child(node);

	while (group_node) {

		if (!xmlnode_has_prop(group_node, "id") ||
			!xmlnode_has_prop(group_node, "name")) {

			hybrid_debug_error("account", "invalid group node");
			group_node = xmlnode_next(group_node);
			continue;
		}

		id = xmlnode_prop(group_node, "id");
		name = xmlnode_prop(group_node, "name");

		group = hybrid_blist_add_group(account, id, name);
		group->cache_node = group_node;

		g_free(id);
		g_free(name);
		
		/*
		 * Iterate the buddy nodes, use the attribute values
		 * to create new HybridBuddys, and add them to the 
		 * buddy list and the GtkTreeView. 
		 */
		buddy_node = xmlnode_child(group_node);

		while (buddy_node) {

			if (!xmlnode_has_prop(buddy_node, "id") ||
				!xmlnode_has_prop(buddy_node, "name")) {

				hybrid_debug_error("account", "invalid buddy node");
				buddy_node = xmlnode_next(buddy_node);
				continue;
			}

			id = xmlnode_prop(buddy_node, "id");
			name = xmlnode_prop(buddy_node, "name");

			buddy = hybrid_blist_add_buddy(account, group, id, name);
			buddy->state = HYBRID_STATE_OFFLINE;

			buddy->cache_node = buddy_node;

			g_free(id);
			g_free(name);

			if (xmlnode_has_prop(buddy_node, "mood")) {
				value = xmlnode_prop(buddy_node, "mood");
				hybrid_blist_set_buddy_mood(buddy, value);
				g_free(value);
			}

			/*
			 * If buddy node has attribute named 'icon', and the value ot the 
			 * attribute is not empty, then load the file pointed by 
			 * the icon value as the buddy's portrait, orelse load the 
			 * default icon, we DONT need to load the default icon file
			 * any more, let the blist_set_buddy_icon() do it, just
			 * set the relevant argument to NULL.
			 */
			if (xmlnode_has_prop(buddy_node, "icon")) {
				value = xmlnode_prop(buddy_node, "icon");

				if (*value != '\0') {
					/* Set the buddy's icon name */
					g_free(buddy->icon_name);
					buddy->icon_name = g_strdup(value);

					name = g_strdup_printf("%s/%s", config->icon_path, value);

					g_file_get_contents(name, (gchar**)&icon_data,
							&icon_data_len, NULL);
					g_free(name);

				} else {
					icon_data = NULL;
					icon_data_len = 0;
				}
				
				g_free(value);

			} else {
				icon_data = NULL;
				icon_data_len = 0;
			}

			value = NULL;

			if (xmlnode_has_prop(buddy_node, "crc")) {
				value = xmlnode_prop(buddy_node, "crc");
				if (*value == '\0') {
					g_free(value);
					value = NULL;
				}
			}

			blist_set_buddy_icon(buddy, icon_data,
					icon_data_len, value);

			g_free(value);
			g_free(icon_data);

			buddy_node = xmlnode_next(buddy_node);
		}

		group_node = xmlnode_next(group_node);
	}

}

/**
 * Save the account's icon to the local file.The naming method is:
 * SHA1(proto_name + '_' + username).type
 */
static void
hybrid_account_icon_save(HybridAccount *account)
{
	gchar *name;
	gchar *hashed_name;
	HybridModule *module;
	HybridConfig *config;

	g_return_if_fail(account != NULL);

	if (account->icon_data == NULL || account->icon_data_len == 0) {
		return;
	}

	module = account->proto;
	config = account->config;

	if (!account->icon_name) {
		name = g_strdup_printf("%s_%s", module->info->name, account->username);
		hashed_name = hybrid_sha1(name, strlen(name));
		g_free(name);

		account->icon_name = g_strdup_printf("%s.jpg", hashed_name);

		name = g_strdup_printf("%s/%s.jpg", config->icon_path, hashed_name);
		g_free(hashed_name);

	} else {
		name = g_strdup_printf("%s/%s", config->icon_path, account->icon_name);
	}

	g_file_set_contents(name, (gchar*)account->icon_data,
			account->icon_data_len, NULL);

	g_free(name);
}

/**
 * Save the icon related attribute only.
 */
static void
account_set_icon(HybridAccount *account, const guchar *icon_data,
		gint icon_data_len, const gchar *icon_crc)
{
	g_return_if_fail(account != NULL);

	g_free(account->icon_data);
	g_free(account->icon_crc);

	account->icon_data = g_memdup(icon_data, icon_data_len);
	account->icon_crc = g_strdup(icon_crc);
	account->icon_data_len = icon_data_len;
}
