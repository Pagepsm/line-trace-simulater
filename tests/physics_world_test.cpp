#include "simulation/PhysicsWorld.h"
#include <iostream>

int main() {
    for (int i = 0; i < 100; ++i) {
        lt::PhysicsWorld first;
        {
            lt::PhysicsWorld second;
            if (!second.valid())
                return 1;
        }
        if (!first.valid())
            return 2;
    }
    std::cout << "200 world creations/destructions; surviving world remains valid\n";
}
