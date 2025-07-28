#include "bnSmartShader.h"

  SmartShader::SmartShader() {
    ref = nullptr;
  }

  SmartShader::SmartShader(const SmartShader& copy) {
    funiforms = copy.funiforms;
    iuniforms = copy.iuniforms;
    vfuniforms = copy.vfuniforms;
    duniforms = copy.duniforms;
    farruniforms = copy.farruniforms;
    coluniforms = copy.coluniforms;
    texuniforms = copy.texuniforms;
    textypeuniforms = copy.textypeuniforms;

    ref = copy.ref;
  }

  SmartShader::~SmartShader() {
    ref = nullptr;
  }

  SmartShader::SmartShader(const sf::Shader& rhs) {
    ref = &const_cast<sf::Shader&>(rhs);
  }

 SmartShader& SmartShader::operator=(const sf::Shader& rhs) {
   ref = &const_cast<sf::Shader&>(rhs);
   return *this;
  }

 SmartShader& SmartShader::operator=(const sf::Shader* rhs) {
   ref = const_cast<sf::Shader*>(rhs);
   return *this;
 }

 sf::Shader* SmartShader::Get() {
   ApplyUniforms();
   return ref;
 }

 bool SmartShader::HasShader() {
   return ref != nullptr;
 }

  void SmartShader::ApplyUniforms() {
    if (!ref) return;

    iiter iIter = iuniforms.begin();
    fiter fIter = funiforms.begin();
    diter dIter = duniforms.begin();
    farriter farrIter = farruniforms.begin();
    vfiter vfIter = vfuniforms.begin();
    coliter colIter = coluniforms.begin();
    texiter texIter = texuniforms.begin();
    textypeiter texTypeIter = textypeuniforms.begin();

    for (; iIter != iuniforms.end(); iIter++) {
      ref->setUniform(iIter->first, iIter->second);
    }

    for (; fIter != funiforms.end(); fIter++) {
      ref->setUniform(fIter->first, fIter->second);
    }

    for (; dIter != duniforms.end(); dIter++) {
      ref->setUniform(dIter->first, static_cast<float>(dIter->second));
    }

    for (; farrIter != farruniforms.end(); farrIter++) {
      ref->setUniformArray(farrIter->first, farrIter->second.data(), farrIter->second.size());
    }

    for (; vfIter != vfuniforms.end(); vfIter++) {
      ref->setUniform(vfIter->first, vfIter->second);
    }

    for (; colIter != coluniforms.end(); colIter++) {
      sf::Color col = colIter->second;
      ref->setUniform(colIter->first, 
        sf::Glsl::Vec4{ 
          col.r/255.f,
          col.g/255.f, 
          col.b/255.f, 
          col.a/255.f 
        }
      );
    }

    for (; texIter != texuniforms.end(); texIter++) {
      ref->setUniform(texIter->first, *texIter->second);
    }

    for (; texTypeIter != textypeuniforms.end(); texTypeIter++) {
      ref->setUniform(texTypeIter->first, texTypeIter->second);
    }

  }

  void SmartShader::ResetUniforms() {
    if (!ref) {
      return;
    }

    // NOTE: Leaving this in here in case this comes back to haunt me.
    //       Basically, revoking shaders was erasing the last shader to update 
    //       and BattleCharacter shader is shared between all entities in the battle scene
    //       So when someone was dying, the white shader replaced their BattleCharacter shader
    //       but it erased all fields in the last BattleCharacter shader to update which wasn't
    //       neccessarily the dying entity's own shader.
    //       This caused some characters to turn black until a successfull RefreshShader() was called.
    //       Update order is not the same as Draw order either! I may need to add this code in here again
    //       but will need address the fact the underlining shader in a SmartShader are shared between many resources
    //       and we need to be careful about overwriting their values in a frame!
    /*iiter iIter = iuniforms.begin();
    fiter fIter = funiforms.begin();
    diter dIter = duniforms.begin();
    farriter farrIter = farruniforms.begin();
    vfiter vfIter = vfuniforms.begin();
    coliter colIter = coluniforms.begin();
    texiter texIter = texuniforms.begin();
    textypeiter texTypeIter = textypeuniforms.begin();

    for (; iIter != iuniforms.end(); iIter++) {
      ref->setUniform(iIter->first, 0);
    }

    for (; fIter != funiforms.end(); fIter++) {
      ref->setUniform(fIter->first, 0.f);
    }

    for (; dIter != duniforms.end(); dIter++) {
      ref->setUniform(dIter->first, 0.f);
    }

    for (; farrIter != farruniforms.end(); farrIter++) {
      std::vector<float> dummyVec;
      dummyVec.reserve(farrIter->second.size());
      ref->setUniformArray(farrIter->first, dummyVec.data(), farrIter->second.size());
    }

    for (; vfIter != vfuniforms.end(); vfIter++) {
      ref->setUniform(vfIter->first, 0.f);
    }

    for (; colIter != coluniforms.end(); colIter++) {
      ref->setUniform(colIter->first, 0.f);
    }

    for (; texIter != texuniforms.end(); texIter++) {
      ref->setUniform(texIter->first, sf::Shader::CurrentTexture);
    }

    for (; texTypeIter != textypeuniforms.end(); texTypeIter++) {
      ref->setUniform(texTypeIter->first, sf::Shader::CurrentTexture);
    }*/

    iuniforms.clear();
    funiforms.clear();
    duniforms.clear();
    farruniforms.clear();
    vfuniforms.clear();
    coluniforms.clear();
    texuniforms.clear();
    textypeuniforms.clear();
  }

  void SmartShader::SetUniform(std::string uniform, float fvalue) {
    funiforms[uniform] = fvalue;
  }

  void SmartShader::SetUniform(std::string uniform, double dvalue)
  {
    duniforms[uniform] = dvalue;
  }

  void SmartShader::SetUniform(std::string uniform, const std::vector<float>& farr)
  {
    farruniforms[uniform] = farr;
  }

  void SmartShader::SetUniform(std::string uniform, int ivalue) {
    iuniforms[uniform] = ivalue;
  }

  void SmartShader::SetUniform(std::string uniform, const sf::Vector2f& vfvalue) {
    vfuniforms[uniform] = vfvalue;
  }

  void SmartShader::SetUniform(std::string uniform, const sf::Color& colvalue)
  {
    coluniforms[uniform] = colvalue;
  }

  void SmartShader::SetUniform(std::string uniform, const std::shared_ptr<sf::Texture>& texvalue)
  {
    texuniforms[uniform] = texvalue;
  }

  void SmartShader::SetUniform(std::string uniform, const sf::Shader::CurrentTextureType& value)
  {
    textypeuniforms[uniform] = value;
  }

  void SmartShader::Reset() {
    ResetUniforms();
    ref = nullptr;
  }
