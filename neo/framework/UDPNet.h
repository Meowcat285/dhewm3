#pragma once

#include "sys/sys_public.h"
#include <string>

class UDPNet {
public:
    UDPNet();
    ~UDPNet();

    bool Init(int port);
    void Shutdown();

    void Send(const netadr_t& to, const std::string& message);

private:
    idPort port;
};
