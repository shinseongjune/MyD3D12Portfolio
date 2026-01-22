#include "World.h"
#include <cassert>
#include <algorithm>
#include <DirectXMath.h>
#include <unordered_set>
#if defined(_DEBUG)
#include <windows.h>
#endif
#include "Behaviour.h"

using namespace DirectX;


EntityId World::CreateEntity(const std::string& name)
{
    uint32_t index = 0;

    if (!m_freeList.empty())
    {
        index = m_freeList.back();
        m_freeList.pop_back();

        Slot& s = m_slots[index];
        s.alive = true;
        // generation은 Destroy에서 올려둔 값 유지
        s.name = name;
    }
    else
    {
        index = (uint32_t)m_slots.size();
        Slot s{};
        s.generation = 1;   // 0은 Invalid과 헷갈릴 여지가 있어 보통 1부터 시작
        s.alive = true;
        s.name = name;
        m_slots.push_back(std::move(s));
    }

    EntityId e{ index, m_slots[index].generation };

    EnsureTransformSparseSize(index);
    EnsureMeshSparseSize(index);
    EnsureMaterialSparseSize(index);
    EnsureCameraSparseSize(index);
    EnsureLightSparseSize(index);
    EnsureRigidBodySparseSize(index);
    EnsureColliderSparseSize(index);
    EnsureUIElementSparseSize(index);
    EnsureAudioSourceSparseSize(index);
    EnsureScriptSparseSize(index);

    if (!name.empty())
        m_nameToEntity[name] = e;

    ++m_aliveCount;
    return e;
}

bool World::IsPendingDestroy(EntityId e) const
{
    return std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), e) != m_pendingDestroy.end();
}

void World::DestroyEntity(EntityId e)
{
    if (!IsAlive(e)) return;

    if (HasScript(e))
    {
        RemoveScriptComponent(e);
    }

    RemoveNameMapping(e);
    RemoveTransform(e);
    RemoveMesh(e);
    RemoveMaterial(e);
    RemoveCamera(e);
    if (HasUIElement(e))   RemoveUIElement_Immediate(e);
    if (HasCollider(e))    RemoveCollider_Immediate(e);
    if (HasRigidBody(e))   RemoveRigidBody_Immediate(e);
    if (HasAudioSource(e)) RemoveAudioSource_Immediate(e);
    if (HasLight(e))       RemoveLight_Immediate(e);

    Slot& s = m_slots[e.index];
    s.alive = false;
    s.name.clear();

    // generation 증가: 예전 핸들 무효화(유령 참조 방지)
    ++s.generation;

    // 재사용 리스트에 넣기
    m_freeList.push_back(e.index);

    if (m_aliveCount > 0) --m_aliveCount;
}

bool World::IsAlive(EntityId e) const
{
    if (!e.IsValid()) return false;
    if (e.index >= m_slots.size()) return false;

    const Slot& s = m_slots[e.index];
    return s.alive && s.generation == e.generation;
}

EntityId World::FindByName(const std::string& name) const
{
    auto it = m_nameToEntity.find(name);
    if (it == m_nameToEntity.end())
        return EntityId::Invalid();

    // 매핑이 오래됐을 수도 있으니 생존 확인
    if (!IsAlive(it->second))
        return EntityId::Invalid();

    return it->second;
}

const std::string& World::GetName(EntityId e) const
{
    static const std::string kEmpty;
    if (!IsAlive(e)) return kEmpty;
    return m_slots[e.index].name;
}

void World::RemoveNameMapping(EntityId e)
{
    // 이름이 있다면 map에서 제거
    if (!e.IsValid() || e.index >= m_slots.size()) return;
    const std::string& name = m_slots[e.index].name;
    if (name.empty()) return;

    auto it = m_nameToEntity.find(name);
    if (it != m_nameToEntity.end() && it->second == e)
        m_nameToEntity.erase(it);
}

// --- Transform API 구현 ---

void World::EnsureTransformSparseSize(uint32_t entityIndex)
{
    if (m_transformSparse.size() <= entityIndex)
        m_transformSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

// --- Mesh Storage ---
void World::EnsureMeshSparseSize(uint32_t entityIndex)
{
    if (m_meshSparse.size() <= entityIndex)
        m_meshSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddMesh(EntityId e, const MeshComponent& comp)
{
    if (!IsAlive(e)) return;
    m_pendingAddMesh.push_back({ e, comp });
}

void World::AddMesh_Immediate(EntityId e, const MeshComponent& comp)
{
    if (!IsAlive(e)) return;
    EnsureMeshSparseSize(e.index);

    uint32_t di = m_meshSparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_meshDenseEntities.size())
        {
            m_meshSparse[e.index] = InvalidDenseIndex; // 깨진 sparse 복구 후 새로 추가로 진행
        }
        else
        {
            // stale/정상 모두 소유자 갱신
            m_meshDenseEntities[di] = e;

            // 기존 정책 유지: draws append
            auto& dst = m_meshes[di].draws;
            dst.insert(dst.end(), comp.draws.begin(), comp.draws.end());
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_meshes.size();
    m_meshSparse[e.index] = denseIndex;
    m_meshDenseEntities.push_back(e);
    m_meshes.push_back(comp);
}


bool World::HasMesh(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_meshSparse.size()) return false;

    const uint32_t di = m_meshSparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_meshDenseEntities.size()) return false;
    return m_meshDenseEntities[di] == e;
}

MeshComponent& World::GetMesh(EntityId e)
{
#if defined(_DEBUG)
    assert(HasMesh(e));
    assert(!m_meshes[m_meshSparse[e.index]].draws.empty());
    assert(m_meshes[m_meshSparse[e.index]].draws[0].mesh.IsValid());
#endif
    return m_meshes[m_meshSparse[e.index]];
}

const MeshComponent& World::GetMesh(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasMesh(e));
    assert(!m_meshes[m_meshSparse[e.index]].draws.empty());
    assert(m_meshes[m_meshSparse[e.index]].draws[0].mesh.IsValid());
#endif
    return m_meshes[m_meshSparse[e.index]];
}

