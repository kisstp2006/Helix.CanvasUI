#include <Helix/CanvasUI/Core/OwnedRoot.h>

namespace helix::canvasui::detail
{
    namespace
    {
        struct Roots
        {
            u64 next = 0;
            HashMap<u64, void*> objects;
        };

        Roots& R()
        {
            static Roots* r = new Roots();
            return *r;
        }
    }

    u64 NewRootId() { return ++R().next; }
    void AddRoot(u64 id, void* object) { R().objects.Set(id, object); }
    void RemoveRoot(u64 id) { R().objects.Remove(id); }

    void* FindRoot(u64 id)
    {
        void** o = R().objects.Find(id);
        return o ? *o : nullptr;
    }
}
