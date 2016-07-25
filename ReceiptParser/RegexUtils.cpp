//
//  RegexUtils.cpp
//  Windfall
//
//  Created by Patrick Questembert on 4/13/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#define CODE_CPP 1

#include "common.hh"
#include "ocr.hh"
#include "RegexUtils.hh"
#include "OCRClass.hh"
#include "OCRUtils.hh"

#include "PcreMiniCpp.h"

#include <string.h>
#include <sstream>
#include <set>

#if OCR_MULTITHREAD
#define CACHED_REGEXES 0 // IMPORTANT: setting this flag to 1 makes the C++ OCR NOT thread-safe because it seems compiled regexes have state data!
#else
#define CACHED_REGEXES 1    // When not multithreading OK to use the cached regexes optimization
#endif

using namespace std;

namespace ocrparser {

// 10/6/2015 accept quotes/dashes/dots glued before & after
#define PRODUCT_NUMBER_REGEX "(?:(?:^| )(?: )*([.\\-*',~:\\/]*)" DIGIT "{[%d]}%charsToIgnoreAfterProduct%markerAndPriceOnNextLine([.\\-*',~:\\/]*)(?: |$))"
#define PRODUCT_EXTERNAL_NUMBER_REGEX "(?:(?:^| )(?: )*([.\\-*',~:\\/]*)[%externalproductregex]([.\\-*',~:\\/]*)(?: |$))"

#define PRICE_MIN_LEN  4
/* Start of item or space (so we catch prices glued to product description with a space between them)
   Allow any number of spaces
   Optional one or more character(s) returned by the OCR for the dollar
   One or more digits (up to 5 - 99999, assuming no comma)
   Decimal point (anything that's not a letter or digit, to allow for mistakes)
   Two digits (cents)
   Allow any number of spaces
*/
// PQTODO allow $1,000+ prices with commas
/*
    - (?: |$) : end of line or just a space (allows catching price in "3 @ $0.50 ea"
    - Note: we allow multiple non-space characters glued to the left of prices in the case of $-prefixed prices because Blink sometimes returns multiple chars for a single $. We can do better once we determine where prices live and apply this regex only in that area
*/

// To test at http://regexr.com/ use this:
// (?:(?:^| )[^\s]*(?:\d|\(\)|i|I|\!|O|o|@|l|G|Z|s|S){1,5}(?:[^0-9a-zA-Z]|(?: )?[.*-](?: )?)(?:\d|\(\)|i|I|\!|O|o|@|l|G|Z|s|S){2}(?: |$))

// 2015-10-09 Accepts either '$' or 1-3 non-space. Too permissive, at least exclude digit lookalikes here!
//#define PRICE_DOLLAR_REGEX "(?:\\$|[^\\s]{1,3})" // ==> "(?:\\$|[^\\s\\(\\)iI\\!BDOo@lGZsS\\/]{1,3})"
// 2015-10-09 why accept up to 3 chars instead of $? 1 is enough - if we also ignore various glued noise characters
// #define PRICE_DOLLAR_REGEX "(?:\\$|[^\\s\\(\\)iI\\!BDOo@lGZsS\\/]{1,3})"
// 2015-10-20 retiring this:
//#define PRICE_DOLLAR_REGEX "(?:\\$|[.\\-*',~:]*[^\\s\\(\\)iI\\!BDOo@lGZsS\\/][.\\-*',~:]*)"
// Updating to remove the dot-like chars from consideration as dollar signs (otherwise ".S8.00" is returned as "58.00"). Also, letting 'S' be accepted as '$' since we have rule 0174 sometimes changing $ to S.
//#define PRICE_DOLLAR_REGEX "(\\$|[.\\-*',~:]*[^\\s\\(\\)iI\\!BDOo@lGQZ\\/.\\-*',~:][.\\-*',~:]*)"
#define PRICE_DOLLAR_REGEX "([.\\-*',~:]*(?:\\$|s|S)[.\\-*',~:]*)"
// Replace %s with PRICE_DOLLAR_REGEX if prices have dollars

// Not sure why we don't accept the comma as delimiter in the price. If updating the below ".*-", need to update isDecimalPointLookalike() function. TODO: doesn't '.' stand for any character, even in square brackets?
// 2015-10-9 no idea why we accepted [^0-9a-zA-Z] for the decimal point, seems sufficient to accept [.*-~]. Note: I think we do need to continue to accept a space (dot missed by Blink)
//#define PRICE_CORE_REGEX "%s" DIGIT "{1,5}(?:[^0-9a-zA-Z]|(?: )?[.*-](?: )?)" DIGIT "{2}"
//#define PRICE_CORE_REGEX "%s" DIGIT_NODOLLAR "{1,5}(?: |(?: )?[\\.\\*\\-~](?: )?)" DIGIT_NODOLLAR "{2}"
// 2016-03-10 no longer accepting space as decimal point!
// 2016-03-11 no longer accepting '-' or '~' as decimal point, don't recall ever seeing that in OCR + we should fix such cases at the OCR level
#define PRICE_CORE_REGEX "%s" DIGIT_NODOLLAR "{1,5}(?: )?[\\.\\*](?: )?" DIGIT_NODOLLAR "{2}"
// Below is the version for retailers using ".XX" for sub-$1 prices (e.g. KMART). If nothing before decimal point, expect ./*/-/~ and no space following
//#define PRICE_CORE_REGEX_SUB1 "%s(?:" DIGIT_NODOLLAR "{1,5}(?: |(?: )?[\\.\\*\\-~](?: )?)|[\\.\\*\\-~])" DIGIT_NODOLLAR "{2}"
// 2016-03-10 no longer accepting space as decimal point!
// 2016-03-11 no longer accepting '-' or '~' as decimal point, don't recall ever seeing that in OCR + we should fix such cases at the OCR level
#define PRICE_CORE_REGEX_SUB1 "%s" DIGIT_NODOLLAR "{0,5}(?: )?[\\.\\*](?: )?" DIGIT_NODOLLAR "{2}"

//#define PRICE_REGEX "(?:(?:^| )" PRICE_CORE_REGEX "%extracharsafterpricesregex(?: |$))"
#define PRICE_REGEX "(?:(?:^| )%extracharsbeforepricesregex%price_core_regex%extracharsafterpricesregex(?=(?: |$)))"

#define PRICE_DISCOUNT_REGEX "(?:(?:^| )%discountStartingRegex%price_core_regex%discountEndingRegex(?: |$))"

/* Date Regex:
- Recognizes 05/06/2015 09:15AM
- Time part optional
- Allowing any characters in the spot for the 4-digit year because we KNOW what year the receipt was scanned - this year, or in January or February perhaps last year but then we can tell by the month being 12 or 11 (any month greater than current month) - we'll adjust for this in the final formatting
- Reason we allow 4 to 8 is because OCR sometimes returns multiple characters: final formatting will check that the total width of the year more or less matches the widths of month + day
*/

typedef map<string, cppPCRE *> CompiledRegExpMap;
#if OCR_MULTITHREAD
static pthread_mutex_t mutexRegexMap = PTHREAD_MUTEX_INITIALIZER;
#endif
    
cppPCRE *compiledRegexForPattern(const string regexPattern) {
#if OCR_MULTITHREAD
    pthread_mutex_lock(&mutexRegexMap);
#endif
    static CompiledRegExpMap m;
    
    cppPCRE *regex = m[regexPattern];
    if (regex == NULL) {
        cppPCRE *regex = new cppPCRE();
        bool success = regex->Compile(regexPattern, PCRE_UTF8 | PCRE_UCP);
        if (!success)
        {
            delete regex;
#if OCR_MULTITHREAD
            pthread_mutex_unlock(&mutexRegexMap);
#endif
            return NULL;
        }
        m[regexPattern] = regex;
#if OCR_MULTITHREAD
        pthread_mutex_unlock(&mutexRegexMap);
#endif
        return regex;
    } else {
#if OCR_MULTITHREAD
        pthread_mutex_unlock(&mutexRegexMap);
#endif
        return regex;
    }
}

bool RegexUtilsTestArrayForType(MatchVectorPtr res, OCRElementType tp) {
	for (MatchVector::iterator iter = res->begin(); iter != res->end(); ++iter) {
	    MatchPtr d = *iter;
		if (getElementTypeFromMatch(d, matchKeyElementType) == tp)
			return true;
	}
	return false;
}

int RegexUtilsStripLeading(wstring& text, const wstring& set) {
    int cnt = 0;
	while ((text.length() > 0) && (set.find(text[0]) != set.npos) ) {
		text.erase(0,1);
        cnt++;
	}
    return cnt;
}

void RegexUtilsRemoveLeadingBlanks(wstring& text) {
    RegexUtilsStripLeading(text, L" ");
}

int RegexUtilsStripTrailing(wstring& text, const wstring& set) {
    int cnt = 0;
	while ((text.length() > 0) && (set.find(text[text.length()-1]) != set.npos) ) {
		text.erase(text.length()-1, 1);
        cnt++;
	}
    return cnt;
}

void RegexUtilsRemoveTrailingBlanks(wstring& text) {
    RegexUtilsStripTrailing(text, L" ");
}

wstring extractNumberSequence(wstring element, int nDigits) {
    if (element.length() < nDigits)
        return wstring();
    else if (element.length() == nDigits) {
        wstring formattedNumber = convertDigitsLookalikesToDigits(element);
        if (countDigits(formattedNumber) == nDigits)
            return formattedNumber;
        else
            return wstring();
    }
    
    // Try to use first n or last n, see which one gets a higher grade
    wstring firstN = element.substr(0,nDigits);
    wstring firstNDigits = convertDigitsLookalikesToDigits(firstN);
    int firstNCountDigits = 0, firstNCountDigitsLookalikes = 0;
    for (int j=0; j<firstNDigits.length(); j++) {
        if (isDigit(firstN[j])) {
            firstNCountDigits++;
        }
        if (isDigit(firstNDigits[j])) {
            firstNCountDigitsLookalikes++;
        }
    }
    wstring lastN = element.substr((int)element.length()-nDigits,nDigits);
    wstring lastNDigits = convertDigitsLookalikesToDigits(lastN);
    int lastNCountDigits = 0, lastNCountDigitsLookalikes = 0;
    for (int j=0; j<lastNDigits.length(); j++) {
        if (isDigit(lastN[j])) {
            lastNCountDigits++;
        }
        if (isDigit(lastNDigits[j])) {
            lastNCountDigitsLookalikes++;
        }
    }
    if ((lastNCountDigitsLookalikes == 3) && (lastNCountDigits >= firstNCountDigits)) {
        return lastNDigits;
    } else if (firstNCountDigitsLookalikes == 3) {
        return firstNDigits;
    } else {
        return wstring();
    }
}

// e.g. C 281 J 351 - 2616
// Takes a phone pattern w/all errors tolerated by the Regex and returns a clean string with the phone in format 222-123-4567
// Returns empty string in case of failure
wstring RegexUtilsBeautifyPhone(const wstring& inputText) {
	wstring final;
    wstring input = inputText;
//#if DEBUG
//    if ((inputText.find(L"5.49") != wstring::npos) || (inputText.find(L"6.49") != wstring::npos)) {
//        DebugLog("");
//    }
//#endif
    vector<wstring> items;
    split(input, L" -", items);
    wstring areacode, digits1to3, digits4to7;
    for (int i=0; i<items.size(); i++) {
        wstring element = items[i];
        if (element.length() >= 3) {
            if (areacode.length() == 0) {
                // Current element should be the area code
                if (element.length() == 3) {
                    wstring tmpString = convertDigitsLookalikesToDigits(element);
                    if (countDigits(tmpString) == 3)
                        areacode = tmpString;
                } else if (element.length() == 5) {
                    // Assume something like "(123)", just strip first/last
                    wstring tmpString = convertDigitsLookalikesToDigits(element.substr(1,3));
                    if (countDigits(tmpString) == 3)
                        areacode = tmpString;
                } else {
                    // Try to use first 3 or last 3, see which one gets a higher grade
                    wstring tmpString = extractNumberSequence(element, 3);
                    if (tmpString.length() == 3)
                        areacode = tmpString;
                }
                continue; // Hope remaining elements get us what we need
            } else if (digits1to3.length() == 0) {
                // Current element should be first 3 digits of phone
                if (element.length() == 3) {
                    wstring tmpString = convertDigitsLookalikesToDigits(element);
                    if (countDigits(tmpString) == 3)
                        digits1to3 = tmpString;
                } else if (element.length() == 5) {
                    // Assume something like "(123)", just strip first/last
                    wstring tmpString = convertDigitsLookalikesToDigits(element.substr(1,3));
                    if (countDigits(tmpString) == 3)
                        digits1to3 = tmpString;
                } else {
                    // Try to use first 3 or last 3, see which one gets a higher grade
                    wstring tmpString = extractNumberSequence(element, 3);
                    if (tmpString.length() == 3)
                        digits1to3 = tmpString;
                }
                continue; // Hope remaining elements get us what we need
            } else if (digits4to7.length() == 0) {
                // Current element should be last 4 digits of phone
                if (element.length() == 4) {
                    wstring tmpString = convertDigitsLookalikesToDigits(element);
                    if (countDigits(tmpString) == 4)
                        digits4to7 = tmpString;
                } else if (element.length() == 6) {
                    // Assume something like "(1234)", just strip first/last
                    wstring tmpString = convertDigitsLookalikesToDigits(element.substr(1,4));
                    if (countDigits(tmpString) == 4)
                        digits4to7 = tmpString;
                } else {
                    // Try to use first 4 or last 4, see which one gets a higher grade
                    wstring tmpString = extractNumberSequence(element, 4);
                    if (tmpString.length() == 4)
                        digits4to7 = tmpString;
                }
                continue; // After area code + 1-3 digits we need the 4-7 digits, skip this element and hope remaining elements get us what we need
            }
        }
    }
    
    if ((areacode.length() == 3) && (digits1to3.length() == 3) && (digits4to7.length() == 4)) {
        return areacode + L"-" + digits1to3 + L"-" + digits4to7;
    } else {
        ErrorLog("RegexUtilsBeautifyPhone: unexpected input text [%s]", toUTF8(input).c_str());
        return wstring();
    }
}

// Takes a price pattern w/all errors tolerated by the Regex and returns a clean string with the price XXX.YY
// Returns empty string in case of failure
wstring RegexUtilsBeautifyPrice(const wstring& inputText, OCRResults *results) {
	wstring final;
    wstring input = inputText;
#if DEBUG
    if ((inputText.find(L"5.49") != wstring::npos) || (inputText.find(L"6.49") != wstring::npos)) {
        DebugLog("");
    }
#endif
    // Eliminate blanks at start / end
    RegexUtilsStripTrailing(input, L" ");
    RegexUtilsStripLeading(input, L" ");
    // Eliminate trailing '-' (discounts)
    RegexUtilsStripTrailing(input, L"-");
    
    if ((input.length()>=3) && (results->retailerParams.noZeroBelowOneDollar) && (input[0] == L'.'))
        input = L"0" + input;   // Re-create the missing '0'
    
    // Should never happen but test anyhow
    if (input.length() < PRICE_MIN_LEN) {
        ErrorLog("RegexUtilsBeautifyPrice: unexpected short input text [%s]", toUTF8(input).c_str());
        return wstring();
    }
    
    // Capture last two digits (decimals)
    if (!isDigit(input[input.length()-2]) || !isDigit(input[input.length()-1])) {
        ErrorLog("RegexUtilsBeautifyPrice: failed to find decimals in [%s]", toUTF8(input).c_str());
        return wstring();
    }
    
    final = L"." + input.substr(input.length()-2, 2);
    
    // Now add all digits before the decimal point
    int lastDigitToAdd = -1;
    int firstDigitToAdd = -1;
    bool foundDigit = false;
    for (int i=(int)input.length() - 3; i >= 0; i--) {
        if (isDigit(input[i]) || isDigitLookalikeExtended(input[i])) {
            firstDigitToAdd = i;
            if (lastDigitToAdd < 0) lastDigitToAdd = i;
            foundDigit = true;
        } else {
            // If still searching for the first digit just skip (Regex decided to accept this char as part of divider between dollars and cents, otherwise stop here, we got one or more digit(s) and now something that's not a digit - done
            if (foundDigit)
                break;
        }
    }
    
    // We expect at least one digit + at least one character for the leading '$' (or whatever char(s) the OCR returned)
        // Not even one digit
    if (!foundDigit) {
        // Or only one digit and nothing before that (i.e. missing '$') - accept a missing $ for now bc Blink seems to totally miss it sometimes (see image sent by Darren 2015/5/6)
        // || ((lastDigitToAdd == firstDigitToAdd) && (firstDigitToAdd == 0))){
        ErrorLog("RegexUtilsBeautifyPrice: failed to find at least one digit in [%s]", toUTF8(input).c_str());
        return wstring();
    }

// Retiring the below - price regex now fully demands a dollar (or '5', 'S' etc) AND strips it at the Regex level via a capturing group
//    if (results->retailerParams.pricesHaveDollar) {
//        // Remove first digit as a possible $ only if we have more than one digit before the decimal point, and it's a digit that looks like $
//        // TODO not very safe considering prices may be above $10 of course, and Blink misses the $ sometimes. Idea: use binary image to look for a pattern left to the first digit
//        if ((lastDigitToAdd - firstDigitToAdd + 1 >= 2) && (firstDigitToAdd == 0) && (input[firstDigitToAdd] == '5'))
//            firstDigitToAdd++;
//    }
    
    final = input.substr(firstDigitToAdd, lastDigitToAdd - firstDigitToAdd + 1) + final;
    return final;
} // RegexUtilsBeautifyPrice

//// Takes a date pattern w/all errors tolerated by the Regex and returns a clean string MM/DD/YYYY
//// Returns empty string in case of failure
//wstring RegexUtilsBeautifyDate(const wstring& inputText) {
//    static vector<wstring> items;
//    if (items.size() == 0) {
//        split(inputText, L'/', items);
//    }
//    
//    if (items.size() < 3) {
//        ErrorLog("RegexUtilsBeautifyDate: date missing two forward slashes in input text [%s]", toUTF8(inputText).c_str());
//        return wstring();
//    }
//    
////    int month = 0, day = 0;
////    month = atoi(toUTF8(items[0]).c_str());
////    day = atoi(toUTF8(items[0]).c_str());
//
//    wstring final;
//    wstring year = items[2];
//    RegexUtilsStripTrailing(year, L" ");
//    if ((year.length() == 4) && (countDigits(year) == 4)) {
//        final = items[0] + L"/" + items[1] + L"/" + year;
//    } else
//        final = items[0] + L"/" + items[1] + L"/2015";
//    
//    // Add optional time part
//    size_t posBlank = items[2].find(L' ');
//    if (posBlank != wstring::npos) {
//        wstring lastPart = items[2].substr(posBlank+1);
//        RegexUtilsRemoveLeadingBlanks(lastPart);
//        if (lastPart.length() >= 8) {
//            // 09:34 PM
//            final += L' ' + lastPart.substr(0,2) + L':' + lastPart.substr(3,2) + L' ' + lastPart.substr(6,1) + L'M';
//        }
//    }
//    
//    return final;
//} // RegexUtilsBeautifyDate

// Takes a product description pattern w/all errors tolerated by the Regex and returns a clean string
// Returns empty string in case of failure
wstring RegexUtilsBeautifyProductDescription(const wstring& inputText, OCRResults *results) {
    if (inputText.length() == 0) return wstring();
//#if DEBUG
//    if (inputText.find(L"CLIF") != wstring::npos) {
//        DebugLog("RegexUtilsBeautifyProductDescription: input text [%s]", toUTF8(inputText).c_str());
//    }
//#endif
    wstring input = inputText;
    // Eliminate blanks at start / end
//    RegexUtilsStripTrailing(input, L" ");
//    RegexUtilsStripLeading(input, L" ");
    RegexUtilsStripLeading(input, L")]},. '\"!_;=””“‘’:/~");
    // Allow '.' to survive at the end of description, it happens
    RegexUtilsStripTrailing(input, L", '\"_;=`*””“‘’-([{/");
    
    findAndReplace(input, L"£", L"&"); // £ gets inserted by ValidateLine to indicate a sure &
    
    if (input.length() < results->retailerParams.productDescriptionMinLen) {
        ErrorLog("RegexUtilsBeautifyProductDescription: unexpected short input text [%s]", toUTF8(input).c_str());
        return wstring();
    }
    
    if ((results->retailerParams.stripAtDescEnd != NULL) && (strlen(results->retailerParams.stripAtDescEnd) > 0)) {
        // Eliminate 'T' of 'F' at the end (Target receipts)
        // PQTODO 'T' often returned as something else, usually a tall letter
        // PQTODO do this while also checking coordinates of isolated letter is at the end of product description range
        
        static string tmpS(results->retailerParams.stripAtDescEnd);
        static std::wstring endingsToStrip(tmpS.begin(), tmpS.end());
        static vector<wstring> items;
        if (items.size() == 0) {
            split(endingsToStrip, L',', items);
        }
        for (int i=0; i<items.size(); i++) {
            wstring actualString = L' '+ items[i];
            if (input.length() >= results->retailerParams.productDescriptionMinLen + actualString.length()) {
                if (input.substr(input.length()-actualString.length(), actualString.length()) == actualString) {
                    // Strip that ending!
                    ReplacingLog("RegexUtilsBeautifyProductDescription: stripping [%s] at the end of [%s]", toUTF8(actualString).c_str(), toUTF8(input).c_str());
                    input = input.substr(0, input.length() - actualString.length());
                    RegexUtilsStripTrailing(input, L" ");   // Just in case although we don't have multiple spaces within text
                    break;
                }
            }
        }
    }
    
    if (input.length() == 0) return wstring();
    
    if (results->retailerParams.noProductNumbers) {
        vector<wstring> words;
        split(input, ' ', words);
        // 2 instead of Z in "OZ"
        if ((words.size() >= 2) && (input[input.length()-1] == '2')) {
            wstring newLastWord;
            bool lastWordChanged = false;
            wstring lastWord = words[(int)words.size()-1];
            // Is it a decimal number whereby last '2' might be a 'Z'? Shortest is say 1.5Z, longest is 16.25Z (e.g. 1 or 2 decimal digits before the 'Z')
            if (lastWord.length() >= 4) {
                size_t indexDot = lastWord.find('.');
                if ((indexDot != wstring::npos)
                    && (indexDot >= 1)
                    && (indexDot <= (int)lastWord.length()-3)) {
                    // Are all chars other than the dot digits?
                    bool allDigits = true;
                    for (int i=0; i<(int)lastWord.length()-1; i++) {
                        if (i == indexDot) continue;
                        wchar_t ch = lastWord[i];
                        if ((ch != 'O')
                            && (ch != 'Z')
                            && !isDigit(lastWord[i])) {
                            allDigits = false;
                            break;
                        }
                    }
                    if (allDigits) {
                        // That's it, convert to digits and replace last letter with 'Z'!
                        newLastWord = convertDigitsLookalikesToDigits(lastWord.substr(0, (int)lastWord.length()-1)) + L'Z';
                        lastWordChanged = true;
                    }
                } // found dot at the right place
            } // possible decimal number
            else if ((lastWord.length() == 2) && ((lastWord[0] == 'O') || (lastWord[0] == '0'))) {
                newLastWord = L"OZ";
                lastWordChanged = true;
            }
            // If we still haven't changed the last word, and there is no decimal dot, and not "0Z", perhaps a whole number of ounces, e.g. 3Z or 20Z
            if (!lastWordChanged && ((lastWord.length() == 2) || (lastWord.length() == 3))) {
                // All digits?
                bool allDigits = true;
                for (int i=0; i<lastWord.length()-1; i++) {
                    wchar_t ch = lastWord[i];
                    if ((ch != 'O')
                        && (ch != 'Z')
                        && !isDigit(lastWord[i])) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) {
                    // That's it, convert to digits and replace last letter with 'Z'!
                    newLastWord = convertDigitsLookalikesToDigits(lastWord.substr(0, (int)lastWord.length()-1)) + L'Z';
                    lastWordChanged = true;
                }
            }
            if (lastWordChanged) {
                input = wstring();
                for (int i=0; i<(int)words.size()-1; i++) {
                    if (i == 0)
                        input = words[i];
                    else
                        input += (L" " + words[i]);
                }
                input += (L" " + newLastWord);
                RegexLog("RegexUtilsBeautifyProductDescription: replaced 2 with Z at the end of [%s] => [%s]", toUTF8(inputText).c_str(), toUTF8(input).c_str());
                DebugLog("");
            }
        } // Fixing 2 to Z in "OZ" situations
        // Fixing 8 to B in "LB" situations
        if ((words.size() >= 3) && (input[input.length()-1] == '8')) {
            wstring newLastWord;
            bool lastWordChanged = false;
            wstring lastWord = words[(int)words.size()-1];
            if (lastWord == L"L8") {
                wstring beforeLastWord = words[(int)words.size()-2];
                float numberOfPounds = atof(toUTF8(beforeLastWord).c_str());
                if (numberOfPounds > 0) {
                    newLastWord = L"LB";
                    lastWordChanged = true;
                }
            }
            if (lastWordChanged) {
                input = wstring();
                for (int i=0; i<(int)words.size()-1; i++) {
                    if (i == 0)
                        input = words[i];
                    else
                        input += (L" " + words[i]);
                }
                input += (L" " + newLastWord);
                RegexLog("RegexUtilsBeautifyProductDescription: replaced 8 with B at the end of [%s] => [%s]", toUTF8(inputText).c_str(), toUTF8(input).c_str());
                DebugLog("");
            }
        } // Fixing 8 to B in "LB" situations
    }
    
    return input;
} // RegexUtilsBeautifyProductDescription


// - text:			String to posibly correct and format
// - type:			Type of the item, if known (otherwise OCRElementTypeUnknown)
// - specificOnly:	If set, specify that we should only perform auto-corrections that apply to a specific type (to save time when we already applied generic corrections)
//                  (one example is URLs & emails: we already processed each fragment, either as "domain", "extension" or "Unknown", now we process the entire item)
bool RegexUtilsAutoCorrect(wstring& text, OCRResults *curResults, OCRElementType tp, OCRElementType subType, bool specificOnly, bool wholeFragment)
{
    // Type-specific corrections
    // E.g. perhaps in number sequences map 'O' to '0': findAndReplace(text, L"O", L"0")
    
    if ((tp == OCRElementTypePrice) || (tp == OCRElementTypeProductPrice) || (tp == OCRElementTypeProductNumber) || (tp == OCRElementTypePotentialPriceDiscount) || (tp ==OCRElementTypePriceDiscount) || (tp == OCRElementTypePriceDiscount) || (tp == OCRElementTypeIgnoredProductNumber) || (tp == OCRElementTypePartialProductPrice) || (tp == OCRElementTypePricePerItem) || (tp == OCRElementTypeNumericSequence) || (tp == OCRElementTypeDate) || (tp == OCRElementTypeDateYear) || (tp == OCRElementTypeDateMonth) || (tp == OCRElementTypeDateDay) || (tp == OCRElementTypeDateHour) || (tp == OCRElementTypeDateMinutes) || (tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine)) {
        RegexUtilsStripLeading(text, L" ");
        RegexUtilsStripTrailing(text, L" ");
        if (tp == OCRElementTypeProductNumber) {
            // Reject product number if if only has "1/I/J"
            bool reject = true;
            for (int kk=0; kk<text.length(); kk++) {
                wchar_t ch = text[kk];
                if (!(isVerticalLine(ch) || (ch == 'J'))) {
                    reject = false;
                    break;
                }
            }
            if (reject) {
                RegexLog("RegexUtilsNewApplyRegex: rejecting <%s> as [%s], bogus", toUTF8(text).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                return false;
            }
            // Reject product numbers containing "/201x" (likely a date)
            size_t locSlash = text.find(L'/', 0);
            while ((locSlash != wstring::npos) && (locSlash < text.length()-1)) {
                size_t newLocSlash = text.find(L'/', locSlash+1);
                if (newLocSlash != wstring::npos)
                    locSlash = newLocSlash;
                else
                    break;
            }
            if (locSlash == text.length()-5) {
                if ((text[locSlash+1] == '2') && (text[locSlash+2] == '0') && (text[locSlash+3] == '1')) {
                    RegexLog("RegexUtilsNewApplyRegex: rejecting <%s> as [%s], probable date", toUTF8(text).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                    return false;
                }
            }
        }
        wstring numberText = text;
#if DEBUG
        if (text == L"$21.81") {
            DebugLog("");
        }
#endif
        // If we have product numbers with an postfix, strip it (e.g. "KI" for Walmart I think)
        if ((tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine) && (text.length() > curResults->retailerParams.productNumberLen)) {
            numberText = text.substr(0, curResults->retailerParams.productNumberLen);
        }
        
        int nSuspiciousReplacements = 0;
        findAndReplace(numberText, L"()", L"0");
        findAndReplace(numberText, L"D", L"0");
        nSuspiciousReplacements += findAndReplace(numberText, L"U", L"0");
        findAndReplace(numberText, L"O", L"0");
        findAndReplace(numberText, L"o", L"0");
        nSuspiciousReplacements += findAndReplace(numberText, L"@", L"0");
        int nJs = 0;
        // 'J' is a special case, could be '0' or '1' - for UPC products test which one makes it a valid UPC number!
        if ((tp == OCRElementTypeProductNumber) && ((curResults->retailerParams.productNumberUPC) || (curResults->retailerParams.productNumberUPCWhenPadded)) && (numberText.length() == 12)) {
            size_t loc = numberText.find(L"J");
            if (loc != wstring::npos) {
                nJs++;
                wstring assume1 = numberText.substr(0,loc) + L'1' + numberText.substr(loc+1, numberText.length() - loc - 1);
                if (RegexUtilsValidateUPCCode(assume1)) {
                    numberText = assume1;
                } else {
                    wstring assume0 = numberText.substr(0,loc) + L'0' + numberText.substr(loc+1, numberText.length() - loc - 1);
                    numberText = assume0;
                }
            }
        }
        // Do the below anyhow just in case there were multiple 'J's in a product number
        nJs += findAndReplace(numberText, L"J", L"0");
        if (nJs > 1) {
            RegexLog("RegexUtilsNewApplyRegex: rejecting <%s> as [%s], found 2 J's", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
            return false;
        }
        nSuspiciousReplacements += nJs;
        findAndReplace(numberText, L"Q", L"0");
        findAndReplace(numberText, L"i", L"1");
        findAndReplace(numberText, L"I", L"1");
        findAndReplace(numberText, L"l", L"1");
        findAndReplace(numberText, L"!", L"1");
        findAndReplace(numberText, L"Z", L"2");
        findAndReplace(numberText, L"s", L"5");
        findAndReplace(numberText, L"S", L"5");
        if ((tp == OCRElementTypePrice) || (tp == OCRElementTypePriceDiscount) || (tp == OCRElementTypePotentialPriceDiscount)) {
            // For these types it's imperative NOT to replace $ to a digit if it's the first element! Beautify will strip it
            if ((numberText.length()>0) && (numberText[0] == L'$')) {
                wstring rest = numberText.substr(1, numberText.length()-1);
                nSuspiciousReplacements += findAndReplace(rest, L"$", L"5");
                numberText = L'$' + rest;
            }
        } else
            nSuspiciousReplacements += findAndReplace(numberText, L"$", L"5");
        nSuspiciousReplacements += findAndReplace(numberText, L"G", L"6");
        findAndReplace(numberText, L"B", L"8");
        
        // For dates replace '/' with '7' only in the sub components (i.e. year, month, day, hour, minutes) so as NOT to replace the legit slashes in the date
        if (tp != OCRElementTypeDate) {
            if ((tp == OCRElementTypeProductNumber) || (tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine)) {
                // Disqualify the item any time we find 2 or more slashes whereby two of the slashes are separated by 2 or 4 digits (month, day or year)
                bool abort = false;
                size_t loc = numberText.find(L"/", 0, 1);
                size_t lastLoc = wstring::npos;
                while (loc != wstring::npos) {
                    if ((lastLoc != wstring::npos) && (loc - lastLoc == 3)) {
                        abort = true;
                        break;
                    }
                    lastLoc = loc;
                    loc = numberText.find(L"/", loc+1, 1);
                }
                if (abort) {
                    RegexLog("RegexUtilsNewApplyRegex: rejecting <%s> as [%s], found 2 slashes", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                    return false;
                }
            }
            nSuspiciousReplacements += findAndReplace(numberText, L"/", L"7");
        }
        
        if ((tp == OCRElementTypeProductNumber) && (nSuspiciousReplacements > numberText.length() / 2)) {
            RegexLog("RegexUtilsNewApplyRegex: rejecting <%s> as [%s], too many suspect replacements (%d)", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str(), nSuspiciousReplacements);
            return false;
        }
        
        if ((tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine) && (text.length() > curResults->retailerParams.productNumberLen)){
            text = numberText + text.substr(curResults->retailerParams.productNumberLen, wstring::npos);
        } else
            text = numberText;
        if (tp == OCRElementTypeDateDay) {
            int dayValue = atoi(toUTF8(text).c_str());
            if ((dayValue < 1) || (dayValue > 31)) {
                RegexLog("RegexUtilsAutoCorrect: rejecting <%s> as [%s]", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                return false;
            }
        } else if (tp == OCRElementTypeDateMonth) {
            int monthValue = atoi(toUTF8(text).c_str());
            if ((monthValue < 1) || (monthValue > 12)) {
                RegexLog("RegexUtilsAutoCorrect: rejecting <%s> as [%s]", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                return false;
            }
        } else if (tp == OCRElementTypeDateYear) {
            if (text.length() < 2)
                return false;
            
            int yearValue = atoi(toUTF8(text.substr(text.length()-2,2)).c_str());
            if (yearValue < 10) {
                RegexLog("RegexUtilsAutoCorrect: rejecting <%s> as [%s]", toUTF8(numberText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
                return false;
            }
        }
    }
    if (tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine) {
        bool allOnes = true;
        for (int k=0; k<text.length(); k++) {
            if (text[k] != '1') {
                allOnes = false;
                break;
            }
        }
        if (allOnes)
            return false;
        if ((curResults->retailerParams.code == RETAILER_WALMART_CODE) && (text.length() > 12)) {
            wchar_t ch = text[text.length()-2];
            if (ch == 'X')
                text = text.substr(0,text.length()-2) + L"K" + text[text.length()-1];
            ch = text[text.length()-1];
            if (ch == '1')
                text = text.substr(0,text.length()-1) + L"I";
        }
    } else if (tp == OCRElementTypeProductNumber) {
        // Reject any product that is all 1's, these are bogus
        bool allOnes = true;
        for (int k=0; k<text.length(); k++) {
            if (text[k] != '1') {
                allOnes = false;
                break;
            }
        }
        if (allOnes)
            return false;
        if ((curResults->retailerParams.productNumberUPC) || (curResults->retailerParams.productNumberUPCWhenPadded)) {
            // If length is less than 12 but productNumberUPCWhenPadded is set, check padded version matches UPC (but don't pad what we send to the server)
            if ((text.length() < 12) && curResults->retailerParams.productNumberUPCWhenPadded) {
                wstring tmpString = text;
                for (int kk=0; kk< 12 - text.length(); kk++) {
                    tmpString = L'0' + tmpString;
                }
                if (!RegexUtilsValidateUPCCode(tmpString))
                    return false;
            }
            // If length is not 12, assume it's a alternate product type and not UPC (e.g. 0000-123-456 at Home Depot)
            else if (text.length() == 12) {
                if (!RegexUtilsValidateUPCCode(text))
                    return false;
            }
        }
    } else if (tp == OCRElementTypeProductDescription) {
        wstring tmpString = RegexUtilsBeautifyProductDescription(text, curResults);
        if (tmpString.length() == 0)
            return false;
        text = tmpString;
    } else if ((tp == OCRElementTypePrice) || (tp == OCRElementTypeProductPrice) || (tp == OCRElementTypePriceDiscount) || (tp == OCRElementTypePotentialPriceDiscount)) {
//#if DEBUG
//    if ((text == L"$50.99") || (text == L" $50.99")) {
//        DebugLog("");
//    }
//#endif
        wstring tmpString = RegexUtilsBeautifyPrice(text, curResults);
        if (tmpString.length() == 0)
            return false;
        text = tmpString;
    } else if (tp == OCRElementTypePhoneNumber) {
//#if DEBUG
//    if ((text == L"$50.99") || (text == L" $50.99")) {
//        DebugLog("");
//    }
//#endif
        wstring tmpString = RegexUtilsBeautifyPhone(text);
        if (tmpString.length() == 0)
            return false;
        text = tmpString;
    }
//    else if (tp == OCRElementTypeDate) {
//        if (curResults->retailerParams.dateCode == DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE) {
//            wstring tmpString = RegexUtilsBeautifyDate(text);
//            if (tmpString.length() == 0)
//                return false;
//            text = tmpString;
//        }
//    }
    
    // For all types, strip leading / trailing spaces and other characters we never want to see
    // IMPORTANT: don't do these before the type-specific corrections as some of these may accept and expect the below weird characters (then remove them as part of the formatting/validation)
    if (wholeFragment) {
        RegexUtilsStripLeading(text, L")]},. '\"!_;=””“‘’:/~");
        RegexUtilsStripTrailing(text, L", '\"_;=`*””“‘’-([{/");
    }
    
    return true;
} // RegexUtilsAutoCorrect


/* Takes a regex pattern and applies it to a given text buffer
 Meant to test the type of a fragment (between newlines or tabs), with multiple words separated by blanks or commas
 Parameters:
 - skipLeftskipRight: how many characters to exclude from the match, to handle regex where some marker is expected left/right but not part of the match
 - elements: pointer to a vector to store extracted sub-groups from the pattern. If NULL, groups will not be extracted.
 */
int RegexUtilsNewApplyRegex(const std::string& regexPattern,
							OCRElementType tp,
							int extractGroup,
                            std::wstring& rawText,
							bool attemptSubstitutions,
							vector<GroupType> groups,
							MatchVectorPtr* elements,
							OCRResults *curResults){

    string rawTextUtf8 = toUTF8(rawText);

	if (rawText.length() < 1)
		return 0;

    bool success;
    
#if CACHED_REGEXES
    cppPCRE *pRegex = compiledRegexForPattern(regexPattern);
    if (pRegex == NULL)
        return 0;
    cppPCRE& regex = *pRegex;
#else
	cppPCRE regex;  // TODO do we need to allocate this object for each regex?

	success = regex.Compile(regexPattern, PCRE_UCP); // TODO not unicode - fix later
	if (!success)
	{
		// [regex release];
		return 0;
	}
#endif    

	int startIndex = 0, numberMatched = 0;

	if (regex.RegExMatches(rawTextUtf8, PCRE_FLAGS, startIndex)) {
		size_t start, len;
		success = regex.GetMatch(&start, &len);

		if (!success) {
			// Unexpected regexp failure, once the call w/o returning start & len succeeded, the call to get the first match should have worked
			return 0;
		}

#if DEBUG
		int actualMatchStartLetters = -2;
		int actualMatchLenLetters = -2;
#endif

		int actualMatchStart = (int)start;
		int actualMatchLen = (int)len;
		bool workingWithinLargerBuffer = false;

		if (extractGroup != 0) {
			size_t groupStart, groupLen;
			if (regex.GetMatch(&groupStart, &groupLen, extractGroup)) {
				actualMatchStart = (int)groupStart;
				actualMatchLen = (int)groupLen;

				if (attemptSubstitutions) {
#if DEBUG
					convertIndexes(rawTextUtf8, actualMatchStart, actualMatchLen, actualMatchStartLetters, actualMatchLenLetters);
#endif
				    rawTextUtf8 = rawTextUtf8.substr(actualMatchStart, actualMatchLen);
					rawText = toUTF32(rawTextUtf8);
					workingWithinLargerBuffer = true;
				}
			}
		}

#if DEBUG
		if (actualMatchStartLetters == -2) {
			convertIndexes(rawTextUtf8, actualMatchStart, actualMatchLen, actualMatchStartLetters, actualMatchLenLetters);
		}
		AutocorrectLog("RegexUtilsNewApplyRegex: Matched: <%s> at [%d - %d]", rawTextUtf8, actualMatchStart, actualMatchStartLetters + actualMatchLenLetters - 1);
#endif

		OCRElementType returnedType = OCRElementTypeUnknown;
		if (attemptSubstitutions) {
			/* 12-14-2009: a bit tricky but ... if extractGroup is not used, then even if the regex matched stuff not at index 0 (start > 0), when getting groups, we do NOT need
			     to treat this as a case of "working within a larger buffer" (last param to autocorrect) in the sense that autocorrect needs to substract start from every group.
			     This is the case ONLY if we modified the input buffer to include only a relevant portion => when extract group was specified
			*/
            // PQTODO handle later
			// returnedType = autoCorrectTextWithGroups(rawText, curResults, rawTextUtf8, groups, tp, OCRElementTypeInvalid, &regex, actualMatchStart, actualMatchLen, workingWithinLargerBuffer, elements);

		} // Substitutions
		if (attemptSubstitutions && (returnedType == OCRElementTypeUnknown)) {
			AutocorrectLog("NewApplyRegex: <%s> match reversed by autoCorrectTextWithGroups!", toUTF8(rawText).c_str());
			numberMatched = 0;
		} else {
			AutocorrectLog("Matched Formatted: <%s>", toUTF8(rawText).c_str());
			numberMatched = 1;
		}
	}
	return numberMatched;
} 	// RegexUtilsNewApplyRegex

vector<Range> RegexUtilsFragmentsBetween(const wstring& rawMatch, MatchVectorPtr substitutionsList) {
	vector<Range> res;
	res.push_back(Range(0, (int)rawMatch.length()));

	//YKYK - need to convert indexes to byte indexes

	for (MatchVector::iterator iter = substitutionsList->begin();iter != substitutionsList->end(); ++iter) {
	    MatchPtr d = *iter;
		Range r = *d[matchKeyRange].castDown<Range>();
		// Find the range that this range cuts in two
		for (int i=0; i<res.size(); i++) {
			Range r1 = res[i];
			if ((r.location >= r1.location) && (r.location < (r1.location + r1.length))) {
				// Cut this range in two parts, or truncate, or eliminate altogether
				if (r1.location == r.location) {
					if (r1.length == r.length) {
						// Just remove this item
						res.erase(res.begin()+i);
					} else {
						// Truncate current range
						r1.location = (r.location + r.length);
						if (r1.length < r.length) {
							// Should never happen - scream and return
							// Likely the result of passing a wrong group ID
							ErrorLog("RegexUtilsFragmentsBetween: ERROR, got an illegal set of fragments, [%d - %d] was supposed to contain [%d - %d] - aborting",
									 r1.location, r1.location + r1.length,
									 r.location, r.location + r.length);
							return vector<Range>();
						}
						r1.length = (r1.length - r.length);
						res[i] = r1;
					}
				} else {
					// r starts in the middle of r1
					// Insert first part in lieu of current range
					Range tmpR(r1.location, r.location - r1.location);
					res[i] = tmpR;
					// Is there something left over from r1 after r?
					if (r.location + r.length < r1.location + r1.length) {
						Range tmpR(r.location + r.length, (r1.location + r1.length) - (r.location + r.length));
						res.insert(res.begin() + i + 1, tmpR);
					}
				}
				break;
			}
		}
	}

	return res;
}


wstring RegexUtilsBuildStringFromFragments(MatchVectorPtr substitutionsList) {
	MatchVector list(*substitutionsList);

	wstring res;

	while (list.size() > 0) {
		// Pick fragment with lowest location
		MatchPtr minD;
		int minIndex = -1;
		unsigned int minLocation = ~0; // TODO: Changes this to a normal const!!
		for (int i=0; i<list.size(); i++) {
			MatchPtr d = list[i];
			Range r = *d[matchKeyRange].castDown<Range>();
			if (r.location < minLocation) {
				minD = d;
				minIndex = i;
				minLocation = r.location;
			}
		}
		res += getStringFromMatch(minD, matchKeyText);
		list.erase(list.begin() + minIndex);
	}

	return res;
}

/*
 - start/len: we may be called in a situation where a pattern has been identified within a text buffer from index "start", and within
   the range [start, start+len-1], we have groups that we wish to auto-correct. start will therefore be used to adjust each sub-match accordingly to fit
   within the established regex matching process within the larger buffer
 - workingWithinLargerBuffer: not certain when it makes sense to say "no" on this parameter, it seems that passing "start=0" does the trick
 - elements: pointer to a NSMutableArray to store the extracted groups. If NULL, groups will not be extracted.
 
 Return value: usually equal to tp - but could be set to OCRElementTypeUnknown if we decide the pattern is not kosher
 */
OCRElementType autoCorrectTextWithGroups(wstring& rawText, OCRResults *curResults, const string& rawTextUtf8, const vector<GroupType>& groups, OCRElementType tp,
			OCRElementType subType, cppPCRE *regex, int startBytes, int lenBytes, bool workingWithinLargerBuffer, MatchVectorPtr *elements, bool &successfullMatch, unsigned long &flags) {
//YKYK - need to convert start & len to byte indexes
	MatchVectorPtr substitutionsList(new MatchVector);
    wstring domainString; wstring extensionString;
	int originalLength = (int)rawText.length();
	int startLetters; int lenLetters;
    
    // Specific to date type
    wstring dateMonth, dateDay, dateYear, dateHour, dateMinutes, dateAMPM;
    bool gotDateMonth = false, gotDateDay = false, gotDateYear = false, gotDateHour = false, gotDateMinutes = false, gotDateAMPM = false;
	startLetters = 0;
    if (workingWithinLargerBuffer) {
		if (startBytes > 0) {
			//pqpq convert more efficiently, we don't care about the length
			convertIndexes(rawTextUtf8, startBytes, lenBytes, startLetters, lenLetters);
		}
	}
	for (vector<GroupType>::const_iterator iter = groups.begin(); iter != groups.end(); ++iter) {
	    GroupType group = *iter;
		int groupIndex = group.groupId;
		OCRElementType groupType = group.type;
		int groupOptions = group.flags;

		wstring groupString;
		if (groupIndex <= 0) {
			ErrorLog("autoCorrectGroups: got invalid group id [%d] with type [%s]", groupIndex, OCRResults::stringForOCRElementType(groupType).c_str());
		} else {
			size_t groupStartBytes, groupLenBytes;
			Range groupRangeBytes; Range groupRangeLetters;
			if (regex->GetMatch(&groupStartBytes, &groupLenBytes, groupIndex)) {
				// Even if we work within a larger buffer, all groups should still be between start and (start+len-1)
				if ((groupStartBytes < startBytes) || (groupStartBytes + groupLenBytes - 1 >= (startBytes + lenBytes))) {
					WarningLog("autoCorrectGroups: WARNING, failed to extract (group %d - type %s) in match [%s] type[%s], got invalid group start index [%d]",
							   groupIndex, OCRResults::stringForOCRElementType(groupType).c_str(), toUTF8(rawText).c_str(), OCRResults::stringForOCRElementType(tp).c_str(), (int)groupStartBytes);

					continue;
				}
				// Need the below for the index adjustments based on the entire string analyzed by regex
				Range groupRangeBytesLargerBuffer = Range((int)groupStartBytes, (int)groupLenBytes);
				if (workingWithinLargerBuffer) {
					// Adjust the range so that it applies to the input text even though that text is in the middle of a larger text on which regex is operating
					groupRangeBytes = Range((int)(groupStartBytes-startBytes), (int)groupLenBytes);
				} else {
					groupRangeBytes = Range((int)groupStartBytes, (int)groupLenBytes);
				}

				bool addGroup = true;
				convertIndexes(rawTextUtf8, groupRangeBytesLargerBuffer.location, groupRangeBytesLargerBuffer.length, groupRangeLetters.location, groupRangeLetters.length);
				if (workingWithinLargerBuffer) {
					groupRangeLetters.location = groupRangeLetters.location - startLetters;
				}
                if ((groupRangeLetters.location < 0) || (groupRangeLetters.location >= rawText.length())) {
					WarningLog("autoCorrectGroups: WARNING, invalid start [%d] + length [%d] into failed to extract (group %d - type %s) in match [%s] type[%s]", (int)groupRangeLetters.location, (int)groupRangeLetters.length, groupIndex, OCRResults::stringForOCRElementType(groupType).c_str(), toUTF8(rawText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());

					continue;
				}
				groupString = rawText.substr(groupRangeLetters.location, groupRangeLetters.length);

//				// Need to convert the range (in the UTF8 string) into the range in the UTF32 string
//				convertIndexes(rawTextUtf8, groupRangeBytes.location, groupRangeBytes.length, groupRangeLetters.location, groupRangeLetters.length);
//				groupString = rawText.substr(groupRangeLetters.location, groupRangeLetters.length);

				RegexLog("autoCorrectGroups: extracted group [%s] (group#%d - type %s) in range [%d - %d]",
						 toUTF8(groupString).c_str(), groupIndex, OCRResults::stringForOCRElementType(groupType).c_str(), groupRangeLetters.location, groupRangeLetters.location + groupRangeLetters.length);
                         
//                if (groupType == OCRElementTypeDomainExtension) {
//                    // TR mystery: domain extension IS going through auto-correctiong through here (called by RegexUtilsParseColumnForType, itself called by RegexUtilsParseFragmentWithPattern) but without this line below, extensions are NOT auto-corrected (at least when parsing URLs) - weird considering the extensive handling of extensions there
//                    RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true);
//                }
 // Not a prefix
 
                 if (tp == OCRElementTypeDate) {
                    if (group.type == OCRElementTypeDateMonth) {
                        if (RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true)) {
                            dateMonth = groupString;
                            gotDateMonth = true;
                            continue;
                        } else
                            return OCRElementTypeUnknown;
                    } else if (group.type == OCRElementTypeDateMonthLetters) {
                        // e.g. JANUARY, FEBRUARY, MARCH, APRIL, MAY, JUNE, JULY, AUGUST, SEPTEMBER, OCTOBER, NOVEMBER, DECEMBER
                        if ((groupString.length() >= 1) && (groupString[0] == 'J')) {
                            // JANUARY, JUNE or JULY
                            if (groupString.length() >= 5) {
                                dateMonth = L"01"; // JANUARY
                                gotDateMonth = true;
                            } else if (groupString.length() >= 4) {
                                if ((groupString[groupString.length()-1] == 'E') || (groupString[groupString.length()-2] == 'N')) {
                                    dateMonth = L"06"; // JUNE
                                } else if ((groupString[groupString.length()-1] == 'Y') || (groupString[groupString.length()-2] == 'L')) {
                                    dateMonth = L"07"; // JULY
                                }
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && (groupString[0] == 'F')) {
                            dateMonth = L"02"; // FEBRUARY
                            gotDateMonth = true;
                        } else if ((groupString.length() >= 1) && (groupString[0] == 'M')) {
                            // MARCH or MAY
                            if (groupString.length() >= 5) {
                                dateMonth = L"03"; // MARCH
                                gotDateMonth = true;
                            } else if (groupString.length() >= 3) {
                                dateMonth = L"05"; // MAY
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && (groupString[0] == 'A')) {
                            // APRIL or AUGUST
                            if ((groupString.length() >= 6) && ((groupString[1] == 'U') || ((groupString[2] == 'G') || (groupString[2] == '6')))) {
                                dateMonth = L"08"; // AUGUST
                                gotDateMonth = true;
                            } else if ((groupString.length() >= 5) && ((groupString[1] == 'P') || (groupString[2] == 'R'))) {
                                dateMonth = L"04"; // APRIL
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && ((groupString[0] == 'S') || (groupString[0] == '$'))) {
                            // SEPTEMBER
                            if (groupString.length() >= 7) {
                                dateMonth = L"09"; // SEPTEMBER
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && ((groupString[0] == 'O') || (groupString[0] == '0'))) {
                            // OCTOBER
                            if ((groupString.length() >= 3) && (groupString[1] == 'E')) {
                                dateMonth = L"12"; // Actually, DECEMBER (D mistaken for O)
                                gotDateMonth = true;
                            } else if ((groupString.length() >= 3) && ((groupString[1] == 'C') || (groupString[1] == 'T'))) {
                                dateMonth = L"10"; // OCTOBER
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && (groupString[0] == 'N')) {
                            // NOVEMBER
                            if ((groupString.length() >= 3)
                                && (((groupString[1] == 'O') || (groupString[1] == '0') || (groupString[1] == 'D'))
                                    || (groupString[2] == 'V'))) {
                                dateMonth = L"11"; // NOVEMBER
                                gotDateMonth = true;
                            }
                        } else if ((groupString.length() >= 1) && (groupString[0] == 'D')) {
                            // DECEMBER
                            if ((groupString.length() >= 3)
                                && ((groupString[1] == 'E') || (groupString[2] == 'C'))) {
                                dateMonth = L"12"; // DECEMBER
                                gotDateMonth = true;
                            }
                        }
                        if (gotDateMonth)
                            continue;
                        else {
                            RegexLog("autoCorrectGroups: ERROR, invalid month [%s] (group#%d - type %s) in range [%d - %d]",
						 toUTF8(groupString).c_str(), groupIndex, OCRResults::stringForOCRElementType(groupType).c_str(), groupRangeLetters.location, groupRangeLetters.location + groupRangeLetters.length);
                            return OCRElementTypeUnknown;
                        }
                    } else if (group.type == OCRElementTypeDateDay) {
                        if (RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true)) {
                            dateDay = groupString;
                            gotDateDay = true;
                            continue;
                        } else
                            return OCRElementTypeUnknown;
                    } else if (group.type == OCRElementTypeDateYear) {
                        if (RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true)) {
                            if (groupString.length() == 2) {
                                dateYear = L"20" + groupString;
                                flags |= matchKeyStatus2DigitDate;
                            } else
                                dateYear = groupString;
                            gotDateYear = true;
                            continue;
                        } else
                            return OCRElementTypeUnknown;
                    } else if (group.type == OCRElementTypeDateHour) {
                        RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true);
                        dateHour = groupString;
                        gotDateHour = true;
                        continue;
                    } else if (group.type == OCRElementTypeDateMinutes) {
                        RegexUtilsAutoCorrect(groupString, curResults, groupType, groupType, true, true);
                        dateMinutes = groupString;
                        gotDateMinutes = true;
                        continue;
                    } else if (group.type == OCRElementTypeDateAMPM) {
                        // Bump hour by 12 if we got PM
                        if (gotDateHour && (groupString.length() >= 1) && (toLower(groupString[0]) == L'p')) {
                            int hour = std::atoi(toUTF8(dateHour).c_str());
                            if (hour < 12)
                                hour += 12;
                            wstringstream ss;
                            ss << hour;
                            dateHour = ss.str();
                        }
                        dateAMPM = groupString;
                        gotDateAMPM = true;
                        flags |= matchKeyStatusDateAMPM;
                        continue;
                    } else if (group.type == OCRElementSubTypeDateDateDivider) {
                        if (groupString == L"-") {
                            if (flags & matchKeyStatusSlashDivider) {
                                RegexLog("autoCorrectTextWithGroups: rejecting date [%s], using both - and /", toUTF8(rawText).c_str());
                                return OCRElementTypeUnknown;
                            }
                            flags |= matchKeyStatusDashDivider;
                        } else if (groupString == L"/") {
                            if (flags & matchKeyStatusDashDivider) {
                                RegexLog("autoCorrectTextWithGroups: rejecting date [%s], using both - and /", toUTF8(rawText).c_str());
                                return OCRElementTypeUnknown;
                            }
                            flags |= matchKeyStatusSlashDivider;
                        }
                    }
                }
                
                if (group.type == OCRElementTypeMarkerOfPriceOnNextLine) {
                    tp = OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine;
                }
            
                if ((groupOptions != groupOptionsDeleteOnly) && (groupOptions != groupOptionsDeleteAndTagAsGroupType) && (groupOptions != groupOptionsDeleteAndSetFlag)) {
                    if (groupOptions == groupOptionsCopyStartNonZero) {
                        RegexUtilsAutoCorrect(groupString, curResults, groupType, OCRElementTypeNumericSequence, false, false);
                        wstring newText;
                        bool startCopying = false;
                        for (int n=0; n<groupString.length(); n++) {
                            wchar_t ch = groupString[n];
                            if (ch != '0')
                                startCopying = true;
                            if (startCopying)
                                newText += ch;
                        }
                        if (newText.length() == 0)
                            groupOptions = groupOptionsDeleteOnly;
                        else
                            groupString = newText;
                    } else {
                        RegexUtilsAutoCorrect(groupString, curResults, groupType, OCRElementTypeInvalid, false, false);
                    }
                }

				// Add group string to elements list (unless this group is marked as don't copy)
				if ((elements != NULL) && (groupOptions != groupOptionsDontCopy) && (groupOptions != groupOptionsDeleteOnly) && (groupOptions != groupOptionsDeleteAndTagAsGroupType) && (groupOptions != groupOptionsDeleteAndSetFlag) && (groupOptions != groupOptionsCopyStartNonZero)) {
                    ErrorLog("autoCorrectTextWithGroup: ERROR, unexpected groupOption=%d", groupOptions);
                    return OCRElementTypeUnknown;
                    
					// Check if need to allocate the mutable array. No need - we tested it's not NULL
//					if (elements->isNull()) {
//						*elements = MatchVectorPtr(new MatchVector);
//                    }

//					MatchVectorPtr extractedGroups = *elements;
//					MatchPtr newElement(true);
//                    newElement[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(groupType)).castDown<EmptyType>();
//					newElement[matchKeyText] = SmartPtr<wstring>(new wstring(groupString)).castDown<EmptyType>();
//
//					// If only copied, tell AB1 by setting confidence to -2
//					if (groupOptions == groupOptionsCopyOnly) {
//					    newElement[matchKeyConfidence] = SmartPtr<float>(new float(-2)).castDown<EmptyType>();
//					}
//
//                    // Commenting out as long as we don't support group option other than groupOptionsDontCopy or groupOptionsDeleteOnly
//					//addDerivedTypeToElements(newElement, extractedGroups);
//
//					if (groupOptions != groupOptionsCopyOnly) {
//						groupString = L""; // Blank out in final results
//					}
				}
                
                if (groupOptions == groupOptionsDeleteAndTagAsGroupType) {
                    groupString = L""; // Erase from final results
                    tp = groupType;
                } else if (groupOptions == groupOptionsDeleteAndSetFlag) {
                    groupString = L""; // Erase from final results
                    if (groupType == OCRElementSubTypeDollar) {
                        flags |= matchKeyStatusHasDollar;
                    } else if (groupType == OCRElementSubTypeDateDatePrefix) {
                        flags |= matchKeyStatusDateHasDatePrefix;
                    } else if (groupType == OCRElementSubTypeDateTimePrefix) {
                        flags |= matchKeyStatusDateHasTimePrefix;
                    }
                }

				if (groupOptions == groupOptionsDeleteOnly) {
					groupString = L""; // Erase from final results
				}

				if (addGroup || (groupOptions == groupOptionsDeleteOnly)) {
				    MatchPtr tmpmatch(true);
				    tmpmatch[matchKeyText] = SmartPtr<wstring>(new wstring(groupString)).castDown<EmptyType>();
				    tmpmatch[matchKeyRange] = SmartPtr<Range>(new Range(groupRangeLetters)).castDown<EmptyType>();

					substitutionsList->push_back(tmpmatch);
				}
			} else {
				WarningLog("autoCorrectGroups: failed to extract (group %d - type %s) in match [%s] type[%s]",
						   groupIndex, OCRResults::stringForOCRElementType(groupType).c_str(),
						   toUTF8(rawText).c_str(), OCRResults::stringForOCRElementType(tp).c_str());
			}
		}
	}
    
    if (tp == OCRElementTypeDate) {
        // For dates we don't really use anything other than the groups we got (month, day, etc) so no need to use substititutions etc. Just put together the desired date string
        if (gotDateMonth && gotDateDay && gotDateYear) {
            // Check validity (day & month were already checked individually), date was checked to be > 2010
            time_t t = time(NULL);
            tm* timePtr = localtime(&t);

            int dayValue = atoi(toUTF8(dateDay).c_str());
            int monthValue = atoi(toUTF8(dateMonth).c_str());
            int yearValue = atoi(toUTF8(dateYear).c_str());
            
            // Day may be off by one, because we get GMT time (even though I called localtime)! However, app is meant only for the US market, which is behind GMT so we can ignore this
            size_t actualDay = timePtr->tm_mday + 1;
            size_t actualMonth = timePtr->tm_mon + 1;
            size_t actualYear = timePtr->tm_year + 1900;
            
            bool accept = true;
            if (yearValue > actualYear) {
                accept = false;
            } else if (yearValue < actualYear) {
                accept = true;
            } else {
                // Same year
                if (monthValue > actualMonth)
                    accept = false;
                else if (monthValue < actualMonth) {
                    accept = true;
                } else {
                    // Same month
                    if (dayValue > actualDay)
                        accept = false;
                }
            }
            
            rawText = dateMonth + L"/" + dateDay + L"/" + dateYear;
            if (gotDateHour && gotDateMinutes)
                rawText += L" " + dateHour + L":" + dateMinutes;
            if (accept) {
                RegexLog("autoCorrectTextWithGroups: returning date [%s]", toUTF8(rawText).c_str());
            } else {
                RegexLog("autoCorrectTextWithGroups: rejecting date [%s], set in the future", toUTF8(rawText).c_str());
                return OCRElementTypeUnknown;
            }
        } else {
            // Do nothing, just leave text unchanged - something went wrong if we didn't find the minimal required elements!
            WarningLog("autoCorrectGroups: ERROR, failed to find required date components in match [%s]", toUTF8(rawText).c_str());
            return OCRElementTypeUnknown;
        }
    } else {
        // Now substitute in the other parts of the match
        vector<Range> otherList = RegexUtilsFragmentsBetween(rawText, substitutionsList);
        for (int i = 0; i < otherList.size(); ++i) {
            Range tmpRange = otherList[i];
            wstring fragment = rawText.substr(tmpRange.location, tmpRange.length);
            RegexUtilsAutoCorrect(fragment, curResults, OCRElementTypeUnknown, OCRElementTypeInvalid, false, false);
            MatchPtr tmpmatch(true);
            tmpmatch[matchKeyText] = SmartPtr<wstring>(new wstring(fragment)).castDown<EmptyType>();
            tmpmatch[matchKeyRange] = SmartPtr<Range>(new Range(tmpRange)).castDown<EmptyType>();
            substitutionsList->push_back(tmpmatch);
        }

        // Finally, put together the entire string from the list of correct fragments ...
        rawText = RegexUtilsBuildStringFromFragments(substitutionsList);
    }

	// Additional corrections which can only be done on entire item
	if (!RegexUtilsAutoCorrect(rawText, curResults, tp, subType, true, true)) {
        // Likely unexpected issue with item text vis-a-vis expected type
        successfullMatch = false;
    }

	// Special handling of items that were completely eliminated because their groups got extracted - for now only address types
	if (rawText.length() == 0) {
		for (int pp=0; pp<originalLength; pp++) {
			rawText += L" ";
		}
	}

	return (tp);
} // autoCorrectGroups

/*
 * Parses a column around a given type. Called only for strong types (phone, URL, email, skype).
 - extractGroup is the ID of the matching group in cases where we want to only capture that element. For example we detect "skype: foobar" but wish to only return "foobar" to the user
 
 */
bool RegexUtilsParseColumnForType(const string& regexPattern,
			OCRElementType typeSearched,
			OCRElementType subType,
			const vector<GroupType>& groups,
			int extractGroup,
			const wstring& input,
            const wstring& input2,
			MatchVectorPtr *resPtr,
			MatchVectorPtr *elements,
			OCRResults *curResults) {

    OCRElementType type = typeSearched;
	if (input.length() < 1)
		return false;
    
#if DEBUG
    if ((type == OCRElementTypePrice) && (input == L"1 OB GLIDE CMFT FLS 43.7 2.64T SWED 50X 2.65")) {
        DebugLog("");
    }
#endif

    string utf8input = toUTF8(input);
    bool gotText2 = (input.length() == input2.length());
    string utf8input2 = string();
    if (gotText2)
        utf8input2 = toUTF8(input2);
    MatchVectorPtr res(new MatchVector);
    bool success;
    
#if CACHED_REGEXES
    cppPCRE *pRegex = compiledRegexForPattern(regexPattern);
    if (pRegex == NULL)
        return 0;
    cppPCRE& regex = *pRegex;
#else    
	cppPCRE regex;

	success = regex.Compile(regexPattern, PCRE_UCP);
	if (!success)
	{
		return 0;
	}
#endif

	int startIndex = 0;
	int lastFragmentEnd = -1;
	while ((startIndex < utf8input.length()) && regex.RegExMatches(utf8input, PCRE_FLAGS, startIndex)) {
		size_t start = startIndex;
        size_t len;
		success = regex.GetMatch(&start, &len);

		if (!success || (start < startIndex)) {
			// Unexpected regexp failure, once the call w/o returning start & len succeeded, the call to get the first match should have worked
            return 0;
		}

        int actualMatchStartBytes = (int)start;
        int actualMatchLenBytes = (int)len;

		if (extractGroup != 0) {
			size_t groupStart, groupLen;
			if (regex.GetMatch(&groupStart, &groupLen, extractGroup)) {
                if ((groupStart <= utf8input.length()) && (groupStart + groupLen <= utf8input.length())) { 
                    actualMatchStartBytes = (int)groupStart;
                    actualMatchLenBytes = (int)groupLen;
                }
#if DEBUG
                else {
                    DebugLog("RegexUtilsParseColumnForType: ERROR, group (%d) indices out of range in [%s]", extractGroup, utf8input.c_str());
                }
#endif                
			}
		}

		int actualMatchStart;
		int actualMatchLen;
		convertIndexes(utf8input, actualMatchStartBytes, actualMatchLenBytes, actualMatchStart, actualMatchLen);

		if (len <= 0) {
			return 0;
		}

		wstring formattedFinal = input.substr(actualMatchStart, actualMatchLen);
        wstring formattedFinal2 = wstring();
        if (gotText2) formattedFinal2 = input2.substr(actualMatchStart, actualMatchLen);
        wstring formattedFinalBeforeTextReplacements = formattedFinal;
        wstring formattedFinalBeforeTextReplacements2 = formattedFinal2;
		RegexLog("RegexUtilsParseColumnForType: Matched: <%ls> at [%d - %d]", formattedFinal.c_str(), actualMatchStart, actualMatchStart+actualMatchLen-1);
#if DEBUG
    if (formattedFinal == L"2.65") {
        DebugLog("");
    }
#endif

        OCRElementType replacementType = typeSearched;
		MatchVectorPtr derivedTypes;
        unsigned long flags = 0;

        success = true;
        type = autoCorrectTextWithGroups(formattedFinal, curResults, utf8input, groups, replacementType, subType, &regex, actualMatchStartBytes, actualMatchLenBytes, true, elements, success, flags);
        if (gotText2) {
            bool success2 = false;
            autoCorrectTextWithGroups(formattedFinal2, curResults, utf8input2, groups, replacementType, subType, &regex, actualMatchStartBytes, actualMatchLenBytes, true, elements, success2, flags);
            if (!success2)
                gotText2 = false;
        }
        //bool success = RegexUtilsAutoCorrect(formattedFinal, curResults, type, subType, true, true);
        // In some case RegexUtilsAutoCorrect may fail - this is a sign the type is wrong for this text, skip
        if (!success) {
            startIndex = actualMatchStartBytes + actualMatchLenBytes;
            continue;
        }
		AutocorrectLog("ApplyRegexToRecognizedText: formatted match: <%@>", formattedFinal);

		// Time to add to fragments array
		if (lastFragmentEnd == -1) { 
			res = MatchVectorPtr(new MatchVector);
		}

		// Add fragment between last match and current match (if there is any)
		// Reaching further in case of extract group: we do want to leave the unconsumed leading chars in the preceding fragment
        int matchStartForNextFragment = actualMatchStart;
        int matchLenForNextFragment = actualMatchLen;
        
		if (matchStartForNextFragment > (lastFragmentEnd+1)) {
			Range r = Range(lastFragmentEnd+1, actualMatchStart-(lastFragmentEnd+1));

            MatchPtr tmpDict(true);
            tmpDict[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
            tmpDict[matchKeyText] = SmartPtr<wstring>(new wstring(input.substr(r.location, r.length))).castDown<EmptyType>();
            if (gotText2) tmpDict[matchKeyText2] = SmartPtr<wstring>(new wstring(input2.substr(r.location, r.length))).castDown<EmptyType>();
            tmpDict[matchKeyRange] = SmartPtr<Range>(new Range(r)).castDown<EmptyType>();

            res->push_back(tmpDict);

			lastFragmentEnd = r.location + r.length - 1;
		}

		// Now add current match
		Range r = Range(matchStartForNextFragment,matchLenForNextFragment);
		// Trim possible '\n' at the end
		if (input[r.location+r.length-1] == '\n') {
			r.length = r.length - 1;
		}


        {
			MatchPtr tmpDict(true);
			tmpDict[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(type)).castDown<EmptyType>();
            tmpDict[matchKeyText] = SmartPtr<wstring>(new wstring(formattedFinal)).castDown<EmptyType>();
            if (gotText2)
                tmpDict[matchKeyText2] = SmartPtr<wstring>(new wstring(formattedFinal2)).castDown<EmptyType>();
            tmpDict[matchKeyOrigText] = SmartPtr<wstring>(new wstring(formattedFinalBeforeTextReplacements)).castDown<EmptyType>();
            tmpDict[matchKeyRange] = SmartPtr<Range>(new Range(r)).castDown<EmptyType>();
            
            tmpDict[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(flags)).castDown<EmptyType>();

            res->push_back(tmpDict);
        }

		// Add all derived types, if any
		if (derivedTypes != NULL) {
			for (int yy=0; yy<derivedTypes->size(); yy++) {
				res->push_back((*derivedTypes)[yy]);
			}
		}
		lastFragmentEnd = r.location + r.length - 1;

		startIndex = (actualMatchStartBytes + actualMatchLenBytes);
	}
//	[regex release];

	// Possibly add the last chars in the column as a fragment
	if (res != NULL) {
		int lenLeftOver = (int)input.length() - (lastFragmentEnd + 1);
		if ((lenLeftOver > 1) || ((lenLeftOver == 1) && (input[lastFragmentEnd+1] != '\n'))) {
			Range r = Range(lastFragmentEnd+1,((int)input.length()-1)-(lastFragmentEnd+1)+1);
			// Trim possible '\n' at the end
			if (input[r.location+r.length-1] == '\n') {
				r.length = r.length - 1;
			}

        {
			MatchPtr tmpDict(true);
			tmpDict[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
			tmpDict[matchKeyText] = SmartPtr<wstring>(new wstring(input.substr(r.location, r.length))).castDown<EmptyType>();
            if (gotText2)
               tmpDict[matchKeyText2] = SmartPtr<wstring>(new wstring(input2.substr(r.location, r.length))).castDown<EmptyType>();
            tmpDict[matchKeyRange] = SmartPtr<Range>(new Range(r)).castDown<EmptyType>();

            res->push_back(tmpDict);
        }

		}
	}

	if (res != NULL) {
#if DEBUG_REGEX
		stringstream tmpBuffer;
		for (int i=0; i<res->size(); i++) {
			tmpBuffer << "[" << toUTF8(getStringFromMatch((*res)[i],matchKeyText)) << "] [type=";
			tmpBuffer << OCRGlobalResults::stringForOCRElementType(getElementTypeFromMatch((*res)[i],matchKeyElementType)) << "] ";
		}
		RegexLog("RegexUtilsParseColumnForType: returning %d fragments: %s", (unsigned int)res->size(), tmpBuffer.str().c_str());
#endif
		*resPtr = res;
	}

	return (lastFragmentEnd > 0);
} 	// RegexUtilsParseColumnForType

// Applies a given pattern to the yet unmatched fragments in an array, updating the array if if is able to match one of these to the given pattern
void RegexUtilsParseFragmentsWithPattern (
			MatchVectorPtr fragments,
			OCRElementType type,
			OCRElementType subType,
			string regexPattern,
			int extractGroup,
			int patternMinLen,
			vector<GroupType> groups,
			MatchVectorPtr* elements,
			OCRResults *curResults
			) {

	bool matched;
	for (int i=0; i<fragments->size(); i++) {
		MatchPtr currentSubFragment = (*fragments)[i];
		if (getElementTypeFromMatch(currentSubFragment, matchKeyElementType) == OCRElementTypeUnknown) {
			wstring& input = getStringFromMatch(currentSubFragment, matchKeyText);
            wstring input2 = wstring();
            if (currentSubFragment.isExists(matchKeyText2))
                input2 = getStringFromMatch(currentSubFragment, matchKeyText2);
#if DEBUG
            if ((input == L"1 OB GLIDE CMFT FLS 43.7 2.64T SWED 50X 2.65") && (type = OCRElementTypePrice)) {
                DebugLog("");
            }
#endif
			if (input.length() >= patternMinLen) {
#if SHOW_REGEX
				RegexLog("RegexUtilsParseFragmentsWithPattern: testing strong type [%s] with pattern [%s] on text [%s]",
				 OCRResults::stringForOCRElementType(type).c_str(), regexPattern.c_str(), toUTF8(input).c_str());
#else
				RegexLog("RegexUtilsParseFragmentsWithPattern: testing strong type [%s] on text [%s]",
				 OCRResults::stringForOCRElementType(type).c_str(), toUTF8(input).c_str());
#endif

//#if DEBUG
//                if ((type == OCRElementTypePrice) && (input == L".S8.00")) {
//                    DebugLog("");
//                }
//#endif

				MatchVectorPtr tmpRes ((MatchVector *)NULL); //PQ leaks
				matched = RegexUtilsParseColumnForType(
						regexPattern,
						type,
						subType,
						groups,
						extractGroup,
						input,
                        input2,
						&tmpRes,
						elements,
						curResults //curResults
						);

				if (matched) {
					RegexLog("RegexUtilsParseFragmentsWithPattern: got match(es):%s", (matched? "YES":"NO"));
					Range currentSubFragmentRange = *currentSubFragment[matchKeyRange].castDown<Range>();
					// Replace current item in fragments with all found fragments
					fragments->erase(fragments->begin() + i);
					for (int j=0; j<tmpRes->size(); j++) {
						MatchPtr d = (*tmpRes)[j];
						// Adjust range of match within sub-fragment
						SmartPtr<Range> rangeObject = d[matchKeyRange].castDown<Range>();
						if (rangeObject != NULL) {
							Range r = *rangeObject;
							r.location += currentSubFragmentRange.location;
							d[matchKeyRange] = SmartPtr<Range>(new Range(r)).castDown<EmptyType>();
						}
						fragments->insert(fragments->begin()+i+j, d);
					}
					i+=(tmpRes->size()- 1);
				}
			}
		}
	}
} // RegexUtilsParseFragmentsWithPattern

MatchVectorPtr RegexUtilsParseFragmentIntoTypes(wstring& inputFragment, wstring &inputFragment2, OCRResults *curResults, MatchPtr d) {

	string textRegex;
    MatchVectorPtr derivedTypes;
    vector<GroupType> tmpGroups;
    int groupID = 1;
    const char *marker;
    size_t locMarker;

	// Before we do anything, perform substitutions common to all types
	// TODO: this can totally mess up the correspondence between text and coords - at best is yield an incorrect highlight, at worse illegal rects
    // At least try to set OrigText before the call below
    MatchVectorPtr res(new MatchVector);
	MatchPtr tmpDict(true);
    tmpDict[matchKeyOrigText] = SmartPtr<wstring>(new wstring(inputFragment)).castDown<EmptyType>();
    //Pass rects along in case auto-correct does replacements which can easily drive updates to rects (e.g. strip leading or trailing)
    // PQTODO deal with below call later
    // RegexUtilsAutoCorrect(inputFragment, curResults, OCRElementTypeUnknown, OCRElementTypeInvalid, false, true);

	tmpDict[matchKeyText] = SmartPtr<wstring>(new wstring(inputFragment)).castDown<EmptyType>();
    if (inputFragment2.length() == inputFragment.length())
        tmpDict[matchKeyText2] = SmartPtr<wstring>(new wstring(inputFragment2)).castDown<EmptyType>();
	tmpDict[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
	tmpDict[matchKeyRange] = SmartPtr<Range>(new Range(0, (int)inputFragment.length())).castDown<EmptyType>();
	res->push_back(tmpDict);

	// First we try to parse around "strong" types, then we will try to match the remaining fragments into types
    // At this time we have no such types: each fragment is expected to match entirely to one type

    if (!curResults->retailerParams.noProductNumbers) {
        textRegex = PRODUCT_NUMBER_REGEX;
        tmpGroups.clear();
        groupID = 1;
        tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        // Set the actual number of digits per product number
        locMarker = textRegex.find("[%d]");
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length()+10); // 10 is plenty to cover the longest string we will stick in there, which is so far "9,11" for BBBY not to mention we remove the marker itself.
            if ((curResults->retailerParams.minProductNumberLen != 0) && (curResults->retailerParams.maxProductNumberLen != 0) && (curResults->retailerParams.maxProductNumberLen > curResults->retailerParams.minProductNumberLen))
                sprintf(tmpS, "%s%d,%d%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.minProductNumberLen, curResults->retailerParams.maxProductNumberLen, textRegex.substr(locMarker+4, string::npos).c_str());
            else
                sprintf(tmpS, "%s%d%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productNumberLen, textRegex.substr(locMarker+4, string::npos).c_str());
            textRegex  = string(tmpS);
        }
        // Set the optional %charsToIgnoreAfterProduct element (or omit if retailer doesn't have these)
        marker = "%charsToIgnoreAfterProduct";
        locMarker = textRegex.find(marker);
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            if ((curResults->retailerParams.charsToIgnoreAfterProductRegex != NULL) && (strlen(curResults->retailerParams.charsToIgnoreAfterProductRegex) > 0)) {
                // Replace the marker with the retailer extra chars regex enclosed in "()?"
                char * tmpS = (char *)malloc(textRegex.length() + 1 + strlen(curResults->retailerParams.charsToIgnoreAfterProductRegex) + 2 - strlen(marker) + 1);
                sprintf(tmpS, "%s(%s)?%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.charsToIgnoreAfterProductRegex, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
                textRegex  = string(tmpS);
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeUnknown, groupOptionsDeleteOnly));
            } else {
                char * tmpS = (char *)malloc(textRegex.length() - strlen(marker) + 1);
                sprintf(tmpS, "%s%s", textRegex.substr(0,locMarker).c_str(), textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
                textRegex  = string(tmpS);
            }
        }
        // Set the optional markerAndPriceOnNextLine element (or omit if retailer doesn't have these)
        locMarker = textRegex.find("%markerAndPriceOnNextLine");
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            if (curResults->retailerParams.hasProductsWithMarkerAndPriceOnNextLine) {
                char * tmpS = (char *)malloc(textRegex.length() + strlen(curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex) - strlen("%markerAndPriceOnNextLine") + 5 + 1);
                sprintf(tmpS, "%s(%s)?%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex, textRegex.substr(locMarker+strlen("%markerAndPriceOnNextLine"), string::npos).c_str());
                textRegex  = string(tmpS);
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeMarkerOfPriceOnNextLine, groupOptionsDontCopy)); // Leave the marker, but change the type!
            } else {
                char * tmpS = (char *)malloc(textRegex.length() - strlen("%markerAndPriceOnNextLine") + 1);
                sprintf(tmpS, "%s%s", textRegex.substr(0,locMarker).c_str(), textRegex.substr(locMarker+strlen("%markerAndPriceOnNextLine"), string::npos).c_str());
                textRegex  = string(tmpS);
            }
        }
        tmpGroups.push_back(GroupType(groupID, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        

    #if SHOW_REGEX
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRODUCT_NUMBER] [%s]", textRegex.c_str());
    #else
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRODUCT_NUMBER]");
    #endif

    #if DEBUG
        if (inputFragment == L"6537 RED CEOAR 11000 TRELLIS CU 39.98")
            DebugLog("");
    #endif

        RegexUtilsParseFragmentsWithPattern(
                                            res,							// fragments
                                            OCRElementTypeProductNumber,	// type
                                            OCRElementTypeUnknown,			// sub-type
                                            textRegex,						// regexPattern,
                                            0,								// Extract group
                                            min(curResults->retailerParams.productNumberLen, curResults->retailerParams.minProductNumberLen),			// patternMinLen
                                            tmpGroups,
                                            &derivedTypes,
                                            curResults
                                            );
        tmpGroups.clear();

        if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
            RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
            return res;
        }
    } // look for product numbers
    
    if (strlen(curResults->retailerParams.productNumber2) > 0) {
    
        // e.g. Home Depot: "((?:[%0]){4}(?: )?(?:\\-)(?: )?)(?:(?:[%digit]){3})((?: )?(?:\\-)(?: )?)(?:(?:[%digit]){3})"
    	textRegex = PRODUCT_EXTERNAL_NUMBER_REGEX;
        tmpGroups.clear();
        int groupID = 1;
        tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly)); // Ignored chars before product number
        
        const char *marker = "[%externalproductregex]";
        size_t locMarker = textRegex.find(marker);
        if ((locMarker != string::npos) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length() + strlen(curResults->retailerParams.productNumber2) - strlen(marker) + 1);
            sprintf(tmpS, "%s%s%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productNumber2, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
            textRegex  = string(tmpS);
        }
        
        marker = "[%digit]";
        locMarker = textRegex.find(marker);
        while ((locMarker != string::npos) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length() + strlen(DIGIT) - strlen(marker) + 1);
            sprintf(tmpS, "%s%s%s", textRegex.substr(0,locMarker).c_str(), DIGIT, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
            textRegex  = string(tmpS);
            locMarker = textRegex.find(marker); // Repeat search until not found
        }
        
        marker = "[%zero]";
        const char *replacement = "0|o|O|@";
        locMarker = textRegex.find(marker);
        while ((locMarker != string::npos) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length() + strlen(DIGIT) - strlen(marker) + 1);
            sprintf(tmpS, "%s%s%s", textRegex.substr(0,locMarker).c_str(), replacement, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
            textRegex  = string(tmpS);
            locMarker = textRegex.find(marker); // Repeat search until not found
        }
        for (int kk=0; kk<strlen(curResults->retailerParams.productNumber2GroupActions); kk++) {
            char actionCh = curResults->retailerParams.productNumber2GroupActions[kk];
            if (actionCh == '0')
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
            else if (actionCh == '3')
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsCopyStartNonZero));
        }
        tmpGroups.push_back(GroupType(groupID, OCRElementTypeInvalid, groupOptionsDeleteOnly)); // Ignored chars after product number
        

#if SHOW_REGEX
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRODUCT_NUMBER2] [%s]", textRegex.c_str());
#else
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRODUCT_NUMBER2]");
#endif

        RegexUtilsParseFragmentsWithPattern(
                                            res,							// fragments
                                            OCRElementTypeProductNumber,	// type
                                            OCRElementTypeUnknown,			// sub-type
                                            textRegex,						// regexPattern,
                                            0,								// Extract group
                                            curResults->retailerParams.productNumberLen,			// patternMinLen
                                            tmpGroups,
                                            &derivedTypes,
                                            curResults
                                            );
        tmpGroups.clear();

        if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
            RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
            return res;
        }
    }
    
    
//#if DEBUG
//	if (RegexUtilsTestArrayForType(res, OCRElementTypeProductNumber)) {
//		DebugLog("RegexUtilsParseFragmentsWithPattern: found product number");
//        for (int k=0; k<res->size(); k++) {
//            MatchPtr d = (*res)[k];
//            DebugLog("res[%d]: %s", k, toUTF8(ocrparser::getStringFromMatch(d, matchKeyText)).c_str());
//        }
//        DebugLog("");
//	}
//#endif
    
    // Need to do the date regex first so that it prevents the time portion (e.g. 09:15 AM) from being parsed as a price
    if (curResults->retailerParams.dateCode != 0) {
        tmpGroups.clear();
        int minLen = DATE_REGEX_YY_HHMM_MIN_LEN;
        if (curResults->retailerParams.dateCode == DATE_REGEX_YY_HHMM_CODE) {
            textRegex = DATE_REGEX_YY_HHMM;
            minLen = DATE_REGEX_YY_HHMM_MIN_LEN;
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YYYY_AMPM_NOSECONDS;
            minLen = DATE_REGEX_YYYY_AMPM_NOSECONDS_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YYYY_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YYYY;
            minLen = DATE_REGEX_YYYY_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YY_HHMM_AMPM_OPTIONAL_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YY_HHMM_AMPM_OPTIONAL;
            minLen = DATE_REGEX_YY_HHMM_AMPM_OPTIONAL_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS;
            minLen = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_AMPM_NOSECONDS_HHMM_YY_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_AMPM_NOSECONDS_HHMM_YY;
            minLen = DATE_REGEX_AMPM_NOSECONDS_HHMM_YY_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YY_SLASHES_DASH_HHMM_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YY_SLASHES_DASH_HHMM;
            minLen = DATE_REGEX_YY_SLASHES_DASH_HHMM_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_MONTH_DD_COMMA_YYYY_CODE) {
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonthLetters, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_MONTH_DD_COMMA_YYYY;
            minLen = DATE_REGEX_MONTH_DD_COMMA_YYYY_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YY_SLASHES_HMM_AMPM_CODE) {
            // 05/06/15 9:15PM (Petco)
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YY_SLASHES_HMM_AMPM;
            minLen = DATE_REGEX_YY_SLASHES_HMM_AMPM_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YYYY_DASHES_HHMM_AMPM_CODE) {
            // 08/25/2011 08:39 PM (Petsmart)
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonthLetters, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_YYYY_DASHES_HHMM_AMPM;
            minLen = DATE_REGEX_YYYY_DASHES_HHMM_AMPM_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_YYYY_HHMMSS_CODE) {
            // 06/03/2016 20:33:48 (Trader Joe's)
            tmpGroups.push_back(GroupType(1, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(2, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(3, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(4, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(5, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(6, OCRElementTypeInvalid, groupOptionsDeleteOnly)); // Ignore seconds
            textRegex = DATE_REGEX_YYYY_HHMMSS;
            minLen = DATE_REGEX_YYYY_HHMMSS_MIN_LEN;
        } else if (curResults->retailerParams.dateCode == DATE_REGEX_GENERIC_CODE) {
            int groupID = 1;
            // Part 1: 06/03/2016 20:33:48 (Trader Joe's) or 03:51 PM (Target)
            tmpGroups.push_back(GroupType(groupID++, OCRElementSubTypeDateDatePrefix, groupOptionsDeleteAndSetFlag));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateMonth, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementSubTypeDateDateDivider, groupOptionsDeleteAndSetFlag));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateDay, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementSubTypeDateDateDivider, groupOptionsDeleteAndSetFlag));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateYear, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementSubTypeDateTimePrefix, groupOptionsDeleteAndSetFlag));
                // e.g. 20:33:48
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly)); // Ignore seconds
                // e.g. 03:51 PM
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateHour, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateMinutes, groupOptionsDeleteOnly));
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeDateAMPM, groupOptionsDeleteOnly));
            textRegex = DATE_REGEX_GENERIC;
            minLen = DATE_REGEX_GENERIC_MIN_LEN;
        } else {
            textRegex = DATE_REGEX_YYYY_AMPM_NOSECONDS;
            minLen = DATE_REGEX_YYYY_AMPM_NOSECONDS_MIN_LEN;
        }
        locMarker = textRegex.find("%digit");
        while (locMarker != string::npos) {
            textRegex = textRegex.substr(0,locMarker) + string(DIGIT) + textRegex.substr(locMarker+strlen("%digit"),string::npos);
            locMarker = textRegex.find("%digit");
        }
        
    #if DEBUG
        if (inputFragment == L"10/07/OB 2.21PM 293 02 2 04358") {
            DebugLog("");
        }
    #endif

    #if SHOW_REGEX
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [DATE] [%s]", textRegex.c_str());
    #else
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [DATE]");
    #endif

        RegexUtilsParseFragmentsWithPattern(
                                            res,					// fragments
                                            OCRElementTypeDate,		// type
                                            OCRElementTypeUnknown,	// sub-type
                                            textRegex,				// regexPattern,
                                            0,						// Extract group
                                            minLen,                 // patternMinLen
                                            tmpGroups,
                                            &derivedTypes,
                                            curResults
                                            );

        if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
            RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
            return res;
        }
    } // has date
    
    if (curResults->retailerParams.shorterProductNumberLen > 0) {
        textRegex = PRODUCT_NUMBER_REGEX;
        tmpGroups.clear();
        int groupID = 1;
        // First group catches ignored chars before product numbers
        tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        // Set the actual number of digits per product number
        size_t locMarker = textRegex.find("[%d]");
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length()+1); // OK to just use textRegex.length because actual number of digits is at most 99 (2 chars) versus the 2 chars we are replacing ("%d")
            sprintf(tmpS, "%s%d%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.shorterProductNumberLen, textRegex.substr(locMarker+4, string::npos).c_str());
            textRegex  = string(tmpS);
        }
        // Set the optional %charsToIgnoreAfterProduct element (or omit if retailer doesn't have these)
        const char *marker = "%charsToIgnoreAfterProduct";
        locMarker = textRegex.find(marker);
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            if ((curResults->retailerParams.charsToIgnoreAfterProductRegex != NULL) && (strlen(curResults->retailerParams.charsToIgnoreAfterProductRegex) > 0)) {
                // Replace the marker with the retailer extra chars regex enclosed in "()?"
                char * tmpS = (char *)malloc(textRegex.length() + 1 + strlen(curResults->retailerParams.charsToIgnoreAfterProductRegex) + 2 - strlen(marker) + 1);
                sprintf(tmpS, "%s(%s)?%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.charsToIgnoreAfterProductRegex, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
                textRegex  = string(tmpS);
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeUnknown, groupOptionsDeleteOnly));
            } else {
                char * tmpS = (char *)malloc(textRegex.length() - strlen(marker) + 1);
                sprintf(tmpS, "%s%s", textRegex.substr(0,locMarker).c_str(), textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
                textRegex  = string(tmpS);
            }
        }
        // Set the optional markerAndPriceOnNextLine element (or omit if retailer doesn't have these)
        marker = "%markerAndPriceOnNextLine";
        locMarker = textRegex.find(marker);
        if ((locMarker > 0) && (locMarker < textRegex.length())) {
            if (curResults->retailerParams.hasProductsWithMarkerAndPriceOnNextLine) {
                char * tmpS = (char *)malloc(textRegex.length() + strlen(curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex) - strlen(marker) + 5 + 1);
                sprintf(tmpS, "%s(%s)?%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
                textRegex  = string(tmpS);
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeMarkerOfPriceOnNextLine, groupOptionsDontCopy)); // Leave the marker, but change the type!
            } else {
                char * tmpS = (char *)malloc(textRegex.length() - strlen("%markerAndPriceOnNextLine") + 1);
                sprintf(tmpS, "%s%s", textRegex.substr(0,locMarker).c_str(), textRegex.substr(locMarker+strlen("%markerAndPriceOnNextLine"), string::npos).c_str());
                textRegex  = string(tmpS);
            }
        }
        tmpGroups.push_back(GroupType(groupID, OCRElementTypeInvalid, groupOptionsDeleteOnly));
    

