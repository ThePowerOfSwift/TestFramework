#ifndef _OCRPARSER_UTILS_H_
#define _OCRPARSER_UTILS_H_

//
//  OCRUtils.hh
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

#include <string>

#import "OCRPage.hh"
#import "single_letter/SingleLetterTests.h"
#import "WFScanDelegate.h"

namespace ocrparser {

using namespace std;

#define OCR_TALL_TO_LOWERCASE_RATIO				1.15    // Minimum ratio between tall (uppercase, digit or tall uppercase) and normal lowercase
#define OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE	1.12

#define CERTFORLOWHURDLE	4   // e.g. number of uppercase we require before we use a low hurdle to test other letters based on the average height of uppercase letters

// Verify that main component is at least 75% of total rect size
#define SINGLE_LETTER_VALIDATE_COMP_SIZE    0x01
// Verify no minor component exceeds 5% of main component area
#define SINGLE_LETTER_VALIDATE_SINGLE_COMP  0x02

#define a_TESTED_AS_o           0x01
#define TESTED_OPENING_RIGHT    0x02
#define TESTED_OPENING_LEFT     0x04
#define TESTED_OPENING_TOP      0x08
#define TESTED_OPENING_BOTTOM   0x10
#define TESTED_OPENING_MULTI    0x20
#define TESTED_AS_CL            0x40
#define FLAGS_TESTED_DOT_AFTER  0x80
#define FLAGS_SUSPECT           0x100   // Used when we make a replacement we are not certain about
#define FLAGS_OUTLIER           0X200   // Removed from stats as outlier
#define FLAGS_TESTED_AS_5       0x400
#define FLAGS_TESTED_AS_DISCONNECTED_DIGITDOLLAR 0x800
#define FLAGS_TESTED_DOT_BEFORE 0x1000

#define FLAGS2_TESTED_AS_DIGIT  0x01
#define NO_SPACE                0x02
#define FLAGS2_TESTED_AS_B      0x04
#define FLAGS2_TESTED           0x08
#define FLAGS2_TESTED_e_AS_6    0x10
#define FLAGS2_TESTED_S_AS_DIGIT 0x20
#define FLAGS2_TESTED_AS_a      0x40
#define FLAGS2_TESTED_AS_F      0x80
#define FLAGS2_TESTED_AS_DASH   0x100
#define FLAGS2_TESTED_AS_J      0x200
#define FLAGS2_TESTED_AS_A      0x400
#define FLAGS2_TESTED_AS_r      0x800
#define FLAGS2_TESTED_AS_4      0x1000
#define FLAGS2_TESTED_AS_9      0x2000
#define FLAGS2_TESTED_AS_1      0x4000
#define FLAGS2_TESTED_AS_2      0x8000
    
#define FLAGS3_TESTED_AS_m      0x01
#define FLAGS3_TESTED_AS_6      0x02
#define FLAGS3_TESTED_AS_K      0x04
#define FLAGS3_TESTED_OPENING_BOTTOM_LEFT 0x08
#define FLAGS3_TESTED_AS_G      0x10
#define FLAGS3_TESTED_AS_Z      0x20
#define FLAGS3_TESTED_AS_7      0x40
#define FLAGS3_TESTED_AS_Q      0x80
#define FLAGS3_TESTED_AS_E      0x100
#define FLAGS3_TESTED_AS_C      0x200
#define FLAGS3_TESTED_AS_PLUS   0x400
#define FLAGS3_TESTED_AS_8      0x800
#define FLAGS3_TESTED_AS_4      0x1000
#define FLAGS3_TESTED_AS_DISCONNECTED_DIGIT9 0x2000
#define FLAGS3_TESTED_AS_DISCONNECTED_DIGIT4 0x4000
#define FLAGS3_TESTED_AS_TWO_RECTS 0x8000

#define FLAGS4_TESTED_RIGHT_BAR  0x01
#define FLAGS4_TESTED_AS_t      0x02
#define FLAGS4_TESTED_FOR_MISSING_DOT      0x04
#define FLAGS4_TESTED_AS_5      0x08
#define FLAGS4_TESTED_AS_c      0x10
#define FLAGS4_TESTED_l_AS_1    0x20
#define FLAGS4_TESTED_OPENING_LEFT   0x40
#define FLAGS4_TESTED_AS_h      0x80
#define FLAGS4_TESTED_AS_8      0x100
#define FLAGS4_TESTED_FOR_NOISE 0x200
#define FLAGS4_TESTED_AS_P      0x400
#define FLAGS4_TESTED_AS_D      0x800
#define FLAGS4_TESTED_AS_DISCONNECTED_DIGIT0 0x1000
#define FLAGS4_TESTED_AS_DISCONNECTED_DIGIT6 0x2000
#define FLAGS4_TESTED_AS_DISCONNECTED_DIGIT7 0x4000
#define FLAGS4_TESTED_AS_DISCONNECTED_DIGIT8 0x8000

#define FLAGS5_TESTED_AS_M      0x01
#define FLAGS5_TESTED_AS_DOT    0x02
#define FLAGS5_TESTED_AS_D      0x04
#define FLAGS5_TESTED_AS_M_versus_m 0x08
#define FLAGS5_TESTED_AS_e      0x10
#define FLAGS5_TESTED_AS_SOLID  0x20
#define FLAGS5_TESTED_AS_v      0x40
#define FLAGS5_TESTED_AS_w      0x80
#define FLAGS5_TESTED_AS_G      0x100
#define FLAGS5_TESTED_AS_SUBSUMED      0x200
#define FLAGS5_TESTED_FOUND_GLUED      0x400
#define FLAGS5_TESTED_DOLLAR_BEFORE    0x800
#define FLAGS5_TESTED_AS_DISCONNECTED_DIGIT2 0x1000
#define FLAGS5_TESTED_AS_DISCONNECTED_DIGIT3 0x2000
#define FLAGS5_TESTED_AS_DISCONNECTED_DIGIT5 0x8000

#define FLAGS6_TESTED_AS_P      0x001
#define FLAGS6_TESTED_AS_COLUMN 0x002
#define FLAGS6_TESTED_AS_O      0x004
#define FLAGS6_TESTED_AS_B      0x008
#define FLAGS6_TESTED_DOT       0x010
#define FLAGS6_TESTED_SQUARE_BRACKET 0x20
#define FLAGS6_TESTED_CIRCUMFLEX 0x40
#define FLAGS6_TESTED_AS_H      0x080
#define FLAGS6_TESTED_AS_U      0x100
#define FLAGS6_TESTED_AS_I      0x200
#define FLAGS6_TESTED_AS_b      0x400
#define FLAGS6_TESTED_AS_N      0x800
#define FLAGS6_TESTED_o_AS_a    0x1000
#define FLAGS6_TESTED_AS_Reala  0x2000
#define FLAGS6_TESTED_AS_i      0x4000
#define FLAGS6_TESTED_AS_ANDSIGN 0x8000

#define FLAGS7_TESTED_AS_DISCONNECTED_RIGHTRECT_DIGIT8  0x0001
#define FLAGS7_TESTED_AS_DISCONNECTED_DIGIT_ALL         0x0002
#define FLAGS7_TESTED_AS_DISCONNECTED_UPPERCASE_ALL     0x0004
#define FLAGS7_TESTED_AS_DISCONNECTED_DIGIT_LEFT3       0x0008
#define FLAGS7_TESTED_AS_T       0x0010
#define FLAGS7_TESTED_AS_PERCENT 0x0020
#define FLAGS7_TESTED_AS_0       0x0040


#define STATUS_SQUARE_BRACKET_NOT_J  1
#define STATUS_c_INSTEADOF_6    2

// Minimum ratio between tall (uppercase, digit or tall uppercase and normal lowercase)
#define OCR_TALL_TO_LOWERCASE_RATIO				1.15
#define OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE	1.12
#define OCR_MIN_WIDTH							5
#define OCR_MIN_HEIGHT_NORMALLOWERCASE			5
#define OCR_ACCEPTABLE_ERROR_WIDTH				0.15
#define OCR_BELOW_LINE_TO_HEIGHT_RATIO			0.25
#define OCR_NORMAL_LOWERCASE_WIDTH_TO_SUPERNARROW_RATIO 1.8
#define OCR_MAX_H_TO_W_RATIO_DOT                1.4
#define OCR_MAX_H_TO_W_RATIO_DOT_SAFE           1.75    // Any dot beyond that treated as a comma
#define OCR_MAX_H_TO_W_RATIO_O                  2.0 // Anything beyond is treated as 0 instead
#define OCR_MIN_GAPBELOW_LINE_COMMA_STRICT      0.30
#define OCR_RATIO_DOT_SIZE_TO_NORMAL_LETTER_MIN     0.024   
#define OCR_RATIO_DOT_SIZE_TO_NORMAL_LETTER_MAX     0.18    // Found ratio was 10.5%
#define OCR_RATIO_DOT_HEIGHT_TO_NORMAL_LETTER_MAX   0.40
#define OCR_RATIO_DOT_WIDTH_TO_NORMAL_LETTER_MAX    0.47
#define OCR_RATIO_DOT_AREA_TO_i_AREA_MAX        0.35
#define OCR_RATIO_DOT_AREA_TO_i_AREA_MIN        0.10
#define OCR_RATIO_I_MIN_HEIGHT_TO_WIDTH         2.75
#define CERTFORLOWHURDLE	4
// Anything with an area larger than that percent of the main connected component is considered significant
#define OCR_RATIO_SIGNIFICANT_AREA  0.05
#define OCR_YMIN_CENTER_HOLE_9                  0.60    // Min height of the center of a hole is '9'
#define OCR_MIN_GAP_BELOW_LINE_g                0.25    // Pure guess
#define OCR_RATIO_DASH_MIN_WIDTH_TO_HEIGHT     1.75

bool isLetter(wchar_t ch);
bool isVowel(wchar_t ch);
bool isConsonant(wchar_t inputChar);
int countConsonnantsAround (SmartPtr<OCRRect> r);
bool isDigit(wchar_t ch, bool lax = false);
bool isLetterOrDigit(wchar_t ch);
bool isLetterOrDigit(wchar_t ch);   // Tests if a Unicode character is a letter or digit
bool isLower(wchar_t ch);           // Tests if a Unicode character is a lowercase letter
bool isUpper(wchar_t ch);           // Tests if a Unicode character is a uppercase letter

bool isTallAbove(wchar_t ch);       // Is letter tall above baseline (e.g. l,f,t)
bool isTallBelow(wchar_t ch);       // Is letter tall below baseline (e.g. p,q)
bool isNormalWidthLowercase(wchar_t ch);
bool isNormalChar(wchar_t ch);
bool abnormalSpacing(wchar_t ch);
bool isNarrow(wchar_t ch);          // Tests if a letter is narrower than "normal" letters, e.g. l or t
bool isSuperNarrow1(wchar_t ch);
bool isSuperNarrow(wchar_t ch);
bool looksSameAsLowercase(wchar_t ch);
bool isLike1 (wchar_t ch);
bool isLeftOverhangUppercase(wchar_t ch);

bool isVerticalLine (wchar_t ch, OCRResults *results = NULL, bool strict = false);
bool hasVerticalLeftSide(wchar_t ch);
bool hasVerticalRightSide(wchar_t ch);

wstring toLower(const wstring& text);   // Lowercases a Unicode string
wchar_t toLower(wchar_t ch);            // Lowercases a Unicode characters
wstring toUpper(const wstring& text);
wchar_t toUpper(wchar_t ch);
bool isBlank(const wstring& text);      // Check if string contains only blanks

std::string toUTF8(const std::wstring& utf32);
std::wstring toUTF32(const std::string& utf8);

#define CONNECTEDCOMPORDER_XMAX_DESC    0
vector<int> sortConnectedComponents(ConnectedComponentList &cpl, int method, int startIndex=-1, int endIndex=-1);

bool OCRUtilsOverlapping(float min1,float max1, float min2, float max2);
bool OCRUtilsOverlappingX(CGRect r1, CGRect r2);
bool OverlappingX(ConnectedComponent &cc1, ConnectedComponent &cc2);
bool OCRUtilsOverlappingY(CGRect r1, CGRect r2);

int findAndReplace(std::wstring& data, const std::wstring& searchString, const std::wstring& replaceString);

void convertIndexes(const std::string& utf8, int byteOffset, int byteLen, int& letterOffset, int& letterCount); // Determin
    
bool spacingAdjustCalculateValues(SmartPtr<OCRWord> line, retailer_t *retailerParams, SmartPtr<OCRRect> prev, SmartPtr<OCRRect> next, bool isURLOrEmail, float *gap, float *realGap, float *effectiveGap, float *gapDividedByPreviousCharWidth, float *gapDividedByAverageWidth, float *effectiveGapDividedByAverageWidth, float *realGapDividedByAverageWidth, bool *bothDigits, bool *clearlyNoSpace, bool *reallyClearSpace, OCRResults *results);

bool isCloseOrSmallerThanAverage(const AverageWithCount& av, float val, float tolerance);
bool isCloseVal1ToVal2(float val1, float val2, float tolerance);
int indexCloserValToAv1OrAv2(float val, AverageWithCount av1, AverageWithCount av2);
int indexCloserValToVal1OrVal2(float val, float val1, float val2);
float heightBelow (SmartPtr<OCRRect> r, bool upsideDown);
float gapBelow (SmartPtr<OCRRect> r, int nRects=0, OCRWordPtr statsWithoutCurrent = OCRWordPtr());
float gapAboveBaselinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool perm, bool skipOne = false);
float gapAboveBaselinePercent (SmartPtr<OCRRect> r, ConnectedComponent &cc, bool per, bool skipOne = false);
float gapAboveBaseline (SmartPtr<OCRRect> r);
float gapAboveBaselineOfOtherRect(SmartPtr<OCRRect> r, SmartPtr<OCRRect> referenceR);
float gapBelowToplineOfOtherRect(SmartPtr<OCRRect> r, SmartPtr<OCRRect> referenceR);
float gapBelowToplinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool per, bool skipOne = false);
float gapBelowToplinePercent (SmartPtr<OCRRect> r, ConnectedComponent &cc, bool per, bool skip);
float gapBelowBaselinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool per, bool skipOne);
float gapAboveToplineOfNeighbors(SmartPtr<OCRRect> r, int lowLetter, bool both, bool per = false);
int testBelowRelativeToNeigbors(SmartPtr<OCRRect> r, float *gapBelow = NULL);
int extendsBelowLineRelativeToNeighbors(SmartPtr<OCRRect> r, bool onlyWithinWord);
bool isLowRelativeToNeigbors(OCRRectPtr r, bool both, bool smallCap, OCRResults *results);
bool isClose(float v1, float v2, float tolerance);
bool isClose(const AverageWithCount& av, float otherValue, float tolerance);
bool isCloseOrGreaterThanAverage(const AverageWithCount& av, float val, float tolerance);
bool isDash (wchar_t ch);
bool isWordDelimiter(wchar_t ch);
bool isGreaterThanEquiv (wchar_t ch);
bool isBracket (wchar_t ch);
bool isOpenBracket (wchar_t ch);
bool isClosingBracket (wchar_t ch);
bool isCLookalike (wchar_t ch);
bool isDigitLookalikeExtended(wchar_t ch);
wchar_t digitForExtendedLookalike(wchar_t ch);
wstring convertDigitsLookalikesToDigits (wstring text);
bool isDecimalPointLookalike(wchar_t ch);
bool iEquiv(wchar_t ch);
bool uEquiv(wchar_t ch);
bool looksSameAsUppercase(wchar_t ch, OCRResults *results, bool *isAccented = NULL);
bool looksSameAsLowercase(wchar_t ch, OCRResults *results);
float maxApplicableHeight (OCRWordPtr stats);
int strictShortTest (OCRRectPtr r, int certLevel);
int tallHeightTest (OCRStats *av, float height, wchar_t ch, bool requireSame, int certLevel, bool lowHurdle);
int lowHeightTest (OCRStats *av, float height, int certLevel, bool lowHurdle, OCRResults *results);
bool isTallRelativeToNeigbors(SmartPtr<OCRRect> r, float height, bool both);
int countLongestLettersAndDigitsSequenceAfter (SmartPtr<OCRRect> r);
int countLongestDigitsSequence(const wstring& text);
int countLongestLettersSequence (const wstring& text);
int countDigitsBefore(SmartPtr<OCRRect> r);
int countDigitsAfter(SmartPtr<OCRRect> r);
int countDigitsInWord(SmartPtr<OCRRect> r);
int countCharsInWord(SmartPtr<OCRRect> r);
int countNonSpace(wstring text);
int countDigits(const wstring& text, bool lax = false);
int countLettersOrDigits (const wstring& text);
SmartPtr<OCRStats> countLetterWordLine (SmartPtr<OCRRect> r, wchar_t letter, bool entireLine);
int OCRUtilsCountUpperBefore(SmartPtr<OCRRect> r, int &totalCountLetters);
int OCRUtilsCountUpperAfter(SmartPtr<OCRRect> r, int &totalCountLetters);
int OCRUtilsCountUpper(SmartPtr<OCRRect> r, int &totalCountLetters);

