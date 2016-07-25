#ifndef _OCR_PAGE_H_
#define _OCR_PAGE_H_
//
//  OCRPage.hh
//
//  Copyright 2015 Patrick Questembert. All rights reserved.
//

#include "ocr.hh"
#include <vector>
#include <string>

using namespace std;

namespace ocrparser {

class OCRPage;
class OCRRect;

class OCRStats {
public:

	AverageWithCount averageWidthWideChars;
	AverageWithCount averageWidthNarrowChars;
	AverageWithCount averageWidthNormalChars;
	AverageWithCount averageWidthLowercase;
	AverageWithCount averageWidthNormalLowercase;
	AverageWithCount averageWidthTallLowercase;
	AverageWithCount averageWidthUppercase;
	AverageWithCount averageWidthSuperNarrow1;
	AverageWithCount averageWidthSuperNarrow15;
	AverageWithCount averageWidthDigits;
    AverageWithCount averageWidthDigit1;
	AverageWithCount averageWidth;
	AverageWithCount averageSpacing;
	AverageWithCount averageSpacingDigits;
	AverageWithCount averageSpacingAfterTall;
	AverageWithCount averageHeightDigits;
    AverageWithCount averageHeightDigit1;
	AverageWithCount averageHeightUppercase;
    AverageWithCount averageHeightUppercaseNotFirstLetter;
	AverageWithCount averageHeightNormalLowercase;
	AverageWithCount averageHeightTallLowercase;
	AverageWithCount averageHeight;

	static AverageWithCount updateAverageWithValueAdding (AverageWithCount av, float val, bool adding);
	static AverageWithCount averageWithout(AverageWithCount av, float val);

	static bool spacingRelevant(SmartPtr<OCRRect> r);
	bool isItalic();

};

typedef SmartPtr<OCRStats> OCRStatsPtr;

class OCRLine;
class OCRColumn;
class OCRWord;
class OCRPage;

	
#if USE_UNICODE
typedef wchar_t OCR_CHAR_TYPE;
#else
typedef unsigned char OCR_CHAR_TYPE;
#endif
	

class OCRRect {
public:
	OCR_CHAR_TYPE ch;
    OCR_CHAR_TYPE ch2;  // Alternate ch (when unsure about an OCR correction)
	CGRect rect;
	float confidence;
	int nSequence;
	int indexInSequence;
    // Flags below are use by the OCR analysis code to indicate which tests were run on the characters etc
    unsigned long flags;
    unsigned long flags2;
    unsigned long flags3;
    unsigned long flags4;
    unsigned long flags5;
    unsigned long flags6;
    unsigned long flags7;
    unsigned short tested;
    unsigned short status;

	OCRWord*  word; // OCRWord structure containing that character. Note: in spite of the name, OCRWord currently includes a whole line, including spaces.

	SmartPtr<OCRRect> next, previous;
	OCRRect();
};

typedef SmartPtr<OCRRect> OCRRectPtr;

// Note: in spite of the name, OCRWord currently includes a whole line, including spaces.
class OCRWord : public OCRStats {
public:
	vector<SmartPtr<OCRRect> > letters;
	int count();
	SmartPtr<OCRWord> next, previous;
	bool needToUpdateText;
	SmartPtr<OCRLine> line;
	SmartPtr<OCRColumn> column;
	SmartPtr<OCRPage> page;
        OCRWord();
        OCRWord(int n);
        OCRWord(SmartPtr<OCRWord> w);

        SmartPtr<OCRRect> lastLetter();
        static SmartPtr<OCRStats> averageLowercaseHeightNRects(wstring& matchText, size_t n, RectVectorPtr rectsBase, RectVector::iterator rects );
    int nLowercase, nDigits;
    int nTallLettersInLine, nUppercaseLookingLikeLowercase;
private:

	void update();
    void updateMyAverageHeightBlendedLettersAndDigits();
	wstring myText, myText2;
    AverageWithCount myAverageHeightBlendedLettersAndDigits;
    bool myAreAllLettersAndDigitsSameHeight;
    bool needToUpdateMyAverageHeightBlendedLettersAndDigits;
    
