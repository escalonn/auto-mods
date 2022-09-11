#include "show_costs.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <deque>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "genie/dat/DatFile.h"
#include "ids.h"

using namespace std::literals;

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

class BaseTest {
  public:
    BaseTest(int id) : id{id} {}
    BaseTest(const BaseTest &) = delete;
    BaseTest &operator=(const BaseTest &) = delete;
    BaseTest(BaseTest &&) = delete;
    BaseTest &operator=(BaseTest &&) = delete;
    virtual ~BaseTest() = default;

    // bool operator==(const BaseTest &other) const noexcept { return id == other.id; }

    int getId() const noexcept { return id; }

  private:
    const int id;
};

namespace std {
template <> struct hash<std::reference_wrapper<const BaseTest>> {
    size_t operator()(const BaseTest &item) const noexcept { return hash<int>{}(item.getId()); }
};

// template <> struct hash<BaseTest> {
//     size_t operator()(std::reference_wrapper<BaseTest> item) noexcept {
//         return hash<int>{}(item.get().getId());
//     }
// };

// template <> struct hash<BaseTest> { // must be ref to const
//     size_t operator()(std::reference_wrapper<BaseTest> item) noexcept {
//         return hash<int>{}(item.get().getId());
//     }
// };

// template <class T> struct equal_to<BaseTest> {
//     size_t operator()(const BaseTest &lhs, const BaseTest &rhs) const noexcept {
//         return equal_to<int>{}(lhs.getId(), rhs.getId());
//     }
// };
}  // namespace std

// void collect() { std::unordered_set<std::reference_wrapper<const BaseTest>> set; }

namespace {

constexpr const char *const languageFilePath{
    "/mnt/c/Program Files "
    "(x86)/Steam/steamapps/common/AoE2DE/resources/en/strings/key-value/key-value-strings-utf8.txt"};
constexpr const char *const outputPath{"build"};

enum class ItemType { unit, building, tech };

struct CreatableContext {
    CreatableContext(std::reference_wrapper<const std::unordered_map<int, std::string>> nameMap,
                     std::reference_wrapper<const std::unordered_set<int>> include,
                     std::reference_wrapper<const std::unordered_set<int>> exclude,
                     std::reference_wrapper<const std::unordered_map<int, std::string>> nameOverride,
                     std::reference_wrapper<const std::unordered_map<int, ItemType>> typeOverride)
        : nameMap{nameMap}, include{include}, exclude{exclude}, nameOverride{nameOverride}, typeOverride{typeOverride} {
    }

    std::reference_wrapper<const std::unordered_map<int, std::string>> nameMap;
    std::reference_wrapper<const std::unordered_set<int>> include;
    std::reference_wrapper<const std::unordered_set<int>> exclude;
    std::reference_wrapper<const std::unordered_map<int, std::string>> nameOverride;
    std::reference_wrapper<const std::unordered_map<int, ItemType>> typeOverride;
};

class Unit;

class Tech;

class Building;

// class Game {};

class AgeC {
  public:
    AgeC(float value) : value{value} {}
    float getValue() { return value; }

  private:
    float value;
};

class Cost {
  public:
    template <class T, class A, class E>
    explicit Cost(const std::vector<genie::ResourceUsage<T, A, E>> &data) noexcept
        : food{readResource(data, TYPE_FOOD)}, wood{readResource(data, TYPE_WOOD)}, gold{readResource(data, TYPE_GOLD)},
          stone{readResource(data, TYPE_STONE)} {}

    int sum() const noexcept { return food + wood + gold + stone; }

    bool empty() const noexcept { return sum() == 0; }

    std::string toString() const noexcept {
        const auto tokens{[this] {
            std::vector<std::string> val;
            if (food > 0) {
                static const auto foodName{"Food"s};
                val.emplace_back(std::to_string(food));
                val.emplace_back(foodName);
            }
            if (wood > 0) {
                static const auto woodName{"Wood"s};
                val.emplace_back(std::to_string(wood));
                val.emplace_back(woodName);
            }
            if (gold > 0) {
                static const auto goldName{"Gold"s};
                val.emplace_back(std::to_string(gold));
                val.emplace_back(goldName);
            }
            if (stone > 0) {
                static const auto stoneName{"Stone"s};
                val.emplace_back(std::to_string(stone));
                val.emplace_back(stoneName);
            }
            val.emplace_back('(' + std::to_string(sum()) + ')');
            return val;
        }()};
        const auto oss{[&] {
            std::ostringstream val;
            for (auto it{tokens.begin()}; it != tokens.end(); ++it) {
                if (it != tokens.begin())
                    val << ' ';
                val << *it;
            }
            return val;
        }()};
        return oss.str();
    }