void World::RemoveMesh(EntityId e)
{
    if (!HasMesh(e)) return;
    const uint32_t denseIndex = m_meshSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_meshes.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_meshes[denseIndex] = std::move(m_meshes[lastIndex]);
        m_meshDenseEntities[denseIndex] = m_meshDenseEntities[lastIndex];
        EntityId movedEntity = m_meshDenseEntities[denseIndex];
        m_meshSparse[movedEntity.index] = denseIndex;
    }

    m_meshes.pop_back();
    m_meshDenseEntities.pop_back();
    m_meshSparse[e.index] = InvalidDenseIndex;
}

// --- Material Storage ---
void World::EnsureMaterialSparseSize(uint32_t entityIndex)
{
    if (m_materialSparse.size() <= entityIndex)
        m_materialSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddMaterial(EntityId e, const MaterialComponent& comp)
{
    if (!IsAlive(e)) return;
    m_pendingAddMaterial.push_back({ e, comp });
}

bool World::HasMaterial(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_materialSparse.size()) return false;

	const uint32_t di = m_materialSparse[e.index];
	if (di == InvalidDenseIndex) return false;
	if (di >= m_materialDenseEntities.size()) return false;
    return m_materialDenseEntities[di] == e;
}

MaterialComponent& World::GetMaterial(EntityId e)
{
#if defined(_DEBUG)
    assert(HasMaterial(e));
#endif
    uint32_t denseIndex = m_materialSparse[e.index];
    return m_materials[denseIndex];
}

const MaterialComponent& World::GetMaterial(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasMaterial(e));
#endif
    const uint32_t denseIndex = m_materialSparse[e.index];
    return m_materials[denseIndex];
}

void World::RemoveMaterial(EntityId e)
{
    if (!HasMaterial(e)) return;
    const uint32_t denseIndex = m_materialSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_materials.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_materials[denseIndex] = std::move(m_materials[lastIndex]);
        m_materialDenseEntities[denseIndex] = m_materialDenseEntities[lastIndex];
        EntityId movedEntity = m_materialDenseEntities[denseIndex];
        m_materialSparse[movedEntity.index] = denseIndex;
    }

    m_materials.pop_back();
    m_materialDenseEntities.pop_back();
    m_materialSparse[e.index] = InvalidDenseIndex;
}

// --- Camera Storage (뼈대) ---
void World::EnsureCameraSparseSize(uint32_t entityIndex)
{
    if (m_cameraSparse.size() <= entityIndex)
        m_cameraSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddCamera(EntityId e, const CameraComponent& c)
{
    if (!IsAlive(e)) return;
    m_pendingAddCamera.push_back({ e, c });
}

bool World::HasCamera(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_cameraSparse.size()) return false;

	const uint32_t di = m_cameraSparse[e.index];
	if (di == InvalidDenseIndex) return false;
	if (di >= m_cameraDenseEntities.size()) return false;
	return m_cameraDenseEntities[di] == e;
}

CameraComponent& World::GetCamera(EntityId e)
{
#if defined(_DEBUG)
    assert(HasCamera(e));
#endif
    uint32_t denseIndex = m_cameraSparse[e.index];
    return m_cameras[denseIndex];
}

const CameraComponent& World::GetCamera(EntityId e) const
{
#if defined(_DEBUG)
	assert(HasCamera(e));
#endif
    const uint32_t denseIndex = m_cameraSparse[e.index];
    return m_cameras[denseIndex];
}

void World::RemoveCamera(EntityId e)
{
    if (!HasCamera(e)) return;
    const uint32_t denseIndex = m_cameraSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_cameras.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_cameras[denseIndex] = std::move(m_cameras[lastIndex]);
        m_cameraDenseEntities[denseIndex] = m_cameraDenseEntities[lastIndex];
        EntityId movedEntity = m_cameraDenseEntities[denseIndex];
        m_cameraSparse[movedEntity.index] = denseIndex;
    }

    m_cameras.pop_back();
    m_cameraDenseEntities.pop_back();
    m_cameraSparse[e.index] = InvalidDenseIndex;
}

EntityId World::FindActiveCamera() const
{
    for (size_t i = 0; i < m_cameras.size(); ++i)
    {
        if (m_cameras[i].active)
            return m_cameraDenseEntities[i];
    }
    return EntityId::Invalid();
}

void World::RequestDestroy(EntityId e)
{
    if (!IsAlive(e))
        return;

    // 중복 요청 방지
    auto it = std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), e);
    if (it != m_pendingDestroy.end())
        return;

    m_pendingDestroy.push_back(e);
}

void World::FlushDestroy()
{
    if (m_pendingDestroy.empty())
        return;

    // 1) 중복/순서 안정화: 정렬 + unique (혹시 모를 중복 방어)
    std::sort(m_pendingDestroy.begin(), m_pendingDestroy.end(),
        [](EntityId a, EntityId b) { return a.index < b.index; });

    m_pendingDestroy.erase(
        std::unique(m_pendingDestroy.begin(), m_pendingDestroy.end()),
        m_pendingDestroy.end());

    // 2) 실제 삭제 실행
    for (EntityId e : m_pendingDestroy)
    {
        if (IsAlive(e))
        {
            DestroyEntity(e);
        }
    }

    m_pendingDestroy.clear();
}

