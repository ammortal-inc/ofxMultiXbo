//
//  ofxMultiFbo.cpp
//
//  Created by Ben Snell on 12/2/20.
//

#include "ofxMultiFbo.hpp"

// -------------------------------------------------
ofxMultiFbo::ofxMultiFbo()
{
}

// -------------------------------------------------
ofxMultiFbo::~ofxMultiFbo()
{
}

// -------------------------------------------------
bool ofxMultiFbo::allocate(int __width, int __height, vector<GLenum> _glFormats,
                           vector<ofColor> _initColors, bool _bPboSupport)
{
    if (_glFormats.size() >= 8 || _initColors.size() >= 8)
    {
        ofLogError("ofxMultiFbo") << "Cannot create a ofxMultiFbo with this many buffers";
        return false;
    }
    if (_initColors.size() < _glFormats.size())
    {
        ofLogNotice("ofxMultiFbo") << "Resizing the color buffers";
        _initColors.resize(_glFormats.size(), _initColors.back());
    }
    if (_glFormats.size() < _initColors.size())
    {
        _glFormats.resize(_initColors.size(), _glFormats.back());
    }

    glFormats = _glFormats;
    _width = __width;
    _height = __height;
    bPboSupport = _bPboSupport;

    // Create this many textures
    ofFboSettings settings;
    settings.width = width();
    settings.height = height();

    // Allocate A
    A.allocate(settings);
    for (int i = 0; i < glFormats.size(); i++)
    {
        A.createAndAttachTexture(glFormats[i], i);
    }
    // Init A
    for (int i = 0; i < _initColors.size(); i++)
    {
        A.begin();
        A.setActiveDrawBuffer(i);
        ofClear(_initColors[i]);
        A.end();
    }

    // Allocate B
    B.allocate(settings);
    for (int i = 0; i < glFormats.size(); i++)
    {
        B.createAndAttachTexture(glFormats[i], i);
    }
    // Init B
    for (int i = 0; i < _initColors.size(); i++)
    {
        B.begin();
        B.setActiveDrawBuffer(i);
        ofClear(_initColors[i]);
        B.end();
    }

    if (bPboSupport)
    {
        // Also allocate a vector of textures for the pbo to copy (without locking onto thr fbo renderer)
        for (int i = 0; i < glFormats.size(); i++)
        {
            int glFormat = glFormats[i];
            int glType = ofGetGLTypeFromInternal(glFormat);
            int numChannels = ofGetNumChannelsFromGLFormat(glFormat);
            int numBytesPerChannel = ofGetBytesPerChannelFromGLType(glType);
            int numBytes = numChannels * numBytesPerChannel;

            ofBufferObject *buf = new ofBufferObject();
            buf->allocate(width() * height() * numBytes, GL_DYNAMIC_COPY);
            pboBuffers.push_back(buf);

            ofTexture *tex = new ofTexture();
            tex->allocate(A.getTexture(i).getWidth(), A.getTexture(i).getHeight(), glFormat);
            pboTextures.push_back(tex);
        }
    }

    data = &A;
    utility = &B;
    updatePboTextures();

    bAllocated = true;

    return true;
}

// -------------------------------------------------
bool ofxMultiFbo::allocate(int __width, int __height, GLenum _glFormat, int _numBuffers, ofColor _initColor, bool _bPboSupport)
{

    vector<GLenum> _glFormats;
    _glFormats.resize(_numBuffers, _glFormat);
    vector<ofColor> _initColors;
    _initColors.resize(_numBuffers, _initColor);
    return allocate(__width, __height, _glFormats, _initColors, _bPboSupport);
}

// -------------------------------------------------
void ofxMultiFbo::updatePboTextures()
{
    if (!bPboSupport)
        return;

    for (int i = 0; i < pboTextures.size(); i++)
    {

        // copy the data
        getTex(i).copyTo(*pboBuffers[i]);

        // load into new texture
        pboTextures[i]->loadData(*pboBuffers[i], glFormats[i], ofGetGLTypeFromInternal(glFormats[i]));
    }
}

// -------------------------------------------------
bool ofxMultiFbo::begin()
{

    if (activeUtilityBuffers.size() > 0)
    {
        ofLogWarning("ofxMultiFbo") << "Fbo already began drawing. Cannot call begin() again.";
        return false;
    }

    // Add all the indices of buffers to which we will bind
    for (int i = 0; i < getNumBuffers(); i++)
        activeUtilityBuffers.insert(i);

    // Begin the fbo and bind to all buffers
    utility->begin();
    utility->activateAllDrawBuffers();

    return true;
}

// -------------------------------------------------
bool ofxMultiFbo::begin(int index)
{
    if (!validBufferIndex(index))
    {
        ofLogError("ofxMultiFbo") << "Invalid buffer object index: " << index;
        return false;
    }

    // Check if we are already drawing to this buffer
    if (isUtilityBufferActive(index))
    {
        ofLogWarning("ofxMultiFbo") << "Fbo buffer at index " << index << " is already active.";
        return false;
    }

    // If we haven't already begun the fbo, begin it now
    if (!isUtilityBufferActive())
        utility->begin();

    // Set a new active draw buffer
    activeUtilityBuffers.insert(index);
    utility->setActiveDrawBuffer(index);

    return true;
}

// -------------------------------------------------
bool ofxMultiFbo::isUtilityBufferActive(int index)
{
    return activeUtilityBuffers.find(index) != activeUtilityBuffers.end();
}

// -------------------------------------------------
bool ofxMultiFbo::isUtilityBufferActive()
{
    return activeUtilityBuffers.size() > 0;
}

// -------------------------------------------------
ofTexture &ofxMultiFbo::getTex(int index)
{

    return data->getTexture(index);
}

// -------------------------------------------------
bool ofxMultiFbo::end(bool swapBuffers)
{

    // Only end if we're drawing to something (or always end ?)
    // if (isUtilityBufferActive())
    {
        utility->end();
        activeUtilityBuffers.clear();
    }

    // Swap if instructed to
    if (swapBuffers)
        swap();
    return true;
}

// -------------------------------------------------
bool ofxMultiFbo::end(int index)
{
    // Validate this index
    if (!validBufferIndex(index))
    {
        ofLogError("ofxMultiFbo") << "Invalid buffer object index: " << index;
        return false;
    }

    // If this index is being drawn to, inactivate its buffer
    if (isUtilityBufferActive(index))
    {
        // Remove its index
        activeUtilityBuffers.erase(index);

        // Get a list of all active indices
        vector<int> tmp(activeUtilityBuffers.size());
        copy(activeUtilityBuffers.begin(), activeUtilityBuffers.end(), tmp.begin());
        utility->setActiveDrawBuffers(tmp);
    }

    // If no buffers are active, end binding
    if (!isUtilityBufferActive())
        utility->end();

    // Use must swap manually

    return true;
}

// -------------------------------------------------
void ofxMultiFbo::swap()
{
    std::swap(data, utility);
    updatePboTextures();
}

// -------------------------------------------------
bool ofxMultiFbo::draw(float x, float y, float w, float h, int index)
{

    // Validate this index
    if (!validBufferIndex(index))
    {
        ofLogError("ofxMultiFbo") << "Invalid buffer object index: " << index;
        return false;
    }

    // Draw the default (data) texture
    getTex(index).draw(x, y, w, h);

    return true;
}

// -------------------------------------------------
bool ofxMultiFbo::draw(float x, float y, int index)
{

    return draw(x, y, width(), height(), index);
}

// -------------------------------------------------

// -------------------------------------------------

// -------------------------------------------------

// -------------------------------------------------

// -------------------------------------------------

// -------------------------------------------------
