//
//  WFScanDelegate.h
//  Windfall
//
//  Created by Patrick Questembert on 3/18/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

/**
 *  This class is really a parsing delegate. Each controller that has a need to parse creates its own instance of this class.
 */

#if !CODE_CPP
#import <Foundation/Foundation.h>
#import <UIKit/UIImage.h>
#endif

#define MAX_TAX 0.20    // Highest possible tax amount (used to disqualify line items from being a tax value)

#define RETAILER_UNKNOWN_CODE   0
#define RETAILER_TARGET_CODE    21
#define RETAILER_WALMART_CODE   25
#define RETAILER_TOYSRUS_CODE   23
#define RETAILER_BABIESRUS_CODE 27
#define RETAILER_STAPLES_CODE   20
#define RETAILER_BESTBUY_CODE   3
#define RETAILER_OFFICEDEPOT_CODE   31
#define RETAILER_HOMEDEPOT_CODE 22
#define RETAILER_LOWES_CODE     28
#define RETAILER_MACYS_CODE     12
#define RETAILER_BBBY_CODE      2
#define RETAILER_SAMSCLUB_CODE  99
#define RETAILER_COSTCO_CODE    29
#define RETAILER_BJSWHOLESALE_CODE 100
#define RETAILER_KOHLS_CODE     11
#define RETAILER_KMART_CODE     10
#define RETAILER_WALGREENS_CODE 24
#define RETAILER_DUANEREADE_CODE 101
#define RETAILER_ALBERTSONS_CODE 102
#define RETAILER_CVS_CODE       103
#define RETAILER_TRADERJOES_CODE    104
#define RETAILER_PETSMART_CODE 105
#define RETAILER_PETCO_CODE    106
#define RETAILER_FAMILYDOLLAR_CODE    107
#define RETAILER_FREDMEYER_CODE 108
#define RETAILER_MARSHALLS_CODE 109
#define RETAILER_WINCO_CODE     110

#define WFSanityCheckFactor     3      // Reject scanned price smaller or larger than server price by that factor

// Flags
#define RETAILER_FLAG_ALIGNED_LEFT          0x1
#define RETAILER_FLAG_ALIGNED_RIGHT         0x2

#define MAX_QUANTITY    99  // Highest quantity we ever expect

#define DATE_REGEX_GENERIC_CODE 1000
/*  06/03/2016 20:33:48 (Trader Joe's)
    06/03/2016 03:51 PM (Target)
    06/03/16   20:33 (Sam's club, Staples)
    06-03-2016 08:33PM (Trader Joe's bottom or receipt)
    DATE-02/08/12 TIME-04:22PM (Toysrus or Babiesrus)
    02-27-16 7:02P (Khol's)
    10-07-15 (Lowe's top of receipt) 
    10/07/15 12:09:10 (Lowe's below subtotal)
*/
#define DATE_REGEX_GENERIC  "(?:(?:^| )(?:((?:.){4}\\-)?(%digit{2})(\\/|\\-)(%digit{2})(\\/|\\-)(%digit{2}|%digit{4})(?:(?: )+((?:.){4}\\-)?(?:(%digit{1,2})(?:.)(%digit{2})(?:(?:.)(%digit{2}))?|(%digit{1,2})(?:.)(%digit{2})(?: )?((?:A|P)(?:[^\\s]{1,3})?)))?)(?: |$))"
#define DATE_REGEX_GENERIC_MIN_LEN 8

// 05/06/2015 09:15 AM (Target)
// 2016-03-23 why does the regex below seemingly accept any 4-8 chars for the date?! I assume it's because of cases with extremely poor OCR?
//#define DATE_REGEX_YYYY_AMPM_NOSECONDS  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/([^\\s]{4,8})(?:(?: )*(%digit{2})(?:.)(%digit{2})(?: )((?:A|P)[^\\s]{1,3}))?(?: |$))"
#define DATE_REGEX_YYYY_AMPM_NOSECONDS  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{4})(?:(?: )*(%digit{2})(?:.)(%digit{2})(?: )((?:A|P)[^\\s]{1,3}))?(?: |$))"
#define DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE 1
#define DATE_REGEX_YYYY_AMPM_NOSECONDS_MIN_LEN 8