#if SHOW_REGEX
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [SHORTER_PRODUCT_NUMBER] [%s]", textRegex.c_str());
#else
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [SHORTER_PRODUCT_NUMBER]");
#endif

        RegexUtilsParseFragmentsWithPattern(
                                            res,							// fragments
                                            (curResults->retailerParams.shorterProductsValid? OCRElementTypeProductNumber:OCRElementTypeIgnoredProductNumber),	// type
                                            OCRElementTypeUnknown,			// sub-type
                                            textRegex,						// regexPattern,
                                            0,								// Extract group
                                            curResults->retailerParams.shorterProductNumberLen,			// patternMinLen
                                            tmpGroups,
                                            &derivedTypes,
                                            curResults
                                            );
        tmpGroups.clear();

        if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
            RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
            return res;
        }
    } // shorterProductNumber exists
    
    textRegex = PRICE_REGEX;
    tmpGroups.clear();
    groupID = 1;
    marker = "%price_core_regex";
    locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        if (curResults->retailerParams.noZeroBelowOneDollar) {
            textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX_SUB1 + textRegex.substr(locMarker+strlen(marker),string::npos);
        } else
            textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX + textRegex.substr(locMarker+strlen(marker),string::npos);
    }
    locMarker = textRegex.find("%s");
    if (locMarker != string::npos) {
        if (curResults->retailerParams.pricesHaveDollar) {
            textRegex = textRegex.substr(0,locMarker) + string(PRICE_DOLLAR_REGEX) + textRegex.substr(locMarker+2,string::npos);
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else {
            // For retailers not using $ sign, accept it anyhow (optional) but only '$' (not substitutes). Help us get the price of TOTAL on Target receipts, see https://www.pivotaltracker.com/n/projects/1118860/stories/109482468
            textRegex = textRegex.substr(0,locMarker) + string("(\\$)?") + textRegex.substr(locMarker+2,string::npos);
            tmpGroups.push_back(GroupType(groupID++, OCRElementSubTypeDollar, groupOptionsDeleteAndSetFlag));
        }
    }
    marker = "%extracharsbeforepricesregex";
    locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        // If retailer has discounts but nothing precedes discounts, it means the '-' follows prices, and therefore we can accept prices with leading '-''s (assumed noise)
        if (curResults->retailerParams.hasDiscounts && ((curResults->retailerParams.discountStartingRegex == NULL) || (strlen(curResults->retailerParams.discountStartingRegex) == 0))) {
            textRegex = textRegex.substr(0,locMarker) + "([\\-]+)?" + textRegex.substr(locMarker+strlen(marker),string::npos);
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen(marker),string::npos);
    }
    marker = "%extracharsafterpricesregex";
    locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        if ((curResults->retailerParams.hasExtraCharsAfterPrices) && (curResults->retailerParams.extraCharsAfterPricesRegex != NULL)) {
            textRegex = textRegex.substr(0,locMarker) + "(" + curResults->retailerParams.extraCharsAfterPricesRegex + ")?" + textRegex.substr(locMarker+strlen(marker),string::npos);
            if (curResults->retailerParams.extraCharsAfterPricesMeansProductPrice)
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeProductPrice, groupOptionsDeleteAndTagAsGroupType));
            else
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen(marker),string::npos);
    }

