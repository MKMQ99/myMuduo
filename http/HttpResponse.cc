#include "HttpResponse.h"

void HttpResponse::appendToBuffer(Buffer* output) const{
    char buf[32];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    output->append(buf, sizeof(buf));
    output->append(statusMessage_.c_str(), sizeof(statusMessage_));
    output->append("\r\n", 3);

    if (closeConnection_){
        output->append("Connection: close\r\n", sizeof("Connection: close\r\n"));
    }else{
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf, sizeof(buf));
        output->append("Connection: Keep-Alive\r\n", 25);
    }

    for (const auto& header : headers_){
        output->append(header.first.c_str(), sizeof(header.first));
        output->append(": ", 3);
        output->append(header.second.c_str(), sizeof(header.second));
        output->append("\r\n", 3);
    }

    output->append("\r\n", 3);
    output->append(body_.c_str(), sizeof(body_));
}