// 05/06/15 09:15 (Bestbuy) or 05/06/15 09:15:33 (Walmart and Winco)
// Allows for any character in lieu of expected ':' in the time + makes the entire time part optional (in case OCR messes it up)
// We ignore seconds, leaving that part as optional just in case *some* stores include it - doesn't hurt
#define DATE_REGEX_YY_HHMM  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{2})(?:(?: )+(%digit{2})(?:.)(%digit{2})(?:(?:.)%digit{2})?)?(?: |$))"
#define DATE_REGEX_YY_HHMM_CODE 2
#define DATE_REGEX_YY_HHMM_MIN_LEN 8

// Office Depot (has time but all the way to the right, with some weird text in the middle e.g. "08/03/2015 15.2.5   12:01 PM"
#define DATE_REGEX_YYYY  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{4})(?: |$))"
#define DATE_REGEX_YYYY_CODE 4
#define DATE_REGEX_YYYY_MIN_LEN 8

// Home Depot (has time but on a different line below, ignoring) e.g. "08/03/15". 09/09/2015: not true for one receipt submitted by Darren - expanding regex to add optional time
#define DATE_REGEX_YY_HHMM_AMPM_OPTIONAL  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{2})(?:(?: )*(%digit{2})(?:.)(%digit{2})(?: )((?:A|P)[^\\s]{1,3}))?(?: |$))"
#define DATE_REGEX_YY_HHMM_AMPM_OPTIONAL_CODE 5
#define DATE_REGEX_YY_HHMM_AMPM_OPTIONAL_MIN_LEN 8

// Lowes (with dashes, not slashes) e.g 03-18-15 (top of receipt) or 03/08/15 (at the bottom of receipt)"
// Sam's Club
// For Lowes time only appears after the bottom version of the date (with slashes). Allowing for both dates (top/bottom) since it's optional.
// Seconds optional (and ignored so not in a capturing group)
#define DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS "(?:(?:^| )(%digit{2})(?:\\-|\\/)(%digit{2})(?:\\-|\\/)(%digit{2})(?:(?: )+(%digit{2})(?:.)(%digit{2})(?:(?:.)%digit{2})?)?(?: |$))"
#define DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE 6
#define DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_MIN_LEN 8

// 3:22 PM 4/07/2015 (Macy's)
// NOTE: month may be just 1 digit!
// Allows for any character in lieu of expected ':' in the time + makes the entire time part optional (in case OCR messes it up)
// We ignore seconds, leaving that part as optional just in case *some* stores include it - doesn't hurt
#define DATE_REGEX_AMPM_NOSECONDS_HHMM_YY  "(?:(?:^| )(?:(?: )*(%digit{2})(?:.)(%digit{2})(?: )((?:A|P)[^\\s]{1,3})(?: )*)?(%digit{1,2})\\/(%digit{2})\\/(%digit{4})(?: |$))"
#define DATE_REGEX_AMPM_NOSECONDS_HHMM_YY_CODE 7
#define DATE_REGEX_AMPM_NOSECONDS_HHMM_YY_MIN_LEN 8

// BBBY e.g 11/25/15-1007 (meaning time 10:07am)
#define DATE_REGEX_YY_SLASHES_DASH_HHMM "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{2})(?:(?: )?\\-(?: )?(%digit{2})(%digit{2}))?(?: |$))"
#define DATE_REGEX_YY_SLASHES_DASH_HHMM_CODE 8
#define DATE_REGEX_YY_SLASHES_DASH_HHMM_MIN_LEN 8

// March 27, 2016     4:48 PM (CVS)
// Allows missed comma after month
#define DATE_REGEX_MONTH_DD_COMMA_YYYY  "(?:(?:^| )((?:.){4,9})(?: )(%digit{2})(?:\\,)?(?: )(%digit{4})(?:(?: )+(%digit{2})((?:A|P)[^\\s]{1,3}))?(?: |$))"
#define DATE_REGEX_MONTH_DD_COMMA_YYYY_CODE 9
#define DATE_REGEX_MONTH_DD_COMMA_YYYY_MIN_LEN 11

