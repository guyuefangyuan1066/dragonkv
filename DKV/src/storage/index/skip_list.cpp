#include "skip_list.hpp"
//#include"../../memory/object_pool.hpp"
int SkipList::randomLevel() const{
    int level = 1;
    while (dist_(randEngine_) < probability_ && level < maxLevel_) {
        ++level;
    }
    return level;
}

void SkipList::clear(){
    Node* curr = head_->forward[0];
    while (curr) {
        Node* next = curr->forward[0];
        delete curr;  
        curr = next;
    }
    for (int i = 0; i < currentLevel_; ++i) {
        head_->forward[i] = nullptr;
    }

    currentLevel_ = 1;
}

SkipList::SkipList(int maxLevel, float probability)
    : maxLevel_(maxLevel), currentLevel_(1), probability_(probability) {
    std::srand(std::time(nullptr));
    head_ = new Node("", ValueMeta{nullptr, 0, 0}, maxLevel_);
}

SkipList::~SkipList() {
    Node* curr = head_;
    while (curr) {
        Node* next = curr->forward[0];
        delete curr;
        curr = next;
    }
}

bool SkipList::insert(const std::string& key, const ValueMeta& value){
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::vector<Node*> update(maxLevel_, nullptr);
    Node* curr = head_;
    for (int i = currentLevel_ - 1; i >= 0; --i) {
        while (curr->forward[i] && curr->forward[i]->key < key) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }
    curr = curr->forward[0];
    if (curr && curr->key == key) {
        curr->value = value; 
        return false;         
    }    
    int nodeLevel = randomLevel();
    if (nodeLevel > currentLevel_) {
        for (int i = currentLevel_; i < nodeLevel; ++i) {
            update[i] = head_;
        }
        currentLevel_ = nodeLevel;
    }

    Node* newNode = new Node(key, value, nodeLevel);

    //Node* newNode = new Node(keyInPool, key.size(), value, nodeLevel);

    for (int i = 0; i < nodeLevel; ++i) {
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }

    return true; 
}

bool SkipList::remove(const std::string& key){
    std::unique_lock<std::shared_mutex> lock(mutex_);
    Node* curr = head_;
    std::vector<Node*> update(maxLevel_, nullptr); // 用于记录每层的前驱节点
    // Step 1: 找到所有层中 key 节点的前驱
    for (int i = currentLevel_ - 1; i >= 0; --i) {
        while (curr->forward[i] && curr->forward[i]->key < key) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }
    // Step 2: 检查是否存在目标 key
    curr = curr->forward[0];
    if (curr && curr->key == key) {
        // Step 3: 更新所有层的 forward 指针，跳过 curr 节点
        for (int i = 0; i < currentLevel_; ++i) {
            if (update[i]->forward[i] != curr) break;
            update[i]->forward[i] = curr->forward[i];
        }
        // Step 4: 释放该节点内存
        delete curr;
        // Step 5: 调整 currentLevel_
        while (currentLevel_ > 1 && head_->forward[currentLevel_ - 1] == nullptr) {
            --currentLevel_;
        }
        return true;
    }
    return false;
}

bool SkipList::get(const std::string& key, ValueMeta& out) const{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    Node* curr = head_;
    for (int i = currentLevel_ - 1; i >= 0; --i) {
        while (curr->forward[i] && curr->forward[i]->key < key) {
            curr = curr->forward[i];
        }
        
    }
    curr = curr->forward[0];
    if (curr && curr->key == key) {
        out = curr->value;
        return true;
    }
    return false;

}

void SkipList::forEach(const std::function<void(const std::string&, const ValueMeta&)>& func) const{
    Node* curr = head_->forward[0];  
    while (curr) {
        func(curr->key, curr->value);
        curr = curr->forward[0]; 
    }
}


// struct node{
//     node*right;
//     node*down;
//     std::string key;
//     ValueMeta value;
// }