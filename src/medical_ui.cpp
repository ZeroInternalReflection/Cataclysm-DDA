#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

#include "addiction.h"
#include "avatar_action.h"
#include "creature.h"
#include "character.h"
#include "character_modifier.h"
#include "display.h"
#include "effect.h"
#include "flag.h"
#include "game.h"
#include "input_context.h"
#include "options.h"
#include "output.h"
#include "ui_manager.h"
#include "weather.h"

static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_infected( "infected" );
static const efftype_id effect_mending( "mending" );

static const json_character_flag json_flag_ECTOTHERM( "ECTOTHERM" );
static const json_character_flag json_flag_PAIN_IMMUNE( "PAIN_IMMUNE" );

static const trait_id trait_SUNLIGHT_DEPENDENT( "SUNLIGHT_DEPENDENT" );
static const trait_id trait_TROGLO( "TROGLO" );
static const trait_id trait_TROGLO2( "TROGLO2" );
static const trait_id trait_TROGLO3( "TROGLO3" );

enum class medical_tab_mode {
    TAB_SUMMARY
};

struct medical_entry {
    std::string entry_text;
    std::string detail_text;

    medical_entry( std::string _entry, std::string _detail ) {
        entry_text = _entry;
        detail_text = _detail;
    }

    multiline_list_entry get_entry() const {
        multiline_list_entry entry;
        entry.entry_text = entry_text;
        entry.prefix = "";
        return entry;
    }
};

static std::string coloured_stat_display( int statCur, int statMax )
{
    nc_color cstatus;
    if( statCur <= 0 ) {
        cstatus = c_dark_gray;
    } else if( statCur < statMax / 2 ) {
        cstatus = c_red;
    } else if( statCur < statMax ) {
        cstatus = c_light_red;
    } else if( statCur == statMax ) {
        cstatus = c_white;
    } else if( statCur < statMax * 1.5 ) {
        cstatus = c_light_green;
    } else {
        cstatus = c_green;
    }
    std::string cur = colorize( string_format( _( "%2d" ), statCur ), cstatus );
    return string_format( _( "%s (%s)" ), cur, statMax );
}