#if SHOW_REGEX
	RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE] [%s]", textRegex.c_str());
#else
	RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE]");
#endif

#if DEBUG
    if (inputFragment == L"1.00-MFGC") {
        DebugLog("");
    }
#endif

	RegexUtilsParseFragmentsWithPattern(
										res,					// fragments
										OCRElementTypePrice,	// type
										OCRElementTypeUnknown,	// sub-type
										textRegex,				// regexPattern,
										0,						// Extract group
										PRICE_MIN_LEN,			// patternMinLen
										tmpGroups,
										&derivedTypes,
										curResults
										);
    tmpGroups.clear();
    groupID = 1;
	if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
		RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
		return res;
	}
    
    if (curResults->retailerParams.hasDiscounts) {
        // Either a prefix is defined or a postfix, can't have a discount with nothing different that a price, e.g. "2.50"!
        
        // Is there a discount prefix defined?
        if ((curResults->retailerParams.discountStartingRegex != NULL) && (strlen(curResults->retailerParams.discountStartingRegex) > 0)) {
            bool hasDiscountPrefix = false;
            textRegex = PRICE_DISCOUNT_REGEX;
            const char *marker = "%price_core_regex";
            locMarker = textRegex.find(marker);
            if (locMarker != string::npos) {
                if (curResults->retailerParams.noZeroBelowOneDollar) {
                    textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX_SUB1 + textRegex.substr(locMarker+strlen(marker),string::npos);
                } else
                    textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX + textRegex.substr(locMarker+strlen(marker),string::npos);
            }
            locMarker = textRegex.find("%s");
            if (locMarker != string::npos) {
                if (curResults->retailerParams.pricesHaveDollar) {
                    textRegex = textRegex.substr(0,locMarker) + string(PRICE_DOLLAR_REGEX) + textRegex.substr(locMarker+2,string::npos);
                    tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
                } else
                    textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+2,string::npos);
            }
            locMarker = textRegex.find("%discountEndingRegex");
            if (locMarker != string::npos) {
                // Remove the marker, there is no postfix part
                textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen("%discountEndingRegex"),string::npos);
            }
            locMarker = textRegex.find("%discountStartingRegex");
            if (locMarker != string::npos) {
                if (((curResults->retailerParams.discountStartingRegex != NULL) && (strlen(curResults->retailerParams.discountStartingRegex) > 0))) {
                    textRegex = textRegex.substr(0,locMarker) + "(" + string(curResults->retailerParams.discountStartingRegex) + ")" +textRegex.substr(locMarker+strlen("%discountStartingRegex"),string::npos);
                    hasDiscountPrefix = true;
                }
            }

            if (hasDiscountPrefix) {
#if SHOW_REGEX
                RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE_DISCOUNT] [%s] on [%s]", textRegex.c_str(), toUTF8(inputFragment).c_str());
