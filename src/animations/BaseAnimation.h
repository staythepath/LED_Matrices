#ifndef BASEANIMATION_H
#define BASEANIMATION_H

// A generic animation interface
class BaseAnimation {
public:
    virtual ~BaseAnimation() {}

    // Called once at startup (or when this animation is selected)
    virtual void begin() = 0;

    // Called repeatedly in loop
    virtual void update() = 0;
};

#endif // BASEANIMATION_H
