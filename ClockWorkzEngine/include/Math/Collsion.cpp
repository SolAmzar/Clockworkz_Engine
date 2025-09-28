/* Start Header ************************************************************************/
/*!
\file		AABB_Collision.cpp
\author		Muhamad Amzar Bin Mohamad Zamre
\par		muhamadamzar.b@digipen.edu
\date		March, 9, 2025
\brief		This file contains the 2 functions for static and dynamic collision

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "Collision.hpp"

// ----------------------------------------------------------------------------
//
// Globals
//
// ----------------------------------------------------------------------------
// fixed dt value to force it
f64					g_fixedDT = 0.01667;

// stores the game loop time that you must use in all your physics calculations
f64					g_dt = g_fixedDT;

// stores to total application time until the current loop
f64					g_appTime = 0.0;


// ----------------------------------------------------------------------------
//
// Static AABB collision:
// AABB collision between two rectangles that are possibly already overlapping
// This is a rigid check, considering both rectangles are not moving
// If this function finds intersection between the two rectangles, there will be
// no need to call the dynamic collision check.
// return true if there is overlap.
//
// ----------------------------------------------------------------------------
bool CollisionIntersection_RectRect_Static(const AABB& aabb1,				//Input
	const AABB& aabb2)				//Input
{
	/*
	Implement the dynamic collision intersection over here.

	The steps "from the lecture slides" are :

	Step 1 : Check for static collision detection between rectangles(static : before moving).
		If the check returns no overlap, you continue with the dynamic collision test
		with the subsequent next steps 2 to 5 (dynamic : with velocities).
		Otherwise you return collision is true, and you stop.
	*/

	if (aabb1.max.x < aabb2.min.x)
		return 0;
	if (aabb1.min.x > aabb2.max.x)
		return 0;
	if (aabb1.max.y < aabb2.min.y)
		return 0;
	if (aabb1.min.y > aabb2.max.y)
		return 0;
	return 1;

	//UNREFERENCED_PARAMETER(aabb1);
	//UNREFERENCED_PARAMETER(aabb2);
}

// ----------------------------------------------------------------------------
//
// Dynamic AABB collision:
// AABB collision between two rectangles, that we already know they are not 
// overlapping.
// Both rectangles may have velocities and the function must return the first
// time of intersection if there is any.
// return true if there is intersection.
//
// ----------------------------------------------------------------------------
bool CollisionIntersection_RectRect_Dynamic(const AABB& aabb1,				//Input
	const Vec2& vel1,				//Input 
	const AABB& aabb2,				//Input 
	const Vec2& vel2,				//Input
	float& firstTimeOfCollision)	//Output: the calculated value of tFirst
{
	/*
	Implement the dynamic collision intersection over here.

	The steps "from the lecture slides" are:

	Step 2: Initialize and calculate the new "relative" velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect
	*/

	/*UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);*/

	Vec2 relativeVel{ vel2.x - vel1.x, vel2.y - vel1.y };
	f32 tFirst = 0, tLast = (f32)g_dt;

	// Check collision on X axis
	if (relativeVel.x < 0)
	{
		if (aabb1.min.x > aabb2.max.x)
			return 0;
		if (aabb1.max.x < aabb2.min.x)
		{
			tFirst = std::max(tFirst, (aabb1.max.x - aabb2.min.x) / relativeVel.x);
		}
		if (aabb1.min.x < aabb2.max.x)
		{
			tLast = std::min(tLast, (aabb1.min.x - aabb2.max.x) / relativeVel.x);
		}

	}
	else if (relativeVel.x > 0)
	{
		if (aabb1.min.x > aabb2.max.x)
		{
			tFirst = std::max(tFirst, (aabb1.min.x - aabb2.max.x) / relativeVel.x);
		}
		if (aabb1.max.x > aabb2.min.x)
		{
			tLast = std::min(tLast, (aabb1.max.x - aabb2.min.x) / relativeVel.x);
		}
		if (aabb1.max.x < aabb2.min.x)
		{
			return 0;
		}
	}
	else //if(relativeVel.x == 0)
	{
		if (aabb1.max.x < aabb2.min.x)
			return 0;
		else if (aabb1.min.x > aabb2.max.x)
			return 0;
	}
	if (tFirst > tLast)
		return 0;


	// Check collision on Y axis
	if (relativeVel.y < 0)
	{
		if (aabb1.min.y > aabb2.max.y)
			return 0;
		if (aabb1.max.y < aabb2.min.y)
		{
			tFirst = std::max(tFirst, (aabb1.max.y - aabb2.min.y) / relativeVel.y);
		}
		if (aabb1.min.y < aabb2.max.y)
		{
			tLast = std::min(tLast, (aabb1.min.y - aabb2.max.y) / relativeVel.y);
		}

	}
	else if (relativeVel.y > 0)
	{
		if (aabb1.min.y > aabb2.max.y)
		{
			tFirst = std::max(tFirst, (aabb1.min.y - aabb2.max.y) / relativeVel.y);
		}
		if (aabb1.max.y > aabb2.min.y)
		{
			tLast = std::min(tLast, (aabb1.max.y - aabb2.min.y) / relativeVel.y);
		}
		if (aabb1.max.y < aabb2.min.y)
		{
			return 0;
		}
	}
	else //if(relativeVel.y == 0)
	{
		if (aabb1.max.y < aabb2.min.y)
			return 0;
		else if (aabb1.min.y > aabb2.max.y)
			return 0;
	}
	if (tFirst > tLast)
		return 0;

	firstTimeOfCollision = tFirst;
	return true;
}