static void draw_medical_titlebar( const catacurses::window &window, avatar *player,
                                   const int WIDTH )
{
    input_context ctxt( "MEDICAL", keyboard_mode::keychar );
    const Character &you = *player->as_character();

    werase( window );
    mvwhline( window, point( 0, 0 ), LINE_OXOX, getmaxx( window ) ); // -
    mvwputch( window, point( 0, 0 ), BORDER_COLOR, LINE_OXXO ); // |^
    mvwputch( window, point( getmaxx( window ) - 1, 0 ), BORDER_COLOR, LINE_OOXX ); // ^|

    // Tabs
    const std::vector<std::pair<medical_tab_mode, std::string>> tabs = {
        { medical_tab_mode::TAB_SUMMARY, string_format( _( "SUMMARY" ) ) }
    };
    draw_tabs( window, tabs, medical_tab_mode::TAB_SUMMARY );

    const int TAB_WIDTH = utf8_width( _( "SUMMARY" ) ) + 4;

    // Draw symbols to connect additional lines to border

    int width = getmaxx( window );
    int height = getmaxy( window );
    for( int i = 1; i < height - 1; ++i ) {
        // |
        mvwputch( window, point( 0, i ), BORDER_COLOR, LINE_XOXO );
        // |
        mvwputch( window, point( width - 1, i ), BORDER_COLOR, LINE_XOXO );
    }
    // |-
    mvwputch( window, point( 0, height - 1 ), BORDER_COLOR, LINE_XXXO );
    // -|
    mvwputch( window, point( width - 1, height - 1 ), BORDER_COLOR, LINE_XOXX );

    int right_indent = 2;
    int cur_str_pos = 0;

    // Pain Indicator
    auto pain_descriptor = display::pain_text_color( *player );
    if( !pain_descriptor.first.empty() ) {
        const std::string pain_str = string_format( _( "In %s" ), pain_descriptor.first );

        cur_str_pos = right_print( window, 1, right_indent, pain_descriptor.second, pain_str );

        // Borders
        for( int i = 1; i < getmaxy( window ) - 1; i++ ) {
            mvwputch( window, point( cur_str_pos - 2, i ), BORDER_COLOR, LINE_XOXO ); // |
        }
        mvwputch( window, point( cur_str_pos - 2, 0 ), BORDER_COLOR, LINE_OXXX ); // ^|^
        mvwputch( window, point( cur_str_pos - 2, 2 ), BORDER_COLOR, LINE_XXOX ); // _|_
        right_indent += utf8_width( remove_color_tags( pain_str ) ) + 3;
    }

    const std::pair<std::string, nc_color> hunger_pair = display::hunger_text_color( you );
    const std::pair<std::string, nc_color> thirst_pair = display::thirst_text_color( you );
    const std::pair<std::string, nc_color> fatigue_pair = display::fatigue_text_color( you );

    // Hunger
    if( !hunger_pair.first.empty() ) {
        cur_str_pos = right_print( window, 1, right_indent, hunger_pair.second, hunger_pair.first );

        // Borders
        for( int i = 1; i < getmaxy( window ) - 1; i++ ) {
            mvwputch( window, point( cur_str_pos - 2, i ), BORDER_COLOR, LINE_XOXO ); // |
        }
        mvwputch( window, point( cur_str_pos - 2, 0 ), BORDER_COLOR, LINE_OXXX ); // ^|^
        mvwputch( window, point( cur_str_pos - 2, 2 ), BORDER_COLOR, LINE_XXOX ); // _|_

        right_indent += utf8_width( hunger_pair.first ) + 3;
    }

    // Thirst
    if( !thirst_pair.first.empty() ) {
        cur_str_pos = right_print( window, 1, right_indent, thirst_pair.second, thirst_pair.first );

        // Borders
        for( int i = 1; i < getmaxy( window ) - 1; i++ ) {
            mvwputch( window, point( cur_str_pos - 2, i ), BORDER_COLOR, LINE_XOXO ); // |
        }
        mvwputch( window, point( cur_str_pos - 2, 0 ), BORDER_COLOR, LINE_OXXX ); // ^|^
        mvwputch( window, point( cur_str_pos - 2, 2 ), BORDER_COLOR, LINE_XXOX ); // _|_

        right_indent += utf8_width( thirst_pair.first ) + 3;
    }

    // Fatigue
    if( !fatigue_pair.first.empty() ) {
        cur_str_pos = right_print( window, 1, right_indent, fatigue_pair.second, fatigue_pair.first );

        // Borders
        for( int i = 1; i < getmaxy( window ) - 1; i++ ) {
            mvwputch( window, point( cur_str_pos - 2, i ), BORDER_COLOR, LINE_XOXO ); // |
        }
        mvwputch( window, point( cur_str_pos - 2, 0 ), BORDER_COLOR, LINE_OXXX ); // ^|^
        mvwputch( window, point( cur_str_pos - 2, 2 ), BORDER_COLOR, LINE_XXOX ); // _|_

        right_indent += utf8_width( fatigue_pair.first ) + 3;
    }

    // Hotkey Helper
    std::string desc;
    desc = string_format( _(
                              "[<color_yellow>%s/%s</color>] Scroll info [<color_yellow>%s</color>] Use item [<color_yellow>%s</color>] Keybindings" ),
                          ctxt.get_desc( "SCROLL_INFOBOX_UP" ), ctxt.get_desc( "SCROLL_INFOBOX_DOWN" ),
                          ctxt.get_desc( "APPLY" ), ctxt.get_desc( "HELP_KEYBINDINGS" ) );

    const int details_width = utf8_width( remove_color_tags( desc ) ) + 3;
    const int max_width = right_indent + TAB_WIDTH;
    if( WIDTH - max_width > details_width ) {
        // If the window runs out of room, we won't print keybindings.
        right_print( window, 1, right_indent, c_white, desc );
    }

    center_print( window, 0, c_red, _( " MEDICAL " ) );
}