void World::FlushStructuralChanges()
{
    // 0) Remove 먼저
    for (EntityId e : m_pendingRemoveAudioSource) if (IsAlive(e)) RemoveAudioSource_Immediate(e);
    for (EntityId e : m_pendingRemoveLight)       if (IsAlive(e)) RemoveLight_Immediate(e);
    for (EntityId e : m_pendingRemoveUIElement)   if (IsAlive(e)) RemoveUIElement_Immediate(e);
    for (EntityId e : m_pendingRemoveRigidBody)   if (IsAlive(e)) RemoveRigidBody_Immediate(e);
    for (EntityId e : m_pendingRemoveCollider)    if (IsAlive(e)) RemoveCollider_Immediate(e);

    m_pendingRemoveAudioSource.clear();
    m_pendingRemoveLight.clear();
    m_pendingRemoveUIElement.clear();
    m_pendingRemoveRigidBody.clear();
    m_pendingRemoveCollider.clear();

    // 1) Add/Upsert 적용
    for (auto& [e, c] : m_pendingAddTransform)   if (IsAlive(e)) AddTransform_Immediate(e, c);

    for (auto& [e, c] : m_pendingAddMesh)        if (IsAlive(e)) AddMesh_Immediate(e, c);
    for (auto& [e, c] : m_pendingAddMaterial)    if (IsAlive(e)) AddMaterial_Immediate(e, c);
    for (auto& [e, c] : m_pendingAddCamera)      if (IsAlive(e)) AddCamera_Immediate(e, c);

    for (auto& [e, rb] : m_pendingAddRigidBody)  if (IsAlive(e)) AddRigidBody_Immediate(e, rb);
    for (auto& [e, col] : m_pendingAddCollider)  if (IsAlive(e)) AddCollider_Immediate(e, col);

    for (auto& [e, a] : m_pendingAddAudioSource) if (IsAlive(e)) AddAudioSource_Immediate(e, a);
    for (auto& [e, l] : m_pendingAddLight)       if (IsAlive(e)) AddLight_Immediate(e, l);
    for (auto& [e, ui] : m_pendingAddUIElement)  if (IsAlive(e)) AddUIElement_Immediate(e, ui);

    m_pendingAddTransform.clear();
    m_pendingAddMesh.clear();
    m_pendingAddMaterial.clear();
    m_pendingAddCamera.clear();
    m_pendingAddRigidBody.clear();
    m_pendingAddCollider.clear();
    m_pendingAddAudioSource.clear();
    m_pendingAddLight.clear();
    m_pendingAddUIElement.clear();

    // 2) Script Add는 ScriptComponent를 여기서 “생성만” 하고, 실제 scripts 벡터 반영은 FlushScripts에서 하게 연결
    for (auto& cmd : m_pendingScriptAdd)
    {
        if (!IsAlive(cmd.e) || !cmd.b) continue;

        cmd.b->_SetEntity(cmd.e);
        auto& sc = EnsureScriptComponent(cmd.e);          // 구조 변경이지만 이제 프레임 경계에서만 실행됨
        sc.pendingAdd.push_back({ std::move(cmd.b), cmd.enabled });
    }
    m_pendingScriptAdd.clear();
}

void World::AddTransform(EntityId e, const TransformComponent& init)
{
    if (!IsAlive(e)) return;

    // 이미 실제 Transform이 있으면 아무것도 안 함
    if (HasTransform(e)) return;

    // 이미 pending이면 "덮어쓰기" 또는 무시
    for (auto& [pe, tc] : m_pendingAddTransform)
    {
        if (pe == e)
        {
            tc = init;      // upsert
            return;
        }
    }

    m_pendingAddTransform.push_back({ e, init });
}

TransformComponent& World::GetPendingTransform(EntityId e)
{
    for (auto& [pe, tc] : m_pendingAddTransform)
    {
        if (pe == e)
            return tc;
    }

#if defined(_DEBUG)
    OutputDebugStringA("GetPendingTransform: pending transform not found\n");
    __debugbreak();
#endif

    // 절대 여기까지 오면 안 되지만 컴파일러용
    static TransformComponent dummy{};
    return dummy;
}

bool World::HasTransform(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_transformSparse.size()) return false;

	const uint32_t di = m_transformSparse[e.index];
	if (di == InvalidDenseIndex) return false;
	if (di >= m_transformDenseEntities.size()) return false;
	return m_transformDenseEntities[di] == e;
}

TransformComponent& World::GetTransform(EntityId e)
{
#if defined(_DEBUG)
	assert(HasTransform(e));
#endif
    const uint32_t denseIndex = m_transformSparse[e.index];
    return m_transforms[denseIndex];
}

const TransformComponent& World::GetTransform(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasTransform(e));
#endif
    const uint32_t denseIndex = m_transformSparse[e.index];
    return m_transforms[denseIndex];
}

bool World::IsTransformPending(EntityId e) const
{
    for (const auto& p : m_pendingAddTransform)
        if (p.first == e)
            return true;
    return false;
}

bool World::HasOrPendingTransform(EntityId e) const
{
    return HasTransform(e) || IsTransformPending(e);
}

void World::RemoveTransform(EntityId e)
{
    if (!HasTransform(e))
        return;

    // parent/child 관계 정리: 부모에서 나 제거, 자식들은 부모 invalid로
    TransformComponent& t = GetTransform(e);

    // 부모에서 분리
    if (t.parent.IsValid() && HasTransform(t.parent))
    {
        TransformComponent& p = GetTransform(t.parent);
        auto& ch = p.children;
        for (size_t i = 0; i < ch.size(); ++i)
        {
            if (ch[i] == e)
            {
                ch[i] = ch.back();
                ch.pop_back();
                break;
            }
        }
    }

    // 자식들 parent 끊기(일단 루트로 올림)
    for (EntityId c : t.children)
    {
        if (HasTransform(c))
        {
            TransformComponent& ct = GetTransform(c);
            ct.parent = EntityId::Invalid();
            ct.dirty = true;
        }
    }
    t.children.clear();
    t.parent = EntityId::Invalid();

    // sparse-set swap-remove
    const uint32_t denseIndex = m_transformSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_transforms.size() - 1;

    if (denseIndex != lastIndex)
    {
        // 마지막 요소를 denseIndex 위치로 옮김
        m_transforms[denseIndex] = std::move(m_transforms[lastIndex]);
        m_transformDenseEntities[denseIndex] = m_transformDenseEntities[lastIndex];

        // sparse 갱신
        EntityId movedEntity = m_transformDenseEntities[denseIndex];
        m_transformSparse[movedEntity.index] = denseIndex;
    }

    m_transforms.pop_back();
    m_transformDenseEntities.pop_back();
    m_transformSparse[e.index] = InvalidDenseIndex;
}

