#include <gtk/gtk.h>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif


#define APP_TYPE_WEBSITE_DATA_STORAGE_ITEM (website_data_storage_item_get_type())

G_DECLARE_FINAL_TYPE(WebsiteDataStorageItem, website_data_storage_item, APP, WEBSITE_DATA_STORAGE_ITEM, GObject);


std::string website_data_storage_item_get_name(WebsiteDataStorageItem* self);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitWebsiteData* website_data_storage_item_get_data(WebsiteDataStorageItem* self);
WebKitWebsiteDataTypes website_data_storage_item_get_types(WebsiteDataStorageItem* self);
#endif

gboolean website_data_storage_item_filter_func(gpointer item, gpointer user_data); //expects a g_utf_casefold'd const char* in user_data



// #define ORION_ALWAYS_INLINE inline __attribute__((always_inline))

// namespace orion::gtk {

// struct Type {
//     GType gtype;
//     Type(GType t) : gtype(t) {}
//     operator GType() const { return gtype; }
// };

// template <typename T>
// struct RefTraits {
// };

// template <std::derived_from<Object> T>
// struct RefTraits <T>{
//     static T* ref(T* ptr) { return T->ref(); }
//     static void unref(T* ptr) { T->unref(); }
// };

// template <typename T>
// struct RefPtr {
//     RefPtr() {}
// };

// class Object {
// //needs to be a POD matching the actual GObject struct structure
//     GTypeInstance g_type_instance;
    
//     guint ref_count;  /* (atomic) */
//     GData* qdata;


// protected:
//     static ORION_ALWAYS_INLINE void init_type(Type tp) {}; //exists here as a noop so that this becomes "optionally overridable" in the type registration by just reimplementing in inheritors

    
//     static ORION_ALWAYS_INLINE Type get_type () {
//         return G_TYPE_OBJECT;
//     }

//     //non glib (c++) inheritors need to be able to placement new themselves but never write into the inherited object's fields,
//     //thus this and all libary gobject wrapping inheritors need 
//     inline Object() {};
//     inline ~Object() {};

// public:
// };

// }