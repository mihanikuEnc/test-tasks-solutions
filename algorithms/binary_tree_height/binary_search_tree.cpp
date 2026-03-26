#include <iostream>
#include <algorithm>

class Node
{

public:
    Node(unsigned int value) : val_(value)
    {
    }

    unsigned int val_;
    Node* left_ = nullptr;
    Node* right_ = nullptr;
};

class Tree
{
public:
    Tree() = default;

    void Insert(unsigned int value)
    {
        root_ = InsertRec(root_, value);
    }

    size_t FindDeep()
    {
        return FindDeepRec(root_);
    }

private:
    size_t deep = 0;
    Node* root_ = nullptr;

private:

    Node* InsertRec(Node* node, unsigned int value)
    {
        if (node == nullptr)
        {
            return new Node(value);
        }
        if (node->val_ > value)
        {
            node->left_ = InsertRec(node->left_, value);
        }
        if (node->val_ < value)
        {
            node->right_ = InsertRec(node->right_, value);
        }
        return node;
    }

    size_t FindDeepRec(Node* node)
    {
        if (node == nullptr)
        {
            return 0;
        }
        return 1 + std::max(FindDeepRec(node->left_), FindDeepRec(node->right_));
    }
};

int main()
{
    Tree bin_tree;
    int new_node_value;
    std::cin >> new_node_value;
    while (new_node_value != 0)
    {
        bin_tree.Insert(new_node_value);
        std::cin >> new_node_value;
    }
    std::cout << bin_tree.FindDeep();

}
