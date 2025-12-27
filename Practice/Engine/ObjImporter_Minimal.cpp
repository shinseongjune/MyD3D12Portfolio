#include "ObjImporter_Minimal.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cctype>
#include <cmath>
#include <algorithm>

// ---------------------------
// 작은 유틸
// ---------------------------
static inline bool IsSpace(char c) { return std::isspace((unsigned char)c) != 0; }

static inline std::string Trim(std::string s)
{
    auto notSpace = [](char c) { return !IsSpace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static inline bool StartsWith(const std::string& s, const char* prefix)
{
    size_t n = std::strlen(prefix);
    return s.size() >= n && std::memcmp(s.data(), prefix, n) == 0;
}

// OBJ 인덱스: 1-based, 음수면 끝에서부터(-1이 마지막)
static inline int ResolveObjIndex(int idx, int count)
{
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx; // idx=-1 => count-1
    return -1; // 0은 invalid
}

struct ObjKey
{
    int vi = -1;
    int ti = -1;
    int ni = -1;

    bool operator==(const ObjKey& o) const { return vi == o.vi && ti == o.ti && ni == o.ni; }
};

struct ObjKeyHash
{
    size_t operator()(const ObjKey& k) const noexcept
    {
        // 간단 해시
        size_t h = 1469598103934665603ull;
        auto mix = [&](int v)
            {
                h ^= (size_t)(uint32_t)v + 0x9e3779b9 + (h << 6) + (h >> 2);
            };
        mix(k.vi); mix(k.ti); mix(k.ni);
        return h;
    }
};

static inline Float3 Sub(const Float3& a, const Float3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline Float3 Cross(const Float3& a, const Float3& b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static inline float Dot(const Float3& a, const Float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Float3 Add(const Float3& a, const Float3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline Float3 Mul(const Float3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
static inline Float3 NormalizeSafe(const Float3& v)
{
    float len2 = Dot(v, v);
    if (len2 <= 1e-20f) return { 0,1,0 };
    float inv = 1.0f / std::sqrt(len2);
    return { v.x * inv, v.y * inv, v.z * inv };
}

// "v/vt/vn" | "v//vn" | "v/vt" | "v"
static bool ParseFaceVertex(const std::string& token, int& outV, int& outT, int& outN)
{
    outV = outT = outN = 0;

    // split by '/'
    int parts[3] = { 0,0,0 };
    int partCount = 0;

    std::string cur;
    for (size_t i = 0; i <= token.size(); ++i)
    {
        char c = (i < token.size()) ? token[i] : '\0';
        if (c == '/' || c == '\0')
        {
            if (partCount < 3)
            {
                if (!cur.empty())
                    parts[partCount] = std::stoi(cur);
                else
                    parts[partCount] = 0; // empty => 0
                partCount++;
            }
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }

    // 최소 v는 있어야
    if (partCount >= 1) outV = parts[0];
    if (partCount >= 2) outT = parts[1];
    if (partCount >= 3) outN = parts[2];
    return outV != 0;
}

static void FinalizeMesh(
    ImportedModel& model,
    ImportedMesh& mesh,
    bool hadAnyTriangles)
{
    if (!hadAnyTriangles) return;
    if (mesh.submeshes.empty())
    {
        ImportedSubmesh sm{};
        sm.startIndex = 0;
        sm.indexCount = (uint32_t)mesh.indices.size();
        sm.materialIndex = 0;
        sm.name = mesh.name;
        mesh.submeshes.push_back(sm);
    }
    model.meshes.push_back(std::move(mesh));
    mesh = ImportedMesh{};
}

Result<ImportedModel> ObjImporter_Minimal::Import(const std::string& filePath, const ImportOptions& options)
{
    std::ifstream f(filePath);
    if (!f.is_open())
        return Result<ImportedModel>::Fail("Failed to open OBJ: " + filePath);

    ImportedModel out{};
    out.sourcePath = filePath;

    std::vector<Float3> pos;
    std::vector<Float2> uv;
    std::vector<Float3> nor;

    ImportedMesh curMesh{};
    curMesh.name = "OBJMesh";

    std::unordered_map<ObjKey, uint32_t, ObjKeyHash> remap;
    remap.reserve(4096);

    bool anyNormalsInFile = false;
    bool anyTriangles = false;

    auto emitVertex = [&](int vIdxRaw, int tIdxRaw, int nIdxRaw) -> uint32_t
        {
            const int vi = ResolveObjIndex(vIdxRaw, (int)pos.size());
            const int ti = (tIdxRaw != 0) ? ResolveObjIndex(tIdxRaw, (int)uv.size()) : -1;
            const int ni = (nIdxRaw != 0) ? ResolveObjIndex(nIdxRaw, (int)nor.size()) : -1;

            if (vi < 0 || vi >= (int)pos.size())
                throw std::runtime_error("OBJ face references invalid position index.");

            ObjKey key{ vi, ti, ni };
            auto it = remap.find(key);
            if (it != remap.end())
                return it->second;

            ImportedVertex v{};
            v.position = pos[vi];

            if (ti >= 0 && ti < (int)uv.size())
                v.uv = uv[ti];

            if (ni >= 0 && ni < (int)nor.size())
            {
                v.normal = nor[ni];
                anyNormalsInFile = true;
            }
            else
            {
                v.normal = { 0, 0, 0 }; // 나중에 생성 가능
            }

            uint32_t newIndex = (uint32_t)curMesh.vertices.size();
            curMesh.vertices.push_back(v);
            remap.emplace(key, newIndex);

            ExpandAABB(curMesh.bounds, v.position);
            return newIndex;
        };

    std::string line;
    try
    {
        while (std::getline(f, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            // 오브젝트/그룹: 메시 분리
            if (StartsWith(line, "o ") || StartsWith(line, "g "))
            {
                // 기존 메시가 삼각형을 갖고 있으면 push하고 새로 시작
                FinalizeMesh(out, curMesh, anyTriangles);
                anyTriangles = false;

                remap.clear();

                std::istringstream iss(line);
                std::string tag, name;
                iss >> tag;
                std::getline(iss, name);
                name = Trim(name);
                curMesh.name = name.empty() ? "OBJMesh" : name;
                continue;
            }

            if (StartsWith(line, "v "))
            {
                std::istringstream iss(line);
                std::string tag;
                Float3 p{};
                iss >> tag >> p.x >> p.y >> p.z;

                // scale
                if (options.uniformScale != 1.0f)
                    p = Mul(p, options.uniformScale);

                pos.push_back(p);
                continue;
            }

            if (StartsWith(line, "vt "))
            {
                std::istringstream iss(line);
                std::string tag;
                Float2 t{};
                iss >> tag >> t.x >> t.y;

                if (options.flipV)
                    t.y = 1.0f - t.y;

                uv.push_back(t);
                continue;
            }

            if (StartsWith(line, "vn "))
            {
                std::istringstream iss(line);
                std::string tag;
                Float3 n{};
                iss >> tag >> n.x >> n.y >> n.z;
                nor.push_back(NormalizeSafe(n));
                continue;
            }

            if (StartsWith(line, "f "))
            {
                std::istringstream iss(line);
                std::string tag;
                iss >> tag;

                std::vector<uint32_t> face;
                face.reserve(8);

                std::string tok;
                while (iss >> tok)
                {
                    int vI = 0, tI = 0, nI = 0;
                    if (!ParseFaceVertex(tok, vI, tI, nI))
                        continue;

                    uint32_t idx = emitVertex(vI, tI, nI);
                    face.push_back(idx);
                }

                if (face.size() < 3)
                    continue;

                if (face.size() == 3)
                {
                    curMesh.indices.push_back(face[0]);
                    curMesh.indices.push_back(face[1]);
                    curMesh.indices.push_back(face[2]);
                    anyTriangles = true;
                }
                else
                {
                    if (!options.triangulate)
                        throw std::runtime_error("OBJ has polygon faces (>3). Enable triangulate option.");

                    // fan triangulation: (0, i, i+1)
                    for (size_t i = 1; i + 1 < face.size(); ++i)
                    {
                        curMesh.indices.push_back(face[0]);
                        curMesh.indices.push_back(face[i]);
                        curMesh.indices.push_back(face[i + 1]);
                        anyTriangles = true;
                    }
                }
                continue;
            }

            // usemtl, mtllib 등은 최소 구현에서는 무시 (엔진 material 파이프가 아직 색만이므로)
        }
    }
    catch (const std::exception& e)
    {
        return Result<ImportedModel>::Fail(std::string("OBJ parse error: ") + e.what());
    }

    // 마지막 메시 푸시
    FinalizeMesh(out, curMesh, anyTriangles);

    if (out.meshes.empty())
        return Result<ImportedModel>::Fail("OBJ contains no valid faces: " + filePath);

    // 노말이 없거나(혹은 부분적으로 비어있으면) 생성
    if (options.generateNormalsIfMissing)
    {
        for (auto& m : out.meshes)
        {
            bool need = !anyNormalsInFile;
            if (!need)
            {
                // 파일에 vn이 있어도, 일부 정점 normal이 0이면 생성해주는 쪽이 실전에서 편함
                for (auto& v : m.vertices)
                {
                    if (std::fabs(v.normal.x) < 1e-10f &&
                        std::fabs(v.normal.y) < 1e-10f &&
                        std::fabs(v.normal.z) < 1e-10f)
                    {
                        need = true;
                        break;
                    }
                }
            }

            if (!need) continue;

            // 누적
            std::vector<Float3> acc(m.vertices.size(), Float3{ 0,0,0 });
            for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
            {
                uint32_t i0 = m.indices[i + 0];
                uint32_t i1 = m.indices[i + 1];
                uint32_t i2 = m.indices[i + 2];

                const Float3& p0 = m.vertices[i0].position;
                const Float3& p1 = m.vertices[i1].position;
                const Float3& p2 = m.vertices[i2].position;

                Float3 e1 = Sub(p1, p0);
                Float3 e2 = Sub(p2, p0);
                Float3 fn = Cross(e1, e2);

                acc[i0] = Add(acc[i0], fn);
                acc[i1] = Add(acc[i1], fn);
                acc[i2] = Add(acc[i2], fn);
            }

            for (size_t i = 0; i < m.vertices.size(); ++i)
                m.vertices[i].normal = NormalizeSafe(acc[i]);
        }
    }

    return Result<ImportedModel>::Ok(std::move(out));
}
