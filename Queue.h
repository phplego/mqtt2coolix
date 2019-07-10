
template <int S>
class Queue
{
    private:
        float   values[S]   = {0};
        int     current     = 0;
        int     actualSize  = 0;    // Number of added values. Cannot be grater that S

    public:
        void    add(float value);
        float   average();
        float   min();
        float   max();
};

template <int S>
void Queue<S>::add(float value){
    this->values[this->current] = value;
    this->current ++;
    if(this->current == S)
        this->current = 0;

    this->actualSize = ::min(this->actualSize + 1, S);
}

template <int S>
float Queue<S>::average(){
    float sum = 0;
    for(int i = 0; i < this->actualSize; i++){
        sum += this->values[i];
    }

    return sum / this->actualSize;
}

template <int S>
float Queue<S>::min(){
    return 0.0;
}

template <int S>
float Queue<S>::max(){
    return 0.0;
}