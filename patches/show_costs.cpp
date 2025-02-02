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
and an "effect" (e.g. upgrade archers to crossbowmen)
crossbowmen can't be built until the tech is researched,
because they start out without a "build location" of their own,
and the upgrade effect lets them inherit the build location of archers.
if the tech has no research location, it is automatically
researched once all its required techs are researched.

markets require having built a mill,
because markets start disabled, but
mills are marked "initiates technology" for the "shadow mill" tech,
and there is a no-research-location "enable market" tech
requiring the shadow mill tech and the feudal age tech,
having an effect to enable the market.

buildings required for age-up are defined as required techs for the age tech.

the upgrade effect will, at runtime,
copy the trainable/researchable units/techs from the one to the other when they are buildings.
todo consider testing this, e.g. upgrade dark age barracks to feudal age archery range or something.

dunno what would happen if you upgraded from one unit to another
that didn't share the same train location and train button ID.
todo test?

enabling scout cavalry disables xolotl warrior,
since they have the same train location and same train button ID.

setting a tech costs to 0 (and/or research time to 0?) results in it
being automatically researched if/when its requirements are met
(e.g. italian team bonus -> condottiero, magyar bonus -> free forging etc)
todo test this?
    could not reproduce problem with condotierro... maybe only research time matters?

disabling an (active) tech that has an effect that disables a tech
results in enabling the second tech?
e.g. vietnamese team bonus, enabling imperial skirm

every civ is linked to a tech tree effect for disabling techs and some attribute changes
other civ bonuses, maybe because they need to interact with other techs,
    use techs named c-bonus with civ requirement and no cost.

most civ bonuses should apply AFTER random costs changes,
so anything implemented as an additive decrease in cost might give infinite resources,
but only if they do still have some cost of that resource type that is smaller than the magnitude of decrease.

unclear how post-imperial age is implemented.
does it automatically research all techs that have any cost at all, if the requirements are fulfilled?

when tech costs are scrambled, civ/team bonuses based on
auto-researching tech should fail most of the time,
since they only set food cost to zero.
test this: berber team, italians team, chinese team.
tech effects change tech costs by setting individual cost types (+/-/set).
likely to override or fail in the presence of random costs changes.
tech effects change unit costs by setting or multiplying special attributes for costs (100,103-106)
for units or classes of units.
likely to apply AFTER random costs changes.

todo test overflow breaking point for spies. does it overflow separately for each resource type?

any-civ techs involving costs:
supplies (militia line cost -food)
    maybe <=15 free food from militia line
    todo IN DOUBT?
shipwright (less ship wood cost via multiply)

civ/team bonuses involving costs:
berber team (genitour unlocked)
    probably broken if hidden genitour tech got more than food cost
    COULD NOT REPRODUCE
bohemians (blacksmiths, universities -wood)
    maybe free <=100 wood in each age from building blacksmiths, universities
bohemian ut hussite reforms (monks, monastery techs cost +food, -gold)
    maybe <=100 free gold from monk
    probably 500-1000 free gold from cancelling heresy (castle), faith (imperial))
bulgarians (blacksmith, sw techs -food)
    probably little free food in every age?
    GOOD, CONFIRMED
burmese (eco techs cost -food)
    probably free food
burmese (stable techs cost -food, -gold)
    probably ~100 free food, gold
burmese (monastery techs cost -gold)
    probably ~200 free gold from cancelling redemption (castle), ~400 from faith (imperial)
    GOOD, CONFIRMED
byzantine (town watch food cost set to 0, town patrol food & gold cost set to 0)
byzantine (imperial age cost -gold -food)
    probably 200+ free food & gold from imperial age
    GOOD, CONFIRMED
chinese team (more food on farms)
    probably will fail because of extra costs on hidden tech
cumans (archery range, stable costs -wood)
    maybe little bit of free wood in any age
dravidians (barracks techs cost -food -gold)
    probably ~100 free food, gold
french (horse collar line food, wood cost set to 0)
gurjara ut kshatriyas (less military food cost via multiply)
    kshatriyas+supplies (militia line food cost set to 30)
italians (age up food costs set to 425, 680, 850; gold costs set to 170, 680)
    instead of cheaper age up, probably more expensive age up
italians (dock/university tech costs -food,-wood,-gold)
    a LOT of techs that could give free resources. galleon food/wood, dry dock food/gold, e cannon gal f/g, shipwright f/g
    heated shot, treadmill crane food
    OKAY, CONFIRMED
