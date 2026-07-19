// LockFreeEventQueue.cpp
// Header-only template; this translation unit exists to allow explicit
// instantiation of concrete event-queue types and to keep the build
// system happy (no .cpp means no object file, which is fine).
//
// Explicit instantiation example (uncomment when YukiEvent is defined):
// #include "LockFreeEventQueue.h"
// #include "brain/core/event/YukiEvent.h"
// namespace yuki::core {
//     template class LockFreeEventQueue<YukiEvent, 12>;  // 4096 slots
// }