void World::SetParent(EntityId child, EntityId newParent)
{
    if (!HasTransform(child)) return;
    if (newParent.IsValid() && !HasTransform(newParent)) return;
    if (child == newParent) return;
    if (newParent.IsValid() && IsDescendant(newParent, child)) return; // cycle 방지

    TransformComponent& ct = GetTransform(child);

    // 기존 parent에서 제거
    if (ct.parent.IsValid() && HasTransform(ct.parent))
    {
        TransformComponent& oldP = GetTransform(ct.parent);
        auto& ch = oldP.children;
        for (size_t i = 0; i < ch.size(); ++i)
        {
            if (ch[i] == child)
            {
                ch[i] = ch.back();
                ch.pop_back();
                break;
            }
        }
    }

    // 새 parent에 추가
    ct.parent = newParent;
    if (newParent.IsValid())
    {
        TransformComponent& np = GetTransform(newParent);
        np.children.push_back(child);
    }

    // 계층 변경은 월드행렬 전체에 영향
    MarkDirtyRecursive(child);
}

bool World::IsDescendant(EntityId node, EntityId potentialAncestor) const
{
    EntityId current = potentialAncestor;
    while (current.IsValid())
    {
        if (current == node)
            return true;
        if (!HasTransform(current))
            break;
        const TransformComponent& t = GetTransform(current);
        current = t.parent;
	}
    return false;
}

void World::MarkDirtyRecursive(EntityId e)
{
    if (!HasTransform(e)) return;

    TransformComponent& t = GetTransform(e);
    if (!t.dirty)
        t.dirty = true;

    for (EntityId c : t.children)
        MarkDirtyRecursive(c);
}

DirectX::XMMATRIX World::LocalMatrix(const TransformComponent& t) const
{
    const XMMATRIX S = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
    const XMVECTOR Q = XMVectorSet(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w);
    const XMMATRIX R = XMMatrixRotationQuaternion(Q);
    const XMMATRIX T = XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
    return S * R * T; // row-vector 관례
}

void World::UpdateWorldRecursive(EntityId e, const DirectX::XMMATRIX& parentWorld)
{
    using namespace DirectX;
    TransformComponent& t = GetTransform(e);

    XMMATRIX local = LocalMatrix(t);
    XMMATRIX worldM = local * parentWorld;

    XMStoreFloat4x4(&t.world, worldM);
    t.dirty = false;

    for (EntityId c : t.children)
        if (HasTransform(c)) UpdateWorldRecursive(c, worldM);
}

void World::UpdateTransformNow(EntityId e)
{
    using namespace DirectX;
    if (!HasTransform(e)) return;

    // Find root ancestor
    EntityId root = e;
    while (HasTransform(root))
    {
        const TransformComponent& t = GetTransform(root);
        if (!t.parent.IsValid()) break;
        root = t.parent;
    }

    const XMMATRIX I = XMMatrixIdentity();
    if (HasTransform(root))
        UpdateWorldRecursive(root, I);

    m_transformUpdatedFrame = m_frameIndex; // treat as updated for this frame
}

void World::UpdateTransforms()
{
    using namespace DirectX;
    const XMMATRIX I = XMMatrixIdentity();

    // dirty가 된 노드들의 "루트"를 모아서 중복 없이 갱신
    std::vector<EntityId> roots;
    roots.reserve(32);

    std::unordered_set<uint32_t> seenRoots; // root.index로 중복 제거
    seenRoots.reserve(64);

    auto FindRoot = [&](EntityId e) -> EntityId
        {
            EntityId r = e;
            while (HasTransform(r))
            {
                const TransformComponent& t = GetTransform(r);
                if (!t.parent.IsValid()) break;
                r = t.parent;
            }
            return r;
        };

    for (EntityId e : m_transformDenseEntities)
    {
        if (!HasTransform(e)) continue;
        const TransformComponent& t = GetTransform(e);
        if (!t.dirty) continue;

        EntityId root = FindRoot(e);
        if (!root.IsValid() || !HasTransform(root)) continue;

        if (seenRoots.insert(root.index).second)
            roots.push_back(root);
    }

    for (EntityId root : roots)
        UpdateWorldRecursive(root, I);

    m_transformUpdatedFrame = m_frameIndex;
}

void World::BeginFrame()
{
    ++m_frameIndex;
}

bool World::TransformsUpdatedThisFrame() const
{
    return m_transformUpdatedFrame == m_frameIndex;
}

