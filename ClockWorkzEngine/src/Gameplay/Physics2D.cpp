#include "Gameplay/Physics2D.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Collider.hpp"


namespace Gameplay {
    void Physics2D::step(std::vector<std::unique_ptr<GameObject>>& objs, double dt) {
        (void)dt;
        // Naive AABB pair check for now; move to spatial grid later
        /*for (size_t i = 0; i < objs.size(); ++i) {
            for (size_t j = i + 1; j < objs.size(); ++j) {
                auto& A = *objs[i]; auto& B = *objs[j];
                if (!A.active() || !B.active()) continue;
                if ((A.collisionMask() & (1u << B.layer())) == 0 &&
                    (B.collisionMask() & (1u << A.layer())) == 0) continue;
                if (A.aabbWorld().intersects(B.aabbWorld())) {
                    A.onCollision(B); B.onCollision(A);
                }
            }
        }*/

        // Multiple colliders - AABB and Circle, can consider spatial grid later
        for (size_t i = 0; i < objs.size(); ++i) {
            for (size_t j = i + 1; j < objs.size(); ++j) {
                auto& A = *objs[i];
                auto& B = *objs[j];

                if (!A.active() || !B.active()) continue;

                const bool aHitsB = (A.collisionMask() & B.layer()) != 0;
                const bool bHitsA = (B.collisionMask() & A.layer()) != 0;
                if (!aHitsB && !bHitsA) continue;
				if ((A.type() == GameObjectType::Wall) && (B.type() == GameObjectType::Wall)) continue;

                for (auto& colA : A.colliders()) {
                    for (auto& colB : B.colliders()) {
                        if (!colA->solid || !colB->solid) continue;

                        bool collided = false;
                        glm::vec2 posA = A.transform().position;
                        glm::vec2 posB = B.transform().position;

                        if (colA->type == ColliderType::AABB && colB->type == ColliderType::AABB) {
                            glm::vec2 amin = colA->worldMin(posA);
                            glm::vec2 amax = colA->worldMax(posA);
                            glm::vec2 bmin = colB->worldMin(posB);
                            glm::vec2 bmax = colB->worldMax(posB);
                            collided = !(amax.x < bmin.x || amin.x > bmax.x ||
                                amax.y < bmin.y || amin.y > bmax.y);
                        }
                        else if (colA->type == ColliderType::Circle && colB->type == ColliderType::Circle) {
                            glm::vec2 diff = (posA + colA->offset) - (posB + colB->offset);
                            float dist2 = glm::dot(diff, diff);
                            float r = colA->radius() + colB->radius();
                            collided = dist2 <= r * r;
                        }
                        else {
                            // AABB vs Circle
                            const Collider* aabbCol = colA->type == ColliderType::AABB ? colA.get() : colB.get();
                            const Collider* circCol = colA->type == ColliderType::Circle ? colA.get() : colB.get();
                            glm::vec2 boxMin = aabbCol->worldMin(aabbCol == colA.get() ? posA : posB);
                            glm::vec2 boxMax = aabbCol->worldMax(aabbCol == colA.get() ? posA : posB);
                            glm::vec2 circlePos = (circCol == colA.get() ? posA : posB) + circCol->offset;
                            float r = circCol->radius();

                            glm::vec2 closest = glm::clamp(circlePos, boxMin, boxMax);
                            glm::vec2 diff = circlePos - closest;
                            float dist2 = glm::dot(diff, diff);
                            collided = dist2 <= r * r;
                        }

                        if (collided) {
                            A.onCollision(B);
                            B.onCollision(A);
                        }
                    }
                }
            }
        }

    }

