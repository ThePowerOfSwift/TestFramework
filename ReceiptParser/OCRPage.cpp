//
//  OCRPage.cpp
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

#define CODE_CPP	1

#include <algorithm>
#include "OCRPage.hh"
#include "OCRUtils.hh"

namespace ocrparser {

// ==================== OCRStats =================

AverageWithCount OCRStats::updateAverageWithValueAdding (AverageWithCount av, float val, bool adding) {
	if (adding) {
		av.sum += val;
		av.count++;
		av.average = av.sum / av.count;
	} else {
		if (av.count > 0) {
			av.sum -= val;
			av.count--;
			if (av.count > 0)
				av.average = av.sum / av.count;
			else
				av.average = 0;
		} else {
			OCRLog("OCRStats::updateAverage: request to remove value [%f] from empty average struct", val);
		}
	}
	return (av);
}

AverageWithCount OCRStats::averageWithout(AverageWithCount av, float val) {
	if (av.count == 0) {
#if DEBUG
		ErrorLog("OCRStats:averageWithout: got average with zero count");
#endif
		return (av);
	}

	AverageWithCount res = av;
	res.sum -= val;
	res.count--;
	if (res.count > 0)
		res.average = res.sum / res.count;
	else {
#if DEBUG
		if (res.sum != 0)
			ErrorLog("OCRStats:averageWithout: removing last value yet sum is non-zero [%f]", av.sum);
#endif
		res.sum = 0;
	}
	return (res);
}

// Determines if we can take into account the space between two letters, or ignore because one of them is special
bool OCRStats::spacingRelevant(SmartPtr<OCRRect> r) {
	return ((!r->previous.isNull()) && (r->previous->ch != ' ') && (r->previous->ch != '\n') && (r->previous->ch != '\t')
			&& !abnormalSpacing(r->ch));
}

bool OCRStats::isItalic() {
    return false;   //No italics on receipts
}

// ==================== OCRRect =================


OCRRect::OCRRect() {

    // Initialization code
	ch = ch2 = '\0';
	confidence = 0;
}

// ==================== OCRLine =================

int OCRLine::count() {
	return (int)columns.size();
}

void OCRLine::addColumn(SmartPtr<OCRColumn> c) {
	SmartPtr<OCRColumn> previousC = columns.size()==0 ? SmartPtr<OCRColumn>() :columns[columns.size()-1];

	// Special test - check for words composed just of a single '|' (or '1' mistaken for a '|') and eliminate
	if (c->averageHeightDigits.average != 0) {
		for (int i=0; i<c->count(); i++) {
			SmartPtr<OCRWord> w = c->words[i];
			if (w->count() == 1) {
				SmartPtr<OCRRect> r = w->letters[0];
				if (   (r->ch == '|')
					|| ((r->ch == '1') && ((r->rect.size.height/c->averageHeightDigits.average > 1.2) || (r->rect.size.height/c->averageHeightDigits.average < 0.73)))
					) {
					// remove that word and replace rect with a tab
					OCRLog("OCRLine::addColumn, removing word with single '|' and replacing with tab");
					r->confidence = 0;
					// TODO: should update word stats
					c->removeWordAtIndex(i);
					page->needToUpdate = true;
					i--;
					continue;
				}
			}
		}
	}

	if (!previousC.isNull()) {
		previousC->next = c;
	}
	c->previous = previousC;
	c->next = SmartPtr<OCRColumn>((OCRColumn *)NULL);
	columns.push_back(c);
}

bool OCRLine::isPresentInLineBeforeRect(wchar_t ch, SmartPtr<OCRRect> r) {
	if (r.isNull())
		return false;
	r = r->previous;
	while ((!r.isNull()) && (r->ch != '\n')) {
		if (r->ch == ch)
			return true;
		r = r->previous;
	}
	return false;
}

bool OCRLine::isPresentInLineAfterRect(wchar_t ch, SmartPtr<OCRRect> r) {
	if (r.isNull())
		return false;
	r = r->next;
	while ((!r.isNull()) && (r->ch != '\n')) {
		if (r->ch == ch)
			return true;
		r = r->next;
	}
	return false;
}

wstring OCRLine::AdHocWord(SmartPtr<OCRRect> r) {
	// Start at beginning of word
	SmartPtr<OCRRect> startR = r;
	while ((!startR->previous.isNull()) && (startR->previous->ch != ' ') && (startR->previous->ch != '\n') && (startR->previous->ch != '\t')) {
		startR = startR->previous;
	}
	if (startR->ch == ' ')
		startR = startR->next;
	if (startR->next.isNull())
		return L"";

	wstring result;
	while ((!startR.isNull()) && (startR->ch != ' ') && (startR->ch != '\n') && (startR->ch != '\t')) {
		result += startR->ch;
		startR = startR->next;
	}

	return (result);
}

// OCRLine

// ==================== OCRColumn =================

 // @implementation OCRColumn

 // @synthesize words;
 // @synthesize next;
 // @synthesize previous;


void OCRColumn::update() {
	needToUpdate = false;
}

int OCRColumn::count() {
	return (int)words.size();
}

void OCRColumn::addWord(SmartPtr<OCRWord> w) {
	SmartPtr<OCRWord> previousW = words.size() == 0 ? OCRWordPtr(): words[words.size()-1];

	if (w->count() < 1) {
		ErrorLog("OCRColumn::addWord adding empty word");
		return;
	}

	needToUpdate = true;

	if (!previousW.isNull()) {
		previousW->next = w;
	}
	w->previous = previousW;
	w->next = SmartPtr<OCRWord>((OCRWord *)NULL);
	words.push_back(w);
}

void OCRColumn::removeWordAtIndex(int i) {
	if ((i<0) || (i>=words.size())) {
		ErrorLog("OCRColumn::removeWordAtIndex, invalid index %d", i);
		return;
	}

	needToUpdate = true;

	//TODO should also recalculate averagaes

	SmartPtr<OCRWord>  w = words[i];
	if (!w->previous.isNull())
		w->previous->next = w->next;
	if (!w->next.isNull())
		w->next->previous = w->previous;
	words.erase(words.begin()+i);
}

