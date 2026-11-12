#pragma once
#include <cstdint>

// A quiet, paper-like palette: warm greys, a single desaturated sage accent.
// Anything the user places on the stage can be recoloured from the inspector.
namespace theme {

constexpr uint32_t kBg            = 0xF2F0EB;
constexpr uint32_t kPanel         = 0xFAF9F6;
constexpr uint32_t kPanelAlt      = 0xF3F1EC;
constexpr uint32_t kBorder        = 0xE4E0D8;
constexpr uint32_t kBorderStrong  = 0xD2CDC3;
constexpr uint32_t kText          = 0x2E2C28;
constexpr uint32_t kTextMuted     = 0x8A8478;
constexpr uint32_t kTextFaint     = 0xB5AFA4;
constexpr uint32_t kAccent        = 0x6E7F71;
constexpr uint32_t kAccentSoft    = 0xE7ECE6;
constexpr uint32_t kField         = 0xFFFFFF;
constexpr uint32_t kHover         = 0xEFECE5;
constexpr uint32_t kCanvas        = 0xEAE8E2;
constexpr uint32_t kOffstage      = 0xEFECE5;
constexpr uint32_t kStageBorder   = 0xA7A196;
constexpr uint32_t kGrid          = 0xE5E1D9;
constexpr uint32_t kGridStrong    = 0xDAD5CB;
constexpr uint32_t kAudience      = 0xA8A296;
constexpr uint32_t kDanger        = 0x8C6A62;

} // namespace theme
