//
//  common.hh
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//
//  Contains definitions shared throughout the OCR parser code
//

#include <stdio.h>

#ifndef _OCRPARSER_COMMON_H_
#define _OCRPARSER_COMMON_H_

typedef unsigned char uint8_t;

#if DEBUG
#define DP_MULTI_PRODUCTS_TEST  0
#endif

#define USE_UNICODE				1

#if USE_UNICODE
#define OCR_CHAR	unichar
#else
#define WF_CHAR	char
#endif

#define OCR_MULTITHREAD 0

// Some OCR packages never err on the side of creating spaces where none exists (e.g. Blink) if which case it's best that we don't revisit existing spaces. Note: if running the parser in a productized library, need to make that a run-time parameter.
#define OCR_ELIMINATE_NARROW_SPACES     1
// Is everything on the same horizontal line sequentially related (yes in the case of receipts)?
#define OCR_ENTIRE_LINE_RELATED         1

#if CODE_CPP
#if ANDROID
#include <android/log.h>
#define NSLog(args...) {__android_log_print(ANDROID_LOG_DEBUG,"windfall", args);}
#else
void displayDebugMessage(const char* msg);
#define NSLog(args...) {char *buffer = (char *)malloc(2048); snprintf(buffer, 2048, args);displayDebugMessage(buffer); free(buffer);}
#endif // !ANDROID
#endif // CODE_CPP

// Retailers reconized by the application
typedef enum {
	RetailerUnknown,
	RetailerTarget
} RetailerIdentification;

#if DEBUG && !DPMODE

// Controls whether to display debug messages for various parts of the parsing
#define DEBUG_LOG				1       // General moderately verbose debugging
#define ERROR_LOG               1       // Serious errors, should always be set
#define WARNING_LOG				1       // Warning, best leave set to 1
#define OCR_LOG                 1       // Low level OCR issues
#define OCR_VERBOSE_LOG         0       // Tons of messages between steps, to identify crashes on Android
#define REGEX_LOG               1       // Messages around Regex parsing (semantics)
#define REPLACING_LOG           1       // Character and space corrections applied to output of underlying OCR
#define LAYOUT_LOG              1       // Debugging the logic that arranges items in blocks & lines
#define MERGING_LOG             1       // Debugging the logic merging frames

#if REGEX_LOG
#define SHOW_REGEX              0       // If RegexLog is also set, shows regexes used. WARNING: very verbose
#endif

#define BINARYIMAGE_LOG         0       // Save binary image to camera roll. Warning: time consuming
#define DEBUG_SINGLELETTER      0

#if DEBUG_LOG
#define DebugLog(args...) NSLog(args)
#else
#define DebugLog(args...)
#endif

#if WARNING_LOG
#define WarningLog(args...) NSLog(args)
#else
#define WarningLog(args...)
#endif

#if OCR_LOG
#define OCRLog(args...) NSLog(args)
#else
#define OCRLog(args...)
#endif

#if OCR_VERBOSE_LOG
#define OCRVerboseLog(args...) NSLog(args)
#else
#define OCRVerboseLog(args...)
#endif

#if ERROR_LOG
#define ErrorLog(args...) NSLog(args)
#else
#define ErrorLog(args...)
#endif

#if REGEX_LOG
#define RegexLog(args...) NSLog(args)
#else
#define RegexLog(args...)
#endif

#if REPLACING_LOG
#define ReplacingLog(args...) NSLog(args)
#else
#define ReplacingLog(args...)
#endif

#if LAYOUT_LOG
#define LayoutLog(args...) NSLog(args)
#else
#define LayoutLog(args...)
#endif

#if MERGING_LOG
#define MergingLog(args...) NSLog(args)
#else
#define MergingLog(args...)
#endif

#define AutocorrectLog(args...) // Ignore for now

// Messages you wants to see even in Release build
#define ReleaseLog(args...) NSLog(args)



#define MERCHANT_TARGET

#else // !DEBUG

#define BINARYIMAGE_LOG         0
#define DEBUG_SINGLELETTER      0

#define DebugLog(x...)
#define ErrorLog(args...)
#define WarningLog(args...)
#define OCRLog(args...)
#define OCRVerboseLog(args...)
#define RegexLog(args...)
#define ReplacingLog(args...)
#define LayoutLog(args...)
#define MergingLog(args...)
#define AutocorrectLog(args...)