 // @end // OCRColumn


// ==================== OCRWord =================
/* OCRWord is used in two ways:
 - as part of a page, with lines, each line containing columns with words
 - or just as a on-demand construct meant to hold stats< and apply post-OCR corrections
 */

 // @implementation OCRWord

 // @synthesize letters;
 // @synthesize needToUpdate;
 // @synthesize next;
 // @synthesize previous;
 // @synthesize line;
 // @synthesize column;
 // @synthesize page;

OCRWord::OCRWord() : line((OCRLine *)NULL),column((OCRColumn *)NULL),page((OCRPage *)NULL) {
    // Initialization code
    setUpdateDirtyBits();
    nLowercase = 0; nDigits = 0;
    nTallLettersInLine = nUppercaseLookingLikeLowercase = 0;
}

OCRWord::OCRWord(int n) :/*bug: letters(n),*/ line((OCRLine *)NULL),column((OCRColumn *)NULL),page((OCRPage *)NULL){

    // Initialization code
	setUpdateDirtyBits();
    nLowercase = 0; nDigits = 0;
    nTallLettersInLine = nUppercaseLookingLikeLowercase = 0;
}

// Allocate a word that is a copy of another word, just for the purpose of stats
OCRWord::OCRWord(SmartPtr<OCRWord>  w) : line((OCRLine *)NULL),column((OCRColumn *)NULL),page((OCRPage *)NULL) {

    // Initialization code
	setUpdateDirtyBits();

	averageWidthWideChars = w->averageWidthWideChars;
	averageWidthNarrowChars = w->averageWidthNarrowChars;
	averageWidthNormalChars = w->averageWidthNormalChars;
	averageWidthLowercase = w->averageWidthLowercase;
	averageWidthNormalLowercase = w->averageWidthNormalLowercase;
	averageWidthTallLowercase = w->averageWidthTallLowercase;
	averageWidthUppercase = w->averageWidthUppercase;
	averageWidthSuperNarrow1 = w->averageWidthSuperNarrow1;
	averageWidthSuperNarrow15 = w->averageWidthSuperNarrow15;
	averageWidthDigits = w->averageWidthDigits;
    averageWidthDigit1 = w->averageWidthDigit1;
	averageWidth = w->averageWidth;
	averageSpacing = w->averageSpacing;
	averageSpacingDigits = w->averageSpacingDigits;
	averageSpacingAfterTall = w->averageSpacingAfterTall;
	averageHeightDigits = w->averageHeightDigits;
    averageHeightDigit1 = w->averageHeightDigit1;
	averageHeightUppercase = w->averageHeightUppercase;
    averageHeightUppercaseNotFirstLetter = w->averageHeightUppercaseNotFirstLetter;
	averageHeightNormalLowercase = w->averageHeightNormalLowercase;
	averageHeightTallLowercase = w->averageHeightTallLowercase;
	averageHeight = w->averageHeight;

}

void OCRWord::updateMyAverageHeightBlendedLettersAndDigits() {
    if (needToUpdateMyAverageHeightBlendedLettersAndDigits) {
        myAverageHeightBlendedLettersAndDigits.count = 0;
        myAverageHeightBlendedLettersAndDigits.sum = 0;
        myAverageHeightBlendedLettersAndDigits.average = 0;
        myAreAllLettersAndDigitsSameHeight = true;
        
        // Empty array of letters - empty stats array is fine
        if (letters.size() == 0) {
            return;
        }
        
        // Update myAverageHeightBlendedLettersAndDigits
        if (averageHeightDigits.count != 0) {
            myAverageHeightBlendedLettersAndDigits.count += averageHeightDigits.count;
            myAverageHeightBlendedLettersAndDigits.sum += averageHeightDigits.sum;
        }
        if (averageHeightNormalLowercase.count != 0) {
            myAverageHeightBlendedLettersAndDigits.count += averageHeightNormalLowercase.count;
            myAverageHeightBlendedLettersAndDigits.sum += averageHeightNormalLowercase.sum;
        }
        if (averageHeightUppercase.count != 0) {
            myAverageHeightBlendedLettersAndDigits.count += averageHeightUppercase.count;
            myAverageHeightBlendedLettersAndDigits.sum += averageHeightUppercase.sum;
        }
        if (myAverageHeightBlendedLettersAndDigits.count != 0) {
            myAverageHeightBlendedLettersAndDigits.average = myAverageHeightBlendedLettersAndDigits.sum / myAverageHeightBlendedLettersAndDigits.count;
        }
        
        // Now iterate through all letters to determine if they are of similar height
        // TODO handle small cap
        myAreAllLettersAndDigitsSameHeight = true; // Until proven otherwise
        SmartPtr<OCRRect> r = letters[0];
        SmartPtr<OCRRect> lastRProcessed((OCRRect *)NULL);
        SmartPtr<OCRRect> currentLastLetter = lastLetter();
        do {
            if (isLetterOrDigit(r->ch)) {
                if (r->rect.size.height > myAverageHeightBlendedLettersAndDigits.average * (1 + OCR_ACCEPTABLE_ERROR)) {
                    myAreAllLettersAndDigitsSameHeight = false;
                    break;
                } else if (r->rect.size.height < myAverageHeightBlendedLettersAndDigits.average * (1 - OCR_ACCEPTABLE_ERROR)) {
                    myAreAllLettersAndDigitsSameHeight = false;
                    break;
                }
            }
            lastRProcessed = r;
            r = r->next;
        } while ((!r.isNull()) && (lastRProcessed != currentLastLetter));
        
        needToUpdateMyAverageHeightBlendedLettersAndDigits = false;
    }
}
    
bool OCRWord::areAllLettersAndDigitsSameHeight() {
    updateMyAverageHeightBlendedLettersAndDigits();
    return myAreAllLettersAndDigitsSameHeight;
}
 
void OCRWord::setUpdateDirtyBits() {
    needToUpdateText = true;
    needToUpdateMyAverageHeightBlendedLettersAndDigits = true;
}
    
void OCRWord::update() {
	myText = L"";
    myText2 = L"";
    
    needToUpdateText = false;
    
	if (letters.size() == 0) {
		return;
	}

	for (size_t  i=0; i<letters.size(); i++) {
		SmartPtr<OCRRect> r = letters[i];
		wchar_t ch = r->ch;
		myText += ch;
	}
    for (size_t  i=0; i<letters.size(); i++) {
		SmartPtr<OCRRect> r = letters[i];
		wchar_t ch = r->ch2;
        if (ch == '\0')
            ch = r->ch;
		myText2 += ch;
	}
}
    
AverageWithCount OCRWord::averageHeightBlendedLettersAndDigits() {
    if (needToUpdateMyAverageHeightBlendedLettersAndDigits) {
        updateMyAverageHeightBlendedLettersAndDigits();
    }
    return myAverageHeightBlendedLettersAndDigits;
}

wstring OCRWord::text() {
	if (needToUpdateText)
		update();
	return (myText);
}

wstring OCRWord::text2() {
	if (needToUpdateText)
		update();
	return (myText2);
}

int OCRWord::count() {
	return ((int)letters.size());
}

SmartPtr<OCRRect> OCRWord::lastLetter() {
	return letters.size() == 0 ? OCRRectPtr() : letters[letters.size()-1];
}

SmartPtr<OCRStats> OCRWord::addStatsToMatch(Match& match) {
	SmartPtr<OCRStats> s(new OCRStats);
	SmartPtr<wstring> matchText = match[matchKeyText].castDown<wstring>();
	SmartPtr<size_t> matchN = match[matchKeyN].castDown<size_t>();

	int n = matchN.isNull() ? 0 : min((int)(*matchN), (int)matchText->length());//YK : TODO: how can this be?!
	SmartPtr<vector<SmartPtr<CGRect> > > rects = match[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();

	for (int i=0; i<n; i++) {
		wchar_t ch = (*matchText)[i];
		SmartPtr<CGRect> r = (*rects)[i];
		if (islower(ch)) {
			if (!isTallLowercase(ch)) {
				s->averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(s->averageHeightNormalLowercase, r->size.height, true);
			}
		}

		if (isLetter(ch) && isUpper(ch)) {
			s->averageHeightUppercase = OCRStats::updateAverageWithValueAdding (s->averageHeightUppercase, r->size.height ,true);
            if ((i>=1) && ((*matchText)[i-1] != ' '))
                s->averageHeightUppercaseNotFirstLetter = OCRStats::updateAverageWithValueAdding (s->averageHeightUppercaseNotFirstLetter, r->size.height ,true); 
		}
	}
	// Add it to match to save time next time it is used in analysis
	match[matchKeyStats] = s.castDown<EmptyType>();

	return s;
}

SmartPtr<OCRStats> OCRWord::averageLowercaseHeightNRects(wstring& matchText, size_t n, RectVectorPtr rectsBase, vector<SmartPtr<CGRect> >::iterator rects){
	SmartPtr<OCRStats> s(new OCRStats());
	n = min(n,matchText.length());
	for (size_t i=0; i<n && rects != rectsBase->end(); i++, ++rects) {
		wchar_t ch = matchText[i];
		SmartPtr<CGRect> r = (*rects);
		if (isLower(ch)) {
			if (!isTallLowercase(ch)) {
				s->averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(s->averageHeightNormalLowercase,r->size.height, true);
			}
		}

		if (isLetter(ch) && isUpper(ch)) {
			s->averageHeightUppercase = OCRStats::updateAverageWithValueAdding(s->averageHeightUppercase, r->size.height, true);
            if ((i>=1) && (matchText[i-1] != ' '))
                s->averageHeightUppercaseNotFirstLetter = OCRStats::updateAverageWithValueAdding(s->averageHeightUppercaseNotFirstLetter, r->size.height, true); 
		}
	}
	return s;
}

float OCRWord::averageUppercaseHeight(Match& match) {
	SmartPtr<OCRStats> s   = match[matchKeyStats].castDown<OCRStats>();
	if (s == NULL)
		s = addStatsToMatch(match);
	return (s->averageHeightUppercase.average);
}

float OCRWord::averageLowercaseHeight(Match& match) {
	SmartPtr<OCRStats> s   = match[matchKeyStats].castDown<OCRStats>();
	if (s == NULL)
		s = addStatsToMatch(match);
	return (s->averageHeightNormalLowercase.average);
}

// Called to either add a new char to the averages or remove a char from the averages
void OCRWord::updateStatsAddOrRemoveLetterAddingSpacingOnly(SmartPtr<OCRRect> currentR, bool adding, bool spacingOnly) {

	if (currentR.isNull())
		return;

	if ((currentR->confidence == 0) || (currentR->flags & FLAGS_OUTLIER))
		// Ignore: manufactured by us AND we decided this rect is unreliable, or already excluded from stats
		return;

	// Update stats
	bool afterTallLowercase = false;
	bool spacingValid = false;
	float spacing = 0;
	bool bothDigits = false; 
	// Add spacing to averages only beetween letters and digits because spacing near special characters is abnormal 
	if (!currentR->previous.isNull() && !(currentR->previous->flags & FLAGS_OUTLIER) && isLetterOrDigit(currentR->previous->ch) && isLetterOrDigit(currentR->ch)
        && ((toLower(currentR->previous->ch) != 'w') || (toLower(currentR->ch) != 'w'))) {
		spacingValid = true;
        if (rectLeft(currentR->rect) < rectLeft(currentR->previous->rect)) {
            WarningLog("updateStatsAddOrRemoveLetter: ERROR letter [%c] is LEFT of previous letter [%c]", currentR->ch, currentR->previous->ch);
        } else {
            float spacing = rectLeft(currentR->rect) - rectRight(currentR->previous->rect) - 1;
            averageSpacing = OCRStats::updateAverageWithValueAdding(averageSpacing, spacing, adding);
            if (adding && (isDigit(currentR->previous->ch) || (currentR->previous->ch == 'O')) && (isDigit(currentR->ch) || (currentR->ch == 'O'))) {
                bothDigits = true;
                averageSpacingDigits = OCRStats::updateAverageWithValueAdding(averageSpacingDigits, spacing, adding);
            }
            // Are we adding a letter after a tall lowercase?
            // Doing the below only when adding because removing the 'a' after 'l' in 'la' is wrong, spacing after 'l' still applies
            if (adding && isTallLowercase(currentR->previous->ch)) {
                afterTallLowercase = true;
            }
            if (afterTallLowercase)
                averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(averageSpacingAfterTall, spacing, adding);
        }
	}
	
	// Are we a tall lowercase and there already is a next rect? Must mean that we are replacing, need to update
	bool tallLowerCaseFollowedByLetter = false;
	float spacingAfter = 0.0;
	if ((!currentR->next.isNull()) && isLetterOrDigit(currentR->next->ch) && isTallLowercase(currentR->ch)) {
		tallLowerCaseFollowedByLetter = true;
		spacingAfter = rectLeft(currentR->next->rect) - rectRight(currentR->rect) - 1;
		averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(averageSpacingAfterTall, spacingAfter, adding);
	}
	if (line != NULL) {
		if (spacingValid) {
			line->averageSpacing = OCRStats::updateAverageWithValueAdding(line->averageSpacing, spacing, adding);
			column->averageSpacing = OCRStats::updateAverageWithValueAdding(column->averageSpacing, spacing, adding);
			page->averageSpacing = OCRStats::updateAverageWithValueAdding(page->averageSpacing, spacing, adding);
			if (afterTallLowercase) {
				line->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(line->averageSpacingAfterTall, spacing, adding);
				column->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(column->averageSpacingAfterTall, spacing, adding);
				page->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(page->averageSpacingAfterTall, spacing, adding);
			}
		}
		if (tallLowerCaseFollowedByLetter) {
			line->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(line->averageSpacingAfterTall, spacingAfter, adding);
			column->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(column->averageSpacingAfterTall, spacingAfter, adding);
			page->averageSpacingAfterTall = OCRStats::updateAverageWithValueAdding(page->averageSpacingAfterTall, spacingAfter, adding);
		}
		if (bothDigits) {
			line->averageSpacingDigits = OCRStats::updateAverageWithValueAdding(line->averageSpacingDigits, spacingAfter, adding);
			column->averageSpacingDigits = OCRStats::updateAverageWithValueAdding(column->averageSpacingDigits, spacingAfter, adding);
			page->averageSpacingDigits = OCRStats::updateAverageWithValueAdding(page->averageSpacingDigits, spacingAfter, adding);
		}
	}

	if (spacingOnly)
		return;

	if (isDigit(currentR->ch)) {
        this->nDigits += (adding? 1:-1);
        this->nTallLettersInLine += (adding? 1:-1);
		if (line != NULL) {
			line->averageHeightDigits = OCRStats::updateAverageWithValueAdding(line->averageHeightDigits, currentR->rect.size.height, adding);
			column->averageHeightDigits = OCRStats::updateAverageWithValueAdding(column->averageHeightDigits, currentR->rect.size.height, adding);
			page->averageHeightDigits = OCRStats::updateAverageWithValueAdding(page->averageHeightDigits, currentR->rect.size.height, adding);
		}
		averageHeightDigits = OCRStats::updateAverageWithValueAdding(averageHeightDigits, currentR->rect.size.height, adding);
		if (currentR->ch != '1') {
			if (line != NULL) {
				line->averageWidthDigits = OCRStats::updateAverageWithValueAdding(line->averageWidthDigits, currentR->rect.size.width, adding);
				column->averageWidthDigits = OCRStats::updateAverageWithValueAdding(column->averageWidthDigits, currentR->rect.size.width, adding);
				page->averageWidthDigits = OCRStats::updateAverageWithValueAdding(page->averageWidthDigits, currentR->rect.size.width, adding);
			}
			averageWidthDigits = OCRStats::updateAverageWithValueAdding(averageWidthDigits, currentR->rect.size.width, adding);
		} else {
            if (line != NULL) {
                line->averageWidthDigit1 = OCRStats::updateAverageWithValueAdding(line->averageWidthDigit1, currentR->rect.size.width, adding);
				column->averageWidthDigit1 = OCRStats::updateAverageWithValueAdding(column->averageWidthDigit1, currentR->rect.size.width, adding);
				page->averageWidthDigit1 = OCRStats::updateAverageWithValueAdding(page->averageWidthDigit1, currentR->rect.size.width, adding);
            }
            averageWidthDigit1 = OCRStats::updateAverageWithValueAdding(averageWidthDigit1, currentR->rect.size.width, adding);
        }
	}

    // Ignoring non-Ascii characters because we don't really know if they should be ignored.
	if (islower(currentR->ch)) {
        nLowercase += (adding? 1:-1);
		// Don't add narrow lowercase characters such as 'l', it would distort the average
		if (!isNarrow(currentR->ch)) {
			if (isTallLowercase(currentR->ch)) {
				if (line != NULL) {
					line->averageWidthTallLowercase = OCRStats::updateAverageWithValueAdding(line->averageWidthTallLowercase, currentR->rect.size.width, adding);
					column->averageWidthTallLowercase = OCRStats::updateAverageWithValueAdding(column->averageWidthTallLowercase, currentR->rect.size.width, adding);
					page->averageWidthTallLowercase = OCRStats::updateAverageWithValueAdding(page->averageWidthTallLowercase, currentR->rect.size.width, adding);
				}
				averageWidthTallLowercase = OCRStats::updateAverageWithValueAdding(averageWidthTallLowercase, currentR->rect.size.width, adding);
			} else {
				if (isNormalWidthLowercase(currentR->ch)) {
					if (line != NULL) {
						line->averageWidthNormalLowercase = OCRStats::updateAverageWithValueAdding(line->averageWidthNormalLowercase, currentR->rect.size.width, adding);
						column->averageWidthNormalLowercase = OCRStats::updateAverageWithValueAdding(column->averageWidthNormalLowercase, currentR->rect.size.width, adding);
						page->averageWidthNormalLowercase = OCRStats::updateAverageWithValueAdding(page->averageWidthNormalLowercase, currentR->rect.size.width, adding);
					}
					averageWidthNormalLowercase = OCRStats::updateAverageWithValueAdding(averageWidthNormalLowercase, currentR->rect.size.width, adding);
				}

				if (line != NULL) {
					line->averageWidthLowercase = OCRStats::updateAverageWithValueAdding(line->averageWidthLowercase, currentR->rect.size.width, adding);
					column->averageWidthLowercase = OCRStats::updateAverageWithValueAdding(column->averageWidthLowercase, currentR->rect.size.width, adding);
					page->averageWidthLowercase = OCRStats::updateAverageWithValueAdding(page->averageWidthLowercase, currentR->rect.size.width, adding);
				}
				averageWidthLowercase = OCRStats::updateAverageWithValueAdding(averageWidthLowercase, currentR->rect.size.width, adding);
			}
		}

		if (!isTallLowercase(currentR->ch)) {
			if (line != NULL) {
				line->averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(line->averageHeightNormalLowercase, currentR->rect.size.height, adding);
				column->averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(column->averageHeightNormalLowercase, currentR->rect.size.height, adding);
				page->averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(page->averageHeightNormalLowercase, currentR->rect.size.height, adding);
			}
			averageHeightNormalLowercase = OCRStats::updateAverageWithValueAdding(averageHeightNormalLowercase, currentR->rect.size.height, adding);
		} else if (isTallLowercase(currentR->ch) 
                   // Below is meant to exclude e.g. wide-looking 't's with height=width
                   && ((currentR->rect.size.height > currentR->rect.size.width * 1.5))) {
            this->nTallLettersInLine += (adding? 1:-1);
			if (line != NULL) {
				line->averageHeightTallLowercase = OCRStats::updateAverageWithValueAdding(line->averageHeightTallLowercase, currentR->rect.size.height, adding);
				column->averageHeightTallLowercase = OCRStats::updateAverageWithValueAdding(column->averageHeightTallLowercase, currentR->rect.size.height, adding);
				page->averageHeightTallLowercase = OCRStats::updateAverageWithValueAdding(page->averageHeightTallLowercase, currentR->rect.size.height, adding);
			}
			averageHeightTallLowercase = OCRStats::updateAverageWithValueAdding(averageHeightTallLowercase, currentR->rect.size.height, adding);
		}
	}

	if (isLetter(currentR->ch) && isUpper(currentR->ch)) {
        this->nTallLettersInLine += (adding? 1:-1);
        if (looksSameAsLowercase(currentR->ch))
            this->nUppercaseLookingLikeLowercase += (adding? 1:-1);
		if (line != NULL) {
			line->averageHeightUppercase = OCRStats::updateAverageWithValueAdding(line->averageHeightUppercase, currentR->rect.size.height, adding);
			column->averageHeightUppercase = OCRStats::updateAverageWithValueAdding(column->averageHeightUppercase, currentR->rect.size.height, adding);
			page->averageHeightUppercase = OCRStats::updateAverageWithValueAdding(page->averageHeightUppercase, currentR->rect.size.height, adding);
            if ((!currentR->previous.isNull()) && (currentR->previous->ch != ' ')) {
                line->averageHeightUppercaseNotFirstLetter = OCRStats::updateAverageWithValueAdding(line->averageHeightUppercaseNotFirstLetter, currentR->rect.size.height, adding);
                column->averageHeightUppercaseNotFirstLetter = OCRStats::updateAverageWithValueAdding(column->averageHeightUppercaseNotFirstLetter, currentR->rect.size.height, adding);
                page->averageHeightUppercaseNotFirstLetter = OCRStats::updateAverageWithValueAdding(page->averageHeightUppercaseNotFirstLetter, currentR->rect.size.height, adding);
            }
		}
		averageHeightUppercase = OCRStats::updateAverageWithValueAdding(averageHeightUppercase, currentR->rect.size.height, adding);

		if (!isNarrow(currentR->ch)) {
			if (line != NULL) {
				line->averageWidthUppercase = OCRStats::updateAverageWithValueAdding(line->averageWidthUppercase, currentR->rect.size.width, adding);
				column->averageWidthUppercase = OCRStats::updateAverageWithValueAdding(column->averageWidthUppercase, currentR->rect.size.width, adding);
				page->averageWidthUppercase = OCRStats::updateAverageWithValueAdding(page->averageWidthUppercase, currentR->rect.size.width, adding);
			}
			averageWidthUppercase = OCRStats::updateAverageWithValueAdding(averageWidthUppercase, currentR->rect.size.width, adding);
		}
	}

	if (isNormalChar(currentR->ch)) {
		if (line != NULL) {
			line->averageWidthNormalChars = OCRStats::updateAverageWithValueAdding(line->averageWidthNormalChars, currentR->rect.size.width, adding);
			column->averageWidthNormalChars = OCRStats::updateAverageWithValueAdding(column->averageWidthNormalChars, currentR->rect.size.width, adding);
			page->averageWidthNormalChars = OCRStats::updateAverageWithValueAdding(page->averageWidthNormalChars, currentR->rect.size.width, adding);
		}
		averageWidthNormalChars = OCRStats::updateAverageWithValueAdding(averageWidthNormalChars, currentR->rect.size.width, adding);
	}
	// Normal "wide" chars, like 'A','9' (not like '|' or '.')
	if ((isLetter(currentR->ch) || isDigit(currentR->ch)) && !isNarrow(currentR->ch)) {
		if (line != NULL) {
			line->averageWidthWideChars = OCRStats::updateAverageWithValueAdding(line->averageWidthWideChars, currentR->rect.size.width, adding);
			column->averageWidthWideChars = OCRStats::updateAverageWithValueAdding(column->averageWidthWideChars, currentR->rect.size.width, adding);
			page->averageWidthWideChars = OCRStats::updateAverageWithValueAdding(page->averageWidthWideChars, currentR->rect.size.width, adding);
		}
		averageWidthWideChars = OCRStats::updateAverageWithValueAdding(averageWidthWideChars, currentR->rect.size.width, adding);
	}

	if (isSuperNarrow1(currentR->ch)) {
		if (line != NULL) {
			line->averageWidthSuperNarrow1 = OCRStats::updateAverageWithValueAdding(line->averageWidthSuperNarrow1, currentR->rect.size.width, adding);
			column->averageWidthSuperNarrow1 = OCRStats::updateAverageWithValueAdding(column->averageWidthSuperNarrow1, currentR->rect.size.width, adding);
			page->averageWidthSuperNarrow1 = OCRStats::updateAverageWithValueAdding(page->averageWidthSuperNarrow1, currentR->rect.size.width, adding);
		}
		averageWidthSuperNarrow1 = OCRStats::updateAverageWithValueAdding(averageWidthSuperNarrow1, currentR->rect.size.width, adding);
	}

	if (isNormalChar(currentR->ch) && isNarrow(currentR->ch)) {
		float width;
		if (isSuperNarrow(currentR->ch))
			width = currentR->rect.size.width*2;
		else
			width = currentR->rect.size.width;
		if (line != NULL) {
			line->averageWidthNarrowChars = OCRStats::updateAverageWithValueAdding(line->averageWidthNarrowChars, width, adding);
			column->averageWidthNarrowChars = OCRStats::updateAverageWithValueAdding(column->averageWidthNarrowChars, width, adding);
			page->averageWidthNarrowChars = OCRStats::updateAverageWithValueAdding(page->averageWidthNarrowChars, width, adding);
		}
		averageWidthNarrowChars = OCRStats::updateAverageWithValueAdding(averageWidthNarrowChars, width, adding);
	}

    if (currentR->ch != ' ') {
        if (line != NULL) {
            line->averageWidth = OCRStats::updateAverageWithValueAdding(line->averageWidth, currentR->rect.size.width, adding);
            column->averageWidth = OCRStats::updateAverageWithValueAdding(column->averageWidth, currentR->rect.size.width, adding);
            page->averageWidth = OCRStats::updateAverageWithValueAdding(page->averageWidth, currentR->rect.size.width, adding);
        }
        averageWidth = OCRStats::updateAverageWithValueAdding(averageWidth, currentR->rect.size.width, adding);
        
        if (line != NULL) {
            line->averageHeight = OCRStats::updateAverageWithValueAdding(line->averageHeight, currentR->rect.size.height, adding);
            column->averageHeight = OCRStats::updateAverageWithValueAdding(column->averageHeight, currentR->rect.size.height, adding);
            page->averageHeight = OCRStats::updateAverageWithValueAdding(page->averageHeight, currentR->rect.size.height, adding);
        }
        averageHeight = OCRStats::updateAverageWithValueAdding(averageHeight, currentR->rect.size.height, adding);
    }
}

void OCRWord::addRectAfterRect(SmartPtr<OCRRect> currentR, SmartPtr<OCRRect> previousR) {

    setUpdateDirtyBits();

	// Confidence == -1 means do not update stats (special / odd characters)
    // When adding a space we DO need to adjust stats as regards spacing: previous and next no longer should contribute to the averageSpacing value
	if (((currentR->confidence != -1) || (currentR->ch == ' '))&& (previousR->next != NULL)) {
		// We are inserting a new rect, undo the stats about the next rect as regards spacing
		updateStatsAddOrRemoveLetterAddingSpacingOnly(previousR->next, false, true);
	}

	// Important: set the next/previous of rects in words only if no context
	// Otherwise, these rects are already connected by virtue of being in the page rect array
	if (page == NULL) {
		currentR->next = previousR->next;
		if (currentR->next != NULL)
			currentR->next->previous = currentR;
		currentR->previous = previousR;
		previousR->next = currentR;
		// Important to add to array so it is properly retained by it
		letters.insert(std::find(letters.begin(), letters.end(), previousR)+1 , currentR);
	}

	// Update stats
	if (currentR->confidence != -1) {
		updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR, true, false);
		// Update stats with space between new rect and next rect
		if (currentR->next != NULL)
			updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR->next, true, true);
	}

