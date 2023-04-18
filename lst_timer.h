#ifndef LST_TIMER
#define LST_TIMER

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "http_conn.h"
#include "locker.h"

/*
    定时器的作用：
        由于客户端不主动关闭，或者没有错误，则不会关闭连接，此时会占用文件描述符；
        因此，定时器用于定时检测不活跃的连接，然后释放掉连接。

    那么对于http_conn这个类，它就有一个静态数据成员：定时器链表类；
    定时器链表类 里面包括一张 定时器链表 和 操作链表的函数；
    每个http_conn对象都有一个定时器(类)，然后这些定时器又组成了一张链表；
    （可以说 http_conn类 和 定时器类 是相互包含的，因为都要用到对方）
    定时器链表 中的每个节点都对应着一个http_conn对象，超时了我们就断开此客户端连接，从链表中删除定时器；

    关键操作在于定时器链表咋写：
        定时器链表，它是一个升序、双向链表，且带有头节点和尾节点。
*/

class http_conn;   // 前向声明

// 定时器类
class util_timer {
    public:
        util_timer() : prev(NULL), next(NULL){}

    public:
    time_t expire;   // 任务超时时间，这里使用绝对时间
    http_conn* user_data; 
    util_timer* prev;    // 指向前一个定时器
    util_timer* next;    // 指向后一个定时器
};

// 定时器链表，它是一个升序、双向链表，且带有头节点和尾节点。
class sort_timer_lst {
    public:
        sort_timer_lst() : head( NULL ), tail( NULL ) {}
        // 链表被销毁时，删除其中所有的定时器
        ~sort_timer_lst() {
            util_timer* tmp = head;
            while( tmp ) {
                head = tmp->next;
                delete tmp;
                tmp = head;
            }
        }
        
        // 将目标定时器timer添加到链表中
        void add_timer( util_timer* timer ); 
        
        /* 当某个定时任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的
        超时时间延长的情况，即该定时器需要往链表的尾部移动。*/
        void adjust_timer(util_timer* timer);
    
        // 将目标定时器 timer 从链表中删除
        void del_timer( util_timer* timer );  

        /* SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务。*/
        void tick(); 

    private:
        /* 一个重载的辅助函数，它被公有的 add_timer 函数和 adjust_timer 函数调用
        该函数表示将目标定时器 timer 添加到节点 lst_head 之后的部分链表中 */
        void add_timer(util_timer* timer, util_timer* lst_head); 

    private:
        util_timer* head;   // 头结点
        util_timer* tail;   // 尾结点
};

#endif