void World::EnsureScriptSparseSize(uint32_t entityIndex)
{
    if (m_scriptSparse.size() <= entityIndex)
		m_scriptSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::RemoveScript(EntityId e, Behaviour* which)
{
    if (!IsAlive(e) || !which) return;
    if (!HasScript(e)) return;

    auto& sc = GetScript(e);
    sc.pendingRemove.push_back({ which });
}

void World::AddTransform_Immediate(EntityId e, const TransformComponent& comp)
{
    if (!IsAlive(e)) return;
    EnsureTransformSparseSize(e.index);

    uint32_t di = m_transformSparse[e.index];
    if (di != InvalidDenseIndex)
    {
        if (di >= m_transformDenseEntities.size())
        {
            // 깨진 sparse 복구 후 새로 추가로 진행
            m_transformSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            // 기존 슬롯 갱신 (Transform은 보통 덮어쓰기)
            m_transformDenseEntities[di] = e;
            m_transforms[di] = comp;

            // world를 identity로 초기화 + dirty
            auto& t = m_transforms[di];
            XMStoreFloat4x4(&t.world, XMMatrixIdentity());
            t.dirty = true;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_transforms.size();
    m_transformSparse[e.index] = denseIndex;

    m_transformDenseEntities.push_back(e);
    m_transforms.push_back(comp);

    auto& t = m_transforms.back();
    XMStoreFloat4x4(&t.world, XMMatrixIdentity());
    t.dirty = true;
}

void World::AddMaterial_Immediate(EntityId e, const MaterialComponent& comp)
{
    if (!IsAlive(e)) return;
    EnsureMaterialSparseSize(e.index);

    uint32_t di = m_materialSparse[e.index];
    if (di != InvalidDenseIndex)
    {
        if (di >= m_materialDenseEntities.size())
        {
            m_materialSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            m_materialDenseEntities[di] = e;
            m_materials[di] = comp;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_materials.size();
    m_materialSparse[e.index] = denseIndex;
    m_materialDenseEntities.push_back(e);
    m_materials.push_back(comp);
}

void World::AddCamera_Immediate(EntityId e, const CameraComponent& c)
{
    if (!IsAlive(e)) return;
    EnsureCameraSparseSize(e.index);

    uint32_t di = m_cameraSparse[e.index];
    if (di != InvalidDenseIndex)
    {
        if (di >= m_cameraDenseEntities.size())
        {
            m_cameraSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            m_cameraDenseEntities[di] = e;
            m_cameras[di] = c;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_cameras.size();
    m_cameraSparse[e.index] = denseIndex;
    m_cameraDenseEntities.push_back(e);
    m_cameras.push_back(c);
}

void World::AddRigidBody_Immediate(EntityId e, const RigidBodyComponent& comp)
{
    if (!IsAlive(e)) return;
    EnsureRigidBodySparseSize(e.index);

    uint32_t di = m_rigidBodySparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_rigidBodyDenseEntities.size())
        {
            m_rigidBodySparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            // stale이든 정상이든: 소유자 갱신 + 덮어쓰기
            m_rigidBodyDenseEntities[di] = e;
            m_rigidBodies[di] = comp;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_rigidBodies.size();
    m_rigidBodySparse[e.index] = denseIndex;
    m_rigidBodyDenseEntities.push_back(e);
    m_rigidBodies.push_back(comp);
}

void World::AddUIElement_Immediate(EntityId e, const UIElementComponent& c)
{
    if (!IsAlive(e)) return;
    EnsureUIElementSparseSize(e.index);

    uint32_t di = m_uiElementSparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_uiElementDenseEntities.size())
        {
            m_uiElementSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            m_uiElementDenseEntities[di] = e;
            m_uiElements[di] = c;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_uiElements.size();
    m_uiElementSparse[e.index] = denseIndex;
    m_uiElementDenseEntities.push_back(e);
    m_uiElements.push_back(c);
}

void World::AddAudioSource_Immediate(EntityId e, const AudioSourceComponent& c)
{
    if (!IsAlive(e)) return;
    EnsureAudioSourceSparseSize(e.index);

    uint32_t di = m_audioSourceSparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_audioSourceDenseEntities.size())
        {
            m_audioSourceSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            m_audioSourceDenseEntities[di] = e;
            m_audioSources[di] = c;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_audioSources.size();
    m_audioSourceSparse[e.index] = denseIndex;
    m_audioSourceDenseEntities.push_back(e);
    m_audioSources.push_back(c);
}

void World::AddLight_Immediate(EntityId e, const LightComponent& c)
{
    if (!IsAlive(e)) return;
    EnsureLightSparseSize(e.index);

    uint32_t di = m_lightSparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_lightDenseEntities.size())
        {
            m_lightSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            m_lightDenseEntities[di] = e;
            m_lights[di] = c;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_lights.size();
    m_lightSparse[e.index] = denseIndex;
    m_lightDenseEntities.push_back(e);
    m_lights.push_back(c);
}

void World::RemoveAudioSource_Immediate(EntityId e)
{
    if (!HasAudioSource(e)) return;

    const uint32_t denseIndex = m_audioSourceSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_audioSources.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_audioSources[denseIndex] = std::move(m_audioSources[lastIndex]);
        m_audioSourceDenseEntities[denseIndex] = m_audioSourceDenseEntities[lastIndex];

        EntityId movedEntity = m_audioSourceDenseEntities[denseIndex];
        EnsureAudioSourceSparseSize(movedEntity.index);
        m_audioSourceSparse[movedEntity.index] = denseIndex;
    }

    m_audioSources.pop_back();
    m_audioSourceDenseEntities.pop_back();
    m_audioSourceSparse[e.index] = InvalidDenseIndex;
}

void World::RemoveLight_Immediate(EntityId e)
{
    if (!HasLight(e)) return;

    const uint32_t denseIndex = m_lightSparse[e.index];
    const uint32_t last = (uint32_t)m_lights.size() - 1;

    if (denseIndex != last)
    {
        m_lights[denseIndex] = m_lights[last];
        m_lightDenseEntities[denseIndex] = m_lightDenseEntities[last];
        m_lightSparse[m_lightDenseEntities[denseIndex].index] = denseIndex;
    }

    m_lights.pop_back();
    m_lightDenseEntities.pop_back();
    m_lightSparse[e.index] = InvalidDenseIndex;
}

void World::RemoveUIElement_Immediate(EntityId e)
{
    if (!HasUIElement(e)) return;

    const uint32_t denseIndex = m_uiElementSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_uiElements.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_uiElements[denseIndex] = std::move(m_uiElements[lastIndex]);
        m_uiElementDenseEntities[denseIndex] = m_uiElementDenseEntities[lastIndex];

        EntityId movedEntity = m_uiElementDenseEntities[denseIndex];
        EnsureUIElementSparseSize(movedEntity.index);
        m_uiElementSparse[movedEntity.index] = denseIndex;
    }

    m_uiElements.pop_back();
    m_uiElementDenseEntities.pop_back();
    m_uiElementSparse[e.index] = InvalidDenseIndex;
}

void World::RemoveRigidBody_Immediate(EntityId e)
{
    if (!HasRigidBody(e)) return;

    const uint32_t denseIndex = m_rigidBodySparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_rigidBodies.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_rigidBodies[denseIndex] = std::move(m_rigidBodies[lastIndex]);
        m_rigidBodyDenseEntities[denseIndex] = m_rigidBodyDenseEntities[lastIndex];

        EntityId movedEntity = m_rigidBodyDenseEntities[denseIndex];
        EnsureRigidBodySparseSize(movedEntity.index);
        m_rigidBodySparse[movedEntity.index] = denseIndex;
    }

    m_rigidBodies.pop_back();
    m_rigidBodyDenseEntities.pop_back();
    m_rigidBodySparse[e.index] = InvalidDenseIndex;
}

void World::RemoveCollider_Immediate(EntityId e)
{
    if (!HasCollider(e)) return;

    const uint32_t denseIndex = m_colliderSparse[e.index];
    const uint32_t lastIndex = (uint32_t)m_colliders.size() - 1;

    if (denseIndex != lastIndex)
    {
        m_colliders[denseIndex] = std::move(m_colliders[lastIndex]);
        m_colliderDenseEntities[denseIndex] = m_colliderDenseEntities[lastIndex];

        EntityId movedEntity = m_colliderDenseEntities[denseIndex];
        EnsureColliderSparseSize(movedEntity.index);
        m_colliderSparse[movedEntity.index] = denseIndex;
    }

    m_colliders.pop_back();
    m_colliderDenseEntities.pop_back();
    m_colliderSparse[e.index] = InvalidDenseIndex;
}

void World::RemoveScriptComponent(EntityId e)
{
    if (!HasScript(e)) return;

    const uint32_t di = m_scriptSparse[e.index];
    const uint32_t last = (uint32_t)m_scripts.size() - 1;

    // 1) 제거 대상 컴포넌트 정리 (pending 포함)
    {
        auto& sc = m_scripts[di];

        // scripts 안에 들어간 Behaviour는 제거 시 OnDestroy 호출 보장
        for (auto& s : sc.scripts)
            if (s.ptr) s.ptr->OnDestroy();

        sc.scripts.clear();

        // 아직 편입 전인 unique_ptr들도 여기서 정리됨
        sc.pendingAdd.clear();

        // raw 포인터 큐는 반드시 비워서 댕글링 읽기 차단
        sc.pendingRemove.clear();
    }

    // 2) dense swap-remove
    if (di != last)
    {
        m_scripts[di] = std::move(m_scripts[last]);
        m_scriptDenseEntities[di] = m_scriptDenseEntities[last];

        EntityId moved = m_scriptDenseEntities[di];
        m_scriptSparse[moved.index] = di;
    }

    m_scripts.pop_back();
    m_scriptDenseEntities.pop_back();
    m_scriptSparse[e.index] = InvalidDenseIndex;
}

XMFLOAT3 World::GetLocalPosition(EntityId e)
{
    if (!HasOrPendingTransform(e)) return XMFLOAT3{ 0,0,0 };

    if (!HasTransform(e))
    {
        auto& pend = GetPendingTransform(e);
        return pend.position;
    }

    const TransformComponent& t = GetTransform(e);
    return t.position;
}

void World::SetLocalPosition(EntityId e, const XMFLOAT3& p)
{
    if (!HasOrPendingTransform(e)) return;

    if (!HasTransform(e))
    {
        auto& pend = GetPendingTransform(e);
        pend.position = p;
        MarkDirtyRecursive(e);
        return;
    }

    TransformComponent& t = GetTransform(e);
    t.position = p;
    MarkDirtyRecursive(e);
}

XMFLOAT4 World::GetLocalRotation(EntityId e) const
{
    if (!HasTransform(e)) return XMFLOAT4{ 0,0,0,1 };
    const TransformComponent& t = GetTransform(e);
    return t.rotation;
}

void World::SetLocalRotation(EntityId e, const XMFLOAT4& q)
{
    using namespace DirectX;

    if (!HasOrPendingTransform(e)) return;

    XMVECTOR vq = XMLoadFloat4(&q);
    float lsq = XMVectorGetX(XMVector4LengthSq(vq));

    XMFLOAT4 out = { 0.f, 0.f, 0.f, 1.f };
    if (lsq >= 1e-8f)
    {
        vq = XMQuaternionNormalize(vq);
        XMStoreFloat4(&out, vq);
    }

    if (HasTransform(e))
    {
        TransformComponent& t = GetTransform(e);
        t.rotation = out;
        MarkDirtyRecursive(e);
    }
    else
    {
        TransformComponent& t = GetPendingTransform(e);
        t.rotation = out;
        MarkDirtyRecursive(e);
    }
}

DirectX::XMFLOAT3 World::GetLocalRotationEuler(EntityId e) const
{
    using namespace DirectX;

    if (!HasTransform(e)) return XMFLOAT3{ 0,0,0 };

    const TransformComponent& t = GetTransform(e);
    const float x = t.rotation.x;
    const float y = t.rotation.y;
    const float z = t.rotation.z;
    const float w = t.rotation.w;

    auto clamp = [](float v, float mn, float mx) { return (v < mn) ? mn : (v > mx) ? mx : v; };

    // pitch (x)
    float sinp = 2.0f * (w * x - z * y);
    sinp = clamp(sinp, -1.0f, 1.0f);
    float pitch = std::asin(sinp);

    // yaw (y)
    float siny_cosp = 2.0f * (w * y + x * z);
    float cosy_cosp = 1.0f - 2.0f * (x * x + y * y);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    // roll (z)
    float sinr_cosp = 2.0f * (w * z + x * y);
    float cosr_cosp = 1.0f - 2.0f * (y * y + z * z);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    return XMFLOAT3{ pitch, yaw, roll };
}

void World::SetLocalRotationEuler(EntityId e, const DirectX::XMFLOAT3& eulerRad)
{
    using namespace DirectX;

    if (!HasOrPendingTransform(e)) return;

    XMVECTOR q = XMQuaternionRotationRollPitchYaw(
        eulerRad.x, eulerRad.y, eulerRad.z
    );

    XMFLOAT4 out;
    XMStoreFloat4(&out, q);

    if (HasTransform(e))
    {
        TransformComponent& t = GetTransform(e);
        t.rotation = out;
        MarkDirtyRecursive(e);
    }
    else
    {
        TransformComponent& t = GetPendingTransform(e);
        t.rotation = out;
        MarkDirtyRecursive(e);
    }
}

XMFLOAT3 World::GetLocalScale(EntityId e) const
{
    if (!HasTransform(e)) return XMFLOAT3{ 1,1,1 };
    const TransformComponent& t = GetTransform(e);
    return t.scale;
}

void World::SetLocalScale(EntityId e, const XMFLOAT3& s)
{
    if (!HasOrPendingTransform(e)) return;

    if (HasTransform(e))
    {
        TransformComponent& t = GetTransform(e);
        t.scale = s;
        MarkDirtyRecursive(e);
    }
    else
    {
        TransformComponent& t = GetPendingTransform(e);
        t.scale = s;
        MarkDirtyRecursive(e);
    }
}

void World::TranslateLocal(EntityId e, const XMFLOAT3& delta)
{
    if (!HasOrPendingTransform(e)) return;

    if (HasTransform(e))
    {
        TransformComponent& t = GetTransform(e);
        t.position.x += delta.x;
        t.position.y += delta.y;
        t.position.z += delta.z;
        MarkDirtyRecursive(e);
    }
    else
    {
        TransformComponent& t = GetPendingTransform(e);
        t.position.x += delta.x;
        t.position.y += delta.y;
        t.position.z += delta.z;

        MarkDirtyRecursive(e);
    }
}

XMFLOAT4X4 World::GetWorldMatrix(EntityId e) const
{
    static const XMFLOAT4X4 I = [] {
        XMFLOAT4X4 m;
        XMStoreFloat4x4(&m, XMMatrixIdentity());
        return m;
        }();

    if (!HasTransform(e)) return I;
    const TransformComponent& t = GetTransform(e);
    return t.world;
}

XMFLOAT3 World::GetWorldPosition(EntityId e) const
{
    if (!HasTransform(e)) return XMFLOAT3{ 0,0,0 };

    const TransformComponent& t = GetTransform(e);

    // world 행렬의 translation 성분 추출
    // 우리 엔진은 row-vector(벡터가 왼쪽) 관례: v * M
    // 따라서 translation은 M의 row 3 (W.r[3])에 존재
    XMMATRIX W = XMLoadFloat4x4(&t.world);
    XMVECTOR pos = W.r[3]; // (x,y,z,1) row 벡터
    XMFLOAT3 out{};
    XMStoreFloat3(&out, pos);
    return out;
}

// ================================
// Rigidbody (dense/sparse) - World.cpp 새 함수
// ================================

void World::EnsureRigidBodySparseSize(uint32_t entityIndex)
{
    if (m_rigidBodySparse.size() <= entityIndex)
        m_rigidBodySparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddRigidBody(EntityId e, const RigidBodyComponent& comp)
{
    if (!IsAlive(e)) return;
    m_pendingAddRigidBody.push_back({ e, comp });
}

bool World::HasRigidBody(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_rigidBodySparse.size()) return false;

    const uint32_t di = m_rigidBodySparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_rigidBodyDenseEntities.size()) return false;
    return m_rigidBodyDenseEntities[di] == e;
}

RigidBodyComponent& World::GetRigidBody(EntityId e)
{
#if defined(_DEBUG)
    assert(HasRigidBody(e));
#endif
    return m_rigidBodies[m_rigidBodySparse[e.index]];
}

const RigidBodyComponent& World::GetRigidBody(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasRigidBody(e));
#endif
    return m_rigidBodies[m_rigidBodySparse[e.index]];
}

void World::RemoveRigidBody(EntityId e)
{
    if (!IsAlive(e)) return;
    m_pendingRemoveRigidBody.push_back(e);
}


// ================================
// Collider (dense/sparse) - World.cpp 새 함수
// ================================

void World::EnsureColliderSparseSize(uint32_t entityIndex)
{
    if (m_colliderSparse.size() <= entityIndex)
        m_colliderSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddCollider(EntityId e, const ColliderComponent& comp)
{
    if (!IsAlive(e)) return;
    m_pendingAddCollider.push_back({ e, comp });
}

void World::AddCollider_Immediate(EntityId e, const ColliderComponent& comp)
{
    if (!IsAlive(e)) return;
    EnsureColliderSparseSize(e.index);

    uint32_t di = m_colliderSparse[e.index];

    if (di != InvalidDenseIndex)
    {
        if (di >= m_colliderDenseEntities.size())
        {
            m_colliderSparse[e.index] = InvalidDenseIndex;
        }
        else
        {
            // denseEntities도 갱신해야 HasCollider/레이캐스트가 정상
            m_colliderDenseEntities[di] = e;
            m_colliders[di] = comp;
            return;
        }
    }

    const uint32_t denseIndex = (uint32_t)m_colliders.size();
    m_colliderSparse[e.index] = denseIndex;
    m_colliderDenseEntities.push_back(e);
    m_colliders.push_back(comp);
}


bool World::HasCollider(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_colliderSparse.size()) return false;

    const uint32_t di = m_colliderSparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_colliderDenseEntities.size()) return false;
    return m_colliderDenseEntities[di] == e;
}

ColliderComponent& World::GetCollider(EntityId e)
{
#if defined(_DEBUG)
    assert(HasCollider(e));
#endif
    return m_colliders[m_colliderSparse[e.index]];
}

const ColliderComponent& World::GetCollider(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasCollider(e));
#endif
    return m_colliders[m_colliderSparse[e.index]];
}

