#pragma once

#include <vector>
#include <string>
#include <algorithm>


/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size



class Buffer{
    public:
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 1024;

        explicit Buffer(size_t initialSize = kInitialSize)
            : buffer_(kCheapPrepend + initialSize)
            , readerIndex_(kCheapPrepend)
            , writerIndex_(kCheapPrepend){}
        
        size_t readableBytes() const{ return writerIndex_ - readerIndex_; }
        size_t writableBytes() const{ return buffer_.size() - writerIndex_; }
        size_t prependableBytes() const{ return readerIndex_; }
        // 返回缓冲区中可读数据的起始地址
        const char* peek() const{ return begin() + readerIndex_; }

        const char* findCRLF() const{
            const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
            return crlf == beginWrite() ? NULL : crlf;
        }

        const char* findCRLF(const char* start) const{
            const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
            return crlf == beginWrite() ? NULL : crlf;
        }
        
        // onMessage string <- Buffer
        void retrieve(size_t len){
            if (len < readableBytes()){
                readerIndex_ += len; // 应用只读取了可读缓冲区数据的一部分，就是len
            }else{
                retrieveAll();
            }
        }

        void retrieveUntil(const char* end){
            retrieve(end - peek());
        }

        void retrieveAll(){
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend;
        }

        // 把OnMessage函数上报的Buffer数据，转成string类型的数据返回
        std::string retrieveAllAsString(){
            return retrieveAsString(readableBytes());
        }

        std::string retrieveAsString(size_t len){
            std::string result(peek(), len);
            retrieve(len); // 上面一句把缓冲区中可读的数据已经读取出来，这里肯定要对缓冲区进行复位操作
            return result;
        }

        // buffer_.size - writeIndex_     len
        void ensureWritableBytes(size_t len){
            if (writableBytes() < len){
                makeSpace(len);
            }
        }

        void makeSpace(size_t len){
            if (writableBytes() + prependableBytes() < len + kCheapPrepend){
                buffer_.resize(writerIndex_+len);
            }
            else{
                size_t readable = readableBytes();
                std::copy(begin()+readerIndex_,
                            begin()+writerIndex_,
                            begin()+kCheapPrepend);
                readerIndex_ = kCheapPrepend;
                writerIndex_ = readerIndex_ + readable;
            }
        }

        // 把[data,date+len]内存上的数据，添加到writable缓冲区中
        void append(const char* data, size_t len){
            ensureWritableBytes(len);
            std::copy(data, data+len, beginWrite());
            hasWritten(len);
        }

        char* beginWrite(){ return begin() + writerIndex_; }
        const char* beginWrite() const { return begin() + writerIndex_; }
        void hasWritten(size_t len){writerIndex_ += len;}

        // 从fd上读取数据
        ssize_t readFd(int fd, int* savedErrno);
        // 通过fd发送数据
        ssize_t writeFd(int fd, int* savedErrno);

    private:
        char* begin(){ return &*buffer_.begin(); }
        const char* begin() const{ return &*buffer_.begin(); }

        std::vector<char> buffer_;
        size_t readerIndex_;
        size_t writerIndex_;

        static const char kCRLF[];
};