// 05/06/15 9:15PM (Petco)
#define DATE_REGEX_YY_SLASHES_HMM_AMPM  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{2})(?:(?: )+(%digit{1,2})(?:.)(%digit{2})((?:A|P)[^\\s]{1,3})?)(?: |$))"
#define DATE_REGEX_YY_SLASHES_HMM_AMPM_CODE 10
#define DATE_REGEX_YY_SLASHES_HMM_AMPM_MIN_LEN 10

// 08/25/2011 08:39 PM (Petsmart)
#define DATE_REGEX_YYYY_DASHES_HHMM_AMPM "(?:(?:^| )(%digit{2})(?:\\-)(%digit{2})(?:\\-)(%digit{4})(?:(?: )+(%digit{2})(?:.)(%digit{2})(?:(?:.)%digit{2})?)?(?: |$))"
#define DATE_REGEX_YYYY_DASHES_HHMM_AMPM_CODE 11
#define DATE_REGEX_YYYY_DASHES_HHMM_AMPM_MIN_LEN 8

// 06/03/2016 20:33:48 (Trader Joe's)
#define DATE_REGEX_YYYY_HHMMSS  "(?:(?:^| )(%digit{2})\\/(%digit{2})\\/(%digit{4})(?:(?: )*(%digit{2})(?:.)(%digit{2})(?:.)(%digit{2}))?(?: |$))"
#define DATE_REGEX_YYYY_HHMMSS_CODE 12
#define DATE_REGEX_YYYY_HHMMSS_MIN_LEN 8

