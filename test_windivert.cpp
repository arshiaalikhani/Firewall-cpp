#include <iostream>
#include <windows.h>
#include "windivert.h"

int main(){
HANDLE handle = WinDivertOpen(
    "true",
    WINDIVERT_LAYER_NETWORK,
    0,
    0
);
if(handle == INVALID_HANDLE_VALUE){
    std::cerr<<"failed to open windivert handle. Error:" << GetLastError()<<std::endl;
    std::cerr<<"Are you running as administrator?"<<std::endl;
    return 1;
}
std::cout<<"WinDivert started! Counting packets... (Ctrl+C to stop)"<<std::endl;
unsigned char packet[65535];
UINT packetLen;
WINDIVERT_ADDRESS addr;
int count = 0;
while(true){
    if(!WinDivertRecv(handle, packet, sizeof(packet), &packetLen, &addr)){
        std::cerr<<"Failed to receive packet. Error: " <<GetLastError()<<std::endl;
        continue;
    }
count ++;
std::cout<<"packet #" <<count<<"-size"<<packetLen<<"bytes"<<std::endl;
WinDivertSend(handle, packet, packetLen, NULL, &addr);
}
WinDivertClose(handle);
return 0;
}
