## myMuduo

### 1 前言

仿照muduo网络库实现了myMuduo库，这个库的特点是：

- **一个基于 reactor 反应堆模型的多线程 C++ 网络库，参考 muduo 网络库实现，去除了 boost 依赖**
- **并发模型采用 one loop per thread + thread pool**
- **IO 复用采用的是 Epoll，LT 模式**


### 2 一些基础类

#### 2.1 noncopyable

noncopyable 是一个公共基类，它的拷贝构造函数和赋值函数是delete的，被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作。

#### 2.2 Logger

Logger类用于日志记录，使用**单例模式**构造，可设置日志级别为INFO、ERROR、FATAL、DEBUG。

Logger类：

```c++
class Logger : noncopyable{
    public:
        // 获取日志唯一的实例对象
        static Logger* instance();
        // 设置日志级别
        void setLogLevel(int Level);
        // 写日志
        void log(std::string msg);
    private:
        static Logger* log_;
        int LogLevel_;
        Logger(){}
};

Logger* Logger::instance(){
    static Logger logger;
    return &logger;
}
```

#### 2.3 TimeStamp

看名字就知道用于时间戳

#### 2.4 InetAddress

封装socket地址类型，成员只有 *sockaddr_in addr_* 一个，以及一系列成员函数，用于获得socket IP Port等信息

#### 2.5 CurrentThread

主要用于获取当前线程的线程id

### 3 事件分发类 Channel

Channel自始至终只负责一个fd的IO事件。**每个Channel自始至终都属于一个EventLoop**（**一个EventLoop对应多个Channel**，处理多个IO）。工作时，Channel类中保存这IO事件的类型以及对应的**回调函数**，当IO事件发生时，最终会调用到Channel类中的回调函数。

具体流程如下：

首先给定Channel所属的 loop，及其要处理的 fd，接着注册 fd 上需要监听的事件，可以调用接口函数`enableReading`或`enableWriting`来注册对应 fd 上的事件，`disable*` 是销毁指定的事件；然后通过 `setCallback$`来设置事件发生时的回调即可。

注册事件时函数调用关系，如下：

`Channel::update()->EventLoop::updateChannel(Channel)->Poller::updateChannel(Channel)`

最终向 Epoll 系统调用的监听事件表注册或修改事件。

