#include <Helix/CanvasUI/Core/ManagedCalls.h>

#include <Helix/CanvasUI/Core/UIBehaviour.h>
#include <Helix/CanvasUI/Rendering/VertexHelper.h>
#include <Helix/Engine/Script/ScriptClass.h>

namespace helix::canvasui
{
    namespace
    {
        // The C# side's last answer.
        struct Answer
        {
            engine::Material* material = nullptr;
            bool flag = false;
            f32 number = 0;
        };

        Answer& A()
        {
            static Answer a;
            return a;
        }

        template <class... Args>
        engine::Event<Args...>& Make()
        {
            static engine::Event<Args...>* e = new engine::Event<Args...>();
            return *e;
        }
    }

    engine::Event<engine::Component*, engine::Material*>& ManagedCalls::modifyMaterial() { return Make<engine::Component*, engine::Material*>(); }
    engine::Event<engine::Component*, VertexHelper*>& ManagedCalls::modifyMesh() { return Make<engine::Component*, VertexHelper*>(); }
    engine::Event<engine::Component*, Vector2, engine::Camera*>& ManagedCalls::raycastFilter() { return Make<engine::Component*, Vector2, engine::Camera*>(); }
    engine::Event<engine::Component*, StringView>& ManagedCalls::implements() { return Make<engine::Component*, StringView>(); }
    engine::Event<engine::Component*, i32>& ManagedCalls::layout() { return Make<engine::Component*, i32>(); }

    void ManagedCalls::SetMaterial(engine::Material* material) { A().material = material; }
    void ManagedCalls::SetBool(bool value) { A().flag = value; }
    void ManagedCalls::SetFloat(f32 value) { A().number = value; }

    bool ManagedCalls::IsManaged(const engine::Component* c)
    {
        if (!c || !c->Is<engine::GameBehaviour>())
            return false;
        const engine::ScriptClass* s = static_cast<const engine::GameBehaviour*>(c)->script();
        return s && s->language == engine::ScriptLanguage::CSharp;
    }

    engine::Material* ManagedCalls::ModifyMaterial(engine::Component* c, engine::Material* material)
    {
        A().material = material;
        modifyMaterial().Invoke(c, material);
        return A().material;
    }

    void ManagedCalls::ModifyMesh(engine::Component* c, VertexHelper& vh) { modifyMesh().Invoke(c, &vh); }

    bool ManagedCalls::RaycastFilter(engine::Component* c, Vector2 sp, engine::Camera* camera, bool& implemented)
    {
        implemented = Implements(c, "ICanvasRaycastFilter");
        if (!implemented)
            return true;
        A().flag = true;
        raycastFilter().Invoke(c, sp, camera);
        return A().flag;
    }

    bool ManagedCalls::Implements(engine::Component* c, StringView interfaceName)
    {
        A().flag = false;
        implements().Invoke(c, interfaceName);
        return A().flag;
    }

    void ManagedCalls::LayoutCall(engine::Component* c, i32 call) { layout().Invoke(c, call); }

    f32 ManagedCalls::LayoutFloat(engine::Component* c, i32 property)
    {
        A().number = 0;
        layout().Invoke(c, property);
        return A().number;
    }

    bool ManagedCalls::LayoutBool(engine::Component* c, i32 property)
    {
        A().flag = false;
        layout().Invoke(c, property);
        return A().flag;
    }
}