typedef struct retailer_s {
    int  code;                  // Retailer code (Target = 1, etc)
    bool pricesHaveDollar;      // Are prices preceded by a $ sign? No for Target, yes for Walmart
    bool noZeroBelowOneDollar;  // Prices below $1 appear as ".XY" (set only for KMART so far)
    bool noProductNumbers;      // True for CVS
    int  productNumberLen;      // 9 for Target, 12 for Walmart
    int  minProductNumberLen;      // Set when there is a range of length, e.g. 9-11 for BBBY
    int  maxProductNumberLen;      // Set when there is a range of length, e.g. 9-11 for BBBY
    int shorterProductNumberLen;   // Babiesrus has 6-digit products
    bool shorterProductsValid;  // No for Babiesrus, yes for Winco
    bool productNumberUPC;      // No for Target, we thought it was true for Walmart but actually no. True for Staples.
    unsigned long productAttributes;        // Various flags like aligned left etc
    const char *productNumber2;     // Additional product format, e.g. 0000-123-456 for Home Depot
    const char *productNumber2GroupActions; // Capturing groups to preserve (1) or delete (0), e.g. "00" for Home Depot (2 groups to delete)
    const char *charsToIgnoreAfterProductRegex; // "H" for some Walmart receipts
    unsigned long productPricesAttributes;  // Various flags like aligned left etc
    bool pricesAfterProduct;    // Yes for Target and Walmart
    bool descriptionBeforePrice;    // Yes for Target, no for Walmart
    bool descriptionBeforeProduct;  // Yes for Walmart, no for Target
    bool descriptionAboveProduct;   // Yes for Toysrus/Staples
    bool descriptionBelowProduct;   // Yes for Best Buy
    bool descriptionAlignedWithProduct; // Yes for Staples + Macy's
    bool descriptionOverlapsWithProduct;    // Set when descriptions and products are not really aligned but largely overlap (e.g. Walgreens)
    bool descriptionAlignedLeft;    // Description starts all the way on the left side (e.g. CVS)
    bool priceAboveProduct;     // Is price on a line above the product number? Yes for Toysrus.
    int productDescriptionMinLen;   // e.g. 3
    int minItemLen;             // e.g. 3
    bool uniformSpacingDigitsAndLetters;    // Seems to hold true of all retailers, even includes some non-alpha like the dash (but we still ignore dashes for that purpose) and spacing around dots is definitely wider (e.g. Target)
    float lineSpacing;          // As percentage of uppercase/digit height (0.28 for Target receipts)
    float maxPrice;     // e.g. 1,000
    const char *stripAtDescEnd;       // Strip these if found at the end of product descriptions ("T,F,FT,FN" for Target)
    bool hasQuantities;         // True for Target, false for Walmart
    bool allProductsHaveQuantities;     // True for CVS. Set only if we want to see the ABSCENCE of a quantity as an indication of a problem. 
    bool quantityAtDescriptionStart;    // True for Staples + CVS
    bool pricePerItemAfterProduct;      // True for Staples
    bool pricePerItemBeforeTotalPrice;  // True for Staples
    int extraCharsAfterPricePerItem;    // Glued to priced per item, 3 for Staples
    int maxCharsOnLineAfterPricePerItem;  // On same line separated by a space, "ea" for Target, i.e. 4 chars (to allow for disconnected chars)
    bool quantityPPItemTotal;   // Total appears on same line as quantity & price per item, e.g. "2@0.98		1.96" (Home Depot)
    bool pricePerItemTaggedAsPrice;     // This is true when the '@' is spaced, as for Kmart "2 @ 1/2.40", yields a price items of "172.40"
    bool hasAdditionalPricePerItemWithSlash;    // As in "2 @ 1.79 or 2/3.00" (Walgreens see https://drive.google.com/open?id=0B4jSQhcYsC9VYnd4SUxYRUNHN3c)
    int maxLinesBetweenProductAndQuantity;    // Set to 1 (e.g. Home Depot) if quantity is right below product line, 2 (e.g. Target) if can be on next line or two lines below
    bool hasDiscounts;          // Are there discounts (negative prices) on receipts?
    const char *discountEndingRegex;  // Regex to recognize the part in discounts which is appended to the price (e.g. for Target a discount looks like "$1.00-" so discountEndingRegex is "\\-"
    const char *discountStartingRegex;  // Regex to recognize the part in discounts which is prepended to the price (e.g. for Staples a discount looks like "-1.00" so discountStartingRegex is "\\-"
    bool hasDiscountsWithoutProductNumbers;  // True for Kmart, see https://drive.google.com/open?id=0B4jSQhcYsC9VdVpqUGJueDdVRW8
    bool hasVoidedProducts;     // When a customer cancels a product, does receipt have both the product + a line cancelling it? Yes for Walmart.
    bool hasProductsWithMarkerAndPriceOnNextLine;   // True for Walmart, e.g. ONIONS is 000000004093KI and price is on next line
    const char *productsWithMarkerAndPriceOnNextLineRegex;  // "KI" for Walmart. Will be placed in the Regex within "()", i.e. OK to set as for example "KI|KR" for Walmart
    bool hasExtraCharsAfterPrices;  // Are there (optional) chars glued to some prices (eg 5.99N for some Staples prices)
    const char *extraCharsAfterPricesRegex; // IMPORTANT: the assumption is that these extra chars (if found) are to be ignored and deleted.
    bool extraCharsAfterPricesMeansProductPrice;   // If extra char is found after a price, it's a product price (e.g. CVS)
    bool has0WithDiagonal;      // If set, don't try to replace 0 with 8 (true for Walmart)
    int dateCode;               // MANDATORY
    bool hasSubtotal;           // True for all retailers for now but set to false if no regex provided
    const char *subtotalRegex;  // e.g. "SUB|Sub|SuB" (may include provisions for OCR mistakes)
    bool hasMultipleSubtotals;  // True only for Walmart so far
    bool hasTaxBalTotal;        // e.g. TAX 1.79 BAL 4.75" (Kmart)
    const char *taxRegex;       // The "TAX" part of the TaxBal combo above, e.g. "TAX"
    const char *balRegex;       // The "BAL" part of the TaxBal combo above, e.g. "BAL"
    bool hasMultipleDates;      // Are there multiple dates, e.g. top and bottom of receipt?
    const char *extraCharsAfterSubtotalRegex;   // If extra chars are expected after the subtotal, e.g. Toyrus "SUBTOTAL IC 7 90.93"
    bool hasTotal;              // True only for retailers where we want to look for the total instead of subtotal. For now only set for Macy's, and because receipts with a single items don't have a subtotal
    const char *totalRegex;     // e.g. "T(?:o|0)tal" (may include provisions for OCR mistakes)
    bool totalIncludesTaxAbove; // Set when the line just above has a tax value which should be deducted from the total amount, at which point we can re-label the total into subtotal (after adjusting the price). Only set for Macy's.
    int minProductNumberEndToPriceStart; // In units of digit width, for Lowes expectation is 38 and acceptable is set to 30 (bad product due to invoice is 25 digits widths away, zip code is even closer). Used to disqualify bogus products.
    int minProductNumberStartToPriceStart; // For some retailers with variable length and prices aligned left, need to use this variable instead
    bool hasDetachedLeadingChar;    // Does this retailer have a single char at the start of lines separated by several spaces from what follows, which we want to preserve? Yes for Staples where that's the quantity number of product lines (set to 1 unless multiple items)
    float letterHeightToRangeWidth; // Typical tall letter height divided by typed range width (0.0385 for Target, 0.043 for Babiesrus)
    float productRangeWidthDividedByLetterHeight;
    float typedRangeToImageWidthTooFar; // Typically we flag TOOFAR when usable receipt range is less than 50% of width, may adjust for some retailers
    float widthToProductWidth;  // Product width divided by receipt width from the left side of products to the right side of prices. 3.86 for Babiesrus
    float leftMarginToProductWidth; // Margin to the left of products, used to make sure we don't accidentally eliminate something there as noise, as a percentage of product width. 0.13 for Babiesrus
    float rightMarginToProductWidth; // Margin to the right of prices, used to make sure we don't accidentally eliminate something there as noise, as a percentage of product width. 0.48 for Babiesrus. Note: use it to capture the case where some types of lines that protude to the right are optional. OK to be liberal, all that matters is that we don't go beyond the receipt right edge
    int productLeftSideToPriceRightSideNumDigits;   // Minimum number of digits that fit between left side of products to right side of prices. For example for BabiesRUs a 12-digit product numbers takes 436 pixels (i.e. 36.33 per digit) and space between left side of product and roght side of price is 1,604 => 44 digits
    int subtotalLineExcessWidthDigits;              // Relevant only together with productLeftSideToPriceRightSideNumDigits, set when subtotal line may be confused with a product but is wider (e.g. 4.4 digits widths for Trader Joe's)
    float spaceWidthAsFunctionOfDigitWidth;  // When set, indicates that any spacing less than this factor times digit width is not a space
    float spaceWidthAroundDecimalDotAsFunctionOfDigitWidth;  // Max observed spacing around the decimal dot in prices
    float spaceBetweenLettersAsFunctionOfDigitWidth;  // Typical spacing between adjacent letters (except around decimal dot)
    float descriptionStartToPriceEnd;    // Distance away on the left of description start to price end (as a function of digit width). Used for retailers like CVS with no product numbers, to weed out irrelevant desc + price
    float leftMarginLeftOfDescription;  // As a function of digit width (anything entirely left of that has to be removed)
    float rightMarginRightOfPrices; // As a function of digit width (anything entirely right of that has to be removed)
    float sanityCheckFactor;    // Reject scanned price smaller or larger than server price by that factor
    bool productNumbersLeftmost; // Are product numbers all the way on the left (i.e. anything left of that is noise)?
    bool productPricesRightmost; // Are product prices all the way on the right (i.e. anything right of that is noise)?
    bool hasSloped6;            // Is the '6' digit with a strong slope top-left? If so we can apply rule 0603 to fix 6 with flat top to 5. Set only for Walmart at this point.
    float minSpaceAroundDotsAsPercentOfWidth;      // As a percentage of digit width, use as a minimum (with 15% margin of error)
    float maxSpaceAroundDotsAsPercentOfWidth;      // As a percentage of digit width, use as a maximum (with 15% margin of error)
    bool allUppercase;          // Set for retailers that have no lowercase letters at all (e.g. CVS)
    bool hasCoupons;            // Are there coupons with the meaning that item doesn't qualify for price matching?
    const char *couponRegex;    // Regex for such coupons
    bool hasPhone;              // Is there a phone number on the receipt? Used only when retailer has no product numbers, to help merge
    const char *phoneRegex;     // Regex for such phone
    int numLinesBetweenProductAndCoupon;    // 2 for Macy's (means one line with stuff in it between product and coupon)
    bool productNumberUPCWhenPadded;        // Do we pad missing digits with '0's (only for UPC check, not passed to server)?
    const char *mergeablePattern1Regex;  // Unique pattern we can use to merge, e.g. "REG#02 TRN#1259 CSHR#1146440 STR#1059" for CVS
    int mergeablePattern1MinLen;
    bool mergeablePatterns1BeforeProducts;  // Set if mergeable pattern 1 can only appear before any products
    float digitWidthToHeightRatio;  // IMPORTANT TO SET: on some receipts letters get either chopped in half vertically or segmented wrongly because there is no space between letters (like CVS). This leads to often incorrect average letter width while letter height is much more reliable => use this ratio to derive digit width from digit height
} retailer_t;