#else
                RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE_DISCOUNT] on [%s]", toUTF8(inputFragment).c_str());
#endif
            
//#if DEBUG
//                if (inputFragment == L"379.OO-X") {
//                    DebugLog("");
//                }
//#endif

                tmpGroups.clear();
                int groupIndex = 1;
                tmpGroups.push_back(GroupType(groupIndex++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
                RegexUtilsParseFragmentsWithPattern(
                    res,					// fragments
                    OCRElementTypePotentialPriceDiscount,	// type
                    OCRElementTypeUnknown,	// sub-type
                    textRegex,				// regexPattern,
                    0,						// Extract group
                    PRICE_MIN_LEN,			// patternMinLen
                    tmpGroups,
                    &derivedTypes,
                    curResults
                    );
                tmpGroups.clear();

                if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
                    RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
                    return res;
                }
            } // found discount prefix
        } // discount prefix set
        
        if ((curResults->retailerParams.discountEndingRegex != NULL) && (strlen(curResults->retailerParams.discountEndingRegex) > 0)) {
            bool hasDiscountPostFix = false;
            textRegex = PRICE_DISCOUNT_REGEX;
            const char *marker = "%price_core_regex";
            locMarker = textRegex.find(marker);
            if (locMarker != string::npos) {
                if (curResults->retailerParams.noZeroBelowOneDollar) {
                    textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX_SUB1 + textRegex.substr(locMarker+strlen(marker),string::npos);
                } else
                    textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX + textRegex.substr(locMarker+strlen(marker),string::npos);
            }
            locMarker = textRegex.find("%s");
            if (locMarker != string::npos) {
                if (curResults->retailerParams.pricesHaveDollar) {
                    textRegex = textRegex.substr(0,locMarker) + string(PRICE_DOLLAR_REGEX) + textRegex.substr(locMarker+2,string::npos);
                    tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
                } else
                    textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+2,string::npos);
            }
            locMarker = textRegex.find("%discountEndingRegex");
            if (locMarker != string::npos) {
                if (((curResults->retailerParams.discountEndingRegex != NULL) && (strlen(curResults->retailerParams.discountEndingRegex) > 0))) {
                    textRegex = textRegex.substr(0,locMarker) + "(" + string(curResults->retailerParams.discountEndingRegex) + ")" +textRegex.substr(locMarker+strlen("%discountEndingRegex"),string::npos);
                    hasDiscountPostFix = true;
                }
            }
            locMarker = textRegex.find("%discountStartingRegex");
            if (locMarker != string::npos) {
                // Just remove the marker
                textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen("%discountStartingRegex"),string::npos);
            }

            if (hasDiscountPostFix) {
#if SHOW_REGEX
                RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE_DISCOUNT] [%s] on [%s]", textRegex.c_str(), toUTF8(inputFragment).c_str());
