#include "show_costs.h"
#include <algorithm>
#include <iomanip>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "genie/dat/DatFile.h"
#include "ids.h"


// ln -s "/mnt/c/Program Files (x86)/Steam/steamapps/common/AoE2DE/resources/_common/dat/empires2_x2_p1.dat" de.dat
// ln -s "/mnt/c/Users/nicho/Games/Age of Empires 2 DE/76561197981945038/mods/subscribed/93975_Random Cost
// Mod/resources/_common/dat/empires2_x2_p1.dat" random-costs.dat

// ./create-data-mod show-costs ../de.dat
// ./create-data-mod show-costs ../random-costs.dat

const char *const LANGUAGE_FILE_PATH =
    "/mnt/c/Program Files "
    "(x86)/Steam/steamapps/common/AoE2DE/resources/en/strings/key-value/key-value-strings-utf8.txt";
const char *const OUTPUT_PATH = "costs.txt";


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
        if (cost.Flag == 1) {
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
                s += "vat?";
            }
        }
    }
    return s;
}


std::string costToString(const std::vector<ResearchResourceCost> &costs) {
    std::string s;
    for (const ResearchResourceCost &cost : costs) {
        if (cost.Flag == 1) {
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
                s += "vat?";
            }
        }
    }
    return s;
}

}  // namespace

void showCosts(genie::DatFile *df) {
    const std::set<int16_t> unitsToInclude = {
        ID_VILLAGER_BASE_F,
    };
    // ratha +e * 2!
    // battering ram * 2!
    // trade cart * 2!
    // house * 6!
    // monastery * 4!
    // trebuchet * 2!
    const std::set<int16_t> unitsToExclude = {
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
    };

    std::ifstream fin(LANGUAGE_FILE_PATH);
    std::map<int, std::string> nameMap;
    std::string line;
    int id;
    std::string name;
    while (getline(fin, line)) {
        if (line.empty() || line.rfind("//", 0) == 0)
            continue;
        std::stringstream ss(line);
        ss >> id >> std::quoted(name);
        nameMap[id] = name;
    }

    std::vector<std::vector<ResourceCost>> allTheCosts;
    std::multimap<int, std::string> output;
    // std::multimap<std::tuple<uint8_t, int16_t, std::string>, std::string> output;

    std::vector<genie::Unit> units = df->Civs.at(0).Units;
    std::vector<int> unitIds;
    for (genie::Unit unit : units) {
        if (hasNaturalResourceCost(unit)) {
            unitIds.push_back(unit.ID);

            std::vector<ResourceCost> resourceCosts = unit.Creatable.ResourceCosts;
            allTheCosts.push_back(resourceCosts);

            if (unitsToInclude.find(unit.ID) != unitsToInclude.end() ||
                unitsToExclude.find(unit.ID) == unitsToExclude.end() && unit.Creatable.TrainLocationID != -1) {
                std::string name = nameMap[unit.LanguageDLLName];
                std::string location = unit.Creatable.TrainLocationID != -1
                                           ? nameMap[units.at(unit.Creatable.TrainLocationID).LanguageDLLName]
                                           : "n/a";
                int totalCost = 0;
                for (const ResourceCost &cost : resourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                // auto sortBy = std::make_tuple(unit.Type, unit.Class, name);
                // auto sortBy = unit.Class;
                auto sortBy = totalCost;

                // output.insert({sortBy,
                //                "Cost of unit " + name + " (" + std::to_string(unit.Type) + " " +
                //                    std::to_string(unit.Class) + " " + std::to_string(unit.ID) + " - " + unit.Name +
                //                    ") is " + costToString(resourceCosts) + "(" + std::to_string(totalCost) + ")"});
                output.insert({sortBy,
                               "Cost of unit " + name + " [" + std::to_string(unit.ID) + " " + unit.Name + " @ " +
                                   location + "] is " + costToString(resourceCosts) + " (" + std::to_string(totalCost) +
                                   ")"});
            }
        }
    }

    std::vector<int> techIds;
    size_t techId = 0;
    for (const genie::Tech &tech : df->Techs) {
        std::vector<ResearchResourceCost> researchResourceCosts = tech.ResourceCosts;
        if (hasNaturalResearchResourceCost(researchResourceCosts)) {
            techIds.push_back(techId);
            allTheCosts.push_back(toResourceCosts(researchResourceCosts));

            if (tech.ResearchLocation > 0) {
                std::string name = nameMap[tech.LanguageDLLName];
                int totalCost = 0;
                for (const ResearchResourceCost &cost : researchResourceCosts) {
                    if (cost.Type != -1 && cost.Type < 4) {
                        totalCost += cost.Amount;
                    }
                }

                // auto sortBy = std::make_tuple(-1, -1, name);
                // auto sortBy = -1;
                auto sortBy = totalCost;

                output.insert({sortBy,
                               "Cost of tech " + name + " [" + std::to_string(techId) + " " + tech.Name + "] is " +
                                   costToString(researchResourceCosts) + " (" + std::to_string(totalCost) + ")"});
            }
        }
        techId++;
    }

    std::ofstream fout(OUTPUT_PATH);
    for (auto itr = output.begin(); itr != output.end(); ++itr) {
        fout << itr->second << std::endl;
    }
}
