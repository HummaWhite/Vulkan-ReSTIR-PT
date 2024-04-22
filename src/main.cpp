#include "Renderer.h"

int main() {
    std::string scene;
    //scene = "res/box.xml";
    //scene = "res/box2.xml";
    //scene = "res/box2_a.xml";
    //scene = "res/box_fake.xml";
    //scene = "res/mis.xml";
    //scene = "res/box5.xml";
    //scene = "res/box6.xml";
    //scene = "res/box7.xml";
    //scene = "res/box8.xml";
    //scene = "res/box2_fake.xml";
    //scene = "res/sponza.xml";
    //scene = "res/sponza_metal.xml";
    //scene = "res/sponza_manylight.xml";
    //scene = "res/sponza_metal2.xml";
    //scene = "res/staircase2.xml";
    scene = "res/model/VeachAjar/ajar.xml";
    //scene = "res/model/VeachAjar/ajar_fake.xml";
    //scene = "res/model/VeachAjar/ajar_fake2.xml";
    //scene = "res/model/San_Miguel/san-miguel.xml";
    //scene = "res/box3.xml";
    //scene = "res/box4.xml";
    //scene = "res/box4_a.xml";
    //scene = "res/zbidir.xml";
    //scene = "res/fireplace.xml";
    Renderer renderer("ReSTIR PT", 1280, 720, scene);
    renderer.exec();
}