// Displays a summary of each bodypart's health, including a display for a few 'statuses'
static void generate_health_summary( avatar *player, std::vector<medical_entry> &bp_entries )
{
    for( const bodypart_id &part : player->get_all_body_parts( get_body_part_flags::sorted ) ) {
        std::string header; // Bodypart Title
        std::string hp_str; // Bodypart HP
        std::string detail;
        std::string description;

        const int bleed_intensity = player->get_effect_int( effect_bleed, part );
        const bool bleeding = bleed_intensity > 0;
        const bool bitten = player->has_effect( effect_bite, part.id() );
        const bool infected = player->has_effect( effect_infected, part.id() );
        const bool no_feeling = player->has_flag( json_flag_PAIN_IMMUNE );
        const int maximal_hp = player->get_part_hp_max( part );
        const int current_hp = player->get_part_hp_cur( part );
        const bool limb_is_broken = player->is_limb_broken( part );
        const bool limb_is_mending = player->worn_with_flag( flag_SPLINT, part );

        if( limb_is_mending ) {
            detail += string_format( _( "[ %s ]" ), colorize( _( "SPLINTED" ), c_yellow ) );
            if( no_feeling ) {
                hp_str = colorize( "==%==", c_blue );
            } else {
                const effect &eff = player->get_effect( effect_mending, part );
                const int mend_perc = eff.is_null() ? 0.0 : 100 * eff.get_duration() / eff.get_max_duration();

                const int num = mend_perc / 20;
                hp_str = colorize( std::string( num, '#' ) + std::string( 5 - num, '=' ), c_blue );
            }
        } else if( limb_is_broken ) {
            detail += string_format( _( "[ %s ]" ), colorize( _( "BROKEN" ), c_red ) );
            hp_str = "==%==";
        } else if( no_feeling ) {
            const float cur_hp_pcnt = current_hp / static_cast<float>( maximal_hp );
            if( cur_hp_pcnt < 0.125f ) {
                hp_str = colorize( _( "Very Bad" ), c_red );
            } else if( cur_hp_pcnt < 0.375f ) {
                hp_str = colorize( _( "Bad" ), c_light_red );
            } else if( cur_hp_pcnt < 0.625f ) {
                hp_str = colorize( _( "So-so" ), c_yellow );
            } else if( cur_hp_pcnt < 0.875f ) {
                hp_str = colorize( _( "Okay" ), c_light_green );
            } else {
                hp_str = colorize( _( "Good" ), c_green );
            }
        } else {
            std::pair<std::string, nc_color> h_bar = get_hp_bar( current_hp, maximal_hp, false );
            hp_str = colorize( h_bar.first, h_bar.second ) +
                     colorize( std::string( 5 - utf8_width( h_bar.first ), '.' ), c_white );
        }
        const std::string bp_name = uppercase_first_letter( body_part_name( part, 1 ) );
        header += colorize( bp_name,
                            display::limb_color( *player,
                                    part, true, true, true ) ) + " " + hp_str;

        // BLEEDING block
        if( bleeding ) {
            const effect bleed_effect = player->get_effect( effect_bleed, part );
            const nc_color bleeding_color = colorize_bleeding_intensity( bleed_intensity );
            detail += string_format( _( "[ %s ]" ), colorize( _( "BLEEDING" ), bleeding_color ) );
            description += string_format( "[ %s ] - %s\n",
                                          colorize( bleed_effect.get_speed_name(),  bleeding_color ),
                                          bleed_effect.disp_short_desc() );
        }

        // BITTEN block
        if( bitten ) {
            const effect bite_effect = player->get_effect( effect_bite, part );
            detail += string_format( _( "[ %s ]" ), colorize( _( "BITTEN" ), c_yellow ) );
            description += string_format( "[ %s ] - %s\n",
                                          colorize( bite_effect.get_speed_name(), c_yellow ),
                                          bite_effect.disp_short_desc() );
        }

        // INFECTED block
        if( infected ) {
            const effect infected_effect = player->get_effect( effect_infected, part );
            detail += string_format( _( "[ %s ]" ), colorize( _( "INFECTED" ), c_pink ) );
            description += string_format( "[ %s ] - %s\n",
                                          colorize( infected_effect.get_speed_name(), c_pink ),
                                          infected_effect.disp_short_desc() );
        }

        std::string entry_text;
        if( !detail.empty() ) {
            entry_text = string_format( "[%s] - %s", header, detail );
        } else {
            entry_text = string_format( "[%s]", header );
        }

        const bodypart *bp = player->get_part( part );
        std::string detail_str;
        for( const limb_score &sc : limb_score::get_all() ) {
            if( !part->has_limb_score( sc.getId() ) ) {
                continue;
            }

            float injury_score = bp->get_limb_score( sc.getId(), 0, 0, 1 );
            float max_score = part->get_limb_score( sc.getId() );

            if( injury_score < max_score ) {
                const float injury_modifier = 100 * ( max_score - injury_score ) / max_score;
                std::pair<std::string, nc_color> score_c;
                if( injury_score < max_score * 0.4f ) {
                    score_c.first = string_format( _( "Crippled (-%.f%%)" ), injury_modifier );
                    score_c.second = c_red;
                } else if( injury_score < max_score * 0.6f ) {
                    score_c.first = string_format( _( "Impaired (-%.f%%)" ), injury_modifier );
                    score_c.second = c_light_red;
                } else if( injury_score < max_score * 0.75f ) {
                    score_c.first = string_format( _( "Weakened (-%.f%%)" ), injury_modifier );
                    score_c.second = c_yellow;
                } else if( injury_score < max_score * 0.9f ) {
                    score_c.first = string_format( _( "Weakened (-%.f%%)" ), injury_modifier );
                    score_c.second = c_dark_gray;
                } else {
                    score_c.first = string_format( _( "OK (-%.f%%)" ), injury_modifier );
                    score_c.second = c_dark_gray;
                }
                detail_str += string_format( _( "%s: %s\n" ), sc.name().translated(), colorize( score_c.first,
                                             score_c.second ) );
            } else {
                detail_str += string_format( _( "%s: %s\n" ), sc.name().translated(), colorize( "OK", c_green ) );
            }
        }

        for( const character_modifier &mod : character_modifier::get_all() ) {
            for( const auto &sc : mod.use_limb_scores() ) {
                if( sc.first.is_null() || !part->has_limb_score( sc.first ) ) {
                    continue;
                }
                std::string desc = mod.description().translated();
                float injury_score = bp->get_limb_score( sc.first, 0, 0, 1 );
                float max_score = part->get_limb_score( sc.first );
                nc_color score_c;

                if( injury_score < max_score * 0.4f ) {
                    score_c = c_red;
                } else if( injury_score < max_score * 0.6f ) {
                    score_c = c_light_red;
                } else if( injury_score < max_score * 0.75f ) {
                    score_c = c_yellow;
                } else {
                    score_c = c_white;
                }

                std::string valstr = colorize( string_format( "%.2f", mod.modifier( *player->as_character() ) ),
                                               score_c );
                detail_str += string_format( "%s: %s%s\n", desc, mod.mod_type_str(), valstr );
            }
        }

        std::string entry_details = colorize( string_format( _( "%s STATS" ), to_upper_case( bp_name ) ),
                                              c_light_blue );
        entry_details.append( "\n" );
        entry_details.append( description );
        entry_details.append( "\n" );
        entry_details.append( detail_str );
        bp_entries.emplace_back( entry_text, entry_details );
    }
}