// Messages you wants to see even in Release build
#define ReleaseLog(args...) NSLog(args)

#endif // !DEBUG

// Keys for the dictionary representing a match in OCRResults

#define matchKeyText            "text"          // Text
#define matchKeyText2           "text2"         // Alternative text (OCR returned different text)
#define matchKeyText3           "text3"         // Alternative text (OCR returned different text)
#define matchKeyFullLineText    "fulllinetext"  // Text of entire line before semantic parsing
#define matchKeyFullLineText2   "fulllinetext2"
#define matchKeyFullLineText3   "fulllinetext3"
#define matchKeyCountMatched    "countMatched"  // Number of time "text" was found in scanned frames
#define matchKeyCountMatched2   "countMatched2" // Number of time "text2" was found in scanned frames
#define matchKeyCountMatched3   "countMatched3" // Number of time "text3" was found in scanned frames
#define matchKeyOrigText        "origtext"      // Text before mods driven by semantic parsing
#define matchKeyElementType     "type"          // High level type, e.g. product number
#define matchKeyElementOrigType "origtype"      // Type before is was changed (e.g. unknow -> product price)
#define matchKeyN               "n"             // Number of rects (one for each character and space)
#define matchKeyRects           "rects"         // Linked list of rectangles for the characters
#define matchKeyConfidence      "confidence"    // Quality indicator for OCR result (from 0 to 100)
#define matchKeyConfidence2     "confidence2"   // Quality indicator for OCR result (from 0 to 100)
#define matchKeyConfidence3     "confidence3"   // Quality indicator for OCR result (from 0 to 100)
#define matchKeyRange           "range"         // Start & length of the rect where the match fits within the image
#define matchKeyRect            "rect"			// The X,Y range encompassing the match's rects (not saved to DB, generated on the fly)
#define matchKeySlant           "slant"         // The slope of the match: will be positive if lines are lower on the right side
#define matchKeyBlock           "block"         // Used to link adjacent lines together
#define matchKeyLine            "line"			// Unique line within a scanned frame (used to be unique within a block)
#define matchKeyNewLine         "newline"		// Temporary, used to log the new line number during spacing calculations by setBlocks()
#define matchKeyGlobalLine      "globalline"	// Present in some new matches, indicates the line number of matching product in existing results
#define matchKeyColumn          "column"		// Used within a line
#define matchKeyPosition        "position"		// 0-based, index of line accross block (i.e. all OCR lines get a unique sequential index from 0 to N)
#define matchKeyStats           "stats"			// Word-level stats built dynamically, to be used by code consuming the OCR results even after our analysis
#define matchKeyAverageHeightTallLetters "matchKeyAverageHeightTallLetters" // Average height of tall letters (digits, uppercase, tall lowercase) - used as better proxy of item height (works better when there is a slope)
#define matchKeyAverageLetterWidth "averageletterwidth" // Average width of letters in match
#define matchKeyXMin		"xMin"				// Used internally to calculate the range occupied by an item
#define matchKeyXMax		"xMax"				// Used internally to calculate the range occupied by an item
#define matchKeyYMin		"yMin"				// Used internally to calculate the range occupied by an item
#define matchKeyYMax		"yMax"				// Used internally to calculate the range occupied by an item
#define matchKeyStatus		"status"			// Internal flags (see below for values)
#define matchKeyQuantity    "quantity"          // Quantity (int) value possibly corresponding to that product
#define matchKeyQuantity2   "quantity2"
#define matchKeyQuantity3   "quantity3"
#define matchKeyPricePerItem    "pricePerItem"  // Price per item (float) value possibly corresponding to that product
#define matchKeyPricePerItem2   "pricePerItem2"
#define matchKeyPricePerItem3   "pricePerItem3"
#define matchKeyPercentVerticalLines "percentVerticalLines" // Percent of non-space chars that are vertical lines (I,1,/)
#define matchKeyNumNonSpaceLetters "numNonSpaceLetters" // Number of letters that aren't space (prior to OCR corrections)


