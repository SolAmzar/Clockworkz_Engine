#include "Gameplay/Physics2D.hpp"
#include "Gameplay/GameObject.hpp"

namespace Gameplay {
    void Physics2D::step(std::vector<std::unique_ptr<GameObject>>& objs, double dt) {
        (void)dt;
        // Naive AABB pair check for now; move to spatial grid later
        for (size_t i = 0; i < objs.size(); ++i) {
            for (size_t j = i + 1; j < objs.size(); ++j) {
                auto& A = *objs[i]; auto& B = *objs[j];
                if (!A.active() || !B.active()) continue;
                if ((A.collisionMask() & (1u << B.layer())) == 0 &&
                    (B.collisionMask() & (1u << A.layer())) == 0) continue;
                if (A.aabbWorld().intersects(B.aabbWorld())) {
                    A.onCollision(B); B.onCollision(A);
                }
            }
        }
    }
}