	currentR->word = this;
} // addRect:AfterRect:

void OCRWord::addRect(SmartPtr<OCRRect> currentR ){

	setUpdateDirtyBits();

	// Important: set the next/previous of rects in words only if no context
	// Otherwise, these rects are already connected by virtue of being in the page rect array
	if (page == NULL) {
		currentR->next = SmartPtr<OCRRect>((OCRRect *)NULL);
		SmartPtr<OCRRect> prev = letters.size() == 0? OCRRectPtr() : letters[letters.size()-1];
		currentR->previous = prev;
		if (!prev.isNull()) prev->next = currentR;
	}

	// Update stats

	updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR, true, false);

	currentR->word = this;

	letters.push_back(currentR);
} // addRect

SmartPtr<OCRRect> OCRWord::addLetterWithRectConfidenceAfterRect(wchar_t ch, CGRect rect,float conf,SmartPtr<OCRRect> previousR) {
	SmartPtr<OCRRect> r = SmartPtr<OCRRect>(new OCRRect());
	r->rect = rect;
	r->ch = ch;
	r->confidence = conf;
	addRectAfterRect(r, previousR);
	if (conf == -1) {
		// -1 is a convention for odd / special chars we want to exclude from stats
		// Now just protect it from changes
		r->confidence = 0;
	}
    r->flags = r->flags2 = r->flags3 = r->flags4 = r->flags5 = r->tested = r->status = 0;
    r->flags6 = r->flags7 = 0;
	setUpdateDirtyBits();	// addRect does it anyway but just in case

	return r;
}

