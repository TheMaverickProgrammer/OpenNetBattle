#pragma once
/** @brief All hard-coded resource paths used in game**/

#define path static const char*

namespace TexturePaths {
  //Tiles
  path TILE_ATLAS_BLUE = "resources/tiles/tile_atlas_blue.png";
  path TILE_ATLAS_RED = "resources/tiles/tile_atlas_red.png";
  path TILE_ATLAS_UNK = "resources/tiles/tile_atlas_unknown.png";

  //Mobs
  path MOB_NAME_BACKDROP = "resources/ui/mob_name_backdrop.png";
  path MOB_NAME_EDGE = "resources/ui/mob_name_backdrop_edge.png";
  path MOB_MOVE = "resources/scenes/battle/mob_move.png";
  path MOB_EXPLOSION = "resources/scenes/battle/mob_explosion.png";
  path MOB_BOSS_SHINE = "resources/scenes/battle/boss_shine.png";

  // Battle Misc
  path CUST_GAUGE = "resources/ui/custom.png";
  path MISC_COUNTER_REVEAL = "resources/scenes/battle/counter_reveal.png";
  path MISC_SMALL_SHADOW = "resources/scenes/battle/small_shadow.png";
  path MISC_BIG_SHADOW = "resources/scenes/battle/big_shadow.png";

  //Spells
  path SPELL_BULLET_HIT = "resources/scenes/battle/spells/spell_bullet_hit.png";
  path SPELL_BUSTER_CHARGE = "resources/scenes/battle/spells/spell_buster_charge.png";
  path SPELL_CHARGED_BULLET_HIT = "resources/scenes/battle/spells/spell_charged_bullet_hit.png";
  path SPELL_BUBBLE_TRAP = "resources/scenes/battle/spells/bubble_trap.png";
  path SPELL_POOF = "resources/scenes/battle/spells/poof.png";
  path ICE_FX = "resources/scenes/battle/spells/ice_fx.png";
  path BLIND_FX = "resources/scenes/battle/blind.png";

  //Card Select 
  path CHIP_SELECT_MENU = "resources/ui/card_select.png";
  path CHIP_SELECT_MEGA_OVERLAY = "resources/ui/card_select_mega_overlay.png";
  path CHIP_SELECT_GIGA_OVERLAY = "resources/ui/card_select_giga_overlay.png";
  path CHIP_SELECT_DARK_OVERLAY = "resources/ui/card_select_dark_overlay.png";
  path CHIP_CURSOR_SMALL = "resources/ui/card_cursor_small.png";
  path CHIP_CURSOR_BIG = "resources/ui/card_cursor_big.png";
  path CHIP_NOSUPPORT = "resources/ui/card_nosupport.png";
  path CHIP_NODATA = "resources/ui/card_nodata.png";
  path CHIP_SENDDATA = "resources/ui/card_senddata.png";
  path CHIP_MISSINGDATA = "resources/ui/card_missing.png";
  path CHIP_ICON_MISSINGDATA = "resources/ui/icon_missing.png";
  path CHIP_LOCK = "resources/ui/card_lock.png";
  path CHIP_FRAME = "resources/ui/card_frame.png";
  path CUST_BADGE = "resources/ui/cust_badge.png";
  path CUST_BADGE_MASK = "resources/ui/cust_badge_mask.png";
  path CUST_FORM_SELECT = "resources/ui/form_select.png";
  path CUST_FORM_CURSOR = "resources/ui/form_cursor.png";
  path CUST_FORM_ITEM_BG = "resources/ui/form_item_bg.png";

  // In-battle signs
  path PROGRAM_ADVANCE = "resources/ui/program_advance.png";
  path BATTLE_START = "resources/ui/battle_start.png";
  path ENEMY_DELETED = "resources/ui/enemy_deleted.png";
  path DOUBLE_DELETE = "resources/ui/double_delete.png";
  path TRIPLE_DELETE = "resources/ui/triple_delete.png";
  path COUNTER_HIT = "resources/ui/counter_hit.png";
  path ELEMENT_ALERT = "resources/ui/alert.png";

  // Battle Result
  path BATTLE_RESULTS_FRAME = "resources/ui/battle_results.png";
  path BATTLE_RESULTS_ZENNY = "resources/ui/zenny.png";
  path BATTLE_RESULTS_NODATA = "resources/ui/nodata.png";
  path BATTLE_RESULTS_PRESS_A = "resources/ui/press_a.png";
  path BATTLE_RESULTS_STAR = "resources/ui/counter_star.png";

