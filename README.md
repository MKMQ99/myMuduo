## myMuduo
重写muduo，实现核心逻辑

### 一些基础类

#### noncopyable

noncopyable 是一个公共基类，它的拷贝构造函数和赋值函数是delete的，被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作

#### Logger

Logger类用于日志记录，使用单例模式构造，可设置日志级别为INFO、ERROR、FATAL、DEBUG

#### TimeStamp

看名字就知道用于时间戳

#### InetAddress

封装socket地址类型，成员只有 *sockaddr_in addr_* 一个，以及一系列成员函数，用于获得socket IP Port等信息