void OCRWord::addLetterWithRectConfidence(wchar_t ch, CGRect rect, float conf) {
	SmartPtr<OCRRect> r = SmartPtr<OCRRect>(new OCRRect());
	r->rect = rect;
	r->ch = ch;
	r->confidence = conf;
    r->flags = r->flags2 = r->flags3 = r->flags4 = r->flags5 = r->tested = r->status = 0;
    r->flags6 = r->flags7 = 0;
	addRect(r);
	setUpdateDirtyBits();	// addRect does it anyway but just in case
}

void OCRWord::updateLetterWithNewCharAndNewRect(SmartPtr<OCRRect> currentR, wchar_t newChar, CGRect newRect) {

	updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR, false, false);
	bool isTallLowercaseOld = isTallLowercase(currentR->ch);
	bool isTallLowercaseNew = isTallLowercase(newChar);
	if ((isTallLowercaseOld && !isTallLowercaseNew) || (isTallLowercaseOld && !isTallLowercaseNew)) {
		updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR->next, false, true);
	}
	currentR->ch = newChar;
	currentR->rect = newRect;
	updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR, true, false);
	if ((isTallLowercaseOld && !isTallLowercaseNew) || (isTallLowercaseOld && !isTallLowercaseNew)) {
		updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR->next, true, true);
	}
	if(!page.isNull()) page->needToUpdate = true;
	setUpdateDirtyBits();
}

