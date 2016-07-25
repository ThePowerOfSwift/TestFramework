#ifndef _OCR_CLASS_H__
#define _OCR_CLASS_H__
//
//  OCRClass.hh
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//
// Represents a class that contains all data relevant to the scanning of a page of text
//

#include "common.hh"
#include "OCRPage.hh"
#import "WFScanDelegate.h"

namespace ocrparser {

    typedef struct _matchedProduct {
        int newLineNumber;
        int existingLineNumber;
        int newIndex;
        int existingIndex;
        wstring newText;
        wstring existingText;
        wstring newProductNumber;
        wstring existingProductNumber;
        wstring newDescriptionText;
        wstring existingDescriptionText;
        wstring newPriceText;
        wstring existingPriceText;
        OCRElementType tp;
    } matchedProduct;
    
    typedef struct _deltaLines {
        int value;
        int count;
        vector<matchedProduct> matchingList;
    } deltaLinesValue;

class OCRResults {
public:

    LanguageCode languageCode;  // Always set to LanguageENG for now
    CountryCode defaultCountryIndex;    // Always set to countryCodeUS for now
    
    // Variables pointing to the binary image associated with the OCR results we are parsing
    uint8_t *binaryImageData;
    size_t binaryImageWidth;
    size_t binaryImageHeight;
    
    bool imageTests;    // Indicates whether to perform our series of visual tests based on the images. Currently set to true if binaryImageData is not null
    
    bool letterAndDigitsOnBaseline; // Are all letters and digits resting on the baseline? Appears to be true of all receipts, even say 'f' doesn't dip below
    OCRStats globalStats;   // Keeps track of metrics across entire frame
    float slant;            // Factor by which to multiple a X delta to get the Y delta, e.g. if lines go up (right side higher) slant is negative (since higher means lower Y value)
    bool meaningfulFrame;   // Is that a frame with interesting results (e.g. new product numbers we didn't have yet)? Used to determine whether to save the image)
    bool newInfoAdded;      // Did the last frame contribute any new info, i.e. caused any change to existing results?
    int frameNumber;
    bool lastFrame;         // e.g. when we get the subtotal for Target receipts
    int numProducts;        // Number of product numbers found in the frame
    int numProductsWithoutProductNumbers;   // The equivalent of numProducts when retailer has no product numbers
    int numProductsWithQuantities; // Even if quanitity wwas found to be 1
    bool gotSubtotal;       // True if we found a subtotal in this frame
    bool subtotalMatchesSumOfPricesMinusDiscounts;
    bool gotTotal;          // True if we found a total in this frame
    bool gotDate;           // True if we found a date in this frame
    bool gotPhone;           // True if we found a phone in this frame
    bool gotMergeablePattern1; // True if we found OCRElementTypeMergeablePattern1 in this frame
    
    CGRect imageRange;      // Range for entire image
    CGRect OCRRange;        // Range where OCR found characters
    CGRect productPricesRange;  // Range where product prices were found
    CGRect productsWithPriceRange;  // Range where products were found that had a price on the same line
    int numProductsWithPrice;   // Used for possibly upgrading a frame where products were missing their price (for the saved frames for receipt images - see below)
    CGRect productsRange;   // Range where product numbers were found
    CGRect typedResultsRange;   // Range where elements with type were found
    CGRect realResultsRange;    // Range where non-noise elements were found
    float productWidth;         // Width of a product number (in pixels). Used to evaluate if the width of realResultsRange makes sense based on what we expect for each retailer
    float productDigitWidth;    // Width of a digit in a product (including spacing). Used to calibrate spacing while taking into account a possible angle of receipt versus face of phone - which would impact the productDigitWidth value
    
    // Variables used to decide which frames to save for the receipt images
    int receiptImagesIndex;  // Index of last saved image for the receipt images (first image has index 0)
    int lastConfirmedReceiptImageHighestProductLineNumber;       // Highest line number for valid product found in last confirmed receipt image
    int currentSavedReceiptImageHighestProductLineNumber;    // Highest line number for valid product found by current best  candidate for next image
    int currentLowestMatchingProductLineNumber; // While merging new frame into existing, lowest existing line number for matching product
    int currentHighestProductLineNumber;        // Highest line number for valid product found by current frame (not declared our best candidate for next image yet)
    int currentNumProductsWithPrice;    // Number of products with prices in the current frame candidates
    static void computePageAverages(SmartPtr<OCRPage> &currentPage, OCRStats &stats, CGRect validRange);
    
    unsigned long frameStatus;  // Contains status flags indicating if frames have products, are close to the phone etc
    
    retailer_t retailerParams;
    MatchPtr retailerInfo;      // Tracks which parts of retailerParams are known
    vector<OCRElementType> retailerLineComponents;

