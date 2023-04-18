## 分支3：用 自旋锁(自己用原子操作实现的) + 信号量

看 threadpool.h 和 lock.h 

```C++
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
```
