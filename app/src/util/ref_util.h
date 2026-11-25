#pragma once

#include <memory>
#include <glib-object.h>

#define VERIFY_LOG_CLEANUP(err, cleanup_cb) \
    do { \
        /* Use local variable to avoid multiple evaluation of the expression.
         * Note that declaration and assignment must be separate statements
         * otherwise this macro will not work in some contexts.
         */ \
        if(err) \
        { \
            (cleanup_cb)(); \
            VERIFY_LOG(err); \
        } \
    } while (0)

namespace orion::gtk {

struct GObjectDeleter
{
    void
    operator()(GObject* object) const
    {
        if(object) g_object_unref(object);
    }
};

typedef std::unique_ptr<GObject, GObjectDeleter> GObjectPtr;

// Wraps around a dynamically allocated GWeakRef, allows for cpp memory management to handle freeing weak refs
// to be used in global core api callbacks (lambdas) only, cannot guarantee results in glib callbacks as user data
// should be used in unique pointers.
struct WeakRefWrapper
{
    WeakRefWrapper();
    WeakRefWrapper(gpointer object);
    ~WeakRefWrapper();
    WeakRefWrapper(const WeakRefWrapper&);
    WeakRefWrapper(WeakRefWrapper&&);

    WeakRefWrapper& operator=(const WeakRefWrapper&);
    WeakRefWrapper& operator=(WeakRefWrapper&&);
    void reset(gpointer object);

    // Returns nullptr if the underlying object has already been freed, and also
    // returns nullptr if the WeakRefWrapper itself has been constructed from nullptr.
    //
    // Typical usage:
    //   auto weak_ref = orion::gtk::WeakRefWrapper(some_gobject);
    //   foo([weak_ref]{
    //      auto ref = weak_ref.lock();
    //      if (!ref) {
    //          return;
    //      }
    //      GObject* obj = ref.get();
    //      // use obj
    //   })
    //   
    GObjectPtr
    lock() const;

    // Do not use this anymore, see lock() usage above. This will be removed in the future.
    gpointer
    get() const;

private:
    mutable GWeakRef ref = {};
};
struct StrongRefWrapper
{
    //takes a new reference to object
    StrongRefWrapper(gpointer object);
    ~StrongRefWrapper();
    StrongRefWrapper(const StrongRefWrapper&);
    StrongRefWrapper(StrongRefWrapper&&);

    StrongRefWrapper& operator=(const StrongRefWrapper&);
    StrongRefWrapper& operator=(StrongRefWrapper&&);
    //takes a new reference to object
    void reset(gpointer object);

    gpointer
    get() const;

    // Creates new wrapper that does not take new reference to the object, but it will unref the object in destructor
    static StrongRefWrapper create_raw(gpointer object);

private:
    mutable gpointer ptr;
};

} // namespace orion::gtk