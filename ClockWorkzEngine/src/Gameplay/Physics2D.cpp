#include "Gameplay/Physics2D.hpp"
#include "Gameplay/Collider.hpp"
#include "Gameplay/GameObject.hpp"
#include <algorithm>

namespace Gameplay {
    //void Physics2D::step(std::vector<std::unique_ptr<GameObject>>& objs, double dt) {
    //    (void)dt;
    //    // Naive AABB pair check for now; move to spatial grid later
    //    /*for (size_t i = 0; i < objs.size(); ++i) {
    //        for (size_t j = i + 1; j < objs.size(); ++j) {
    //            auto& A = *objs[i]; auto& B = *objs[j];
    //            if (!A.active() || !B.active()) continue;
    //            if ((A.collisionMask() & (1u << B.layer())) == 0 &&
    //                (B.collisionMask() & (1u << A.layer())) == 0) continue;
    //            if (A.aabbWorld().intersects(B.aabbWorld())) {
    //                A.onCollision(B); B.onCollision(A);
    //            }
    //        }
    //    }*/

    //    // Multiple colliders - AABB and Circle, can consider spatial grid later
    //    for (size_t i = 0; i < objs.size(); ++i) {
    //        for (size_t j = i + 1; j < objs.size(); ++j) {
    //            auto& A = *objs[i];
    //            auto& B = *objs[j];

    //            if (!A.active() || !B.active()) continue;

    //            const bool aHitsB = (A.collisionMask() & B.layer()) != 0;
    //            const bool bHitsA = (B.collisionMask() & A.layer()) != 0;
    //            if (!aHitsB && !bHitsA) continue;
				//if ((A.type() == GameObjectType::Wall) && (B.type() == GameObjectType::Wall)) continue;

    //            for (auto& colA : A.colliders()) {
    //                for (auto& colB : B.colliders()) {
    //                    if (!colA->solid || !colB->solid) continue;

    //                    bool collided = false;
    //                    vec2 posA = A.transform().position;
    //                    vec2 posB = B.transform().position;

    //                    if (colA->type == ColliderType::AABB && colB->type == ColliderType::AABB) {
    //                        vec2 amin = colA->worldMin(posA);
    //                        vec2 amax = colA->worldMax(posA);
    //                        vec2 bmin = colB->worldMin(posB);
    //                        vec2 bmax = colB->worldMax(posB);
    //                        collided = !(amax.x < bmin.x || amin.x > bmax.x ||
    //                            amax.y < bmin.y || amin.y > bmax.y);
    //                    }
    //                    else if (colA->type == ColliderType::Circle && colB->type == ColliderType::Circle) {
    //                        vec2 diff = (posA + colA->offset) - (posB + colB->offset);
    //                        float dist2 = dot(diff, diff);
    //                        float r = colA->radius() + colB->radius();
    //                        collided = dist2 <= r * r;
    //                    }
    //                    else {
    //                        // AABB vs Circle
    //                        const Collider* aabbCol = colA->type == ColliderType::AABB ? colA.get() : colB.get();
    //                        const Collider* circCol = colA->type == ColliderType::Circle ? colA.get() : colB.get();
    //                        vec2 boxMin = aabbCol->worldMin(aabbCol == colA.get() ? posA : posB);
    //                        vec2 boxMax = aabbCol->worldMax(aabbCol == colA.get() ? posA : posB);
    //                        vec2 circlePos = (circCol == colA.get() ? posA : posB) + circCol->offset;
    //                        float r = circCol->radius();

    //                        vec2 closest = glm::clamp(circlePos, boxMin, boxMax);
    //                        vec2 diff = circlePos - closest;
    //                        float dist2 = dot(diff, diff);
    //                        collided = dist2 <= r * r;
    //                    }

    //                    if (collided) {
    //                        A.onCollision(B);
    //                        B.onCollision(A);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //}