// Displays a summary list of all visible effects.
static void generate_effects_summary( avatar *player, std::vector<medical_entry> &effect_entries )
{
    effect_entries.clear();
    for( const effect &eff : player->get_effects() ) {
        const std::string name = eff.disp_name();
        if( name.empty() ) {
            continue;
        }
        effect_entries.emplace_back( name, eff.disp_desc() );
    }

    const float bmi = player->get_bmi_fat();

    if( bmi < character_weight_category::underweight ) {
        std::string starvation_name;
        std::string starvation_text;

        if( bmi < character_weight_category::emaciated ) {
            starvation_name = _( "Severely Malnourished" );
            starvation_text =
                _( "Your body is severely weakened by starvation.  You might die if you don't start eating regular meals!\n\n" );
        } else {
            starvation_name = _( "Malnourished" );
            starvation_text =
                _( "Your body is weakened by starvation.  Only time and regular meals will help you recover.\n\n" );
        }

        if( bmi < character_weight_category::underweight ) {
            const float str_penalty = 1.0f - ( ( bmi - 13.0f ) / 3.0f );
            starvation_text += std::string( _( "Strength" ) ) + " -" + string_format( "%2.0f%%\n",
                               str_penalty * 100.0f );
            starvation_text += std::string( _( "Dexterity" ) ) + " -" + string_format( "%2.0f%%\n",
                               str_penalty * 50.0f );
            starvation_text += std::string( _( "Intelligence" ) ) + " -" + string_format( "%2.0f%%",
                               str_penalty * 50.0f );
        }

        effect_entries.emplace_back( starvation_name, starvation_text );
    }

    if( player->has_trait( trait_TROGLO3 ) && g->is_in_sunlight( player->pos() ) ) {
        effect_entries.emplace_back( _( "In Sunlight" ), _( "The sunlight irritates you terribly." ) );
    } else if( player->has_trait( trait_TROGLO2 ) && g->is_in_sunlight( player->pos() ) &&
               incident_sun_irradiance( get_weather().weather_id, calendar::turn ) > irradiance::low ) {
        effect_entries.emplace_back( _( "In Sunlight" ), _( "The sunlight irritates you badly." ) );
    } else if( ( player->has_trait( trait_TROGLO ) || player->has_trait( trait_TROGLO2 ) ) &&
               g->is_in_sunlight( player->pos() ) &&
               incident_sun_irradiance( get_weather().weather_id, calendar::turn ) > irradiance::moderate ) {
        effect_entries.emplace_back( _( "In Sunlight" ), _( "The sunlight irritates you." ) );
    }

    for( addiction &elem : player->addictions ) {
        if( elem.sated < 0_turns && elem.intensity >= MIN_ADDICTION_LEVEL ) {
            effect_entries.emplace_back( elem.type->get_name().translated(),
                                         elem.type->get_description().translated() );
        }
    }

    if( effect_entries.empty() ) {
        effect_entries.emplace_back( colorize( _( "None" ), c_dark_gray ), _( "No effects active" ) );
    }
}

