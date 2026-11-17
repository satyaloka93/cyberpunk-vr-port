#pragma once
#include <cstdint>
namespace cvr::hooks {
enum class CreateHookResult { Created, AllocationFailure, Failed };
// A trampoline being allocated is not proof that its detour was enabled.
// Keep it for a subsequent enable attempt; never recreate an owned hook.
struct HookActivation {
    bool prepared{},ready{};
    template<class Create,class Enable,class Far>
    bool Ensure(Create create,Enable enable,Far fallback) {
        if(ready)return true;
        if(!prepared) {
            const auto result=create();
            if(result==CreateHookResult::AllocationFailure){ready=fallback();return ready;}
            if(result!=CreateHookResult::Created)return false;
            prepared=true;
        }
        ready=enable();return ready;
    }
};
struct HookAttemptBudget {
    static constexpr unsigned limit=3;
    uintptr_t module{};unsigned attempts{};
    bool Begin(uintptr_t current) {
        if(!current)return false;
        if(module!=current){module=current;attempts=0;}
        if(attempts>=limit)return false;
        ++attempts;return true;
    }
};
}