int SingleLetterNumComponents(CGRect rect, OCRResults *results, float tolerance, SingleLetterTests *inputSt, ConnectedComponent *cc1, ConnectedComponent *cc2); 
bool validateConnectedComponents (ConnectedComponentList &cpl, CGRect &inputRect);
bool validateConnectedComponents (SingleLetterTests *st, CGRect &inputRect);

bool OCRUtilsOpeningsTest(CGRect inputRect, OCRResults *results,
                                      int side, //GET_OPENINGS_TOP/BOTTOM/LEFT/RIGHT
                                      bool neigbors4,
                                      float start_pos, float end_pos,
                                      bool bounds_required,
                                      float letter_thickness,
									  float* outWidth, float* outHeight, 
                                      float* outMaxDepth, float* outAverageWidth, float* outOpeningWidth,
                                      float* outMaxWidth, float* outArea);
bool OCRUtilsTopTest(CGRect inputRect, OCRResults *results, bool *res);                                      

int strictTallTest (OCRRectPtr r, float height, int certLevel);    
int widthTest (OCRStats *av, SmartPtr<OCRRect> r, float width, char ch, int certLevel, int testCriteria);

bool digitsNearby(OCRRectPtr r, bool strict = true);
#define GLUED_LEFT      0x01
#define GLUED_RIGHT     0x02
#define GLUED_BOTH      (GLUED_LEFT|GLUED_RIGHT)
unsigned short glued(OCRRectPtr r,unsigned short flags = GLUED_BOTH);

