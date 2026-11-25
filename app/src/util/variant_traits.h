#pragma once
#include <type_traits>

namespace orion::gtk {



template <typename T>
struct VariantTraits {

//implemented only in template specialisations
static T get(GVariant* var) = delete;
static GVariant* create(T val) = delete;
static const GVariantType* type() = delete;

};



template <typename T>
    requires std::is_enum_v<T>
struct VariantTraits<T> {
static T get(GVariant* var) { return T(g_variant_get_int64(var)); };
static GVariant* create(T val) { return g_variant_new_int64(gint64(val)); };
static const GVariantType* type() { return G_VARIANT_TYPE_INT64; };
};



template <>
struct VariantTraits<int64_t> {
static int64_t get(GVariant* var) { return g_variant_get_int64(var); };
static GVariant* create(int64_t val) { return g_variant_new_int64(val); };
static const GVariantType* type() { return G_VARIANT_TYPE_INT64; };
};



template <>
struct VariantTraits<double> {
static double get(GVariant* var) { return g_variant_get_double(var); };
static GVariant* create(double val) { return g_variant_new_double(val); };
static const GVariantType* type() { return G_VARIANT_TYPE_DOUBLE; };
};



template <>
struct VariantTraits<bool> {
static bool get(GVariant* var) { return g_variant_get_boolean(var); };
static GVariant* create(bool val) { return g_variant_new_boolean(val); };
static const GVariantType* type() { return G_VARIANT_TYPE_BOOLEAN; };
};



template <>
struct VariantTraits<std::string> {

static std::string get(GVariant* var) {
    gsize len = 0;
    const gchar* val = g_variant_get_string(var, &len);
    return len ? std::string(val, len) : "";
}
static GVariant* create(std::string val) {
    return g_variant_new_string(val.c_str());
};
static const GVariantType* type() { return G_VARIANT_TYPE_STRING; };

};



template <>
struct VariantTraits<std::string_view> {

static std::string_view get(GVariant* var) {
    gsize len = 0;
    const gchar* val = g_variant_get_string(var, &len);
    return len ? std::string_view(val, len) : "";
}
static GVariant* create(std::string_view val) {
    gchar* str = g_strndup(val.data(), val.length());
    return g_variant_new_take_string(str);
}
static const GVariantType* type() { return G_VARIANT_TYPE_STRING; };

};



template <>
struct VariantTraits<const char*> {

static const char* get(GVariant* var) {
    return g_variant_get_string(var, NULL);
}
static GVariant* create(const char* val) {
    return g_variant_new_string(val);
}
static const GVariantType* type() { return G_VARIANT_TYPE_INT64; };

};



}