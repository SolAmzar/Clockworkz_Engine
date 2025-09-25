//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//About "PlayingWith_Messagingsystem_Basics" unit test
//Demonstrates a simple/simplified C/C++ messaging system
//Following Observer Design pattern
//		-Observer(s)/Observable(s)
//This sample is not using:
//		-A real time test with high frequency game loop
//		-Delegates (or member function pointers) - currently the Handlers are global functions
//		-Queues to postpone the processing of the messages (which may not be needed in all implementations)
//Author - Elie Hosry
//		DigiPen (c) 2019
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "UnitTest01.h"

int main()
{
	std::cout << "START : " << __func__ << std::endl;
	std::cout << std::endl;

	PlayingWith_MessagingSystem_Basics::TestMe();

	std::cout << std::endl;
	std::cout << "END : " << __func__ << std::endl;

	return 0;
}