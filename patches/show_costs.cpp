#include "show_costs.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iterator>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "genie/dat/DatFile.h"
#include "ids.h"

/*
ln -s "/mnt/c/Program Files (x86)/Steam/steamapps/common/AoE2DE/resources/_common/dat/empires2_x2_p1.dat" de.dat

ln -s "/mnt/c/Users/nicho/Games/Age of Empires 2 DE/76561197981945038/mods/subscribed/93975_Random Cost
    Mod/resources/_common/dat/empires2_x2_p1.dat" random-costs.dat

ln -s "/mnt/c/Users/nicho/Games/Age of Empires 2 DE/76561197981945038/mods/local/Test Random
    Costs/resources/_common/dat/empires2_x2_p1.dat" test.dat

build/create-data-mod random-costs+show-costs de.dat

build/create-data-mod random-costs+show-costs de.dat test.dat

build/create-data-mod show-costs de.dat

build/create-data-mod show-costs test.dat

build/create-data-mod show-costs random-costs.dat
*/

/*
notes -

a "tech" (e.g. crossbowman upgrade)
has "required techs" (e.g. castle age and 'shadow archery range' (?)),
a "research location" (e.g. archery range),
and an "effect" (e.g. turn existing archers into crossbowmen)
crossbowmen can't be built until the tech is researched,
probably because of the tech tree.

markets require having built a mill,
probably because of the tech tree.

buildings required for age-up are defined as required techs for the age tech.

what defines the fact that building a mill triggers the "shadow mill -- age one" tech?
the "initiates technology" attribute on the mill.

it's possible that the upgrade effect will, at runtime,
copy the trainable/researchable units/techs from the one to the other when they are buildings.
otherwise, unclear how feudal barracks is connected to any of its units/techs. test this.
*/

// todo: flag age up costs
// todo: detect and flag cheap buildings that become much more expensive
// todo: detect and flag the cheapest buildings necessary for age up
// todo: indicate for each building, the cheaper and more expensive ages to build it
// todo: indicate for each age, the most cost effective way to gain pop space (+ considering slav,chinese,inca civ
//  bonuses)
// todo: detect and flag cheap units
// todo: for every cheap unit, indicate the age & cost of its production
//  building at cheapest?
// todo: given villager m+f cost, indicate resource gathering ratios for 1 tc booming

// todo: rename age-varying buildings according to the age e.g. Blacksmith (Castle)
// todo: consider making ranking system like [$], [$$], [$$$] to flag cheap things from expensive easier?
// todo: consider scoring wood as less valuable, and food's value dependent on the price of farms
//   (or =gold to keep it simple... or maybe just wood < food=gold=stone since stone isnt needed for castles)

// todo: check and document behavior of relevant civ bonuses

// todo: consider civ bonuses (like cuman ranges -75 wood) that allow infinite resources (kinda hard)
// todo: distinguish cheap units that are gated behind expensive techs from those that aren't (hard)


const char *const LANGUAGE_FILE_PATH =
    "/mnt/c/Program Files "
    "(x86)/Steam/steamapps/common/AoE2DE/resources/en/strings/key-value/key-value-strings-utf8.txt";
const std::filesystem::path OUTPUT_PATH = "build";


namespace {

typedef genie::ResourceUsage<int16_t, int16_t, int16_t> ResourceCost;
typedef genie::ResourceUsage<int16_t, int16_t, uint8_t> ResearchResourceCost;


ResourceCost toResourceCost(const ResearchResourceCost &researchResourceCost) {
    ResourceCost resourceCost;
    resourceCost.Type = researchResourceCost.Type;
    resourceCost.Amount = researchResourceCost.Amount;
    resourceCost.Flag = (bool)researchResourceCost.Flag;
    return resourceCost;
}


std::vector<ResourceCost> toResourceCosts(const std::vector<ResearchResourceCost> &researchResourceCosts) {
    std::vector<ResourceCost> resourceCosts;
    resourceCosts.reserve(researchResourceCosts.size());
    for (const ResearchResourceCost &researchResourceCost : researchResourceCosts) {
        resourceCosts.push_back(toResourceCost(researchResourceCost));
    }
    return resourceCosts;
}


bool isNaturalResourceCost(const ResourceCost &cost) {
    return cost.Type != -1 && cost.Type < 4 && cost.Amount > 0 && cost.Flag == 1;
}


bool isNaturalResearchResourceCost(const ResearchResourceCost &cost) {
    return cost.Type != -1 && cost.Type < 4 && cost.Amount > 0 && cost.Flag == 1;
}

class Cost {
  public:
    Cost(const std::vector<ResourceCost> &data) {
        for (const ResourceCost &cost : data) {
            if (cost.Flag == 1) {
                switch (cost.Type) {
                case TYPE_FOOD:
                    food = cost.Amount;
                    break;
                case TYPE_WOOD:
                    wood = cost.Amount;
                    break;
                case TYPE_GOLD:
                    gold = cost.Amount;
                    break;
                case TYPE_STONE:
                    stone = cost.Amount;
                    break;
                default:
                    break;
                }
            }
        }
    }