    std::optional<glm::vec2> Physics2D::raycastLight(
        const glm::vec2& origin,
        const glm::vec2& dir,
        float maxDist,
        const std::vector<std::unique_ptr<GameObject>>& objs)
    {
        glm::vec2 closestHit = origin + dir * maxDist;
        float closestDist = maxDist;

        for (auto& obj : objs) {
            if (!obj->active()) continue;

            for (auto& col : obj->colliders()) {
                if (!col->blocksLight) continue;

                const glm::vec2 objPos = obj->transform().position;

                // ---------- AABB Collider ----------
                if (col->type == ColliderType::AABB) {
                    glm::vec2 boxMin = col->worldMin(objPos);
                    glm::vec2 boxMax = col->worldMax(objPos);

                    glm::vec2 invDir;
                    invDir.x = (fabs(dir.x) < 1e-6f) ? 1e6f : 1.0f / dir.x;
                    invDir.y = (fabs(dir.y) < 1e-6f) ? 1e6f : 1.0f / dir.y;

                    glm::vec2 t1 = (boxMin - origin) * invDir;
                    glm::vec2 t2 = (boxMax - origin) * invDir;

                    float tmin = std::max(0.0f, std::max(std::min(t1.x, t2.x), std::min(t1.y, t2.y)));
                    float tmax = std::min(std::max(t1.x, t2.x), std::max(t1.y, t2.y));

                    if (tmax >= tmin && tmin < closestDist) {
                        closestDist = tmin;
                        closestHit = origin + dir * tmin;
                    }
                }

                // ---------- Circle Collider ----------
                else if (col->type == ColliderType::Circle) {
                    glm::vec2 center = objPos + col->offset;
                    float r = col->radius();

                    glm::vec2 oc = origin - center;
                    float b = 2.0f * glm::dot(oc, dir);
                    float c = glm::dot(oc, oc) - r * r;
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

    struct MTV { glm::vec2 n; float d; }; // normal (from a -> b), penetration depth

    bool AABB_AABB(Gameplay::AABB const& a, Gameplay::AABB const& b, MTV& mtv) {
        glm::vec2 aCenter = 0.5f * (a.min + a.max);
        glm::vec2 bCenter = 0.5f * (b.min + b.max);
        glm::vec2 aHalf = 0.5f * (a.max - a.min);
        glm::vec2 bHalf = 0.5f * (b.max - b.min);

        glm::vec2 delta = bCenter - aCenter;
        float px = (aHalf.x + bHalf.x) - std::abs(delta.x);
        if (px <= 0) return false;
        float py = (aHalf.y + bHalf.y) - std::abs(delta.y);
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

    bool Circle_Circle(glm::vec2 ca, float ra, glm::vec2 cb, float rb, MTV& mtv) {
        glm::vec2 d = cb - ca;
        float dist2 = glm::dot(d, d);
        float r = ra + rb;
        if (dist2 >= r * r) return false;
        float dist = std::max(std::sqrt(dist2), 1e-6f);
        mtv.n = d / dist;
        mtv.d = r - dist;
        return true;
    }

    // AABB (as box center/half) vs Circle
    bool AABB_Circle(Gameplay::AABB const& box, glm::vec2 cc, float r, MTV& mtv) {
        glm::vec2 center = 0.5f * (box.min + box.max);
        glm::vec2 half = 0.5f * (box.max - box.min);
        glm::vec2 rel = cc - center;
        glm::vec2 clamped = glm::clamp(rel, -half, half);
        glm::vec2 closest = center + clamped;
        glm::vec2 diff = cc - closest;
        float d2 = glm::dot(diff, diff);
        if (d2 >= r * r) return false;

        float d = std::max(std::sqrt(d2), 1e-6f);
        mtv.n = diff / d;          // outward from box to circle
        mtv.d = r - d;
        // Edge case: circle center inside box -> choose axis of least penetration
        if (d < 1e-3f) {
            float px = half.x - std::abs(rel.x);
            float py = half.y - std::abs(rel.y);
            if (px < py) mtv.n = { (rel.x < 0 ? -1.f : 1.f), 0.f }, mtv.d = px;
            else         mtv.n = { 0.f, (rel.y < 0 ? -1.f : 1.f) }, mtv.d = py;
        }
        return true;
    }

    // Conservative positional correction (prevents sinking/jitter)
    inline void positionalCorrection(Gameplay::GameObject& a, Gameplay::GameObject& b,
        MTV const& mtv, bool aMovable, bool bMovable) {
        static constexpr float percent = 0.8f; // 80% of penetration
        static constexpr float slop = 0.01f;
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
        // mark bounds dirty
        a.markDirty(); b.markDirty();
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