// Values for matchKeyStatus                   // Internal flags
#define matchKeyStatusTouchingLeftSideItem  0x1  //item belongs on the same line as item to the left (for whatever reason we determined)
#define matchKeyStatusPossiblePricePerItem  0x2  // Price is at a location such that it could be part of a quantity / price-per-item line
#define matchKeyStatusInPricesColumn        0x4  // Price is at a location such that it could be part of a product price or discount
#define matchKeyStatusPriceCameWithProduct  0x8  // Price was found in a frame where a product number was also found on the same line
#define matchKeyStatusNotAProduct           0x10 // Just looks like a product (e.g. coupon number on discount line)
#define matchKeyStatusProductWithPriceOnNextLine 0x20 // Happens on some Walmart products
#define matchKeyStatusHasTaxAbove           0x40 // Set when we found this price after a TOTAL and with a tax amount above
#define matchKeyStatusMerged                0x80    // Was merged, need to recalculate spaces
#define matchKeyStatusInProductsColumn      0x100   // Located within the range for product numbers (i.e. can't be a product price), set only when we are sure
#define matchKeyStatusBeforeProductsColumn  0x200   // Located left of the range for product numbers (i.e. can't be a product price), set only when we are sure
#define matchKeyStatusHasCoupon             0x400   // There is a coupon associated with this product (e.g. Macy's)
#define matchKeyStatusHasQuantity           0x800   // Use to tag items with quantity even when quanitity is 1 (used for CVS to try to determine bad products where no quantity was found)
#define matchKeyStatusHasDollar             0x1000  // Use to tag prices with a dollar in front of them
#define matchKeyStatus2DigitDate            0x2000  // Use to tag dates where date has only 2 digits (e.g. 06/03/16")
#define matchKeyStatusSlashDivider          0x4000  // Use to tag dates with / in the date part (e.g. 06/03/16")
#define matchKeyStatusDashDivider           0x8000  // Use to tag dates with - in the date part (e.g. 06-03-16")
#define matchKeyStatusDateAMPM              0x10000  // Use to tag dates with AM/PM in time part (e.g. 06-03-16 03:51 PM")
#define matchKeyStatusDateHasDatePrefix     0x20000  // E.g. "DATE-" before ToysRUs dates
#define matchKeyStatusDateHasTimePrefix     0x40000  // E.g. "TIME-" before ToysRUs dates
#define matchKeyStatusTotal                 0x80000

// Types reconized by the application. Do NOT change the order, these values are in the DB as integers
typedef enum {
	OCRElementTypeInvalid,              // 0 - After semantic analysis, a type of element we want to completely ignore
	OCRElementTypeUnknown,              // Still unknown after attempts to determine type
    OCRElementTypeNotAnalyzed,          // Right after getting raw OCR results, without any semantic analysis performed yet
    OCRElementTypeProductNumber,        // 3
    OCRElementTypePrice,                // 4
    OCRElementTypeProductDescription,   // 5
    OCRElementTypeProductQuantity,      // 6 - Used for lines indicating X items @ price y.yy
    OCRElementTypeDate,                 // 7
    OCRElementTypeSubtotal,             // 8 - Tags the text preceding the subtotal, e.g. "SUBTOTAL"
    OCRElementTypePotentialPriceDiscount,   // Temporary type until upgraded to OCRElementTypePriceDiscount if located in the prices column
    OCRElementTypePriceDiscount,        // 10
    OCRElementTypePartialProductPrice,      // 11 - Has a $ or a digit and located in the prices column
    OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine,  // 12 - Internal only, e.g. 000000004093KI for Walmart ONIONS product 
    OCRElementTypeMarkerOfPriceOnNextLine,  // 13 - Internal only, e.g. the "KI" part after some Walmart products
    OCRElementTypeIgnoredProductNumber,     // 14 - products which will never match anything on the backend (eg 6-digit Babiesrus products)
    OCRElementTypeDateMonth,                // 15 - Internal only, the month component within a date
    OCRElementTypeDateDay,                  // 16 - Internal only, the day component within a date
    OCRElementTypeDateYear,                 // 17 - Internal only, the year component within a date
    OCRElementTypeDateHour,                 // 18 - Internal only, the year component within a date
    OCRElementTypeDateMinutes,              // 19 - Internal only, the year component within a date
    OCRElementTypeDateAMPM,                 // 20 - Internal only, the AM/PM component within a date
    OCRElementTypePricePerItem,             // 21 - distinct from Price so we can tell it apart from the price later on same line
    OCRElementSubTypeDollar,                // 22 - Internal only, the '$' in front of prices
    OCRElementTypeTotal,                    // 23 - Set only when we also found the tax amount above
    OCRElementTypeNumericSequence,          // 24 - Internal only, used from fragments within numeric types (e.g. product number)
    OCRElementTypePartialSubtotal,          // 25 - subtotal in the middle of receipt, helps merging for Walmart
    OCRElementTypeTotalWithoutTax,          // 26 - found TOTAL but not the tax amount above it - set anyhow to help with merging
    OCRElementTypeCoupon,                   // 27 - Internal only
    OCRElementTypeTax,                      // 28 - Internal only, used in sequences like "TAX 1.79 BAL 4.75" (Kmart)
    OCRElementTypeTaxPrice,                 // 29 - Internal only, used in sequences like "TAX 1.79 BAL 4.75" (Kmart)
    OCRElementTypeBal,                      // 30 - Internal only, used in sequences like "TAX 1.79 BAL 4.75" (Kmart)
    OCRElementTypeBalPrice,                 // 31 - Internal only, used in sequences like "TAX 1.79 BAL 4.75" (Kmart)
    OCRElementTypeProductPrice,             // 32 - Used for retailers w/o product numbers (e.g. CVS), to indicate the price is known to belong to a product
    OCRElementTypeDateMonthLetters,         // 33 - Internal only, the month component within a date in uppercase letters (e.g. "MARCH")
    OCRElementTypePhoneNumber,              // 34 - Internal only, used to help merge
    OCRElementTypeMergeablePattern1,        // 35 - Internal only, used to help merge
    OCRElementSubTypeDateDateDivider,       // 36 - Internal only, - or / within dates
    OCRElementSubTypeDateDatePrefix,        // 37 - Internal only, "DATE-" for ToysRUS
    OCRElementSubTypeDateTimePrefix         // 38 - Internal only, "TIME-" for ToysRUS
} OCRElementType;

