
#include<atomic>

namespace mylock {
    struct mutex {
        std::atomic<bool> b; // true iff locked
        mutex() : b(false) {};  // constructor

        void lock() {
            while(true) {
                bool old = false;
                if(b.compare_exchange_strong(old, true, std::memory_order_seq_cst))
                    break;
            }
        }

        bool try_lock() {
            bool old = false;
            return b.compare_exchange_strong(old, true, std::memory_order_seq_cst);
        }

        void unlock() {
            b.store(false, std::memory_order_seq_cst);
        }
    };
}