#else
                RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PRICE_DISCOUNT] on [%s]", toUTF8(inputFragment).c_str());
#endif
            
//#if DEBUG
//              if (inputFragment == L"HURST - 817-282-8808") {
//                  DebugLog("");
//              }
//#endif

                tmpGroups.clear();
                int groupIndex = 1;
                tmpGroups.push_back(GroupType(groupIndex++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
                RegexUtilsParseFragmentsWithPattern(
                        res,					// fragments
                        OCRElementTypePotentialPriceDiscount,	// type
                        OCRElementTypeUnknown,	// sub-type
                        textRegex,				// regexPattern,
                        0,						// Extract group
                        PRICE_MIN_LEN,			// patternMinLen
                        tmpGroups,
                        &derivedTypes,
                        curResults
                        );
                tmpGroups.clear();

                if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
                    RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
                    return res;
                }
            } // found discount postfix
        } // discount postfix set
    } // has discounts
    
    if ((curResults->retailerParams.hasPhone) && (curResults->retailerParams.phoneRegex != NULL)) {
        textRegex = curResults->retailerParams.phoneRegex;
        tmpGroups.clear();
        
        marker = "[%digit]";
        locMarker = textRegex.find(marker);
        while ((locMarker != string::npos) && (locMarker < textRegex.length())) {
            char * tmpS = (char *)malloc(textRegex.length() + strlen(DIGIT) - strlen(marker) + 1);
            sprintf(tmpS, "%s%s%s", textRegex.substr(0,locMarker).c_str(), DIGIT, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
            textRegex  = string(tmpS);
            locMarker = textRegex.find(marker); // Repeat search until not found
        }

#if DEBUG
      if (inputFragment == L"HURST - 817-282-8808") {
          DebugLog("");
      }
#endif

#if SHOW_REGEX
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PHONE] [%s]", textRegex.c_str());
#else
        RegexLog("RegexUtilsParseFragmentIntoTypes: Testing [PHONE]");
#endif

        RegexUtilsParseFragmentsWithPattern(
                                            res,							// fragments
                                            OCRElementTypePhoneNumber,	// type
                                            OCRElementTypeUnknown,			// sub-type
                                            textRegex,						// regexPattern,
                                            0,								// Extract group
                                            12,			// patternMinLen
                                            tmpGroups,
                                            &derivedTypes,
                                            curResults
                                            );
        tmpGroups.clear();

        if (!RegexUtilsTestArrayForType(res, OCRElementTypeUnknown)) {
            RegexLog("RegexUtilsParseFragmentsWithPattern: done, all %d fragments in input have assigned types", (unsigned short)res->size());
            return res;
        }
    } // phone regex defined
    
	return res;
} // RegexUtilsParseFragmentIntoTypes

