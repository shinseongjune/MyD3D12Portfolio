#include "EnemyIndicatorHUD.h"
using namespace DirectX;

static void DrawBracket(SceneContext& ctx, float x, float y, float halfSize, float fontPx)
{
    const float s = halfSize;

    ctx.DrawWText(x - s, y - s, L"┌", fontPx);
    ctx.DrawWText(x + s, y - s, L"┐", fontPx);
    ctx.DrawWText(x - s, y + s, L"└", fontPx);
    ctx.DrawWText(x + s, y + s, L"┘", fontPx);
}

static bool ProjectToScreen(
    const XMFLOAT3& worldPos,
    const XMMATRIX& viewProj,
    float screenW, float screenH,
    XMFLOAT2& outScreen)
{
    XMVECTOR p = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(p, viewProj);

    float cw = XMVectorGetW(clip);
    if (cw <= 0.0f) return false;

    float ndcX = XMVectorGetX(clip) / cw;
    float ndcY = XMVectorGetY(clip) / cw;

    if (ndcX < -1 || ndcX > 1 || ndcY < -1 || ndcY > 1)
        return false;

    outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH;
    return true;
}

void EnemyIndicatorHUD::Initialize(SceneContext& ctx, int maxSlots)
{
    m_slots.resize(maxSlots);
}

void EnemyIndicatorHUD::Update(
    SceneContext& ctx,
    EntityId cam,
    EntityId player,
    const std::vector<EntityId>& enemies)
{
    if (!m_enabled) return;

    const auto& camTr = ctx.world.GetTransform(cam);
    
    // view
    const auto& camC = ctx.world.GetCamera(cam);

    XMFLOAT3 p = camTr.position;
    XMFLOAT4 q = camTr.rotation;

    XMVECTOR pos = XMVectorSet(p.x, p.y, p.z, 1.0f);
    XMVECTOR quat = XMVectorSet(q.x, q.y, q.z, q.w);

    XMVECTOR fwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), quat);
    XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), quat);

    XMMATRIX V = XMMatrixLookToLH(pos, fwd, up);

    // proj
    float aspect = float(ctx.screenW) / float(ctx.screenH);
    float fovY = camC.FovYRadians();
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, camC.nearZ, camC.farZ);

    XMMATRIX viewProj = XMMatrixMultiply(V, P);

    float W = (float)ctx.screenW;
    float H = (float)ctx.screenH;

    int slot = 0;

    for (EntityId e : enemies)
    {
        if (slot >= (int)m_slots.size()) break;
        if (!ctx.world.IsAlive(e)) continue;

        const auto& tr = ctx.world.GetTransform(e);

        XMVECTOR camPos = XMVectorSet(p.x, p.y, p.z, 1.0f);
        XMVECTOR camFwd = fwd;
        XMVECTOR toEnemy = XMLoadFloat3(&tr.position) - camPos;

        float front = XMVectorGetX(XMVector3Dot(camFwd, toEnemy));
        if (front <= 0.0f)
            continue;

        XMFLOAT2 screen;
        if (!ProjectToScreen(tr.position, viewProj, W, H, screen))
            continue;

        // 거리 계산
        XMVECTOR a = XMLoadFloat3(&tr.position);
        XMVECTOR b = XMVectorSet(p.x, p.y, p.z, 1.0f);
        float dist = XMVectorGetX(XMVector3Length(a - b));

        // 브라켓 크기 (에이스컴뱃식: 살짝만 변화)
        float t = std::min(dist / 2000.0f, 1.0f);
        float size = std::lerp(42.0f, 18.0f, t);
        float corner = size * 0.35f;

        float x = screen.x;
        float y = screen.y;

        // === 브라켓 그리기 (4개 코너) ===
        DrawBracket(ctx, x, y, size/2, 14);

        // === 거리 텍스트 (임계 이상만) ===
        if (dist > 300.0f)
        {
            wchar_t buf[32];
            if (dist < 2000.0f)
                swprintf(buf, 32, L"%dm", (int)(dist / 10) * 10);
            else
                swprintf(buf, 32, L"%.2fkm", dist / 1000.0f);

            ctx.DrawWText((int)(x + size + 4), (int)(y + size + 4), buf);
        }

        slot++;
    }
}
