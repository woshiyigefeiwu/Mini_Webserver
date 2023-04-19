#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/*
    线程同步机制封装类

    这里把实现和头文件写一块了
*/

// 封装一下 互斥锁类
class locker {

    private:
        pthread_mutex_t m_mutex;    //互斥锁

    public:
        locker() {  //构造函数：创建互斥锁
            if(pthread_mutex_init(&m_mutex, NULL) != 0) {
                throw std::exception(); //出错了，抛出异常
            }
        }

        ~locker() { //析构函数：销毁互斥锁
            pthread_mutex_destroy(&m_mutex);
        }

        bool lock() {  //上锁，pthread_mutex_lock 成功返回0
            return pthread_mutex_lock(&m_mutex) == 0;
        }

        bool unlock() { //解锁，pthread_mutex_unlock 成功返回0
            return pthread_mutex_unlock(&m_mutex) == 0;
        }

        pthread_mutex_t *get()  //获取一个锁
        {
            return &m_mutex;
        }
};


/*
    条件变量:
        不是锁，不能解决数据安全问题！但是能配合互斥锁一起用。
            作用：
                - 某个条件满足以后，引起 线程阻塞；
                - 某个条件满足以后，接触 线程阻塞；

    具体看 unit_3/lesson_13/cond.c
*/
// 封装一下 条件变量类（配合互斥锁一起使用的）
class cond {

    private:
        pthread_cond_t m_cond;  //条件变量

    public:
        cond(){
            if (pthread_cond_init(&m_cond, NULL) != 0) {
                throw std::exception();
            }
        }
        ~cond() {
            pthread_cond_destroy(&m_cond);
        }

        // - 等待，调用了该函数，线程会阻塞。
        bool wait(pthread_mutex_t *m_mutex) {
            int ret = 0;
            ret = pthread_cond_wait(&m_cond, m_mutex);
            return ret == 0;
        }

        // - 等待多长时间，调用了这个函数，线程会阻塞，直到指定的时间结束。
        bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
            int ret = 0;
            ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
            return ret == 0;
        }

        // - 唤醒一个或者多个等待的线程
        bool signal() {
            return pthread_cond_signal(&m_cond) == 0;
        }

        // - 唤醒所有的等待的线程
        bool broadcast() {
            return pthread_cond_broadcast(&m_cond) == 0;
        }
};


// 封装一下 信号量类（具体看 unit_3/lesson_14/semaphore.c）
class sem {

    private:
        sem_t m_sem;

    public:
        sem() {
            if( sem_init( &m_sem, 0, 0 ) != 0 ) {
                throw std::exception();
            }
        }
        sem(int num) {
            if( sem_init( &m_sem, 0, num ) != 0 ) {
                throw std::exception();
            }
        }
        ~sem() {
            sem_destroy( &m_sem );
        }

        // 等待信号量
        bool wait() {
            return sem_wait( &m_sem ) == 0;
        }
        
        // 增加信号量
        bool post() {
            return sem_post( &m_sem ) == 0;
        }
};

#endif