#include "index_manager.hpp"
#include "skip_list.hpp"  

IndexManager::IndexManager() {
    skipList_ = new SkipList();  
}

IndexManager::~IndexManager() {
    delete skipList_;
}

void IndexManager::put(const std::string& key, const ValueMeta& value) {
    // ValueMeta oldvalue;
    // skipList_->get(key,oldvalue);
    // if(oldvalue.dataPtr!=nullptr){
    //     //.....
    //     ValueStore m(ObjectPool::getInstance()).remove(oldvalue.ptr); 
    // }
    skipList_->insert(key, value);
}

bool IndexManager::get(const std::string& key, ValueMeta& out) const {
    return skipList_->get(key, out);
}

bool IndexManager::remove(const std::string& key) {
    return skipList_->remove(key);
}

void IndexManager::clear() {
    skipList_->clear();
}

void IndexManager::forEach(std::function<void(const std::string&, const ValueMeta&)> func) const {
    skipList_->forEach(func);
}
