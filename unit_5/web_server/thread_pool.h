#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

/*
    实现线程池类，主要包括下面的东西：

    一个线程池：pthread_t * m_threads;
    以及有关线程池的一些属性：
        - int m_thread_number; 线程的数量

    一个请求队列：std::list< T* > m_workqueue;
    以及有关请求队列的一些属性：
        - int m_max_requests; 
        - locker m_queuelocker;

    以及一些其他的 数据成员 和 成员函数：
        - sem m_queuestat;  信号量，判断是否有任务需要处理
        - bool m_stop;    是否结束线程这个类 
        
        - threadpool()
        - ~threadpool()
        - bool append(T* request);  往工作队列当中去添加任务
        - static void* worker(void* arg);   工作线程运行的函数
        - void run();   让这个线程池这个跑起来

    ---------------------------------------------------------
    这里一定要理解什么是类模板！！！（不懂看书c++ prinme 第583页）

    如果：
    template<typename T> class Bob{
        T val;
    }

    那么：
    Bob<int> bob; 则等价于把bob这个对象里面 T val;变成 int val；
    Bob<string> bob; 则等价于把bob这个对象里面 T val;变成 string val；
    ---------------------------------------------------------
*/


// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template<typename T> class threadpool {

    public:
        /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
        threadpool(int thread_number = 8, int max_requests = 10000);
        ~threadpool();
        bool append(T* request);    //往工作队列当中去添加任务

    private:
        /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
        static void* worker(void* arg); //注意这里要静态函数（全局）
        void run();     //让这个线程池这个跑起来

    private:
        // 线程的数量
        int m_thread_number;
        
        // 描述线程池的数组，大小为m_thread_number    
        pthread_t * m_threads;  //线程池

        // 请求队列中最多允许的、等待处理的请求的数量  
        int m_max_requests; 

        // 请求队列（注意访问这个队列的时候不能同时访问，即这个队列是互斥资源，所以下面要用互斥锁）
        std::list< T* > m_workqueue;  

        // 保护请求队列的互斥锁
        locker m_queuelocker;

        // 信号量，判断是否有任务需要处理
        sem m_queuestat;

        // 是否结束线程这个类         
        bool m_stop;                    
};

// 下面是类中函数的实现

template< typename T >  //构造函数
threadpool< T >::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), 
        m_stop(false), m_threads(NULL) //冒号后面，初始化（c++语法）
{

    //不合法
    if((thread_number <= 0) || (max_requests <= 0) ) {
        throw std::exception();
    }

    //创建一个线程池
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw std::exception();
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程(自己释放资源)。
    for ( int i = 0; i < thread_number; ++i ) {
        printf( "create the %dth thread\n", i); //打印正在创建第几个线程

        //创建失败
        if(pthread_create(m_threads + i, NULL, worker, this ) != 0) {
            delete [] m_threads;    //释放掉
            throw std::exception();
        }
        
        //设置线程分离
        if( pthread_detach( m_threads[i] ) ) {
            delete [] m_threads;    //释放掉
            throw std::exception();
        }
    }
}

template< typename T >  //析构函数
threadpool< T >::~threadpool() {
    delete [] m_threads;    //释放线程池数组
    m_stop = true;
}

template< typename T >  //往工作队列当中去添加任务（注意要保证线程同步）
bool threadpool< T >::append( T* request )
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queuelocker.lock();   //上锁
    if ( m_workqueue.size() > m_max_requests ) {  //不能继续添加任务了
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request); 
    m_queuelocker.unlock(); //解锁
    m_queuestat.post(); //工作队列的信号量增加
    return true;
}

/*
    这里细节了！
    注意到worker是一个静态函数，它是不能访问类里的私有成员的，那么如何获取对象的数据成员呢？

    注意到pthread_create(m_threads + i, NULL, worker, this )里面最后一个参数是这个worker的参数；
    而且这个pthread_create是类里实现的，所以我们可以直接传入this指针（这个threadpool对象的指针）；
    这样我们就能通过这个this去访问到数据成员了！
*/
template< typename T >  //工作线程运行的函数
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >  //让这个线程池这个跑起来
void threadpool< T >::run() {

    while (!m_stop) {
        m_queuestat.wait(); //这里是默认阻塞的，如果有工作要处理，则信号量wait
        m_queuelocker.lock();   //此时要操作工作队列，所以上锁
        if ( m_workqueue.empty() ) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();   //从m_workqueue中取出一个任务交给process()处理
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( !request ) {
            continue;
        }
        request->process(); //process()是做任务的函数
    }
}

#endif
