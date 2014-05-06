/*!
 * Contact: Mohammed Hassan <mohammed.hassan@jolla.com>
 */

#include <glib.h>
#include <gconf/gconf-client.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <boost/algorithm/string.hpp>
extern "C" {
#include <dconf.h>
};
#include <ctime>

#define MIGRATION_KEY "/dconf/migration_timestamp"

static GVariant *convert_list(const GConfValue *value);

template<typename T>class GObjectWrapper {
public:
    GObjectWrapper(T *obj) : m_obj(obj) {

    }

    GObjectWrapper(const GObjectWrapper& other) {
        if (other.m_obj) {
            m_obj = other.m_obj ? other.ref() : NULL;
        }
    }

    ~GObjectWrapper() {
        unref();
    }

    GObjectWrapper& operator=(const GObjectWrapper& other) {
        m_obj = other.m_obj ? other.ref() : NULL;

        return *this;
    }

    operator bool() {
        return m_obj != 0;
    }

    T *operator*() {
        return m_obj;
    }

    T *operator*() const {
        return m_obj;
    }

private:
    T *ref() const {
        g_object_ref(m_obj);
        return m_obj;
    }

    void unref() {
        g_object_unref(m_obj);
        m_obj = NULL;
    }

    T *m_obj;
};

// Specialization for GConfEntry which is not a GObject subclass
template<> GConfEntry *GObjectWrapper<GConfEntry>::ref() const {
    return gconf_entry_ref(m_obj);
}

template<> void GObjectWrapper<GConfEntry>::unref() {
    gconf_entry_unref(m_obj);
    m_obj = NULL;
}

// Specialization for DConfChangeset which is not a GObject subclass
template<> DConfChangeset *GObjectWrapper<DConfChangeset>::ref() const {
    return dconf_changeset_ref(m_obj);
}

template<> void GObjectWrapper<DConfChangeset>::unref() {
    dconf_changeset_unref(m_obj);
    m_obj = NULL;
}

static bool
get_all_keys(GConfClient *client, std::vector<GObjectWrapper<GConfEntry> >& keys, std::string dir) {
    GError *err = NULL;

    // First we add all keys
    GSList *children = gconf_client_all_entries(client, dir.c_str(), &err);
    if (err) {
        std::cout << "Error getting keys for " << dir << ": " << err->message << std::endl;
        g_error_free(err);
    }

    if (children) {
        GSList *entry = children;
        while (entry) {
            GConfEntry *key = reinterpret_cast<GConfEntry *>(entry->data);
            if (key && key->value && !gconf_entry_get_is_default(key)) {
                // NULL value means unset.
                keys.push_back(GObjectWrapper<GConfEntry>(key));
            }

            entry = entry->next;
        }

        // We are not unreffing the keys on purpose because now they belong to GObjectWrapper.
        // We just free the list
        g_slist_free(children);
    }

    children = NULL;

    // Now the dirs
    GSList *dirs = gconf_client_all_dirs(client, dir.c_str(), &err);
    if (err) {
        std::cout << "Error getting subdirs of " << dir << ": " << err->message << std::endl;
        g_error_free(err);
        return false;
    }

    if (!dirs) {
        return true;
    }

    GSList *entry = dirs;
    bool ret = true;
    while (entry) {
        gchar *path = reinterpret_cast<gchar *>(entry->data);
        // We are not interested in schemas
        if (strcmp(path, "/schemas")) {
            if (!get_all_keys(client, keys, path)) {
                // We will store the value and return it but we will not stop processing
                // and hope we will get a partial migration.
                ret = false;
            }
        }

        entry = entry->next;
    }

    g_slist_foreach(dirs, (GFunc)g_free, NULL);
    g_slist_free(dirs);
    dirs = NULL;
    return ret;
}

static GVariant *
to_variant(const GConfValue *value) {
    switch (value->type) {
    case GCONF_VALUE_INVALID:
    case GCONF_VALUE_SCHEMA:
    case GCONF_VALUE_PAIR:
        return NULL;
    case GCONF_VALUE_INT:
        return g_variant_new_int32(gconf_value_get_int(value));
    case GCONF_VALUE_BOOL:
        return g_variant_new_boolean(gconf_value_get_bool(value));
    case GCONF_VALUE_FLOAT:
        return g_variant_new_double(gconf_value_get_float(value));
    case GCONF_VALUE_STRING:
        return g_variant_new_string(gconf_value_get_string(value));

    case GCONF_VALUE_LIST:
        return convert_list(value);
    }

    return NULL;
}

static GVariant *
convert_list(const GConfValue *value) {
    GSList *list = gconf_value_get_list(value);
    if (!list) {
        return NULL;
    }

    GSList *l;
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

    for (l = list; l; l = l->next) {
        GVariant *v = to_variant(reinterpret_cast<GConfValue *>(l->data));
        if (v) {
            g_variant_builder_add_value(builder, v);
        }
    }

    GVariant *result = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return result;
}

static void
migrate(DConfChangeset *set, const std::vector<GObjectWrapper<GConfEntry> >& keys) {
    for (unsigned x = 0; x < keys.size(); x++) {
        const char *key = gconf_entry_get_key(*keys[x]);
        const GConfValue *value = gconf_entry_get_value(*keys[x]);
        GVariant *v = to_variant(value);
        if (v) {
            dconf_changeset_set(set, key, v);
        }
    }
}

int
main(int argc, char *argv[]) {
    std::cout << "Welcome to the gconf to dconf migration tool!" << std::endl;

    GObjectWrapper<GConfClient> gconf(gconf_client_get_default());
    if (!gconf) {
        std::cerr << "Failed to initialize gconf" << std::endl;
        return 1;
    }

    GObjectWrapper<DConfClient> dconf = dconf_client_new();
    if (!dconf) {
        std::cerr << "Failed to initialize dconf" << std::endl;
        return 1;
    }

    GObjectWrapper<DConfChangeset> set(dconf_changeset_new());
    if (!set) {
        std::cerr << "Failed to create a dconf changeset" << std::endl;
        return 1;
    }

    // Check if we have completed the migration previously
    GVariant *var = dconf_client_read (*dconf, MIGRATION_KEY);
    if (var) {
      // We don't really care about the value.
      g_variant_unref (var);
      std::cerr << "migration has been completed previously" << std::endl;
      return 0;
    }

    // mark in the database that we are done with the migration
    GError *err = NULL;
    var = g_variant_new_int64 (time(NULL));
    if (!dconf_client_write_sync (*dconf, MIGRATION_KEY, var, NULL, NULL, &err)) {
      // We are doomed but we will go on :(
      std::cerr << "Failed to store key " << MIGRATION_KEY << " :" << err->message << std::endl;
      g_error_free(err);
      err = NULL;
    }

    std::vector<GObjectWrapper<GConfEntry> > keys;
    bool have_all_keys = get_all_keys(*gconf, keys, "/");
    if (!have_all_keys && keys.size() == 0) {
        std::cerr << "Failed to read gconf database" << std::endl;
        return 1;
    }
    else if (!have_all_keys && keys.size() > 0) {
        std::cerr << "Failed to read all gconf database. Partial migration on the way." << std::endl;
    }

    migrate(*set, keys);

    keys.clear();

    dconf_client_change_sync(*dconf, *set, NULL, NULL, &err);
    if (err) {
      std::cerr << "Error saving dconf changeset :" << err->message << std::endl;
      g_error_free(err);
      return 1;
    }

    return 0;
}