  // Folder View
  path FOLDER_CHIP_HOLDER = "resources/ui/folder_card.png";
  path FOLDER_CHIP_HOLDER_MEGA = "resources/ui/folder_card_mega.png";
  path FOLDER_CHIP_HOLDER_GIGA = "resources/ui/folder_card_giga.png";
  path FOLDER_CHIP_HOLDER_DARK = "resources/ui/folder_card_dark.png";
  path FOLDER_DOCK = "resources/ui/folder_dock.png";
  path PACK_DOCK = "resources/ui/pack_dock.png";
  path FOLDER_RARITY = "resources/ui/folder_rarity.png";
  path FOLDER_SCROLLBAR = "resources/ui/scrollbar.png";
  path FOLDER_CURSOR = "resources/ui/select_cursor.png";
  path FOLDER_EQUIP = "resources/ui/folder_equip.png";
  path FOLDER_BOX = "resources/ui/folder_name.png";
  path FOLDER_BOX_CURSOR = "resources/ui/folder_cursor.png";
  path FOLDER_OPTIONS = "resources/ui/folder_options.png";
  path FOLDER_OPTIONS_NEW = "resources/ui/folder_new.png";
  path FOLDER_SIZE = "resources/ui/folder_size.png";
  path FOLDER_TEXTBOX = "resources/ui/folder_textbox.png";
  path FOLDER_MB = "resources/ui/mb_placeholder.png";
  path FOLDER_NEXT_ARROW = "resources/ui/folder_edit_next.png";
  path FOLDER_SORT = "resources/ui/folder_sort.png";
  path LETTER_CURSOR = "resources/ui/letter_cursor.png";

  // Navi Select View
  path GLOWING_PAD_ATLAS = "resources/scenes/select/glow_sheet.png";
  path GLOWING_PAD_BASE = "resources/scenes/select/pad_base.png";
  path GLOWING_PAD_BOTTOM = "resources/scenes/select/pad_bottom.png";
  path CHAR_NAME = "resources/scenes/select/char_name.png";
  path CHAR_STAT = "resources/scenes/select/stat.png";
  path CHAR_ELEMENT = "resources/scenes/select/element.png";
  path CHAR_INFO_BOX = "resources/scenes/select/info_box.png";
  path SYMBOL_SLOTS = "resources/scenes/select/symbol_slots.png";

  // Navigator and textbox
  path MUG_NAVIGATOR = "resources/ui/navigator.png";
  path ANIMATED_TEXT_BOX = "resources/ui/textbox.png";
  path TEXT_BOX_NEXT_CURSOR = "resources/ui/textbox_next.png";
  path TEXT_BOX_CURSOR = "resources/ui/textbox_cursor.png";

  //Background/Foreground
  path BG_BLUE = "resources/scenes/title/bg_blue.png";
  path TITLE_ANIM_CHAR = "resources/scenes/title/prog-pulse.png";
  path GAME_OVER = "resources/scenes/game_over/game_over.png";
  path BATTLE_SELECT_BG = "resources/scenes/select/battle_select.png";
  path FOLDER_VIEW_BG = "resources/scenes/folder/bg.png";
  path FOLDER_INFO_BG = "resources/scenes/folder/folder_info.png";
  path FOLDER_CHANGE_NAME_BG = "resources/scenes/folder/folder_name.png";
  path NAVI_SELECT_BG = "resources/scenes/select/bg.png";

  // UI OTHER / ICONS
  path AURA_NUMSET = "resources/ui/aura_numset.png";
  path ENEMY_HP_NUMSET = "resources/ui/hp_numset.png";
  path PLAYER_HP_NUMSET = "resources/ui/player_numset.png";
  path GAMEPAD_SUPPORT_ICON = "resources/ui/gamepad_support_icon.png";
  path MAIN_MENU_UI = "resources/ui/main_menu_ui.png";
  path ELEMENT_ICON = "resources/ui/elements.png";

  // SHADER TEXTURE MAPS
  path DISTORTION_TEXTURE = "resources/shaders/textures/distortion.png";
  path HEAT_TEXTURE = "resources/shaders/textures/heat.png";
  path NOISE_TEXTURE = "resources/shaders/textures/noise.png";
  path NOISE_INVERT_TEXTURE = "resources/shaders/textures/noise_invert.png";

  // MISC UI
  path LIGHT = "resources/ui/light.png";
  path WAITING_SPINNER = "resources/ui/spinner.png";
  path SCREEN_BAR = "resources/ui/screen_bar.png";

  // FONT
  path FONT = "resources/fonts/fonts_compressed.png";

  // CONFIG UI
  path AUDIO_ICO = "resources/scenes/config/audio.png";
  path END_BTN = "resources/scenes/config/end_btn.png";
};

namespace AnimationPaths {
  path ICE_FX = "resources/scenes/battle/spells/ice_fx.animation";
  path BLIND_FX = "resources/scenes/battle/blind.animation";
  path MISC_COUNTER_REVEAL = "resources/scenes/battle/counter_reveal.animation";
}

namespace SoundPaths {
  path ICE_FX = "resources/sfx/freeze.ogg";
}
#undef path