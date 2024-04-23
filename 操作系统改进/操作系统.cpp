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
        else { // ��ͷ�ڵ�֮�����
            head_->next = newNode;
            newNode->prev = head_;
        }

        if (curr) {
            newNode->next = curr;
            curr->prev = newNode;
        }
        else { // ��β������
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
        else if (!prev && next) { // ɾ��ͷ�ڵ�
            head_->next = next;
            next->prev.reset();
            return true;
        }
        else if (prev && !next) { // ɾ��β�ڵ�
            tail_ = prev;
            prev->next = nullptr;
            return true;
        }
        else if (!prev && !next) { // ������ֻ��һ���ڵ�
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
    std::atomic<int> parallelInserts(0); // ԭ�Ӽ����������ڸ��ٲ��в��������

    // ��������Ĳ��Ժ���
    auto insertTest = [&list, &parallelInserts](int start, int end) {
        std::random_device rd;  // ���������������
        std::mt19937 gen(rd()); // ʹ�� Mersenne Twister �㷨���������

        for (int i = start; i < end; ++i) {
            std::uniform_int_distribution<> dis(1, 100); // ���ɷ�Χ�� 1 �� 100 ֮��������
            int randomValue = dis(gen); // ���������
            // ���Ӳ��в��������
            parallelInserts.fetch_add(1);

            list.Insert(randomValue, list.Find(0).size());
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ģ�⹤��


            std::cout << "�߳� " << std::this_thread::get_id() << " ������ֵ " << randomValue << std::endl;
            std::cout << "��ǰ�������ݣ�";
            list.Print();
        }

        // ���ٲ��в��������
        parallelInserts.fetch_sub(1);
        };

    // ɾ�������Ĳ��Ժ���
    auto eraseTest = [&list](int start, int end) {

        for (int i = start; i < end; ++i) {

            if (!list.Erase(i)) {
                std::cout << "�߳� " << std::this_thread::get_id() << " ɾ��ʧ�ܣ�ָ��λ�ò��������ݣ�" << i << std::endl;
            }
            else {
                std::cout << "�߳� " << std::this_thread::get_id() << " ɾ���ɹ���λ�ã�" << i << std::endl;
            }
            std::cout << "��ǰ�������ݣ�";
            list.Print();
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // ģ�⹤��
        }

        };

    // ��������߳������Բ�����
    std::thread inserter1(insertTest, 1, 6);
    std::thread inserter2(insertTest, 6, 11);
    std::thread eraser1(eraseTest, 1, 3);
    std::thread eraser2(eraseTest, 3, 6);

    std::cout << "�Ѵ����߳� " << std::endl;

    // �ȴ������߳����
    inserter1.join();
    inserter2.join();
    eraser1.join();
    eraser2.join();
    std::cout << "�����߳������ " << std::endl;


    // ��ӡ���в��������
    std::cout << "���в����������" << parallelInserts.load() << std::endl;



    // ��ӡ���������״̬
    std::cout << "��������";
    list.Print();

    return 0;

}

