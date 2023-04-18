// /*
//     无锁队列的实现：通过 CAS操作(compare_and_swap) 来实现；
//     （实际上就是 CAS操作 + 严密的逻辑判断 实现无锁）

//     具体参考下面两篇文章：
//         https://coolshell.cn/articles/8239.html
//         https://blog.csdn.net/weixin_42250655/article/details/108583128?ops_request_misc=&request_id=&biz_id=102&utm_term=c++%E6%97%A0%E9%94%81%E9%98%9F%E5%88%97&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-4-108583128.142^v76^insert_down2,201^v4^add_ask,239^v2^insert_chatgpt&spm=1018.2226.3001.4449

//     原型是这样的：
//     int compare_and_swap (int* reg, int oldval, int newval)
//     {
//         int old_reg_val = *reg;
//         if (old_reg_val == oldval) 
//         {
//             *reg = newval;
//         }
//         return old_reg_val;
//     }
//     意思就是说，看一看内存*reg里的值是不是oldval，如果是的话，则对其赋值newval；

//     我们封装一下，变成bool类型的，更容易用：
//     bool compare_and_swap (int *addr, int oldval, int newval)
//     {
//         if ( *addr != oldval ) {
//             return false;
//         }
//         *addr = newval;
//         return true;
//     }

//     下面开始敲代码

//     这里需要注意的是，直接用队列的话显然是线程不安全的；
//     我们需要通过对 插入 删除 的细节逻辑判断来实现线程安全；
//     这里的链表是带头节点的。
// */
// #include <iostream>
// using namespace std;

// template <typename T>
// bool CAS(T *addr, T oldval, T newval)
// {
//     if(*addr != oldval) return false;  //发生了改变

//     *addr = newval;
//     return true;
// }

// // 模板类 的 无锁队列（链表模拟队列）
// template <typename T>
// class unlock_queue
// {
// private:
//     struct node //链表的节点
//     {
//         T date;
//         node *next;

//         node(): date(), next(nullptr) {}
//         node(T &da): date(da), next(nullptr) {}
//         node(const T &da): date(da), next(nullptr) {}
//     };

//     node *head,*tail;   //链表的头指针和尾指针

// public:
//     unlock_queue(): head(new node()), tail(head) { }
//     ~unlock_queue()
//     {
//         node *tem;
//         while(head != nullptr)
//         {
//             tem = head;     //这两个顺序很重要！！！别搞反了...
//             head = head->next;  
//             delete tem;
//         }
//         tem = head = tail = nullptr;
//     }

//     void q_push(const T& date) //队列插入元素
//     {
//         //创建一个节点
//         node *new_node = new node(date);
//         node *old_tail, *old_tail_next; //当前时刻的指针

//         while(true)
//         {
//             // 取出当前时刻的指针
//             old_tail = tail;
//             old_tail_next = old_tail->next;

//             // 当前tail指针移动了，说明有其他线程进行插入操作，则跳过
//             if(old_tail != tail) continue;

//             /*
//                 这里 old_tail != nullptr 说明其他线程已经插入了节点；
//                 但是 tail 指针还没更新，此时 old_tail 是= tail 的

//                 而且不能直接contiue，每个线程要有主动的移动 tail；
//                 防止前面线程更新节点，但是再更新tail之前挂了，导致所有线程阻塞的问题；
//             */
//             if(old_tail_next != nullptr)
//             {
//                 CAS(&tail, old_tail, old_tail_next);
//                 // __sync_bool_compare_and_swap(&tail, old_tail, old_tail_next);
//                 continue;
//             }

//             //否则就是可以更新的，添加节点
//             if(CAS(&(old_tail->next), old_tail_next, new_node)) break;
//             // if(__sync_bool_compare_and_swap(&(old_tail->next), old_tail_next, new_node)) break;
//         }

//         // 不管是不是我们这个线程添加的节点，我们都要保证尾指针指向最后一个节点
//         CAS(&tail,old_tail,new_node);
//         // __sync_bool_compare_and_swap(&tail,old_tail,new_node);
//         // cout<<"q_push  "<<date<<'\n';
//     }

//     bool q_try_pop(T &date)      //队列尝试弹出元素
//     {
//         node *old_head,*old_tail, *first_node;
//         while(true)
//         {
//             //取出头指针，尾指针，和第一个元素的指针
//             old_head = head;
//             old_tail = tail;

//             /*
//                 这里是判断是防止：
//                     在这里被其他线程成功弹出后，其释放了old_head;
//                     而下面使用了old_head导致的 悬空指针访问内存的情况
//             */
//             // if(old_head == nullptr) continue;
//             first_node = old_head->next;

//             //有线程成功弹出了
//             if(old_head != head) continue;

//             if(old_head == old_tail)
//             {
//                 if(first_node == nullptr) return false; //队列为空

//                 //说明有线程弹出了
//                 CAS(&tail,old_tail,first_node);
//                 // __sync_bool_compare_and_swap(&tail,old_tail,first_node);
//                 continue;
//             }