italians ut silk road (less trade unit cost via multiply)
italians team (condotierro unlocked)
    probably broken if hidden condotierro tech got more than food cost
    COULD NOT REPRODUCE
magyars (forging line food/gold cost set to 0, time set to 0)
    instead of free forging line, may result in tech costing only wood/stone and researching instantly
    todo test
magyar ut corvinian army (magyar huszar gold cost set to 0)
malay ut forced levy (militia line cost +food, -gold)
    maybe <=20 free gold from militia line
persian ut kamandaran (archer line cost -gold, +wood)
    maybe <=45 free gold from archer line
    OKAY, CONFIRMED
pole ut szlachta privileges (less knight line gold cost via multiply)
saracens(market cost -wood)
    probably free <=100 wood from building market in one age or another
    OKAY, CONFIRMED (get free wood when placed, lose it when deleted unless it's fully built)
sicilians team (less transport ship cost via multiply)
slavs (supplies food/gold cost set to 0, time set to 0)
    instead of free supplies, may result in tech costing only wood/stone and researching instantly
slav ut detinets (castle/tower cost +wood, -stone)
    probably <=50 free stone from towers, <=260 free stone from castles
teutons (murder holes, herbal medicine costs set to 0)
turks (chemistry, light cav, hussar costs set to 0)
turks (cannon galleon tech, bombard tower tech cost -food -wood)
    probably 150+ free food/wood from cannon galleon tech, bombard tower tech
turks (e cannon galleon tech cost -food -gold)
    probably 200+ free food/gold from e cannon galleon tech
vikings (wheelbarrow, hand cart costs set to 0)
vikings (less warship cost via multiply)
vikings team (less dock cost via multiply)

todo check supplies


todo tech 606 seems to be a redundant? way to upgrade to elite genitour in post-imperial; should break some of the time.
todo check what enables tech 790,791, it enabling winged hussar should break some of the time
    unclear

most of the techs that replace one kind of cost with another for some unit or tech
    provide an opportunity for free resources of the first kind by queuing/placing
    and then cancelling it

most of the techs that reduce cost(s) by a percentage work normally on the
    correct units, after the costs were randomized.

most of the civ bonuses for "free techs" work by setting the usual costs of those
    techs to 0 & research time to 0. if they got randomized to have
    different other resource type costs, they won't be free and have to be researched manually (instantly)
    probably.

todo tabulate units trainable by each civilization, to see who has more
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

namespace {

constexpr const char *languageFilePath{
    "/mnt/c/Program Files "
    "(x86)/Steam/steamapps/common/AoE2DE/resources/en/strings/key-value/key-value-strings-utf8.txt"};
constexpr const char *outputPath{"build"};

enum class ItemType { unit, building, tech };

// todo rename to Age when done with existing Age type?
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

class Unit;

class Tech;

class Building;

template <class TGenie> class GenieItem {
  public:
    template <class T> struct Hash {
        std::size_t operator()(const std::reference_wrapper<const T> &key) const noexcept {
            return std::hash<const T *>{}(&key.get());
        }
    };
    template <class T> struct Equal {
        std::size_t operator()(const std::reference_wrapper<const T> &lhs,
                               const std::reference_wrapper<const T> &rhs) const noexcept {
            return std::equal_to<const T *>{}(&lhs.get(), &rhs.get());
        }
    };

    GenieItem(const GenieItem &) = delete;
    GenieItem &operator=(const GenieItem &) = delete;
    GenieItem(GenieItem &&) = delete;
    GenieItem &operator=(GenieItem &&) = delete;
    virtual ~GenieItem() = default;

    void init(const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
              const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
              const std::vector<std::reference_wrapper<const Tech>> &techs) {
        assert(!initialized);
        postInit(units, buildings, techs);
        initialized = true;
    }

    int getID() const noexcept { return id; }

    const std::string &getName() const noexcept { return name; }

    const Cost &getCost() const noexcept { return cost; }

  protected:
    GenieItem(int id, std::string name, const Cost &cost, const TGenie &data)
        : id{id}, name{name}, cost{cost}, data{data} {}

    virtual void
    postInit([[maybe_unused]] const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
             [[maybe_unused]] const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
             [[maybe_unused]] const std::vector<std::reference_wrapper<const Tech>> &techs) {}

    const TGenie &getData() const noexcept { return data; }

    bool isInitialized() const noexcept { return initialized; }

  private:
    const int id;
    const std::string name;
    const Cost cost;
    const std::reference_wrapper<const TGenie> data;
    bool initialized{false};
};

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
              // this logic is bad for some "base units" that upgrade into others: town center 109 or monastery 104,
              // possibly others for techs
              const auto hasTrainLocation{data.Creatable.TrainLocationID != -1};
              if (!hasTrainLocation)  // && getID() != 109)
                  return false;
              if (getCost().empty())
                  return false;
              return true;
          }()} {}

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

  private:
    const bool relevant;
};