OCRElementType RegexUtilsSearchForType(wstring& input, OCRResults* curResults, const std::set<OCRElementType>& excludeList, MatchVectorPtr* elements) {
	RegexLog("SearchForType: got text [%ls]", input.c_str());

	string textRegex;

	// Before we do anything, perform substitutions common to all types
	// Now done on entire fragment
	//RegexUtilsAutoCorrect(input, OCRElementTypeUnknown, OCRElementTypeInvalid, false, true);

    // Start by stripping leading and trailing blanks, they are never any good to anyone
    // PQTODO handle later
    // RegexUtilsStripLeading(input, L" ");
    // RegexUtilsStripTrailing(input, L" ");

	RegexLog("SearchForType invoked on [%s]", toUTF8(input).c_str());

	RegexLog("SearchForType: Testing for product number on text [%s]", toUTF8(input).c_str());
	OCRElementType tp = testForProductNumber(input, curResults);
	if (tp != OCRElementTypeUnknown)
		return tp;
    
	RegexLog("SearchForType: Testing for price on text [%s]", toUTF8(input).c_str());
	tp = testForPrice(input, curResults);
	if (tp != OCRElementTypeUnknown)
		return tp;

	// Call auto-correct anyhow, this will for example strip trailing blanks
    // PQTODO handle the below call later
	// RegexUtilsAutoCorrect(input, curResults, OCRElementTypeUnknown, OCRElementTypeInvalid, false, true);

	int blanks=0, nonBlanksDigitsOrLetters=0, digits=0, letters=0, longestSequence=0, currentSequence=0;
	for (int i=0; i<input.length(); i++) {
		wchar_t ch = input[i];
		if (ch == ' ') {
			blanks++;
			currentSequence=0;
		} else if (isDigit(ch)) {
			digits++;
			if (++currentSequence > longestSequence)
				longestSequence = currentSequence;
		} else if (isLetter(ch)) {
			letters++;
			if (++currentSequence > longestSequence)
				longestSequence = currentSequence;
		} else {
			nonBlanksDigitsOrLetters++;
		}
	}

	// Require at least 3 non-blanks chars
	// We only deal here with items not matching anything
    int minValidChars = 3;
	if (((letters + digits) < minValidChars)
		// Require more digits & letters than other (punctuation) chars
		|| (nonBlanksDigitsOrLetters > (digits + letters)))
	{
		DebugLog("RegexUtilsSearchForType: eliminating [%s], not enough letters or digits", toUTF8(input).c_str());
		return (OCRElementTypeInvalid);
	}

	return (OCRElementTypeUnknown);
}

