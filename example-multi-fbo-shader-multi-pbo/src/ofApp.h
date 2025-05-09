#pragma once

#include "ofMain.h"
#include "ofxMultiXbo.hpp"

#define USE_FLOATS 1
#define NUM_BUFFERS 5 // The shader will need to be adjusted if this exceeds 5

class ofApp : public ofBaseApp
{

  public:
    void setup();
    void update();
    void draw();

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);

    ofxMultiFboShader renderer;
    ofxMultiPbo pbo;

    // Display images - one for each buffer
    ofImage displayImages[5];
};