![](https://github.com/MKMQ99/myMuduo/raw/main/img/Channel.png)

**关于void Channel::tie(const std::shared_ptr\<void\> &obj)**

这里是一个智能指针使用的特定场景之一，用于延长特定对象的生命期

![](https://github.com/MKMQ99/myMuduo/raw/main/img/channel的tie函数.png)

当对方断开TCP连接，这个IO事件会触发Channel::handleEvent()调用，后者会调用用户提供的CloseCallback，而用户代码在onClose()中有可能析构Channel对象，这就造成了灾难。等于说Channel::handleEvent()执行到一半的时候，其所属的Channel对象本身被销毁了。

所以采用`void Channel::tie(const std::shared_ptr<void> &obj)`函数延长某些对象（可以是Channel对象，也可以是其owner对象）的生命期，使之长过Channel::handleEvent()函数。

Muduo TcpConnection采用shared_ptr管理对象生命期的原因之一就是因为这个。

当有关闭事件时，调用流程如下：

`Channel::handleEvent -> TcpConnection::handleClose ->TcpClient::removeConnection ->TcpConnection::connectDestroyed->channel_->remove()`

1、为了在Channel::handleEvent处理期间，防止因其owner对象被修改，进而导致Channel被析构，最后出现不可预估错误。 Channel::tie()的作用就是将Channel的owner对象进行绑定保护起来。

 2、另外channel->remove的作用是删除channel在Poll中的地址拷贝，而不是销毁channel。channel的销毁由其owner对象决定。

### 4 IO multiplexing 类 Poller/EpollPoller

Poller类**负责监听文件描述符事件是否触发**以及**返回发生事件的文件描述符以及具体事件**。

muduo提供了epoll和poll两种IO多路复用方法来实现事件监听。不过默认是使用epoll来实现，也可以通过选项选择poll。但是myMuduo库只支持epoll。

这个Poller是个抽象虚类，由EpollPoller继承实现，与监听文件描述符和返回监听结果的具体方法也基本上是在这两个派生类中实现。EpollPoller封装了用epoll方法实现的与事件监听有关的各种方法。

**Poller/EpollPoller的重要成员变量：**

- `epollfd_`就是用`epoll_create`方法返回的epoll句柄。
- `channels_`：这个变量是`std::unordered_map<int, Channel*>`类型，负责记录 `文件描述符 -> Channel` 的映射，也帮忙保管所有注册在你这个Poller上的Channel。
- `ownerLoop_`：所属的EventLoop对象。

**EpollPoller给外部提供的最重要的方法：**

```c++
TimeStamp poll(int timeoutMs, ChannelList *activeChannels)
```

这个函数可以说是**Poller的核心**了，当外部调用`poll`方法的时候，该方法底层其实是通过`epoll_wait`获取这个事件监听器上发生事件的fd及其对应发生的事件，我们知道每个fd都是由一个Channel封装的，通过哈希表`channels_`可以根据fd找到封装这个fd的Channel。**将事件监听器监听到该fd发生的事件写进这个Channel中的revents成员变量中。**然后把这个Channel装进`activeChannels`中（它是一个vector<Channel*>）。这样，当外界调用完`poll`之后就能拿到事件监听器的**监听结果（`activeChannels_`）**，这个activeChannels就是事件监听器监听到的发生事件的fd，以及每个fd都发生了什么事件。

### 5 事件循环类 EventLoop

EventLoop是负责事件循环的重要模块，Channel和Poller其实相当于EventLoop的手下，EventLoop整合封装了二者并向上提供了更方便的接口来使用。EventLoop和Channel，Poller的关系如下图：

![](https://github.com/MKMQ99/myMuduo/raw/main/img/EventLoop总体逻辑.png)

EventLoop最终要的函数就是事件循环函数`void EventLoop::loop()`

```c++
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_){
        activeChannels_.clear();
        // 监听两类fd 一种是client的fd 一种是wakeupFd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_){
            // Poller监听哪些channel发生事件了，然后上报EventLoop，通知channel处理相应事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop 接收连接，将 fd 打包给 channel，发给subLoop
         * mainLoop 事先注册一个回调cb（需要subLoop来执行）
         * wakeup subLoop后，执行下面的方法，执行maiinLoop注册的cb操作
         */
        doPendingFunctors();
    }

    LOG_INFO("Eventloop %p stop looping \n", this);
    looping_ = false;
}
```

每一个EventLoop都绑定了一个线程（一对一绑定），也就是 **one loop per thread 模型**。

#### runInLoop()和queueInLoop()

EventLoop有一个非常有用的功能：在它的IO线程内执行某个用户任务回调，即EventLoop::runInLoop(const Functor& cb)，其中Functor是 boost::function。如果用户在当前IO线程调用这个函数，回调会 步进行；如果用户在其他线程调用runInLoop()，cb会被加入队列，IO 线程会被唤醒来调用这个Functor。

有了这个功能，我们就能轻易地在**线程间调配任务**，比方说把 TimerQueue的成员函数调用移到其IO线程，这样可以在不用锁的情况下 保证线程安全性。

#### wake()

前面说到**唤醒IO线程**，EventLoop阻塞在poll函数上，怎么去唤醒它？以前的做法是利用pipe，向pipe中写一个字节，监视在这个pipe的读事件的poll函数就会立刻返回。在muduo中，采用了linux中eventfd调用。

每个EventLoop都有成员wakeupFd，以及相应封装的wakeupChannel。wakeupFd是每个EventLoop构造时生成的eventfd，EventLoop用这个wakeupFd构造一个Channel就是wakeupChannel，对这个Channel enableReading，并将其放入Poller，如果有别的线程想要唤醒这个EventLoop，只需要向wakeupFd对应的eventfd写入，Poller就会检测到可读事件，从而唤醒相应EventLoop。

#### doPendingFunctors()

doPendingFunctors并没有直接在临界区去执行functors,而是利用了一个栈对象，把事件swap到栈对象中，再去执行。这样做有两个好处：

- 减少了临界区的长度，其它线程调用queueInLoop对pendingFunctors加锁时，就不会被阻塞
- 避免了死锁，可能在functors里面也会调用queueInLoop()，从而造成死锁。

回过头来看，muduo在处理多线程加锁访问共享数据的策略上，有一个很重要的原则:拼命减少临界区的长度。

### 6 EventLoop封装到线程中

相关包括Thread类，EventLoopThread类，EventLoopThreadPool类

muduo的并发模型为one loop per thread+ threadpool。

EventLoopThread是事件循环线程，包含一个Thread对象，一个EventLoop对象。在构造函数中，把EventLoopThread::threadFunc 注册到Thread对象中（线程启动时会回调）。

EventLoopThread工作流程：

- 在主线程创建EventLoopThread对象。
- 主线程调用EventLoopThread.startLoop()，startLoop()调用EventLoopThread中的Thread成员thread_的start()方法，然后等待Loop创建完成。
- thread_的start()方法会新创建一个线程，因为线程一创建直接启动，所以start()方法要等待线程创建完成再返回，这里使用的是信号量。
- 然后thread_调用回调函数回到EventLoopThread类的threadFunc()方法，创建新的Loop，创建完成使用条件变量通知startLoop()。
- startLoop()返回新Loop。

EventLoopThreadPool是事件循环线程池，管理所有客户端连接，每个线程都有唯一一个事件循环，可以调用setThreadNum设置线程的数目。使用轮询策略获取事件循环线程。

总体逻辑：

![](https://github.com/MKMQ99/myMuduo/raw/main/img/EventLoopThread.png)

### 7 Acceptor类

Acceptor属于baseLoop或者说mainLoop，它负责处理新连接。Acceptor有成员acceptSocket（Socket对象）和acceptChannel（Channel对象），初始化一个Acceptor会创建一个socket和对应的Channel。当调用Acceptor的listen方法，acceptSocket开始listen，之后acceptChannel_.enableReading()。baseLoop检测到新连接就触发Acceptor的handleRead回调至TcpServer::newConnection，由TcpServer轮询找到subloop，根据连接成功的sockfd，创建TcpConnection连接对象，从而建立连接。

### 8 Buffer类

应用层为什么需要Buffe：非阻塞IO的核心思想是避免阻塞在read()或write()或其它IO系统调用上，这样可以最大最大限度的复用。

- TcpConnection需要有output buffer。假设程序想发送100KB的数据，但是调用write之后，操作系统只接受了80KB（受TCP advertised window控制），调用者肯定不想原地等待，如果有buffer，调用者只管将数据放入buffer，其它由网络库处理即可。
- TcpConnection需要有input buffer。TCP是一个无边界的字节流协议，接收方必须要处理“收到的数据尚不构成一条完整的消息”和“一次收到两条消息的数据”等情况。如果有buffer，网络库收到数据之后，先放到input buffer，等构成一条完整的消息再通知程序的业务逻辑。同时，网络库在处理“socket 可读”事件的时候，必须一次性把 socket 里的数据读完（从操作系统 buffer 搬到应用层 buffer），否则会反复触发 POLLIN 事件，造成 busy-loop。

Buffer仅有3个成员，存放数据的vector\<char\> buffer\_，已经size\_t类型的readerIndex\_和writerIndex\_。

### 9 TcpConnection类

这个类主要封装了一个已建立的TCP连接，以及控制该TCP连接的方法（连接建立和关闭和销毁），以及该连接发生的各种事件（读/写/错误/连接）对应的处理函数，以及这个TCP连接的服务端和客户端的套接字地址信息等。

**TcpConnection类和Acceptor类可以认为是兄弟关系，Acceptor用于main EventLoop中，对服务器监听套接字fd及其相关方法进行封装（监听、接受连接、分发连接给SubEventLoop等），TcpConnection用于SubEventLoop中，对连接套接字fd及其相关方法进行封装（读消息事件、发送消息事件、连接关闭事件、错误事件等）**

**TcpConnection的重要变量**

- socket_：用于保存已连接套接字文件描述符。
- channel\_：封装了上面的socket_及其各类事件的处理函数（读、写、错误、关闭等事件处理函数）。**这个Channel种保存的各类事件的处理函数是在TcpConnection对象构造函数中注册的。**
- loop\_：这是一个EventLoop*类型，该Tcp连接的Channel注册到了哪一个sub EventLoop上。这个loop_就是那一个sub EventLoop。
- inputBuffer_：这是一个Buffer类，是该TCP连接对应的用户接收缓冲区。
- outputBuffer\_：也是一个Buffer类，不过是用于暂存那些暂时发送不出去的待发送数据。因为Tcp发送缓冲区是有大小限制的，假如达到了高水位线，就没办法把发送的数据通过send()直接拷贝到Tcp发送缓冲区，而是暂存在这个outputBuffer\_中，等TCP发送缓冲区有空间了，触发可写事件了，再把outputBuffer\_中的数据拷贝到Tcp发送缓冲区中。
- state\_：这个成员变量标识了当前TCP连接的状态（Connected、Connecting、Disconnecting、Disconnected）
- connetionCallback\_、messageCallback\_、writeCompleteCallback\_、closeCallback\_ ： 用户会自定义 [连接建立/关闭后的处理函数] 、[收到消息后的处理函数]、[消息发送完后的处理函数]以及Muduo库中定义的[连接关闭后的处理函数]。这四个函数都会分别注册给这四个成员变量保存。

### 10 最后配上总逻辑图

![](https://github.com/MKMQ99/myMuduo/raw/main/img/总逻辑.png)