// Generates a summary list of the player's statistics.
static void generate_stats_summary( avatar *player, std::vector<medical_entry> &stat_entries )
{
    stat_entries.clear();

    std::string speed_detail_str;
    int runcost = player->run_cost( 100 );
    int newmoves = player->get_speed();

    const int speed_modifier = player->get_enchantment_speed_bonus();

    std::string pge_str;
    if( speed_modifier != 0 ) {
        pge_str = pgettext( "speed bonus", "Bio/Mut/Effects " );
        speed_detail_str += colorize( string_format( _( "%s    -%2d%%\n" ), pge_str, speed_modifier ),
                                      c_green );
    }

    int pen = 0;

    if( player->weight_carried() > player->weight_capacity() ) {
        pen = 25 * ( player->weight_carried() - player->weight_capacity() ) / player->weight_capacity();
        pge_str = pgettext( "speed penalty", "Overburdened " );
        speed_detail_str += colorize( string_format( _( "%s    -%2d%%\n" ), pge_str, pen ), c_red );
    }

    pen = player->get_pain_penalty().speed;
    if( pen >= 1 ) {
        pge_str = pgettext( "speed penalty", "Pain " );
        speed_detail_str += colorize( string_format( _( "%s    -%2d%%\n" ), pge_str, pen ), c_red );
    }
    if( player->get_thirst() > 40 ) {
        pen = std::abs( Character::thirst_speed_penalty( player->get_thirst() ) );
        pge_str = pgettext( "speed penalty", "Thirst " );
        speed_detail_str += colorize( string_format( _( "%s    -%2d%%\n" ), pge_str, pen ), c_red );
    }
    if( player->kcal_speed_penalty() < 0 ) {
        pen = std::abs( player->kcal_speed_penalty() );
        pge_str = pgettext( "speed penalty", player->get_bmi() < character_weight_category::underweight ?
                            "Starving" : "Underfed" );
        speed_detail_str += colorize( string_format( _( "%s    -%2d%%\n" ), pge_str, pen ), c_red );
    }
    if( player->has_trait( trait_SUNLIGHT_DEPENDENT ) && !g->is_in_sunlight( player->pos() ) ) {
        pen = ( g->light_level( player->posz() ) >= 12 ? 5 : 10 );
        pge_str = pgettext( "speed penalty", "Out of Sunlight " );
        speed_detail_str += colorize( string_format( _( "%s     -%2d%%\n" ), pge_str, pen ), c_red );
    }

    const float temperature_speed_modifier = player->mutation_value( "temperature_speed_modifier" );
    if( temperature_speed_modifier != 0 ) {
        nc_color pen_color;
        std::string pen_sign;
        const units::temperature player_local_temp = get_weather().get_temperature( player->pos() );
        if( player->has_flag( json_flag_ECTOTHERM ) && player_local_temp > units::from_fahrenheit( 65 ) ) {
            pen_color = c_green;
            pen_sign = "+";
        } else if( player_local_temp < units::from_fahrenheit( 65 ) ) {
            pen_color = c_red;
            pen_sign = "-";
        }
        if( !pen_sign.empty() ) {
            pen = ( units::to_fahrenheit( player_local_temp ) - 65 ) * temperature_speed_modifier;
            pge_str = pgettext( "speed modifier", "Cold-Blooded " );
            speed_detail_str += colorize( string_format( _( "%s     %s%2d%%\n" ), pge_str, pen_sign,
                                          std::abs( pen ) ), pen_color );
        }
    }

    std::map<std::string, int> speed_effects;
    for( const effect &elem : player->get_effects() ) {
        bool reduced = player->resists_effect( elem );
        int move_adjust = elem.get_mod( "SPEED", reduced );
        if( move_adjust != 0 ) {
            const std::string dis_text = elem.get_speed_name();
            speed_effects[dis_text] += move_adjust;
        }
    }

    for( const std::pair<const std::string, int> &speed_effect : speed_effects ) {
        nc_color col = speed_effect.second > 0 ? c_green : c_red;
        speed_detail_str += colorize( string_format( _( "%s    %s%d%%\n" ), speed_effect.first,
                                      ( speed_effect.second > 0 ? "+" : "-" ),
                                      std::abs( speed_effect.second ) ), col );
    }

    std::string coloured_str = colorize( string_format( _( "%d" ), runcost ),
                                         ( runcost <= 100 ? c_green : c_red ) );
    std::string assembled_details = colorize(
                                        _( "Base move cost is the final modified movement cost taken to traverse flat ground." ),
                                        c_light_blue );
    assembled_details.append( "\n" );
    assembled_details.append( speed_detail_str );
    stat_entries.emplace_back( string_format( _( "Base Move Cost: %s" ), coloured_str ),
                               assembled_details );

    coloured_str = colorize( string_format( _( "%d" ), newmoves ),
                             ( newmoves >= 100 ? c_green : c_red ) );
    assembled_details = colorize(
                            _( "Speed determines the amount of actions or movement points you can perform in a turn." ),
                            c_light_blue );
    assembled_details.append( "\n" );
    assembled_details.append( speed_detail_str );
    stat_entries.emplace_back( string_format( _( "Current Speed: %s" ), coloured_str ),
                               assembled_details );

    std::string strength_str = coloured_stat_display( player->get_str(), player->get_str_base() );
    stat_entries.emplace_back( string_format( _( "Strength: %s" ), strength_str ),
                               _( "Strength affects your melee damage, the amount of weight you can carry, your total HP, "
                                  "your resistance to many diseases, and the effectiveness of actions which require brute force." ) );

    std::string dexterity_str = coloured_stat_display( player->get_dex(), player->get_dex_base() );
    stat_entries.emplace_back( string_format( _( "Dexterity: %s" ), dexterity_str ),
                               _( "Dexterity affects your chance to hit in melee combat, helps you steady your "
                                  "gun for ranged combat, and enhances many actions that require finesse." ) );

    std::string intelligence_str = coloured_stat_display( player->get_int(), player->get_int_base() );
    stat_entries.emplace_back( string_format( _( "Intelligence: %s" ), intelligence_str ),
                               _( "Intelligence is less important in most situations, but it is vital for more complex tasks like "
                                  "electronics crafting.  It also affects how much skill you can pick up from reading a book." ) );

    std::string perception_str = coloured_stat_display( player->get_per(), player->get_per_base() );
    stat_entries.emplace_back( string_format( _( "Perception: %s" ), perception_str ),
                               _( "Perception is the most important stat for ranged combat.  It's also used for "
                                  "detecting traps and other things of interest." ) );
}

