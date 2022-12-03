#pragma once
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Buffer.h>
#include "../overworld/bnOverworldPlayerSession.h"
#include "../overworld/bnOverworldPollingPacketProcessor.h"
#include "../overworld/bnOverworldMenuSystem.h"
#include "../bnKeyItemScene.h"
#include "../bnInbox.h"
#include "../bnScene.h"
#include "../bnPA.h"
#include "../bnCardFolderCollection.h"
#include "../bnInputRepeater.h"
#include "../bnRandom.h"
#include "../bnTextArea.h"
#include "bnRevolvingMenuWidget.h"

using Overworld::PollingPacketProcessor;
using Overworld::ServerStatus;
using Overworld::PlayerSession;
using Overworld::MenuSystem;

namespace RealPET {
  struct Particle {
    double lifetime{};
    double max_lifetime{};
    sf::Vector2f acceleration;
    sf::Vector2f velocity;
    sf::Vector2f friction;
    sf::Vector2f position;
    bool scaleIn{}, scaleOut{};
    int type{};
    // below is contextual and mostly unused
    std::optional<sf::Vector2f> click{};
  };

  struct StaticParticle {
    double lifetime{};
    double max_lifetime{};
    sf::Vector2f pos;
    // below is contextual and mostly unused
    double startup_delay{};
  };

  class Homepage final : public Scene {
  private:
    double animElapsed{};
    bool hideTextbox{};
    bool lastIsConnectedState; /*!< Set different animations if the connection has changed */
    bool playJackin{};
    bool mouseClicked{};
    size_t mouseBufferIdx{};
    sf::Vector2f currMenuPosition, otherMenuPosition;
    std::array<sf::Vector2f, 5> lastMousef;
    sf::Sprite bgSprite, jackinSprite, playerSprite, dockSprite, speakSprite;
    std::shared_ptr<sf::Texture> bgTexture, folderTexture, windowTexture, jackinTexture, speakTexture;
    Animation folderAnim, windowAnim, jackinAnim;
    TextArea textbox;
    std::shared_ptr<sf::SoundBuffer> jackinsfx;

    size_t maxPoolSize{};
    std::vector<Particle> pool;

    size_t maxStaticPoolSize{};
    std::vector<StaticParticle> staticPool;

    std::vector<KeyItemScene::Item> items;
    uint16_t maxPayloadSize{};

    /*!< Current player package selection */
    std::shared_ptr<PlayerSession> playerSession;
    std::string currentNaviId, lastSelectedNaviId;

    RevolvingMenuWidget menuWidget, miscMenuWidget;
    InputRepeater repeater;

    CardFolderCollection* folders{ nullptr }; /*!< Collection of folders */
    PA programAdvance;

    enum class state : char {
      menu, misc_menu, size
    } currState{ state::menu };

    void InitializeFolderParticles();
    void InitializeWindowParticles();
    void HandleInput(RevolvingMenuWidget& widget);
    void UpdateFolderParticles(double elapsed);
    void UpdateWindowParticles(double elapsed);
    void DrawFolderParticles(IRenderer& renderer);
    void DrawWindowParticles(IRenderer& renderer);

  public:

    /**
     * @brief Loads the player's library data and loads graphics
     */
    Homepage(swoosh::ActivityController&);
    ~Homepage();

    /**
     * @brief Checks input events and listens for select buttons. Segues to new screens.
     * @param elapsed in seconds
     */
    virtual void onUpdate(double elapsed) override;

    /**
     * @brief Draws the UI
     * @param surface
     */
    virtual void onDraw(IRenderer& surface) override;

    /**
     * @brief Stops music, plays new track
     */
    virtual void onStart() override;

    /**
     * @brief Does nothing
     */
    virtual void onLeave() override;

    /**
     * @brief Drops packet processor
     */
    virtual void onExit() override;

    /**
     * @brief Checks the selected navi if changed and loads the new images
     */
    virtual void onEnter() override;

    /**
     * @brief Resumes scene's music
     */
    virtual void onResume() override;

    /**
     * @brief Stops the music
     */
    virtual void onEnd() override;

    /**
    * @brief Update's the player sprites according to the most recent selection
    */
    void RefreshNaviSprite();

    /**
    * @brief Equip a folder for the navi that was last used when playing as them
    */
    void NaviEquipSelectedFolder();

    //
    // Menu selection callbacks
    //

    void GotoChipFolder();
    void GotoNaviSelect();
    void GotoConfig();
    void GotoMobSelect();
    void GotoPVP();
    void GotoMail();
    void GotoKeyItems();
    void GotoOverworld();
    void GotoPlayerCust();

    std::string& GetCurrentNaviID();
    PA& GetProgramAdvance();
    std::optional<CardFolder*> GetSelectedFolder();
    bool IsInFocus();
  };
}