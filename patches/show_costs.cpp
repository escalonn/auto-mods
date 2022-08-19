#include "show_costs.h"
#include <algorithm>
#include <iomanip>
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

but what defines the fact that building a mill presumably triggers the "shadow mill -- age one" tech?

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
// todo: detect and flag cheap units todo: for every cheap unit, indicate the age & cost of its production
//  building at cheapest?
// todo: given villager m+f cost, indicate resource gathering ratios for 1 tc booming

// todo: consider making ranking system like [$], [$$], [$$$] to flag cheap things from expensive easier?
// todo: figure out wtf is up with the "vat?" and any other non fwgs resources

// todo: check apparently duplicate units listed in comment at top of showCosts function
// todo: check and document behavior of relevant civ bonuses

// todo: consider civ bonuses (like cuman ranges -75 wood) that allow infinite resources (kinda hard)
// todo: distinguish cheap units that are gated behind expensive techs from those that aren't (hard)


char const *const LANGUAGE_FILE_PATH =
    "/mnt/c/Program Files "
    "(x86)/Steam/steamapps/common/AoE2DE/resources/en/strings/key-value/key-value-strings-utf8.txt";
char const *const OUTPUT_PATH = "build/costs.txt";


namespace {

typedef genie::ResourceUsage<int16_t, int16_t, int16_t> ResourceCost;
typedef genie::ResourceUsage<int16_t, int16_t, uint8_t> ResearchResourceCost;


bool isNaturalResourceCost(const ResourceCost &cost) {
    return cost.Type != -1 && cost.Type < 4 && cost.Amount > 0 && cost.Flag == 1;
}


bool isNaturalResearchResourceCost(const ResearchResourceCost &cost) {
    return cost.Type != -1 && cost.Type < 4 && cost.Amount > 0 && cost.Flag == 1;
}


bool hasNaturalResourceCost(const genie::Unit &unit) {
    std::vector<ResourceCost> costs = unit.Creatable.ResourceCosts;
    return std::any_of(costs.begin(), costs.end(), isNaturalResourceCost);
}


bool hasNaturalResearchResourceCost(std::vector<ResearchResourceCost> costs) {
    return std::any_of(costs.begin(), costs.end(), isNaturalResearchResourceCost);
}


int getSumOfNaturalResourceCosts(const std::vector<ResourceCost> &resourceCosts) {
    int16_t sum = 0;
    for (const ResourceCost &cost : resourceCosts) {
        if (cost.Type > -1 && cost.Type < 4) {
            sum += cost.Amount;
        }
    }
    return sum;
}


ResearchResourceCost toResearchResourceCost(const ResourceCost &resourceCost) {
    ResearchResourceCost researchResourceCost;
    researchResourceCost.Type = resourceCost.Type;
    researchResourceCost.Amount = resourceCost.Amount;
    researchResourceCost.Flag = (bool)resourceCost.Flag;
    return researchResourceCost;
}


ResourceCost toResourceCost(const ResearchResourceCost &researchResourceCost) {
    ResourceCost resourceCost;
    resourceCost.Type = researchResourceCost.Type;
    resourceCost.Amount = researchResourceCost.Amount;
    resourceCost.Flag = (bool)researchResourceCost.Flag;
    return resourceCost;
}


std::vector<ResearchResourceCost> toResearchResourceCosts(const std::vector<ResourceCost> &resourceCosts) {
    std::vector<ResearchResourceCost> researchResourceCosts;
    researchResourceCosts.reserve(resourceCosts.size());
    for (const ResourceCost &resourceCost : resourceCosts) {
        if (resourceCost.Type < TYPE_POPULATION_HEADROOM) {
            researchResourceCosts.push_back(toResearchResourceCost(resourceCost));
        } else {
            ResearchResourceCost researchResourceCost;
            researchResourceCost.Type = -1;
            researchResourceCost.Amount = 0;
            researchResourceCost.Flag = false;
            researchResourceCosts.push_back(researchResourceCost);
        }
    }
    return researchResourceCosts;
}


std::vector<ResourceCost> toResourceCosts(const std::vector<ResearchResourceCost> &researchResourceCosts) {
    std::vector<ResourceCost> resourceCosts;
    resourceCosts.reserve(researchResourceCosts.size());
    for (const ResearchResourceCost &researchResourceCost : researchResourceCosts) {
        resourceCosts.push_back(toResourceCost(researchResourceCost));
    }
    return resourceCosts;
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

void showCosts(genie::DatFile *df) {
    std::unordered_set<int16_t> const unitsToInclude = {
        ID_VILLAGER_BASE_F,
    };

    // monastery * 4!
    // - ingame(+tt) castle: 31CRCH3
    // - ingame imp: 32CRCH4
    // - techtree: 104CRCH (lie)
    // - ingame techtree imp: 110 food?!?! (lie)
    // - genie hiddenineditor: all but 104CRCH hidden
    // - 104CRCH and 30CRCH2 appear irrelevant
    // ratha * 2!
    // elite ratha * 2!

    std::unordered_set<int16_t> const unitsToExclude = {
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

    std::unordered_map<int16_t, std::string> const unitNameOverride = {
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
        {BATTERING_RAM, "Battering Ram (Cuman Feudal)"},
        {ID_TRADE_CART_EMPTY, "Trade Cart"},
        {SIEGE_WORKSHOP_CUMAN_FEUDAL, "Siege Workshop (Cuman Feudal)"},
        {HOUSE_NOMADS_CASTLE, "House (Nomads Castle)"},
        {HOUSE_NOMADS_IMPERIAL, "House (Nomads Imperial)"},
        {TREBUCHET, "Trebuchet"}};


    int const UNIT = 0, BUILDING = 1, TECH = 2;
    std::unordered_map<int16_t, int> const unitTypeOverride = {{TREBUCHET, UNIT}};

    std::unordered_map<int, std::string> nameMap;
    std::ifstream fin(LANGUAGE_FILE_PATH);
    std::string line;
    while (getline(fin, line)) {
        if (line.empty() || line.rfind("//", 0) == 0)
            continue;
        std::stringstream ss(line);
        int id;
        std::string name;
        ss >> id >> std::quoted(name);
        nameMap[id] = name;
    }

    // std::multimap<int, std::string> output;
    std::multimap<std::tuple<int, int>, std::string> output;
    // std::multimap<std::tuple<uint8_t, int16_t, std::string>, std::string> output;
    // std::multimap<std::tuple<int, uint8_t, int16_t, std::string>, std::string> output;

    std::vector<genie::Unit> const units = df->Civs[0].Units;
    for (auto const &unit : units) {
        if (hasNaturalResourceCost(unit)) {
            bool const hasTrainLocation = unit.Creatable.TrainLocationID != -1;
            if (unitsToInclude.find(unit.ID) != unitsToInclude.end() ||
                unitsToExclude.find(unit.ID) == unitsToExclude.end() && hasTrainLocation) {
                std::string name;
                if (auto itr = unitNameOverride.find(unit.ID); itr != unitNameOverride.end()) {
                    name = itr->second;
                } else {
                    name = nameMap[unit.LanguageDLLName];
                }
                std::string const location =
                    hasTrainLocation ? nameMap[units[unit.Creatable.TrainLocationID].LanguageDLLName] : "n/a";

                auto totalCost = 0;
                std::vector<ResourceCost> resourceCosts = unit.Creatable.ResourceCosts;
                for (const auto &cost : resourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                int type;
                if (auto itr = unitTypeOverride.find(unit.ID); itr != unitTypeOverride.end()) {
                    type = itr->second;
                } else {
                    type = unit.Type == 80 ? BUILDING : UNIT;
                }

                // auto const sortBy = std::make_tuple(unit.Type, unit.Class, name);
                // auto const sortBy = unit.Class;
                // auto const sortBy = totalCost;
                // auto const sortBy = std::make_tuple(type, unit.Type, unit.Class, name);
                auto const sortBy = std::make_tuple(type, totalCost);
                std::string const type_str = type == BUILDING ? "building" : "unit";

                std::string const info = std::to_string(unit.ID) + " " + unit.Name + " @ " + location;
                // std::string const info = std::to_string(unit.Type) + " " + std::to_string(unit.Class) + " " +
                //                          std::to_string(unit.ID) + " - " + unit.Name + " @ " + location;

                std::string const cost = costToString(resourceCosts) + " (" + std::to_string(totalCost) + ")";

                output.insert({sortBy, "Cost of " + type_str + " " + name + " [" + info + "] is " + cost});
            }
        }
    }

    for (size_t techId = 0; techId < df->Techs.size(); ++techId) {
        auto const tech = df->Techs[techId];
        std::vector<ResearchResourceCost> researchResourceCosts = tech.ResourceCosts;
        if (hasNaturalResearchResourceCost(researchResourceCosts)) {
            if (tech.ResearchLocation > 0) {
                std::string name = nameMap[tech.LanguageDLLName];
                if (name.empty()) {
                    name = tech.Name;
                }

                auto totalCost = 0;
                for (const ResearchResourceCost &cost : researchResourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                // auto const sortBy = std::make_tuple(-1, -1, name);
                // auto const sortBy = -1;
                // auto const sortBy = totalCost;
                // auto const sortBy = std::make_tuple(TECH, -1, -1, name);
                auto const sortBy = std::make_tuple(TECH, totalCost);

                std::string const type = "tech";

                std::string const info = std::to_string(techId);

                std::string const cost = costToString(researchResourceCosts) + " (" + std::to_string(totalCost) + ")";

                output.insert({sortBy, "Cost of " + type + " " + name + " [" + info + "] is " + cost});
            }
        }
    }

    std::ofstream fout(OUTPUT_PATH);
    for (auto itr = output.begin(); itr != output.end(); ++itr) {
        fout << itr->second << std::endl;
    }
}
