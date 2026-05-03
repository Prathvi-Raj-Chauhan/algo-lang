#include <iostream>
#include <ctime>


// function to return a pseudo random number using xorshift32 
unsigned int xorshift(unsigned int &x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static unsigned int x = time(0); // persistant seed to maintain randomness even in quick repeated generations

int randInRange(int lb, int ub) {

    unsigned int range = ub - lb + 1;
    unsigned int limit = UINT32_MAX - (UINT32_MAX % range);

    unsigned int val;
    do {
        val = xorshift(x);
    } while (val >= limit);

    return lb + (val % range);
}
int main(){
    int a = 5;
    int b = 500;
    for(int i = 0; i < 50; i++){
        int rand = randInRange(0,100000000);
        std::cout << rand << std::endl;
    }

    return 0;
}
