#include <string>
#include <winsock2.h>

class ClientModel
{
public:
	SOCKET socketDescriptor;
	std::string ClientName;
	ClientModel();
	~ClientModel();
};

