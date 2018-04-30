#include "buffer.h"

#include <iostream>

int main()
{
	buffer goodBuffer(24);
	char* sending;
	int length;
	std::string ending;
	
	goodBuffer.writeInt32BE(12345678);
	//std::cout << goodBuffer.readInt32BE() << std::endl;
	
	std::string writeWords = "Testinado";
	
	goodBuffer.writeString(writeWords);
	//std::cout<<goodBuffer.readString(writeWords.length())<<std::endl;

	goodBuffer.retrieveData(&sending, &length);
	std::cout << sending[2] << std::endl;

	std::cin >> ending;
	return 0;
}