//             //前面的操作都是在确保全局尾指针在全局头指针之后，只有这样才能安全的删除数据
//             date = first_node->date;
//             if(CAS(&head,old_head,first_node)) break;    //向后移动
//             // if(__sync_bool_compare_and_swap(&head,old_head,first_node)) break;
//         }

//         delete old_head;
//         // old_head = nullptr;
        
//         return true;
//     }
// };

#include <atomic>
#include <memory>
#include <vector>

template <typename T> class unlock_queue 
{
  // 用链表实现LockFreeQueue  Node定义节点的类型
private:
  struct Node 
  {
    T data_;
    Node *next_;
    Node() : data_(), next_(nullptr) {}
    Node(T &data) : data_(data), next_(nullptr) {}
    Node(const T &data) : data_(data), next_(nullptr) {}
  };

private:
  Node *head_, *tail_;

public:
  unlock_queue() : head_(new Node()), tail_(head_) {}
  ~unlock_queue() 
  {
    Node *tmp;
    while (head_ != nullptr) 
    {
      tmp = head_;
      head_ = head_->next_;
      delete tmp;
    }
    tmp = nullptr;
    tail_ = nullptr;
  }

  bool Try_Dequeue(T &data) 
  {
    Node *old_head, *old_tail, *first_node;
    while(true) 
    {
      // 取出头指针，尾指针，和第一个元素的指针
      old_head = head_;
      old_tail = tail_; // 前面两步的顺序非常重要，一定要在获取old_tail之前获取old_head
                        // 保证old_head_不落后于old_tail
      first_node = old_head->next_;

      // 下面的工作全是在确保上面三个指针的“一致性”
      // 1. 保证head_的一致性
      /* 
         判断head_指针是否已经移动
         如果head已经被改变我们需要重新再去获取它
      */
      if (old_head != head_) 
      {
        continue;
      }

      // 2. 保证尾指针落后于头指针 (尾指针与全局尾指针部分一致性保持)
      // 空队列或者全局尾指针落后
      // 落后是指其他线程的已经更新，而当前线程没有更新
      if (old_head == old_tail) 
      {
        //这里是队列为空，返回false
        if (first_node == nullptr) 
        {
          return false;
        }
        // 证明全局尾指针没有被更新，尝试更新一下 “主动操作”
        //尾指针落后了，所以我们要更新尾指针，然后在循环一次
        ::__sync_bool_compare_and_swap(&tail_, old_tail, first_node);
        continue;
      } 
      else // 前面的操作都是在确保全局尾指针在全局头指针之后，只有这样才能安全的删除数据
      {
        // 在CAS前先取出数据，防止其他线程dequeue造成数据的缺失
        data = first_node->data_;
        // 在取出数据后，如果head指针没有落后，移动 old_head 指针成功则退出
        if (::__sync_bool_compare_and_swap(&head_, old_head, first_node)) 
        {
          break;
        }
      }
    }
    delete old_head;
    return true;
  }

  void Enqueue(const T &data) 
  {
    Node *enqueue_node = new Node(data);
    Node *old_tail, *old_tail_next;

    while(true) 
    {
      //先取一下尾指针和尾指针的next
      old_tail = tail_;
      old_tail_next = old_tail->next_;

      //如果尾指针已经被移动了，则重新开始
      if (old_tail != tail_) 
      {
        continue;
      }

      // 判断尾指针是否指向最后一个节点
      if (old_tail_next == nullptr) 
      {
        if (::__sync_bool_compare_and_swap(&(old_tail->next_), old_tail_next,
                                           enqueue_node)) 
        {
          break;
        }
      } 
      else 
      {
        // 全局尾指针不是指向最后一个节点，就把全局尾指针向后移动

        // 全局尾指针不是指向最后一个节点，发生在其他线程已经完成节点添加操作，
        // 但是并没有更新最后一个节点，此时，当前线程的(tail_和old_tail是相等的，)
        // 可以更新全局尾指针为old_tail_next，如果其他线程不更新全局尾指针，
        // 那么当前线程会不断移动，直到  old_tail_next == nullptr 为true

        // 为什么这里不直接Continue 原因是:
        // 如果其他线程添加了节点，但是并没有更新
        // 全局尾节点，就会导致所有的线程原地循环等待，所以每一个线程必须要有一些
        // “主动的操作” 去获取尾节点，这种思想在 dequeue的时候也有体现
        ::__sync_bool_compare_and_swap(&(tail_), old_tail, old_tail_next);
        continue;
      }
    }
    // 重置尾节点, 也有可能已经被别的线程重置，那么当前线程就不用管了
    /*
      不用管是因为我们已经将节点成功添加，每一次向队列中加入数据的时候都要保证将tail指针移动到最尾端
    */
    ::__sync_bool_compare_and_swap(&tail_, old_tail, enqueue_node);
  }
};