// Keys to access the retailerInfo dictionary to determine if a value has been set (as opposed to having a default which all values do)
#define retailerKeyProductNumberLen "productNumberLen"

// Flags indicating attributes of a scanned frame
#define SCAN_STATUS_NODATA  0x1     // No structured data (no products, prices etc)
#define SCAN_STATUS_TOOFAR  0x2     // Camera too far from receipt
#define SCAN_STATUS_TRUNCATEDRIGHT   0x4     // Camera too far on the right (cut off product numbers on Target)
#define SCAN_STATUS_TRUNCATEDLEFT    0x8     // Camera too far on the left (cut off prices on Target)
//#define SCAN_STATUS_NONEWDATA 0x10  // No new products contributed by frame
#define SCAN_STATUS_TOOCLOSE  0x20  // Camera too close to receipt

#define WFScanFlagsNoData @"WFScanFlagsNoData"
#define WFScanFlagsTooFar @"WFScanFlagsTooFar"
#define WFScanFlagsTruncatedRight @"WFScanFlagsTruncatedRight"
#define WFScanFlagsTruncatedLeft @"WFScanFlagsTruncatedLeft"
#define WFScanFlagsNoNewData @"WFScanFlagsNoNewData"
#define WFScanFlagsTooClose @"WFScanFlagsTooClose"
#define WFScanFlagsDone @"WFScanFlagsDone"
#define WFScanFlagsMeaningful @"WFScanFlagsMeaningful"

