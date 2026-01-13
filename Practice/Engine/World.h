#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include "EntityId.h"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "CameraComponent.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
#include "CollisionEvents.h"
#include "AudioSourceComponent.h"
#include "LightComponent.h"
#include "UIElementComponent.h"
#include "ScriptComponent.h"

class Behaviour;

class World
{
public:
    World() = default;

    EntityId CreateEntity(const std::string& name = "");
    bool IsAlive(EntityId e) const;

	// Name API
    EntityId FindByName(const std::string& name) const;
    const std::string& GetName(EntityId e) const;

    uint32_t AliveCount() const { return m_aliveCount; }

    // --- Transform API ---
    void AddTransform(EntityId e, const TransformComponent& init = TransformComponent{});
    TransformComponent& GetPendingTransform(EntityId e);
    bool HasTransform(EntityId e) const;
    TransformComponent& GetTransform(EntityId e);
    const TransformComponent& GetTransform(EntityId e) const;
    bool IsTransformPending(EntityId e) const;
    bool HasOrPendingTransform(EntityId e) const;

    // parent/child
    void SetParent(EntityId child, EntityId newParent);
	inline void Detach(EntityId child) { SetParent(child, EntityId::Invalid()); }
    bool IsDescendant(EntityId node, EntityId potentialAncestor) const;

	// Mark the given entity and its subtree as dirty (world matrix needs update).
    void UpdateTransforms();

    // Force-refresh the world matrix for the given entity (and its subtree) immediately.
    // Useful for scripts that need up-to-date world transforms before the global UpdateTransforms() pass.
    void UpdateTransformNow(EntityId e);

	// Frame management
    void BeginFrame();
    bool TransformsUpdatedThisFrame() const;

private:
    struct Slot
    {
        uint32_t generation = 0;
        bool alive = false;
        std::string name;
    };

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeList;
    std::unordered_map<std::string, EntityId> m_nameToEntity;

    uint32_t m_aliveCount = 0;

	std::vector<EntityId> m_pendingDestroy;

    // Structural change queues (applied only in FlushStructuralChanges)
    struct PendingScriptAdd { EntityId e; std::unique_ptr<Behaviour> b; bool enabled; };
    std::vector<PendingScriptAdd> m_pendingScriptAdd;

    std::vector<std::pair<EntityId, TransformComponent>> m_pendingAddTransform;
    std::vector<std::pair<EntityId, MeshComponent>> m_pendingAddMesh;
    std::vector<std::pair<EntityId, MaterialComponent>> m_pendingAddMaterial;
    std::vector<std::pair<EntityId, CameraComponent>> m_pendingAddCamera;

    std::vector<std::pair<EntityId, RigidBodyComponent>> m_pendingAddRigidBody;
    std::vector<std::pair<EntityId, ColliderComponent>> m_pendingAddCollider;

    std::vector<std::pair<EntityId, AudioSourceComponent>> m_pendingAddAudioSource;
    std::vector<std::pair<EntityId, LightComponent>> m_pendingAddLight;
    std::vector<std::pair<EntityId, UIElementComponent>> m_pendingAddUIElement;

    // Remove도 public API로 열려있는 것들만 큐잉
    std::vector<EntityId> m_pendingRemoveAudioSource;
    std::vector<EntityId> m_pendingRemoveLight;
    std::vector<EntityId> m_pendingRemoveUIElement;
    std::vector<EntityId> m_pendingRemoveRigidBody;
    std::vector<EntityId> m_pendingRemoveCollider;

    uint64_t m_frameIndex = 0;
    uint64_t m_transformUpdatedFrame = UINT64_MAX;

private:
    void DestroyEntity(EntityId e);

    void RemoveNameMapping(EntityId e);

    // --- Transform Storage (sparse set) ---
    static constexpr uint32_t InvalidDenseIndex = 0xFFFFFFFFu;

    std::vector<uint32_t> m_transformSparse;        // [entity.index] -> denseIndex or Invalid
    std::vector<EntityId> m_transformDenseEntities; // denseIndex -> EntityId
    std::vector<TransformComponent> m_transforms;   // denseIndex -> TransformComponent

    void EnsureTransformSparseSize(uint32_t entityIndex);

    void RemoveTransform(EntityId e);

    void MarkDirtyRecursive(EntityId e);

    DirectX::XMMATRIX LocalMatrix(const TransformComponent& t) const;
    void UpdateWorldRecursive(EntityId e, const DirectX::XMMATRIX& parentWorld);

