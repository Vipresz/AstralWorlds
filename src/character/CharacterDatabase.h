#pragma once

#include "character/CharacterTypes.h"

namespace Game {
namespace Character {

class ICharacterDatabase {
public:
    virtual ~ICharacterDatabase() = default;

    // Called at login (online or offline). Fills equipped items and body selection.
    virtual bool FetchLoadout(const CharacterLoginContext& context, CharacterLoadout& out) = 0;
};

// Reads per-user JSON from character/loadouts/<userId>.json (simulates server DB in dev).
class ServerLoadoutDatabase : public ICharacterDatabase {
public:
    explicit ServerLoadoutDatabase(const char* loadoutsDir = "character/loadouts");

    bool FetchLoadout(const CharacterLoginContext& context, CharacterLoadout& out) override;

private:
    bool LoadFromFile(const std::string& path, CharacterLoadout& out) const;

    std::string m_loadoutsDir;
};

} // namespace Character
} // namespace Game