void OCRWord::removeLetter(SmartPtr<OCRRect> currentR) {
	// Adjust averages to account for the removal of this character
    // Only if we didn't previously excluded the stats for this one on the basis of it being an outlier (in which case we set its confidence to 1000+
    if (currentR->confidence < 1000) {
        // Cancel spacing stats between rect we are about to remove and the one following it
        if (currentR->next != NULL)
            updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR->next, false, true);
        // Cancel stats for rect we are about to remove
        updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR, false, false);
    }
    //bool addToSpacingStatsLater = (currentR->previous != NULL) && (currentR->next != NULL);
	// Stich previous and next
	if (currentR->previous != NULL)
		currentR->previous->next = currentR->next;
	if (currentR->next != NULL)
		currentR->next->previous = currentR->previous;
	// Remove rect from our array of letters
        letters.erase(std::find(letters.begin(), letters.end(), currentR));
	if (page != NULL) {
		// Also remove from the page structure
		page->removeRectStich(currentR, false);
	}
    //if (addToSpacingStatsLater)
    //    updateStatsAddOrRemoveLetterAddingSpacingOnly(currentR->next, false, true);
	setUpdateDirtyBits();
}

void OCRWord::adjustRectsInMatch(Match& m) {

	SmartPtr<vector<SmartPtr<CGRect> > > newRects(new vector<SmartPtr<CGRect> >(letters.size()));

	for (size_t  i=0; i < letters.size(); ++i) {
		(*newRects)[i] = SmartPtr<CGRect>(new CGRect(letters[i]->rect) );
	}

	// Set new array in match
        m[matchKeyRects] = newRects.castDown<EmptyType>();
        m[matchKeyN] = SmartPtr<size_t>(new size_t(letters.size())).castDown<EmptyType>();
}

