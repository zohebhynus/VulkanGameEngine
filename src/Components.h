#pragma once

#include <cstdint>
#include <bitset>
#include <queue>
#include <array>
#include <cassert>
#include <unordered_map>
#include <memory>
#include <set>
#include <string>

#include <glm/glm/gtc/matrix_transform.hpp>

#include "Model.h"
#include "Graphics/Texture.h"



// Entity

using Entity = uint32_t;
const Entity MAX_ENTITIES = 5000;

// Entity

// Component ID

using ComponentID = uint8_t;
const ComponentID MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

// Component ID


// Entity Manager

class EntityManager
{
public:
	EntityManager()
	{
		for (Entity entity = 0; entity < MAX_ENTITIES; entity++)
		{
			m_EntityPool.push(entity);
		}
	}

	Entity CreateEntity()
	{
		assert(m_ActiveEntityCount < MAX_ENTITIES && "Max Entity count has been reached");

		Entity id = m_EntityPool.front();
		m_EntityPool.pop();
		m_ActiveEntityCount++;

		return id;
	}

	void DestroyEntity(Entity entity)
	{
		assert(entity < MAX_ENTITIES && "Entity ID out of scope. Not valid entity");

		m_EntitySignatures[entity].reset();
		m_EntityPool.push(entity);
		m_ActiveEntityCount--;
	}

	void SetSignature(Entity entity, Signature signature)
	{
		assert(entity < MAX_ENTITIES && "Entity ID out of scope. Not valid entity");

		m_EntitySignatures[entity] = signature;
	}

	Signature GetSignature(Entity entity)
	{
		assert(entity < MAX_ENTITIES && "Entity ID out of scope. Not valid entity");

		return m_EntitySignatures[entity];
	}

private:
	std::queue<Entity> m_EntityPool{};

	std::array<Signature, MAX_ENTITIES> m_EntitySignatures{};

	uint32_t m_ActiveEntityCount{};
};

// Entity Manager

// ComponentArray

class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void EntityDestroyed(Entity entity) = 0;
};

template<typename T>
class ComponentArray : public  IComponentArray
{
public:
	void InsertData(Entity entity, T component)
	{
		assert(m_EntityToIndexMap.find(entity) == m_EntityToIndexMap.end() && "Entity already has this component!");

		m_ComponentArray[m_ArraySize] = component;
		m_EntityToIndexMap[entity] = m_ArraySize;
		m_IndexToEntityMap[m_ArraySize] = entity;

		m_ArraySize++;
	}

	void DeleteData(Entity entity)
	{
		assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() && "Entity does not contain this component!");

		size_t indexToDel = m_EntityToIndexMap[entity];
		size_t indexOfLastElement = m_ArraySize - 1;

		m_ComponentArray[indexToDel] = m_ComponentArray[indexOfLastElement];

		Entity lastIndexEntity = m_IndexToEntityMap[indexOfLastElement];
		m_EntityToIndexMap[lastIndexEntity] = indexToDel;
		m_IndexToEntityMap[indexToDel] = lastIndexEntity;

		m_EntityToIndexMap.erase(entity);
		m_IndexToEntityMap.erase(indexOfLastElement);

		m_ArraySize--;
	}

	T& GetData(Entity entity)
	{
		assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() && "Entity does not contain this component!");

		return m_ComponentArray[m_EntityToIndexMap[entity]];
	}

	void EntityDestroyed(Entity entity) override
	{
		if (m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end())
		{
			DeleteData(entity);
		}
	}


private:
	std::array<T, MAX_ENTITIES> m_ComponentArray;

	std::unordered_map<Entity, size_t> m_EntityToIndexMap;
	std::unordered_map<size_t, Entity> m_IndexToEntityMap;

	size_t m_ArraySize{};
};

// ComponentArray


// Component Manager

class ComponentManager
{
public:
	template<typename T>
	void RegisterComponent()
	{
		const char* componentName = typeid(T).name();
		assert(m_ComponentIDs.find(componentName) == m_ComponentIDs.end() && "Component Already Registered!");

		m_ComponentIDs.insert({ componentName, m_NextComponentID });
		m_ComponentArray.insert({componentName, std::make_shared<ComponentArray<T>>()});

		m_NextComponentID++;
	}

	template<typename T>
	ComponentID GetComponentID()
	{
		const char* componentName = typeid(T).name();
		assert(m_ComponentIDs.find(componentName) != m_ComponentIDs.end() && "Component Not Registered!");

		return m_ComponentIDs[componentName];
	}

