
#include <hwi/include/bqc/A2_inlines.h>

#include <iostream>

int main()
{
    std::cout << "getting time base\n";
    unsigned long time = GetTimeBase();
    std::cout << "time base is: " << time << "\n";
    std::cout << "reducing thread priority\n";
    ThreadPriority_Low();
    std::cout << "Testcase succesful\n";
}


