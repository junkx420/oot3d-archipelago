// Host driver for OoT3D_Randomizer's logic: answers "which locations can be
// reached with these items" so the apworld's ported rules can be compared
// with the 3D randomizer's own rules (recon/compare_logic.py).
//
// Built from an OoT3D_Randomizer checkout by build_oracle.ps1; this file and
// the stubs are the only TriAevum code in the build.
//
// Usage:
//   oot3d_logic_oracle [--set "Option Name=Option Text"]... [--vanilla-except FILE] COMMAND
// --vanilla-except places every location's vanilla item except at the
// locations named in FILE (one per line), which stay empty.
// COMMAND is one of
//   --dump-settings    every option menu, option, selected and possible values
//   --list-locations   every location in the pool, one name per line
//   --list-items       every item name with its advancement flag
//   --query            reads one item list per stdin line ('|'-separated item
//                      names, an item may repeat) and writes one line with the
//                      reachable locations ('|'-separated), or "error: ...";
//                      items placed in the world are collected on the way

#include "fill.hpp"
#include "hint_list.hpp"
#include "item_list.hpp"
#include "item_location.hpp"
#include "item_pool.hpp"
#include "location_access.hpp"
#include "logic.hpp"
#include "random.hpp"
#include "settings.hpp"
#include "shops.hpp"
#include "enemizer.hpp"
#include "ocarina_notes.hpp"
#include "patch.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// The 3DS build generates this from the patch's symbol map; message writing
// needs it, the logic does not.
const PatchSymbols gPatchSymbols{};

// debug.cpp's emulator log, which needs the 3DS; logic messages go to stderr.
void CitraPrint(std::string_view message) {
    std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()), message.data());
}

// menu.cpp and music.cpp symbols that settings.cpp and spoiler_log.cpp refer
// to; the oracle never opens a menu or shuffles music.
bool gInitError = false;
void GoToMenu(Menu*) {}
namespace Music {
void InitMusicRandomizer() {}
void ShuffleSequences(int) {}
} // namespace Music

namespace {

std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find(separator, start);
        const std::string part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) {
            parts.push_back(part);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

Option* FindOption(const std::string& name) {
    for (Menu* menu : Settings::GetAllOptionMenus()) {
        if (menu->mode != OPTION_MENU) {
            continue;
        }
        for (Option* option : *menu->settingsList) {
            if (SanitizedString(option->GetName()) == name) {
                return option;
            }
        }
    }
    return nullptr;
}

bool ApplySetting(const std::string& assignment) {
    const size_t equals = assignment.find('=');
    if (equals == std::string::npos) {
        std::fprintf(stderr, "bad --set %s (expected Name=Value)\n", assignment.c_str());
        return false;
    }
    const std::string name = assignment.substr(0, equals);
    const std::string value = assignment.substr(equals + 1);
    Option* option = FindOption(name);
    if (option == nullptr) {
        std::fprintf(stderr, "unknown option %s\n", name.c_str());
        return false;
    }
    for (size_t i = 0; i < option->GetOptionCount(); i++) {
        if (SanitizedString(option->GetOptions()[i]) == value) {
            option->SetSelectedIndex(i);
            return true;
        }
    }
    std::fprintf(stderr, "option %s has no value %s\n", name.c_str(), value.c_str());
    return false;
}

void DumpSettings() {
    for (Menu* menu : Settings::GetAllOptionMenus()) {
        if (menu->mode != OPTION_MENU) {
            continue;
        }
        for (Option* option : *menu->settingsList) {
            if (!option->IsCategory(OptionCategory::Setting)) {
                continue;
            }
            std::string values;
            for (const std::string& text : option->GetOptions()) {
                values += (values.empty() ? "" : " | ") + SanitizedString(text);
            }
            std::printf("%s\t%s\t%s\t%s\n", SanitizedString(menu->name).c_str(),
                        SanitizedString(option->GetName()).c_str(),
                        SanitizedString(option->GetSelectedOptionText()).c_str(), values.c_str());
        }
    }
}

std::map<std::string, ItemKey> ItemsByName() {
    std::map<std::string, ItemKey> items;
    for (int key = NONE + 1; key < KEY_ENUM_MAX; key++) {
        const std::string name(ItemTable(static_cast<ItemKey>(key)).GetName().GetNAEnglish());
        if (!name.empty()) {
            items.emplace(name, static_cast<ItemKey>(key));
        }
    }
    return items;
}

// The world as Fill() sees it right before placing items: the area graph,
// the location pool and the vanilla enemy and song tables.
void PrepareWorld() {
    Random_Init(0);
    Settings::UpdateSettings();
    Logic::UpdateHelpers();
    AreaTable_Init();
    ItemReset();
    GenerateLocationPool();
    Enemizer::RandomizeEnemies();
    OcarinaNotes::GenerateSongList();
}

// Every location except the listed ones gets its vanilla item (shops their
// vanilla stock), the way the apworld turns locations that are not checks into
// events or locks vanilla items in place. The listed locations stay empty.
bool PlaceVanillaExcept(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot read %s\n", path.c_str());
        return false;
    }
    std::string text;
    char buffer[4096];
    for (size_t read; (read = std::fread(buffer, 1, sizeof(buffer), file)) > 0;) {
        text.append(buffer, read);
    }
    std::fclose(file);
    std::vector<std::string> names = Split(text, '\n');
    for (std::string& name : names) {
        if (!name.empty() && name.back() == '\r') {
            name.pop_back();
        }
    }

    PlaceVanillaShopItems();
    for (const LocationKey key : allLocations) {
        ItemLocation* location = Location(key);
        if (std::find(names.begin(), names.end(), location->GetName()) != names.end()) {
            location->SetPlacedItem(NONE);
        } else if (location->GetPlacedItemKey() == NONE && location->GetVanillaItem() != NONE) {
            location->PlaceVanillaItem();
        }
    }
    return true;
}

