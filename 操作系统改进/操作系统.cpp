#include <iostream>
#include <mutex>
#include <thread>
#include <memory>
#include <vector>
#include <random>
#include <atomic>


template <typename T>
struct Node {
    T data;
    std::shared_ptr<Node<T>> next;
    std::weak_ptr<Node<T>> prev;
    explicit Node(const T& value) : data(value) {}
};

template <typename T>
class ThreadSafeLinkedList {
public:
    ThreadSafeLinkedList() : head_(std::make_shared<Node<T>>(T())), tail_(head_) {}

    void Insert(const T& value, int position) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (position < 0)
            return;

        auto newNode = std::make_shared<Node<T>>(value);

        auto curr = head_->next;
        int index = 0;
        while (curr && index < position) {
            curr = curr->next;
            ++index;
        }

        auto prev = curr ? curr->prev.lock() : tail_;
        if (prev) {
            prev->next = newNode;
            newNode->prev = prev;
        }
        else { // 在头节点之后插入
            head_->next = newNode;
            newNode->prev = head_;
        }

        if (curr) {
            newNode->next = curr;
            curr->prev = newNode;
        }
        else { // 在尾部插入
            tail_ = newNode;
        }
    }


    bool Erase(int position) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (position < 0)
            return false;

        auto curr = head_->next;
        int index = 0;
        while (curr && index < position) {
            curr = curr->next;
            ++index;
        }

        if (!curr)
            return false;

        auto prev = curr->prev.lock();
        auto next = curr->next;
        if (prev && next) {
            prev->next = next;
            next->prev = prev;
            return true;
        }
        else if (!prev && next) { // 删除头节点
            head_->next = next;
            next->prev.reset();
            return true;
        }
        else if (prev && !next) { // 删除尾节点
            tail_ = prev;
            prev->next = nullptr;
            return true;
        }
        else if (!prev && !next) { // 链表中只有一个节点
            head_->next = nullptr;
            tail_ = head_;
            return true;
        }

        return false;
    }


    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_->next = nullptr;
        tail_ = head_;
    }

    std::vector<std::shared_ptr<Node<T>>> Find(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Node<T>>> result;

        auto curr = head_->next;
        while (curr) {
            if (curr->data == value) {
                result.push_back(curr);
            }
            curr = curr->next;
        }

        return result;
    }

    void Print() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto curr = head_->next;
        while (curr) {
            std::cout << curr->data << " ";
            curr = curr->next;
        }
        std::cout << std::endl;
    }

private:
    std::shared_ptr<Node<T>> head_;
    std::shared_ptr<Node<T>> tail_;
    std::mutex mutex_;

};
int main() {
    ThreadSafeLinkedList<int> list;
    std::atomic<int> parallelInserts(0); // 原子计数器，用于跟踪并行插入的数量

    // 插入操作的测试函数
    auto insertTest = [&list, &parallelInserts](int start, int end) {
        std::random_device rd;  // 用于生成随机种子
        std::mt19937 gen(rd()); // 使用 Mersenne Twister 算法生成随机数

        for (int i = start; i < end; ++i) {
            std::uniform_int_distribution<> dis(1, 100); // 生成范围在 1 到 100 之间的随机数
            int randomValue = dis(gen); // 生成随机数
            // 增加并行插入计数器
            parallelInserts.fetch_add(1);

            list.Insert(randomValue, list.Find(0).size());
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作


            std::cout << "线程 " << std::this_thread::get_id() << " 插入了值 " << randomValue << std::endl;
            std::cout << "当前链表内容：";
            list.Print();
        }

        // 减少并行插入计数器
        parallelInserts.fetch_sub(1);
        };

    // 删除操作的测试函数
    auto eraseTest = [&list](int start, int end) {

        for (int i = start; i < end; ++i) {

            if (!list.Erase(i)) {
                std::cout << "线程 " << std::this_thread::get_id() << " 删除失败！指定位置不存在数据：" << i << std::endl;
            }
            else {
                std::cout << "线程 " << std::this_thread::get_id() << " 删除成功！位置：" << i << std::endl;
            }
            std::cout << "当前链表内容：";
            list.Print();
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 模拟工作
        }

        };

    // 创建多个线程来测试并发性
    std::thread inserter1(insertTest, 1, 6);
    std::thread inserter2(insertTest, 6, 11);
    std::thread eraser1(eraseTest, 1, 3);
    std::thread eraser2(eraseTest, 3, 6);

    std::cout << "已创建线程 " << std::endl;

    // 等待所有线程完成
    inserter1.join();
    inserter2.join();
    eraser1.join();
    eraser2.join();
    std::cout << "所有线程已完成 " << std::endl;


    // 打印并行插入的数量
    std::cout << "并行插入的数量：" << parallelInserts.load() << std::endl;



    // 打印链表的最终状态
    std::cout << "最终链表：";
    list.Print();

    return 0;

}

