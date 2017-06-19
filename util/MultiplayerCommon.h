#ifndef _MultiplayerCommon_h_
#define _MultiplayerCommon_h_

#include "../universe/EnumsFwd.h"
#include "../network/Networking.h"
#include "OptionsDB.h"
#include "Export.h"
#include "Serialize.h"

#include <GG/Clr.h>

#include <list>
#include <set>
#include <vector>
#include <map>
#include <boost/serialization/access.hpp>


FO_COMMON_API extern const std::string MP_SAVE_FILE_EXTENSION;
FO_COMMON_API extern const std::string SP_SAVE_FILE_EXTENSION;

FO_COMMON_API extern const int ALL_EMPIRES;

class GameRules;

/////////////////////////////////////////////
// Free Functions
/////////////////////////////////////////////
typedef void (*GameRulesFn)(GameRules&); ///< the function signature for functions that add Rules to the GameRules (void (GameRules&))

/** adds \a function to a vector of pointers to functions that add Rules to
  * the GameRules.  This function returns a boolean so that it can be used to
  * declare a dummy static variable that causes \a function to be registered as
  * a side effect (e.g. at file scope:
  * "bool unused_bool = RegisterGameRules(&foo)"). */
FO_COMMON_API bool RegisterGameRules(GameRulesFn function);

/** returns the single instance of the GameRules class */
FO_COMMON_API GameRules& GetGameRules();

/** Database of values that control how the game mechanics function. */
class FO_COMMON_API GameRules {
public:
    typedef OptionsDB::Option Rule;

    /** \name Structors */ //@{
    GameRules();
    //@}

    /** \name Accessors */ //@{
    bool    RuleExists(const std::string& name) const
    { return m_game_rules.find(name) != m_game_rules.end(); }

    std::vector<std::pair<std::string, std::string>> GetRulesAsStrings() const;

    template <typename T>
    T       Get(const std::string& name) const
    {
        auto it = m_game_rules.find(name);
        if (it == m_game_rules.end())
            throw std::runtime_error("GameRules::Get<>() : Attempted to get nonexistent rule \"" + name + "\".");
        return boost::any_cast<T>(it->second.value);
    }
    //@}

    /** \name Mutators */ //@{
    /** adds a rule, optionally with a custom validator */
    template <class T>
    void    Add(const std::string& name, const std::string& description, T default_value,
                bool engine_interal, const ValidatorBase& validator = Validator<T>())
    {
        auto it = m_game_rules.find(name);
        if (it != m_game_rules.end())
            throw std::runtime_error("GameRules::Add<>() : Rule " + name + " was added twice.");
        m_game_rules[name] = Rule(static_cast<char>(0), name, default_value, default_value,
                                  description, validator.Clone(), engine_interal, false, true);
        DebugLogger() << "Added game rule named " << name << " with default value " << default_value;
    }

    template <typename T>
    void    Set(const std::string& name, const T& value)
    {
        auto it = m_game_rules.find(name);
        if (it == m_game_rules.end())
            throw std::runtime_error("GameRules::Set<>() : Attempted to set nonexistent rule \"" + name + "\".");
        it->second.SetFromValue(value);
    }

    void    SetFromStrings(const std::vector<std::pair<std::string, std::string>>& names_values);

    /** Removes game rules that were added without being specified as
        engine internal. */
    void    ClearExternalRules();

    /** Resets all rules to default values. */
    void    ResetToDefaults();
    //@}

private:
    std::map<std::string, Rule> m_game_rules;

    //friend class boost::serialization::access;
    //template <class Archive>
    //void serialize(Archive& ar, const unsigned int version);
};
//bool FO_COMMON_API operator==(const GameRules& lhs, const GameRules& rhs);
//bool FO_COMMON_API operator!=(const GameRules& lhs, const GameRules& rhs);