  private:
    template <class T, class A, class E>
    static int readResource(const std::vector<genie::ResourceUsage<T, A, E>> &data, int type) noexcept {
        auto result{0};
        static constexpr E flagPaid{1};
        for (const auto &cost : data)
            if (cost.Flag == flagPaid && cost.Type == type)
                result += cost.Amount;
        return result;
    }

    int food;
    int wood;
    int gold;
    int stone;
};

template <class TGenie> class GenieItem {
  public:
    // todo: isn't there some way to implement these better?
    // template <class Key> struct Hash {
    //     // also, why can i access id directly when Key is Tech but not when Key is Unit...
    //     // size_t operator()(const Key &key) const noexcept { return std::hash<int>{}(key.id); }
    //     size_t operator()(const Key &key) const noexcept { return std::hash<int>{}(key.getID()); }
    // };

    template <class Key> struct Hash {
        // also, why can i access id directly when Key is Tech but not when Key is Unit...
        // size_t operator()(const Key &key) const noexcept { return std::hash<int>{}(key.id); }
        size_t operator()(const Key &key) const noexcept { return std::hash<int>{}(key.getID()); } // TODO segfaults for Tech ?? so... test calling it directly
    };

    template <class T> struct Equal {
        size_t operator()(const T &lhs, const T &rhs) const noexcept {
            return std::equal_to<int>{}(lhs.getID(), rhs.getID());
        }
    };

    GenieItem(const GenieItem &) = delete;
    GenieItem &operator=(const GenieItem &) = delete;
    GenieItem(GenieItem &&) = delete;
    GenieItem &operator=(GenieItem &&) = delete;
    virtual ~GenieItem() = default;

    // bool operator==(const GenieItem &lhs, const GenieItem &rhs) const noexcept {
    //     return lhs.id == rhs.id;
    // }

    // bool operator==(const GenieItem &rhs) const noexcept {
    //     return this == &rhs;
    // }

    int getID() const noexcept { return id; }

    const std::string &getName() const noexcept { return name; }

    const Cost &getCost() const noexcept { return cost; }

  protected:
    GenieItem(int id, std::string name, const Cost &cost, const TGenie &data)
        : id{id}, name{name}, cost{cost}, data{data} {}

    const TGenie &getData() const noexcept { return data; }

  private:
    const int id;
    const std::string name;
    const Cost cost;
    const std::reference_wrapper<const TGenie> data;
};

// todo: one creatable instance should hold a whole set of units that upgrade into each other?
// should hold a primary ID and then a collection of secondary IDs?
// should be pointed to by multiple entries in the units & buildings maps?
// no, that won't work, duh, i want to see 6 different houses etc...
class Creatable : public GenieItem<genie::Unit> {
  public:
    Creatable(const genie::Unit &data, const CreatableContext &context)
        : GenieItem{data.ID,
                    [&data, &context, this] {
                        if (const auto it{context.nameOverride.get().find(getID())};
                            it != context.nameOverride.get().end())
                            return it->second;
                        if (const auto it{context.nameMap.get().find(data.LanguageDLLName)};
                            it != context.nameMap.get().end())
                            return it->second;
                        return data.Name;
                    }(),
                    Cost{data.Creatable.ResourceCosts},
                    data},
          relevant{[&data, &context, this] {
              if (context.include.get().find(getID()) != context.include.get().end())
                  return true;
              if (context.exclude.get().find(getID()) != context.exclude.get().end())
                  return false;
              // this logic is bad for some "base units" that upgrade into others; town center 109 or monastery 104,
              // possibly others for techs
              const auto hasTrainLocation{data.Creatable.TrainLocationID != -1};
              if (!hasTrainLocation)  // && getID() != 109)
                  return false;
              if (getCost().empty())
                  return false;
              return true;
          }()} {}