void World::RemoveCollider(EntityId e)
{
    if (!IsAlive(e)) return;
    m_pendingRemoveCollider.push_back(e);
}

void World::PushCollisionEvent(const CollisionEvent& ev)
{
    m_collisionEvents.push_back(ev);
}

void World::DrainCollisionEvents(std::vector<CollisionEvent>& out)
{
    out.clear();
    out.swap(m_collisionEvents); // 빠르게 넘기고 내부 비움
}

ScriptComponent& World::EnsureScriptComponent(EntityId e)
{
    EnsureScriptSparseSize(e.index);

    uint32_t di = m_scriptSparse[e.index];
    if (di != InvalidDenseIndex)
        return m_scripts[di];

    di = (uint32_t)m_scripts.size();
    m_scriptSparse[e.index] = di;
    m_scriptDenseEntities.push_back(e);
    m_scripts.emplace_back();          // empty ScriptComponent
    return m_scripts.back();
}

void World::AddScript(EntityId e, std::unique_ptr<Behaviour> b, bool enabled)
{
    if (!IsAlive(e) || !b) return;

    m_pendingScriptAdd.push_back({ e, std::move(b), enabled });
}

bool World::HasScript(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_scriptSparse.size()) return false;

    const uint32_t di = m_scriptSparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_scriptDenseEntities.size()) return false;
	return m_scriptDenseEntities[di] == e;
}

