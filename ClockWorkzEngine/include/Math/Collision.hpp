#ifndef COLLISION
#define COLLISION

#include <iostream>
#include "Types.hpp"
#include "Vec2.hpp"
#include <cmath>
#include <algorithm>


extern f64					g_fixedDT;
extern f64					g_dt;
extern f64					g_appTime;

struct AABB
{
	Vec2	min;
	Vec2	max;
};

bool CollisionIntersection_RectRect_Static(const AABB& aabb1,					//Input
	const AABB& aabb2);					//Input


bool CollisionIntersection_RectRect_Dynamic(const AABB& aabb1,					//Input
	const Vec2& vel1,					//Input 
	const AABB& aabb2,					//Input 
	const Vec2& vel2,					//Input
	float& firstTimeOfCollision);

inline bool CollisionIntersection_RectRect(const AABB& aabb1,					//Input
	const Vec2& vel1,					//Input 
	const AABB& aabb2,					//Input 
	const Vec2& vel2,					//Input
	float& firstTimeOfCollision)		//Output: return the calculated value of tFirst
{
	//Step 1
	bool staticCollision = false;
	staticCollision = CollisionIntersection_RectRect_Static(aabb1, aabb2);
	if (staticCollision)
	{
		return true;
	}

	//Step 2 until 5
	return CollisionIntersection_RectRect_Dynamic(aabb1, vel1, aabb2, vel2, firstTimeOfCollision);
}
// CSD1130_COLLISION_H_

class Collision {
public:
	

};

#endif 