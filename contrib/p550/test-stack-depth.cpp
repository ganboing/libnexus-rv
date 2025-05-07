#include <cstdio>

constexpr unsigned depth = 1030;

void *addr_func[depth];
void *addr_ret[depth];

typedef int (*func_ptr)();

template<unsigned D>
int test_func() {
    static volatile func_ptr fp = &test_func<D-1>;
    addr_ret[D] = __builtin_return_address(0);
    addr_func[D-1] = (void*)fp;
    return fp() + D;
}

template<>
int test_func<0>() {
    addr_ret[0] = __builtin_return_address(0);
    return 0;
}

int main() {
    addr_func[depth-1] = (void*)&test_func<depth-1>;
    test_func<depth-1>();
    for (unsigned i = depth; i > 0; --i)
        printf("addr_func[%d]=%p\n", i-1, addr_func[i-1]);
    for (unsigned i = 0; i < depth; ++i)
        printf("addr_ret[%d]=%p\n", i, addr_ret[i]);
    return 0;
}