// OCRWord


// ==================== OCRPage ===================
                                                /* we cant set capacity! doing this will set size instead*/
OCRPage::OCRPage(int n) : stats(new OCRStats())/*, rects(n)*/  {

    // Initialization code
	needToUpdate = true;
}


#if DEBUG
void OCRPage::checkRectsConsistency() {
// Commenting out - unused and very time consuming when processing lots of bogus characters as in https://www.pivotaltracker.com/n/projects/1118860/stories/96697234
//	if (rects.size() == 0)
//		return;
//	for (size_t  i=0; i<rects.size(); i++) {
//		SmartPtr<OCRRect> r = rects[i];
//		SmartPtr<OCRRect> head = rects[0];
//        bool found = false;
//        int numIterations = 0;
//		while (head != NULL) {
//			if (head == r) {
//				// Found it, great
//                found = true;
//				break;
//			}
//            numIterations++;
//			head = head->next;
//		}
//		if (!found) {
//			ErrorLog("OCRPage::checkRectsConsistency: unconnected rect at index %d [out of size=%d], char=[%C 0x%X], gave up after %d iterations", (unsigned short)i, (int)rects.size(), r->ch, (unsigned short)r->ch, numIterations);
//		} else if (numIterations > ((int)rects.size() + 1)) {
//            ErrorLog("OCRPage::checkRectsConsistency: found rect at index %d [out of size=%d], char=[%C 0x%X] but required too many iterations [%d]", (unsigned short)i, (int)rects.size(), r->ch, (unsigned short)r->ch, numIterations);
//        }
//	}
}
#endif