//extern template FO_COMMON_API void GameRules::serialize<freeorion_bin_oarchive>(freeorion_bin_oarchive&, const unsigned int);
//extern template FO_COMMON_API void GameRules::serialize<freeorion_bin_iarchive>(freeorion_bin_iarchive&, const unsigned int);
//extern template FO_COMMON_API void GameRules::serialize<freeorion_xml_oarchive>(freeorion_xml_oarchive&, const unsigned int);
//extern template FO_COMMON_API void GameRules::serialize<freeorion_xml_iarchive>(freeorion_xml_iarchive&, const unsigned int);

/** The data that represent the galaxy setup for a new game. */
struct FO_COMMON_API GalaxySetupData {
    /** \name Structors */ //@{
    GalaxySetupData();
    //@}

    /** \name Accessors */ //@{
    const std::string&  GetSeed() const;
    int                 GetSize() const;
    Shape               GetShape() const;
    GalaxySetupOption   GetAge() const;
    GalaxySetupOption   GetStarlaneFreq() const;
    GalaxySetupOption   GetPlanetDensity() const;
    GalaxySetupOption   GetSpecialsFreq() const;
    GalaxySetupOption   GetMonsterFreq() const;
    GalaxySetupOption   GetNativeFreq() const;
    Aggression          GetAggression() const;
    const std::vector<std::pair<std::string, std::string>>&
                        GetGameRules() const;
    //@}

