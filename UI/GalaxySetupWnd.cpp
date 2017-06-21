#include "GalaxySetupWnd.h"

#include "CUIControls.h"
#include "CUISpin.h"
#include "Sound.h"
#include "../universe/Universe.h"
#include "../universe/Enums.h"
#include "../client/human/HumanClientApp.h"
#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../util/Directories.h"
#include "../util/AppInterface.h"

#include <boost/filesystem/fstream.hpp>

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/Layout.h>
#include <GG/TabWnd.h>


namespace {
    const GG::X CONTROL_MARGIN(5);
    const GG::Y CONTROL_VMARGIN(5);
    const GG::Y CONTROL_HEIGHT(30);
    const GG::Y PANEL_CONTROL_SPACING(33);
    const GG::X INDENTATION(20);
    const GG::X SPIN_WIDTH(92);
    const GG::Y GAL_SETUP_PANEL_HT(PANEL_CONTROL_SPACING * 10);
    const GG::X GalSetupWndWidth()
    { return GG::X(345 + FontBasedUpscale(300)); }
    const GG::Y GalSetupWndHeight()
    { return GG::Y(FontBasedUpscale(29) + (PANEL_CONTROL_SPACING * 6) + GAL_SETUP_PANEL_HT); }
    const GG::Pt PREVIEW_SZ(GG::X(248), GG::Y(186));
    const bool ALLOW_NO_STARLANES = false;

    class RowContentsWnd : public GG::Control {
    public:
        RowContentsWnd(GG::X w, GG::Y h, GG::Wnd* contents, int indentation_level) :
            Control(GG::X0, GG::Y0, w, h, GG::INTERACTIVE),
            m_contents(contents)
        {
            if (!m_contents)
                return;
            AttachChild(m_contents);
            m_contents->MoveTo(GG::Pt(GG::X(indentation_level * INDENTATION), GG::Y0));
            DoLayout();
        }

        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            const GG::Pt old_size = Size();
            GG::Control::SizeMove(ul, lr);
            if (old_size != Size())
                DoLayout();
        }

        void DoLayout() {
            if (m_contents) {
                //std::cout << "RowContentsWnd::DoLayout()" << std::endl;
                m_contents->SizeMove(GG::Pt(), Size());
            }
        }

