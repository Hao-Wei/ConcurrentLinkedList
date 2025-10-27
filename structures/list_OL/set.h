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
    bool removed;
    mylock::mutex lck;
    node(K key, V value, node* next)
      : key(key), value(value), next(next), is_end(false), removed(false) {};
    node(node* next, bool is_end) // for head and tail
      : next(next), is_end(is_end), removed(false) {};
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
    node* curr = prev->next;
    while (!curr->is_end && curr->key < k) {
      prev = curr;
      curr = curr->next;
    }
    return std::make_pair(prev, curr);
  }

  bool insert(K k, V v) {
    while (true) {
      auto [prev, curr] = traverse(k);
      if (!curr->is_end && curr->key == k && !curr->removed) return false; //already there
      prev->lck.lock();
      if (!prev->removed && prev->next == curr) { //validate
        node* new_node = new node(k, v, curr);
        prev->next = new_node; // splice in
        prev->lck.unlock();
        return true;
      }
      prev->lck.unlock();
    }
  }

  bool remove(K k) {
    while (true) {
      auto [prev, curr] = traverse(k);
      if (curr->is_end || k != curr->key) return false; // not found
      prev->lck.lock();
      if (!prev->removed && prev->next == curr) { // validate
        node* next = curr->next;
        curr->removed = true;
        prev->next = next; // shortcut
        delete curr;
        prev->lck.unlock();
        return true;
      }
      prev->lck.unlock();
    }
  }

  std::optional<V> find(K k) {
    auto [prev, curr] = traverse(k);
    if (!curr->is_end && curr->key == k) return curr->value; 
    else return {};
  }

  // print linked list for debugging purposes
  // cannot be run concurrently with operations modifying the linked list
  void print() {
    node* ptr = head->next;
    while (!ptr->is_end) {
      std::cout << ptr->key << ", ";
      ptr = ptr->next;
    }
    std::cout << std::endl;
  }
  
  // check that linked list is sorted and return the number of keys in the linked list
  // cannot be run concurrently with operations modifying the linked list
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
