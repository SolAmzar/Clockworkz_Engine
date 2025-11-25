#ifndef 0

#include "Types.hpp"	
#include "Collision.hpp"

typedef struct AEGfxVertexBuffer AEGfxVertexBuffer;

typedef struct AEGfxVertexList
{
	AEGfxVertexBuffer* mpVtxBuffer;
	u32						vtxNum;
}AEGfxVertexList;


struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList*    pMesh;		// This will hold the triangles which will form the shape of the object
};


struct GameObjInst
{
	GameObj* pObject;						// pointer to the 'original'
	unsigned int	flag;					// bit flag or-ed together
	Vec2			scale;					// scaling value of the object instance
	Vec2			posCurr;				// object current position
	Vec2			velCurr;				// object current velocity
	float			dirCurr;				// object current direction

	Vec2			posPrev;				// object previous position -> it's the position calculated in the previous loop

	//AEMtx33			transform;				// object transformation matrix: Each frame, 
	// calculate the object instance's transformation matrix and save it here.

	AABB			boundingBox;			// object bouding box that encapsulates the object

	int				gridCollisionFlag;		// used to hold the current object's collision flag status

	void* pUserData;						// pointer to custom data specific for each object type

	enum			STATE state;			// state of the object instance
	enum			INNER_STATE innerState;	// inner State of the object instance		
	double			idle_counter;			// general purpose counter (this variable will be used in the enemy state machine for the idle time counting)

	//For Particle System
	float			lifetime;				// particle's lifetime value if this instance is a particle
};

#endif