// Target values
//#define RETAILER_MIN_ITEM_LEN                   3   // Any item shorter than that should be eliminated (set to 0 if even single letter items have meaning)
//#define RETAILER_PRODUCT_DESCRIPTION_MIN_LEN    3
//#define RETAILER_PRODUCT_NUMBER_LEN             9
//#define RETAILER_STRIP_AT_DESC_END              L"T,F,FT,FN"  // Strip these if found at the end of product descriptions
//#define RETAILER_PRICES_HAVE_DOLLAR             1
//#define RETAILER_PRICES_AFTER_PRODUCT           1       // Price shows up to the right of the product number
//#define RETAILER_LINE_SPACING                   0.28    // As percentage of uppercase/digit height for Target receipts
//#define RETAILER_MAX_PRICE                      1000.00

//#define RETAILER_PRODUCT_COMPONENT1             OCRElementTypeProductNumber
//#define RETAILER_PRODUCT_COMPONENT2             OCRElementTypeProductQuantity
//#define RETAILER_PRODUCT_COMPONENT3             OCRElementTypeProductDescription
//#define RETAILER_PRODUCT_COMPONENT4             OCRElementTypeSubtotal
//#define RETAILER_PRODUCT_COMPONENT5             OCRElementTypePrice
// Valid starting type for interesting sequences
//#define RETAILER_GROUP_START1                   OCRElementTypeProductNumber
//#define RETAILER_GROUP_START2                   OCRElementTypeProductQuantity
//#define RETAILER_GROUP_START3                   OCRElementTypeSubtotal

typedef enum {	
	LanguageENG=0,//  1 English -	US
	LanguageBUL, //  2 Bulgarian -	Bulgaria
	LanguageCES, //  3 Czech -		Czech Republic
	LanguageCHS, // Chinese (Simplified) - China
	LanguageCHT, // Chinese (Traditional) - China
	LanguageDAN, // Danish -		Denmark
	LanguageDEU, // German -		Germany
	LanguageELL, //  8 Greek -		Greece
	LanguageFIN, // Finnish -		Finland
	LanguageFRA, // 10 French -		France
	LanguageHUN, // Hungarian -		Hungary
	LanguageIND, // Indonesian -	Indonesia
	LanguageITA, // Italian -		Italy
	LanguageJPN, // Japanese -		Japan
	LanguageKOR, // Korean -		South Korea
	LanguageLAV, // Latvian -		Latvia
	LanguageLIT, // Lithuanian -	Lithuania
	LanguageNLD, // Dutch -			The Netherlands
	LanguageNOR, // Norwegian -		Norway
	LanguagePOL, // Polish -		Poland
	LanguagePOR, // Portuguese -	Brazil
	LanguageRON, // Romanian -		Romania
	LanguageRUS, // 23 Russian -	Russian Republic
	LanguageSLK, // Slovakian -		Slovak Republic
	LanguageSPA, // Spanish -		Spain
	LanguageSRP, // 26 Serbian -	Serbia
	LanguageSWE, // 27 Swedish -	Sweden
	LanguageTUR, // 28 Turkish -	Turkey
	LanguageUKR, // 29 Ukranian -	Ukraine
	LanguageVIE, // 30 Viet -		Vietnam	
	LanguageHEB, // 31 Hebrew -		Israel
	LanguageSLV // 32 Slovenian -	Slovenia
} LanguageCode;

