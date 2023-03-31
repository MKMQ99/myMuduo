## myMuduo

重写muduo，实现核心逻辑

### 1 一些基础类

#### 1.1 noncopyable

noncopyable 是一个公共基类，它的拷贝构造函数和赋值函数是delete的，被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作

#### 1.2 Logger

Logger类用于日志记录，使用单例模式构造，可设置日志级别为INFO、ERROR、FATAL、DEBUG

#### 1.3 TimeStamp

看名字就知道用于时间戳

#### 1.4 InetAddress

封装socket地址类型，成员只有 *sockaddr_in addr_* 一个，以及一系列成员函数，用于获得socket IP Port等信息

#### 1.5 CurrentThread

主要用于获取当前线程的线程id



### 2 重要的Channel Poller EventLoop

#### 2.1 EventLoop

EventLoop总体逻辑

![](https://github.com/MKMQ99/myMuduo/raw/main/img/EventLoop总体逻辑.png)

#### 2.2 事件分发类 Channel

Channel 自始至终只负责一个 fd 的(注册与响应) IO 事件，但是不拥有该 fd ,所以也就在析构的时候不关闭它。每个Channel自始至终都属于一个EventLoop（一个EventLoop对应多个Channel，处理多个IO）。工作时，Channel类中保存这IO事件的类型以及对应的回调函数，当IO事件发生时，最终会调用到Channel类中的回调函数。

具体流程如下：
首先给定Channel所属的 loop，及其要处理的 fd；接着注册 fd 上需要监听的事件，可以调用接口函数$enableReading$或$enableWriting$来注册对应 fd 上的事件，$disable*$ 是销毁指定的事件；然后通过 $setCallback$ 来设置事件发生时的回调即可。
注册事件时函数调用关系，如下：$Channel::update()->EventLoop::updateChannel(Channel)->Poller::updateChannel(Channel)$，
最终向 Epoll 系统调用的监听事件表注册或修改事件。

![](https://github.com/MKMQ99/myMuduo/raw/main/img/Channel.png)

#### 2.3 IO multiplexing 类 Poller

Poller是一个基类，这里实现使用的是Epoll。

#### 2.4 一些多线程设计

一个线程一个EventLoop，每个线程都有自己管理的各种ChannelList。有时候，我们总有一些需求，要在各个线程之间调配任务。

##### 2.4.1 runInLoop()和queueInLoop()

EventLoop还有一个重要的成员pendingFunctors_（私有成员），该成员是暴露给其他线程的。这样，其他线程向IO线程添加定时时间的流程就是：

其他线程调用runInLoop()，如果不是当前IO线程，再调用queueInLoop()。在queueLoop中，将时间push到pendingFunctors_中，并唤醒当前IO线程。

注意这里的唤醒条件：不是当前IO线程肯定要唤醒；此外，如果正在调用Pending functor，也要唤醒；（为什么？，因为如果正在执行PendingFunctor里面，如果也执行了queueLoop，如果不唤醒的话，新加的cb就不会立即执行了。）

##### 2.4.2 doPendingFunctors()

doPendingFunctors并没有直接在临界区去执行functors,而是利用了一个栈对象，把事件swap到栈对象中，再去执行。这样做有两个好处：

- 减少了临界区的长度，其它线程调用queueInLoop对pendingFunctors加锁时，就不会被阻塞
- 避免了死锁，可能在functors里面也会调用queueInLoop()，从而造成死锁。

回过头来看，muduo在处理多线程加锁访问共享数据的策略上，有一个很重要的原则:拼命减少临界区的长度。

##### 2.4.3 wake()

前面说到唤醒IO线程，EventLoop阻塞在poll函数上，怎么去唤醒它？以前的做法是利用pipe,向pipe中写一个字节，监视在这个pipe的读事件的poll函数就会立刻返回。在muduo中，采用了linux中eventfd调用。

每个EventLoop都有成员wakeupFd，以及相应封装的wakeupChannel。wakeupFd是每个EventLoop构造时生成的eventfd，EventLoop用这个wakeupFd构造一个Channel就是wakeupChannel，对这个Channel enableReading，并将其放入Poller，如果有别的线程想要唤醒这个EventLoop，只需要向wakeupFd对应的eventfd写入，Poller就会检测到可读事件，从而唤醒相应EventLoop。

### 3 EventLoop封装到线程中

相关包括Thread类，EventLoopThread类，EventLoopThreadPool类

muduo的并发模型为one loop per thread+ threadpool。

EventLoopThread是事件循环线程，包含一个Thread对象，一个EventLoop对象。在构造函数中，把EventLoopThread::threadFunc 注册到Thread对象中（线程启动时会回调）。

EventLoopThread工作流程：

- 在主线程创建EventLoopThread对象。
- 主线程调用EventLoopThread.startLoop()，startLoop()调用EventLoopThread中的Thread成员thread_的start()方法，然后等待Loop创建完成。
- thread_的start()方法会新创建一个线程，因为线程一创建直接启动，所以start()方法要等待线程创建完成再返回，这里使用的是信号量。
- 然后thread\_调用回调函数回到EventLoopThread类的threadFunc()方法，创建新的Loop，创建完成使用条件变量通知startLoop()。
- startLoop()返回新Loop。

EventLoopThreadPool是事件循环线程池，管理所有客户端连接，每个线程都有唯一一个事件循环,可以调用setThreadNum设置线程的数目。使用轮询策略获取事件循环线程。

总体逻辑：

![](https://github.com/MKMQ99/myMuduo/raw/main/img/EventLoopThread.png)