class Unit : public Creatable {
  public:
    Unit(const genie::Unit &data, const CreatableContext &creatableContext) : Creatable{data, creatableContext} {}

    const Building *getTrainLocation() const noexcept {
        assert(isInitialized());
        return trainLocation;
    }

    // todo remove maybe_unused
  protected:
    void postInit([[maybe_unused]] const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
                  const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
                  [[maybe_unused]] const std::vector<std::reference_wrapper<const Tech>> &techs) override {
        // todo
        // Flemish Militia (1699): 109
        // Villager (Male) (83): 109
        // Missionary (775): 104
        // Monk (125): 104
        if (auto locationID{getData().Creatable.TrainLocationID}; locationID != -1)
            try {
                trainLocation = &buildings.at(locationID).get();
            } catch (const std::out_of_range &) {
                std::cout << getName() << " ("s << getID() << "): "s << locationID << '\n';
            }
    }

  private:
    const Building *trainLocation{};
};

class Building : public Creatable {
  public:
    Building(const genie::Unit &data, const CreatableContext &creatableContext) : Creatable{data, creatableContext} {}
};

// things i want to be able to see on this class...
// - what is its name (via language dll or internal name)? done
// - what is its resource cost? done
// - what building is it researched at, if any? done
// - is it an age tech? for what age? done, maybe
// - what required tech objects does it have? done (not transitive)
//   - whose job is it to decide to construct the required tech objects? i guess we need
//     a global list of techs by ID to draw from? should the constructor register them in that list?
//     done
// - what units does it upgrade, and to what? done (not unit group)
// - what units does it enable? done (not unit group)
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
    Tech(int id, const genie::Tech &data, const genie::Effect *effectData,
         const std::unordered_map<int, std::string> &nameMap)
        : Tech{id, data, nameMap, [&] {
                   Tech::EffectInfo val{};
                   if (!effectData)
                       return val;
                   static constexpr uint8_t typeResourceModifier{1};
                   static constexpr auto resourceCurrentAge{6};
                   static constexpr auto modeSet{0};
                   static constexpr auto resourceMultiplyNone{-1};
                   static constexpr uint8_t typeEnableDisableUnit{2};
                   static constexpr auto modeEnable{1};
                   static constexpr uint8_t typeUpgradeUnit{3};
                   static constexpr auto modeAll{-1};
                   for (const auto &command : effectData->EffectCommands)
                       if (command.Type == typeResourceModifier && command.A == resourceCurrentAge) {
                           assert(command.B == modeSet && command.C == resourceMultiplyNone);
                           // float value representing age (dark=0 to imperial=3)
                           val.ageSet.emplace(command.D);
                       } else if (command.Type == typeEnableDisableUnit && command.B == modeEnable)
                           val.unitIDsEnabled.emplace(command.A);
                       else if (command.Type == typeUpgradeUnit && command.C == modeAll)
                           val.unitIDsUpgraded.try_emplace(command.A, command.B);
                   return val;
               }()} {}

    void postInit(const std::unordered_map<int, std::reference_wrapper<const Unit>> &units,
                  const std::unordered_map<int, std::reference_wrapper<const Building>> &buildings,
                  const std::vector<std::reference_wrapper<const Tech>> &techs) override {
        // todo: wrongly discarding 109 town center for example
        if (auto it{buildings.find(getData().ResearchLocation)}; it != buildings.end())
            researchLocation = &it->second.get();

        for (const auto &id : getData().RequiredTechs)
            if (id != -1)
                requiredTechs.emplace(techs.at(id));

        for (const auto &id : unitIDsEnabled)
            if (auto it{units.find(id)}; it != units.end())
                unitsEnabled.emplace(it->second);
            else if (auto it2{buildings.find(id)}; it2 != buildings.end())
                buildingsEnabled.emplace(it2->second);
        // others are irrelevant

        for (const auto &[id1, id2] : unitIDsUpgraded)
            if (auto it{units.find(id1)}; it != units.end()) {
                if (auto it2{units.find(id2)}; it2 != units.end())
                    unitsUpgraded.try_emplace(it->second, it2->second);
            } else if (auto it2{buildings.find(id1)}; it2 != buildings.end())
                if (auto it3{buildings.find(id2)}; it3 != buildings.end())
                    buildingsUpgraded.try_emplace(it2->second, it3->second);
        // others are irrelevant
    }

    const Building *getResearchLocation() const noexcept {
        assert(isInitialized());
        return researchLocation;
    }

    // should either store this as a field, or, expose it as an iterator?
    // maybe just expose methods like "hasRequiredTech"?
    const std::unordered_set<std::reference_wrapper<const Tech>, Hash<Tech>, Equal<Tech>> &
    getRequiredTechs() const noexcept {
        assert(isInitialized());
        return requiredTechs;
    }

    const std::unordered_set<std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>> &
    getUnitsEnabled() const noexcept {
        assert(isInitialized());
        return unitsEnabled;
    }

    const std::unordered_set<std::reference_wrapper<const Building>, Hash<Building>, Equal<Building>> &
    getBuildingsEnabled() const noexcept {
        assert(isInitialized());
        return buildingsEnabled;
    }

    const std::unordered_map<std::reference_wrapper<const Unit>, std::reference_wrapper<const Unit>, Hash<Unit>,
                             Equal<Unit>> &
    getUnitsUpgraded() const noexcept {
        assert(isInitialized());
        return unitsUpgraded;
    }

    const std::unordered_map<std::reference_wrapper<const Building>, std::reference_wrapper<const Building>,
                             Hash<Building>, Equal<Building>> &
    getBuildingsUpgraded() const noexcept {
        assert(isInitialized());
        return buildingsUpgraded;
    }

    const std::optional<AgeC> ageSet;

  private:
    struct EffectInfo {
        std::optional<AgeC> ageSet{};
        std::unordered_set<int> unitIDsEnabled;
        std::unordered_set<int> buildingIDsEnabled;
        std::unordered_map<int, int> unitIDsUpgraded;
        std::unordered_map<int, int> buildingIDsUpgraded;
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
          ageSet{effectInfo.ageSet}, unitIDsEnabled{effectInfo.unitIDsEnabled}, unitIDsUpgraded{
                                                                                    effectInfo.unitIDsUpgraded} {}

    const std::unordered_set<int> unitIDsEnabled;
    const std::unordered_map<int, int> unitIDsUpgraded;
    const Building *researchLocation{};
    std::unordered_set<std::reference_wrapper<const Tech>, Hash<Tech>, Equal<Tech>> requiredTechs{};
    std::unordered_set<std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>> unitsEnabled{};
    std::unordered_set<std::reference_wrapper<const Building>, Hash<Building>, Equal<Building>> buildingsEnabled{};
    std::unordered_map<std::reference_wrapper<const Unit>, std::reference_wrapper<const Unit>, Hash<Unit>, Equal<Unit>>
        unitsUpgraded{};
    std::unordered_map<std::reference_wrapper<const Building>, std::reference_wrapper<const Building>, Hash<Building>,
                       Equal<Building>>
        buildingsUpgraded{};
};

