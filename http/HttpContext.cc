#include "HttpContext.h"

// 请求的第一行
// 例如 POST /v1/purchase/list HTTP/1.1 
bool HttpContext::processRequestLine(const char* begin, const char* end){
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');
    // 第一个空格前的字符串，请求方法
    if (space != end && request_.setMethod(start, space)){
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end){
            // 第二个空格前的字符串，URL
            // 如果有"?"，分割成path和请求参数
            const char* question = std::find(start, space, '?');
            if (question != space){
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }else{
                request_.setPath(start, space);
            }
            start = space + 1;
            // 最后一部分，解析HTTP协议
            succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
            if (succeed){
                if (*(end-1) == '1'){
                    request_.setVersion(HttpRequest::kHttp11);
                }else if (*(end-1) == '0'){
                    request_.setVersion(HttpRequest::kHttp10);
                }else{
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime){
    bool ok = true;
    bool hasMore = true;
    while(hasMore){
        if (state_ == kExpectRequestLine){
            const char* crlf = buf->findCRLF();
            if (crlf){
                ok = processRequestLine(buf->peek(), crlf);
                if (ok){
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf+2);
                    state_ = kExpectHeaders;
                }else{
                    hasMore = false;
                }
            }else{
                hasMore = false;
            }
        }else if (state_ == kExpectHeaders){
            const char* crlf = buf->findCRLF();
            if (crlf){
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf){
                    request_.addHeader(buf->peek(), colon, crlf);
                }else{
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf+2);
            }else{
                hasMore = false;
            }
        }else if (state_ == kExpectBody){

        }
    }
    return ok;
}