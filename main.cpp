#define _WIN32_WINNT 0x0600
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <memory>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// این می‌گه به کامپایلر: «هیچ بایت اضافه‌ای بین فیلدها نذار»
// چون داریم یه بسته‌ی واقعی شبکه می‌سازیم و باید دقیقاً همون فرمتی باشه که DNS انتظار داره
#pragma pack(push, 1)
struct DNSHeader {
    unsigned short id;       // شناسه‌ی یکتا برای این سوال
    unsigned short flags;    // می‌گه این یه سوال ساده‌ست
    unsigned short qdcount;  // چند تا سوال داریم = 1
    unsigned short ancount;  // چند تا جواب داریم = 0 (چون داریم می‌پرسیم)
    unsigned short nscount;  // 0
    unsigned short arcount;  // 0
};
#pragma pack(pop)
//این بخش مربوط هستش به بخش 3.5 که برای خواندن پیش فرض DNS هستش
std::vector<std::string> GetSystemDnsServers() {
        std::vector<std::string> dnsServers;
        // ۱. دریافت اندازه‌ی مورد نیاز بافر
        ULONG outBufLen = 0;
        DWORD dwRetVal = GetNetworkParams(nullptr, &outBufLen);
        //اگر خطا ERROR_BUFFER_OVERFLOW نبود، یعنی مشکل دیگری وجود دارد
        if(dwRetVal != ERROR_BUFFER_OVERFLOW){
            std::cerr<<"Error: Unable to get buffer size. Code:"<<dwRetVal<<std::endl;
            return dnsServers;
        }
        // ۲. تخصیص حافظه بر اساس اندازه‌ی به‌دست‌آمده
        // استفاده از std::unique_ptr برای مدیریت خودکار حافظه
        std::unique_ptr<BYTE[]> buffer(new BYTE[outBufLen]);
        PFIXED_INFO pFixedInfo = reinterpret_cast<PFIXED_INFO>(buffer.get());
        // ۳. فراخوانی دوم برای دریافت اطلاعات واقعی
        dwRetVal = GetNetworkParams(pFixedInfo, &outBufLen);
        if (dwRetVal != ERROR_SUCCESS) {
            std::cerr << "Error: GetNetworkParams failed. Code: " << dwRetVal << std::endl;
            return dnsServers;
        }
         // ۴. استخراج آدرس‌های DNS از لیست پیوندی
        // ساختار DnsServerList یک لیست پیوندی از IP_ADDR_STRING است
        IP_ADDR_STRING* pCurrentDns = &(pFixedInfo->DnsServerList);
        while(pCurrentDns != nullptr){
                // اضافه کردن آدرس IP به بردار
            if(pCurrentDns->IpAddress.String[0] != '\0'){
                dnsServers.push_back(pCurrentDns->IpAddress.String);
            }
             pCurrentDns = pCurrentDns->Next; // رفتن به گره‌ی بعدی
        }
        return dnsServers;
}
int main()
{
    WSADATA wsaData;
    int sockfd;
    struct sockaddr_in dns_server;
    char buffer[1024];

    // ------------------------------------------------
    // 1. راه‌اندازی Winsock
    // ------------------------------------------------
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // ------------------------------------------------
    // 2. ساخت سوکت UDP
    // ------------------------------------------------
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // ------------------------------------------------
    // 3. آدرس سرور DNS مقصد (نه آدرس خودمون - آدرس گوگل)
    // ------------------------------------------------
    memset(&dns_server, 0, sizeof(dns_server));
    dns_server.sin_family = AF_INET;
    dns_server.sin_port = htons(53);
    //dns_server.sin_addr.s_addr = inet_addr("8.8.8.8");
    // ------------------------------------------------
    // 3.5. خوندن DNS پیش‌فرض سیستم به جای هاردکد 8.8.8.8
    // ------------------------------------------------
    std::vector<std::string> dnsServers = GetSystemDnsServers();

    if (dnsServers.empty()) {
        std::cout << "No DNS servers found, using 8.8.8.8 as fallback" << std::endl;
        dns_server.sin_addr.s_addr = inet_addr("8.8.8.8");
    } else {
        std::cout << "System DNS Servers:" << std::endl;
        for (const auto& dns : dnsServers) {
            std::cout << "  - " << dns << std::endl;
        }
        dns_server.sin_addr.s_addr = inet_addr(dnsServers[0].c_str());
        std::cout << "Using: " << dnsServers[0] << std::endl;
    }
    // ------------------------------------------------
    // 4. ساختن بسته‌ی DNS - قسمت هدر
    // ------------------------------------------------
    char packet[512];
    memset(packet, 0, sizeof(packet));

    DNSHeader* header = (DNSHeader*)packet;
    header->id = htons(1234);       // یه عدد دلخواه، فقط باید یکتا باشه
    header->flags = htons(0x0100);  // یعنی: "لطفاً اگه خودت نمی‌دونی جواب رو، بگرد پیداش کن" (recursive query)
    header->qdcount = htons(1);     // فقط 1 تا سوال داریم
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;

    // ------------------------------------------------
    // 5. ساختن بسته‌ی DNS - قسمت سوال (اسم دامنه)
    // ------------------------------------------------
    // DNS اسم دامنه رو این‌جوری می‌خواد: [طول]google[طول]com[صفر]
    // مثلاً google.com میشه: 6google3com0
    int pos = sizeof(DNSHeader);  // بعد از هدر شروع می‌کنیم

    std::string domain = "google.com";
    std::string label = "";

    for (int i = 0; i <= (int)domain.length(); i++) {
        if (i == (int)domain.length() || domain[i] == '.') {
            packet[pos] = (char)label.length();  // اول طول کلمه رو می‌نویسیم
            pos++;
            for (int j = 0; j < (int)label.length(); j++) {
                packet[pos] = label[j];  // بعد خود کلمه رو می‌نویسیم
                pos++;
            }
            label = "";
        } else {
            label += domain[i];
        }
    }
    packet[pos] = 0;  // بایت صفر یعنی "اسم دامنه اینجا تموم شد"
    pos++;

    // نوع رکورد A (یعنی می‌خوایم IPv4 بگیریم) = عدد 1
    unsigned short qtype = htons(1);
    memcpy(packet + pos, &qtype, 2);
    pos += 2;

    // کلاس IN (یعنی اینترنت) = عدد 1
    unsigned short qclass = htons(1);
    memcpy(packet + pos, &qclass, 2);
    pos += 2;

    // ------------------------------------------------
    // 6. فرستادن بسته به سرور DNS
    // ------------------------------------------------
    int sent = sendto(sockfd, packet, pos, 0,
                       (struct sockaddr*)&dns_server, sizeof(dns_server));
    if (sent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    std::cout << "Query sent (" << sent << " bytes). Waiting for response..." << std::endl;

    // ------------------------------------------------
    // 7. گرفتن جواب از سرور DNS
    // ------------------------------------------------
    struct sockaddr_in from;
    int from_len = sizeof(from);

    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                      (struct sockaddr*)&from, &from_len);
    if (n == SOCKET_ERROR) {
        std::cerr << "recvfrom failed: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    std::cout << "Got response: " << n << " bytes" << std::endl;
    // ------------------------------------------------
    // 8. باز کردن جواب و پیدا کردن IP
    // ------------------------------------------------
    // جواب سرور دقیقاً از همون جایی شروع میشه که سوال ما تموم شد
    // (یعنی همون "pos" که موقع ساختن سوال حساب کردیم)
    int ans_pos = pos;

    ans_pos += 2;  // اسم دامنه (فشرده‌شده، فقط یه اشاره‌گر ۲ بایتیه، ردش کن)

    unsigned short type;
    memcpy(&type, buffer + ans_pos, 2);
    ans_pos += 2;  // نوع رکورد (باید 1 باشه یعنی A)

    ans_pos += 2;  // کلاس (رد کن)
    ans_pos += 4;  // TTL (رد کن)

    unsigned short rdlength;
    memcpy(&rdlength, buffer + ans_pos, 2);
    rdlength = ntohs(rdlength);
    ans_pos += 2;  // طول داده (باید 4 باشه برای IPv4)

    // حالا دقیقاً اینجا خود IP نشسته - 4 بایت
    unsigned char ip_bytes[4];
    memcpy(ip_bytes, buffer + ans_pos, 4);

    std::cout << "IP: "
          << (int)ip_bytes[0] << "."
          << (int)ip_bytes[1] << "."
          << (int)ip_bytes[2] << "."
          << (int)ip_bytes[3] << std::endl;
    // ------------------------------------------------
    //  تمیز کردن
    // ------------------------------------------------
    closesocket(sockfd);
    WSACleanup();
    return 0;
}
