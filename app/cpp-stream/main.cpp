#include <iostream>

extern "C"
{
    void __libc_init_array(void);
}

int main()
{
    __libc_init_array();

    std::cout << "What's your name?" << std::endl;
    std::string name;
    std::cin >> name;
    std::cout << "Hello, " << name << "!" << std::endl;
    return 0;
}