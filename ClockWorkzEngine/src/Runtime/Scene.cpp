#include "Runtime/Scene.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Physics2D.hpp"
#include <iostream>

using namespace Runtime;
using namespace Gameplay;

Scene::Scene(const std::string& name) : m_name(name) {}
Scene::~Scene() = default;         
const std::string& Scene::name() const { return m_name; }
std::vector<std::unique_ptr<Gameplay::GameObject>>& Scene::objects() { return m_objects; }

bool Scene::load() {
	std::cout << "[Scene] load: " << m_name << "\n";
	// Quick test room
	objects().push_back(GameObject::MakeFloor({ 0,0 }, { 20,12 }, "tex_floor"));
	objects().push_back(GameObject::MakeWall({ -10,0 }, { 1,12 }, "tex_wall"));
	objects().push_back(GameObject::MakeWall({ 10,0 }, { 1,12 }, "tex_wall"));
	objects().push_back(GameObject::MakeWall({ 0,-6 }, { 20,1 }, "tex_wall"));
	objects().push_back(GameObject::MakeWall({ 0, 6 }, { 20,1 }, "tex_wall"));
	objects().push_back(GameObject::MakePlayer({ -6,-2 }, { 1,1 }, "tex_player"));
	objects().push_back(GameObject::MakeEnemy({ 3,0 }, { 1,1 }, 2, "tex_enemy"));
	objects().push_back(GameObject::MakeTrap({ 0,3 }, { 1,1 }, 1, "tex_trap"));
	return true;
}

void Scene::unload() {
	std::cout << "[Scene] unload: " << m_name << "\n";
	m_objects.clear();
}

void Scene::update(double dt) {
	for (auto& o : objects()) if (o->active()) o->update(dt);
	m_physics->step(objects(), dt);
}

void Scene::render() {
	// TODO: submit sprites/meshes to renderer
	for (auto& o : objects()) if (o->active()) o->render();
}
