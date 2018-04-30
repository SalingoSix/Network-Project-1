#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <conio.h>
#include <algorithm>
//#include <thread>
//#include <future>

#include "buffer.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

bool shutIt = false;

//Buffers we'll be storing data in
buffer sendBuffer(512);
buffer recvBuffer(512);

//A list of rooms the user is in, so we don't waste server requests doing things we shouldn't
std::vector<std::string> rooms;

bool processMessage(std::string userMessage);
bool joinRoom(std::string);
bool leaveRoom(std::string);
bool checkRoom(std::string);
bool sendMessage(std::string, std::string);
void recvMessage();

//Socket is now global
SOCKET ConnectSocket = INVALID_SOCKET;
int iResult;
//For user input using _kbhit
std::string userIn = "";
char keyIn;

int getIntegerFromInputStream(std::istream & is)
{
	std::string input;
	std::getline(is, input);

	// C++11 version
	return stoi(input);
	// throws on failure

	// C++98 version
	/*
	std::istringstream iss(input);
	int i;
	if (!(iss >> i)) {
	// handle error somehow
	}
	return i;
	*/
}

int __cdecl main(int argc, char **argv)
{
	using namespace std::literals;
	WSADATA wsaData;
	
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;


	rooms.resize(20);

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL;ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	u_long iMode = 1;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);
	// Receive until the peer closes the connection
		do 
		{
			if (_kbhit()) 
			{
				char keyIn;
				keyIn = _getch();
				if (keyIn == '\r')
				{
					std::cout << '\r';
					for (int i = 0; i < userIn.length(); i++)
						std::cout << ' ';
					std::cout << '\r';
					shutIt = !processMessage(userIn);
					userIn = "";
				}
				else if (keyIn == 127 || keyIn == 8) //backspace OR delete
				{
					userIn = userIn.substr(0, userIn.length() - 1);
					std::cout << keyIn << ' ' << keyIn;
				}
				else
				{
					userIn += keyIn;
					std::cout << keyIn;
				}
			}

			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0)
			{
				std::string getData = "";
				for (int i = 0; i < 4; i++)
					getData += recvbuf[i];
				recvBuffer.writeString(getData);
				int recvSize = recvBuffer.readInt32BE();
				for (int i = 4; i < recvSize; i++)
					getData += recvbuf[i];
				recvBuffer.writeString(getData);

				recvMessage();

			}
			else if (iResult == 0)
				//printf("Connection closed\n");
				continue;
		} while (!shutIt);

		// cleanup
		closesocket(ConnectSocket);
		WSACleanup();

		return 0;
}

bool processMessage(std::string userMessage)
{

	std::vector<std::string> result;
	std::istringstream iss(userMessage);
	std::string s;
	iss >> s;
	if (s == "/join")
		return(joinRoom(userMessage));
	else if (s == "/leave")
		leaveRoom(userMessage);
	else if (s == "/send")
	{
		iss >> s;
		if (checkRoom(s))
			sendMessage(s, userMessage);
		else
			std::cout << "Not a part of the room " << s << '\n';
	}
	else if (s == "/logout") {
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}
	else
	{
		std::cout << "Invalid command. Begin all lines with /join, /leave, /send followed by your room name, or /logout\n";
	}
	return 1;
}

bool joinRoom(std::string input)
{
	input.erase(0, 6); //Removes "/join " from the beginning of the string

	for (int i = 0; i < input.length(); i++)
		if (input[i] == ' ')
			input[i] = '_';

	if (checkRoom(input))
	{
		std::cout << "Already part of the room " << input << '\n';
		return 1;
	}
	//std::cout << "Joining the room " << input << '\n';

	int roomLength = input.length();
	int packetID = 1;
	int packetLength = roomLength + 12;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(packetID);
	sendBuffer.writeInt32BE(roomLength);
	sendBuffer.writeString(input);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	rooms.push_back(input);
	return 1;
}

bool leaveRoom(std::string input)
{
	input.erase(0, 7);

	for (int i = 0; i < input.length(); i++)
		if (input[i] == ' ')
			input[i] = '_';

	if (checkRoom(input))
	{
		//std::cout << "Leaving the room " << input << '\n';

		int roomLength = input.length();
		int packetID = 2;
		int packetLength = roomLength + 12;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(packetID);
		sendBuffer.writeInt32BE(roomLength);
		sendBuffer.writeString(input);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 0;
		}

		rooms.erase(std::remove(rooms.begin(), rooms.end(), input), rooms.end());
		return 1;
	}
	std::cout << "You are not in the room " << input << '\n';
	return 1;
}

bool checkRoom(std::string input)
{
	for (std::vector<std::string>::iterator it = rooms.begin(); it != rooms.end(); ++it)
		if (*it == input)
			return 1;

	return 0;
}

bool sendMessage(std::string room, std::string input)
{
	int eraseHead = room.length() + 7; // "/send RoomName " to be removed
	input.erase(0, eraseHead);

	int roomLength = room.length();
	int messageLength = input.length();
	int messageID = 3;
	int packetLength = roomLength + messageLength + 16;
	
	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(roomLength);
	sendBuffer.writeString(room);
	sendBuffer.writeInt32BE(messageLength);
	sendBuffer.writeString(input);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

void recvMessage()
{
	int packetLength = recvBuffer.readInt32BE();
	int messageID = recvBuffer.readInt32BE();
	if (messageID == 1)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		std::cout << "Joined room " << roomName << " successfully\n";
	}
	else if (messageID == 2)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		std::cout << "Left room " << roomName << " successfully\n";
	}
	else if (messageID == 3)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		int messageLength = recvBuffer.readInt32BE();
		std::string message = recvBuffer.readString(messageLength);
		std::cout << '\r';
		for (int i = 0; i < userIn.length(); i++)
			std::cout << ' ';
		std::cout << '\r';
		std::cout << "[" << roomName << "] " << message << "\n";

		std::cout << userIn;
	}
	return;
}