    Creatable(const Creatable &) = delete;
    Creatable &operator=(const Creatable &) = delete;
    Creatable(Creatable &&) = delete;
    Creatable &operator=(Creatable &&) = delete;
    ~Creatable() override = default;

    static bool isBuilding(const genie::Unit &data, const CreatableContext &context) {
        if (const auto it{context.typeOverride.get().find(data.ID)}; it != context.typeOverride.get().end())
            return it->second == ItemType::building;
        return data.Type == genie::UnitType::UT_Building;
    }

    bool isRelevant() const noexcept { return relevant; }

    void init(const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
              const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
              const std::vector<std::reference_wrapper<const Tech>> &techs) {
        postInit(units, buildings, techs);
    }

  protected:
    virtual void
    postInit([[maybe_unused]] const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
             [[maybe_unused]] const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
             [[maybe_unused]] const std::vector<std::reference_wrapper<const Tech>> &techs) {}

  private:
    const bool relevant;
};

// todo: consistency between Id and ID

class Unit : public Creatable {
  public:
    Unit(const genie::Unit &data, const CreatableContext &creatableContext) : Creatable{data, creatableContext} {}

  protected:
    void postInit(const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
                  const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
                  const std::vector<std::reference_wrapper<const Tech>> &techs) override {
        // todo
        // Flemish Militia (1699): 109
        // Villager (Male) (83): 109
        // Missionary (775): 104
        // Monk (125): 104
        if (auto locationId{getData().Creatable.TrainLocationID}; locationId != -1)
            try {
                trainLocation = &buildings.at(locationId).get();
            } catch (const std::out_of_range &) {
                std::cout << getName() << " ("s << getID() << "): "s << locationId << '\n';
            }
    }

  private:
    Building const *trainLocation{};
};

class Building : public Creatable {
  public:
    Building(const genie::Unit &data, const CreatableContext &creatableContext) : Creatable{data, creatableContext} {}
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

// todo: consider adding more runtime assertions about assumptions like only one tech per age

class Tech : public GenieItem<genie::Tech> {
  public:
    Tech(int id, const genie::Tech &data, const genie::Effect *const effectDataPtr,
         const std::unordered_map<int, std::string> &nameMap)
        : Tech{id, data, nameMap, [&] {
                   Tech::EffectInfo val{};
                   if (!effectDataPtr)
                       return val;
                   for (const auto &command : effectDataPtr->EffectCommands) {
                       static constexpr uint8_t typeResourceModifier{1};
                       static constexpr auto resourceCurrentAge{6};
                       static constexpr auto modeSet{0};
                       static constexpr auto resourceMultiplyNone{-1};
                       static constexpr uint8_t typeEnableDisableUnit{2};
                       static constexpr auto modeEnable{1};
                       static constexpr uint8_t typeUpgradeUnit{3};
                       static constexpr auto modeAll{-1};
                       if (command.Type == typeResourceModifier && command.A == resourceCurrentAge) {
                           assert(command.B == modeSet && command.C == resourceMultiplyNone);
                           val.ageSet.emplace(command.D);
                       } else if (command.Type == typeEnableDisableUnit && command.B == modeEnable) {
                           val.unitIdsEnabled.emplace(command.A);  // the enabled unit's ID
                       } else if (command.Type == typeUpgradeUnit && command.C == modeAll) {
                           val.unitIdsUpgraded.try_emplace(command.A, command.B);
                       }
                   }
                   return val;
               }()} {}

    Tech(const Tech &) = delete;
    Tech &operator=(const Tech &) = delete;
    Tech(Tech &&) = delete;
    Tech &operator=(Tech &&) = delete;
    ~Tech() override = default;

    void init(const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
              const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
              const std::vector<std::reference_wrapper<const Tech>> &techs) {
        // todo: wrongly discarding 109 town center for example
        if (auto it{buildings.find(getData().ResearchLocation)}; it != buildings.end())
            researchLocation = &it->second.get();

        for (const auto &id : getData().RequiredTechs)
            if (id != -1)
                requiredTechs.emplace(techs[id]);

        for (const auto &id : unitIdsEnabled)
            unitsEnabled.emplace(units.at(id));

        for (const auto &[id1, id2] : unitIdsUpgraded)
            unitsUpgraded.try_emplace(units.at(id1), units.at(id1));
    }

