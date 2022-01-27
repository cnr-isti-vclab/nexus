#include <utility>

auto func(int i)
{
    return [i=std::move(i)](int b){return b+i;};
} 

int main() 
{ 
    int num = func(3)(5);
    // use num
}
