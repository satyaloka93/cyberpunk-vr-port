#include "Core/GameCompatibility.hpp"

using namespace cvr::compatibility;

// Only the migrated exact 2.50 SDK/engine profile is accepted by default.
// Rolled-back and unknown/future executables have no compatibility fallback.
static_assert(Evaluate(true, Game250) == Status::Ready);
static_assert(Evaluate(true, Game231) == Status::SdkMismatch);
static_assert(Evaluate(false, Game250) == Status::UnknownExecutable);
static_assert(Evaluate(true, {3, 0, 88, 46236}) == Status::SdkMismatch);
static_assert(Evaluate(true, Game250, Game250, Game250) == Status::Ready);
static_assert(Evaluate(true, Game231, Game231, Game231) == Status::Ready);
static_assert(Evaluate(true, Game250, Game231, Game250) == Status::SdkMismatch);
static_assert(Evaluate(true, Game231, Game231, Game250) == Status::EngineHooksMismatch);
static_assert(Evaluate(true, Game250, Game250, Game231) == Status::EngineHooksMismatch);

int main() { return 0; }