void avatar::disp_medical()
{
    // Windows
    catacurses::window w_title; // Title Bar - Tabs, Pain Indicator & Blood Indicator
    catacurses::window wMedical; // Primary Window
    catacurses::window w_description; // Bottom Detail Bar
    catacurses::window w_left;
    catacurses::window w_mid;
    catacurses::window w_right;

    scrolling_text_view details( w_description );
    bool details_recalc = true;

    multiline_list bp_list( w_left );
    bp_list.list_id = 0;
    multiline_list effect_list( w_mid );
    effect_list.list_id = 1;
    multiline_list stat_list( w_right );
    stat_list.list_id = 2;
    std::vector<medical_entry> bp_entries;
    std::vector<medical_entry> effect_entries;
    std::vector<medical_entry> stat_entries;
    generate_health_summary( this, bp_entries );
    generate_stats_summary( this, stat_entries );
    generate_effects_summary( this, effect_entries );

    // Window Definitions
    const int TITLE_W_HEIGHT = 3;
    const int DESC_W_HEIGHT = 6; // Consistent with Player Info (@) Menu
    const int HEADER_Y = TITLE_W_HEIGHT;
    const int TEXT_START_Y = HEADER_Y + 1;
    int DESC_W_BEGIN;
    int HEIGHT;
    int WIDTH;

    // Column Definitions
    int second_column_x = 0;
    int third_column_x = 0;

    // Cursor
    point cursor;

    const std::string screen_reader_mode = get_option<std::string>( "SCREEN_READER_MODE" );

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const int WIDTH_OFFSET = ( TERMX - FULL_SCREEN_WIDTH ) / 4;
        HEIGHT = std::min( TERMY, FULL_SCREEN_HEIGHT );
        WIDTH = FULL_SCREEN_WIDTH + WIDTH_OFFSET;

        const point win( ( TERMX - WIDTH ) / 2, ( TERMY - HEIGHT ) / 2 );

        wMedical = catacurses::newwin( HEIGHT, WIDTH, win );

        w_title = catacurses::newwin( TITLE_W_HEIGHT, WIDTH, win );

        DESC_W_BEGIN = HEIGHT - DESC_W_HEIGHT - 1;
        w_description = catacurses::newwin( DESC_W_HEIGHT, WIDTH - 1,
                                            win + point( 0, DESC_W_BEGIN ) );
        details_recalc = true;

        //40% - 30% - 30%
        second_column_x = WIDTH / 2.5f;
        third_column_x = second_column_x + WIDTH / 3.3f;
        w_left = catacurses::newwin( DESC_W_BEGIN - TEXT_START_Y - 1, WIDTH / 2.5f - 1,
                                     win + point( 0, TEXT_START_Y ) );
        w_mid = catacurses::newwin( DESC_W_BEGIN - TEXT_START_Y - 1, WIDTH / 3.3f - 1,
                                    win + point( second_column_x, TEXT_START_Y ) );
        w_right = catacurses::newwin( DESC_W_BEGIN - TEXT_START_Y - 1, WIDTH / 3.3f - 1,
                                      win + point( third_column_x, TEXT_START_Y ) );

        bp_list.fold_entries();
        effect_list.fold_entries();
        stat_list.fold_entries();

        ui.position_from_window( wMedical );
    } );

    ui.mark_resize();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( wMedical );

        // Description Text
        std::string detail_str = "";
        switch( cursor.x ) {
            case 0:
                if( screen_reader_mode == "orca" ) {
                    detail_str = bp_entries[cursor.y].entry_text + "\n";
                }
                detail_str = detail_str.append( bp_entries[cursor.y].detail_text );
                break;
            case 1:
                if( screen_reader_mode == "orca" ) {
                    detail_str = effect_entries[cursor.y].entry_text + "\n";
                }
                detail_str = detail_str.append( effect_entries[cursor.y].detail_text );
                break;
            case 2:
                if( screen_reader_mode == "orca" ) {
                    detail_str = stat_entries[cursor.y].entry_text + "\n";
                }
                detail_str = detail_str.append( stat_entries[cursor.y].detail_text );
                break;
            default:
                break;
        }

        if( details_recalc ) {
            details.set_text( detail_str );
            details_recalc = false;
        }

        // Print column headers
        fold_and_print( wMedical, point( 2, HEADER_Y ), WIDTH - 2, c_light_blue, _( "HEALTH" ) );
        mvwprintz( wMedical, point( second_column_x + 2, HEADER_Y ), c_light_blue, _( "EFFECTS" ) );
        mvwprintz( wMedical, point( third_column_x + 2, HEADER_Y ), c_light_blue, _( "STATS" ) );

        // Overall borders and header
        draw_border( wMedical );
        if( !detail_str.empty() ) {
            mvwputch( wMedical, point( 0, DESC_W_BEGIN - 1 ), BORDER_COLOR, LINE_XXXO ); // |-
            mvwhline( wMedical, point( 1, DESC_W_BEGIN - 1 ), LINE_OXOX, getmaxx( wMedical ) - 2 ); // -
            mvwputch( wMedical, point( getmaxx( wMedical ) - 1, DESC_W_BEGIN - 1 ), BORDER_COLOR,
                      LINE_XOXX ); // -|
            mvwputch( wMedical, point( second_column_x, DESC_W_BEGIN - 1 ), BORDER_COLOR, LINE_XXOX ); // _|_
            mvwputch( wMedical, point( third_column_x, DESC_W_BEGIN - 1 ), BORDER_COLOR, LINE_XXOX ); // _|_
        }

        // UI Header
        draw_medical_titlebar( w_title, this, WIDTH );
        mvwputch( w_title, point( second_column_x, HEADER_Y - 1 ), BORDER_COLOR, LINE_OXXX ); // ^|^
        mvwputch( w_title, point( third_column_x, HEADER_Y - 1 ), BORDER_COLOR, LINE_OXXX ); // ^|^

        mvwputch( wMedical, point( 0, 2 ), BORDER_COLOR, LINE_XXXO ); // |-
        mvwputch( wMedical, point( getmaxx( wMedical ) - 1, 2 ), BORDER_COLOR, LINE_XOXX ); // -|
        mvwputch( wMedical, point( second_column_x, HEADER_Y ), BORDER_COLOR, LINE_XOXO ); // |
        mvwputch( wMedical, point( third_column_x, HEADER_Y ), BORDER_COLOR, LINE_XOXO ); // |;

        wnoutrefresh( wMedical );
        wnoutrefresh( w_title );

        // Print columns, then description.  Screen reader cursor position is set when drawing description
        bp_list.print_entries( cursor.x );
        effect_list.print_entries( cursor.x );
        stat_list.print_entries( cursor.x );
        details.draw( c_light_gray );
    } );

    input_context ctxt( "MEDICAL" );
    ctxt.register_action( "LEFT" );
    ctxt.register_action( "RIGHT" );
    ctxt.register_action( "APPLY" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    details.set_up_navigation( ctxt, scrolling_key_scheme::angle_bracket_scroll );

    /* Given the cramped layout, it can be difficult to move the mouse down to the description window
     * without accidentally moving the mouse over another entry.  So, require a click to select an entry
     * rather than just hovering with the mouse.
     */
    bp_list.set_up_navigation( ctxt, false );
    effect_list.set_up_navigation( ctxt, false );
    stat_list.set_up_navigation( ctxt, false );

    bp_list.create_entries( bp_entries );
    bp_list.set_entry_pos( 0, false );
    effect_list.create_entries( effect_entries );
    effect_list.set_entry_pos( 0, false );
    stat_list.create_entries( stat_entries );
    stat_list.set_entry_pos( 0, false );

    for( ;; ) {
        ui_manager::redraw();
        std::string action = ctxt.handle_input();

        if( details.handle_navigation( action, ctxt ) ) {
            // No further action required
        } else if( bp_list.handle_navigation( action, ctxt, cursor.x ) ) {
            cursor.x = bp_list.list_id;;
            cursor.y = bp_list.get_entry_pos();
            details_recalc = true;
        } else if( effect_list.handle_navigation( action, ctxt, cursor.x ) ) {
            cursor.x = effect_list.list_id;;
            cursor.y = effect_list.get_entry_pos();
            details_recalc = true;
        } else if( stat_list.handle_navigation( action, ctxt, cursor.x ) ) {
            cursor.x = stat_list.list_id;;
            cursor.y = stat_list.get_entry_pos();
            details_recalc = true;
        } else if( action == "RIGHT" || action == "LEFT" ) {
            const int step = cursor.x + ( action == "RIGHT" ? 1 : -1 );
            const int limit = 2;
            if( step == -1 ) {
                cursor.x = limit;
            } else if( step > limit ) {
                cursor.x = 0;
            } else {
                cursor.x = step;
            }
            if( cursor.x == 0 ) {
                cursor.y = bp_list.get_entry_pos();
            } else if( cursor.x == 1 ) {
                cursor.y = effect_list.get_entry_pos();
            } else {
                cursor.y = stat_list.get_entry_pos();
            }
            details_recalc = true;
        } else if( action == "APPLY" ) {
            avatar_action::use_item( *this );
            details_recalc = true;
            generate_health_summary( this, bp_entries );
            generate_stats_summary( this, stat_entries );
            generate_effects_summary( this, effect_entries );
        } else if( action == "QUIT" ) {
            break;
        }
    }
}