    Cost(const std::vector<ResearchResourceCost> &data) : Cost(toResourceCosts(data)) {}

    bool getCostSum() const { return food + wood + gold + stone; }

    bool hasCost() const { return getCostSum() > 0; }

    std::string toString() const {
        std::vector<std::string> tokens;
        if (food > 0) {
            tokens.emplace_back(std::to_string(food));
            tokens.emplace_back("food");
        }
        if (wood > 0) {
            tokens.emplace_back(std::to_string(wood));
            tokens.emplace_back("wood");
        }
        if (gold > 0) {
            tokens.emplace_back(std::to_string(gold));
            tokens.emplace_back("gold");
        }
        if (stone > 0) {
            tokens.emplace_back(std::to_string(stone));
            tokens.emplace_back("stone");
        }
        tokens.emplace_back('(' + std::to_string(getCostSum()) + ')');
        std::ostringstream oss;
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            if (it != tokens.begin()) {
                oss << ' ';
            }
            oss << *it;
        }
        return oss.str();
    }

  private:
    int food;
    int wood;
    int gold;
    int stone;
};


class Game {

};

// things i want to be able to see on this class...
// - what is its name (via language dll or internal name)?
// - what is its resource cost?
// - what building is it researched at, if any?
// - is it an age tech? for what age?
// - what required tech objects does it have?
//   - whose job is it to decide to construct the required tech objects? i guess we need
//     a global list of techs by ID to draw from? should the constructor register them in that list?
// - what units does it upgrade, and to what?
// - what units does it enable?
// - [maybe] what are all the tech objects that require this tech?

// object is not fully usable until all related objects are constructed
// options:
// 1) some methods just wont work until then; nothing is precomputed based on them
//       i don't like this one
// 2) same as (1) but with memoization into fields
// 3) explicit 2nd-phase initialization method once all related objects have been constructed
//    to precompute everything
// 4) container class for all related objects
//     (at extreme end, it would just contain all existing code, no abstraction/encapsulation benefit)
//

class Tech {
  public:
    struct Hash {
        size_t operator()(const Tech &tech) const { return std::hash<int>()(tech.id); }
    };

    Tech(const genie::Tech &data, std::unordered_map<int, std::string> &nameMap)
        : data(data), cost(data.ResourceCosts) {
        // initialize id
        id = techs.size();

        // initialize name
        name = nameMap[data.LanguageDLLName];
        if (name.empty()) {
            name = data.Name;
        }

        // initialize requiredTechs
        // for (const int &i : data.RequiredTechs) {
        //     if (i > -1) {
        //         requiredTechs.emplace(techs[i]);
        //     }
        // }

        techs.emplace_back(std::cref(*this));
    }

    Tech(const Tech &) = delete;

    Tech &operator=(const Tech &) = delete;

    // should i delete move as well?

    // requires other techs to exist
    void Initialize() {

    }

    std::string getName() const { return name; }

    Cost getCost() const { return cost; }

    // should either store this as a field, or, expose it as an iterator?
    // maybe just expose methods like "hasRequiredTech"?
    // std::unordered_set<std::reference_wrapper<const Tech>, Hash> getRequiredTechs() {
    //     std::unordered_set<std::reference_wrapper<const Tech>, Hash> result;
    //     for (const int &i : data.RequiredTechs) {
    //         if (i > -1) {
    //             result.emplace(techs[i]);
    //         }
    //     }
    //     return result;
    // }