    std::string         m_seed;
    int                 m_size;
    Shape               m_shape;
    GalaxySetupOption   m_age;
    GalaxySetupOption   m_starlane_freq;
    GalaxySetupOption   m_planet_density;
    GalaxySetupOption   m_specials_freq;
    GalaxySetupOption   m_monster_freq;
    GalaxySetupOption   m_native_freq;
    Aggression          m_ai_aggr;
    std::vector<std::pair<std::string, std::string>>
                        m_game_rules;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

BOOST_CLASS_VERSION(GalaxySetupData, 1);

extern template FO_COMMON_API void GalaxySetupData::serialize<freeorion_bin_oarchive>(freeorion_bin_oarchive&, const unsigned int);
extern template FO_COMMON_API void GalaxySetupData::serialize<freeorion_bin_iarchive>(freeorion_bin_iarchive&, const unsigned int);
extern template FO_COMMON_API void GalaxySetupData::serialize<freeorion_xml_oarchive>(freeorion_xml_oarchive&, const unsigned int);
extern template FO_COMMON_API void GalaxySetupData::serialize<freeorion_xml_iarchive>(freeorion_xml_iarchive&, const unsigned int);

/** Contains the UI data that must be saved in save game files in order to
  * restore games to the users' last views. */
struct FO_COMMON_API SaveGameUIData {
    int     map_top;
    int     map_left;
    double  map_zoom_steps_in;
    std::set<int> fleets_exploring;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

extern template FO_COMMON_API void SaveGameUIData::serialize<freeorion_bin_oarchive>(freeorion_bin_oarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameUIData::serialize<freeorion_bin_iarchive>(freeorion_bin_iarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameUIData::serialize<freeorion_xml_oarchive>(freeorion_xml_oarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameUIData::serialize<freeorion_xml_iarchive>(freeorion_xml_iarchive&, const unsigned int);

/** The data for one empire necessary for game-setup during multiplayer loading. */
struct FO_COMMON_API SaveGameEmpireData {
    /** \name Structors */ //@{
    SaveGameEmpireData() :
        m_empire_id(ALL_EMPIRES),
        m_empire_name(),
        m_player_name(),
        m_color()
    {}
    SaveGameEmpireData(int empire_id, const std::string& empire_name,
                       const std::string& player_name, const GG::Clr& colour) :
        m_empire_id(empire_id),
        m_empire_name(empire_name),
        m_player_name(player_name),
        m_color(colour)
    {}
    //@}

    int         m_empire_id;
    std::string m_empire_name;
    std::string m_player_name;
    GG::Clr     m_color;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

extern template FO_COMMON_API void SaveGameEmpireData::serialize<freeorion_bin_oarchive>(freeorion_bin_oarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameEmpireData::serialize<freeorion_bin_iarchive>(freeorion_bin_iarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameEmpireData::serialize<freeorion_xml_oarchive>(freeorion_xml_oarchive&, const unsigned int);
extern template FO_COMMON_API void SaveGameEmpireData::serialize<freeorion_xml_iarchive>(freeorion_xml_iarchive&, const unsigned int);

/** The data structure used to represent a single player's setup options for a
  * multiplayer game (in the multiplayer lobby screen). */
struct PlayerSetupData {
    /** \name Structors */ //@{
    PlayerSetupData() :
        m_player_name(),
        m_player_id(Networking::INVALID_PLAYER_ID),
        m_empire_name(),
        m_empire_color(GG::Clr(0, 0, 0, 0)),
        m_starting_species_name(),
        m_save_game_empire_id(ALL_EMPIRES),
        m_client_type(Networking::INVALID_CLIENT_TYPE),
        m_player_ready(false)
    {}
    //@}

    std::string             m_player_name;          ///< the player's name
    int                     m_player_id;            ///< player id

    std::string             m_empire_name;          ///< the name of the player's empire when starting a new game
    GG::Clr                 m_empire_color;         ///< the color used to represent this player's empire when starting a new game
    std::string             m_starting_species_name;///< name of the species with which the player starts when starting a new game

    int                     m_save_game_empire_id;  ///< when loading a game, the ID of the empire that this player will control

    Networking::ClientType  m_client_type;          ///< is this player an AI, human player or...?
    bool                    m_player_ready;         ///< if player ready to play.

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};
bool FO_COMMON_API operator==(const PlayerSetupData& lhs, const PlayerSetupData& rhs);
bool operator!=(const PlayerSetupData& lhs, const PlayerSetupData& rhs);

/** The data needed to establish a new single player game.  If \a m_new_game
  * is true, a new game is to be started, using the remaining members besides
  * \a m_filename.  Otherwise, the saved game \a m_filename will be loaded
  * instead. */
struct SinglePlayerSetupData : public GalaxySetupData {
    /** \name Structors */ //@{
    SinglePlayerSetupData():
        m_new_game(true),
        m_filename(),
        m_players()
    {}
    //@}

    bool                                m_new_game;
    std::string                         m_filename;
    std::vector<PlayerSetupData>        m_players;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** The data structure that represents the state of the multiplayer lobby. */
struct FO_COMMON_API MultiplayerLobbyData : public GalaxySetupData {
    /** \name Structors */ //@{
    MultiplayerLobbyData() :
        m_new_game(true),
        m_players(),
        m_save_game(),
        m_save_game_empire_data()
    {}
    //@}

    std::string Dump() const;

    bool                                        m_new_game;
    // TODO: Change from a list<(player_id, PlayerSetupData)> where
    // PlayerSetupData contain player_id to a vector of PlayerSetupData
    std::list<std::pair<int, PlayerSetupData>>  m_players;              // <player_id, PlayerSetupData>

    std::string                                 m_save_game;            //< File name of a save file
    std::map<int, SaveGameEmpireData>           m_save_game_empire_data;// indexed by empire_id

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Information about one player that other players are informed of.  Assembled by server and sent to players. */
struct PlayerInfo {
    PlayerInfo() :
        name(""),
        empire_id(ALL_EMPIRES),
        client_type(Networking::INVALID_CLIENT_TYPE),
        host(false)
    {}
    PlayerInfo(const std::string& player_name_, int empire_id_,
               Networking::ClientType client_type_, bool host_) :
        name(player_name_),
        empire_id(empire_id_),
        client_type(client_type_),
        host(host_)
    {}

    std::string             name;           ///< name of this player (not the same as the empire name)
    int                     empire_id;      ///< id of the player's empire
    Networking::ClientType  client_type;    ///< is this a human player, AI player, or observer?
    bool                    host;           ///< true iff this is the host player

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

// Note: *::serialize() implemented in SerializeMultiplayerCommon.cpp.

#endif // _MultiplayerCommon_h_