    // should either store this as a field, or, expose it as an iterator?
    // maybe just expose methods like "hasRequiredTech"?
    const std::unordered_set<std::reference_wrapper<const Tech>, Hash<Tech>, Equal<Tech>> &
    getRequiredTechs() const noexcept {
        return requiredTechs;
    }

    const std::unordered_set<std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>> &
    getUnitsEnabled() const noexcept {
        return unitsEnabled;
    }

    const std::unordered_map<std::reference_wrapper<const Unit>, std::reference_wrapper<const Unit>, Hash<Unit>,
                             Equal<Unit>> &
    getUnitsUpgraded() const noexcept {
        return unitsUpgraded;
    }

    const std::optional<AgeC> ageSet;

  private:
    struct EffectInfo {
        std::optional<AgeC> ageSet{};
        std::unordered_set<int> unitIdsEnabled;
        std::unordered_map<int, int> unitIdsUpgraded;
    };

    Tech(int id, const genie::Tech &data, const std::unordered_map<int, std::string> &nameMap,
         const EffectInfo &effectInfo)
        : GenieItem{id,
                    [&] {
                        if (const auto it{nameMap.find(data.LanguageDLLName)}; it != nameMap.end())
                            return it->second;
                        return data.Name;
                    }(),
                    Cost{data.ResourceCosts},
                    data},
          ageSet{effectInfo.ageSet}, unitIdsEnabled{effectInfo.unitIdsEnabled}, unitIdsUpgraded{
                                                                                    effectInfo.unitIdsUpgraded} {}

    const std::unordered_set<int> unitIdsEnabled;
    const std::unordered_map<int, int> unitIdsUpgraded;
    const Building *researchLocation{};
    std::unordered_set<std::reference_wrapper<const Tech>, Hash<Tech>, Equal<Tech>> requiredTechs;
    std::unordered_set<std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>> unitsEnabled;
    std::unordered_map<std::reference_wrapper<const Unit>, std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>>
        unitsUpgraded;
};

// todo maybe we can build this list dynamically and have no need to hardcode?
// given the four age techs, either assume they're in numerical order, or put them in reverse order of requirements
// assert that there aren't duplicates or disconnected ages or whatever
enum class Age { dark = 0, feudal = 1, castle = 2, imperial = 3 };
}  // namespace

namespace std {
template <class T> struct hash<std::reference_wrapper<const GenieItem<T>>> {
    size_t operator()(const GenieItem<T> &item) const noexcept { return hash<int>{}(item.getID()); }
};

// template <class T> struct equal_to<GenieItem<T>> {
//     size_t operator()(const GenieItem<T> &lhs, const GenieItem<T> &rhs) const noexcept {
//         return equal_to<int>{}(lhs.id, rhs.id);
//     }
// };
}  // namespace std

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
    const std::unordered_set<int> creatablesToInclude{
        ID_VILLAGER_BASE_F,
    };

