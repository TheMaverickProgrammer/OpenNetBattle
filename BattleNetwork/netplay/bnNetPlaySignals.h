#pragma once

enum class NetPlaySignals : uint32_t {
  none = 0,
  ///////////////////////
  //       ACKs        //
  ///////////////////////
  ack,

  ///////////////////////
  //    Matchmaking    //
  ///////////////////////
  matchmaking_request,
  matchmaking_handshake,

  ///////////////////////
  //  PVP Battle Cmds  // 
  ///////////////////////
  handshake, // send round information along with hand and form selections
  sync,
  frame_data,

  ///////////////////////
  // PVP Download Cmds //
  ///////////////////////
  download_handshake,
  coin_flip,            // Keep flipping coin until both clients agree
  trade_player_package, // Swap player package data with opponent
  trade_card_package_list, // Swap card packages list with opponent
  trade_block_package_list, // Swap block packages list with opponent
  player_package_request, // Ask to download only specific package
  player_package_download, // Download player package data
  card_package_request, // Ask to download only specific package
  card_package_download, // Download card package data
  block_package_request, // Ask to download only specific package
  block_package_download, // Download block package data
  downloads_complete,
  download_transition, // transition to pvp

  ///////////////////////
  //     Misc. Cmds    //
  ///////////////////////
  ping // to keep from kicking 
};