void OCRPage::update() {
	if (myText != NULL)
		myText = SmartPtr<wstring>((wstring *)NULL);
    if (myText2 != NULL)
		myText2 = SmartPtr<wstring>((wstring *)NULL);
	if (myCoordinates != NULL)
		myCoordinates.setNull();

	needToUpdate = false;

	if (rects.size() == 0) {
		myText = SmartPtr<wstring>(new wstring(L""));
		myCoordinates.setNull();
		return;
	}

	SmartPtr<OCRRect> r;

	myText = SmartPtr<wstring>(new wstring(L""));
	for (size_t i=0; i<rects.size(); i++) {
		r = rects[i];
		wchar_t ch = r->ch;
		(*myText) += ch;
	}
    myText2 = SmartPtr<wstring>(new wstring(L""));
	for (size_t i=0; i<rects.size(); i++) {
		r = rects[i];
		wchar_t ch = r->ch2;
        if (ch == '\0')
            ch = r->ch;
		(*myText2) += ch;
	}

	// Update coordinates
	myCoordinates = SmartPtr<vector<SmartPtr<CGRect> > >(new vector<SmartPtr<CGRect> >(rects.size()));
	r = rects[0];
	int j=0;
	while (!r.isNull()) {
		(*myCoordinates)[j++] = SmartPtr<CGRect>(new CGRect(r->rect));
		r = r->next;
	}
}

const wstring& OCRPage::text() {
	if (needToUpdate) {
		update();
	}
	return *myText;
}

const wstring& OCRPage::text2() {
	if (needToUpdate) {
		update();
	}
	return *myText2;
}