// todo maybe we can build this list dynamically and have no need to hardcode?
// given the four age techs, either assume they're in numerical order, or put them in reverse order of requirements
// assert that there aren't duplicates or disconnected ages or whatever
enum class Age { dark = 0, feudal = 1, castle = 2, imperial = 3 };
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
    const std::unordered_set<int> creatablesToInclude{
        ID_VILLAGER_BASE_F,
    };

    // todo probably plenty of these can be excluded dynamically based on
    // excluding everything without a train location and/or not enabled
    // while taking into account tech enablings and upgrades
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
        // init phase one
        const CreatableContext creatableContext{
            nameMap, creatablesToInclude, creatablesToExclude, nameOverride, typeOverride};
        std::unordered_map<int, std::shared_ptr<Unit>> mutableUnits;
        std::unordered_map<int, std::shared_ptr<Building>> mutableBuildings;
        std::unordered_map<int, std::reference_wrapper<const Unit>> unitRefs;
        std::unordered_map<int, std::reference_wrapper<const Building>> buildingRefs;
        for (const auto &data : df->Civs[0].Units) {
            if (Creatable::isBuilding(data, creatableContext)) {
                const auto building{std::make_shared<Building>(data, creatableContext)};
                if (!building->isRelevant())
                    continue;
                mutableBuildings.try_emplace(building->getID(), building);
                buildingRefs.try_emplace(building->getID(), *building);
            } else {
                const auto unit{std::make_shared<Unit>(data, creatableContext)};
                if (!unit->isRelevant())
                    continue;
                mutableUnits.try_emplace(unit->getID(), unit);
                unitRefs.try_emplace(unit->getID(), *unit);
            }
        }
        std::vector<std::shared_ptr<Tech>> mutableTechs;
        std::vector<std::reference_wrapper<const Tech>> techRefs;
        for (std::size_t id{0}; id < df->Techs.size(); ++id) {
            const auto &techData{df->Techs[id]};
            const genie::Effect *effect{};
            if (techData.EffectID != -1)
                effect = &df->Effects[techData.EffectID];
            const auto tech{std::make_shared<Tech>(id, techData, effect, nameMap)};
            mutableTechs.emplace_back(tech);
            techRefs.emplace_back(*tech);
        }
        // init phase two
        for (const auto &tech : mutableTechs)
            tech->init(unitRefs, buildingRefs, techRefs);
        for (const auto &entry : mutableUnits)
            entry.second->init(unitRefs, buildingRefs, techRefs);
        for (const auto &entry : mutableBuildings)
            entry.second->init(unitRefs, buildingRefs, techRefs);
        // references no longer needed, mutation no longer needed
        return std::tuple{
            std::unordered_map<int, std::shared_ptr<const Unit>>{mutableUnits.begin(), mutableUnits.end()},
            std::unordered_map<int, std::shared_ptr<const Building>>{mutableBuildings.begin(), mutableBuildings.end()},
            std::vector<std::shared_ptr<const Tech>>{mutableTechs.begin(), mutableTechs.end()}};
    }()};

    const std::vector<Age> ages{Age::dark, Age::feudal, Age::castle, Age::imperial};

    // todo: when refactoring, these should be const after initialization
    std::unordered_set<Age> agesNotSeen(ages.begin(), ages.end());
    std::unordered_map<int16_t, Age> ageEffects;
    std::unordered_map<int16_t, std::unordered_set<int16_t>> enableEffects;
    // std::unordered_map<int16_t, std::unordered_map<int16_t, int16_t>> upgradeEffects;
    std::unordered_map<Age, std::unordered_set<int16_t>> unitsByAge;
    // consider looking for ALL upgrade effects & tracking those units
    //  might be needed e.g. to see house -> nomads house. is needed for cuman feudal SW -> SW.
    std::unordered_map<Age, std::unordered_map<int16_t, std::unordered_set<int16_t>>> ageUpgrades;
    // does not include tech-upgraded units
    // std::unordered_map<int16_t, std::unordered_map<Age, std::unordered_set<int16_t>>> unitUpgradesByAge;
    for (std::size_t effectID{0}; effectID < df->Effects.size(); ++effectID) {
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
                            // unitUpgradesByAge[c2.A][age].emplace(c2.B);
                        }
                }
            } else if (c.Type == typeEnableDisableUnit && c.B == modeEnable)
                enableEffects[effectID].emplace(c.A);  // the enabled unit's ID
            // else if (c.Type == typeUpgradeUnit && c.C == modeAll)
            //     upgradeEffects[effectID].emplace(c.A, c.B);  // the two units' IDs
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
    for (std::size_t techID{0}; techID < df->Techs.size(); ++techID) {
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
            if (const auto it2{ageUpgrades.find(enabledBy)}; it2 != ageUpgrades.end())
                // check each enabled unit
                for (const auto unitID : it->second)
                    if (const auto it3{it2->second.find(unitID)}; it3 != it2->second.end())
                        // todo: this line is partially redundant with adding units directly enabled by age tech
                        // in the earlier effect processing loop.
                        unitsByAge[enabledBy].insert(it3->second.begin(), it3->second.end());
                    else
                        unitsByAge[enabledBy].emplace(unitID);
            else
                unitsByAge[enabledBy].insert(it->second.begin(), it->second.end());
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
                        it != nextAgeRequirements.end())
                        // mark buildings involved in age-up requirements
                        val += "(*) "s;
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