        void Render() override
        { /*GG::FlatRectangle(UpperLeft(), LowerRight(), GG::CLR_DARK_RED, GG::CLR_PINK, 1);*/ }
    private:
        Wnd* m_contents;
    };

    class RuleListRow : public GG::ListBox::Row {
    public:
        RuleListRow(GG::X w, GG::Y h, RowContentsWnd* contents) :
            GG::ListBox::Row(w, h, ""),
            m_contents(contents)
        {
            SetChildClippingMode(ClipToClient);
            if (m_contents)
                push_back(m_contents);
        }

        RuleListRow(GG::X w, GG::Y h, Wnd* contents, int indentation = 0) :
            GG::ListBox::Row(w, h, ""),
            m_contents(nullptr)
        {
            SetChildClippingMode(ClipToClient);
            if (contents) {
                m_contents = new RowContentsWnd(w, h, contents, indentation);
                push_back(m_contents);
            }
        }

        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            //std::cout << "RuleListRow::SizeMove(" << ul << ", " << lr << ")" << std::endl;
            const GG::Pt old_size = Size();
            GG::ListBox::Row::SizeMove(ul, lr);
            if (!empty() && old_size != Size() && m_contents)
                m_contents->Resize(Size());
        }

        void Render() override
        { /*GG::FlatRectangle(UpperLeft(), LowerRight(), GG::CLR_DARK_BLUE, GG::CLR_YELLOW, 1);*/ }
    private:
        RowContentsWnd* m_contents;
    };

    class GameRulesList : public CUIListBox {
    public:
        GameRulesList() :
            CUIListBox()
        {
            InitRowSizes();

            SetColor(GG::CLR_ZERO);
            SetStyle(GG::LIST_NOSORT | GG::LIST_NOSEL);
            SetVScrollWheelIncrement(ClientUI::Pts() * 10);
        }

        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) override {
            const GG::Pt old_size = Size();
            CUIListBox::SizeMove(ul, lr);
            if (old_size != Size()) {
                const GG::X row_width = ListRowWidth();
                for (GG::ListBox::Row* row : *this)
                    row->Resize(GG::Pt(row_width, row->Height()));
            }
        }

    private:
        GG::X ListRowWidth() const
        { return Width() - RightMargin() - 5; }

        void InitRowSizes() {
            // preinitialize listbox/row column widths, because what
            // ListBox::Insert does on default is not suitable for this case
            SetNumCols(1);
            SetColWidth(0, GG::X0);
            LockColWidths();
        }
    };


    // persistant between-executions galaxy setup settings, mainly so I don't have to redo these settings to what I want every time I run FO to test something
    void AddOptions(OptionsDB& db) {
        db.Add("GameSetup.seed",                UserStringNop("OPTIONS_DB_GAMESETUP_SEED"),                    std::string("0"),   Validator<std::string>());
        db.Add("GameSetup.stars",               UserStringNop("OPTIONS_DB_GAMESETUP_STARS"),                   150,                RangedValidator<int>(10, 5000));
        db.Add("GameSetup.galaxy-shape",        UserStringNop("OPTIONS_DB_GAMESETUP_GALAXY_SHAPE"),            DISC,               RangedValidator<Shape>(SPIRAL_2, RANDOM));
        db.Add("GameSetup.galaxy-age",          UserStringNop("OPTIONS_DB_GAMESETUP_GALAXY_AGE"),              GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(GALAXY_SETUP_LOW, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.planet-density",      UserStringNop("OPTIONS_DB_GAMESETUP_PLANET_DENSITY"),          GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(GALAXY_SETUP_LOW, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.starlane-frequency",  UserStringNop("OPTIONS_DB_GAMESETUP_STARLANE_FREQUENCY"),      GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(ALLOW_NO_STARLANES ? GALAXY_SETUP_NONE : GALAXY_SETUP_LOW, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.specials-frequency",  UserStringNop("OPTIONS_DB_GAMESETUP_SPECIALS_FREQUENCY"),      GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(GALAXY_SETUP_NONE, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.monster-frequency",   UserStringNop("OPTIONS_DB_GAMESETUP_MONSTER_FREQUENCY"),       GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(GALAXY_SETUP_NONE, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.native-frequency",    UserStringNop("OPTIONS_DB_GAMESETUP_NATIVE_FREQUENCY"),        GALAXY_SETUP_MEDIUM,RangedValidator<GalaxySetupOption>(GALAXY_SETUP_NONE, GALAXY_SETUP_RANDOM));
        db.Add("GameSetup.empire-name",         UserStringNop("OPTIONS_DB_GAMESETUP_EMPIRE_NAME"),             std::string(""),    Validator<std::string>());
        db.Add("GameSetup.player-name",         UserStringNop("OPTIONS_DB_GAMESETUP_PLAYER_NAME"),             std::string(""),    Validator<std::string>());
        db.Add("GameSetup.empire-color",        UserStringNop("OPTIONS_DB_GAMESETUP_EMPIRE_COLOR"),            9,                  RangedValidator<int>(0, 100));
        db.Add("GameSetup.starting-species",    UserStringNop("OPTIONS_DB_GAMESETUP_STARTING_SPECIES_NAME"),   std::string("SP_HUMAN"),    Validator<std::string>());
        db.Add("GameSetup.ai-players",          UserStringNop("OPTIONS_DB_GAMESETUP_NUM_AI_PLAYERS"),          6,                  RangedValidator<int>(0, IApp::MAX_AI_PLAYERS()));
        db.Add("GameSetup.ai-aggression",       UserStringNop("OPTIONS_DB_GAMESETUP_AI_MAX_AGGRESSION"),       MANIACAL,           RangedValidator<Aggression>(BEGINNER, MANIACAL));
    }
    bool temp_bool = RegisterOptions(&AddOptions);
}

////////////////////////////////////////////////
// GameRulesPanel
////////////////////////////////////////////////
const GG::X GameRulesPanel::DefaultWidth() {
    return GG::X(FontBasedUpscale(305));
}

GameRulesPanel::GameRulesPanel(GG::X w, GG::Y h) :
    GG::Control(GG::X0, GG::Y0, w, h, GG::NO_WND_FLAGS)
{
    const int tooltip_delay = GetOptionsDB().Get<int>("UI.tooltip-delay");
    GG::ListBox* current_page = nullptr;

    m_tabs = new GG::TabWnd(GG::X0, GG::Y0, SPIN_WIDTH, GG::Y1,
                            ClientUI::GetFont(), ClientUI::WndColor(), ClientUI::TextColor());
    AttachChild(m_tabs);

    current_page = CreatePage("GENERAL");
    BoolRuleWidget(current_page, 0, "RULE_CHEAP_AND_FAST_BUILDING_PRODUCTION");
    BoolRuleWidget(current_page, 0, "RULE_CHEAP_AND_FAST_SHIP_PRODUCTION");
    BoolRuleWidget(current_page, 0, "RULE_CHEAP_AND_FAST_TECH_RESEARCH");
    IntRuleWidget( current_page, 0, "RULE_NUM_COMBAT_ROUNDS");
    BoolRuleWidget(current_page, 0, "RULE_RESEED_PRNG_SERVER");

    CreatePage("TEST");

    DoLayout();
    m_tabs->SetCurrentWnd(0);
}

std::vector<std::pair<std::string, std::string>> GameRulesPanel::GetRulesAsStrings() const {
    std::vector<std::pair<std::string, std::string>> retval;
    for (const auto& entry : m_rules)
        retval.push_back(entry);
    return retval;
}

void GameRulesPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Control::SizeMove(ul, lr);
    DoLayout();
}

void GameRulesPanel::Disable(bool b) {
    for (GG::Wnd* child : Children())
        static_cast<GG::Control*>(child)->Disable(b);
}

void GameRulesPanel::DoLayout() {
    GG::Pt MARGINS(CONTROL_MARGIN, CONTROL_VMARGIN);
    m_tabs->SizeMove(MARGINS, ClientSize() - MARGINS);
}

void GameRulesPanel::Render()
{ /*GG::FlatRectangle(UpperLeft(), LowerRight(), GG::CLR_CYAN, GG::CLR_GREEN, 1);*/ }

void GameRulesPanel::SettingChanged() {
    Sound::TempUISoundDisabler sound_disabler;
    SettingsChangedSignal();
}

GG::ListBox* GameRulesPanel::CreatePage(const std::string& name) {
    GG::ListBox* page = new GameRulesList();
    m_tabs->AddWnd(page, name);
    m_tabs->SetCurrentWnd(m_tabs->NumWnds() - 1);
    return page;
}

void GameRulesPanel::CreateSectionHeader(GG::ListBox* page, int indentation_level,
                                     const std::string& name, const std::string& tooltip)
{
    assert(0 <= indentation_level);
    GG::Label* heading_text = new CUILabel(name, GG::FORMAT_LEFT | GG::FORMAT_NOWRAP);
    heading_text->SetFont(ClientUI::GetFont(ClientUI::Pts() * 4 / 3));

    GG::ListBox::Row* row = new RuleListRow(Width(), heading_text->MinUsableSize().y + CONTROL_VMARGIN + 6,
                                            heading_text, indentation_level);

    if (!tooltip.empty()) {
        row->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
        row->SetBrowseText(tooltip);
    }

    page->Insert(row);
}

GG::StateButton* GameRulesPanel::BoolRuleWidget(GG::ListBox* page, int indentation_level,
                                                const std::string& rule_name)
{
    GG::StateButton* button = new CUIStateButton(UserString(rule_name), GG::FORMAT_LEFT,
                                                 std::make_shared<CUICheckBoxRepresenter>());
    GG::ListBox::Row* row = new RuleListRow(Width(), button->MinUsableSize().y + CONTROL_VMARGIN + 6,
                                            button, indentation_level);

    button->SetCheck(GetGameRules().Get<bool>(rule_name));
    button->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    button->SetBrowseText(UserString(GetGameRules().GetDescription(rule_name)));
    button->CheckedSignal.connect(boost::bind(&GameRulesPanel::BoolRuleChanged, this, button,
                                              rule_name));

    page->Insert(row);
    return button;
}

GG::Spin<int>* GameRulesPanel::IntRuleWidget(GG::ListBox* page, int indentation_level,
                                             const std::string& rule_name)
{
    GG::Label* text_control = new CUILabel(UserString(rule_name), GG::FORMAT_LEFT | GG::FORMAT_NOWRAP, GG::INTERACTIVE);

    std::shared_ptr<const ValidatorBase> validator = GetGameRules().GetValidator(rule_name);
    int value = GetGameRules().Get<int>(rule_name);

    GG::Spin<int>* spin = nullptr;
    if (std::shared_ptr<const RangedValidator<int>> ranged_validator = std::dynamic_pointer_cast<const RangedValidator<int>>(validator))
        spin = new CUISpin<int>(value, 1, ranged_validator->m_min, ranged_validator->m_max, true);

    else if (std::shared_ptr<const StepValidator<int>> step_validator = std::dynamic_pointer_cast<const StepValidator<int>>(validator))
        spin = new CUISpin<int>(value, step_validator->m_step_size, -1000000, 1000000, true);

    else if (std::shared_ptr<const RangedStepValidator<int>> ranged_step_validator = std::dynamic_pointer_cast<const RangedStepValidator<int>>(validator))
        spin = new CUISpin<int>(value, ranged_step_validator->m_step_size, ranged_step_validator->m_min, ranged_step_validator->m_max, true);

    else if (std::shared_ptr<const Validator<int>> int_validator = std::dynamic_pointer_cast<const Validator<int>>(validator))
        spin = new CUISpin<int>(value, 1, -1000000, 1000000, true);

    if (!spin) {
        ErrorLogger() << "Unable to create IntRuleWidget spin";
        return nullptr;
    }

    spin->Resize(GG::Pt(SPIN_WIDTH, spin->MinUsableSize().y));
    GG::Layout* layout = new GG::Layout(GG::X0, GG::Y0, Width(), spin->MinUsableSize().y, 1, 2, 0, 5);
    layout->Add(spin, 0, 0, GG::ALIGN_VCENTER | GG::ALIGN_LEFT);
    layout->Add(text_control, 0, 1, GG::ALIGN_VCENTER | GG::ALIGN_LEFT);
    layout->SetMinimumColumnWidth(0, SPIN_WIDTH);
    layout->SetColumnStretch(1, 1.0);
    layout->SetChildClippingMode(ClipToClient);

    GG::ListBox::Row* row = new RuleListRow(Width(), spin->MinUsableSize().y + CONTROL_VMARGIN + 6,
                                            spin, indentation_level);
    page->Insert(row);

    spin->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    spin->SetBrowseText(UserString(GetGameRules().GetDescription(rule_name)));

    spin->ValueChangedSignal.connect(boost::bind(&GameRulesPanel::IntRuleChanged,
                                                 this, spin, rule_name));
    return spin;
}

GG::Spin<double>* GameRulesPanel::DoubleRuleWidget(GG::ListBox* page, int indentation_level,
                                                   const std::string& rule_name)
{
    GG::Label* text_control = new CUILabel(UserString(rule_name), GG::FORMAT_LEFT | GG::FORMAT_NOWRAP, GG::INTERACTIVE);

    std::shared_ptr<const ValidatorBase> validator = GetGameRules().GetValidator(rule_name);
    double value = GetGameRules().Get<double>(rule_name);

    GG::Spin<double>* spin = nullptr;
    if (std::shared_ptr<const RangedValidator<double>> ranged_validator = std::dynamic_pointer_cast<const RangedValidator<double>>(validator))
        spin = new CUISpin<double>(value, 1, ranged_validator->m_min, ranged_validator->m_max, true);

    else if (std::shared_ptr<const StepValidator<double>> step_validator = std::dynamic_pointer_cast<const StepValidator<double>>(validator))
        spin = new CUISpin<double>(value, step_validator->m_step_size, -1000000, 1000000, true);

    else if (std::shared_ptr<const RangedStepValidator<double>> ranged_step_validator = std::dynamic_pointer_cast<const RangedStepValidator<double>>(validator))
        spin = new CUISpin<double>(value, ranged_step_validator->m_step_size, ranged_step_validator->m_min, ranged_step_validator->m_max, true);

    else if (std::shared_ptr<const Validator<double>> int_validator = std::dynamic_pointer_cast<const Validator<double>>(validator))
        spin = new CUISpin<double>(value, 1, -1000000, 1000000, true);

    if (!spin) {
        ErrorLogger() << "Unable to create DoubleRuleWidget spin";
        return nullptr;
    }

    spin->Resize(GG::Pt(SPIN_WIDTH, spin->MinUsableSize().y));
    GG::Layout* layout = new GG::Layout(GG::X0, GG::Y0, Width(), spin->MinUsableSize().y, 1, 2, 0, 5);
    layout->Add(spin, 0, 0, GG::ALIGN_VCENTER | GG::ALIGN_LEFT);
    layout->Add(text_control, 0, 1, GG::ALIGN_VCENTER | GG::ALIGN_LEFT);
    layout->SetMinimumColumnWidth(0, SPIN_WIDTH);
    layout->SetColumnStretch(1, 1.0);
    layout->SetChildClippingMode(ClipToClient);

    GG::ListBox::Row* row = new RuleListRow(Width(), spin->MinUsableSize().y + CONTROL_VMARGIN + 6,
                                            spin, indentation_level);
    page->Insert(row);

    spin->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    spin->SetBrowseText(UserString(GetGameRules().GetDescription(rule_name)));

    spin->ValueChangedSignal.connect(boost::bind(&GameRulesPanel::DoubleRuleChanged,
                                                 this, spin, rule_name));
    return spin;
}

void GameRulesPanel::BoolRuleChanged(const GG::StateButton* button,
                                     const std::string& rule_name)
{
    std::shared_ptr<const ValidatorBase> val = GetGameRules().GetValidator(rule_name);
    if (!val)
        return;
    m_rules[rule_name] = val->String(button->Checked());

    DebugLogger() << "Set Rules:";
    for (const auto& entry : m_rules)
        DebugLogger() << "  " << entry.first << " : " << entry.second;

    SettingChanged();
}

void GameRulesPanel::IntRuleChanged(const GG::Spin<int>* spin,
                                     const std::string& rule_name)
{
    std::shared_ptr<const ValidatorBase> val = GetGameRules().GetValidator(rule_name);
    if (!val)
        return;
    m_rules[rule_name] = val->String(spin->Value());

    DebugLogger() << "Set Rules:";
    for (const auto& entry : m_rules)
        DebugLogger() << "  " << entry.first << " : " << entry.second;

    SettingChanged();
}

void GameRulesPanel::DoubleRuleChanged(const GG::Spin<double>* spin,
                                       const std::string& rule_name)
{
    std::shared_ptr<const ValidatorBase> val = GetGameRules().GetValidator(rule_name);
    if (!val)
        return;
    m_rules[rule_name] = val->String(spin->Value());

    DebugLogger() << "Set Rules:";
    for (const auto& entry : m_rules)
        DebugLogger() << "  " << entry.first << " : " << entry.second;

    SettingChanged();
}


////////////////////////////////////////////////
// GalaxySetupPanel
////////////////////////////////////////////////
const GG::X GalaxySetupPanel::DefaultWidth() {
    return GG::X(FontBasedUpscale(305));
}

GalaxySetupPanel::GalaxySetupPanel(GG::X w, GG::Y h) :
    GG::Control(GG::X0, GG::Y0, w, h, GG::NO_WND_FLAGS)
{
    Sound::TempUISoundDisabler sound_disabler;

    // seed
    m_seed_label = new CUILabel(UserString("GSETUP_SEED"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_seed_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_seed_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.seed")));
    m_seed_edit = new CUIEdit(GetOptionsDB().Get<std::string>("GameSetup.seed"));

    boost::filesystem::path button_texture_dir = ClientUI::ArtDir() / "icons" / "buttons";

    // random seed button
    m_random = new CUIButton(
        GG::SubTexture(ClientUI::GetTexture(button_texture_dir / "randomize.png")),
        GG::SubTexture(ClientUI::GetTexture(button_texture_dir / "randomize_clicked.png")),
        GG::SubTexture(ClientUI::GetTexture(button_texture_dir / "randomize_mouseover.png")));

    m_random->SetBrowseText(UserString("GSETUP_RANDOM_SEED"));
    m_random->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));

    // number of stars
    m_stars_label = new CUILabel(UserString("GSETUP_STARS"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_stars_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_stars_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.stars")));
    m_stars_spin = new CUISpin<int>(100, 1, 10, 5000, true);

    // galaxy shape
    m_galaxy_shapes_label = new CUILabel(UserString("GSETUP_SHAPE"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_galaxy_shapes_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_galaxy_shapes_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.galaxy-shape")));
    m_galaxy_shapes_list = new CUIDropDownList(5);
    m_galaxy_shapes_list->SetStyle(GG::LIST_NOSORT);

    // galaxy age
    m_galaxy_ages_label = new CUILabel(UserString("GSETUP_AGE"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_galaxy_ages_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_galaxy_ages_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.galaxy-age")));
    m_galaxy_ages_list = new CUIDropDownList(5);
    m_galaxy_ages_list->SetStyle(GG::LIST_NOSORT);

    // starlane frequency
    m_starlane_freq_label = new CUILabel(UserString("GSETUP_STARLANE_FREQ"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_starlane_freq_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_starlane_freq_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.starlane-frequency")));
    m_starlane_freq_list = new CUIDropDownList(5);
    m_starlane_freq_list->SetStyle(GG::LIST_NOSORT);

    // planet density
    m_planet_density_label = new CUILabel(UserString("GSETUP_PLANET_DENSITY"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_planet_density_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_planet_density_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.planet-density")));
    m_planet_density_list = new CUIDropDownList(5);
    m_planet_density_list->SetStyle(GG::LIST_NOSORT);

    // specials frequency
    m_specials_freq_label = new CUILabel(UserString("GSETUP_SPECIALS_FREQ"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_specials_freq_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_specials_freq_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.specials-frequency")));
    m_specials_freq_list = new CUIDropDownList(5);
    m_specials_freq_list->SetStyle(GG::LIST_NOSORT);

    // monster frequency
    m_monster_freq_label = new CUILabel(UserString("GSETUP_MONSTER_FREQ"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_monster_freq_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_monster_freq_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.monster-frequency")));
    m_monster_freq_list = new CUIDropDownList(5);
    m_monster_freq_list->SetStyle(GG::LIST_NOSORT);

    // native frequency
    m_native_freq_label = new CUILabel(UserString("GSETUP_NATIVE_FREQ"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_native_freq_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_native_freq_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.native-frequency")));
    m_native_freq_list = new CUIDropDownList(5);
    m_native_freq_list->SetStyle(GG::LIST_NOSORT);

    // ai aggression
    m_ai_aggression_label = new CUILabel(UserString("GSETUP_AI_AGGR"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_ai_aggression_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_ai_aggression_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.ai-aggression")));
    m_ai_aggression_list = new CUIDropDownList(5);
    m_ai_aggression_list->SetStyle(GG::LIST_NOSORT);

    AttachChild(m_seed_label);
    AttachChild(m_seed_edit);
    AttachChild(m_random);
    AttachChild(m_stars_label);
    AttachChild(m_stars_spin);
    AttachChild(m_galaxy_shapes_label);
    AttachChild(m_galaxy_shapes_list);
    AttachChild(m_galaxy_ages_label);
    AttachChild(m_galaxy_ages_list);
    AttachChild(m_starlane_freq_label);
    AttachChild(m_starlane_freq_list);
    AttachChild(m_planet_density_label);
    AttachChild(m_planet_density_list);
    AttachChild(m_specials_freq_label);
    AttachChild(m_specials_freq_list);
    AttachChild(m_monster_freq_label);
    AttachChild(m_monster_freq_list);
    AttachChild(m_native_freq_label);
    AttachChild(m_native_freq_list);
    AttachChild(m_ai_aggression_label);
    AttachChild(m_ai_aggression_list);

    DoLayout();

    m_random->LeftClickedSignal.connect(
        boost::bind(&GalaxySetupPanel::RandomClicked, this));
    m_seed_edit->FocusUpdateSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_stars_spin->ValueChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_galaxy_shapes_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_galaxy_ages_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_starlane_freq_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_planet_density_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_specials_freq_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_monster_freq_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_native_freq_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_ai_aggression_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::SettingChanged, this));
    m_galaxy_shapes_list->SelChangedSignal.connect(
        boost::bind(&GalaxySetupPanel::ShapeChanged, this, _1));

    // create and load textures
    m_textures.clear();
    m_textures.resize(GALAXY_SHAPES);
    m_textures[SPIRAL_2] =    ClientUI::GetTexture(ClientUI::ArtDir() / "gp_spiral2.png");
    m_textures[SPIRAL_3] =    ClientUI::GetTexture(ClientUI::ArtDir() / "gp_spiral3.png");
    m_textures[SPIRAL_4] =    ClientUI::GetTexture(ClientUI::ArtDir() / "gp_spiral4.png");
    m_textures[CLUSTER] =     ClientUI::GetTexture(ClientUI::ArtDir() / "gp_cluster.png");
    m_textures[ELLIPTICAL] =  ClientUI::GetTexture(ClientUI::ArtDir() / "gp_elliptical.png");
    m_textures[DISC] =        ClientUI::GetTexture(ClientUI::ArtDir() / "gp_disc.png");
    m_textures[BOX] =         ClientUI::GetTexture(ClientUI::ArtDir() / "gp_box.png");
    m_textures[IRREGULAR] =   ClientUI::GetTexture(ClientUI::ArtDir() / "gp_irregular.png");
    m_textures[RING] =        ClientUI::GetTexture(ClientUI::ArtDir() / "gp_ring.png");
    m_textures[RANDOM] =      ClientUI::GetTexture(ClientUI::ArtDir() / "gp_random.png");

    // fill droplists
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_2ARM")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_3ARM")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_4ARM")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_CLUSTER")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_ELLIPTICAL")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_DISC")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_BOX")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_IRREGULAR")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RING")));
    m_galaxy_shapes_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_galaxy_ages_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_YOUNG")));
    m_galaxy_ages_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MATURE")));
    m_galaxy_ages_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_ANCIENT")));
    m_galaxy_ages_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    if (ALLOW_NO_STARLANES)
        m_starlane_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_NONE")));
    m_starlane_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_LOW")));
    m_starlane_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MEDIUM")));
    m_starlane_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_HIGH")));
    m_starlane_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_planet_density_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_LOW")));
    m_planet_density_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MEDIUM")));
    m_planet_density_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_HIGH")));
    m_planet_density_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_specials_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_NONE")));
    m_specials_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_LOW")));
    m_specials_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MEDIUM")));
    m_specials_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_HIGH")));
    m_specials_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_monster_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_NONE")));
    m_monster_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_LOW")));
    m_monster_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MEDIUM")));
    m_monster_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_HIGH")));
    m_monster_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_native_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_NONE")));
    m_native_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_LOW")));
    m_native_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MEDIUM")));
    m_native_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_HIGH")));
    m_native_freq_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_RANDOM")));

    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_BEGINNER")));
    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_TURTLE")));
    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_CAUTIOUS")));
    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_TYPICAL")));
    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_AGGRESSIVE")));
    m_ai_aggression_list->Insert(new CUISimpleDropDownListRow(UserString("GSETUP_MANIACAL")));

    // initial settings from stored results or defaults
    m_seed_edit->SetText(GetOptionsDB().Get<std::string>("GameSetup.seed"));
    m_stars_spin->SetValue(GetOptionsDB().Get<int>("GameSetup.stars"));
    m_galaxy_shapes_list->Select(GetOptionsDB().Get<Shape>("GameSetup.galaxy-shape"));
    ShapeChanged(m_galaxy_shapes_list->CurrentItem());
    m_galaxy_ages_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.galaxy-age") - 1);
    m_starlane_freq_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.starlane-frequency") - (ALLOW_NO_STARLANES ? 0 : 1));
    m_planet_density_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.planet-density") - 1);
    m_specials_freq_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.specials-frequency"));
    m_monster_freq_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.monster-frequency"));
    m_native_freq_list->Select(GetOptionsDB().Get<GalaxySetupOption>("GameSetup.native-frequency"));
    m_ai_aggression_list->Select(GetOptionsDB().Get<Aggression>("GameSetup.ai-aggression"));

    SettingsChangedSignal();
}

namespace {
    // set of characters from which to generate random seed that excludes some ambiguous letter/number pairs
    static char alphanum[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
}

void GalaxySetupPanel::RandomClicked() {
    std::string s;
    ClockSeed(); // to ensure we don't always get the same sequence of seeds
    for (int i = 0; i < 8; ++i)
        s += alphanum[ RandSmallInt(0, (sizeof(alphanum) - 2))];
    m_seed_edit->SetText(s);
    //std::cout << "GalaxySetupPanel::RandomClicked() new seed: " << s << std::endl;
    SettingChanged();
}

const std::string& GalaxySetupPanel::GetSeed() const
{ return m_seed_edit->Text(); }

int GalaxySetupPanel::Systems() const
{ return m_stars_spin->Value(); }

Shape GalaxySetupPanel::GetShape() const
{ return Shape(m_galaxy_shapes_list->CurrentItemIndex()); }

GalaxySetupOption GalaxySetupPanel::GetAge() const
{ return GalaxySetupOption(m_galaxy_ages_list->CurrentItemIndex() + 1); }

GalaxySetupOption GalaxySetupPanel::GetStarlaneFrequency() const
{ return GalaxySetupOption(m_starlane_freq_list->CurrentItemIndex() + (ALLOW_NO_STARLANES ? 0 : 1)); }

GalaxySetupOption GalaxySetupPanel::GetPlanetDensity() const
{ return GalaxySetupOption(m_planet_density_list->CurrentItemIndex() + 1); }

GalaxySetupOption GalaxySetupPanel::GetSpecialsFrequency() const
{ return GalaxySetupOption(m_specials_freq_list->CurrentItemIndex()); }

GalaxySetupOption GalaxySetupPanel::GetMonsterFrequency() const
{ return GalaxySetupOption(m_monster_freq_list->CurrentItemIndex()); }

GalaxySetupOption GalaxySetupPanel::GetNativeFrequency() const
{ return GalaxySetupOption(m_native_freq_list->CurrentItemIndex()); }

Aggression GalaxySetupPanel::GetAIAggression() const
{ return Aggression(m_ai_aggression_list->CurrentItemIndex()); }

std::shared_ptr<GG::Texture> GalaxySetupPanel::PreviewImage() const
{ return m_textures[GetShape()]; }

void GalaxySetupPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Control::SizeMove(ul, lr);
    DoLayout();
}

void GalaxySetupPanel::DoLayout() {
    const GG::X LABELS_WIDTH = (Width() - CONTROL_MARGIN) / 2;
    const GG::X DROPLIST_WIDTH = LABELS_WIDTH;
    const GG::Y DROPLIST_HEIGHT(ClientUI::Pts() + 12);

    GG::Pt row_advance(GG::X0, PANEL_CONTROL_SPACING);

    GG::Pt label_ul(CONTROL_MARGIN, GG::Y0);
    GG::Pt label_lr = label_ul + GG::Pt(LABELS_WIDTH, CONTROL_HEIGHT);

    GG::Pt control_ul(GG::Pt(LABELS_WIDTH + 2 * CONTROL_MARGIN, GG::Y0) + GG::Pt(GG::X0, (PANEL_CONTROL_SPACING - m_seed_edit->MinUsableSize().y) / 2));
    GG::Pt control_lr = control_ul + GG::Pt(LABELS_WIDTH -30, m_seed_edit->MinUsableSize().y);
    GG::Pt button_ul(2 * LABELS_WIDTH + 3 * CONTROL_MARGIN - 30, CONTROL_VMARGIN);
    GG::Pt button_lr = button_ul + GG::Pt(GG::X(20), GG::Y(20));

    m_seed_label->SizeMove(label_ul, label_lr);
    m_seed_edit->SizeMove(control_ul, control_lr);
    m_random->SizeMove(button_ul, button_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr = control_ul + GG::Pt(GG::X(75),  ClientUI::GetFont()->Height() + 2 * 5);

    m_stars_label->SizeMove(label_ul, label_lr);
    m_stars_spin->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr = control_ul + GG::Pt(DROPLIST_WIDTH, DROPLIST_HEIGHT);

    m_galaxy_shapes_label->SizeMove(label_ul, label_lr);
    m_galaxy_shapes_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_galaxy_ages_label->SizeMove(label_ul, label_lr);
    m_galaxy_ages_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_starlane_freq_label->SizeMove(label_ul, label_lr);
    m_starlane_freq_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_planet_density_label->SizeMove(label_ul, label_lr);
    m_planet_density_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_specials_freq_label->SizeMove(label_ul, label_lr);
    m_specials_freq_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_monster_freq_label->SizeMove(label_ul, label_lr);
    m_monster_freq_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_native_freq_label->SizeMove(label_ul, label_lr);
    m_native_freq_list->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_ai_aggression_label->SizeMove(label_ul, label_lr);
    m_ai_aggression_list->SizeMove(control_ul, control_lr);
}

void GalaxySetupPanel::Disable(bool b/* = true*/) {
    for (GG::Wnd* child : Children())
        static_cast<GG::Control*>(child)->Disable(b);
}

void GalaxySetupPanel::SetFromSetupData(const GalaxySetupData& setup_data) {
    m_seed_edit->SetText(setup_data.m_seed);
    m_stars_spin->SetValue(setup_data.m_size);
    m_galaxy_shapes_list->Select(setup_data.m_shape);
    ShapeChanged(m_galaxy_shapes_list->CurrentItem());
    m_galaxy_ages_list->Select(setup_data.m_age - 1);
    m_starlane_freq_list->Select(setup_data.m_starlane_freq - (ALLOW_NO_STARLANES ? 0 : 1));
    m_planet_density_list->Select(setup_data.m_planet_density - 1);
    m_specials_freq_list->Select(setup_data.m_specials_freq);
    m_monster_freq_list->Select(setup_data.m_monster_freq);
    m_native_freq_list->Select(setup_data.m_native_freq);
    m_ai_aggression_list->Select(setup_data.m_ai_aggr);
}

void GalaxySetupPanel::GetSetupData(GalaxySetupData& setup_data) const {
    setup_data.m_seed =             GetSeed();
    setup_data.m_size =             Systems();
    setup_data.m_shape =            GetShape();
    setup_data.m_age =              GetAge();
    setup_data.m_starlane_freq =    GetStarlaneFrequency();
    setup_data.m_planet_density =   GetPlanetDensity();
    setup_data.m_specials_freq =    GetSpecialsFrequency();
    setup_data.m_monster_freq =     GetMonsterFrequency();
    setup_data.m_native_freq =      GetNativeFrequency();
    setup_data.m_ai_aggr =          GetAIAggression();
}

void GalaxySetupPanel::SettingChanged() {
    Sound::TempUISoundDisabler sound_disabler;
    SettingsChangedSignal();
}

void GalaxySetupPanel::ShapeChanged(GG::DropDownList::iterator it)
{ ImageChangedSignal(m_textures[m_galaxy_shapes_list->IteratorToIndex(it)]); }


////////////////////////////////////////////////
// GalaxySetupWnd
////////////////////////////////////////////////
GalaxySetupWnd::GalaxySetupWnd() :
    CUIWnd(UserString("GSETUP_WINDOW_TITLE"), GG::INTERACTIVE | GG::MODAL)
{
    Sound::TempUISoundDisabler sound_disabler;

    m_galaxy_setup_panel = new GalaxySetupPanel();
    m_game_rules_panel = new GameRulesPanel();

    const GG::X LABELS_WIDTH = (GalaxySetupPanel::DefaultWidth() - 5) / 2;

    // player name
    m_player_name_label = new CUILabel(UserString("GSETUP_PLAYER_NAME"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_player_name_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_player_name_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.player-name")));
    m_player_name_edit = new CUIEdit(GetOptionsDB().Get<std::string>("GameSetup.player-name"));

    // empire name
    m_empire_name_label = new CUILabel(UserString("GSETUP_EMPIRE_NAME"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_empire_name_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_empire_name_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.empire-name")));
    m_empire_name_edit = new CUIEdit(GetOptionsDB().Get<std::string>("GameSetup.empire-name"));

    // empire color
    m_empire_color_label = new CUILabel(UserString("GSETUP_EMPIRE_COLOR"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_empire_color_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_empire_color_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.empire-color")));
    m_empire_color_selector = new EmpireColorSelector(CONTROL_HEIGHT - CONTROL_VMARGIN);
    m_empire_color_selector->Select(GetOptionsDB().Get<int>("GameSetup.empire-color"));

    // starting species
    m_starting_species_label = new CUILabel(UserString("GSETUP_SPECIES"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_starting_species_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_starting_species_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.starting-species")));
    // Subtract the drop down entry padding for the inner element height.
    m_starting_secies_selector = new SpeciesSelector(LABELS_WIDTH, CONTROL_HEIGHT - 5);
    std::string default_starting_species = GetOptionsDB().Get<std::string>("GameSetup.starting-species");

    if (default_starting_species.empty() || default_starting_species == "1") {
        // kludge / bug workaround for bug with options storage and retreival.
        // Empty-string options are stored, but read in as "true" boolean, and
        // converted to string equal to "1"

        // if no previously-stored species selection, need to pick a default
        std::vector<std::string> selector_avail_species = m_starting_secies_selector->AvailableSpeciesNames();
        if (!selector_avail_species.empty()) {
            for (const std::string& species_name : selector_avail_species) {
                // special case: see if humans are available.
                if ("SP_HUMAN" == species_name) {
                    default_starting_species = "SP_HUMAN";
                    break;
                }
            }
            // if no humans, default to first listed species
            if (default_starting_species.empty())
                default_starting_species = *selector_avail_species.begin();
        }
    }
    m_starting_secies_selector->SelectSpecies(default_starting_species);

    // number of AIs
    m_number_ais_label = new CUILabel(UserString("GSETUP_NUMBER_AIS"), GG::FORMAT_RIGHT, GG::INTERACTIVE);
    m_number_ais_label->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
    m_number_ais_label->SetBrowseText(UserString(GetOptionsDB().GetDescription("GameSetup.ai-players")));
    m_number_ais_spin = new CUISpin<int>(GetOptionsDB().Get<int>("GameSetup.ai-players"), 1, 0, IApp::MAX_AI_PLAYERS(), true);

    // create a temporary texture and static graphic
    static auto temp_tex = std::make_shared<GG::Texture>();
    m_preview_image =  new GG::StaticGraphic(temp_tex, GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE); // create a blank graphic

    m_ok = new CUIButton(UserString("OK"));
    m_cancel = new CUIButton(UserString("CANCEL"));

    AttachChild(m_galaxy_setup_panel);
    AttachChild(m_game_rules_panel);
    AttachChild(m_player_name_label);
    AttachChild(m_player_name_edit);
    AttachChild(m_empire_name_label);
    AttachChild(m_empire_name_edit);
    AttachChild(m_empire_color_label);
    AttachChild(m_empire_color_selector);
    AttachChild(m_starting_species_label);
    AttachChild(m_starting_secies_selector);
    AttachChild(m_number_ais_label);
    AttachChild(m_number_ais_spin);
    AttachChild(m_preview_image);
    AttachChild(m_ok);
    AttachChild(m_cancel);

    ResetDefaultPosition();
    DoLayout();

    m_galaxy_setup_panel->ImageChangedSignal.connect(
        boost::bind(&GalaxySetupWnd::PreviewImageChanged, this, _1));
    m_player_name_edit->EditedSignal.connect(
        boost::bind(&GalaxySetupWnd::PlayerNameChanged, this, _1));
    m_empire_name_edit->EditedSignal.connect(
        boost::bind(&GalaxySetupWnd::EmpireNameChanged, this, _1));
    m_ok->LeftClickedSignal.connect(
        boost::bind(&GalaxySetupWnd::OkClicked, this));
    m_cancel->LeftClickedSignal.connect(
        boost::bind(&GalaxySetupWnd::CancelClicked, this));

    PreviewImageChanged(m_galaxy_setup_panel->PreviewImage());
}

const std::string& GalaxySetupWnd::EmpireName() const
{ return m_empire_name_edit->Text(); }

GG::Clr GalaxySetupWnd::EmpireColor() const
{ return m_empire_color_selector->CurrentColor(); }

const std::string& GalaxySetupWnd::StartingSpeciesName() const
{ return m_starting_secies_selector->CurrentSpeciesName(); }

int GalaxySetupWnd::NumberAIs() const
{ return m_number_ais_spin->Value(); }

std::vector<std::pair<std::string, std::string>> GalaxySetupWnd::GetRulesAsStrings() const
{ return m_game_rules_panel->GetRulesAsStrings(); }

void GalaxySetupWnd::Render() {
    CUIWnd::Render();
    GG::FlatRectangle(GG::Pt(ClientUpperLeft().x + m_preview_ul.x - 2,
                             ClientUpperLeft().y + m_preview_ul.y - 2),
                      GG::Pt(ClientUpperLeft().x + m_preview_ul.x + PREVIEW_SZ.x + 2,
                             ClientUpperLeft().y + m_preview_ul.y + PREVIEW_SZ.y + 2),
                      GG::CLR_BLACK, ClientUI::WndInnerBorderColor(), 1);
}

void GalaxySetupWnd::KeyPress(GG::Key key, std::uint32_t key_code_point, GG::Flags<GG::ModKey> mod_keys) {
    if (!m_ok->Disabled() && (key == GG::GGK_RETURN || key == GG::GGK_KP_ENTER)) // Same behaviour as if "OK" was pressed
        OkClicked();
    else if (key == GG::GGK_ESCAPE) // Same behaviour as if "Cancel" was pressed
        CancelClicked();
}

void GalaxySetupWnd::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    CUIWnd::SizeMove(ul, lr);
    DoLayout();
}

GG::Rect GalaxySetupWnd::CalculatePosition() const {
    GG::Pt new_ul((HumanClientApp::GetApp()->AppWidth() - GalSetupWndWidth()) / 2,
                  (HumanClientApp::GetApp()->AppHeight() - GalSetupWndHeight()) / 2);
    GG::Pt new_sz(GalSetupWndWidth(), GalSetupWndHeight());
    return GG::Rect(new_ul, new_ul + new_sz);
}

void GalaxySetupWnd::DoLayout() {
    m_galaxy_setup_panel->MoveTo(GG::Pt(GG::X0, GG::Y(4)));

    const GG::X LABELS_WIDTH = (GalaxySetupPanel::DefaultWidth() - 5) / 2;

    GG::Pt row_advance(GG::X0, PANEL_CONTROL_SPACING);
    GG::Pt label_ul(CONTROL_MARGIN, GAL_SETUP_PANEL_HT + (PANEL_CONTROL_SPACING - CONTROL_HEIGHT) / 2 + 4);
    GG::Pt label_lr = label_ul + GG::Pt(LABELS_WIDTH, CONTROL_HEIGHT);
    GG::Pt control_ul(LABELS_WIDTH + 2 * CONTROL_MARGIN, GAL_SETUP_PANEL_HT + (PANEL_CONTROL_SPACING - CONTROL_HEIGHT) / 2 + 7);
    GG::Pt control_lr = control_ul + GG::Pt(LABELS_WIDTH, m_player_name_edit->MinUsableSize().y);

    m_player_name_label->SizeMove(label_ul, label_lr);
    m_player_name_edit->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_empire_name_label->SizeMove(label_ul, label_lr);
    m_empire_name_edit->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_empire_color_label->SizeMove(label_ul, label_lr);
    m_empire_color_selector->SizeMove(control_ul, control_lr - GG::Pt(GG::X(75), GG::Y0));

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr += row_advance;

    m_starting_species_label->SizeMove(label_ul, label_lr);
    m_starting_secies_selector->SizeMove(control_ul, control_lr);

    label_ul += row_advance;
    label_lr += row_advance;
    control_ul += row_advance;
    control_lr = control_ul + GG::Pt(GG::X(75),  ClientUI::GetFont()->Height() + 2 * 5);

    m_number_ais_label->SizeMove(label_ul, label_lr);
    m_number_ais_spin->SizeMove(control_ul, control_lr);

    m_preview_ul = GG::Pt(ClientWidth() - PREVIEW_SZ.x - 7, GG::Y(7));
    m_preview_image->SizeMove(m_preview_ul, m_preview_ul + PREVIEW_SZ);


    GG::Pt rules_ul = m_preview_ul + GG::Pt(GG::X0, PREVIEW_SZ.y);
    GG::Pt rules_lr = ClientSize() - GG::Pt(CONTROL_MARGIN, CONTROL_VMARGIN);
    m_game_rules_panel->SizeMove(rules_ul, rules_lr);

    GG::Pt button_ul(CONTROL_MARGIN * 2, ScreenToClient(ClientLowerRight()).y - CONTROL_VMARGIN * 2 - m_ok->MinUsableSize().y);
    GG::Pt button_lr(m_ok->MinUsableSize());
    button_lr.x = std::max(button_lr.x, m_cancel->MinUsableSize().x);
    button_lr.y = std::max(button_lr.y, m_cancel->MinUsableSize().y);
    button_lr += button_ul;

    m_ok->SizeMove(button_ul, button_lr);

    button_ul.x += m_ok->Width() + CONTROL_MARGIN * 2;
    button_lr.x += m_ok->Width() + CONTROL_MARGIN * 2;
    m_cancel->SizeMove(button_ul, button_lr);
}

void GalaxySetupWnd::PreviewImageChanged(std::shared_ptr<GG::Texture> new_image) {
    if (m_preview_image)
        m_preview_image->SetTexture(new_image);
    DoLayout();
}

void GalaxySetupWnd::EmpireNameChanged(const std::string& name)
{ m_ok->Disable(name.empty()); }

void GalaxySetupWnd::PlayerNameChanged(const std::string& name)
{ m_ok->Disable(name.empty()); }

void GalaxySetupWnd::OkClicked() {
    // record selected galaxy setup options as new defaults
    GetOptionsDB().Set("GameSetup.seed",                m_galaxy_setup_panel->GetSeed());
    GetOptionsDB().Set("GameSetup.stars",               m_galaxy_setup_panel->Systems());
    GetOptionsDB().Set("GameSetup.galaxy-shape",        m_galaxy_setup_panel->GetShape());
    GetOptionsDB().Set("GameSetup.galaxy-age",          m_galaxy_setup_panel->GetAge());
    GetOptionsDB().Set("GameSetup.starlane-frequency",  m_galaxy_setup_panel->GetStarlaneFrequency());
    GetOptionsDB().Set("GameSetup.planet-density",      m_galaxy_setup_panel->GetPlanetDensity());
    GetOptionsDB().Set("GameSetup.specials-frequency",  m_galaxy_setup_panel->GetSpecialsFrequency());
    GetOptionsDB().Set("GameSetup.monster-frequency",   m_galaxy_setup_panel->GetMonsterFrequency());
    GetOptionsDB().Set("GameSetup.native-frequency",    m_galaxy_setup_panel->GetNativeFrequency());
    GetOptionsDB().Set("GameSetup.ai-aggression",       m_galaxy_setup_panel->GetAIAggression());
    GetOptionsDB().Set("GameSetup.player-name",         m_player_name_edit->Text());
    GetOptionsDB().Set("GameSetup.empire-name",         EmpireName());
    GetOptionsDB().Set("GameSetup.empire-color",        static_cast<int>(m_empire_color_selector->CurrentItemIndex()));
    GetOptionsDB().Set("GameSetup.starting-species",    m_starting_secies_selector->CurrentSpeciesName());
    GetOptionsDB().Set("GameSetup.ai-players",          m_number_ais_spin->Value());
    GetOptionsDB().Commit();

    m_ended_with_ok = true;
    m_done = true;
}

void GalaxySetupWnd::CancelClicked() {
    m_ended_with_ok = false;
    m_done = true;
}
