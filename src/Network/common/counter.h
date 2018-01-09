#ifndef COUNTER_H_
#define COUNTER_H_
namespace hdcs {
namespace networking {
// free lock couter.
// when muitl-thread compete to obtain sequence id, using this couter can avoid lock.
template <typename T>
inline T atomic_inc_ret_old64(volatile T* n)
{
    T r = 1;
    asm volatile ("lock; xaddq %1, %0;":"+m"(*n), "+r"(r)::"cc");
    return r;
}

template <typename T>
inline T atomic_dec_ret_old64(volatile T* n)
{
    T r = (T)-1;
    asm volatile ("lock; xaddq %1, %0;":"+m"(*n), "+r"(r)::"cc");
    return r;
}

template <typename T>
inline T atomic_swap(volatile T* lockword, T value)
{
    asm volatile ("lock; xchg %0, %1;" : "+r"(value), "+m"(*lockword));
    return value;
}

class AtomicCounter64
{
public:
    AtomicCounter64() : _counter(0) {}
    AtomicCounter64(uint64_t init) : _counter(init) {}
    uint64_t operator ++ ()
    {
        return atomic_inc_ret_old64(&_counter) + 1LU;
    }
    uint64_t operator -- ()
    {
        return atomic_dec_ret_old64(&_counter) - 1LU;
    }
    operator uint64_t () const
    {
        return _counter;
    }
private:
    volatile uint64_t _counter;
};

}
}

#endif