    // --- Mesh Storage (sparse set) ---
    std::vector<uint32_t> m_meshSparse;
    std::vector<EntityId> m_meshDenseEntities;
    std::vector<MeshComponent> m_meshes;
    void EnsureMeshSparseSize(uint32_t entityIndex);
    void RemoveMesh(EntityId e);

    // --- Material Storage (sparse set) ---
    std::vector<uint32_t> m_materialSparse;
    std::vector<EntityId> m_materialDenseEntities;
    std::vector<MaterialComponent> m_materials;
    void EnsureMaterialSparseSize(uint32_t entityIndex);
    void RemoveMaterial(EntityId e);

    // --- Camera Storage (sparse set) ---
    std::vector<uint32_t> m_cameraSparse;
    std::vector<EntityId> m_cameraDenseEntities;
    std::vector<CameraComponent> m_cameras;
    void EnsureCameraSparseSize(uint32_t entityIndex);
    void RemoveCamera(EntityId e);

    // Rigidbody storage (dense/sparse)
    std::vector<RigidBodyComponent> m_rigidBodies;
    std::vector<EntityId> m_rigidBodyDenseEntities;
    std::vector<uint32_t> m_rigidBodySparse;

    // Collider storage
    std::vector<ColliderComponent> m_colliders;
    std::vector<EntityId> m_colliderDenseEntities;
    std::vector<uint32_t> m_colliderSparse;

	// Collision Events
    std::vector<CollisionEvent> m_collisionEvents;

    // AudioSource storage (dense/sparse)
    std::vector<AudioSourceComponent> m_audioSources;
    std::vector<EntityId>            m_audioSourceDenseEntities;
    std::vector<uint32_t>            m_audioSourceSparse;

    void EnsureAudioSourceSparseSize(uint32_t entityIndex);


    // Light storage (dense/sparse)
    std::vector<LightComponent>     m_lights;
    std::vector<EntityId>           m_lightDenseEntities;
    std::vector<uint32_t>           m_lightSparse;

    void EnsureLightSparseSize(uint32_t entityIndex);

    // UIElement storage (dense/sparse)
    std::vector<UIElementComponent> m_uiElements;
    std::vector<EntityId>           m_uiElementDenseEntities;
    std::vector<uint32_t>           m_uiElementSparse;

    void EnsureUIElementSparseSize(uint32_t entityIndex);

	// Script storage (dense/sparse)
    std::vector<ScriptComponent>    m_scripts;
    std::vector<EntityId>           m_scriptDenseEntities;
    std::vector<uint32_t>           m_scriptSparse;
    void EnsureScriptSparseSize(uint32_t entityIndex);
	void RemoveScript(EntityId e, Behaviour* which);

    // Add Immediate
    void AddTransform_Immediate(EntityId e, const TransformComponent& comp);
    void AddMesh_Immediate(EntityId e, const MeshComponent& comp);
    void AddMaterial_Immediate(EntityId e, const MaterialComponent& comp);
    void AddCamera_Immediate(EntityId e, const CameraComponent& c);
    void AddRigidBody_Immediate(EntityId e, const RigidBodyComponent& comp);
    void AddCollider_Immediate(EntityId e, const ColliderComponent& comp);
    void AddAudioSource_Immediate(EntityId e, const AudioSourceComponent& c);
    void AddLight_Immediate(EntityId e, const LightComponent& c);
    void AddUIElement_Immediate(EntityId e, const UIElementComponent& c);

    // Remove Immediate
    void RemoveAudioSource_Immediate(EntityId e);
    void RemoveLight_Immediate(EntityId e);
    void RemoveUIElement_Immediate(EntityId e);
    void RemoveRigidBody_Immediate(EntityId e);
    void RemoveCollider_Immediate(EntityId e);

public:
    // --- Transform Public API ---
    DirectX::XMFLOAT3 GetLocalPosition(EntityId e) const;
    void SetLocalPosition(EntityId e, const DirectX::XMFLOAT3& p);

    DirectX::XMFLOAT4 GetLocalRotation(EntityId e) const; // quat
    void SetLocalRotation(EntityId e, const DirectX::XMFLOAT4& q);

    DirectX::XMFLOAT3 GetLocalRotationEuler(EntityId e) const;     // (pitch, yaw, roll) radians
    void SetLocalRotationEuler(EntityId e, const DirectX::XMFLOAT3& eulerRad);

