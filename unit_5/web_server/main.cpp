#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include "locker.h"
#include "thread_pool.h"
#include "http_conn.h"

#define MAX_FD 65536   // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000  // 监听的最大的事件数量

// 添加文件描述符
extern void addfd( int epollfd, int fd, bool one_shot );
// 从epoll中删除文件描述符
extern void removefd( int epollfd, int fd );

//添加信号捕捉（handler是信号处理函数，函数指针）
void addsig(int sig, void( handler )(int)){
    struct sigaction sa;        //信号的参数
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );  //设置临时阻塞信号集
    assert( sigaction( sig, &sa, NULL ) != -1 );    
}

int main( int argc, char* argv[] ) {
    
    if( argc <= 1 ) {   //至少要有一个端口号
        printf( "usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    //获取端口号
    int port = atoi( argv[1] ); //转换成整数
    addsig( SIGPIPE, SIG_IGN ); //对SIGPIPE信号进行忽略

    //创建线程池类，并且初始化（这里我们是服务器，那么要创建一个处理http连接的线程池类）
    threadpool< http_conn >* pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    } catch( ... ) {
        return 1;
    }

    //创建一个数组，用于保存所有客户端的信息
    http_conn* users = new http_conn[ MAX_FD ];

    //下面处理网络通信

    //创建监听的套接字
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );

    // 设置端口复用（一定要在绑定之前设置）
    int reuse = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );

    //绑定 到本地的ip和端口
    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;   //IP地址
    address.sin_family = AF_INET;       //用于通信的协议族（ipv4）
    address.sin_port = htons( port );   //端口号（转换成网络字节序）
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );

    //监听
    ret = listen( listenfd, 5 );

    // （进行I/O多路复用，忘记了回去看4.25的课件）创建epoll对象，和事件数组，添加
    int epollfd = epoll_create( 5 );
    // 将监听的描述符添加到epoll对象中
    addfd( epollfd, listenfd, false );
    http_conn::m_epollfd = epollfd;
    epoll_event events[ MAX_EVENT_NUMBER ]; //这个是epoll_wait检测到发生变化后，把发生变化的文件描述符放到这个数组中

    //主线程不断循环检测 是否有事件发生
    while(true) {
        
        //检测有几个事件发生改变
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        
        if ( ( number < 0 ) && ( errno != EINTR ) ) {
            printf( "epoll failure\n" );
            break;
        }

        //循环遍历事件数组
        for ( int i = 0; i < number; i++ ) {
            
            int sockfd = events[i].data.fd;
            
            if( sockfd == listenfd ) {
                //有客户端连接进来
                
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                
                if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                }

                //目前连接数已经满了
                if( http_conn::m_user_count >= MAX_FD ) {
                    //可以给客户端写一个信息（要写一个报文）
                    close(connfd);
                    continue;
                }
                //将新的客户的数据初始化，放到数组中
                users[connfd].init( connfd, client_address);

            } else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) {
                //对方异常断开 或者 错误等事件
                users[sockfd].close_conn(); //关闭连接

            } else if(events[i].events & EPOLLIN) { //判断是否有读的事件发生
                
                if(users[sockfd].read()) {  //一次性把所有数据读完
                    //读完后加入到pool的工作队列中，交给工作线程处理，等待解析客户端的http数据
                    pool->append(users + sockfd);
                } else {    //没读到 或者 失败了，则关闭连接
                    users[sockfd].close_conn();
                }

            }  else if( events[i].events & EPOLLOUT ) { //判断是否有写的事件发生

                if( !users[sockfd].write() ) {  //一次性写完所有数据
                    users[sockfd].close_conn();
                }

            }
        }
    }
    
    close( epollfd );
    close( listenfd );
    delete [] users;
    delete pool;
    return 0;
}