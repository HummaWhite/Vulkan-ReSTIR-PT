#include "Renderer.h"

int main() {
    std::string scene;
    //scene = "res/box2.xml";
    //scene = "res/box3.xml";
    //scene = "res/box4.xml";
    //scene = "res/box5.xml";
    //scene = "res/box6.xml";
    //scene = "res/box7.xml";
    //scene = "res/box2_fake.xml";
    //scene = "res/sponza.xml";
    //scene = "res/staircase2.xml";
    //scene = "res/model/VeachAjar/ajar.xml";
    //scene = "res/model/VeachAjar/ajar_fake.xml";
    scene = "res/model/VeachAjar/ajar_fake2.xml";
    //scene = "res/model/San_Miguel/san-miguel.xml";
    Renderer renderer("ReSTIR PT", 1920, 1080, scene);
    renderer.exec();
}
