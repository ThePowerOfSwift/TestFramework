//
//  RegexUtils.hh
//  Windfall
//
//  Created by Patrick Questembert on 4/13/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#ifndef __Windfall__RegexUtils__
#define __Windfall__RegexUtils__

#include <stdio.h>
#include <set>
#include "OCRClass.hh"

#define PCRE_FLAGS 0

/* When changing the below need to also update:
   1. digitsLookalikesUTF8 in OCRUtils.cpp
   2. The digitForExtendedLookalike function in OCRUtils.cpp (right below digitsLookalikesUTF8)
   3. PRICE_DOLLAR_REGEX (see below). NOTE: it's OK not to include all variants in prices, to be conservative (since products are longer, i.e. harder to make a mistake by assuming say 'J' is a '0')
   4. RegexUtilsAutoCorrect (where we correct lookalikes back into the expected digit)
 */
#define DIGIT "(?:\\d|\\(\\)|i|I|\\!|B|D|U|O|o|@|l|G|Z|s|S|\\$|J|\\/|Q)"
#define DIGIT_NODOLLAR "(?:\\d|\\(\\)|i|I|\\!|B|D|U|O|o|@|l|G|Z|s|S|J|\\/|Q)"

namespace ocrparser {

MatchVectorPtr RegexUtilsParseFragmentIntoTypes(wstring& inputFragment, wstring& inputFragment2, OCRResults *curResults, MatchPtr m = MatchPtr());
OCRElementType RegexUtilsSearchForType(wstring& input, OCRResults* curResults, const std::set<OCRElementType>& excludeList, MatchVectorPtr* elements);

OCRElementType testForProductNumber(wstring& input, OCRResults *curResults);
bool testForPhoneNumber(wstring& input, OCRResults *curResults);
bool testForRegexPattern(wstring& input, const char *regex, OCRElementType tp, OCRResults *curResults);
bool testForCoupon(wstring& input, OCRResults *curResults);
bool testForSubtotal(wstring& input, OCRResults *curResults);
bool testForTotal(wstring& input, OCRResults *curResults);
bool testForTax(wstring& input, OCRResults *curResults);
bool testForBal(wstring& input, OCRResults *curResults);
OCRElementType testForPrice(wstring& input, OCRResults *curResults);
bool RegexUtilsAutoCorrect(wstring& text, OCRResults *curResults, OCRElementType tp, OCRElementType subType, bool specificOnly, bool wholeFragment);
void RegexUtilsRemoveLeadingBlanks(wstring& text);
int RegexUtilsStripTrailing(wstring& text, const wstring& set);
int RegexUtilsStripLeading(wstring& text, const wstring& set);
bool RegexUtilsValidateUPCCode(wstring &text);
wchar_t RegexUtilsDetermineMissingUPCDigits(wstring &text, int indexMissingDigit);
wchar_t RegexUtilsDetermineMissingUPCDigits(OCRRectPtr r);
wstring RegexUtilsBeautifyProductDescription(const wstring& inputText, OCRResults *results);

} // end namespace ocrparser

#endif /* defined(__Windfall__RegexUtils__) */
