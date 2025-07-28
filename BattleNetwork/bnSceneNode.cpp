#include "bnSceneNode.h"
#include <cmath>

SceneNode::SceneNode() :
show(true), layer(0), parent(nullptr), childNodes() {
}

SceneNode::~SceneNode() {

}

void SceneNode::SetLayer(int layer) {
  SceneNode::layer = layer;
}

const int SceneNode::GetLayer() const {
  return layer;
}

void SceneNode::Hide() {
  show = false;
}

void SceneNode::Reveal() {
  show = true;
}

const bool SceneNode::IsHidden() const {
  return !show;
}

const bool SceneNode::IsVisible() const {
  return show;
}

void SceneNode::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  if (!show) return;

  std::sort(childNodes.begin(), childNodes.end(), [](std::shared_ptr<SceneNode>& a, std::shared_ptr<SceneNode>& b) { return (a->GetLayer() > b->GetLayer()); });

  // draw its children
  for (auto& childNode : childNodes) {
    auto childStates = states;

    if (!childNode->useParentShader) {
      childStates.shader = nullptr;
    }

    childNode->draw(target, childStates);
  }
}

void SceneNode::AddNode(std::shared_ptr<SceneNode> child) { 
  if (child == nullptr) return;  child->parent = this; childNodes.push_back(child); 
}

void SceneNode::RemoveNode(std::shared_ptr<SceneNode> find) {
  RemoveNode(find.get());
}

void SceneNode::RemoveNode(SceneNode* find) {
  if (find == nullptr) return;

  auto iter = std::remove_if(childNodes.begin(), childNodes.end(), [find](std::shared_ptr<SceneNode>& in) { return in.get() == find; });

  if (iter != childNodes.end()) {
    // something is causing *iter to be zero here?
    find->parent = nullptr;
  }

  childNodes.erase(iter, childNodes.end());
}

void SceneNode::EnableParentShader(bool use)
{
  useParentShader = use;
}

const bool SceneNode::IsUsingParentShader() const
{
  return useParentShader;
}

std::vector<std::shared_ptr<SceneNode>>& SceneNode::GetChildNodes() const
{
  return childNodes;
}

std::set<std::shared_ptr<SceneNode>> SceneNode::GetChildNodesWithTag(const std::vector<std::string>& query)
{
  std::set<std::shared_ptr<SceneNode>> results;

  for (auto& q : query) {
    for (auto& n : childNodes) {
      if (n->HasTag(q)) {
        results.insert(n);
      }
    }
  }

  return results;
}

SceneNode* SceneNode::GetParent() {
  return parent;
}

void SceneNode::AddTags(std::vector<std::string> tags)
{
  for (auto& t : tags) {
    this->tags.insert(t);
  }
}

void SceneNode::RemoveTags(std::vector<std::string> tags)
{
  for (auto& t : tags) {
    this->tags.insert(t);
  }
}

const bool SceneNode::HasTag(const std::string& name)
{
  for (auto& t : tags) {
    if (t == name) return true;
  }

  return false;
}
