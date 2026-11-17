#ifdef _WIN32
#include <Windows.h>
#endif
#include "Hooks/HookActivation.hpp"
#include <stdexcept>
#include <iostream>
using namespace cvr::hooks;
void check(bool v){if(!v)throw std::runtime_error("hook activation regression");}
int main() {
 int creates=0,enables=0,fars=0;
 HookActivation s;
 auto create=[&]{++creates;return CreateHookResult::Created;};
 auto enable=[&]{return ++enables>1;};
 auto fallback=[&]{++fars;return false;};
 check(!s.Ensure(create,enable,fallback));check(s.prepared&&!s.ready);
 check(s.Ensure(create,enable,fallback));check(creates==1&&enables==2&&fars==0);
 check(s.Ensure(create,enable,fallback));check(creates==1&&enables==2);
 HookActivation f;creates=enables=fars=0;
 auto allocateFail=[&]{++creates;return CreateHookResult::AllocationFailure;};
 auto busyThenReady=[&]{return ++fars>1;};
 check(!f.Ensure(allocateFail,enable,busyThenReady));check(!f.prepared&&!f.ready);
 check(f.Ensure(allocateFail,enable,busyThenReady));check(creates==2&&enables==0&&fars==2);
 check(f.Ensure(allocateFail,enable,busyThenReady));check(creates==2&&fars==2);
 HookActivation unavailable;creates=enables=fars=0;
 auto failed=[&]{++creates;return CreateHookResult::Failed;};
 check(!unavailable.Ensure(failed,enable,fallback));check(!unavailable.prepared&&!unavailable.ready&&enables==0&&fars==0);
 HookAttemptBudget budget;check(!budget.Begin(0));
 for(int i=0;i<1000;++i)check(budget.Begin(42)==(i<3));
 check(budget.attempts==3);check(budget.Begin(43)&&budget.attempts==1);
 // Independent evaluation routes: success on one must not stop retries on the other.
 HookActivation route[2];int a=0,b=0;
 check(route[0].Ensure(create,[&]{++a;return true;},fallback));
 check(!route[1].Ensure(create,[&]{++b;return false;},fallback));
 check(route[0].Ensure(create,[&]{++a;return true;},fallback));
 check(route[1].Ensure(create,[&]{++b;return true;},fallback));check(a==1&&b==2);
 std::cout<<"PASS creation/enable retry, fallback retry, no repeated live hooks, per-module attempt cap, partial routes\n";
}
