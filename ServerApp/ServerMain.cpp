#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <future>
#include "buffer.h"
#include <errno.h> 
#include <sys/types.h> 
#include <map> 
#include "ClientModel.h"
#include <ctime>
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define TRUE   1 
#define FALSE  0 
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

std::map<std::string, std::vector<SOCKET>> gMapRoomToSockets;
std::vector<ClientModel*> gVecClientSockets;
buffer sendBuffer(512);
buffer recvBuffer(512);

//Example code: A simple server side code, which echos back the received message.
//Handle multiple socket connections with select and fd_set on Linux 


bool sendMessage(SOCKET clientSocket, std::string message, std::string room,int msgType)
{
	//If the message is untampered or coming from other clients
	if (msgType == 0) {
		int eraseHead = room.length() + 7; // "/send RoomName " to be removed
		message.erase(0, eraseHead);
	}

	
	int roomLength = room.length();
	int messageLength = message.length();
	int messageID = 3;
	int packetLength = roomLength + messageLength + 16;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(roomLength);
	sendBuffer.writeString(room);
	sendBuffer.writeInt32BE(messageLength);
	sendBuffer.writeString(message);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	int iResult = send(clientSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

void processMessage(buffer recvBuffer,ClientModel* client) {
	int packetLength = recvBuffer.readInt32BE();
	int messageID = recvBuffer.readInt32BE();
	if (messageID == 1)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		std::vector<SOCKET> vecSocketsInRoom = gMapRoomToSockets[roomName];
		vecSocketsInRoom.push_back(client->socketDescriptor);
		gMapRoomToSockets[roomName] = vecSocketsInRoom;
		std::string messageFromServer = "Joined room " + roomName + " successfully";
		for (int index = 0; index < vecSocketsInRoom.size(); index++) {
			sendMessage(vecSocketsInRoom[index], client->ClientName + " : " + messageFromServer, roomName, 1);
		}
	}
	else if (messageID == 2)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		std::vector<SOCKET> vecSocketsInRoom = gMapRoomToSockets[roomName];
		for (int index = 0; index < vecSocketsInRoom.size(); index++) {
			if (client->socketDescriptor == vecSocketsInRoom[index]) {
				vecSocketsInRoom.erase(vecSocketsInRoom.begin() + index);
				std::string messageFromServer = "Left room " + roomName + " successfully";
				if (vecSocketsInRoom.size() != 0) {
					gMapRoomToSockets[roomName] = vecSocketsInRoom;
					for (int index = 0; index < vecSocketsInRoom.size(); index++) {
						sendMessage(vecSocketsInRoom[index], client->ClientName + " : " + messageFromServer, roomName, 1);
					}
				}
				else
					gMapRoomToSockets.erase(roomName);
				sendMessage(client->socketDescriptor, client->ClientName + " : " + messageFromServer, roomName, 1);
				break;
			}
		}
	}
	else if (messageID == 3)
	{
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		int messageLength = recvBuffer.readInt32BE();
		std::string message = recvBuffer.readString(messageLength);
		std::vector<SOCKET> vecSocketsInRoom = gMapRoomToSockets[roomName];
		for (int index = 0; index < vecSocketsInRoom.size(); index++) {
				sendMessage(vecSocketsInRoom[index],client->ClientName + " : " + message, roomName, 1);
			}	
	}
	return;

}

void closeClientConnection(SOCKET clientSocket, fd_set &readfds) {
		int iResult = closesocket(clientSocket);
		if (iResult == SOCKET_ERROR)
			printf("Error in closing socket...\n");
		else
			printf("Socket connection closed...\n");
		FD_CLR(clientSocket, &readfds);
		printf("Removed Socket from FD SET...\n");
		for (int index = 0; index < ::gVecClientSockets.size(); index++) {
			if (::gVecClientSockets[index]->socketDescriptor == clientSocket) {
				::gVecClientSockets.erase(::gVecClientSockets.begin() + index);
				break;
			}
		}
		
}