ScriptComponent& World::GetScript(EntityId e)
{
    #if defined(_DEBUG)
	assert(HasScript(e));
    #endif
    const uint32_t denseIndex = m_scriptSparse[e.index];
	return m_scripts[denseIndex];
}

void World::FlushScripts()
{
#if defined(_DEBUG)
    if (m_scripts.size() != m_scriptDenseEntities.size())
    {
        OutputDebugStringA("Script storage mismatch: m_scripts.size != m_scriptDenseEntities.size\n");
        __debugbreak();
    }
#endif

    size_t di = 0;
    while (di < m_scripts.size())
    {
        EntityId e = m_scriptDenseEntities[di];
        if (!IsAlive(e) || IsPendingDestroy(e))
        {
            ++di;
            continue;
        }

        auto& sc = m_scripts[di];

        // 1) pendingRemove
        if (!sc.pendingRemove.empty())
        {
            for (auto& r : sc.pendingRemove)
            {
                Behaviour* target = r.ptr;
                if (!target) continue;

                for (size_t i = 0; i < sc.scripts.size(); ++i)
                {
                    auto& entry = sc.scripts[i];
                    if (entry.ptr.get() != target) continue;

                    entry.ptr->OnDestroy();

                    const size_t last = sc.scripts.size() - 1;
                    if (i != last) sc.scripts[i] = std::move(sc.scripts[last]);
                    sc.scripts.pop_back();
                    break;
                }
            }
            sc.pendingRemove.clear();
        }

        // 2) pendingAdd
        if (!sc.pendingAdd.empty())
        {
            sc.scripts.reserve(sc.scripts.size() + sc.pendingAdd.size());
            for (auto& a : sc.pendingAdd)
            {
                if (!a.ptr) continue;
                ScriptEntry se;
                se.ptr = std::move(a.ptr);
                se.enabled = a.enabled;
                sc.scripts.push_back(std::move(se));
            }
            sc.pendingAdd.clear();
        }

        // 3) empty => remove component
        if (sc.scripts.empty() && sc.pendingAdd.empty())
        {
            RemoveScriptComponent(e);
            // swap-remove로 현재 di 위치에 "새 엔티티"가 들어왔으니 di 증가 없이 재검사
            continue;
        }

        ++di;
    }
}