bool isNormalChar(wchar_t ch);
wchar_t realChar(SmartPtr<OCRRect> r);
bool isLetterOrDigitOrNormalHeight(wchar_t ch);
bool isDelimiter(wchar_t ch);
bool isQuote (wchar_t ch);
float abs_float(float f);

SingleLetterTests* CreateSingleLetterTests(CGRect inputRect, OCRResults *results, bool neighbors4 = false, int validation = 0, float minSize = 0.03, bool acceptInverted = false);
char OCRUtilsSuggestLetterReplacement(CGRect currentRect, wchar_t currentChar, double maxHeight, OCRResults *results, bool force_replace, float* i_body_height);
#if DEBUG
void SingleLetterPrint(ConnectedComponentList &cpl, CGRect &rect);
void SingleLetterPrint(ConnectedComponent &cc, CGRect &rect);
void SingleLetterPrint(SegmentList &sl, float ref);
#else
#define SingleLetterPrint(...)
#endif
void replaceTwo (SmartPtr<OCRRect> r, wchar_t newCh, const char *msg = "", float minBottom = -1.0f, float newHeight = -1.0f);
void replaceTwoWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg = NULL);
void replaceThreeWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg = NULL);
void replaceFourWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg = NULL);
CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2);
CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2, CGRect &rect3);
CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2, CGRect &rect3, CGRect &rect4);

