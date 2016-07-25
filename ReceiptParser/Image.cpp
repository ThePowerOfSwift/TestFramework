#include "common.hh"
#include "Image.h"

#include "string.h"

namespace ocrparser {

Image::Image(const Image& sourceImage, const  Rect& cropRect, ImageOrientation orient /*= ImageOrientationUp*/) : m_orientation(orient) {

 m_width  = cropRect.size.width;
 m_height = cropRect.size.height;
 m_bytesPerPixel = sourceImage.m_bytesPerPixel;
 m_dataPtr = new uint8_t[m_bytesPerPixel*m_width*m_height];

 for (int x = 0; x < m_width; ++x) {
 for (int y = 0; y < m_height; ++y) {
     setPixel(x,y, sourceImage.getPixel(x+cropRect.origin.x, y + cropRect.origin.y));
 }
 }
}
// should we delete the buffer or not!?
Image::Image(int width, int height, int bpp, uint8_t* imageData, bool copyBuffer) : m_orientation(ImageOrientationUp) {

 m_width  = width;
 m_height = height;
 m_bytesPerPixel = bpp;
 m_dataPtr =  copyBuffer ? new uint8_t[m_bytesPerPixel*m_width*m_height] : imageData;
 m_deleteBuffer = copyBuffer;

 if(copyBuffer && (imageData != NULL)) {
    memcpy(m_dataPtr, imageData, width*height*m_bytesPerPixel);
 }

}

Image::~Image() {
  if (m_deleteBuffer)
    delete []m_dataPtr;
}

/* Not used for now
SmartPtr<Image> ImageManipulator::getGrayscaledImage(SmartPtr<Image> image) {
    if (image->getBytesPerPixel() == 1) {
        return image;
    }

    SmartPtr<Image> result(new Image(image->getWidth(), image->getHeight(),1));

    uint8_t* input = image->getDataPtr();
    uint8_t* output = result->getDataPtr();

    int rgbCorrection = image->getBytesPerPixel() == 3 ? 1 : 0;
    
    int length = image->getWidth()*image->getHeight();
    int pixelSize = 4 - rgbCorrection;
    int rOffset =  1 - rgbCorrection;
    int gOffset =  2 - rgbCorrection;
    int bOffset =  3 - rgbCorrection;

    for (int i = 0; i < length; ++i) {

        float r = input[(i*pixelSize)+rOffset];
        float g = input[(i*pixelSize)+gOffset];
        float b = input[(i*pixelSize)+bOffset];
        uint8_t grayLevel = (uint8_t)(g*0.59 + r*0.30 + b*0.11);
        output[i] = grayLevel;
    }
    
    return result;

}
*/
}