  private:
    static std::vector<std::reference_wrapper<const Tech>> techs;
    int id;
    std::string name;
    Cost cost;
    // std::unordered_set<std::reference_wrapper<const Tech>, Hash> requiredTechs;
    const genie::Tech &data;
};


bool hasNaturalResourceCost(const genie::Unit &unit) {
    std::vector<ResourceCost> costs = unit.Creatable.ResourceCosts;
    return std::any_of(costs.begin(), costs.end(), isNaturalResourceCost);
}


bool hasNaturalResearchResourceCost(std::vector<ResearchResourceCost> costs) {
    return std::any_of(costs.begin(), costs.end(), isNaturalResearchResourceCost);
}


std::string costToString(const std::vector<ResourceCost> &costs) {
    std::string s;
    for (const ResourceCost &cost : costs) {
        if (cost.Flag == 1 && cost.Type != -1) {
            s += std::to_string(cost.Amount);
            s += " ";
            switch (cost.Type) {
            case TYPE_FOOD:
                s += "Food ";
                break;
            case TYPE_WOOD:
                s += "Wood ";
                break;
            case TYPE_GOLD:
                s += "Gold ";
                break;
            case TYPE_STONE:
                s += "Stone ";
                break;
            case TYPE_POPULATION_HEADROOM:
                s += "Pop ";
                break;
            default:
                s += "vat? ";
            }
        }
    }
    return s.substr(0, s.length() - 1);
}


std::string costToString(const std::vector<ResearchResourceCost> &costs) {
    std::string s;
    for (const ResearchResourceCost &cost : costs) {
        if (cost.Flag == 1 && cost.Type != -1) {
            s += std::to_string(cost.Amount);
            s += " ";
            switch (cost.Type) {
            case TYPE_FOOD:
                s += "Food ";
                break;
            case TYPE_WOOD:
                s += "Wood ";
                break;
            case TYPE_GOLD:
                s += "Gold ";
                break;
            case TYPE_STONE:
                s += "Stone ";
                break;
            case TYPE_POPULATION_HEADROOM:
                s += "Pop ";
                break;
            default:
                s += "vat? ";
            }
        }
    }
    return s.substr(0, s.length() - 1);
}

}  // namespace

