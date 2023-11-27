#include "Renderer.h"

int main() {
    //const std::string scene = "res/box2.xml";
    //const std::string scene = "res/staircase2.xml";
    const std::string scene = "res/model/VeachAjar/ajar.xml";
    Renderer renderer("ReSTIR PT", 1920, 1080, scene);
    renderer.exec();
}
