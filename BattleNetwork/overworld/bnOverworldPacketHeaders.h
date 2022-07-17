#pragma once

#include <stdint.h>
#include <string>

namespace Overworld
{
  constexpr std::string_view VERSION_ID = "https://github.com/ArthurCose/Scriptable-OpenNetBattle-Server";
  const uint64_t VERSION_ITERATION = 49;

  constexpr double PACKET_RESEND_RATE = 1.0 / 20.0;

  // server expects uint16_t codes
  enum class ClientEvents : uint16_t
  {
    version_request = 0,
    ack,
    server_message, // not used by clients
    authorize,
    heartbeat,
    asset_found,
    asset_stream,
    login,
    logout,
    request_join,
    ready,
    transferred_out,
    position,
    avatar_change,
    emote,
    custom_warp,
    object_interaction,
    actor_interaction,
    tile_interaction,
    textbox_response,
    prompt_response,
    board_open,
    board_close,
    post_request,
    post_selection,
    shop_close,
    shop_purchase,
    battle_results,
    terminal_command,
    size,
    unknown = size
  };

  enum ClientAssetType : char {
    texture = 0,
    animation,
    mugshot_texture,
    mugshot_animation,
  };

  enum class ServerEvents : uint16_t
  {
    version_info = 0,
    ack,
    heartbeat,
    authorize,
    login,
    connection_complete,
    transfer_warp,
    transfer_start,
    transfer_complete,
    transfer_server,
    kick,
    remove_asset,
    asset_stream_start,
    asset_stream,
    preload,
    custom_emotes_path,
    map,
    health,
    emotion,
    money,
    add_item,
    remove_item,
    play_sound,
    exclude_object,
    include_object,
    exclude_actor,
    include_actor,
    move_camera,
    slide_camera,
    shake_camera,
    fade_camera,
    track_with_camera,
    enable_camera_controls,
    unlock_camera,
    enable_camera_zoom,
    disable_camera_zoom,
    lock_input,
    unlock_input,
    teleport,
    message,
    question,
    quiz,
    prompt,
    open_board,
    prepend_posts,
    append_posts,
    remove_post,
    post_selection_ack,
    close_bbs,
    shop_inventory,
    open_shop,
    package_offer,
    load_package,
    mod_whitelist,
    mod_blacklist,
    initiate_mob,
    initiate_pvp,
    actor_connected,
    actor_disconnect,
    actor_set_name,
    actor_move_to,
    actor_set_avatar,
    actor_emote,
    actor_animate,
    actor_keyframes,
    actor_minimap_color,
    synchronize_updates,
    end_synchronization,
    terminal_response,
    size,
    unknown = size
  };

  enum class AssetType : char {
    text = 0,
    compressed_text,
    texture,
    audio,
    data,
  };

  enum class PackageType : char {
    blocks,
    card,
    encounter,
    character,
    library,
    player,
  };
} // namespace Overworld