    static SmartPtr<OCRStats> addStatsToMatch(Match& match);

    void addRectAfterRect(SmartPtr<OCRRect> currentR, SmartPtr<OCRRect> previousR);

public:

// Dynamically generated
wstring text();
wstring text2();
AverageWithCount averageHeightBlendedLettersAndDigits(); 

bool areAllLettersAndDigitsSameHeight();

void setUpdateDirtyBits();
OCRWord(const OCRWord& w);
void addRect(SmartPtr<OCRRect> r);
void removeLetter(SmartPtr<OCRRect> currentR);
void updateLetterWithNewCharAndNewRect(SmartPtr<OCRRect> currentR, wchar_t newChar, CGRect newRect);
void updateStatsAddOrRemoveLetterAddingSpacingOnly(SmartPtr<OCRRect> currentR, bool adding, bool spacingOnly);
static float averageUppercaseHeight(Match& bmatch);
static float averageLowercaseHeight(Match& match);
static OCRStats * averageLowercaseHeightNRects(wstring& matchText, int n, SmartPtr<CGRect> rects);
void addLetterWithRectConfidence(wchar_t ch, CGRect r, float conf);
SmartPtr<OCRRect> addLetterWithRectConfidenceAfterRect(wchar_t ch, CGRect r, float conf, SmartPtr<OCRRect> rect);
void adjustRectsInMatch(Match& m);

}; // OCRWord

typedef SmartPtr<OCRWord> OCRWordPtr;


class OCRColumn : public OCRStats {
public:
	vector<SmartPtr<OCRWord> > words;
	SmartPtr<OCRColumn> next;
	SmartPtr<OCRColumn> previous;
	bool needToUpdate;

	int count();
	void addWord(SmartPtr<OCRWord> w);
	void removeWordAtIndex(int i);
private:
        void update();

}; // OCRColumn

class OCRLine : public OCRStats {
public:
//  	OCRLine();
	vector<SmartPtr<OCRColumn> > columns;
	SmartPtr<OCRPage> page;
	SmartPtr<OCRLine> next;
	SmartPtr<OCRLine> previous;

	int count();

void addColumn(SmartPtr<OCRColumn> c);
static bool isPresentInLineBeforeRect(wchar_t ch, SmartPtr<OCRRect> r);
static bool isPresentInLineAfterRect(wchar_t ch, SmartPtr<OCRRect> r);
static wstring AdHocWord(SmartPtr<OCRRect> r);

}; // OCRLine

class OCRPage : public OCRStats {
public:
	SmartPtr<OCRStats> stats;
	int count;
	vector<SmartPtr<OCRRect> > rects;
	vector<SmartPtr<OCRLine> > lines;
	SmartPtr<OCRLine> previous, next;
	bool needToUpdate;
	SmartPtr<wstring> myText;
    SmartPtr<wstring> myText2;
	SmartPtr<vector<SmartPtr<CGRect> > > myCoordinates;


// Properties
bool isEmpty();
const wstring& text();
const wstring& text2();

// Methods
OCRPage(int n);
#if DEBUG
void checkRectsConsistency();
#endif
void addRect(SmartPtr<OCRRect> r);
SmartPtr<OCRRect>  insertRectAtIndex(SmartPtr<OCRRect> r, int i);
void insertRectAfterRect(SmartPtr<OCRRect>  r, SmartPtr<OCRRect>  insertionRect);
SmartPtr<OCRRect>  insertLetterWithRectConfidenceAfterLetter(wchar_t ch, CGRect r, float conf, SmartPtr<OCRRect>  existingR);
void removeRectAtIndex(int i);
void removeRectStich(SmartPtr<OCRRect> r, bool stich);
void updateRectwithCharRectConfidence(SmartPtr<OCRRect> r, wchar_t ch, CGRect rect, float c);
void updateRectWithRect(SmartPtr<OCRRect> existingR, SmartPtr<OCRRect>  newR);
void addLine(SmartPtr<OCRLine> line);

SmartPtr<vector<SmartPtr<CGRect> > > coordinates();

private:

void update();

}; // OCRPage

}
#endif