bool OCRPage::isEmpty() {
	return (rects.size() == 0);
}

SmartPtr<vector<SmartPtr<CGRect> > > OCRPage::coordinates() {
	if (needToUpdate) {
		update();
	}
	return (myCoordinates);
}

// Add a rect instance at the end of the rects array
void OCRPage::addRect(SmartPtr<OCRRect> r) {

	needToUpdate = true;

	SmartPtr<OCRRect> previousR = rects.size() == 0 ? SmartPtr<OCRRect>():rects[rects.size()-1];

	if (!previousR.isNull()) {
		previousR->next = r;
	}
	r->previous = previousR;
	r->next.setNull();
	rects.push_back(r);
#if DEBUG
	//checkRectsConsistency();
#endif
}

// i=0 is accepted (inserts at first position), even if array is empty
// rects->count() is also accepted (inserts at the end)
SmartPtr<OCRRect> OCRPage::insertRectAtIndex(SmartPtr<OCRRect> r, int i ){

	if ((i<0) || ((i != 0) && (i>rects.size()))) {
		ErrorLog("OCRPage::removeRectAtIndex, invalid index %d", i);
		return SmartPtr<OCRRect>((OCRRect *)NULL);
	}

	needToUpdate = true;

	if (i == rects.size()) {
		// Adding at the end
		r->previous = rects.size() == 0 ? OCRRectPtr() : rects[rects.size()-1];
		r->next.setNull();
		rects.push_back(r);
		return r;
	}

	// current is the existing element being pushed to the right
	// previous is the previous rect, whose 'next' will now need to point to the newly inserted element
	SmartPtr<OCRRect> currentR, previousR;
	if (rects.size() == 0) {
		currentR.setNull();
		previousR.setNull();
	} else {
		currentR = rects[i];
		previousR = currentR->previous;
		if (!previousR.isNull())
			previousR->next = r;
	}

	// Set next/previous of new element
	if (!currentR.isNull()) {
		// r->previous need to point to the element currentR->previous was pointing to (possibly NULL)
		r->previous = currentR->previous;
	} else
		// must be first position
		r->previous.setNull();
	// next of new element just point to currentR (even if currentR is NULL)
	r->next = currentR;

	// Now adjust the element being pushed right to have its previous point to the new element
	if (!currentR.isNull())
		currentR->previous = r;

	rects.insert(rects.begin() + i, r);

#if DEBUG
	checkRectsConsistency();
#endif
	return r;
}

void OCRPage::insertRectAfterRect(SmartPtr<OCRRect> r, SmartPtr<OCRRect> insertionRect) {

	DebugLog("OCRPage::insertRect:AfterRect: about to insert %lc", r->ch);
	
	needToUpdate = true;

	int i = (int)(std::find(rects.begin(), rects.end(), insertionRect) - rects.begin());

	if (i == rects.size()) {
		ErrorLog("OCRPage::insertRect:AfterRect: ERROR, rect [char=%c] not found in page rects", r->ch);
		return;
	}

	insertRectAtIndex(r, i+1);
} // insertRect:AfterRect:

SmartPtr<OCRRect> OCRPage::insertLetterWithRectConfidenceAfterLetter(wchar_t ch, CGRect r,  float conf, SmartPtr<OCRRect> existingR) {
	SmartPtr<OCRRect> newR(new OCRRect());
	newR->ch = ch;
	newR->rect = r;
	newR->confidence = conf;
	insertRectAfterRect(newR, existingR);
	return newR;
}

// Remove a rect instance
void OCRPage::removeRectAtIndex(int i) {

	if ((i<0) || (i>=rects.size())) {
		ErrorLog("OCRPage::removeRectAtIndex, invalid index %d", i);
		return;
	}

	needToUpdate = true;

	SmartPtr<OCRRect> r = rects[i];
//    SmartPtr<OCRRect> rPrevious; bool setRPrevious = false;
//    SmartPtr<OCRRect> rNext; bool setRNext = false;
//    if (r->previous != NULL) {
//		rPrevious = r->previous;
//        setRPrevious = true;
//    }
//	if (r->next != NULL) {
//		rNext = r->next;
//        setRNext = true;
//    }
// PQTODO - looks like the below bombs sometimes, perhaps because the first assignment detaches r from the array and thus it gets released? Doesn't make sense, we only detach it in terms of the linked list, it's still in the array ...
	if (r->previous != NULL)
		r->previous->next = r->next;
	if (r->next != NULL)
		r->next->previous = r->previous;

//	if (setRPrevious) {
//        rPrevious->next = rNext;
//    }
//	if (setRNext) {
//		rNext->previous = rPrevious;
//    }
	rects.erase(rects.begin() + i);

#if DEBUG
	checkRectsConsistency();
#endif
}

void OCRPage::updateRectWithRect(SmartPtr<OCRRect> existingR, SmartPtr<OCRRect> newR) {
	updateRectwithCharRectConfidence(existingR, newR->ch, newR->rect, newR->confidence);
}

// Remove a rect instance
void OCRPage::removeRectStich(SmartPtr<OCRRect> r, bool stich) {

	needToUpdate = true;

	if (stich) {
		if (r->previous != NULL)
			r->previous->next = r->next;
		if (r->next != NULL)
			r->next->previous = r->previous;
	}
	rects.erase(std::find(rects.begin(),rects.end(),r));

#if DEBUG
	checkRectsConsistency();
#endif
}

// Updates an existing rect, without changing its position in the array
void OCRPage::updateRectwithCharRectConfidence(SmartPtr<OCRRect> r, wchar_t ch, CGRect rect,float c) {
	r->ch = ch;
	r->rect = rect;
	r->confidence = c;
	needToUpdate = true;
}

void OCRPage::addLine(SmartPtr<OCRLine> l) {
	SmartPtr<OCRLine> previousL = lines.size()==0? SmartPtr<OCRLine>():lines[lines.size()-1];

	if (!previousL.isNull()) {
		previousL->next = l;
	}
	l->previous = previousL;
	l->next.setNull();
        lines.push_back(l);
}

 // @end // OCRPage
}