    OCRResults();   // Default constructor
    void buildAllWords ();  // Arranges an OCRPage into matchesAddressBook elements
    void checkForSubtotalTaggedAsProduct();
    void checkForCoupons();
    static int extractQuantity(MatchPtr d, wstring &quantityText, OCRResults * results);
    void checkForQuantities();
    void checkForTotal();
    void checkForTaxBal();
    
    static CGRect matchRange(MatchPtr m);   // Box encompassing a match
    static float matchSlant(MatchPtr m);
    static int getIntIfExists(MatchPtr m, std::string type, int defaultValue);    // Returns a given int value if exists or else the specified default value
    static float averageHeightTallLetters(MatchPtr m);
    static bool yOverlapFacingSides(MatchPtr m1, MatchPtr m2, float &overlap, float &percent, bool useSlant);
    static float heightTallestChar(MatchPtr m);
#define ALIGNED_LEFT    0
#define ALIGNED_CENTER  1    
    static bool matchesAligned(MatchPtr m1, MatchPtr m2, int alignment);
    static float averageLetterWidthInMatch(MatchPtr m);
    static bool averageDigitHeightWidthSpacingInMatch(MatchPtr m, float& width, float& height, float& spacing, bool &uniform, int &maxSequence, float &maxSequenceAverageHeight, int &maxPriceSequence, bool &uniformPriceSequence, float &maxPriceSequenceAverageHeight, float &productWidth, retailer_s *retailerParams);
    static bool averageUppercaseHeightWidthSpacingInMatch(MatchPtr m, float& width, float& height, float& spacing, bool &uniform, int &maxSequence, float &maxSequenceAverageHeight);
    
    MatchPtr findItemJustAfterMatchAtIndexInMatchesExcluding(int i, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool assumeBlocksSet, OCRResults *results);
    MatchPtr findItemJustAfterMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool assumeBlocksSet, OCRResults *results = NULL);
    static MatchPtr findItemJustAboveMatchAtIndexInMatchesExcluding(int i, MatchVector& matches, MatchVectorPtr ex);
    MatchPtr findItemJustAboveMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex);
    MatchPtr findItemJustBelowMatchAtIndexInMatchesFindBestExcluding(int i, MatchVector& matches,bool findBest, MatchVectorPtr ex, bool lax = false);
    MatchPtr findItemJustBelowMatchInMatchesFindBestExcluding(MatchPtr& match, MatchVector& matches,bool findBest, MatchVectorPtr ex, bool lax = false);
    static MatchPtr findItemJustBeforeMatchAtIndexInMatchesExcluding(int i, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool blocksSet);
    static MatchPtr findItemJustBeforeMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool blocksSet);
    static int indexOfMatchInMatches(MatchPtr m, MatchVector& matches);
    static MatchPtr findStartOfTopmostLine(MatchVector& matches, int acceptableNumberOfSpaces);
    
     void setBlockandLineandColumnandPositioninMatch(int block, int line, int col, int pos, MatchPtr m);    // Short helper function setting values into a match dictionary
    static void swapLines(int firstLineIndex, MatchVector &matches);
    void setBlocks(bool adjustLinesOnly = false);       // Sets block, line and column numbers
    MatchPtr addNewFragmentToWithTextAndTypeFromIndexToIndexAtIndex(MatchVector& targetMatches, const wstring& newText, OCRElementType tp, int start, int end, int index);
    MatchPtr addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex(MatchVector& targetMatches, wstring newText, OCRElementType tp, RectVectorPtr rects, int start, int end, float conf , int index);
    static MatchPtr addSubFragmentWithTextAndTypeAndRectsFromIndexToIndexWithConfidenceBlockLineColumnAtIndex(MatchVector& targetMatches, wstring newText, OCRElementType tp, RectVectorPtr rects, int start, int end, float conf , int block, int line, int column, int index);
    static MatchPtr copyMatch(MatchPtr d);
    static string stringForOCRElementType(OCRElementType tp);
    static int elementWithTypeInlineAtIndex(int matchIndex, OCRElementType tp, MatchVector *matches, retailer_t *retailerParams, bool testOrder = false);
    static vector<int> elementsListWithTypeInlineAtIndex(int matchIndex, OCRElementType tp, MatchVector *matches);
    static int indexOfElementPastLineOfMatchAtIndex(int matchIndex, MatchVector *matches);
    static int indexFirstElementOnLineAtIndex(int lineStartIndex, MatchVector *matches);
    static int indexLastElementOnLineAtIndex(int lineStartIndex, MatchVector *matches);
    static int indexFirstElementForLine(int lineNumber, MatchVector *matches);
    static int indexJustBeforeLine(int lineNumber, MatchVector *matches);
    static void setLineNumberStartingAtIndex(int i, int lineNumber, MatchVector *matchesAddressBook);
    static void setNewLineNumberStartingAtIndex(int i, int lineNumber, MatchVector *matchesAddressBook);
    static bool getLineInfo(int lineStartIndex, int lineEndIndex, CGRect *firstRect, CGRect *lastRect, float *height, MatchVector *matchesAddressBook);
    static bool getLineInfoIgnoringLetters(int lineStartIndex, int lineEndIndex, CGRect *firstRect, CGRect *lastRect, float *height, MatchVector *matchesAddressBook);
    static void checkForSpecificMatch(MatchPtr m, MatchVector& matches, int index = -1);
    static void checkForSpecificMatches(MatchVector& matches);
    static void checkLines(MatchVector& matches);
    
    static bool isMeaningfulLineAtIndex(int lineIndex, OCRResults *results);
    static bool isWithinPricesColumn(int matchIndex, MatchVector &matches, OCRResults *results);
    static bool isWithinProductNumbersColumn(int matchIndex, MatchVector matches, OCRResults *results);
    static void sortText123(MatchPtr m);
    bool checkPriceQuantityLinkage (int index, MatchVector matches, wstring newText, retailer_t *retailerParams);
    bool deltaLineNumbersNewResultsToExistingResults(MatchVector &matches, int &deltaLines, retailer_t *retailerParams);