int main(int argc, char *argv[])
{
	int opt = TRUE;
	int activity,sd;
	int max_sd;

	WSADATA wsaData;
	int iResult;

	SOCKET masterSocket = INVALID_SOCKET;
	SOCKET clientSocket = INVALID_SOCKET;


	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	fd_set readfds;

	//a message 
	char *message = "Welcome to Gam-0-Sapiens Chatroom";

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	masterSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (masterSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//set master socket to allow multiple connections , 
	//this is just a good habit, it will work without this 
	if (setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
		sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Setup the TCP listening socket
	iResult = bind(masterSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(masterSocket);
		WSACleanup();
		return 1;
	}

	//freeaddrinfo(result);

	//try to specify maximum of 3 pending connections for the master socket 
	/*if (listen(masterSocket, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}*/

	iResult = listen(masterSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(masterSocket);
		WSACleanup();
		return 1;
	}

	//accept the incoming connection 
	//addrlen = sizeof(address);
	puts("Waiting for connections ...");

	while (TRUE)
	{
		//clear the socket set 
		FD_ZERO(&readfds);

		//add master socket to set 
		FD_SET(masterSocket, &readfds);
		max_sd = masterSocket;


		//add child sockets to set 
		for (int index = 0; index < gVecClientSockets.size(); index++)
		{
			//socket descriptor 
			sd = gVecClientSockets[index]->socketDescriptor;

			//if valid socket descriptor then add to read list 
			if (sd > 0)
				FD_SET(sd, &readfds);

			//highest file descriptor number, need it for the select function 
			if (sd > max_sd)
				max_sd = sd;
		}

		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR))
		{
			printf("select error");
		}

		//If something happened on the master socket , 
		//then its an incoming connection 
		if (FD_ISSET(masterSocket, &readfds))
		{
			clientSocket = accept(masterSocket, NULL, NULL);
			if (clientSocket == INVALID_SOCKET) {
				printf("accept failed with error: %d\n", WSAGetLastError());
				//closesocket(masterSocket);
				//WSACleanup();
				//return 1;
			}

			//inform user of socket number - used in send and receive commands 
			//printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs
			//	(address.sin_port));

			//add new socket to array of sockets 
			ClientModel* tmpClientModel = new ClientModel();
			tmpClientModel->socketDescriptor = clientSocket;
			std::time_t epochTime = std::time(0);
			std::stringstream ss;
			ss << epochTime;
			std::string strEpochTime = ss.str();
			tmpClientModel->ClientName = tmpClientModel->ClientName + strEpochTime;
			gVecClientSockets.push_back(tmpClientModel);
			printf("Adding to list of sockets to Vector \n");
			clientSocket = INVALID_SOCKET;
			sendMessage(tmpClientModel->socketDescriptor, message, "Welcome", 1);

			puts("Welcome message sent successfully");
		}

		//else its some IO operation on some other socket
		for (int index = 0; index < gVecClientSockets.size(); index++)
		{
			sd = gVecClientSockets[index]->socketDescriptor;

			if (FD_ISSET(sd, &readfds))
			{
					iResult = recv(sd, recvbuf, recvbuflen, 0);
					if (iResult > 0) {
						printf("Bytes received: %d\n", iResult);
						std::string getData = "";
						recvBuffer.displayIndices();
						recvBuffer.resetIndicesManually(); //Hack as write Index gets restored .... need to understand
						for (int i = 0; i < 4; i++)
							getData += recvbuf[i];
						recvBuffer.writeString(getData);
						recvBuffer.displayIndices();
						int recvSize = recvBuffer.readInt32BE();
						recvBuffer.displayIndices();
						for (int i = 4; i < recvSize; i++)
							getData += recvbuf[i];
						recvBuffer.writeString(getData);
						recvBuffer.displayIndices();
						processMessage(recvBuffer,gVecClientSockets[index]);
					}
					else if (iResult == 0) {
						closeClientConnection(sd, readfds);
					}
					else {
						printf("recv failed with error: %d\n", WSAGetLastError());
						closeClientConnection(sd, readfds);
						//return 1;
					}
			}
		}
	}

	WSACleanup();
	return 0;
}