void World::EnsureAudioSourceSparseSize(uint32_t entityIndex)
{
    if (m_audioSourceSparse.size() <= entityIndex)
        m_audioSourceSparse.resize(entityIndex + 1, InvalidDenseIndex);
}


void World::EnsureLightSparseSize(uint32_t entityIndex)
{
    if (m_lightSparse.size() <= entityIndex)
        m_lightSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddAudioSource(EntityId e, const AudioSourceComponent& c)
{
    if (!IsAlive(e)) return;
    m_pendingAddAudioSource.push_back({ e, c });
}

bool World::HasAudioSource(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_audioSourceSparse.size()) return false;

    const uint32_t di = m_audioSourceSparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_audioSourceDenseEntities.size()) return false;
    return m_audioSourceDenseEntities[di] == e;
}

AudioSourceComponent& World::GetAudioSource(EntityId e)
{
#if defined(_DEBUG)
    assert(HasAudioSource(e));
#endif
    return m_audioSources[m_audioSourceSparse[e.index]];
}

const AudioSourceComponent& World::GetAudioSource(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasAudioSource(e));
#endif
    return m_audioSources[m_audioSourceSparse[e.index]];
}

void World::RemoveAudioSource(EntityId e)
{
    if (!IsAlive(e)) return;
    m_pendingRemoveAudioSource.push_back(e);
}

void World::AddLight(EntityId e, const LightComponent& c)
{
    if (!IsAlive(e)) return;
    m_pendingAddLight.push_back({ e, c });
}

bool World::HasLight(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_lightSparse.size()) return false;
    const uint32_t denseIndex = m_lightSparse[e.index];
    if (denseIndex == InvalidDenseIndex) return false;
    if (denseIndex >= m_lightDenseEntities.size()) return false;
    return m_lightDenseEntities[denseIndex].index == e.index && m_lightDenseEntities[denseIndex].generation == e.generation;
}

LightComponent& World::GetLight(EntityId e)
{
    return m_lights[m_lightSparse[e.index]];
}

const LightComponent& World::GetLight(EntityId e) const
{
    return m_lights[m_lightSparse[e.index]];
}

void World::RemoveLight(EntityId e)
{
    if (!IsAlive(e)) return;
    m_pendingRemoveLight.push_back(e);
}

const std::vector<LightComponent>& World::GetLightsDense() const { return m_lights; }

void World::EnsureUIElementSparseSize(uint32_t entityIndex)
{
    if (m_uiElementSparse.size() <= entityIndex)
        m_uiElementSparse.resize(entityIndex + 1, InvalidDenseIndex);
}

void World::AddUIElement(EntityId e, const UIElementComponent& c)
{
    if (!IsAlive(e)) return;
    m_pendingAddUIElement.push_back({ e, c });
}

bool World::HasUIElement(EntityId e) const
{
    if (!IsAlive(e)) return false;
    if (e.index >= m_uiElementSparse.size()) return false;

    const uint32_t di = m_uiElementSparse[e.index];
    if (di == InvalidDenseIndex) return false;
    if (di >= m_uiElementDenseEntities.size()) return false;
    return m_uiElementDenseEntities[di] == e;
}

UIElementComponent& World::GetUIElement(EntityId e)
{
#if defined(_DEBUG)
    assert(HasUIElement(e));
#endif
    return m_uiElements[m_uiElementSparse[e.index]];
}

const UIElementComponent& World::GetUIElement(EntityId e) const
{
#if defined(_DEBUG)
    assert(HasUIElement(e));
#endif
    return m_uiElements[m_uiElementSparse[e.index]];
}

void World::RemoveUIElement(EntityId e)
{
    if (!IsAlive(e)) return;
    m_pendingRemoveUIElement.push_back(e);
}