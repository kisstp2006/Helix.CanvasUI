#pragma once

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Types/Type.h>

// The ids of the canvas UI's own root classes (objects that are not engine objects: VertexHelper, the event data):
// each object has an id while it lives and the slot of its C# wrapper; C# makes and owns them (they go after their C#
// objects are collected) unless C++ made them.
namespace helix::canvasui
{
    namespace detail
    {
        u64 NewRootId();
        void AddRoot(u64 id, void* object);
        void RemoveRoot(u64 id);
        void* FindRoot(u64 id);
    }

    template <class Self>
    class OwnedRoot
    {
    public:
        OwnedRoot() : id_(detail::NewRootId()) { detail::AddRoot(id_, static_cast<Self*>(this)); }
        virtual ~OwnedRoot()
        {
            detail::RemoveRoot(id_);
            scripting::ReleaseWrapper(wrapper_);
        }
        OwnedRoot(const OwnedRoot&) = delete;
        OwnedRoot& operator=(const OwnedRoot&) = delete;

        u64 Id() const { return id_; }
        u64& Wrapper() { return wrapper_; }
        static Self* Find(u64 id) { return static_cast<Self*>(detail::FindRoot(id)); }
        virtual scripting::TypeId HelixType() const = 0;

    private:
        u64 id_;
        u64 wrapper_ = 0;
    };
}

// The handles of a root class for the bindings.
#define HX_CANVASUI_ROOT_HANDLES(Type)                                                                     \
    template <>                                                                                            \
    struct helix::scripting::ObjectHandles<Type>                                                           \
    {                                                                                                      \
        static u64 ToId(const Type* o) { return o->Id(); }                                                 \
        static Type* FromId(u64 id) { return Type::Find(id); }                                             \
        static TypeId TypeOf(const Type* o) { return o->HelixType(); }                                     \
        static u64& WrapperOf(Type* o) { return o->Wrapper(); }                                            \
    };