int Query() {
    const std::map<std::string, ItemKey> items = ItemsByName();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        Logic::LogicReset();
        std::string unknown;
        for (const std::string& name : Split(line, '|')) {
            const auto item = items.find(name);
            if (item == items.end()) {
                unknown = name;
                break;
            }
            ItemTable(item->second).ApplyEffect();
        }
        if (!unknown.empty()) {
            std::printf("error: unknown item %s\n", unknown.c_str());
            std::fflush(stdout);
            continue;
        }
        Logic::UpdateHelpers();
        GetAccessibleLocations(allLocations);
        // Every location the search reached, whether it holds a placed item or not.
        std::string out;
        for (const LocationKey key : allLocations) {
            if (Location(key)->IsAddedToPool()) {
                out += (out.empty() ? "" : "|") + Location(key)->GetName();
            }
        }
        std::printf("%s\n", out.c_str());
        std::fflush(stdout);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    HintTable_Init();
    ItemTable_Init();
    LocationTable_Init();
    Settings::InitSettings();
    Settings::SetDefaultSettings();

    std::string command;
    std::string vanillaExcept;
    for (int i = 1; i < argc; i++) {
        const std::string argument = argv[i];
        if (argument == "--set" && i + 1 < argc) {
            if (!ApplySetting(argv[++i])) {
                return 2;
            }
        } else if (argument == "--vanilla-except" && i + 1 < argc) {
            vanillaExcept = argv[++i];
        } else {
            command = argument;
        }
    }

    if (command == "--dump-settings") {
        DumpSettings();
        return 0;
    }
    PrepareWorld();
    if (command == "--list-locations") {
        for (const LocationKey location : allLocations) {
            std::printf("%s\n", Location(location)->GetName().c_str());
        }
        return 0;
    }
    if (command == "--list-items") {
        for (const auto& [name, key] : ItemsByName()) {
            std::printf("%s\t%d\n", name.c_str(), ItemTable(key).IsAdvancement() ? 1 : 0);
        }
        return 0;
    }
    if (command == "--query") {
        if (!vanillaExcept.empty() && !PlaceVanillaExcept(vanillaExcept)) {
            return 2;
        }
        return Query();
    }
    std::fprintf(stderr, "usage: oot3d_logic_oracle [--set Name=Value]... [--vanilla-except FILE] "
                         "--dump-settings|--list-locations|--list-items|--query\n");
    return 2;
}
