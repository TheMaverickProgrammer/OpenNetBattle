/*! \file bnTextureResourceManager.h */

/*! \brief resource manager for texture data
 * 
 * Texture resource manager provides utilities to load textures from disc 
 * as well as hard-coded textures loaded at startup.
 */

#pragma once
#include "bnResourcePaths.h"
#include "bnLogger.h"
#include "bnCachedResource.h"

#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <iostream>
#include <atomic>

using std::cerr;
using std::endl;
using std::vector;
using std::map;
using std::pair;
using sf::Texture;
using std::string;

class TextureResourceManager {
public:
  TextureResourceManager();
  ~TextureResourceManager();

  // no copies
  TextureResourceManager(const TextureResourceManager&) = delete;

  /**
   * @brief Loads all hard-coded textures
   * @param status Increases the count after each texture loads
   */
  void LoadAllTextures(std::atomic<int> &status);

  /**
  * @brief will clean expired textures from the cache and free image data
  */
  void HandleExpiredTextureCache();
  
  /**
   * @brief Given a file path, returns a pointer to the loaded texture
   * @param _path Relative path to the application
   * @return Texture. The texture is cached.
   */
  std::shared_ptr<Texture> LoadFromFile(string _path);

private:
  std::mutex mutex;
  vector<string> paths; /**< Paths to all textures. Must be in order of TextureType @see TextureType */
  map<std::string, CachedResource<Texture>> texturesFromPath; /**< Cache for textures loaded at run-time */
};