#if DEBUG
    static wstring textForLineAtIndex(int lineIndex, MatchVector &matches);
#endif
    bool importNewResults (OCRResults *newResults);
    static int highestProductLineNumber (MatchVector matches);
    static bool textAndRectsInRange(MatchPtr m, CGRect range, int *entirelyContainedOrNoOverlap, bool xRange, bool yRange, vector<SmartPtr<CGRect> > *rectsPtr, wstring *textPtr);
    static bool textAndRectsOutsideRange(MatchPtr m, CGRect range, int *entirelyContained, bool xRange, bool yRange, vector<SmartPtr<CGRect> > *rectsBeforePtr, wstring *textBeforePtr, vector<SmartPtr<CGRect> > *rectsAfterPtr, wstring *textAfterPtr);
    static bool checkPartialPrice (MatchPtr m, OCRResults *results);
    static void checkForProductsOnDiscountLines(MatchVector& matches);
    static int productIndexForPriceAtIndex(int priceIndex, MatchVector &matches, retailer_t *retailerParams, int *productLineNumberPtr = NULL);
    static int priceOrPartialPriceIndexForProductAtIndex(int productIndex, OCRElementType tp, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams);
    static int productPriceIndexForDescriptionAtIndex(int descriptionIndex, OCRElementType tp, MatchVector &matches, retailer_t *retailerParams);
    static int priceIndexForProductAtIndex(int productIndex, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams);
    static int partialPriceIndexForProductAtIndex(int productIndex, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams);
    static int descriptionIndexForProductAtIndex(int productIndex, MatchVector &matches, int *descriptionLineNumberPtr, retailer_t *retailerParams);
    static int descriptionIndexForProductPriceAtIndex(int productPriceIndex, MatchVector &matches, retailer_t *retailerParams);
    static int productIndexForDescriptionAtIndex(int descriptionIndex, MatchVector &matches, retailer_t *retailerParams);
    static int productIndexForQuantityAtIndex(int quantityIndex, MatchVector &matches, retailer_t *retailerParams);
    static float overlapAmountRectsXAxisAnd(CGRect r1, CGRect r2);
    static void adjustAllLinesToHighStartIfNeeded(MatchVector &matches);
    void buildLines();
    static int globalLineForLineAtIndex(int lineIndex, MatchVector &matches);
    bool mergeNewItemWithExistingItem(int indexNewItem, MatchVector &newMatches, int indexExistingItem, MatchVector &existingMatches);
    static bool mergeM1WithM2(MatchPtr existingMatch, MatchPtr newMatch);

    // Data structure holding all OCR characters with their coordinates. Each page contains multiple lines.
	SmartPtr<OCRPage> page;
	MatchVector matchesAddressBook;	// Latest set of 'matches', which are OCR lines along with type information
    static bool overlappingRectsYAxisAnd(CGRect r1, CGRect r2);
    static bool overlappingRectsXAxisAnd(CGRect r1, CGRect r2);
    static bool gapBetweenLines (int indexLineTop, int indexLineBottom, float &gap, MatchVector &matches);
    static bool closeMatchPriceToPartialPrice(float price, wstring partialPrice);
    static void countCharsAroundItemAtIndex(int index, MatchVector &matches, int &charsBefore, int &charsAfter);
    static bool typeRelevantToUser(OCRElementType tp, retailer_t *retailerParams);

private:
    static float overlapAmountRectsYAxisAnd(CGRect r1, CGRect r2, float minOverlap = 0.20);
    static float overlapPercentRectsYAxisAnd(CGRect r1, CGRect r2);
    static CGRect rangeWithNRects(int n, RectVector::iterator rects);
    bool winnerExistingVersusNewMatch (MatchPtr existingM, MatchPtr newM, bool *newWins, bool *replace);
    static bool priceRectsXOverlap (MatchPtr potentialPriceItem, CGRect productPriceRect, CGRect potentialProductPriceRect, OCRResults *results);
};

}
#endif