typedef enum {
	countryCodeInvalid = 0,
	countryCodeAF,
	countryCodeAX,
	countryCodeAL,
	countryCodeDZ,
	countryCodeAS,
	countryCodeAD,
	countryCodeAO,
	countryCodeAI,
	countryCodeAQ,
	countryCodeAG, // 10
	countryCodeAR,
	countryCodeAM,
	countryCodeAW,
	countryCodeAC,
	countryCodeAU,
	countryCodeAT,
	countryCodeAZ,
	countryCodeBS,
	countryCodeBH,
	countryCodeBB, // 20
	countryCodeBD,
	countryCodeBY,
	countryCodeBE,
	countryCodeBZ,
	countryCodeBJ,
	countryCodeBM,
	countryCodeBT,
	countryCodeBW,
	countryCodeBO,
	countryCodeBA, // 30
	countryCodeBV,
	countryCodeBR,	// Brazil
	countryCodeIO,
	countryCodeBN,
	countryCodeBG,	// Blugaria
	countryCodeBF,
	countryCodeBI,
	countryCodeKH,
	countryCodeCM,
	countryCodeCA, // 40
	countryCodeCV,
	countryCodeKY,
	countryCodeCF,
	countryCodeTD,
	countryCodeCL,
	countryCodeCN,
	countryCodeCX,
	countryCodeCC,
	countryCodeCO,
	countryCodeKM,
	countryCodeCG,
	countryCodeCD,
	countryCodeCK,
	countryCodeCR,
	countryCodeCI,
	countryCodeHR,
	countryCodeCU,
	countryCodeCY,
	countryCodeCZ,//countryCodeCS,
	countryCodeDK,
	countryCodeDJ,
	countryCodeDM,
	countryCodeDO,
	countryCodeTP,
	countryCodeEC,
	countryCodeEG,
	countryCodeSV,
	countryCodeGQ,
	countryCodeER,
	countryCodeEE,
	countryCodeET,
	countryCodeFK,
	countryCodeFO,
	countryCodeFJ,
	countryCodeFI,
	countryCodeFR,
	countryCodeGF,
	countryCodePF,
	countryCodeTF,
	countryCodeMK,
	countryCodeGA,
	countryCodeGM,
	countryCodeGE,
	countryCodeDE,
	countryCodeGH,
	countryCodeGI,
	countryCodeGR,
	countryCodeGD,
	countryCodeGL,
	countryCodeGP,
	countryCodeGU,
	countryCodeGT,
	countryCodeGG,
	countryCodeGN,
	countryCodeGY,
	countryCodeHT,
	countryCodeHM,
	countryCodeHN,
	countryCodeHK,
	countryCodeHU,
	countryCodeIS,
	countryCodeIN,
	countryCodeID,
	countryCodeIR,
	countryCodeIQ,
	countryCodeIE,
	countryCodeIL,
	countryCodeIM,
	countryCodeIT,
	countryCodeJE,
	countryCodeJM,
	countryCodeJP,
	countryCodeJO,
	countryCodeKZ,
	countryCodeKE,
	countryCodeKI,
	countryCodeKP,
	countryCodeKR,
	countryCodeXK,
	countryCodeKW,
	countryCodeKG,
	countryCodeLA,
	countryCodeLV,
	countryCodeLB,
	countryCodeLI,
	countryCodeLR,
	countryCodeLY,
	countryCodeLS,
	countryCodeLT,
	countryCodeLU,
	countryCodeMO,
	countryCodeMG,
	countryCodeMW,
	countryCodeMY,
	countryCodeMV,
	countryCodeML,
	countryCodeMT,
	countryCodeMH,
	countryCodeMQ,
	countryCodeMR,
	countryCodeMU,
	countryCodeYT,
	countryCodeMX,
	countryCodeFM,
	countryCodeMD,
	countryCodeMC,
    countryCodeMN,
	countryCodeME,
	countryCodeMS,
	countryCodeMA,
	countryCodeMZ,
	countryCodeMM,
	countryCodeNA,
	countryCodeNR,
	countryCodeNP,
	countryCodeNL,
	countryCodeAN,
	countryCodeNC,
	countryCodeNZ,
	countryCodeNI,
	countryCodeNE,
	countryCodeNG,
	countryCodeNF,
	countryCodeMP,
	countryCodeNO,
	countryCodeOM,
	countryCodePK,
	countryCodePW,
	countryCodePS,
	countryCodePA,
	countryCodePG,
	countryCodePY,
	countryCodePE,
	countryCodePH,
	countryCodePN,
	countryCodePL,
	countryCodePT,
	countryCodePR,
	countryCodeQA,
	countryCodeRE,
	countryCodeRO,
	countryCodeRU,
	countryCodeRW,
	countryCodeKN,
	countryCodeLC,
	countryCodeVC,
	countryCodeWS,
	countryCodeSM,
	countryCodeST,
	countryCodeSA,
	countryCodeSN,
	countryCodeRS,
	countryCodeSC,
	countryCodeSL,
	countryCodeSG,
	countryCodeSI,
	countryCodeSK,
	countryCodeSB,
	countryCodeSO,
	countryCodeZA,
	countryCodeES,
	countryCodeLK,
	countryCodeSH,
	countryCodePM,
	countryCodeSD,
	countryCodeSR,
	countryCodeSJ,
	countryCodeSZ,
	countryCodeSE,
	countryCodeCH,
	countryCodeSY,
	countryCodeTW,
	countryCodeTJ,
	countryCodeTZ,
	countryCodeTH,
	countryCodeTG,
	countryCodeTK,
	countryCodeTO,
	countryCodeTT,
	countryCodeTN,
	countryCodeTR,
	countryCodeTM,
	countryCodeTC,
	countryCodeTV,
	countryCodeUG,
	countryCodeUA,
	countryCodeAE,
	countryCodeUK,
	countryCodeUS,
	countryCodeUM,
	countryCodeUY,
	countryCodeUZ,
	countryCodeVU,
	countryCodeVA,
	countryCodeVE,
	countryCodeVN,
	countryCodeVG,
	countryCodeVI,
	countryCodeWF,
	countryCodeEH,
	countryCodeYE,
	countryCodeZM,
	countryCodeZW,
    countryCodeSS
} CountryCode;

