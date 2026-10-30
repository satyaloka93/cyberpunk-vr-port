#pragma once
namespace cvr::hud {
// Called at RED4ext unload, before CRT static destruction. Idempotent.
void ShutdownUpkeep();
}