    DirectX::XMFLOAT3 GetLocalScale(EntityId e) const;
    void SetLocalScale(EntityId e, const DirectX::XMFLOAT3& s);

    void TranslateLocal(EntityId e, const DirectX::XMFLOAT3& delta);

    // World-space
    DirectX::XMFLOAT4X4 GetWorldMatrix(EntityId e) const;
    DirectX::XMFLOAT3  GetWorldPosition(EntityId e) const;

    // --- Mesh API ---
    void AddMesh(EntityId e, const MeshComponent& comp);
    bool HasMesh(EntityId e) const;
    MeshComponent& GetMesh(EntityId e);
    const MeshComponent& GetMesh(EntityId e) const;

    // --- Material API ---
    void AddMaterial(EntityId e, const MaterialComponent& m);
    bool HasMaterial(EntityId e) const;
    MaterialComponent& GetMaterial(EntityId e);
    const MaterialComponent& GetMaterial(EntityId e) const;

    // --- Camera API ---
    void AddCamera(EntityId e, const CameraComponent& c = CameraComponent{});
    bool HasCamera(EntityId e) const;
    CameraComponent& GetCamera(EntityId e);
    const CameraComponent& GetCamera(EntityId e) const;

	// Find the first active camera in the world (or Invalid if none)
    EntityId FindActiveCamera() const;

    // --- AudioSource API ---
    void AddAudioSource(EntityId e, const AudioSourceComponent& c);
    bool HasAudioSource(EntityId e) const;
    AudioSourceComponent& GetAudioSource(EntityId e);
    const AudioSourceComponent& GetAudioSource(EntityId e) const;
    void RemoveAudioSource(EntityId e);

    const std::vector<EntityId>& GetAudioSourceEntities() const { return m_audioSourceDenseEntities; }

    // --- Light API ---
    void AddLight(EntityId e, const LightComponent& c);
    bool HasLight(EntityId e) const;
    LightComponent& GetLight(EntityId e);
    const LightComponent& GetLight(EntityId e) const;
    void RemoveLight(EntityId e);

    const std::vector<EntityId>& GetLightEntities() const { return m_lightDenseEntities; }
    const std::vector<LightComponent>& GetLightsDense() const;

    // --- UIElement API ---
    void AddUIElement(EntityId e, const UIElementComponent& c);
    bool HasUIElement(EntityId e) const;
    UIElementComponent& GetUIElement(EntityId e);
    const UIElementComponent& GetUIElement(EntityId e) const;
    void RemoveUIElement(EntityId e);

    const std::vector<EntityId>& GetUIElementEntities() const { return m_uiElementDenseEntities; }

	// --- Transform Entities ---
    const std::vector<EntityId>& GetTransformEntities() const { return m_transformDenseEntities; }

	// --- Destroy API ---
    void RequestDestroy(EntityId e);
    void FlushDestroy();

    void FlushStructuralChanges();

    // Rigidbody
	void EnsureRigidBodySparseSize(uint32_t entityIndex);
    void AddRigidBody(EntityId e, const RigidBodyComponent& rb);
    bool HasRigidBody(EntityId e) const;
    RigidBodyComponent& GetRigidBody(EntityId e);
    const RigidBodyComponent& GetRigidBody(EntityId e) const;
	void RemoveRigidBody(EntityId e);

    // Collider
	void EnsureColliderSparseSize(uint32_t entityIndex);
    void AddCollider(EntityId e, const ColliderComponent& c);
    bool HasCollider(EntityId e) const;
    ColliderComponent& GetCollider(EntityId e);
    const ColliderComponent& GetCollider(EntityId e) const;
	void RemoveCollider(EntityId e);

	// Collider Entities
    const std::vector<EntityId>& GetColliderEntities() const { return m_colliderDenseEntities; }

    // Collision Events
    void PushCollisionEvent(const CollisionEvent& ev);
    void DrainCollisionEvents(std::vector<CollisionEvent>& out);

	// --- Script API ---
    ScriptComponent& EnsureScriptComponent(EntityId e);
    void AddScript(EntityId e, std::unique_ptr<Behaviour> b, bool enabled = true);
    bool HasScript(EntityId e) const;
    ScriptComponent& GetScript(EntityId e);
    const std::vector<EntityId>& GetScriptEntities() const { return m_scriptDenseEntities; }
    void FlushScripts();
    void RemoveScriptComponent(EntityId e);
    bool IsPendingDestroy(EntityId e) const;
};