int countDigitLookalikes(SmartPtr<OCRRect> r, bool after, OCRResults *results, bool acceptDot = false);
wchar_t testAsDisconnectedDigit(OCRRectPtr r, CGRect &rect, OCRWordPtr &stats, OCRResults *results, wchar_t onlyTestCh, wchar_t onlyTestCh2, bool checkWidth, int lax=0, bool singleRect=false);
bool testAsL (OCRRectPtr r, CGRect rect, OCRResults *results, bool singleRect);
wchar_t SingleLetterTestAsi(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results);
wchar_t SingleLetterTestAsGC(OCRRectPtr r, CGRect rect, OCRResults *results, bool forceChange, SingleLetterTests **inputStPtr = NULL);
wchar_t SingleLetterTestAsFP(OCRRectPtr r, CGRect rect, OCRResults *results, bool forceChange, SingleLetterTests **inputStPtr = NULL);
int SingleLetterTestAsM(OCRRectPtr r, CGRect rect, OCRResults *results);
bool SingleLetterTestAsm(OCRRectPtr r, OCRResults *results, char *ch1, CGRect *rect1, char *ch2, CGRect *rect2);
char SingleLetterTestAs17T(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, SingleLetterTests **inputStPtr = NULL);
bool SingleLetterTestAsV(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrentAndNext, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL, CGRect *outRec = NULL);
wchar_t SingleLetterTestAsnTilde(OCRRectPtr r, CGRect rect, OCRResults *results, SingleLetterTests **inputStPtr = NULL);
bool needToInsertDot (OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, ConnectedComponentList &cpl, CGRect &newRect1, CGRect &newRect2);
bool needToInsertDot (OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, CGRect &rect1, CGRect &rect2, wchar_t &newCh1, wchar_t &newCh2, SingleLetterTests **st = NULL);
int SingleLetterTestAs6(OCRRectPtr r, OCRResults *results, bool strict, SingleLetterTests *inputST);
bool SingleLetterTestCircumflex(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestAsO(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrentAndNext, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
wchar_t SingleLetterTestAsi(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results);
char SingleLetterTestSAsDigit(OCRRectPtr r, CGRect rect, int nRects, OCRResults *results, bool lax = false);
bool SingleLetterTestAsa(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, wchar_t &newCh, bool lineHasRealAs, bool *isRealA, bool strict, SingleLetterTests **inputStPtr = NULL);
bool SingleLetterTestAsA(OCRRectPtr r, CGRect rect, CGRect *outRect, OCRWordPtr &stats, OCRResults *results, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestW(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestAsb(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestAsSquareBracket(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestAsColumn(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, CGRect &newRect, SingleLetterTests **inputStPtr = NULL);
wchar_t SingleLetterTestAsAndSign(OCRRectPtr r, CGRect rect, bool testNextSize, OCRWordPtr &stats, OCRResults *results);
char SingleLetterTestoAsa (OCRRectPtr r,OCRResults *results, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestU(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar1, wchar_t &newChar2, CGRect &newRect1, CGRect &newRect2, SingleLetterTests **inputStPtr = NULL);
int SingleLetterTestAsD(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr);
wchar_t SingleLetterTest8(OCRRectPtr r,OCRResults *results);
wchar_t SingleLetterTestAsgOry(OCRRectPtr r, OCRResults *results, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestAsB(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
float SingleLetterTestH(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr = NULL);
wchar_t SingleLetterTestAsDigit(OCRRectPtr r,OCRResults *results, OCRWordPtr &stats, bool forceDigit);
wchar_t SingleLetterTestAsi(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results);
bool SingleLetterTestAsI(OCRRectPtr r, CGRect rect, OCRResults *results);
int isNormalDigitWidth(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, bool lax);
bool isGluedRectInCombinedRect (CGRect combinedRect, CGRect subRect, OCRResults *results);

bool OCRUtilsPossibleDollar(CGRect r, MatchPtr m, OCRResults *results);

void split(const std::wstring& source, wchar_t separator, std::vector<std::wstring>& result);
void split(const std::wstring& source, wstring separators, std::vector<std::wstring>& result);

int gapRectAboveToplineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, CGRect rect, float limit, bool absolute);
int gapAboveToplineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, float limit, bool absolute);
int gapRectAboveBaselineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, CGRect rect, float limit, bool absolute);
int gapAboveBaselineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, float limit, bool absolute);

int insertToFloatVectorInOrder (vector<float> &v, float val);
bool foundInVector (vector<int> v, int value);

bool possiblePrice(SmartPtr<OCRRect> r, SmartPtr<OCRRect> *lastR, SmartPtr<OCRWord> line, OCRResults *results);
float myAtoF(const char *text);
int countDiscrepancies (wstring text1, wstring text2);
int countDiscrepancies (vector<wstring> text1List, vector<wstring> text2List);
CGRect computeCapturingRectRight(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool trim = true);
CGRect computeCapturingRectLeft(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool trim = true, bool includeCurrent = true);
CGRect computeTrimmedRect(CGRect largerRect, OCRResults *results);
CGRect computeRightRect(CGRect largerRect, ConnectedComponentList &cpl, int leftMostX);
CGRect computeLeftRect(CGRect largerRect, ConnectedComponentList &cpl, int rightMostX);
bool hasFlatTop(CGRect rect, OCRResults *results);
bool compareRects (CGRect r1, CGRect r2);
CGRect findClosestDigitLookAlike(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool forwardOnly = false);
CGRect computeTallerRect(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrent, OCRResults *results);
bool findDotBetween(OCRRectPtr previous, OCRRectPtr next, SmartPtr<OCRWord> &statsWithoutCurrent, CGRect &dotRect, OCRResults *results);
bool findDashBetween(OCRRectPtr previous, OCRRectPtr next, SmartPtr<OCRWord> &statsWithoutCurrent, CGRect &dotRect, OCRResults *results);
bool checkForSlashPricePerItem (wstring pricePerItemText, wstring &actualPricePerItemText, float &actualPricePerItemValue);
wstring canonicalString(wstring text);
bool OCRAwarePriceCompare(wstring price1Text, wstring price2Text);
float OCRAwareProductNumberCompare(wstring text1, wstring text2);
float totalWidth(SegmentList sl);
float combinedLengths(SegmentList sl, float min = -1, float max = -1);
float largestGap(SegmentList sl, float *gapStart = NULL, float *gapEnd = NULL);
float leftMargin(SegmentList sl);
float rightMargin(SegmentList sl, CGRect rect);
int validateFuzzyResults(vector<wstring> fuzzyText, float index0Score, wstring text, bool *exact);
bool fuzzyCompare(wstring text1, wstring text2);
float OCRAwareTextCompare(wstring text1, wstring text2);
int countMistakes(wstring text1, wstring text2);
bool hasRightRIndent(CGRect rect, SingleLetterTests *st, bool lax=false);
wchar_t testForUppercase(OCRRectPtr r, CGRect &rect, OCRResults *results, bool twoRects, wchar_t assumedChar = '\0', bool lax = false);
}

#endif // _OCRPARSER_UTILS_H_