	template<typename T>
	void AddComponent(Entity entity, T component)
	{
		GetComponentArray<T>()->InsertData(entity, component);
	}

	template<typename T>
	void DeleteComponent(Entity entity)
	{
		GetComponentArray<T>()->DeleteData(entity);
	}

	template<typename T>
	T& GetComponent(Entity entity)
	{
		return GetComponentArray<T>()->GetData(entity);
	}

	void EntityDestroyed(Entity entity)
	{
		for (auto& kv : m_ComponentArray)
		{
			auto& componentArray = kv.second;

			componentArray->EntityDestroyed(entity);
		}
	}


private:
	std::unordered_map<const char*, ComponentID> m_ComponentIDs{};
	std::unordered_map<const char*, std::shared_ptr<IComponentArray>> m_ComponentArray{};

	ComponentID m_NextComponentID {};

	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray()
	{
		const char* componentName = typeid(T).name();
		assert(m_ComponentIDs.find(componentName) != m_ComponentIDs.end() && "Component Not Registered!");

		return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArray[componentName]);
	}
};

// Component Manager

// System

class System
{
public:
	std::set<Entity> m_Entities;
};

class SystemManager
{
public:
	template<typename T, typename... Args>
	std::shared_ptr<T> RegisterSystem(Args&&... args)
	{
		const char* systemName = typeid(T).name();
		assert(m_Systems.find(systemName) == m_Systems.end() && "System already Registered");

		auto system = std::make_shared<T>(std::forward<Args>(args)...);
		m_Systems.insert({systemName, system});

		return system;
	}

	template<typename T>
	void SetSignature(Signature signature)
	{
		const char* systemName = typeid(T).name();
		assert(m_Systems.find(systemName) != m_Systems.end() && "System not Registered");

		m_SystemSignatures.insert({ systemName, signature });
	}

	void EntityDestroyed(Entity entity)
	{
		for (auto& kv : m_Systems)
		{
			auto& system = kv.second;
			system->m_Entities.erase(entity);
		}
	}

	void EntitySignatureChanged(Entity entity, Signature newSignature)
	{
		for (auto& kv : m_Systems)
		{
			auto& systemName = kv.first;
			auto& system = kv.second;

			if ((newSignature & m_SystemSignatures[systemName]) == m_SystemSignatures[kv.first])
			{
				system->m_Entities.insert(entity);
			}
			else
			{
				system->m_Entities.erase(entity);
			}
		}
	}

private:
	std::unordered_map<const char*, Signature> m_SystemSignatures{};
	std::unordered_map<const char*, std::shared_ptr<System>> m_Systems{};

};

// System

// Coordinator

class Coordinator
{
public:
	Coordinator()
	{
		m_EntityManager = std::make_unique<EntityManager>();
		m_ComponentManager = std::make_unique<ComponentManager>();
		m_SystemManager = std::make_unique<SystemManager>();
	}
	~Coordinator() {};

	// Entity Functions
	Entity CreateEntity()
	{
		return m_EntityManager->CreateEntity();
	}

	Entity DestroyeEntity(Entity entity)
	{
		m_EntityManager->DestroyEntity(entity);
		m_ComponentManager->EntityDestroyed(entity);
		m_SystemManager->EntityDestroyed(entity);
	}

	// Component Functions
	template<typename T>
	void RegisterComponent()
	{
		m_ComponentManager->RegisterComponent<T>();
	}

	template<typename T>
	void AddComponent(Entity entity, T component)
	{
		m_ComponentManager->AddComponent(entity, component);

		auto signature = m_EntityManager->GetSignature(entity);
		signature.set(m_ComponentManager->GetComponentID<T>(), true);
		m_EntityManager->SetSignature(entity, signature);
		m_SystemManager->EntitySignatureChanged(entity, signature);
	}

	template<typename T>
	void RemoveComponent(Entity entity)
	{
		m_ComponentManager->DeleteComponent(entity);
		auto signature = m_EntityManager->GetSignature(entity);
		signature.set(m_ComponentManager->GetComponentID<T>(), false);
		m_EntityManager->SetSignature(entity, signature);
		m_SystemManager->EntitySignatureChanged(entity, signature);
	}

	template<typename T>
	T& GetComponent(Entity entity)
	{
		return m_ComponentManager->GetComponent<T>(entity);
	}

