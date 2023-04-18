#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
#include <atomic>
using namespace std;

// 信号量类
class sem
{
private:
    sem_t m_sem;
public:
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){    // 信号量值初始化为0 （消费者信号量）
            throw std::exception();
        }
    }

    sem(int num){           // 传参创建
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }
    
    ~sem(){                 // 释放
        sem_destroy(&m_sem);
    }

    bool wait(){            // 等待（减少）信号量
        return sem_wait(&m_sem) == 0;
    }

    bool post(){            // 增加信号量
        return sem_post(&m_sem) == 0;
    }
};

//这里实现了一个自旋锁，直接用自旋锁就行
class SpinLock
{
    public:
        SpinLock(): flag(false) {}
        SpinLock(bool fl): flag(fl) {}

        void lock()
        {
            bool tem_flag = false;
            while(!flag.compare_exchange_weak(tem_flag,true)) tem_flag = false;
        }

        void unlock()
        {
            flag.store(false);
        }

    private:
        atomic<bool> flag;  //原子类型，false表示没被抢占，true表示被抢占了
}; 
 

#endif
