#ifndef _WF_IMAGE_H_
#define _WF_IMAGE_H_

#include "ocr.hh"

namespace ocrparser {

enum ImageOrientation{
    ImageOrientationUndefined,
    ImageOrientationRight,
    ImageOrientationUp,
    ImageOrientationDown,
    ImageOrientationLeft

    };


class Image {
public:

Image(const Image& sourceImage, const  Rect& cropRect, ImageOrientation orient = ImageOrientationUp);
~Image();

/**
bytesPerPixel is 1(grayscale), 3(rgb) or 4 (argb).
 */
Image(int width, int height, int bytesPerPixel = 3, uint8_t* imageData = NULL, bool copyBuffer = true);

int getPixel(int x, int y) const {
    int pixel = 0;
    for(int i = 0; i < m_bytesPerPixel; ++i) {
        pixel |= m_dataPtr[m_bytesPerPixel*(m_width*y + x) + i] << (i*8);
    }
    return pixel;
}

int getBytesPerPixel() const {return m_bytesPerPixel;}

int getWidth() const {return m_width;}

int getHeight() const {return m_height;}

ImageOrientation getOrientation() const {return m_orientation;}

const uint8_t* getDataPtr() const {return m_dataPtr;}
uint8_t* getDataPtr() {return m_dataPtr;}

ImageOrientation getImageOrientation() const {return m_orientation;}

private:
// Disable Copy ctor and assignment.
Image(const Image&);
const Image& operator=(Image&);

void setPixel(int x, int y, int pixel) {

    for(int i = 0; i < m_bytesPerPixel; ++i) {
        m_dataPtr[m_bytesPerPixel*(m_width*y + x) + i] = pixel&0xFF;
        pixel = pixel >> 8;
    }
}

    int m_bytesPerPixel;

    int m_width;
    int m_height;

    uint8_t* m_dataPtr;
    bool m_deleteBuffer;

    ImageOrientation m_orientation;
};


class ImageManipulator {
    public:


  static  SmartPtr<Image> getGrayscaledImage(SmartPtr<Image> image);

};

} // namespace ocrparser

#endif