OCRElementType testForProductNumber(wstring& input, OCRResults *curResults) {
    
	if (input.length() < curResults->retailerParams.productNumberLen)
        return OCRElementTypeUnknown;

    OCRElementType tp = OCRElementTypeProductNumber;

    string textRegex = PRODUCT_NUMBER_REGEX;
    vector<GroupType> tmpGroups;

    int groupID = 1;
    tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
    // Set the actual number of digits per product number
    size_t locMarker = textRegex.find("[%d]");
    if ((locMarker > 0) && (locMarker < textRegex.length())) {
        char * tmpS = (char *)malloc(textRegex.length()+10); // 10 is plenty to cover the longest string we will stick in there, which is so far "9,11" for BBBY not to mention we remove the marker itself.
        if ((curResults->retailerParams.minProductNumberLen != 0) && (curResults->retailerParams.maxProductNumberLen != 0) && (curResults->retailerParams.maxProductNumberLen > curResults->retailerParams.minProductNumberLen))
            sprintf(tmpS, "%s%d,%d%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.minProductNumberLen, curResults->retailerParams.maxProductNumberLen, textRegex.substr(locMarker+4, string::npos).c_str());
        else
            sprintf(tmpS, "%s%d%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productNumberLen, textRegex.substr(locMarker+4, string::npos).c_str());
        textRegex  = string(tmpS);
    }
    // Set the optional markerAndPriceOnNextLine element (or omit if retailer doesn't have these)
    locMarker = textRegex.find("%markerAndPriceOnNextLine");
    if ((locMarker > 0) && (locMarker < textRegex.length())) {
        if (curResults->retailerParams.hasProductsWithMarkerAndPriceOnNextLine) {
            char * tmpS = (char *)malloc(textRegex.length() + strlen(curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex) - strlen("%markerAndPriceOnNextLine") + 5 + 1);
            sprintf(tmpS, "%s(%s)?%s", textRegex.substr(0,locMarker).c_str(), curResults->retailerParams.productsWithMarkerAndPriceOnNextLineRegex, textRegex.substr(locMarker+strlen("%markerAndPriceOnNextLine"), string::npos).c_str());
            textRegex  = string(tmpS);
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeMarkerOfPriceOnNextLine, groupOptionsDontCopy)); // Leave the marker, but change the type!
        } else {
            char * tmpS = (char *)malloc(textRegex.length() - strlen("%markerAndPriceOnNextLine") + 1);
            sprintf(tmpS, "%s%s", textRegex.substr(0,locMarker).c_str(), textRegex.substr(locMarker+strlen("%markerAndPriceOnNextLine"), string::npos).c_str());
            textRegex  = string(tmpS);
        }
    }
    tmpGroups.push_back(GroupType(groupID, OCRElementTypeInvalid, groupOptionsDeleteOnly));

#if SHOW_REGEX
    DebugLog("testForProductNumber: Testing [PRODUCT_NUMBER] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForProductNumber: Testing [PRODUCT_NUMBER] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                            0,
                            input,
                            false,	// substitute
                            tmpGroups,
                            NULL,
                            NULL);

    RegexLog("testForProductNumber: got %d matches", n);

    if (n > 0) {
        // PQTODO handle below call later
        // RegexUtilsAutoCorrect(input, curResults, tp, OCRElementTypeInvalid, true, true);
        return tp;
    }
    
	return (OCRElementTypeUnknown);
} // testForProductNumber

