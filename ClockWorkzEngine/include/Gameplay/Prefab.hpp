
/*
Prefab.cpp by Amirul Syafiq (100%)
*/

#pragma once

#include <Gameplay/GameObject.hpp>
#include <string>
#include <memory>
#include <map>
#include "Math/Vec2.hpp"
#include "Core/json.hpp"

namespace Gameplay
{
	using json = nlohmann::json;

	// New: Component-based blueprint that stores JSON
	struct PrefabData
	{
		std::string name;
		json componentData; // Stores all component definitions
	};

	class PrefabFactory
	{
	public:
		// Singleton access
		static PrefabFactory &get();

		// Save/Load prefabs to/from JSON files
		bool loadPrefabFromFile(const std::string &filepath);
		bool savePrefabToFile(const std::string &prefabName, const std::string &filepath);
		bool loadAllPrefabsFromDirectory(const std::string &directory);

		// Register a prefab from JSON data
		void registerPrefab(const std::string &name, const json &componentData);

		// Create GameObject from prefab
		std::unique_ptr<GameObject> create(const std::string &prefabName);
		std::unique_ptr<GameObject> createAt(const std::string &prefabName, glm::vec2 position);

		// Create GameObject with component overrides
		std::unique_ptr<GameObject> createWithOverrides(
				const std::string &prefabName,
				const json &overrides);

		// Check if prefab exists
		bool hasPrefab(const std::string &name) const;

		// Get list of all prefab names
		std::vector<std::string> getPrefabNames() const;

	private:
		std::map<std::string, PrefabData> prefabs_;
	};

	// Helper function to set up built-in prefabs (optional, for backwards compatibility)
	void setupGameBlueprints();

} // namespace Gameplay