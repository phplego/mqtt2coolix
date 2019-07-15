#pragma once


typedef void (*ChangesDetectedCallbackType)(void);
typedef void (*GetValuesCallbackType)(float *);

template <int SZ>
class ChangesDetector {
    const float THRESHOLD = 0.2;

    private:
        float                           values [SZ]                 = {0};    // Remembered values
        ChangesDetectedCallbackType     changesDetectedCallback     = [](){};
        GetValuesCallbackType           getValuesCallback           = [](float *){};
        

    public:
        ChangesDetector();
        void    loop();
        void    setChangesDetectedCallback(void callback(void));
        void    setGetValuesCallback(void callback(float[]));
        void    remember();

};


template <int SZ>
ChangesDetector<SZ>::ChangesDetector()
{
}


template <int SZ>
void ChangesDetector<SZ>::setChangesDetectedCallback(void callback(void))
{
    this->changesDetectedCallback = callback;
}

template <int SZ>
void ChangesDetector<SZ>::setGetValuesCallback(void callback(float *))
{
    this->getValuesCallback = callback;
}


template <int SZ>
void ChangesDetector<SZ>::loop()
{
    float buf[SZ] = {0};

    this->getValuesCallback(buf);

    bool detected = false;
    
    // check each value if changed enough
    for(int i = 0; i < SZ; i++){
        if(std::abs(buf[i] - this->values[i]) >= THRESHOLD){
            detected = true;
        }
    }

    if(detected){
        this->changesDetectedCallback();
        this->remember();
    }
}

template <int SZ>
void ChangesDetector<SZ>::remember()
{
    float buf[SZ] = {0};
    this->getValuesCallback(buf);
    
    for(int i = 0; i < SZ; i++){
        this->values[i] = buf[i];
    }

}