bool testForCoupon(wstring& input, OCRResults *curResults) {

    if (!curResults->retailerParams.hasCoupons)
        return false;

    OCRElementType tp = OCRElementTypeCoupon;

    string textRegex = curResults->retailerParams.couponRegex;
    
    const char *marker = "%digit";
    size_t locMarker = textRegex.find(marker);
    if (locMarker != string::npos)
        textRegex = textRegex.substr(0,locMarker) + DIGIT + textRegex.substr(locMarker+strlen(marker),string::npos);

#if SHOW_REGEX
    DebugLog("testForCoupon: Testing [COUPON] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForCoupon: Testing [COUPON] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForCoupon: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForCoupon

bool testForSubtotal(wstring& input, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    if (!curResults->retailerParams.hasSubtotal)
        return false;

    OCRElementType tp = OCRElementTypeSubtotal;

    string textRegex = curResults->retailerParams.subtotalRegex;
    
    size_t locMarker = textRegex.find("%extracharsaftersubtotal");
    if (locMarker != string::npos) {
        if (strlen(curResults->retailerParams.extraCharsAfterSubtotalRegex) > 0) {
            textRegex = textRegex.substr(0,locMarker) + string(curResults->retailerParams.extraCharsAfterSubtotalRegex) + textRegex.substr(locMarker+strlen("%extracharsaftersubtotal"),string::npos);
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen("%extracharsaftersubtotal"),string::npos);
    }    

#if SHOW_REGEX
    DebugLog("testForSubtotal: Testing [SUBTOTAL] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForSubtotal: Testing [SUBTOTAL] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForSubTotal: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForSubtotal

bool testForPhoneNumber(wstring& input, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    if (!curResults->retailerParams.hasPhone || (curResults->retailerParams.phoneRegex == NULL))
        return false;

    OCRElementType tp = OCRElementTypePhoneNumber;

    string textRegex = curResults->retailerParams.phoneRegex;
    
    
    const char *marker = "[%digit]";
    size_t locMarker = textRegex.find(marker);
    while ((locMarker != string::npos) && (locMarker < textRegex.length())) {
        char * tmpS = (char *)malloc(textRegex.length() + strlen(DIGIT) - strlen(marker) + 1);
        sprintf(tmpS, "%s%s%s", textRegex.substr(0,locMarker).c_str(), DIGIT, textRegex.substr(locMarker+strlen(marker), string::npos).c_str());
        textRegex  = string(tmpS);
        locMarker = textRegex.find(marker); // Repeat search until not found
    }

#if SHOW_REGEX
    DebugLog("testForPhoneNumber: Testing [PHONE] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForPhoneNumber: Testing [PHONE] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForPhoneNumber: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForPhoneNumber


bool testForRegexPattern(wstring& input, const char *regex, OCRElementType tp, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    string textRegex = regex;

    DebugLog("testForRegexPattern: Testing [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());

    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForRegexPattern: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForRegexPattern

bool testForTotal(wstring& input, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    if (!curResults->retailerParams.hasTotal)
        return false;

    OCRElementType tp = OCRElementTypeTotal;

    string textRegex = curResults->retailerParams.totalRegex;

#if SHOW_REGEX
    DebugLog("testForTotal: Testing [TOTAL] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForTotal: Testing [TOTAL] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForTotal: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForTotal

bool testForBal(wstring& input, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    if (!curResults->retailerParams.hasTaxBalTotal || ((curResults->retailerParams.balRegex == NULL) || (strlen(curResults->retailerParams.balRegex) == 0)))
        return false;

    OCRElementType tp = OCRElementTypeBal;

    string textRegex = curResults->retailerParams.balRegex;

#if SHOW_REGEX
    DebugLog("testForTotal: Testing [BAL] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForTotal: Testing [BAL] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForTotal: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForBal

bool testForTax(wstring& input, OCRResults *curResults) {

//#if DEBUG
//    if (input == L"ISJBTOTAL") {
//        DebugLog("");
//    }
//#endif

    if (!curResults->retailerParams.hasTaxBalTotal || ((curResults->retailerParams.taxRegex == NULL) || (strlen(curResults->retailerParams.taxRegex) == 0)))
        return false;

    OCRElementType tp = OCRElementTypeTax;

    string textRegex = curResults->retailerParams.taxRegex;

#if SHOW_REGEX
    DebugLog("testForTotal: Testing [TAX] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForTotal: Testing [TAX] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                0,
                input,
                false,	// substitute
                vector<GroupType>(),
                NULL,
                NULL);

    RegexLog("testForTotal: got %d matches", n);

    if (n > 0) {
        return true;
    } else {
        return false;
    }
} // testForTax

OCRElementType testForPrice(wstring& input, OCRResults *curResults) {
    
	if (input.length() < PRICE_MIN_LEN)
        return OCRElementTypeUnknown;

    OCRElementType tp = OCRElementTypePrice;

    vector<GroupType> tmpGroups;
    int groupID = 1;
    string textRegex = PRICE_REGEX;
    const char *marker = "%price_core_regex";
    size_t locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        if (curResults->retailerParams.noZeroBelowOneDollar) {
            textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX_SUB1 + textRegex.substr(locMarker+strlen(marker),string::npos);
        } else
            textRegex = textRegex.substr(0,locMarker) + PRICE_CORE_REGEX + textRegex.substr(locMarker+strlen(marker),string::npos);
    }
    locMarker = textRegex.find("%s");
    if (locMarker != string::npos) {
        if (curResults->retailerParams.pricesHaveDollar) {
            textRegex = textRegex.substr(0,locMarker) + string(PRICE_DOLLAR_REGEX) + textRegex.substr(locMarker+2,string::npos);
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+2,string::npos);
    }
    marker = "%extracharsbeforepricesregex";
    locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        // If retailer has discounts but nothing precedes discounts, it means the '-' follows prices, and therefore we can accept prices with leading '-''s (assumed noise)
        if (curResults->retailerParams.hasDiscounts && ((curResults->retailerParams.discountStartingRegex == NULL) || (strlen(curResults->retailerParams.discountStartingRegex) == 0))) {
            textRegex = textRegex.substr(0,locMarker) + "([\\-]+)?" + textRegex.substr(locMarker+strlen(marker),string::npos);
            tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen(marker),string::npos);
    }
    marker = "%extracharsafterpricesregex";
    locMarker = textRegex.find(marker);
    if (locMarker != string::npos) {
        if (curResults->retailerParams.hasExtraCharsAfterPrices) {
            textRegex = textRegex.substr(0,locMarker) + "(" + curResults->retailerParams.extraCharsAfterPricesRegex + ")?" + textRegex.substr(locMarker+strlen(marker),string::npos);
            if (curResults->retailerParams.extraCharsAfterPricesMeansProductPrice)
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeProductPrice, groupOptionsDeleteAndTagAsGroupType));
            else
                tmpGroups.push_back(GroupType(groupID++, OCRElementTypeInvalid, groupOptionsDeleteOnly));
        } else
            textRegex = textRegex.substr(0,locMarker) + textRegex.substr(locMarker+strlen(marker),string::npos);
    }

#if SHOW_REGEX
    DebugLog("testForName: Testing [PRICE] [%s] on text [%s]", textRegex.c_str(), toUTF8(input).c_str());
#else
    RegexLog("testForName: Testing [PRICE] on text [%s]", toUTF8(input).c_str());
#endif
    int n = RegexUtilsNewApplyRegex(textRegex, tp,
                            0,
                            input,
                            false,	// substitute
                            tmpGroups,
                            NULL,
                            NULL);

    RegexLog("testForPrice: got %d matches", n);

    if (n > 0) {
        // PQTODO handle below call later
        // RegexUtilsAutoCorrect(input, curResults, tp, OCRElementTypeInvalid, true, true);
        return tp;
    }
    
	return (OCRElementTypeUnknown);
} // testForPrice

bool RegexUtilsValidateUPCCode(wstring &text) {
    if (text.length() != 12)
        return false;
    int oddSum = 0, evenSum = 0;
    for (int i=0; i<=10; i++) {
        wchar_t ch = text[i];
        int intValue = ch - '0';
        if ((i == 0) || (i == 2) || (i == 4) || (i == 6) || (i == 8) || (i == 10))
            oddSum += intValue;
        else
            evenSum += intValue;
    }
    int lastDigit = (oddSum * 3 + evenSum) % 10;
    // How much needs to be added to get to 10? That's the UPC check (12th) digit
    int checkDigit = (10 - lastDigit) % 10;
    if (checkDigit != (text[11] - '0')) {
        RegexLog("RegexUtilsValidateUPCCode: rejecting invalid UPC code [%s]", toUTF8(text).c_str());
        return false;
    } else {
        return true;
    }
}

wchar_t RegexUtilsDetermineMissingUPCDigits(wstring &text, int indexMissingDigit) {
    for (wchar_t ch='0'; ch<='9'; ch++) {
        wstring testText;
        if (indexMissingDigit == 0)
            testText = ch + text.substr(1, wstring::npos);
        else if (indexMissingDigit == 11)
            testText = text.substr(0, 11) + ch;
        else {
            testText = text.substr(0,indexMissingDigit) + ch + text.substr(indexMissingDigit+1, wstring::npos);
        }
        if (RegexUtilsValidateUPCCode(testText))
            return (ch);
    }
    return '\0';
}

wchar_t RegexUtilsDetermineMissingUPCDigits(OCRRectPtr r) {
    // First step is to determine if we have 11 digits around current char (not included) - that's the only way this char could make up a valid UPC code

    wstring productText;
    OCRRectPtr p = r->previous;
    int previousDigits = 0;
    while ((p != NULL) && isDigitLookalikeExtended(p->ch)) {
        productText = p->ch + productText;
        previousDigits++;
        p = p->previous;
    }
    // We got all digits prior - stick a '0' place holder for current char
    productText = productText + L'0';
    p = r->next;
    while ((p != NULL) && isDigit(p->ch)) {
        productText = productText + p->ch;
        p = p->next;
    }
    if (productText.length() != 12)
        return '\0';    // Can't be a valid UPC no matter what
    wstring formattedText = convertDigitsLookalikesToDigits(productText);

    return (RegexUtilsDetermineMissingUPCDigits(formattedText, previousDigits));
}

} // namespace ocrparser