// general plan for this method
// 1. input
//    - read in language file (namemap), all techs and all units.
// 2. processing
//    - distinguish buildings and units
//    - associate techs to research locations
//    - associate units to training locations
//    - find age techs and associate to ages.
//    - find enabling techs and associate to units and ages
//    - find upgrading techs and associate to units and ages
//    - associate unit groups that are upgraded versions of each other as one thing
// 3. output
//    - write out units.txt
//    - write out ages.txt
//    - write out buildings.txt
void showCosts(const genie::DatFile *const df) {
    // possibly consider including dark age TC, because it can be built
    // if you delete the first one or in nomad start? cumans can always build 2nd feudal TC.
    const std::unordered_set<int16_t> unitsToInclude = {
        ID_VILLAGER_BASE_F,
    };

    const std::unordered_set<int16_t> unitsToExclude = {
        HEAVY_CROSSBOWMAN,
        ID_VILLAGER_FISHER_M,
        ID_VILLAGER_BUILDER_M,
        ID_VILLAGER_FORAGER_M,
        ID_VILLAGER_HUNTER_M,
        ID_VILLAGER_WOOD_M,
        ID_VILLAGER_STONE_M,
        ID_VILLAGER_REPAIRER_M,
        ID_VILLAGER_FARMER_M,
        ID_VILLAGER_GOLD_M,
        ID_VILLAGER_SHEPHERD_M,
        SPY,
        NORSE_WARRIOR,
        EASTERN_SWORDSMAN,
        HENRY_II,
        NINJA,
        GAJAH_MADA,
        SUNDA_ROYAL_FIGHTER,
        JAYAVIRAVARMAN,
        IROQUOIS_WARRIOR,
        CENTURION,
        ID_TRISTAN,
        RADEN_WIJAYA,
        ID_SURYAVARMAN_I,
        ID_BAYINNAUNG,
        CRUSADER_KNIGHT,
        RATHA_MELEE,
        ELITE_RATHA_MELEE,
        YOUNG_BABUR,
        ID_TRADE_CART_FULL,
        DONKEY,
        VASCO_DA_GAMA,
        LEIF_ERIKSON,
        ADMIRAL_YI_SUN_SHIN,
        THE_MIDDLEBROOK,
        DAGNAJAN,
        TABINSHWEHTI,
        GIRGEN_KHAN,
        KHAN,
        IBRAHIM_LODI,
        SHAYBANI_KHAN,
        MONKBT,
        COW_A,
        COW_B,
        COW_C,
        COW_D,
        GOAT,
        GOOSE,
        LLAMA,
        PIG,
        SHEEP,
        TURKEY,
        WATER_BUFFALO,
        // todo: the monasteries might possibly not need to be explicitly excluded
        //  if some of the age.txt logic is generalized correctly.
        MONASTERY_DARK,
        MONASTERY_FEUDAL,
        INDESTRUCT,
        PALACE,
        SANKORE_MADRASAH,
        TOWER_OF_LONDON,
        DORMITION_CATHEDRAL,
        AACHEN_CATHEDRAL,
        MINARET_OF_JAM,
        FORTIFIED_PALISADE_WALL,
        SEA_GATE,
        CITY_GATE,
        FIRE_TOWER,
        TREBUCHET_UNPACKED,
    };

    const std::unordered_map<int16_t, std::string> unitNameOverride = {
        {HUSKARL_BARRACKS, "Huskarl (Barracks)"},
        {ELITE_HUSKARL_BARRACKS, "Elite Huskarl (Barracks)"},
        {SERJEANT, "Serjeant (Castle)"},
        {ELITE_SERJEANT, "Elite Serjeant (Castle)"},
        {DSERJEANT, "Serjeant (Donjon)"},
        {ELITE_DSERJEANT, "Elite Serjeant (Donjon)"},
        {TARKAN_STABLE, "Tarkan (Stable)"},
        {ELITE_TARKAN_STABLE, "Elite Tarkan (Stable)"},
        {KONNIK, "Konnik (Castle)"},
        {ELITE_KONNIK, "Elite Konnik (Castle)"},
        {KONNIK_2, "Konnik (Krepost)"},
        {ELITE_KONNIK_2, "Elite Konnik (Krepost)"},
        // todo: maybe need some special handling for buildings.txt
        {BATTERING_RAM, "Battering Ram (Cuman Feudal)"},
        {ID_TRADE_CART_EMPTY, "Trade Cart"},
        {RATHA_RANGED, "Ratha"},
        {ELITE_RATHA_RANGED, "Elite Ratha"},
        {ID_TRADE_CART_EMPTY, "Trade Cart"},
        // todo: probably these three will need some kind of special handling when appending age names for units.txt
        {SIEGE_WORKSHOP_CUMAN_FEUDAL, "Siege Workshop (Cuman Feudal)"},
        {HOUSE_NOMADS_CASTLE, "House (Nomads Castle)"},
        {HOUSE_NOMADS_IMPERIAL, "House (Nomads Imperial)"},
        {TREBUCHET, "Trebuchet"}};

    enum UnitType { unit, building, tech };
    const std::unordered_map<int16_t, UnitType> unitTypeOverride = {{TREBUCHET, UnitType::unit}};

    enum Age { dark = 0, feudal = 1, castle = 2, imperial = 3 };
    const std::vector<Age> ages = {Age::dark, Age::feudal, Age::castle, Age::imperial};

    const uint8_t RESOURCE_MODIFIER = 1, ENABLE_DISABLE_UNIT = 2, UPGRADE_UNIT = 3;
    const int16_t CURRENT_AGE = 6;
    const int16_t SET = 0;
    const int16_t NONE = -1;
    const int16_t ALL = -1;
    const int16_t ENABLE = 1;

    std::unordered_map<int16_t, std::string> nameMap;
    std::ifstream fin(LANGUAGE_FILE_PATH);
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line.rfind("//", 0) == 0)
            continue;
        std::stringstream ss(line);
        int16_t id;
        std::string name;
        ss >> id >> std::quoted(name);
        nameMap[id] = name;
    }

    // todo: consider scrapping effect loop and just looping through all techs. why not? only one effect per tech.
    std::unordered_set<Age> agesNotSeen(ages.begin(), ages.end());
    std::unordered_map<int16_t, Age> ageEffects;
    std::unordered_map<int16_t, std::unordered_set<int16_t>> enableEffects;
    std::unordered_map<int16_t, std::unordered_map<int16_t, int16_t>> upgradeEffects;
    std::unordered_map<Age, std::unordered_set<int16_t>> unitsByAge;
    // consider looking for ALL upgrade effects & tracking those units
    //  might be needed e.g. to see house -> nomads house. is needed for cuman feudal SW -> SW.
    std::unordered_map<Age, std::unordered_map<int16_t, std::unordered_set<int16_t>>> ageUpgrades;
    // does not include tech-upgraded units
    std::unordered_map<int16_t, std::unordered_map<Age, std::unordered_set<int16_t>>> unitUpgradesByAge;
    for (int16_t effectID = 0; effectID < df->Effects.size(); ++effectID) {
        const std::vector<genie::EffectCommand> &commands = df->Effects[effectID].EffectCommands;
        for (const auto &c : commands) {
            if (!agesNotSeen.empty() && c.Type == RESOURCE_MODIFIER && c.A == CURRENT_AGE && c.B == SET &&
                c.C == NONE && std::trunc(c.D) == c.D) {
                const auto age = static_cast<Age>(c.D);
                if (const auto it = agesNotSeen.find(age); it != agesNotSeen.end()) {
                    agesNotSeen.erase(it);
                    ageEffects[effectID] = age;
                    // any upgrade effects side-by-side with the age-up effect
                    for (const auto &c2 : commands) {
                        if (c2.Type == UPGRADE_UNIT && c2.C == ALL) {  // todo: DRY
                            // todo: should i really put this unit in unitsByAge here?
                            // what if it's not enabled yet? e.g. monastery, town center
                            // i might need to track "enabledness" of a sequence of units more robustly
                            // through the ages.
                            // for now, i'll just exclude monastery & town center
                            // i'm tracking upgradeEffects distinctly now, so, this section should probably be
                            // refactored
                            unitsByAge[age].emplace(c2.B);
                            ageUpgrades[age][c2.A].emplace(c2.B);
                            unitUpgradesByAge[c2.A][age].emplace(c2.B);
                        }
                    }
                }
            } else if (c.Type == ENABLE_DISABLE_UNIT && c.B == ENABLE) {
                enableEffects[effectID].emplace(c.A);  // the enabled unit's ID
            } else if (c.Type == UPGRADE_UNIT && c.C == ALL) {
                upgradeEffects[effectID].emplace(c.A, c.B);
            }
        }
    }

    std::unordered_map<int16_t, std::unordered_set<int16_t>> techBuildingRequirements;

    const std::vector<genie::Unit> &units = df->Civs[0].Units;

    for (const auto &unit : units) {
        const bool hasTrainLocation = unit.Creatable.TrainLocationID != -1;  // todo: DRY
        if (unit.Building.TechID != -1) {
            // do i want ALL the archery ranges, or just one archery range? for now, all
            techBuildingRequirements[unit.Building.TechID].emplace(unit.ID);
        }
    }

    // std::multimap<int, std::string> unitsOutput;
    // std::multimap<std::tuple<int, int>, std::string> unitsOutput;
    // std::multimap<std::tuple<uint8_t, int16_t, std::string>, std::string> unitsOutput;
    std::multimap<std::tuple<int, uint8_t, int16_t, std::string>, std::string> unitsOutput;

    std::unordered_map<Age, std::unordered_set<int16_t>> ageBuildingRequirements;
    std::unordered_map<Age, int16_t> ageTech;

    agesNotSeen.insert(ages.begin(), ages.end());
    for (int16_t techID = 0; techID < df->Techs.size(); ++techID) {
        const genie::Tech &tech = df->Techs[techID];
        if (!agesNotSeen.empty()) {
            if (const auto it = ageEffects.find(tech.EffectID); it != ageEffects.end()) {
                const Age age = it->second;
                ageTech[age] = techID;
                agesNotSeen.erase(age);
                std::deque<int16_t> stack(tech.RequiredTechs.begin(), tech.RequiredTechs.end());
                while (!stack.empty()) {
                    const int16_t requiredID = stack.front();
                    stack.pop_front();
                    if (requiredID == -1) {
                        continue;
                    }
                    const genie::Tech &required = df->Techs[requiredID];
                    if (const auto it = ageEffects.find(required.EffectID); it != ageEffects.end()) {
                        continue;
                    }
                    const std::unordered_set<int16_t> &buildings = techBuildingRequirements[requiredID];
                    ageBuildingRequirements[age].insert(buildings.begin(), buildings.end());

                    stack.insert(stack.begin(), required.RequiredTechs.begin(), required.RequiredTechs.end());
                }
            }
        }

        // todo: now that we're tracking upgradeEffects,
        //  use it to ensure that cuman feudal siege workshops become regular siege workshops.
        //  i guess first i have to figure out what tech the upgrade effect is from, and then determine an age,
        //  like i'm doing for enable effects. this implies looping through techs more than once.
        //  might as well stop looping through effects separately at the same time, right?

        // for any techs with unit-enable effects, determine what age the units should be categorized under.
        if (const auto it = enableEffects.find(tech.EffectID); it != enableEffects.end()) {
            // if tech has no obvious age prereq, assume it's dark age (like polish folwark)
            // (this logic is not very robust)
            Age enabledBy = Age::dark;
            // if the tech is itself an age tech, then of course that's the right age.
            if (const auto it2 = ageEffects.find(tech.EffectID); it2 != ageEffects.end()) {
                enabledBy = it2->second;
            } else {
                // otherwise, breadth-first search the graph of required techs until you find an
                // age tech. we can't use the ageTech map because we haven't finished iterating through
                // all the techs yet, but we can use the ageEffects map.
                std::deque<int16_t> queue(tech.RequiredTechs.begin(), tech.RequiredTechs.end());
                while (!queue.empty()) {
                    const int16_t requiredID = queue.front();
                    queue.pop_front();
                    if (requiredID == -1) {
                        continue;
                    }
                    const genie::Tech &required = df->Techs[requiredID];
                    // if the directly-or-indirectly required tech is an age tech, then, that's the age we'll choose.
                    if (const auto it2 = ageEffects.find(required.EffectID); it2 != ageEffects.end()) {
                        enabledBy = it2->second;
                        break;
                    }
                    queue.insert(queue.end(), required.RequiredTechs.begin(), required.RequiredTechs.end());
                }
            }
            // track the units enabled in an age. but if they are also upgraded to other units in or before that age,
            // track the upgraded unit(s) instead (blacksmith, monastery)
            if (const auto it2 = ageUpgrades.find(enabledBy); it2 != ageUpgrades.end()) {
                // check each enabled unit
                for (const int16_t unitID : it->second) {
                    if (const auto it3 = it2->second.find(unitID); it3 != it2->second.end()) {
                        // todo: this line is partially redundant with adding units directly enabled by age tech
                        // in the earlier effect processing loop.
                        unitsByAge[enabledBy].insert(it3->second.begin(), it3->second.end());
                    } else {
                        unitsByAge[enabledBy].emplace(unitID);
                    }
                }
            } else {
                unitsByAge[enabledBy].insert(it->second.begin(), it->second.end());
            }
        }

        std::vector<ResearchResourceCost> researchResourceCosts = tech.ResourceCosts;
        if (hasNaturalResearchResourceCost(researchResourceCosts)) {
            if (tech.ResearchLocation > 0) {
                std::string name = nameMap[tech.LanguageDLLName];
                if (name.empty()) {
                    name = tech.Name;
                }

                int totalCost = 0;
                for (const ResearchResourceCost &cost : researchResourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                // const auto sortBy = std::tuple(-1, -1, name);
                // const auto sortBy = -1;
                // const auto sortBy = totalCost;
                const auto sortBy = std::tuple(UnitType::tech, -1, -1, name);
                // const auto sortBy = std::tuple(UnitType::tech, totalCost);

                const std::string type = "tech";

                const std::string info = std::to_string(techID);

                const std::string cost = costToString(researchResourceCosts) + " (" + std::to_string(totalCost) + ")";

                unitsOutput.emplace(sortBy, "Cost of " + type + " " + name + " [" + info + "] is " + cost);
            }
        }
    }

    for (const genie::Unit &unit : units) {
        if (hasNaturalResourceCost(unit)) {
            const bool hasTrainLocation = unit.Creatable.TrainLocationID != -1;
            if (unitsToInclude.find(unit.ID) != unitsToInclude.end() ||
                unitsToExclude.find(unit.ID) == unitsToExclude.end() && hasTrainLocation) {
                // if unit "available"/"enabled" from start without tech: dark age.
                if (unit.Enabled) {
                    unitsByAge[Age::dark].emplace(unit.ID);
                }

                std::string name;
                if (const auto it = unitNameOverride.find(unit.ID); it != unitNameOverride.end()) {
                    name = it->second;
                } else {
                    name = nameMap[unit.LanguageDLLName];
                }
                const std::string location =
                    hasTrainLocation ? nameMap[units[unit.Creatable.TrainLocationID].LanguageDLLName] : "n/a";

                int totalCost = 0;
                std::vector<ResourceCost> resourceCosts = unit.Creatable.ResourceCosts;
                for (const auto &cost : resourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                int type;
                if (const auto it = unitTypeOverride.find(unit.ID); it != unitTypeOverride.end()) {
                    type = it->second;
                } else {
                    type = unit.Type == genie::UnitType::UT_Building ? UnitType::building : UnitType::unit;
                }

                // const auto sortBy = std::tuple(unit.Type, unit.Class, name);
                // const auto sortBy = unit.Class;
                // const auto sortBy = totalCost;
                const auto sortBy = std::tuple(type, unit.Type, unit.Class, name);
                // const auto sortBy = std::tuple(type, totalCost);
                const std::string type_str = type == UnitType::building ? "building" : "unit";

                // const std::string info = std::to_string(unit.ID) + " " + unit.Name + " @ " + location;
                const std::string info = std::to_string(unit.Type) + " " + std::to_string(unit.Class) + " " +
                                         std::to_string(unit.ID) + " - " + unit.Name + " @ " + location;

                const std::string cost = costToString(resourceCosts) + " (" + std::to_string(totalCost) + ")";

                // todo: DRY
                unitsOutput.emplace(sortBy, "Cost of " + type_str + " " + name + " [" + info + "] is " + cost);
            }
        }
    }

    std::ofstream unitsFile(OUTPUT_PATH / "units.txt");
    for (const auto &entry : unitsOutput) {
        unitsFile << entry.second << std::endl;
    }

    std::map<Age, std::multimap<std::tuple<uint8_t, uint8_t>, std::string>> agesOutput;

    // todo: handle units here, not just buildings
    for (const auto &[age, ageUnits] : unitsByAge) {
        std::unordered_set<int16_t> nextAgeRequirements;
        if (const auto it = std::next(std::find(ages.begin(), ages.end(), static_cast<Age>(age))); it != ages.end()) {
            // find the next age, if exists
            const Age nextAge = *it;
            nextAgeRequirements = ageBuildingRequirements[nextAge];
        }
        for (const int16_t &unitID : ageUnits) {
            const genie::Unit &unit = units[unitID];
            if (unit.Type == genie::UnitType::UT_Building && (unitsToInclude.find(unit.ID) != unitsToInclude.end() ||
                                                              unitsToExclude.find(unit.ID) == unitsToExclude.end() &&
                                                                  unit.Creatable.TrainLocationID != -1)) {  // todo: DRY
                std::string name;                                                                           // todo: DRY
                if (const auto it = unitNameOverride.find(unitID); it != unitNameOverride.end()) {
                    name = it->second;
                } else {
                    name = nameMap[unit.LanguageDLLName];
                }
                std::string output;
                if (const auto it = std::find(nextAgeRequirements.begin(), nextAgeRequirements.end(), unitID);
                    it != nextAgeRequirements.end()) {
                    // mark buildings involved in age-up requirements
                    output += "(*) ";
                }
                output += name + " [" + std::to_string(unitID) + "]";
                // todo: add resource costs
                // sort by position of the build button (e.g. house, mill, mining camp, etc.)
                agesOutput[age].emplace(std::tuple(unit.InterfaceKind, unit.Creatable.ButtonID), output);
            }
        }
    }

    std::ofstream agesFile(OUTPUT_PATH / "ages.txt");
    for (const auto &[age, entries] : agesOutput) {
        // todo: probably add resource costs for age techs
        agesFile << nameMap[df->Techs[ageTech[age]].LanguageDLLName] << std::endl;
        for (const auto &entry : entries) {
            agesFile << '\t' << entry.second << std::endl;
        }
    }
}
