// ---------------------------------------------------------------------------
// includes
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include <iostream>
#include "AEVec2.h"
#include <math.h>
#include "Side.hpp"
// ---------------------------------------------------------------------------
// main
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	int gGameRunning = 1;
	// Initialization of your own variables go here
	u32 black{ 0xFF000000 }, red{ 0xFFFF0000 }, green{ 0xFF00FF00 }, blue{ 0xFF0000FF }, white{ 0xFFFFFFFF }, transparent{ 0x00000000 }, grey{ 0xFF808080 };
	AEVec2 posPlayer{ 0.f, 0.f }, posLeftObj{ -450.f, -150.f }, posRightObj{ 450.f, -150.f }; //How is this possible? !!!!!!!!!!
	f32 radPlayer{ 50.f }, radObj{ 100.f };
	u32 pHealth{ 100 };
	// Using custom window procedure
	AESysInit(hInstance, nCmdShow, 1600, 900, 1, 60, false, NULL);
	// Changing the window title
	AESysSetWindowTitle("My New Demo!");
	// reset the system modules
	AESysReset();
	printf("Hello World\n");
	AEGfxMeshStart();
	AEGfxVertexAdd(0.5f, 0.5f, blue, 0.0f, 0.0f);
	AEGfxVertexAdd(-0.5f, 0.5f, blue, 0.0f, 0.0f);
	AEGfxVertexAdd(0.0f, 0.0f, blue, 0.0f, 0.0f);
	AEGfxVertexList* triangle = AEGfxMeshEnd();
	AEGfxVertexList* squareMesh = CreatePolygonMesh(4, transparent);
	AEGfxVertexList* circleMesh = CreatePolygonMesh(100, transparent);
	//AEGfxTexture* pTex = AEGfxTextureLoad("Assets/PlanetTexture.png");
	//s8 pFont = AEGfxCreateFont("Assets/liberation-mono.ttf", 72);
	//AEAudio ore = AEAudioLoadSound("Assets/ore.mp3");
	//AEAudioGroup sound_effect = AEAudioCreateGroup();
	////AEAudioPlay(ore, sound_effect, 1.f, 1.f, -1);
	//AEAudio bouken = AEAudioLoadMusic("Assets/bouken.mp3");
	//AEAudioGroup bgm = AEAudioCreateGroup();
	//AEAudioPlay(bouken, bgm, 1.f, 1.f, -1);
	//This statement is optional.
	//AEGfxSetCamPosition(0, 0);
	//AEGfxGetCamPosition();
	//AEMtx33 scale = { 0 };
	//AEMtx33 rotate = { 0 };
	//AEMtx33 translate = { 0 };
	// Game Loop
	while (gGameRunning)
	{
		// Informing the system about the loop's start
		AESysFrameStart();
		// Basic way to trigger exiting the application
		// when ESCAPE is hit or when the window is closed
		if (AEInputCheckTriggered(AEVK_ESCAPE) || 0 == AESysDoesWindowExist())
			gGameRunning = 0;
		// Your own update logic goes here
		AEMtx33 scale{ 0 }, rotate{ 0 }, translate{ 0 }, transform{ 0 }; //why can't i put 		AEMtx33 scale = { 0 }; again !!!!!!!!!
		HandlePlayerInput(posPlayer); // ### [FUNCTION] ###   ###   ###   ###   ###   ###   ###
		// Your own rendering logic goes here
		AEGfxSetBackgroundColor(0.5f, 0.5f, 0.5f);
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		//These two statements are default.
		/*AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
		AEGfxSetColorToAdd(0.0f, 0.0f, 0.0f, 0.0f);*/
		//These two statements come together.
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxSetTransparency(1.0f);
		//This statement is not needed.
		//AEGfxTextureSet(pTex, 0, 0);
		AEGfxSetTransform(transform.m);
		AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);
		AEMtx33Scale(&scale, radObj, radObj);
		AEMtx33Rot(&rotate, 0);
		AEMtx33Trans(&translate, posLeftObj.x, posLeftObj.y);
		AEMtx33Concat(&transform, &rotate, &scale);
		AEMtx33Concat(&transform, &translate, &transform);
		AEGfxSetTransform(transform.m);
		AEGfxSetColorToAdd(0.0f, 1.0f, 0.0f, 1.0f);
		AEGfxMeshDraw(circleMesh, AE_GFX_MDM_TRIANGLES);
		AEMtx33Trans(&translate, posRightObj.x, posRightObj.y);
		AEMtx33Concat(&transform, &rotate, &scale);
		AEMtx33Concat(&transform, &translate, &transform);
		AEGfxSetTransform(transform.m);
		AEGfxSetColorToAdd(1.0f, 0.0f, 0.0f, 1.0f);
		AEGfxMeshDraw(circleMesh, AE_GFX_MDM_TRIANGLES);
		AEMtx33Scale(&scale, radPlayer, radPlayer);
		AEMtx33Trans(&translate, posPlayer.x, posPlayer.y);
		AEMtx33Concat(&transform, &rotate, &scale);
		AEMtx33Concat(&transform, &translate, &transform);
		AEGfxSetTransform(transform.m);
		AEGfxSetColorToAdd(1.0f, 1.0f, 1.0f, 1.0f);
		AEGfxMeshDraw(circleMesh, AE_GFX_MDM_TRIANGLES);
		//const char* pText = "Hello World";
		//f32 width, height;
		//AEGfxGetPrintSize(pFont, pText, 1.f, &width, &height);
		//AEGfxPrint(pFont, pText, -width / 2, -height / 2, 1, 1, 1, 1, 1);
		s32 x, y;
		AEInputGetCursorPosition(&x, &y); // ### [FUNCTION] ###   ###   ###   ###   ###   ###
		std::cout << x << ' ' << y << '\n';
		//std::cout << AEFrameRateControllerGetFrameRate() << '\n'; //~60fps
		if (AECalcDistCircleToCircle(&posPlayer, radPlayer, &posLeftObj, radObj) < 0)
			if (pHealth < 100) {
				pHealth += 1;
			}
		if (AECalcDistCircleToCircle(&posPlayer, radPlayer, &posRightObj, radObj) < 0)
			if (pHealth > 0) {
				pHealth -= 1;
			}
		/*std::cout << "Current  step HP: " << ((pHealth + 9) / 10) << '\n';
		std::cout << "Current  step HP: " << pHealth << '\n';*/
		AEMtx33Scale(&scale, 1000.f * AECosDeg(45), 100.f * AESinDeg(45));	//Healthbar bg
		AEMtx33Trans(&translate, 0.f, 350.f);
		AEMtx33Concat(&transform, &rotate, &scale);
		AEMtx33Concat(&transform, &translate, &transform);
		AEGfxSetTransform(transform.m);
		AEGfxSetColorToAdd(1.0f, 1.0f, 1.0f, 1.0f);
		AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);
		RenderHealthBar(squareMesh, pHealth, { -500.0f * (100 - pHealth) / 100, 350.0f }); // ### [FUNCTION] ###   ###   ###   ###   ###   ###
		AEMtx33Scale(&scale, 90.f * AECosDeg(45), 50.f * AECosDeg(45)); //not 100.f, to leave a gap
		for (u32 i{ 0 }; i < ((pHealth + 9) / 10); i++) {
			AEMtx33Trans(&translate, -450.f + i * 100.f, 250.f); //Health
			AEMtx33Concat(&transform, &rotate, &scale);
			AEMtx33Concat(&transform, &translate, &transform);
			AEGfxSetTransform(transform.m);
			AEGfxMeshDraw(squareMesh, AE_GFX_MDM_TRIANGLES);
		}
		// Informing the system about the loop's end
		AESysFrameEnd();
	}
	AEGfxMeshFree(squareMesh);
	AEGfxMeshFree(circleMesh);
	/*AEGfxTextureUnload(pTex);
	AEGfxDestroyFont(pFont);
	AEAudioUnloadAudio(bouken);
	AEAudioUnloadAudio(ore);
	AEAudioUnloadAudioGroup(sound_effect);
	AEAudioUnloadAudioGroup(bgm);*/
	// free the system
	AESysExit();
}