// Note: default (0) is copy to derived elements + blank out in base item
#define groupOptionsCopyOnly	1 // Only copy the group to the elements array, don't blank in the original string
#define groupOptionsDontCopy	2 // Don't copy that group to the elements array, don't blank in the original string
#define groupOptionsCopyStartNonZero	3 // Copy only starting at the first non-zero digit, e.g. 0100 -> 100, 1120 -> 1120
#define groupOptionsDeleteOnly	4 // Just delete that group from original string, and don't copy to elements
#define groupOptionsDeleteAndTagAsGroupType  5 // Delete and change the type being searched for to the type associated with the group
#define groupOptionsDeleteAndSetFlag  6 // Delete and set the flag corresponding to the group type (e.g. if group type is OCRElementTypeDateYear, set the flag for 1 or 2 digit date)

// Macros to access CGRect stuctures (composed of an origin point (x,y) and a size (width, height))
// Note: in an image model where (0,0) is the top-left corner, rectTop is actually the bottom of the letter 
#define rectSpaceBetweenRects(r1,r2)		(r2.origin.x - (r1.origin.x+r1.size.width))
#define rectDeltaBetweenBottoms(r1,r2)		(r2.origin.y - r1.origin.y)
#define rectLeft(r)							(r.origin.x)
#define rectEnd(r)							(r.origin.x + r.size.width - 1)
#define rectRight(r)						(r.origin.x + r.size.width - 1)
#define rectTop(r)							(r.origin.y + r.size.height - 1)
#define rectBottom(r)						(r.origin.y)
#define rectCenterX(r)                      (r.origin.x + r.size.width/2)

#endif