#if !CODE_CPP
#import "WFRetailer.h"

@interface WFScanDelegate : NSObject

+ (NSString*)whitelistForRetailer:(WFRetailer*)retailer;
+ (NSArray *)excludedFontsForRetailer:(WFRetailer*)retailer;

- (NSDictionary *)processMicroBlinkResultsArray:(NSArray*)results
                                      withImage:(UIImage *)img
                                  imageIsBinary:(bool)isBinary
                                    newSequence:(bool)newSequence
                                    retailerId:(NSUInteger)retailerId
                                   serverPrices:(NSArray*)serverPricesArray
                              receiptImageIndex:(int *)receiptImageIndexPtr
                                    statusFlags:(NSDictionary**)statusFlags;

- (NSDictionary*)processServerPrices:(NSArray*)serverPrices;

- (UIImage *)createGPUBinaryImage:(UIImage *)img;
- (UIImage *)fixRotation:(UIImage *)image;

- (void)resetParser;

- (NSArray *)retailersForBarcode:(NSString *)barcode;

+ (int)validateFuzzyResults:(NSArray *)fuzzyTextArray index0Score:(float)score forScannedText:(NSString *)inputText isExactMatch:(bool *)exact;

- (void)setBlurRadius:(CGFloat)blurRadius;

- (int) getBlurryScoreForImage:(UIImage*)image;
- (NSArray*)getEdgesForImage:(UIImage*)image outContentPercent:(float*)contentPercent;

@end
#endif
