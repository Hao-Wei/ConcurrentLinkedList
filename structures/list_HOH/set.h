#include <mylock/lock.h>
#include <atomic>
#include <optional>
#include <iostream>

template <typename K, typename V>
struct Set {
  struct node {
    node* next;
    K key;
    V value;
    bool is_end;
    mylock::mutex lck;
    node(K key, V value, node* next)
      : key(key), value(value), next(next), is_end(false) {};
    node(node* next, bool is_end) // for head and tail
      : next(next), is_end(is_end) {};
  };

  node* head;
  
  Set() { // constructor
    node* tail = new node(nullptr, true); // create dummy tail
    head = new node(tail, false);  // create dummy head
  }

  ~Set() { // destructor
    node* n = head;
    while(!n->is_end) {
      node* tmp = n;
      n = n->next;
      delete tmp;
    }
    delete n;
    head = nullptr;
  }

  auto traverse(K k) {
    node* prev = head;
    prev->lck.lock();
    node* curr = prev->next;
    curr->lck.lock();
    while (!curr->is_end && curr->key < k) {
      prev->lck.unlock();
      prev = curr;
      curr = curr->next;
      curr->lck.lock();
    }
    return std::make_pair(prev, curr); // prev and curr are both locked
  }

  bool insert(K k, V v) {
    auto [prev, curr] = traverse(k);
    if (!curr->is_end && curr->key == k) {
      prev->lck.unlock();
      curr->lck.unlock();
      return false; //already there
    } else {
      node* new_node = new node(k, v, curr);
      prev->next = new_node; // splice in
      prev->lck.unlock();
      curr->lck.unlock();
      return true;
    }
  }

  bool remove(K k) {
    auto [prev, curr] = traverse(k);
    if (curr->is_end || k != curr->key) {
      prev->lck.unlock();
      curr->lck.unlock();
      return false; // not found
    } else {
        node* next = curr->next;
        prev->next = next; // shortcut
        prev->lck.unlock();
        curr->lck.unlock();
        delete curr;
        return true;
    }
  }

  std::optional<V> find(K k) {
    auto [prev, curr] = traverse(k);
    std::optional<V> return_val;
    if (!curr->is_end && curr->key == k) return_val = curr->value; 
    else return_val = {};
    prev->lck.unlock();
    curr->lck.unlock();
    return return_val;
  }

  void print() { // print linked list for debugging purposes
    node* ptr = head->next;
    while (!ptr->is_end) {
      std::cout << ptr->key << ", ";
      ptr = ptr->next;
    }
    std::cout << std::endl;
  }
  
  // check that linked list is sorted and return the number of keys in the linked list
  // cannot be concurrent with operations modifying the linked list
  long count_keys_and_check_consistency() { 
    node* ptr = head->next;
    if (ptr->is_end) return 0;
    K k = ptr->key;
    ptr = ptr->next;
    long i = 1;
    while (!ptr->is_end) {
      i++;
      if (ptr->key <= k) {
        std::cout << "bad key: " << k << ", " << ptr->key << std::endl;
        abort();
      }
      k = ptr->key;
      ptr = ptr->next;
    }
    return i;
  }
};
