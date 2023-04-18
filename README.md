## 分支2：用 自旋锁 + 信号量

用的自旋锁是pthread库里面自带的 pthread_spinlock_t ；

直接看threadpool里面的区别就行；

记得自旋锁要初始化，不然用不了：pthread_spin_init(&my_lock,0);



