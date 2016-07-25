#ifndef _RECEIPTPARSER_H_
#define _RECEIPTPARSER_H_

//
//  receiptParser.hh
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

#import "ocr.hh"
#import "WFScanDelegate.h"

// Defining my own version of CGFloat because this is defined only in the iOS OBJ-C headers and this is C++ code which I can't include there. Note: option to limit the type to 'float' even if 64-bit platform because that's sufficient for image coordinates.
struct CGPointFloat {
  float x;
  float y;
};
typedef struct CGPointFloat CGPointFloat;


typedef struct {
    wchar_t value;  // Recognized character
	CGPointFloat ul;     // Upper left coordinate
    CGPointFloat ur;     // Upper right coordinate
    CGPointFloat ll;     // Lower left coordinate
    CGPointFloat lr;     // Lower right coordinate
    int quality;    // Estimated OCR recognition quality
} MicroBlinkOCRChar;

// To debug the Android - C++ interface
struct simpleM {
    wstring text;
    OCRElementType type;
    int n;
};
typedef struct simpleM simpleMatch;

// Structure used by mobile app to pass a list of server prices to the C++ parsing code
struct serverPriceStruct {
    wstring sku;
    float price;
};
typedef struct serverPriceStruct serverPrice;

// Returned by the C++ parser to the app for each product
struct productStruct {
    wstring description;
    wstring description2;
    wstring description3;
    int line;
    wstring sku;
    wstring sku2;
    wstring sku3;
    float price;
    float price2;
    float price3;
    wstring partialPrice;
    int quantity;
    float totalPrice;
    bool ignored;           // Rarely set, set only when it's a product we know shouldn't be displaying to the user but want it in the results set to add up to the subtotal
    bool not_eligigle;      // Means product is not eligible for price matching. For now this is only for Macy's product with coupons
};
typedef struct productStruct scannedProduct;

struct itemStruct {
    wstring itemText;   // Text for the item itself, e.g. "(212) 123-4567" if it's a phone
    wstring itemText2;
    wstring itemText3;
    wstring description;    // Additonal related text, e.g. "UNION NORTH next to a phone in "UNION NORTH - (212) 123-4567"
    wstring description2;
    wstring description3;
    int line;
    unsigned long flags;
};
typedef struct itemStruct scannedItem;

typedef struct _matchingItem {
    int lineNumber;
    int index;
    int index1;
    wstring text;
    OCRElementType tp;
    float floatValue;  // Meaning depends on usage
    ocrparser::CGRect r;  // Meaning depends on usage
} matchingItem;

typedef struct _matchingList {
    float average;
    int count;
    vector<matchingItem> list;
} matchingList;

namespace ocrparser {

void resetParser(); // Resets all state, called before processing a new receipt
// Processes a set of OCR results from MicroBlink and returns a set of matches
MatchVector processBlinkOCRResults(MicroBlinkOCRChar *results, uint8_t *binaryImageData, size_t binaryImageWidth, size_t binaryImageHeight, int retailerId, retailer_s *retailerParamsPtr, bool newSequence, bool *meaningfulFramePtr, bool *oldDataPtr, bool *done, int *receiptImgIndexPtr, unsigned long *frameStatusPtr);
// Just returned current combined OCRResults
MatchVector getCurrentOCRResults();
vector<simpleMatch> exportCurrentOCRResultsVector();
void exportCurrentScanResults (vector<scannedProduct> *productsVectorPtr, vector<float> *discountsVectorPtr, float *subTotalPtr, float *totalPtr, wstring *datePtr, vector<scannedItem> *phonesVectorPtr, vector<serverPrice> serverPrices);
simpleMatch *exportCurrentOCRResultsArray(int *numItems);
retailer_t getCurrentRetailersParams();
void mySplit(wstring text, wchar_t ch, vector<wstring> &elements);
vector<int> retailersForBarcode(wstring barcode);
}

#endif // _RECEIPTPARSER_H_