	template<typename T>
	ComponentID GetComponentID()
	{
		return m_ComponentManager->GetComponentID<T>();
	}

	// System Functions
	template<typename T, typename... Args>
	std::shared_ptr<T> RegisterSystem(Args&&... args)
	{
		return m_SystemManager->RegisterSystem<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	void SetSystemSignature(Signature signature)
	{
		m_SystemManager->SetSignature<T>(signature);
	}

private:
	std::unique_ptr<EntityManager> m_EntityManager;
	std::unique_ptr<ComponentManager> m_ComponentManager;
	std::unique_ptr<SystemManager> m_SystemManager;
};

// Coordinator


// Components

struct ModelComponent
{
	std::shared_ptr<Model> model;
};

struct ECSTransformComponent
{
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};

struct LightObjectComponent
{
	bool isPoint = true;
	glm::vec3 lightColor;

	float lightIntensity;
	float lightObjectRadius;

	glm::vec3 lightDirection;
	float cutOff;
	float outerCutOff;

	static LightObjectComponent PointLight(glm::vec3 color, float lightIntensity, float lightObjectRadius)
	{
		LightObjectComponent obj{};
		obj.isPoint = true;
		obj.lightColor = color;
		obj.lightIntensity = lightIntensity;
		obj.lightObjectRadius = lightObjectRadius;
		return obj;
	}

	static LightObjectComponent SpotLight(glm::vec3 color, float lightIntensity, float lightObjectRadius, glm::vec3 spotDirection, float cutOff, float outerCutOff)
	{
		LightObjectComponent obj{};
		obj.isPoint = false;
		obj.lightColor = color;
		obj.lightIntensity = lightIntensity;
		obj.lightObjectRadius = lightObjectRadius;
		obj.lightDirection = spotDirection;
		obj.cutOff = cutOff;
		obj.outerCutOff = outerCutOff;
		return obj;
	}
};

struct CameraObjectComponent
{
	enum CameraType
	{
		Orthographic,
		Perspective
	};

	CameraType cameraType = CameraType::Perspective;
	float near;
	float far;

	// Perspective Data
	float fieldOfView;
	float aspectRatio;

	// Orthographic Data
	float left;
	float right;
	float top;
	float bottom;
};

// Components

// Helper Function

static glm::mat4 modelMatrix(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
	const float c3 = glm::cos(rotation.z);
	const float s3 = glm::sin(rotation.z);
	const float c2 = glm::cos(rotation.x);
	const float s2 = glm::sin(rotation.x);
	const float c1 = glm::cos(rotation.y);
	const float s1 = glm::sin(rotation.y);
	return glm::mat4{
		{
			scale.x * (c1 * c3 + s1 * s2 * s3),
			scale.x * (c2 * s3),
			scale.x * (c1 * s2 * s3 - c3 * s1),
			0.0f,
		},
		{
			scale.y * (c3 * s1 * s2 - c1 * s3),
			scale.y * (c2 * c3),
			scale.y * (c1 * c3 * s2 + s1 * s3),
			0.0f,
		},
		{
			scale.z * (c2 * s1),
			scale.z * (-s2),
			scale.z * (c1 * c2),
			0.0f,
		},
		{translation.x, translation.y, translation.z, 1.0f} };
}

static glm::mat3 normalMatrix(glm::vec3 rotation, glm::vec3 scale)
{
	const float c3 = glm::cos(rotation.z);
	const float s3 = glm::sin(rotation.z);
	const float c2 = glm::cos(rotation.x);
	const float s2 = glm::sin(rotation.x);
	const float c1 = glm::cos(rotation.y);
	const float s1 = glm::sin(rotation.y);

	glm::vec3 inverseScale = 1.0f / scale;

	return glm::mat3{
		{
			inverseScale.x * (c1 * c3 + s1 * s2 * s3),
			inverseScale.x * (c2 * s3),
			inverseScale.x * (c1 * s2 * s3 - c3 * s1),
		},
		{
			inverseScale.y * (c3 * s1 * s2 - c1 * s3),
			inverseScale.y * (c2 * c3),
			inverseScale.y * (c1 * c3 * s2 + s1 * s3),
		},
		{
			inverseScale.z * (c2 * s1),
			inverseScale.z * (-s2),
			inverseScale.z * (c1 * c2),
		},
	};
}

// Helper Function