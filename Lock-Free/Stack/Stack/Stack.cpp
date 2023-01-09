#include <iostream>
#include <atomic>
#include <optional>
#include <cassert>

struct Node
{
    uint16_t Data{};
    struct Node* Next{ nullptr };

    Node(uint16_t Value) : Data(Value) {}
};

class Stack
{
    std::atomic<Node*> Top;

public:
    
    std::optional<uint16_t> Peak()
    {
        std::optional<uint16_t> top;
        if (Top)
        {
            top = Top.load()->Data;
        }
        return top;
    }

    void Pop()
    {
        Node* node = Top;
        while (Top != nullptr)
        {
            bool success = (node != nullptr) && (Top.compare_exchange_weak(node, node->Next));
            if (node != nullptr && success)
            {
                delete node;
                node = nullptr;
                return;
            }
        }
    }

    void Push(uint16_t Data)
    {
        Node* node = new(std::nothrow) Node(Data);
        if (node != nullptr)
        {
            while (true)
            {
                node->Next = Top;
                bool success = Top.compare_exchange_weak(node->Next, node);
                if (success)
                {
                    return;
                }
            }
        }
    }
};

void Test_Peak()
{
    Stack s;
    std::optional<uint16_t> data = s.Peak();
    assert(!data.has_value());
}

void Test_Push()
{
    Stack s;
    s.Push(10);
    assert(s.Peak().value() == 10);

    s.Push(20);
    assert(s.Peak().value() == 20);

    s.Push(30);
    assert(s.Peak().value() == 30);
}

void Test_Pop()
{
    Stack s;
    assert(!s.Peak().has_value());
    s.Pop();
    assert(!s.Peak().has_value());


    s.Push(50);
    s.Push(40);
    s.Push(30);
    s.Push(20);
    s.Push(10);
    assert(s.Peak() == 10);

    s.Pop();
    assert(s.Peak() == 20);

    s.Pop();
    assert(s.Peak() == 30);

    s.Pop();
    assert(s.Peak() == 40);

    s.Pop();
    assert(s.Peak() == 50);

    s.Pop();
    assert(!s.Peak().has_value());

    s.Pop();
    assert(!s.Peak().has_value());
}

int main()
{
    Test_Peak();
    Test_Push();
    Test_Pop();

    //TODO: Add more test for testing concurrency
    return 0;
}