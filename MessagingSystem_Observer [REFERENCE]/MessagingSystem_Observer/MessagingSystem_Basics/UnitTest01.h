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

#pragma once

#include <string>
#include <map>
#include <functional>
#include <iostream>

namespace PlayingWith_MessagingSystem_Basics
{	
	//enum - not really used in this sample - to be replaced with a "std::vector<std::string> MessageIds" as a dynamic container for different messages types
	enum MessageId
	{
		MESSAGE_01 = 0,
		MESSAGE_02,
		MESSAGE_03,
		MESSAGE_NONE
	};

	class Observable;

	//dummy
	class GOC
	{
		//I am an observable
		Observable* m_observable;
		//better - std::unique_ptr<Observable> m_observable;

		//list of properties
		//...

	public:
		GOC() : m_observable(nullptr) {}
		~GOC() {}		
	};
	
	class IMessage
	{
	protected:

		std::string m_id;
		
	public:

		IMessage() : m_id("MESSAGE_NONE") {}

		virtual ~IMessage() {};

		IMessage(const std::string & id) : m_id(id) {}

		inline std::string GetId() const noexcept { return m_id; }
	};

	typedef void(*MESSAGE_HANDLER)(IMessage*);

	
	class Message01 : public IMessage
	{
	private:
		int m_data01;
		int m_data02;

	public:

		Message01(const int & data01, const int & data02) : IMessage("MESSAGE_01"), m_data01(data01), m_data02(data02) {}
		~Message01() {};

		inline int GetData01() const noexcept { return m_data01; };
		inline int GetData02() const noexcept { return m_data02; };
	};

	class Message02 : public IMessage
	{
	private:
		std::string m_data01;

	public:

		Message02(const std::string & data01) : IMessage("MESSAGE_02"), m_data01(data01) {}
		~Message02() {};

		inline std::string GetData01() const noexcept { return m_data01; };
	};

	//not really used now
	class Message03 : public IMessage
	{
	private:
		GOC* m_data01;
		GOC* m_data02;

	public:

		Message03(GOC* data01, GOC* data02) : IMessage("MESSAGE_03"), m_data01(data01), m_data02(data02) {}
		~Message03() {};
	};




	class Observer
	{
		std::map<std::string, MESSAGE_HANDLER> m_handlers;

		std::string m_id;

	public:
		Observer() : m_id("no_id") {};
		Observer(const std::string& id) : m_id(id) {};
		~Observer() {};

		void AttachHandler(const std::string & messageId, MESSAGE_HANDLER handler) { m_handlers.insert(std::pair<std::string, MESSAGE_HANDLER>(messageId, handler)); };

		inline MESSAGE_HANDLER GetHandler(const std::string & messageId) noexcept { return m_handlers[messageId]; }

		inline std::string GetId() const noexcept { return m_id; }
	};


	class Observable
	{
		//the observable will send specific message only to those subscribed to it
		//e.g. an observable might send 3 types of different messages to those whom subscribe to them
		//the registration/subscription is basically to allow us to call the specific member function of the correspondant observer

		//std::map<std::string, Observer *> messagesMap;
		//typedef std::multimap<std::string, MESSAGE_HANDLER> MessagesMap;
		typedef std::multimap<std::string, Observer*> MessagesMap;
		MessagesMap messagesMap;

		std::string m_id;

	public:
		Observable() : m_id("no_id") {}
		Observable(const std::string& id) : m_id(id) {}
		~Observable() {}

		//void Register(std::string messageId, MESSAGE_HANDLER handler) { messagesMap.insert(MessagesMap::value_type(messageId, handler)); };
		void Register(const std::string & messageId, Observer* observer) { messagesMap.insert(MessagesMap::value_type(messageId, observer)); };

		void ProcessMessage(IMessage* message)
		{
			std::cout << std::endl << std::endl;
			std::cout << "OBSERVABLE:  " << m_id << "  is sending his messages......" << std::endl << std::endl;

			std::pair<std::multimap<std::string, Observer*>::iterator, std::multimap<std::string, Observer*>::iterator> range = messagesMap.equal_range(message->GetId());
			for (auto it = range.first; it != range.second; ++it)
			{
				std::cout << "Message Processed by Observer: " << it->second->GetId() << std::endl;
				it->second->GetHandler(message->GetId())(message);//invoking the function pointed to				
			}
		};

		inline std::string GetId() const noexcept { return m_id; }
	};




	//In the following we are using the messaging system sample

	void DoSomething01(Message01* message) { std::cout << "Processing Message01 object with data01 = " << message->GetData01() << " and data02 = " << message->GetData02() << std::endl; };
	void DoSomething02(Message02* message) { std::cout << "Processing Message02 object with data01 = " << message->GetData01() << std::endl; };
	void DoSomething03(Message03* message) { message; std::cout << "Processing Message03 object" << std::endl; };

	void TestMe()
	{
		Observer observer01("observer_01");//this can be used by a GOC/system
		observer01.AttachHandler("MESSAGE_01", (MESSAGE_HANDLER)DoSomething01);

		Observer observer02("observer_02");//this can be used by a GOC/system
		observer02.AttachHandler("MESSAGE_01", (MESSAGE_HANDLER)DoSomething01);

		Observer observer03("observer_03");//this can be used by a GOC/system
		observer03.AttachHandler("MESSAGE_02", (MESSAGE_HANDLER)DoSomething02);



		//USUALLY THIS CODE IS DONE AT LOAD TIME - ONCE
		Observable observable01("observable_01");
		//observable.Register("Message01", (void(*)(IMessage *))DoSomething01);//instead of the second being an Observer*
		observable01.Register("MESSAGE_01", &observer01);
		observable01.Register("MESSAGE_01", &observer02);//we can register the same message id because it's a multimap
		Observable observable02("observable_02");
		observable02.Register("MESSAGE_02", &observer03);



		//USUALLY THIS CODE IS USED ONLY WHEN NEEDED (e.g. on triggers)
		Message01 msg01(4, 7);//custom data sent for processing
		observable01.ProcessMessage(&msg01);
		Message02 msg02("DigiPen");//custom data sent for processing
		observable02.ProcessMessage(&msg02);
	}
}