    std::optional<vec2> Physics2D::raycastLight(
        const vec2& origin,
        const vec2& dir,
        float maxDist,
        const std::vector<std::unique_ptr<GameObject>>& objs)
    {
        vec2 closestHit = origin + dir * maxDist;
        float closestDist = maxDist;

        for (auto& obj : objs) {
            if (!obj->active()) continue;

            for (auto& col : obj->colliders()) {
                if (!col->blocksLight) continue;

                const vec2 objPos = obj->transform().position;

                // ---------- AABB Collider ----------
                if (col->type == ColliderType::AABB) {
                    vec2 boxMin = col->worldMin(objPos);
                    vec2 boxMax = col->worldMax(objPos);

                    vec2 invDir;
                    invDir.x = (fabs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
                    invDir.y = (fabs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;

                    vec2 t1 = (boxMin - origin) * invDir;
                    vec2 t2 = (boxMax - origin) * invDir;

                    float tmin = std::max(0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
                    float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));

                    if (tmax >= tmin && tmin < closestDist) {
                        closestDist = tmin;
                        closestHit = origin + dir * tmin;
                    }
                }

                // ---------- Circle Collider ----------
                else if (col->type == ColliderType::Circle) {
                    vec2 center = objPos + col->offset;
                    float r = col->radius();

                    vec2 oc = origin - center;
                    float b = 2.0f * dot(oc, dir);
                    float c = dot(oc, oc) - r * r;
                    float discriminant = b * b - 4.0f * c;

                    if (discriminant < 0.0f) continue; // no hit

                    float sqrtD = sqrtf(discriminant);
                    // Smaller positive root
                    float t1 = (-b - sqrtD) * 0.5f;

                    if (t1 > 0.0f && t1 < closestDist) {
                        closestDist = t1;
                        closestHit = origin + dir * t1;
                    }
                }
            }
        }

        if (closestDist < maxDist)
            return closestHit;
        return std::nullopt;
    }

    namespace {
        inline vec2 vmin(vec2 a, vec2 b) { return { std::min(a.x,b.x), std::min(a.y,b.y) }; }
        inline vec2 vmax(vec2 a, vec2 b) { return { std::max(a.x,b.x), std::max(a.y,b.y) }; }
    }

    void SpatialHash::insert(GameObject* obj) {
        AABB aabb = obj->worldAABB();
        CellKey minK = key(aabb.min);
        CellKey maxK = key(aabb.max);
        for (int cy = minK.y; cy <= maxK.y; ++cy)
            for (int cx = minK.x; cx <= maxK.x; ++cx)
                m_cells[{cx, cy}].push_back(obj);
    }

    template <typename F>
    void SpatialHash::forNeighbors(GameObject* obj, F&& func) const {
        AABB aabb = obj->worldAABB();
        CellKey minK = key(aabb.min);
        CellKey maxK = key(aabb.max);
        for (int cy = minK.y; cy <= maxK.y; ++cy)
            for (int cx = minK.x; cx <= maxK.x; ++cx) {
                CellKey k{ cx, cy };
                auto it = m_cells.find(k);
                if (it == m_cells.end()) continue;
                for (GameObject* cand : it->second) func(cand);
            }
    }

    // ---------- Narrowphase helpers ----------
    namespace {

        struct MTV { vec2 n; float d; }; // normal (from A->B), penetration depth

        bool AABB_AABB(AABB const& a, AABB const& b, MTV& mtv) {
            vec2 aCenter = (a.min + a.max) * 0.5f;
            vec2 bCenter = (b.min + b.max) * 0.5f;
            vec2 aHalf = (a.max - a.min) * 0.5f;
            vec2 bHalf = (b.max - b.min) * 0.5f;

            vec2 delta = { bCenter.x - aCenter.x, bCenter.y - aCenter.y };
            float px = (aHalf.x + bHalf.x) - std::fabs(delta.x);
            if (px <= 0) return false;
            float py = (aHalf.y + bHalf.y) - std::fabs(delta.y);
            if (py <= 0) return false;

            if (px < py) {
                mtv.n = { (delta.x < 0 ? -1.f : 1.f), 0.f };
                mtv.d = px;
            }
            else {
                mtv.n = { 0.f, (delta.y < 0 ? -1.f : 1.f) };
                mtv.d = py;
            }
            return true;
        }

        bool Circle_Circle(vec2 ca, float ra, vec2 cb, float rb, MTV& mtv) {
            vec2 d = { cb.x - ca.x, cb.y - ca.y };
            float r = ra + rb;
            float dist2 = dot(d, d);                 // your dot(vec2,vec2)
            if (dist2 >= r * r) return false;
            float dist = std::max(std::sqrt(dist2), Math::Epsilon);
            mtv.n = (1.0f / dist) * d;               // normalize(d) without tiny divide
            mtv.d = r - dist;
            return true;
        }

        bool AABB_Circle(AABB const& box, vec2 cc, float r, MTV& mtv) {
            vec2 center = (box.min + box.max) * 0.5f;
            vec2 half = (box.max - box.min) * 0.5f;
            vec2 rel = { cc.x - center.x, cc.y - center.y };

            auto clampf = [](float v, float lo, float hi) {
                return std::max(lo, std::min(v, hi));
                };
            vec2 clamped = { clampf(rel.x, -half.x, half.x),
                             clampf(rel.y, -half.y, half.y) };

            vec2 closest = { center.x + clamped.x, center.y + clamped.y };
            vec2 diff = { cc.x - closest.x, cc.y - closest.y };
            float d2 = dot(diff, diff);
            if (d2 >= r * r) return false;

            float d = std::max(std::sqrt(d2), Math::Epsilon);
            mtv.n = (1.0f / d) * diff;
            mtv.d = r - d;

            // Center inside box -> push along least-penetration axis
            if (d < 1e-4f) {
                float px = half.x - std::fabs(rel.x);
                float py = half.y - std::fabs(rel.y);
                if (px < py) { mtv.n = { (rel.x < 0 ? -1.f : 1.f), 0.f }; mtv.d = px; }
                else { mtv.n = { 0.f, (rel.y < 0 ? -1.f : 1.f) }; mtv.d = py; }
            }
            return true;
        }

        inline void positionalCorrection(GameObject& a, GameObject& b,
            MTV const& mtv, bool aMovable, bool bMovable) {
            constexpr float percent = 0.8f; // move 80% to avoid jitter
            constexpr float slop = 0.01f;
            float corr = std::max(mtv.d - slop, 0.f) * percent;

            if (aMovable && bMovable) {
                a.transform().position -= 0.5f * corr * mtv.n;
                b.transform().position += 0.5f * corr * mtv.n;
            }
            else if (aMovable) {
                a.transform().position -= corr * mtv.n;
            }
            else if (bMovable) {
                b.transform().position += corr * mtv.n;
            }
            a.markDirty(); b.markDirty();
        }

        inline bool canCollide(GameObject* a, GameObject* b);
        inline bool movable(GameObject* g) { return true; } // hook to your solid/static flags

    } // anonymous

    namespace {
        inline bool canCollide(GameObject* a, GameObject* b) {
            if (a == b) return false;
            if (!a->active() || !b->active()) return false;
            // layer/mask filter:
            if ((a->collisionMask() & b->layer()) == 0) return false;
            if ((b->collisionMask() & a->layer()) == 0) return false;
            return true;
        }

    }

    // ---------- Physics2D::step ----------
    void Physics2D::step(std::vector<std::unique_ptr<GameObject>>& objs, double dt) {
        (void)dt;

        // 1) Ensure AABBs are cached
        for (auto& u : objs) { (void)u->worldAABB(); }

        // 2) Build grid
        constexpr float kCell = 128.f; // tune to your scale
        SpatialHash grid{ kCell };
        grid.clear();
        for (auto& u : objs) {
            if (!u->active()) continue;
            grid.insert(u.get());
        }

        // 3) Pair testing via grid neighbors
        for (size_t i = 0; i < objs.size(); ++i) {
            GameObject* a = objs[i].get();
            if (!a->active()) continue;

            grid.forNeighbors(a, [&](GameObject* b) {
                if (b <= a) return;                 // unique pairs
                if (!canCollide(a, b)) return;

                AABB wa = a->worldAABB();
                AABB wb = b->worldAABB();
                MTV mtv{0.f};
                if (!AABB_AABB(wa, wb, mtv)) return;

                // Pull collider types
                CircleCollider const* ca = nullptr; AABBCollider const* aa = nullptr;
                CircleCollider const* cb = nullptr; AABBCollider const* ab = nullptr;
                for (auto const& c : a->colliders()) {
                    if (c->type == ColliderType::Circle) ca = static_cast<CircleCollider const*>(c.get());
                    if (c->type == ColliderType::AABB)   aa = static_cast<AABBCollider   const*>(c.get());
                }
                for (auto const& c : b->colliders()) {
                    if (c->type == ColliderType::Circle) cb = static_cast<CircleCollider const*>(c.get());
                    if (c->type == ColliderType::AABB)   ab = static_cast<AABBCollider   const*>(c.get());
                }

                bool hit = true; MTV best = mtv;
                if (ca && cb) {
                    MTV m{};
                    hit = Circle_Circle(a->transform().position, ca->radius(),
                        b->transform().position, cb->radius(), m);
                    if (hit) best = m;
                }
                else if (ca && ab) {
                    MTV m{};
                    hit = AABB_Circle(wb, a->transform().position, ca->radius(), m);
                    if (hit) { best = m; best.n = (-1.0f) * best.n; } // ensure A->B
                }
                else if (aa && cb) {
                    MTV m{};
                    hit = AABB_Circle(wa, b->transform().position, cb->radius(), m);
                    if (hit) best = m;
                }
                else {
                    // AABB–AABB MTV already computed
                }

                if (!hit) return;

                bool aMov = movable(a);
                bool bMov = movable(b);
                positionalCorrection(*a, *b, best, aMov, bMov);

                // TODO: add bounce/slide with your velocity fields if needed
                });
        }
    }

    

    //std::optional<glm::vec2> Physics2D::raycastLight(
    //    const glm::vec2& origin,
    //    const glm::vec2& dir,
    //    float maxDist,
    //    const std::vector<std::unique_ptr<GameObject>>& objs)
    //{
    //    glm::vec2 closestHit = origin + dir * maxDist;
    //    float closestDist = maxDist;

    //    for (auto& obj : objs) {
    //        if (!obj->active() || !obj->blocksLight()) continue;

    //        AABB box = obj->aabbWorld();

    //        // Ray vs AABB (Slab method)
    //        glm::vec2 invDir;
    //        invDir.x = (fabs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
    //        invDir.y = (fabs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;

    //        glm::vec2 t1 = (box.min - origin) * invDir;
    //        glm::vec2 t2 = (box.max - origin) * invDir;

    //        float tmin = std::max(0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
    //        float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));

    //        if (tmax >= tmin && tmin < closestDist) {
    //            closestDist = tmin;
    //            closestHit = origin + dir * tmin;
    //        }
    //    }

    //    if (closestDist < maxDist)
    //        return closestHit;
    //    return std::nullopt;
    //}

}