    const std::unordered_set<int> creatablesToExclude{
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
        // well, i think we actually need MONASTERY_DARK (104) from input to understand that monks are built there
        // what if this data structure became just for filtering output, and we filtered input by type and class?
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

    const std::unordered_map<int, std::string> nameOverride{
        {HUSKARL_BARRACKS, "Huskarl (Barracks)"s},
        {ELITE_HUSKARL_BARRACKS, "Elite Huskarl (Barracks)"s},
        {SERJEANT, "Serjeant (Castle)"s},
        {ELITE_SERJEANT, "Elite Serjeant (Castle)"s},
        {DSERJEANT, "Serjeant (Donjon)"s},
        {ELITE_DSERJEANT, "Elite Serjeant (Donjon)"s},
        {TARKAN_STABLE, "Tarkan (Stable)"s},
        {ELITE_TARKAN_STABLE, "Elite Tarkan (Stable)"s},
        {KONNIK, "Konnik (Castle)"s},
        {ELITE_KONNIK, "Elite Konnik (Castle)"s},
        {KONNIK_2, "Konnik (Krepost)"s},
        {ELITE_KONNIK_2, "Elite Konnik (Krepost)"s},
        // todo: maybe need some special handling for buildings.txt
        {BATTERING_RAM, "Battering Ram (Cuman Feudal)"s},
        {ID_TRADE_CART_EMPTY, "Trade Cart"s},
        {RATHA_RANGED, "Ratha"s},
        {ELITE_RATHA_RANGED, "Elite Ratha"s},
        {ID_TRADE_CART_EMPTY, "Trade Cart"s},
        // todo: probably these three will need some kind of special handling when appending age names for units.txt
        {SIEGE_WORKSHOP_CUMAN_FEUDAL, "Siege Workshop (Cuman Feudal)"s},
        {HOUSE_NOMADS_CASTLE, "House (Nomads Castle)"s},
        {HOUSE_NOMADS_IMPERIAL, "House (Nomads Imperial)"s},
        {TREBUCHET, "Trebuchet"s}};

    const std::unordered_map<int, ItemType> typeOverride{{TREBUCHET, ItemType::unit}};

    const auto nameMap{[] {
        std::unordered_map<int, std::string> val;
        std::ifstream fin{languageFilePath};
        std::string line;
        while (std::getline(fin, line)) {
            if (line.empty() || line.rfind("//", 0) == 0)
                continue;
            std::istringstream ss{line};
            int id{};
            std::string name;
            ss >> id >> std::quoted(name);
            val.try_emplace(id, name);
        }
        return val;
    }()};

    const auto [units, buildings, techs]{[&] {
        const CreatableContext creatableContext{
            nameMap, creatablesToInclude, creatablesToExclude, nameOverride, typeOverride};
        std::unordered_map<int, std::shared_ptr<Unit>> mutableUnitPtrs;
        std::unordered_map<int, std::reference_wrapper<const Unit>> unitRefs;
        std::unordered_map<int, std::shared_ptr<Building>> mutableBuildingPtrs;
        std::unordered_map<int, std::reference_wrapper<const Building>> buildingRefs;
        for (const auto &data : df->Civs[0].Units) {
            if (Creatable::isBuilding(data, creatableContext)) {
                const auto buildingPtr{std::make_shared<Building>(data, creatableContext)};
                if (!buildingPtr->isRelevant())
                    continue;
                mutableBuildingPtrs.try_emplace(buildingPtr->getID(), buildingPtr);
                buildingRefs.try_emplace(buildingPtr->getID(), *buildingPtr);
            } else {
                const auto unitPtr{std::make_shared<Unit>(data, creatableContext)};
                if (!unitPtr->isRelevant())
                    continue;
                mutableUnitPtrs.try_emplace(unitPtr->getID(), unitPtr);
                unitRefs.try_emplace(unitPtr->getID(), *unitPtr);
            }
        }
        std::vector<std::shared_ptr<Tech>> mutableTechPtrs;
        std::vector<std::reference_wrapper<const Tech>> techRefs;
        for (size_t id{0}; id < df->Techs.size(); ++id) {
            const auto tech{df->Techs[id]};
            const genie::Effect *effectPtr{};
            if (tech.EffectID != -1)
                effectPtr = &df->Effects[tech.EffectID];
            const auto techPtr{std::make_shared<Tech>(id, tech, effectPtr, nameMap)};
            mutableTechPtrs.emplace_back(techPtr);
            techRefs.emplace_back(*techPtr);
        }

        // todo: fix segfault
        // for (const auto &techPtr : mutableTechPtrs)
        //     techPtr->init(unitRefs, buildingRefs, techRefs);
        // for (const auto &entry : mutableUnitPtrs)
        //     entry.second->init(unitRefs, buildingRefs, techRefs);
        // for (const auto &entry : mutableBuildingPtrs)
        //     entry.second->init(unitRefs, buildingRefs, techRefs);

        return std::tuple{
            std::unordered_map<int, std::shared_ptr<const Unit>>{mutableUnitPtrs.begin(), mutableUnitPtrs.end()},
            std::unordered_map<int, std::shared_ptr<const Building>>{mutableBuildingPtrs.begin(),
                                                                     mutableBuildingPtrs.end()},
            std::vector<std::shared_ptr<const Tech>>{mutableTechPtrs.begin(), mutableTechPtrs.end()}};
    }()};

    // const auto &tech{techs.at(0)};
    // const auto &tech2{techs.at(0)};
    // const auto equal{tech == tech2};

    const std::vector<Age> ages{Age::dark, Age::feudal, Age::castle, Age::imperial};

    // todo: consider scrapping effect loop and just looping through all techs. why not? only one effect per tech.
    // todo: when refactoring, these should be const after initialization
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
    for (size_t effectID{0}; effectID < df->Effects.size(); ++effectID) {
        const auto &commands = df->Effects[effectID].EffectCommands;  // todo DRY
        for (const auto &c : commands) {
            static constexpr uint8_t typeResourceModifier{1};
            static constexpr uint8_t typeEnableDisableUnit{2};
            static constexpr uint8_t typeUpgradeUnit{3};
            static constexpr int16_t resourceCurrentAge{6};
            static constexpr int16_t modeSet{0};
            static constexpr int16_t resourceMultiplyNone{-1};
            static constexpr int16_t modeEnable{1};
            static constexpr int16_t modeAll{-1};
            if (!agesNotSeen.empty() && c.Type == typeResourceModifier && c.A == resourceCurrentAge && c.B == modeSet &&
                c.C == resourceMultiplyNone && std::trunc(c.D) == c.D) {
                const auto age = static_cast<Age>(c.D);
                if (const auto it{agesNotSeen.find(age)}; it != agesNotSeen.end()) {
                    agesNotSeen.erase(it);
                    ageEffects.try_emplace(effectID, age);
                    // any upgrade effects side-by-side with the age-up effect
                    for (const auto &c2 : commands)
                        if (c2.Type == typeUpgradeUnit && c2.C == modeAll) {  // todo: DRY
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
            } else if (c.Type == typeEnableDisableUnit && c.B == modeEnable) {
                enableEffects[effectID].emplace(c.A);  // the enabled unit's ID
            } else if (c.Type == typeUpgradeUnit && c.C == modeAll) {
                upgradeEffects[effectID].emplace(c.A, c.B);
            }
        }
    }

    std::unordered_map<int16_t, std::unordered_set<int16_t>> techBuildingRequirements;

    const auto &unitData{df->Civs[0].Units};

    for (const auto &unit : unitData) {
        // const auto hasTrainLocation{unit.Creatable.TrainLocationID != -1};  // todo: DRY
        if (unit.Building.TechID != -1)
            // do i want ALL the archery ranges, or just one archery range? for now, all
            techBuildingRequirements[unit.Building.TechID].emplace(unit.ID);
    }

    // std::multimap<int, std::string> unitsOutput;
    // std::multimap<std::tuple<int, int>, std::string> unitsOutput;
    // std::multimap<std::tuple<uint8_t, int16_t, std::string>, std::string> unitsOutput;
    std::multimap<std::tuple<int, uint8_t, int16_t, std::string, int>, std::string> unitsOutput;

    std::unordered_map<Age, std::unordered_set<int16_t>> ageBuildingRequirements;
    std::unordered_map<Age, int16_t> ageTech;

    agesNotSeen.insert(ages.begin(), ages.end());
    for (size_t techID{0}; techID < df->Techs.size(); ++techID) {
        const auto &tech{df->Techs[techID]};
        if (!agesNotSeen.empty()) {
            if (const auto it{ageEffects.find(tech.EffectID)}; it != ageEffects.end()) {
                const auto age{it->second};
                ageTech.try_emplace(age, techID);
                agesNotSeen.erase(age);
                std::deque<int16_t> stack(tech.RequiredTechs.begin(), tech.RequiredTechs.end());
                while (!stack.empty()) {
                    const auto requiredID{stack.front()};
                    stack.pop_front();
                    if (requiredID == -1)
                        continue;
                    const auto &required{df->Techs[requiredID]};
                    if (const auto it2{ageEffects.find(required.EffectID)}; it2 != ageEffects.end())
                        continue;
                    const auto &buildings(techBuildingRequirements[requiredID]);
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
        if (const auto it{enableEffects.find(tech.EffectID)}; it != enableEffects.end()) {
            // (this logic is not very robust)
            const auto enabledBy{[&] {
                // if the tech is itself an age tech, then of course that's the right age.
                if (const auto it2{ageEffects.find(tech.EffectID)}; it2 != ageEffects.end())
                    return it2->second;
                // otherwise, breadth-first search the graph of required techs until you find an
                // age tech. we can't use the ageTech map because we haven't finished iterating through
                // all the techs yet, but we can use the ageEffects map.
                std::deque<int16_t> queue(tech.RequiredTechs.begin(), tech.RequiredTechs.end());
                while (!queue.empty()) {
                    const auto requiredID{queue.front()};
                    queue.pop_front();
                    if (requiredID == -1)
                        continue;
                    const auto &required{df->Techs[requiredID]};
                    // if the directly-or-indirectly required tech is an age tech, then, that's the age we'll
                    // choose.
                    if (const auto it3{ageEffects.find(required.EffectID)}; it3 != ageEffects.end())
                        return it3->second;
                    queue.insert(queue.end(), required.RequiredTechs.begin(), required.RequiredTechs.end());
                }
                // if tech has no obvious age prereq, assume it's dark age (like polish folwark)
                return Age::dark;
            }()};
            // track the units enabled in an age. but if they are also upgraded to other units in or before that age,
            // track the upgraded unit(s) instead (blacksmith, monastery)
            if (const auto it2{ageUpgrades.find(enabledBy)}; it2 != ageUpgrades.end()) {
                // check each enabled unit
                for (const auto unitID : it->second)
                    if (const auto it3{it2->second.find(unitID)}; it3 != it2->second.end()) {
                        // todo: this line is partially redundant with adding units directly enabled by age tech
                        // in the earlier effect processing loop.
                        unitsByAge[enabledBy].insert(it3->second.begin(), it3->second.end());
                    } else {
                        unitsByAge[enabledBy].emplace(unitID);
                    }
            } else {
                unitsByAge[enabledBy].insert(it->second.begin(), it->second.end());
            }
        }

        const Cost cost{tech.ResourceCosts};
        if (!cost.empty()) {
            if (tech.ResearchLocation > 0) {
                const auto name{[&] {
                    if (const auto it = nameMap.find(tech.LanguageDLLName); it != nameMap.end())
                        return it->second;
                    return tech.Name;
                }()};

                // const auto sortBy{std::tuple{0, 0, 0, name, 0}};
                // const auto sortBy{std::tuple{0, 0, 0, ""s, 0}};
                // const auto sortBy{std::tuple{0, 0, 0, ""s, cost.sum()}};
                const auto sortBy{std::tuple{static_cast<int>(ItemType::tech), 0, 0, name, 0}};
                // const auto sortBy{std::tuple{static_cast<int>(ItemType::tech), 0, 0, ""s, cost.sum()}};

                const auto type{"tech"s};

                const auto info{std::to_string(techID)};

                unitsOutput.emplace(sortBy,
                                    "Cost of "s + type + " "s + name + " ["s + info + "] is "s + cost.toString());
            }
        }
    }

    for (const auto &unit : unitData) {
        const Cost cost{unit.Creatable.ResourceCosts};
        if (!cost.empty()) {
            const auto hasTrainLocation{unit.Creatable.TrainLocationID != -1};
            if (creatablesToInclude.find(unit.ID) != creatablesToInclude.end() ||
                (creatablesToExclude.find(unit.ID) == creatablesToExclude.end() && hasTrainLocation)) {
                // if unit "available"/"enabled" from start without tech: dark age.
                if (unit.Enabled)
                    unitsByAge[Age::dark].emplace(unit.ID);

                const auto name{[&] {
                    if (const auto it{nameOverride.find(unit.ID)}; it != nameOverride.end())
                        return it->second;
                    if (const auto it{nameMap.find(unit.LanguageDLLName)}; it != nameMap.end())
                        return it->second;
                    return unit.Name;
                }()};

                // todo: generalize the name getting stuff
                const auto location{
                    hasTrainLocation ? nameMap.at(unitData[unit.Creatable.TrainLocationID].LanguageDLLName) : "n/a"s};

                const auto type{[&] {
                    if (const auto it{typeOverride.find(unit.ID)}; it != typeOverride.end())
                        return it->second;
                    return unit.Type == genie::UnitType::UT_Building ? ItemType::building : ItemType::unit;
                }()};

                // const auto sortBy{std::tuple{0, unit.Type, unit.Class, name, 0}};
                // const auto sortBy{std::tuple{0, 0, unit.Class, ""s, 0}};
                // const auto sortBy{std::tuple{0, 0, 0, ""s, cost.sum()}};
                // const auto sortBy{std::tuple{static_cast<int>(type), unit.Type, unit.Class, name, 0}};
                const auto sortBy{std::tuple{static_cast<int>(type), 0, 0, ""s, cost.sum()}};
                const auto typeStr{type == ItemType::building ? "building"s : "unit"s};

                // const auto info = std::to_string(unit.ID);
                // const auto info = std::to_string(unit.ID) + " "s + unit.Name;
                // const auto info = std::to_string(unit.ID) + " "s + unit.Name + " @ "s + location;
                const auto info{std::to_string(unit.Type) + " "s + std::to_string(unit.Class) + " "s +
                                std::to_string(unit.ID) + " - "s + unit.Name + " @ "s + location};

                // todo: DRY
                unitsOutput.emplace(sortBy,
                                    "Cost of "s + typeStr + " "s + name + " ["s + info + "] is "s + cost.toString());
            }
        }
    }

    std::filesystem::path outputFsPath{outputPath};
    {
        std::ofstream unitsFile{outputFsPath / "units.txt"};
        for (const auto &entry : unitsOutput)
            unitsFile << entry.second << '\n';
    }

    std::map<Age, std::multimap<std::tuple<uint8_t, uint8_t>, std::string>> agesOutput;

    // todo: handle units here, not just buildings
    for (const auto &[age, ageUnits] : unitsByAge) {
        std::unordered_set<int16_t> nextAgeRequirements;
        if (const auto it{std::next(std::find(ages.begin(), ages.end(), static_cast<Age>(age)))}; it != ages.end()) {
            // find the next age, if exists
            const auto nextAge{*it};
            nextAgeRequirements = ageBuildingRequirements[nextAge];
        }
        for (const auto &unitID : ageUnits) {
            const auto &unit{unitData[unitID]};
            if (unit.Type == genie::UnitType::UT_Building &&
                (creatablesToInclude.find(unit.ID) != creatablesToInclude.end() ||
                 (creatablesToExclude.find(unit.ID) == creatablesToExclude.end() &&
                  unit.Creatable.TrainLocationID != -1))) {  // todo: DRY
                const auto name{[&] {                        // todo: DRY
                    if (const auto it{nameOverride.find(unitID)}; it != nameOverride.end())
                        return it->second;
                    if (const auto it{nameMap.find(unit.LanguageDLLName)}; it != nameMap.end())
                        return it->second;
                    return unit.Name;
                }()};
                const auto output{[&] {
                    std::string val;
                    if (const auto it{std::find(nextAgeRequirements.begin(), nextAgeRequirements.end(), unitID)};
                        it != nextAgeRequirements.end()) {
                        // mark buildings involved in age-up requirements
                        val += "(*) "s;
                    }
                    val += name + " ["s + std::to_string(unitID) + "]"s;
                    return val;
                }()};
                // todo: add resource costs
                // sort by position of the build button (e.g. house, mill, mining camp, etc.)
                agesOutput[age].emplace(std::tuple(unit.InterfaceKind, unit.Creatable.ButtonID), output);
            }
        }
    }

    {
        std::ofstream agesFile{outputFsPath / "ages.txt"s};
        for (const auto &[age, entries] : agesOutput) {
            // todo: probably add resource costs for age techs
            agesFile << nameMap.at(df->Techs[ageTech[age]].LanguageDLLName) << '\n';
            for (const auto &entry : entries)
                agesFile << '\t' << entry.second << '\n';
        }
    }
}
