#include "UDPNet.h"
#include "framework/Common.h"

UDPNet::UDPNet() {
}

UDPNet::~UDPNet() {
    Shutdown();
}

bool UDPNet::Init(int portNumber) {
    return port.InitForPort(portNumber);
}

void UDPNet::Shutdown() {
    port.Close();
}

void UDPNet::Send(const netadr_t& to, const std::string& message) {
    port.SendPacket(to, message.c_str(), message.length());
}
