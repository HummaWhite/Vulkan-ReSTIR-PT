#include "Renderer.h"

int main() {
    Renderer renderer("ReSTIR PT", 1920, 1080, "res/box2.xml");
    renderer.exec();
}
