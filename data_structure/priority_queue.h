#ifndef PRIORITY_QUEUE__H
#define PRIORITY_QUEUE__H

#include<vector>

template <typename T>
class priority_queue{
    typedef bool(*cmp)(T,T);
private:
    size_t m_size;
    std::vector<T> data;
    cmp compare_function;
    bool default_cmp(T a1,T,a2){return a1<a2;}
public:

    priority_queue(size_t _size=0,cmp func=default_cmp);
    ~priority_queue();
    T top();
    
};


#endif