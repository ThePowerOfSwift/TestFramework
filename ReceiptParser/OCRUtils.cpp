//
//  OCRUtils.cpp
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

#define CODE_CPP 1

#include "ocr.hh"
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cmath>

#include "OCRClass.hh"
#include "OCRPage.hh"
#include "OCRUtils.hh"

// Class to analyze character images
#include "single_letter/slcommon.h"
#include "single_letter/cimage.h"
#include "single_letter/SingleLetterTests.h"

#define ABS abs
#define MAX max
#define MIN min

namespace ocrparser {

wstring charsWithAbnormalSpacing = L".,;:'\"! \t\n";    // Special characters between which spacing is unusual
wstring normalWidthLowercaseCharset = L"abcdeghknopqsuvxz";
wstring looksSameAsLowercaseCharset = L"CMOSUVWXZ";
wstring superNarrowCharset1 = L".|li:!'";
wstring superNarrowCharset15 = L";,`";
wstring tallBelowCharset = L"gjqpy";
wstring tallAboveCharset = L"bdfhiklt";

// U+00C7	Ç	c3 87	LATIN CAPITAL LETTER C WITH CEDILLA
// U+00D2	Ò	c3 92	LATIN CAPITAL LETTER O WITH GRAVE
// U+00D3	Ó	c3 93	LATIN CAPITAL LETTER O WITH ACUTE
// U+00D4	Ô	c3 94	LATIN CAPITAL LETTER O WITH CIRCUMFLEX
// U+00D5	Õ	c3 95	LATIN CAPITAL LETTER O WITH TILDE
// U+00D6	Ö	c3 96	LATIN CAPITAL LETTER O WITH DIAERESIS
// U+00D8	Ø	c3 98	LATIN CAPITAL LETTER O WITH STROKE
// U+00D9	Ù	c3 99	LATIN CAPITAL LETTER U WITH GRAVE
// U+00DA	Ú	c3 9a	LATIN CAPITAL LETTER U WITH ACUTE
// U+00DB	Û	c3 9b	LATIN CAPITAL LETTER U WITH CIRCUMFLEX
// U+00DC	Ü	c3 9c	LATIN CAPITAL LETTER U WITH DIAERESIS
// U+0106	Ć	c4 86	LATIN CAPITAL LETTER C WITH ACUTE
// U+0108	Ĉ	c4 88	LATIN CAPITAL LETTER C WITH CIRCUMFLEX
// U+010A	Ċ	c4 8a	LATIN CAPITAL LETTER C WITH DOT ABOVE
// U+010C	Č	c4 8c	LATIN CAPITAL LETTER C WITH CARON
// U+014C	Ō	c5 8c	LATIN CAPITAL LETTER O WITH MACRON
// U+014E	Ŏ	c5 8e	LATIN CAPITAL LETTER O WITH BREVE
// U+0150	Ő	c5 90	LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
// U+015A	Ś	c5 9a	LATIN CAPITAL LETTER S WITH ACUTE
// U+015C	Ŝ	c5 9c	LATIN CAPITAL LETTER S WITH CIRCUMFLEX
// U+015E	Ş	c5 9e	LATIN CAPITAL LETTER S WITH CEDILLA  
// U+0160	Š	c5 a0	LATIN CAPITAL LETTER S WITH CARON
// U+0168	Ũ	c5 a8	LATIN CAPITAL LETTER U WITH TILDE
// U+016A	Ū	c5 aa	LATIN CAPITAL LETTER U WITH MACRON
// U+016C	Ŭ	c5 ac	LATIN CAPITAL LETTER U WITH BREVE
// U+016E	Ů	c5 ae	LATIN CAPITAL LETTER U WITH RING ABOVE
// U+0170	Ű	c5 b0	LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
// U+0172	Ų	c5 b2	LATIN CAPITAL LETTER U WITH OGONEK
// U+0174	Ŵ	c5 b4	LATIN CAPITAL LETTER W WITH CIRCUMFLEX
// U+0179	Ź	c5 b9	LATIN CAPITAL LETTER Z WITH ACUTE
// U+017B	Ż	c5 bb	LATIN CAPITAL LETTER Z WITH DOT ABOVE
// U+017D	Ž	c5 bd	LATIN CAPITAL LETTER Z WITH CARON
// U+0182	Ƃ	c6 82	LATIN CAPITAL LETTER B WITH TOPBAR
// U+0184	Ƅ	c6 84	LATIN CAPITAL LETTER TONE SIX
// U+0187	Ƈ	c6 87	LATIN CAPITAL LETTER C WITH HOOK
// U+018B	Ƌ	c6 8b	LATIN CAPITAL LETTER D WITH TOPBAR
// U+01A2	Ƣ	c6 a2	LATIN CAPITAL LETTER OI
// U+01AF	Ư	c6 af	LATIN CAPITAL LETTER U WITH HORN
// U+01B5	Ƶ	c6 b5	LATIN CAPITAL LETTER Z WITH STROKE
// U+01B8	Ƹ	c6 b8	LATIN CAPITAL LETTER EZH REVERSED
// U+01BC	Ƽ	c6 bc	LATIN CAPITAL LETTER TONE FIVE

string looksSameAsLowercaseCharsetIntlUTF8 = "ÇÒÓÔÕÖØÙÚÛÜĆĈĊČŌŎŐŚŞŜŠŨŪŬŮŰŲŴŹŻƂƄƇƋƢƯƵƸƼ";
wstring looksSameAsLowercaseCharsetIntlUTF32;

wstring looksSameAsUppercaseCharset = L"cmosuvwxz";
wstring looksSameAsUppercaseCharsetIntlUTF32;

wstring letteriEquivUTF32;
string letteriEquivUTF8 = "ìíîï¦ĩīĭįıįļ";

// U+00D9	Ù	c3 99	LATIN CAPITAL LETTER U WITH GRAVE
// U+00DA	Ú	c3 9a	LATIN CAPITAL LETTER U WITH ACUTE
// U+00DB	Û	c3 9b	LATIN CAPITAL LETTER U WITH CIRCUMFLEX
// U+00DC	Ü	c3 9c	LATIN CAPITAL LETTER U WITH DIAERESIS
// U+00F9	ù	c3 b9	LATIN SMALL LETTER U WITH GRAVE
// U+00FA	ú	c3 ba	LATIN SMALL LETTER U WITH ACUTE
// U+00FB	û	c3 bb	LATIN SMALL LETTER U WITH CIRCUMFLEX
// U+00FC	ü	c3 bc	LATIN SMALL LETTER U WITH DIAERESIS

wstring letterUEquivUTF32;
string letterUEquivUTF8 = "UuÙÚÛÜùúûü";

// U+013C	ļ	c4 bc	LATIN SMALL LETTER L WITH CEDILLA
// U+013E	ľ	c4 be	LATIN SMALL LETTER L WITH CARON
// U+0140	ŀ	c5 80	LATIN SMALL LETTER L WITH MIDDLE DOT
// U+0142	ł	c5 82	LATIN SMALL LETTER L WITH STROKE
// U+0163	ţ	c5 a3	LATIN SMALL LETTER T WITH CEDILLA
// U+017F	ſ	c5 bf	LATIN SMALL LETTER LONG S
// U+019A	ƚ	c6 9a	LATIN SMALL LETTER L WITH BAR
// U+021B	ț	c8 9b	LATIN SMALL LETTER T WITH COMMA BELOW
string letteriCaseSensitiveEquivUTF8 = "ļľŀłţſƚț";
wstring letteriCaseSensitiveEquivUTF32;

wstring normalHeightNonAlpha = L"|/()!{}[]?";

wstring englishVowels = L"aeiouy";
static string extendedVowelsUTF8 = "àáâãäåæèéêëìíîïðòóôõöùúûüýýāăąēĕėęěĩīĭįıōŏőœũūŭůűųŷơƣǐǒǔǖǘǚǜǝǟǡǣǫǭǻǽƍȁȃȅȇȉȋȍȏȕȗȧȩȫȭȯȱȳɑɐɘɵ";
static wstring extendedVowelsUTF32;

static string digitsLookalikesUTF8 = "iI!Oo@lGZsS$Q";
static wstring digitsLookalikesUTF32;

bool isLetter(wchar_t ch) {
	if ( ((ch>='a')&&(ch<='z')) || ((ch>='A')&&(ch<='Z')))
		return true;
	if ( ((ch >= 0x00c0) && (ch <= 0x02af))
		|| ((ch >= 0x0386) && (ch <= 0x0556))
		|| ((ch >= 0x048a) && (ch <= 0x0523))
		|| ((ch >= 0x0531) && (ch <= 0x0556))
		|| ((ch >= 0x0561) && (ch <= 0x0587))
		|| ((ch >= 0x05cf) && (ch <= 0x05ea)) ) {
		return true;
	}

	return false;
}

bool isDigit(wchar_t ch, bool lax) {
	if ((ch>='0') && (ch<='9'))
        return true;
    if (!lax)
        return false;
    if ((ch == 'O') || (ch == 'I') || (ch == '|'))
        return true;
    return false;
}

// 2016-04-11 no longer accepting '-' and '~' as decimal point (foiled the logic counting price sequences for "-2.B0")
static string decimalPointLookAlikesUTF8 = ".*";
static wstring decimalPointLookAlikesUTF32;

bool isDecimalPointLookalike(wchar_t ch) {
    if (decimalPointLookAlikesUTF32.length() == 0) {
        decimalPointLookAlikesUTF32 = toUTF32(decimalPointLookAlikesUTF8);
    }
    
    return (decimalPointLookAlikesUTF32.find(ch) != wstring::npos);
}

bool isDigitLookalike(wchar_t ch) {
    if (isDigit(ch))
        return true;

    if (digitsLookalikesUTF32.length() == 0) {
        digitsLookalikesUTF32 = toUTF32(digitsLookalikesUTF8);
    }
    
    return (digitsLookalikesUTF32.find(ch) != wstring::npos);
}


static string digitsLookalikesExtendedUTF8 = "iI!lBDOo@ZSs$JG/UQ";
static wstring digitsLookalikesExtendedUTF32;

wchar_t digitForExtendedLookalike(wchar_t ch) {
    if (isDigit(ch))
        return (ch);
    switch (ch) {
        case 'O':
        case 'D':
        case '0':
        case '@':
        case 'C':
        case 'U':
        case 'c':
        case 'J':
        case 'Q':
            return '0';
        case 'i':
        case 'I':
        case '!':
        case 'l':
            return '1';
        case 'Z':
            return '2';
        case 'S':
        case 's':
        case '$':
            return '5';
        case 'G':
            return '6';            
        case '/':
            return '7';
        case 'B':
            return '8';
        default:
            break;
    }

    return '\0';
}

bool isDigitLookalikeExtended(wchar_t ch) {
    if (isDigit(ch))
        return true;

    if (digitsLookalikesExtendedUTF32.length() == 0) {
        digitsLookalikesExtendedUTF32 = toUTF32(digitsLookalikesExtendedUTF8);
    }
    
    return (digitsLookalikesExtendedUTF32.find(ch) != wstring::npos);
}

bool isLetterOrDigit(wchar_t ch) {
	return (isLetter(ch) || isDigit(ch));
}

bool isDelimiter(wchar_t ch) {
	return ((ch==' ') || (ch == '-') || (ch == '.') || (ch == ',') || (ch == '#') || (ch == '/') || (ch == '|') || (ch == '~'));
}

bool isLower(wchar_t ch) {
	if (isLetter(ch) && (toLower(ch) == ch)) {
		return true;
	} else {
		return false;
	}
}

bool isUpper(wchar_t ch) {
	if (isLetter(ch) && (toUpper(ch) == ch)) {
		return true;
	} else {
		return false;
	}
}

bool isLike1 (wchar_t ch) {
	if ((ch == '1') || (ch == '|') || (ch == '\\') || (ch == 'I') || (ch == 'l'))
		return true;
	else
		return false;
}

bool isTallAbove(wchar_t ch) {
	return ((tallAboveCharset.find(ch) != wstring::npos));
}

bool isTallBelow(wchar_t ch) {
	return ((tallBelowCharset.find(ch) != wstring::npos));
}

bool extendsBelowLine(wchar_t ch) {
	return ((ch=='f') || (ch=='g') || (ch=='j') || (ch=='p') || (ch=='q') || (ch == 'y'));
}

bool isVowel(wchar_t inputChar) {
	wchar_t ch = toLower(inputChar);
	
    if (extendedVowelsUTF32.length() == 0) {
        extendedVowelsUTF32 = toUTF32(extendedVowelsUTF8);
    }
    
	if (!isLetter(ch)) 
		return false;
		
	// Optimize for regular English - first vowel above normal set is 'à'
    // U+00E0	à	c3 a0	LATIN SMALL LETTER A WITH GRAVE
	if (ch < 0x00E0)
		return (englishVowels.find(ch) != wstring::npos);
	else 
		return (extendedVowelsUTF32.find(ch) != wstring::npos);
} // isVowel

bool isConsonant(wchar_t inputChar) {
	if (!isLetter(inputChar)) {
		return false;
	} else {
		return (!isVowel(inputChar));
	}
}

int countConsonnantsAround (SmartPtr<OCRRect> r) {
	if (r == NULL)
		return 0;
		
	int cnt = 0;
		
	// Count consonnants before
	SmartPtr<OCRRect> tmpR = r->previous;
	while (tmpR != NULL) {
		if (isConsonant(tmpR->ch)) {
			cnt++;
			tmpR = tmpR->previous;
		} else {
			break;
		}
	}
	
	// Count consonnants after
	tmpR = r->next;
	while (tmpR != NULL) {
		if (isConsonant(tmpR->ch)) {
			cnt++;
			tmpR = tmpR->next;
		} else {
			break;
		}
	}
	
	return cnt;
}

// TODO add other international vertical letters such as the weird 'l's
bool isVerticalLine (wchar_t ch, OCRResults *results, bool strict) {
    if (letteriEquivUTF32.length() == 0) {
        letteriEquivUTF32 = toUTF32(letteriEquivUTF8);
    }
    if (letteriCaseSensitiveEquivUTF32.length() == 0) {
        letteriCaseSensitiveEquivUTF32 = toUTF32(letteriCaseSensitiveEquivUTF8);
    }
	if ((ch == '|') || (ch == '1') || (ch == 'l') || (ch == '!') || (ch == ']') || (ch == '[') || (ch == '(') || (ch == ')') || (ch == 'i') || (ch == 'I')
        // U+00A1	¡	c2 a1	INVERTED EXCLAMATION MARK
        || (ch == 0xa1)
		|| (letteriEquivUTF32.find(toLower(ch)) != wstring::npos)
        || (letteriCaseSensitiveEquivUTF32.find(ch) != wstring::npos)
		) {
		return true;
	}
    if ((!strict) && ((ch == '/') || (ch == '\\'))) {
        return true;
    }
    if ((results == NULL) || (results->languageCode != LanguageENG)) {
        // U+0131	ı	c4 b1	LATIN SMALL LETTER DOTLESS I
        if (ch == 0x131) {
            return true;
        }
    }
	return false;
}

// Used for the rule mapping things like "IE" to 'E' when glued. Don't treat 'I' as having a  vertical left side because it's not wide, i.e. different rules.
bool hasVerticalLeftSide(wchar_t ch) {
    if ((ch == 'B') || (ch == 'D') || (ch == 'E') || (ch == 'F') || (ch == 'H') || (ch == 'K') || (ch == 'L') || (ch == 'M') || (ch == 'N') || (ch == 'P') || (ch == 'R') || (ch == 'U'))
        return true;
    else
        return false;
}

bool hasVerticalRightSide(wchar_t ch) {
    if ((ch == 'H') || (ch == 'M') || (ch == 'N') || (ch == 'U'))
        return true;
    else
        return false;
}

wstring toLower(const wstring& text) {
	wstring res;
	for (int i=0; i<text.length(); i++) {
		res += toLower(text[i]);
	}
	return res;
}

wchar_t toLower(wchar_t ch) {
	if ((ch >= 'A') && (ch <= 'Z')) {
		return (wchar_t)(ch + 0x20);
	}

	if ( ((ch >= 0xc0) && (ch <= 0xd6))
		|| ((ch >= 0x00d8) && (ch <= 0x00de))
	   ) {
		return (wchar_t)(ch + 0x20);
	}

	if ( ((ch >= 0x100) && (ch <= 0x136))
		|| ((ch >= 0x13d) && (ch <= 0x147))
		|| ((ch >= 0x14a) && (ch <= 0x17d))
		|| (ch == 0x187)
		|| ((ch >= 0x1cd) && (ch <= 0x1db))
		|| ((ch >= 0x1de) && (ch <= 0x1ee))
		|| (ch == 0x1f4)
		|| ((ch >= 0x1f8) && (ch <= 0x232))
		|| (ch == 0x23b)
		|| (ch == 0x242)
		) {
		return (wchar_t)(ch + 0x1);
	}

	if (  (ch == 0x1c4)
		|| (ch == 0x1c7)
		|| (ch == 0x1c4)
		|| (ch == 0x1f1)
		) {
		return (wchar_t)(ch + 0x2);
	}

	return (ch);
}

wstring toUpper(const wstring& text) {
	wstring res;
	for (int i=0; i<text.length(); i++) {
		res += toUpper(text[i]);
	}
	return res;
}

// Custom implementation of uppercase conversion of lowercase Unicode characters. Apparently I decided there was no suitable function in the C++ run time?
wchar_t toUpper(wchar_t ch) {
    // Lowercase English letters
	if ((ch >= 'a') && (ch <= 'z')) {
		return (wchar_t)(ch - 0x20);
	}
    
    // Accented English lowercase letters
        // From:    U+00E0	à	c3 a0	LATIN SMALL LETTER A WITH GRAVE
        // To:      U+00F6	ö	c3 b6	LATIN SMALL LETTER O WITH DIAERESIS
	if ( ((ch >= 0xe0) && (ch <= 0xf6))
        // From:    U+00F8	ø	c3 b8	LATIN SMALL LETTER O WITH STROKE
        // To:      U+00FE	þ	c3 be	LATIN SMALL LETTER THORN
		|| ((ch >= 0x00f8) && (ch <= 0x00fe))
		) {
		return (wchar_t)(ch - 0x20);
	}

	if ( ((ch >= 0x101) && (ch <= 0x137))
		|| ((ch >= 0x13e) && (ch <= 0x148))
		|| ((ch >= 0x14b) && (ch <= 0x17e))
		|| (ch == 0x188)
		|| ((ch >= 0x1ce) && (ch <= 0x1dc))
		|| ((ch >= 0x1df) && (ch <= 0x1ef))
		|| (ch == 0x1f5)
		|| ((ch >= 0x1f9) && (ch <= 0x233))
		|| (ch == 0x23c)
		|| (ch == 0x243)
		) {
		return (wchar_t)(ch - 0x1);
	}

	if (  (ch == 0x1c6)
		|| (ch == 0x1c9)
		|| (ch == 0x1c6)
		|| (ch == 0x1f3)
		) {
		return (wchar_t)(ch - 0x2);
	}

	return (ch);
}

bool isNormalWidthLowercase(wchar_t ch) {
	return ((normalWidthLowercaseCharset.find(ch) != wstring::npos));
}

bool isNormalChar(wchar_t ch) {
	return ((ch>32) && (ch<126));
}

bool abnormalSpacing(wchar_t ch) {
	return ((charsWithAbnormalSpacing.find(ch) != wstring::npos));
}

// All the below are replaced with a simple quote at the start of validateLine
// U+2018	‘	e2 80 98	LEFT SINGLE QUOTATION MARK
// U+2019	’	e2 80 99	RIGHT SINGLE QUOTATION MARK
// U+201B	‛	e2 80 9b	SINGLE HIGH-REVERSED-9 QUOTATION MARK

// U+201C	“	e2 80 9c	LEFT DOUBLE QUOTATION MARK
// U+201D	”	e2 80 9d	RIGHT DOUBLE QUOTATION MARK
// U+201F	‟	e2 80 9f	DOUBLE HIGH-REVERSED-9 QUOTATION MARK
bool isQuote (wchar_t ch) {
	if ((ch == '\'') || (ch == '`') || (ch == '"')
        || (ch == 0x2019) || (ch == 0x201B) || (ch == 0x2018) || (ch == 0x201C) || (ch == 0x201D) || (ch == 0x201F)) {
		return true;
	} else {
		return false;
	}
}

bool isLetterOrDigitOrNormalHeight(wchar_t ch) {
    if (isLetterOrDigit(ch)) 
        return true;
    else return (normalHeightNonAlpha.find(ch) != wstring::npos); 
}

bool iEquiv(wchar_t ch) {
    if (letteriEquivUTF32.length() == 0) {
        letteriEquivUTF32 = toUTF32(letteriEquivUTF8);
    }
	return (letteriEquivUTF32.find(toLower(ch)) != wstring::npos);
}

bool uEquiv(wchar_t ch) {
    if (letterUEquivUTF32.length() == 0) {
        letterUEquivUTF32 = toUTF32(letterUEquivUTF8);
    }
	return (letterUEquivUTF32.find(toLower(ch)) != wstring::npos);
}

bool isDash (wchar_t ch) {
	if ((ch == '-') || (ch == '~')) {
		return true;
	} else {
		return false;
	}
}

bool isWordDelimiter(wchar_t ch) {
    if ((ch == ' ') || (ch == ','))
        return true;
    else 
        return false;
}

bool isLeftOverhangUppercase(wchar_t ch) {
    if ((ch == 'C') || (ch == 'O') || (ch == 'Q') || (ch == 'W') || (ch == 'V') || (ch == 'G'))
        return true;
    else 
        return false;
}

bool isGreaterThanEquiv (wchar_t ch) {
	if ((ch == 0x0bb) 
		|| (ch == 0x203a) // U+203A	›	e2 80 ba	SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
		|| (ch == '>')
		) {
		return true;
	}
	return false;
}

bool isCLookalike (wchar_t ch) {
	wchar_t lowerCh = toLower(ch);
	if ((lowerCh == 'c')
		|| (ch == '<')
		|| (ch == 0x2039) // U+2039	‹	e2 80 b9	SINGLE LEFT-POINTING ANGLE QUOTATION MARK
		// U+00E7	ç	c3 a7	LATIN SMALL LETTER C WITH CEDILLA
		|| (lowerCh == 0xE7)
		// U+0107	ć	c4 87	LATIN SMALL LETTER C WITH ACUTE
		|| (lowerCh == 0x107)
		// U+0109	ĉ	c4 89	LATIN SMALL LETTER C WITH CIRCUMFLEX
		|| (lowerCh == 0x109)
		// U+010D	č	c4 8d	LATIN SMALL LETTER C WITH CARON
		|| (lowerCh == 0x10D)
		) {
		return true;
	}
	return false;
}

bool isBlank(const wstring& text) {
    for (int i=0; i<text.length(); i++) {
        if (text[i] != ' ') return false;
    }
    return true;
}

bool isBracket (wchar_t ch) {
	if ((ch == '(') || (ch == ')') || (ch == '[') || (ch == ']') || (ch == '{') || (ch == '}') || (ch == '<') || (ch == '>')) {
		return true;
	}
	return false;
}

bool isOpenBracket (wchar_t ch) {
	if ((ch == '(') || (ch == '[') || (ch == '{') || (ch == '<')) {
		return true;
	}
	return false;
}

bool isClosingBracket (wchar_t ch) {
	if ((ch == ')') || (ch == ']') || (ch == '}') || (ch == '>')) {
		return true;
	}
	return false;
}

bool looksSameAsLowercase(wchar_t ch) {
	return ((looksSameAsLowercaseCharset.find(ch) != wstring::npos));
}

bool looksSameAsLowercase(wchar_t ch, OCRResults *results) {
    if (looksSameAsLowercaseCharset.find(ch) != wstring::npos) {
        return true;
    }
    // If not English try some more special chars
    if (results->languageCode != LanguageENG) {
        if (looksSameAsLowercaseCharsetIntlUTF32.length() == 0)
            looksSameAsLowercaseCharsetIntlUTF32 = toUTF32(looksSameAsLowercaseCharsetIntlUTF8);    
       return (looksSameAsLowercaseCharsetIntlUTF32.find(ch) != wstring::npos); 
    } else {
        return false;
    }
    return false;
} // looksSameAsLowercase

// Tests if a letter is narrower than "normal" letters, e.g. l or t. Used when computing stats for a line, to exclude letters that would skew the "norm".
bool isNarrow(wchar_t ch) {
	return ((ch=='|') || (ch=='1') || (ch=='I') || (ch=='i') || (ch == 'l') || (ch=='t') || (ch=='[') || (ch==']')
			|| (ch=='{') || (ch == '}') || (ch=='\'') || (ch=='`') || (ch==':') || (ch=='!') || (ch==';') || (ch==',') || (ch=='.')
			|| (ch=='/') || (ch=='\\') || (ch=='*') || (ch==')') || (ch=='(') || (ch=='^'));
}

bool isSuperNarrow1(wchar_t ch) {
	return ((superNarrowCharset1.find(ch) != wstring::npos));
}

bool isSuperNarrow15(wchar_t ch) {
	return ((superNarrowCharset15.find(ch) != wstring::npos));
}

bool isSuperNarrow(wchar_t ch) {
	return (isSuperNarrow1(ch) || isSuperNarrow15(ch));
}
    
bool isTallLowercase(wchar_t ch) {
    return ((tallAboveCharset.find(ch) != wstring::npos) || (tallBelowCharset.find(ch) != wstring::npos));
}

typedef unsigned int uint32_t;
inline void appendToUTF8(uint32_t cp, std::string& result) {

        if (cp < 0x80)                        // one octet
            result += static_cast<uint8_t>(cp);
        else if (cp < 0x800) {                // two octets
            result +=  static_cast<uint8_t>((cp >> 6)            | 0xc0);
            result +=  static_cast<uint8_t>((cp & 0x3f)          | 0x80);
        }
        else if (cp < 0x10000) {              // three octets
            result +=  static_cast<uint8_t>((cp >> 12)           | 0xe0);
            result +=  static_cast<uint8_t>(((cp >> 6) & 0x3f)   | 0x80);
            result +=  static_cast<uint8_t>((cp & 0x3f)          | 0x80);
        }
        else {      // four octets
            result +=  static_cast<uint8_t>((cp >> 18)           | 0xf0);
            result +=  static_cast<uint8_t>(((cp >> 12) & 0x3f)  | 0x80);
            result +=  static_cast<uint8_t>(((cp >> 6) & 0x3f)   | 0x80);
            result +=  static_cast<uint8_t>((cp & 0x3f)          | 0x80);
        }
    }

std::string toUTF8(const std::wstring& utf32) {
 // TODO this is a lie!
 std::string res;
  for(size_t i = 0; i < utf32.length(); i++){
      appendToUTF8(utf32[i], res);
  }

  return res;
}

// Returns a vector of indexes (relative to source CC list
vector<int> sortConnectedComponents(ConnectedComponentList &cpl, int method, int start, int end) {
    vector<int> results;
    
    if (method != CONNECTEDCOMPORDER_XMAX_DESC)
        return results;
    
    if (start == -1) start = 1; // Start at 1 because first entry in a connected comps list is a special CC encompassing all CCs
    if (end == -1) end = (int)cpl.size()-1;
    
    for (int i=start; i<=end; i++) {
        ConnectedComponent cc = cpl[i];
        int insertionIndex = -1;
        for (int j=0; j<results.size(); j++) {
            ConnectedComponent existingCC = cpl[results[j]];
            if ((method == CONNECTEDCOMPORDER_XMAX_DESC) && (cc.xmax > existingCC.xmax)) {
                insertionIndex = j;
                break;
            }
        }
        if (insertionIndex == -1)
            results.push_back(i);
        else
            results.insert(results.begin() + insertionIndex, i);
    }
    
    return (results);
}

bool OverlappingX(ConnectedComponent &cc1, ConnectedComponent &cc2) {
    return OCRUtilsOverlapping(cc1.xmin, cc1.xmax, cc2.xmin, cc2.xmax);
}

bool OCRUtilsOverlapping(float min1,float max1, float min2, float max2) {
	float MinX = MAX(min1, min2);
	float MaxX = MIN(max1, max2);
	if (MaxX < MinX)
		return false;
	else
		return true;
}

bool OCRUtilsOverlappingX(CGRect r1, CGRect r2) {
    return OCRUtilsOverlapping(r1.origin.x, r1.origin.x+r1.size.width-1, r2.origin.x, r2.origin.x+r2.size.width-1);
}

bool OCRUtilsOverlappingY(CGRect r1, CGRect r2) {
    return OCRUtilsOverlapping(r1.origin.y, r1.origin.y+r1.size.height-1, r2.origin.y, r2.origin.y+r2.size.height-1);
}

int sequence_length(char leadChar)
{
    uint8_t lead = leadChar;
    if (lead < 0x80)
        return 1;
    else if ((lead >> 5) == 0x6)
        return 2;
    else if ((lead >> 4) == 0xe)
        return 3;
    else if ((lead >> 3) == 0x1e)
        return 4;
    else
        return 0;
}

uint32_t lengthMask[] = {0x0,0x0,0xc0,0xe0,0xf0};

inline wchar_t next(const char*& start, const char* end) {

    uint32_t res = 0;
    uint32_t curChar;
    int length = sequence_length(*start);

    if (length == 0)
        return 0; // Invalid utf8!!

    if (start + length > end)
        return 0; // Not enought room for char!

    // Clean of the size mask:
    curChar = static_cast<uint8_t>(*start);
    res = curChar ^ lengthMask[length];
    ++start;

    for (int i = 1; i < length; ++i) {
        res <<= 6;
        curChar = static_cast<uint8_t>(*start);
        curChar ^= 0x80;
        res |= curChar;
        ++start;
    }

    return res;
}

std::wstring toUTF32(const std::string& utf8) {

 std::wstring res;

  const char* cStr = utf8.c_str();
  const char* start = cStr;
  const char* end = cStr + utf8.length();

  while (start < end) {
      wchar_t unicodeChar = next(start, end);
      if (unicodeChar == 0) {return std::wstring();} // bad conversion
      res += unicodeChar;
 }
  return res;
}

int findAndReplace(std::wstring& data, const std::wstring& searchString, const std::wstring& replaceString ) {
  std::string::size_type pos = 0;
  int i = 0;
    while ( (pos = data.find(searchString, pos)) != data.npos ) {
        data.replace( pos, searchString.size(), replaceString );
        pos++;
        i++;
    }
    return i;
}

// Given a start & length of a range within a UTF8 string (e.g. from byte 10, 5 bytes) returns the index of the Unicode letter in the wider string (e.g. perhaps the 9 bytes preceding corresponded to 7 Unicode letters) and how many Unicode letters are comprised in the range (e.g. might be 2 letters if first letter was 3 UTF8 bytes and 2md was 2 UTF8 bytes)
void convertIndexes(const std::string& utf8, int byteOffset, int byteLen, int& letterOffset, int& letterCount) {
  const char* cStr = utf8.c_str();
  const char* start = cStr;
  const char* end = cStr + byteOffset;

  letterOffset = 0;

  while (start < end) {
      int length = sequence_length(*start);
      if (length == 0) {
         letterOffset = letterCount = -1;
         return;
      }
      start += length;
      ++letterOffset;
 }
 // in here this should exist: start == end.
  letterCount = 0;
  end += byteLen;
  while (start < end) {
      int length = sequence_length(*start);
      if (length == 0) {
         letterOffset = letterCount = -1;
         return;
      }
      start += length;
      ++letterCount;
 }
} // convertIndexes

// Computes by how much the letter's top lies above/below the top of neighbors, returns -1000 is no applicable neigbors 
// lowLetter: 0 = no (i.e. tall/uppercase), 1 = yes (lowercase), -1 = anything (but not tall above)
// per: if set, returns the result in terms of percentage of height of neigbors.
float gapAboveToplineOfNeighbors(SmartPtr<OCRRect> r, int lowLetter, bool both, bool per) {
    int numNeigbors = 0;
	float sum = 0;
    
	if ( (r->previous != NULL) 
        && (   ((lowLetter == -1) && !isTallAbove(r->previous->ch))
            || ((lowLetter == 1) && isLower(r->previous->ch) && !isTallAbove(r->previous->ch))
            || ((lowLetter == 0) && (isDigit(r->previous->ch) || isTallAbove(r->previous->ch) || isUpper(r->previous->ch)))
           ) ) 
    {
        if (per) {
			sum += (rectBottom(r->previous->rect)-rectBottom(r->rect)) / r->previous->rect.size.height;
		} else {
            sum += (rectBottom(r->previous->rect)-rectBottom(r->rect));
		}

        numNeigbors++;
	} else if (both) {
        return -1;
    }
    
	if ( (r->next != NULL) 
        && (   ((lowLetter == -1) && !isTallAbove(r->next->ch))
            || ((lowLetter == 1) && isLower(r->next->ch) && !isTallAbove(r->previous->ch))
            || ((lowLetter == 0) && (isDigit(r->next->ch) || isTallAbove(r->next->ch) || isUpper(r->next->ch)))
           ) ) 
    {
        if (per) {
            sum += (rectBottom(r->next->rect)-rectBottom(r->rect)) / r->next->rect.size.height;
        } else {
            sum += (rectBottom(r->next->rect)-rectBottom(r->rect));
        }
        numNeigbors++;
    } else if (both) {
        return -1;
    }
        
	if (numNeigbors == 0)
		return -1000;
	else {
		return (sum/numNeigbors);
	}
} // gapAboveToplineOfNeighbors

int gapRectAboveBaselineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, CGRect rect, float limit, bool absolute) {
    int numNeigbors = 0;
    
	if (r->previous != NULL) {
        if (isDigit(r->previous->ch) || isUpper(r->previous->ch) || (isLower(r->previous->ch) && !isTallBelow(r->previous->ch))) {
            float gap = rectTop(r->previous->rect)-rectTop(rect);
            if (absolute)
                gap = abs(gap);
            if (gap > limit)
                return 0;   // Failed test
            numNeigbors++;
        }
    }
    
    if (r->next != NULL) {
        if (isDigit(r->next->ch) || isUpper(r->next->ch) || (isLower(r->next->ch) && !isTallBelow(r->next->ch))) {
            float gap = rectTop(r->next->rect)-rectTop(rect);
            if (absolute)
                gap = abs(gap);
            if (gap > limit)
                return 0; // Failed test
            numNeigbors++;
        }
    }
    
    if (numNeigbors == 0)
		return -1;  // Can't tell
	else {
		return 1; // Passed test
	}
} // gapAboveBaselineOfNeighborsLessThanLimit

/* Computes by how much the letter's baseline (i.e. top since y=0 is the top) is above baseline of neighbors
    Returns:
    - 1: less than given limit (with one or both neighbors applicate to test)
    - 0: more than given limit (with one or both neighbors applicate to test)
    - -1: none of the neighbors was applicable to test
*/
int gapAboveBaselineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, float limit, bool absolute) {
    return (gapRectAboveBaselineOfNeighborsLessThanLimit(r, r->rect, limit, absolute));
} // gapAboveBaselineOfNeighborsLessThanLimit

/* Computes by how much the letter's topline (i.e. rectbottom since y=0 is the top) is above topline of neighbors
    Returns:
    - 1: less than given limit (with one or both neighbors applicate to test)
    - 0: more than given limit (with one or both neighbors applicate to test)
    - -1: none of the neighbors was applicable to test
*/
int gapRectAboveToplineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, CGRect rect, float limit, bool absolute) {
    int numNeigbors = 0;
    
	if (r->previous != NULL) {
        if (isDigit(r->previous->ch) || isUpper(r->previous->ch) || (isLower(r->previous->ch) && !isTallAbove(r->previous->ch))) {
            float gap = rectBottom(r->previous->rect)-rectBottom(rect);
            if (absolute)
                gap = abs(gap);
            if (gap > limit)
                return 0;   // Failed test
            numNeigbors++;
        }
    }
    
    if (r->next != NULL) {
        if (isDigit(r->next->ch) || isUpper(r->next->ch) || (isLower(r->next->ch) && !isTallAbove(r->next->ch))) {
            float gap = rectBottom(r->next->rect)-rectBottom(rect);
            if (absolute)
                gap = abs(gap);
            if (gap > limit)
                return 0; // Failed test
            numNeigbors++;
        }
    }
    
    if (numNeigbors == 0)
		return -1;  // Can't tell
	else {
		return 1; // Passed test
	}
} // gapAboveToplineOfNeighborsLessThanLimit

int gapAboveToplineOfNeighborsLessThanLimit(SmartPtr<OCRRect> r, float limit, bool absolute) {
    return (gapRectAboveToplineOfNeighborsLessThanLimit(r, r->rect, limit, absolute));
} // gapAboveToplineOfNeighborsLessThanLimit

int tallScoreRelativeToNeigbor(SmartPtr<OCRRect> r, float height, SmartPtr<OCRRect> otherR) {
	if (otherR == NULL)
		return -1;
    bool checkingPrevious = false;
    bool checkingNext = false;
    if (otherR == r->previous)
        checkingPrevious = true;
    else if (otherR == r->next)
        checkingNext = true;
	if (isLower(otherR->ch)) {
		// OK to judge based on lowercase (normal or tall)
		if (isTallLowercase(otherR->ch)) {
			return ((height > otherR->rect.size.height * (1-OCR_ACCEPTABLE_ERROR))? 1:0);
		} else
			return ((height > otherR->rect.size.height * (1+OCR_ACCEPTABLE_ERROR))? 1:0);
	} else if (isDigit(otherR->ch) || isUpper(otherR->ch)) {
		return ((r->rect.size.height > otherR->rect.size.height * (1-OCR_ACCEPTABLE_ERROR))? 1:0);
	}
	return -1;
} // tallScoreRelativeToNeigbor

int lowScoreRelativeToNeigbor(OCRRectPtr r, OCRRectPtr otherR) {
	if (otherR == NULL)
		return -1;
	if (isLower(otherR->ch)) {
		// OK to judge based on lowercase (normal or tall)
		if (isTallLowercase(otherR->ch)) {
			return ((r->rect.size.height * OCR_TALL_TO_LOWERCASE_RATIO < otherR->rect.size.height * (1+OCR_ACCEPTABLE_ERROR))? 1:0);
		} else
			return ((r->rect.size.height < otherR->rect.size.height * (1+OCR_ACCEPTABLE_ERROR))? 1:0);
	} else if (isDigit(otherR->ch) || isUpper(otherR->ch)) {
		return (r->rect.size.height < otherR->rect.size.height * 0.88);
	}
	return -1;
} // lowScoreRelativeToNeigbor

// Returns true only if we know for a fact that the letter is low relative to neighbors. "false" means it's not low or that we don't know
bool isLowRelativeToNeigbors(OCRRectPtr r, bool both, bool smallCap, OCRResults *results) {
    if (results->retailerParams.allUppercase)
        return false;
    if (results->globalStats.averageHeightDigits.count > 0) {
        if (r->rect.size.height >= results->globalStats.averageHeightDigits.average * 0.80)
            return false;
    }

	if (both) {
		if (smallCap && (r->previous != NULL) && ((r->previous->previous == NULL) || (r->previous->previous->ch == ' '))) {
			return (lowScoreRelativeToNeigbor(r,r->next) == 1);
		} else {
			return ((lowScoreRelativeToNeigbor(r,r->previous) == 1) && (lowScoreRelativeToNeigbor(r,r->next) == 1));
		}
	} else {
		int lowRelativeToPrevious = lowScoreRelativeToNeigbor(r,r->previous);
		int lowRelativeToNext = lowScoreRelativeToNeigbor(r,r->next);
		// Require that neither test says "high" but OK if one test is inconclusive (return -1)
		if ((lowRelativeToNext == 0) || (lowRelativeToPrevious == 0)) {
			return false;
		} else if ((lowRelativeToNext == 1) || (lowRelativeToPrevious == 1)) {
			return true;
		} else {
			return false;
		}
	}
} // isLowRelativeToNeigbors

int extendsBelowLineRelativeToNeighbors(SmartPtr<OCRRect> r, bool onlyWithinWord) {
    int res = -1;
    
    // Look before
	OCRRectPtr rBack = r->previous;
	while ((rBack != NULL) && (!onlyWithinWord || (rBack->ch != ' ')) && (rBack->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
		if (isLetterOrDigit(rBack->ch)) {
            float yBottomDiff = rectTop(r->rect)-rectTop(rBack->rect);
            if (extendsBelowLine(rBack->ch)) {
                if (yBottomDiff > r->rect.size.height * 0.15) {
                    res = 1;
                    break;
                } else {
                    return 0;
                }
            } else {
                if (yBottomDiff > r->rect.size.height * 0.20) {
                    res = 1;
                    break;
                } else {
                    return 0;
                }
            }
        }
		rBack = rBack->previous;
	}
    
    // Look after
	OCRRectPtr rNext = r->next;
	while ((rNext != NULL) && (!onlyWithinWord || (rNext->ch != ' ')) && (rNext->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
		if (isLetterOrDigit(rNext->ch)) {
            float yBottomDiff = rectTop(r->rect)-rectTop(rNext->rect);
            if (extendsBelowLine(rNext->ch)) {
                if (yBottomDiff > r->rect.size.height * 0.15) {
                    return 1;
                } else {
                    return 0;
                }
            } else {
                if (yBottomDiff > r->rect.size.height * 0.20) {
                    return 1;
                } else {
                    return 0;
                }
            }
        }
		rNext = rNext->next;
	}
    
    return res;
}

int testBelowRelativeToNeigbor(SmartPtr<OCRRect> r, SmartPtr<OCRRect> otherR, float *gapBelow = NULL) {
	if ((otherR == NULL) || (r->rect.size.height == 0) || (otherR->rect.size.height == 0))
		return -1;
	if (isNormalChar(otherR->ch)) {
		float yBottomDiff = rectTop(r->rect)-rectTop(otherR->rect);
		if (extendsBelowLine(otherR->ch)) {
			if (abs(yBottomDiff) < OCR_ACCEPTABLE_ERROR)
				return 1;
			else
				return 0;
		} else {
			if (yBottomDiff/otherR->rect.size.height > OCR_BELOW_LINE_TO_HEIGHT_RATIO) {
                if (gapBelow != NULL)
                    *gapBelow = yBottomDiff;
				return 1;
			} else
				return 0;
		}
	} else {
		return -1;
	}
} // testBelowRelativeToNeigbor

int testBelowRelativeToNeigbors(SmartPtr<OCRRect> r, float *gapBelow) {
	int res1 = testBelowRelativeToNeigbor(r, r->previous, gapBelow);
	if (res1 != -1)
		return res1;
	else {
		return testBelowRelativeToNeigbor(r, r->next, gapBelow);
	}
} // testBelowRelativeToNeigbors

// If per is true calculates the gap in terms of % height of relevant letters(s)
float gapBelowToplinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool per, bool skipOne) {
	int numNeigbors = 0;
	float sumAbove = 0;

	if (r->previous != NULL) {
        SmartPtr<OCRRect> rPrev = r->previous;
        bool doit = false;
        if ((rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallAbove(rPrev->ch)) {
            doit = true;
        } else {
            rPrev = r->previous->previous;
            if ((rPrev != NULL) && (rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallAbove(rPrev->ch)) {
                doit = true;
            }
        }
        if (doit) {
            if (per) {
                sumAbove += ((rectBottom(rect) - rectBottom(rPrev->rect)) / rPrev->rect.size.height);
            } else {
                sumAbove += rectBottom(rect) - rectBottom(rPrev->rect);
            }
            numNeigbors++;
        }
	}
    
    if (r->next != NULL) {
        SmartPtr<OCRRect> rNext = r->next;
        bool doit = false;
        if ((rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallAbove(rNext->ch)) {
            doit = true;
        } else {
            rNext = r->next->next;
            if ((rNext != NULL) && (rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallAbove(rNext->ch)) {
                doit = true;
            }
        }
        if (doit) {
            float delta = rectBottom(rect) - rectBottom(rNext->rect);
            if (per) {
                sumAbove += delta / rNext->rect.size.height;
            } else {
                sumAbove += delta;
            }
            numNeigbors++;
        }
	}
    
	if (numNeigbors == 0)
		return -1000;
	else {
		return (sumAbove/numNeigbors);
	}
} // gapBelowToplinePercent

float gapBelowToplinePercent (SmartPtr<OCRRect> r, ConnectedComponent &cc, bool per, bool skip) {
    CGRect tmpRect(rectLeft(r->rect)+cc.xmin, rectBottom(r->rect)+cc.ymin, cc.getWidth(), cc.getHeight());
    return (gapBelowToplinePercent(r, tmpRect, per, skip));
}

// If per is true calculates the gap in terms of % height of relevant letters(s)
float gapBelowBaselinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool per, bool skipOne) {
	int numNeigbors = 0;
	float sumAbove = 0;

	if (r->previous != NULL) {
        SmartPtr<OCRRect> rPrev = r->previous;
        bool doit = false;
        if ((rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallBelow(rPrev->ch)) {
            doit = true;
        } else {
            rPrev = r->previous->previous;
            if ((rPrev != NULL) && (rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallBelow(rPrev->ch)) {
                doit = true;
            }
        }
        if (doit) {
            float delta = rectTop(rect) - rectTop(rPrev->rect);
            if (per) {
                sumAbove += (delta / rPrev->rect.size.height);
            } else {
                sumAbove += delta;
            }
            numNeigbors++;
        }
	}
    
    if (r->next != NULL) {
        SmartPtr<OCRRect> rNext = r->next;
        bool doit = false;
        if ((rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallBelow(rNext->ch)) {
            doit = true;
        } else {
            rNext = r->next->next;
            if ((rNext != NULL) && (rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallBelow(rNext->ch)) {
                doit = true;
            }
        }
        if (doit) {
            float delta = rectTop(rect) - rectTop(rNext->rect);
            if (per) {
                sumAbove += delta / rNext->rect.size.height;
            } else {
                sumAbove += delta;
            }
            numNeigbors++;
        }
	}
    
	if (numNeigbors == 0)
		return -1000;
	else {
		return (sumAbove/numNeigbors);
	}
} // gapBelowBaseLinePercent

// Check is current letter is tall relative to its first letter neighbor to the right or to the left (until the next word boundary)    
int tallScoreRelativeToNeigbors(SmartPtr<OCRRect> r, float height, bool right) {
    SmartPtr<OCRRect> otherR = (right? r->next:r->previous);
    
    while ((otherR != NULL) && !isWordDelimiter(otherR->ch)) {
        int res = tallScoreRelativeToNeigbor(r, height, otherR);
        if (res != -1)
            return res;
        otherR = (right? otherR->next:otherR->previous);
    }
    return -1;
}

// If both == true, requires that both neighbors yield definite verdict and both say tall
// Otherwise just require that one of them says tall and the other doesn't say low
bool isTallRelativeToNeigbors(SmartPtr<OCRRect> r, float height, bool both) {
    int tallRelativeToPrevious = tallScoreRelativeToNeigbors(r, height, false);
    int tallRelativeToNext = tallScoreRelativeToNeigbors(r, height, true);
    if (both) {
        if ((tallRelativeToPrevious == 1) && (tallRelativeToNext == 1))
            return true;
        else 
            return false;
    } else {
        // Require that neither test says "low" but OK if one test is inconclusive (return -1)
        if ((tallRelativeToPrevious == 1) && (tallRelativeToNext != 0))
            return true;
        if ((tallRelativeToNext == 1) && (tallRelativeToPrevious != 0))
            return true;   
        return false;
    }
} // isTallRelativeToNeigbors

// Compare against all letters in the current word
int strictShortTest (OCRRectPtr r, int certLevel) {
    
    int countLetters = 0;
    int countLettersBeyondSpaces = 0;
    bool percent10Taller = false;
    bool percent15Shorter = false;
    
    bool foundSpace = false;
    OCRRectPtr rBack = r->previous;
    while ((rBack != NULL) && (rBack->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
        if ((rBack->confidence < 1000) && isLetterOrDigit(rBack->ch)) {
            if (r->rect.size.height > rBack->rect.size.height * 1.15) {
                return 0;
            } else if (r->rect.size.height > rBack->rect.size.height * 1.10) {
                percent10Taller = true;
            } else if (rBack->rect.size.height > r->rect.size.height * 1.15) {
                percent15Shorter = true;
            }
            if (foundSpace)
                countLettersBeyondSpaces++;
            else
                countLetters++;
        } else if (rBack->ch == ' ') {
            foundSpace = true;
        }
        rBack = rBack->previous;
    }
    
    foundSpace = false;
    OCRRectPtr rNext = r->next;
    while ((rNext != NULL) && (rNext->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
        if ((rNext->confidence < 1000) && isLetterOrDigit(rNext->ch)) {
            if (r->rect.size.height > rNext->rect.size.height * 1.15) {
                return 0;
            } else if (r->rect.size.height > rNext->rect.size.height * 1.10) {
                percent10Taller = true;
            } else if (rNext->rect.size.height > r->rect.size.height * 1.15) {
                percent15Shorter = true;
            }
            if (foundSpace)
                countLettersBeyondSpaces++;
            else
                countLetters++;
        } else if (rNext->ch == ' ') {
            foundSpace = true;
        }
        rNext = rNext->next;
    }
    
    if (countLetters + countLettersBeyondSpaces <= certLevel) {
        return -1;
    } else {
        if (!percent10Taller || percent15Shorter)
            return 1;
        else
            return 0;
    }
} // strictShortTest

int lowHeightTest (OCRStats *av, float height, int certLevel, bool lowHurdle, OCRResults *results) {
    if (results->retailerParams.allUppercase)
        return false;
    if (results->globalStats.averageHeightDigits.count > 0) {
        if (height < results->globalStats.averageHeightDigits.average * 0.80)
            return 1;
        else
            return 0;
    } else {
        int maxCount = MAX(av->averageHeightNormalLowercase.count, av->averageHeightTallLowercase.count);
        maxCount = MAX(maxCount, av->averageHeightDigits.count);
        maxCount = MAX(maxCount, av->averageHeightUppercase.count);

        if (maxCount <= certLevel)
            return -1;
            
        int certain = MAX(1, certLevel);
            
        // First case: if we have > certLevel normal lowercase chars and we are about the same height, and there are too few tall letters, or we are shorter than them, we ARE short
        if ((av->averageHeightNormalLowercase.count > certain) && (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
            && ((av->averageHeightUppercase.count <= certain) 
                || (height < av->averageHeightUppercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO)))
            && ((av->averageHeightUppercaseNotFirstLetter.count <= certain) 
                || (height < av->averageHeightUppercaseNotFirstLetter.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO)))
            && ((av->averageHeightTallLowercase.count <= certain) 
                || (height < av->averageHeightTallLowercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO)))
            && ((av->averageHeightDigits.count <= certain) 
                || (height < av->averageHeightDigits.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO)))) {
                return 1;
        }

        // Inverse case
        if ((av->averageHeightNormalLowercase.count > certain) 
            && (height > av->averageHeightNormalLowercase.average * OCR_TALL_TO_LOWERCASE_RATIO)) {
            return 0;
        }
        
        if ( (av->averageHeightNormalLowercase.count >= CERTFORLOWHURDLE) || (av->averageHeightNormalLowercase.count == maxCount)) {
            if (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
                return 1;
            else {
                // Do NOT do other test when hudle is low: if current height is such that it's not the same or slighly above
                // a large set of letters, it CANNOT be low
                return 0;
            }
        } else if (av->averageHeightUppercaseNotFirstLetter.count == maxCount) {
            if (height < av->averageHeightUppercaseNotFirstLetter.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))
                return 1;
            else {
                if (lowHurdle
                    && (  ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
                           && (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR))))
                        || ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
                            && (height < av->averageHeightDigits.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                        || ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
                            && (height < av->averageHeightTallLowercase.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                       )
                    ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        } else if (av->averageHeightUppercase.count == maxCount) {
            if (height < av->averageHeightUppercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))
                return 1;
            else {
                if (lowHurdle
                    && (  ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
                           && (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR))))
                        || ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
                            && (height < av->averageHeightDigits.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                        || ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
                            && (height < av->averageHeightTallLowercase.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                       )
                    ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        } else if (av->averageHeightTallLowercase.count == maxCount) {
            if (height < av->averageHeightTallLowercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))
                return 1;
            else {
                if ( lowHurdle
                    && (  ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
                           && (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR))))
                           || ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
                               && (height < av->averageHeightDigits.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                           || ((av->averageHeightUppercase.count > CERTFORLOWHURDLE)
                               && (height < av->averageHeightUppercase.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                           )
                        ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        } else if (av->averageHeightDigits.count == maxCount) {
            if (height < av->averageHeightDigits.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))
                return 1;
            else {
                if ( lowHurdle
                    && (  ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
                           && (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR))))
                           || ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
                               && (height < av->averageHeightTallLowercase.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                           || ((av->averageHeightUppercase.count > CERTFORLOWHURDLE)
                               && (height < av->averageHeightUppercase.average/OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE))
                           )
                    ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        }
        return -1;
    }
} // lowHeightTest

// Compare against all letters in the current word
int strictTallTest (OCRRectPtr r, float height, int certLevel) {

	int countLetters = 0;
    if (height == 0)
        height = r->rect.size.height;

	OCRRectPtr rBack = r->previous;
	while ((rBack != NULL) && (rBack->ch != ' ') && (rBack->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
        // Not an outlier
		if ((rBack->confidence < 1000)
            && isLetterOrDigit(rBack->ch)) {
            if (height < rBack->rect.size.height * (1 - OCR_ACCEPTABLE_ERROR)) {
                return 0;
            }
            countLetters++;
        }
		rBack = rBack->previous;
	}

	OCRRectPtr rNext = r->next;
	while ((rNext != NULL) && (rNext->ch != ' ') && (rNext->ch != '\n')) {
        // Only test against letters and digits, NOT punctuations like '.'!
		if ((rNext->confidence < 1000) && isLetterOrDigit(rNext->ch)) {
            if (height < rNext->rect.size.height * (1 - OCR_ACCEPTABLE_ERROR)) {
                return 0;
            }
            countLetters++;
        }
		rNext = rNext->next;
	}

	if (countLetters > certLevel)
		return 1;
	else
		return -1;
} // strictTallTest


/* Returns:
 -1: can't tell
 0: not tall
 1: tall
*/
int tallHeightTest (OCRStats *av, float height, wchar_t ch, bool requireSame, int certLevel, bool lowHurdle) {

	int certain = MAX(2, certLevel);

	// First case: if we have > certLevel normal lowercase chars and we are about the same height, and there are too few tall letters, or we are shorter than them, we ARE short
	if ((av->averageHeightNormalLowercase.count > certain) 
			&& (height < av->averageHeightNormalLowercase.average * (1+(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR))))
    {
        if ((av->averageHeightUppercase.count > certain) 
			&& (height > av->averageHeightUppercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
			return -1;
		}
		if ((av->averageHeightUppercase.count > certain) 
			&& (height > av->averageHeightUppercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
			return -1;
		}
        if ((av->averageHeightUppercaseNotFirstLetter.count > certain) 
			&& (height > av->averageHeightUppercaseNotFirstLetter.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
			return -1;
		}
		if ((av->averageHeightTallLowercase.count > certain)
			&& (height > av->averageHeightTallLowercase.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
			return -1;
		}
		if ((av->averageHeightDigits.count > certain)
			&& (height > av->averageHeightDigits.average / (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
			return -1;
		}
		return 0;
	}

	// Inverse case
	if ((av->averageHeightNormalLowercase.count > certain) 
		&& (height < av->averageHeightNormalLowercase.average * OCR_TALL_TO_LOWERCASE_RATIO)) {
		return 0;
	}

	// First test the char according to its kind, always more reliable
	if (isLower(ch)) {
//#if DEBUG
//		if ((ch != 0) && !isTall(ch)) {
//			ErrorLog("tallHeightTest: testing [%c] as tall, make no sense", ch);
//		}
//#endif
		if (isTallLowercase(ch)) {
			if (av->averageHeightTallLowercase.count > certLevel) {
				if (height > av->averageHeightTallLowercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
					return 1;
				else
					return 0;
			} else if (requireSame) {
				return -1;
			}
		} else if (av->averageHeightNormalLowercase.count > certLevel) {
			if (height > av->averageHeightNormalLowercase.average*OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE) {
				return 1;
			} else {
				return 0;
			}
		} else if (requireSame) {
			return -1;
		}
	} else if (isDigit(ch)) {
		if (av->averageHeightDigits.count > certLevel) {
			if (height > av->averageHeightDigits.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
				return 1;
			else
				return 0;
		} else if (requireSame) {
			return -1;
		}
	} else if (isupper(ch)) {
		if (av->averageHeightUppercaseNotFirstLetter.count > certLevel) {
			if (height > av->averageHeightUppercaseNotFirstLetter.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
				return 1;
			else
				return 0;
		} else if (av->averageHeightUppercase.count > certLevel) {
			if (height > av->averageHeightUppercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
				return 1;
			else
				return 0;
		} else if (requireSame) {
			return -1;
		}
	}

	int maxCount = MAX(av->averageHeightNormalLowercase.count, av->averageHeightTallLowercase.count);
	maxCount = MAX(maxCount, av->averageHeightDigits.count);
	maxCount = MAX(maxCount, av->averageHeightUppercase.count);
	if (maxCount <= certLevel)
		return -1;
	if (av->averageHeightNormalLowercase.count == maxCount) {
		if (height > av->averageHeightNormalLowercase.average * (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))
			return 1;
		else {
			if (lowHurdle) {
				if ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
					&& (height > av->averageHeightTallLowercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				} else if ((av->averageHeightUppercase.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightUppercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				} else if ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightDigits.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				}
			}
			return 0;
		}
	} else if (av->averageHeightUppercase.count == maxCount) {
		if (height > av->averageHeightUppercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
			return 1;
		else {
			if (lowHurdle) {
				if ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
					&& (height > av->averageHeightTallLowercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				} else if ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightNormalLowercase.average * (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
					return 1;
				} else if ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightDigits.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				}
			}
			return 0;
		}
	} else if (av->averageHeightTallLowercase.count == maxCount) {
		if (height > av->averageHeightTallLowercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
			return 1;
		else {
			if (lowHurdle) {
				if ((av->averageHeightUppercase.count > CERTFORLOWHURDLE)
					&& (height > av->averageHeightUppercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				} else if ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightNormalLowercase.average * (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
					return 1;
				} else if ((av->averageHeightDigits.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightDigits.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				}
			}
			return 0;
		}
	} else if (av->averageHeightDigits.count == maxCount) {
		if (height > av->averageHeightDigits.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))
			return 1;
		else {
			if (lowHurdle) {
				if ((av->averageHeightUppercase.count > CERTFORLOWHURDLE)
					&& (height > av->averageHeightUppercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				} else if ((av->averageHeightNormalLowercase.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightNormalLowercase.average * (lowHurdle? OCR_TALL_TO_LOWERCASE_RATIO_LOWHURDLE:OCR_TALL_TO_LOWERCASE_RATIO))) {
					return 1;
				} else if ((av->averageHeightTallLowercase.count > CERTFORLOWHURDLE)
						   && (height > av->averageHeightTallLowercase.average * (1-(lowHurdle? OCR_ACCEPTABLE_ERROR_LOWHURDLE:OCR_ACCEPTABLE_ERROR)))) {
					return 1;
				}
			}
			return 0;
		}
	}
	return -1;
} // tallHeightTest

int countDigitLookalikes(SmartPtr<OCRRect> r, bool after, OCRResults *results, bool acceptDot) {
    if (r == NULL) return 0;
    int count = 0, lastGoodCount = 0;
    bool foundDot = false;
    SmartPtr<OCRRect> p = r;
    if (after) {
        while (p != NULL) {
            if (isDigitLookalikeExtended(p->ch))
                count++;
            else {
                if (acceptDot && !foundDot && (p->ch == '.')) {
                    foundDot = true;
                    p = p->next; continue;
                }
                // Not a digit lookalike
                if (p->ch == ' ') {
                    // Either return count thus far or accept/skip that space and keep going
                    // Check if we can accept this space (because we may have TWO spaces to be removed, looking at the first one
                    if ((p->previous != NULL) && (p->previous->ch != ' ') && (p->next != NULL) && (p->next->ch != ' '))  {
                        float spaceWidth = rectLeft(p->next->rect) - rectRight(p->previous->rect) - 1;
                        if ((results->globalStats.averageHeightDigits.count >= 10) && (spaceWidth < results->globalStats.averageHeightDigits.average * 0.25)) {
#if DEBUG
                            DebugLog("spacingAdjust: accepting space=%.0f between [%c] and [%c]) in [%s]", spaceWidth, (unsigned short)p->previous->ch, (unsigned short)p->next->ch, toUTF8(r->word->text()).c_str());
                            DebugLog("");
#endif
                            lastGoodCount = count; // Remember that value in case accepting spaces leads us to an unacceptable glued character later
                            p = p->next; continue;
                        }
                    }
                    return count;
                } else if ((p->ch != '.') && (p->ch != '*'))
                    count = 0;  // Not a character we can accept glued to digits
                // If we got here it means we found a non-digit different than ./* (in which case we set the count to 0) or ./* (in which case we left count untouched)
                // Either way, if we had a lastGoodCount (set when skipping a digit), return that rather than 0
                if (lastGoodCount > count)
                    return lastGoodCount;
                else
                    return count;
            }
            p = p->next;
        }
    } else {
        while (p != NULL) {
            if (acceptDot && !foundDot && (p->ch == '.')) {
                foundDot = true;
                p = p->previous; continue;
            }
            if (isDigitLookalikeExtended(p->ch))
                count++;
            else {
                if ((p->ch != ' ') && (p->ch != '.') && (p->ch != '*'))
                    count = 0;
                break;
            }
            p = p->previous;
        }
    }
    return count;
}

void testSpaceBetweenRects(SmartPtr<OCRWord> &line, SmartPtr<OCRRect> prev, SmartPtr<OCRRect> next, OCRResults *results) {
    OCRWordPtr statsWithoutNext(new OCRWord(line));
    // Now remove next char from stats
    statsWithoutNext->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
    
    if (!results->retailerParams.uniformSpacingDigitsAndLetters || !isLetterOrDigit(prev->ch) || !isLetterOrDigit(next->ch))
        return;

#if DEBUG
    if ((prev->ch=='B') && (next->ch=='1') && (prev->previous != NULL) && (prev->previous->ch == ' ')) {
        DebugLog("Found in word [%s]", toUTF8(prev->word->text()).c_str());
        DebugLog("");
    }
#endif

    float gap = rectLeft(next->rect) - rectRight(prev->rect) - 1;

    // Adjust gap if one of the two letters is abnormally narrow (could indicate partly erased letter or sliced vertically). For now do it only if we find pixels in the space between as confirmation that some part of a letter was sliced off
    if ((results->imageTests) && (statsWithoutNext->averageSpacing.average > 0) && (gap > results->globalStats.averageWidthDigits.average * 0.33)
        && (((gap > statsWithoutNext->averageSpacing.average * 1.15) && (prev->rect.size.height > results->globalStats.averageHeightDigits.average * 0.85) && (prev->rect.size.height > results->globalStats.averageHeightDigits.average * 0.85))
            || (gap > statsWithoutNext->averageSpacing.average * 2))) {
        bool narrowLeft = (isVerticalLine(prev->ch) && (results->globalStats.averageWidthDigit1.average > 0) && (prev->rect.size.width < results->globalStats.averageWidthDigit1.average * 0.85)) || (!isVerticalLine(prev->ch) && (prev->rect.size.width < results->globalStats.averageWidthDigits.average * 0.85));
        bool narrowRight = (isVerticalLine(next->ch) && (results->globalStats.averageWidthDigit1.average > 0) && (next->rect.size.width < results->globalStats.averageWidthDigit1.average * 0.85)) || (!isVerticalLine(next->ch) && (next->rect.size.width < results->globalStats.averageWidthDigits.average * 0.85));
        if (narrowLeft || narrowRight || (gap > statsWithoutNext->averageSpacing.average * 2)) {
            float minY = min(rectBottom(prev->rect), rectBottom(next->rect));
            float maxY = max(rectTop(prev->rect), rectTop(next->rect));
            CGRect gapRect (rectRight(prev->rect) + 1, minY, rectLeft(next->rect) - rectRight(prev->rect) - 1, maxY - minY - 1);
            SingleLetterTests *st = CreateSingleLetterTests(gapRect, results);
            if (st != NULL) {
                ConnectedComponentList cpl = st->getConnectedComponents();
                if ((cpl.size() > 1) && (cpl[0].getWidth() > 0) && (cpl[0].area > 6)) {
                    // Build the rect around only actual patterns
                    CGRect rectBetween (rectLeft(gapRect) + cpl[0].xmin, rectBottom(gapRect) + cpl[0].ymin, cpl[0].getWidth(), cpl[0].getHeight());
                    // Test if this is a '.', only in prices

                    //#|         111        |
                    //#|         1111       |
                    // Digit width = 12/height = 21, rectBetween w=4/h=2, space before = 8, space after = 6
                    if ((prev->ch !='.') && (next->ch != '.')) {
                        int nAfter = countDigitLookalikes(next, true, results);
                        int nBefore = countDigitLookalikes(prev, false, results);
                        if ((nAfter == 2) && (nBefore > 0)
                            && (rectTop(rectBetween) - rectTop(prev->rect) < results->globalStats.averageHeightDigits.average * 0.10)
                            && (rectTop(rectBetween) - rectTop(next->rect) < results->globalStats.averageHeightDigits.average * 0.10)
                            && (rectBetween.size.width < results->globalStats.averageWidthDigits.average * 0.50)
                            && (rectBetween.size.width > results->globalStats.averageWidthDigits.average * 0.15)
                            && (rectBetween.size.height < results->globalStats.averageHeightDigits.average * 0.30)
                            && (rectBetween.size.height > results->globalStats.averageHeightDigits.average * 0.08)
                            && (rectSpaceBetweenRects(prev->rect, rectBetween) > results->globalStats.averageHeightDigits.average * 0.20)
                            && (rectSpaceBetweenRects(rectBetween, next->rect) > results->globalStats.averageHeightDigits.average * 0.20)
                            ) {
                            ReplacingLog("testSpaceBetweenRects: rule 0600 inserting [.] between [%c] and [%c] in word [%s]", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str());
                            if (prev->next->ch == ' ') {
                                next->word->updateLetterWithNewCharAndNewRect(prev->next, '.', rectBetween);
                            } else {
                                wchar_t nextCh = next->ch;
                                CGRect nextRect = next->rect;
                                float nextConfidence = next->confidence;
                                next->word->updateLetterWithNewCharAndNewRect(next, '.', rectBetween);
                                line->addLetterWithRectConfidenceAfterRect(nextCh, nextRect, nextConfidence, next);
                                next->next->flags = next->flags; next->next->flags2 = next->flags2; next->next->flags3 = next->flags3; next->next->flags4 = next->flags4; next->next->flags5 = next->flags5; next->next->flags6 = next->flags6;
                                next->flags = next->flags2 = next->flags3 = next->flags4 = next->flags5 = next->flags6 = 0;
                            }
                            return;
                        }
                    }
                    // Now add this rect to either prev or next, depending of which one creates a suitable rect
                    CGRect rectPrev = CreateCombinedRect(prev->rect, rectBetween);
                    CGRect rectNext = CreateCombinedRect(rectBetween, next->rect);
                    bool updatePrev = false, updateNext = false;
                    // 2016-03-29 we were wrongly testing with on the current rect (e.g. next->rect) instead of the manufactured rect (e.g. rectNext)
                    bool notTooWideLeft = (isVerticalLine(prev->ch) && (results->globalStats.averageWidthDigit1.average > 0) && (rectPrev.size.width < results->globalStats.averageWidthDigit1.average * 1.15)) || (!isVerticalLine(prev->ch) && (prev->rect.size.width < results->globalStats.averageWidthDigits.average * 1.15));
                    bool notTooWideRight = (isVerticalLine(next->ch) && (results->globalStats.averageWidthDigit1.average > 0) && (rectNext.size.width < results->globalStats.averageWidthDigit1.average * 1.15)) || (!isVerticalLine(next->ch) && (next->rect.size.width < results->globalStats.averageWidthDigits.average * 1.15));
                    if (narrowLeft && notTooWideLeft && !(narrowRight && notTooWideRight)) {
                        updatePrev = true;
                    } else if (narrowRight && notTooWideRight && !(narrowLeft && notTooWideLeft)) {
                        updateNext = true;
                    } else if (notTooWideRight && (!notTooWideLeft || (rectNext.size.width < rectPrev.size.width))) {
                        updateNext = true;
                    } else if (notTooWideLeft && (!notTooWideRight || (rectPrev.size.width < rectNext.size.width))) {
                        updatePrev = true;
                    }
                    if (updatePrev) {
                        if (!(narrowRight || narrowLeft)) {
                            DebugLog("spacingAdjust: tested without narrow - widening rect of [%c] (next char [%c]) in [%s]", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str());
                        } else {
                            DebugLog("spacingAdjust: widening rect of [%c] (next char [%c]) in [%s]", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str());
//#if DEBUG
//                            if ((prev->ch == 'C') && (next->ch=='4')) {
//                                CGRect tmpRect = CreateCombinedRect(prev->rect, gapRect);
//                                SingleLetterTests *stTmp = CreateSingleLetterTests(tmpRect, results); if (stTmp != NULL) delete stTmp;
//                                DebugLog("");
//                            }
//#endif
                        }
                        char newCh = '\0';
                        if ((isVerticalLine(prev->ch) || (prev->ch == 'E')) && !(prev->flags4 & FLAGS4_TESTED_AS_DISCONNECTED_DIGIT6)) {
                            prev->flags4 |= FLAGS4_TESTED_AS_DISCONNECTED_DIGIT6;
                            OCRWordPtr statsWithoutCurrent(new OCRWord(line));
                            statsWithoutCurrent->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            newCh = testAsDisconnectedDigit(prev, rectPrev, statsWithoutCurrent, results, '6', '\0', false, false, true); // Indicate we are working with a single rect
                            if ((newCh != '\0') && (newCh != prev->ch)) {
                                ReplacingLog("ValidateLine: rule 0600 replacing [%c] with [%c] in word [%s]", (unsigned short)prev->ch, newCh, toUTF8(prev->word->text()).c_str());
                            }
                        }
                        if ((newCh == '\0') && (isVerticalLine(prev->ch) || (prev->ch == 'E')) && !(prev->flags5 & FLAGS5_TESTED_AS_DISCONNECTED_DIGIT5)) {
                            prev->flags5 |= FLAGS5_TESTED_AS_DISCONNECTED_DIGIT5;
                            OCRWordPtr statsWithoutCurrent(new OCRWord(line));
                            statsWithoutCurrent->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            newCh = testAsDisconnectedDigit(prev, rectPrev, statsWithoutCurrent, results, '5', '\0', false, false, true); // Indicate we are working with a single rect
                            if ((newCh != '\0') && (newCh != prev->ch)) {
                                ReplacingLog("ValidateLine: rule 0577 replacing [%c] with [%c] in word [%s]", (unsigned short)prev->ch, newCh, toUTF8(prev->word->text()).c_str());
                            }
                        }
                        if ((newCh == '\0') && ((prev->ch == 'C') || isVerticalLine(prev->ch)) && !(prev->flags4 & FLAGS4_TESTED_AS_DISCONNECTED_DIGIT0)) {
                            prev->flags4 |= FLAGS4_TESTED_AS_DISCONNECTED_DIGIT0;
                            OCRWordPtr statsWithoutCurrent(new OCRWord(line));
                            statsWithoutCurrent->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            newCh = testAsDisconnectedDigit(prev, rectPrev, statsWithoutCurrent, results, '0', '\0', false, false, true); // Indicate we are working with a single rect
#if DEBUG
                            if ((newCh != '\0') && (newCh != prev->ch)) {
                                ReplacingLog("ValidateLine: rule 0596 replacing [%c] with [%c] in word [%s]", (unsigned short)prev->ch, newCh, toUTF8(prev->word->text()).c_str());
                                DebugLog("");
                            }
#endif                            
                        }
                        // Don't update rect if we don't have a replacement char and no reason to think left or right chars have a too narrow rect
                        if ((newCh != '\0') || narrowLeft || isVerticalLine(prev->ch)) {
#if DEBUG
                            if (newCh != prev->ch)
                                ReplacingLog("ValidateLine: rule 0598 replacing [%c] with [%c] in word [%s]", (unsigned short)prev->ch, newCh, toUTF8(prev->word->text()).c_str());
#endif
                            prev->word->updateLetterWithNewCharAndNewRect(prev, ((newCh == '\0')? prev->ch:newCh), rectPrev);
                             if (newCh == '\0') prev->flags |= FLAGS_SUSPECT;
                        }
#if DEBUG
                        else {
                            SingleLetterTests *st = CreateSingleLetterTests(rectNext, results); if (st != NULL) delete st;
                            DebugLog("spacingAdjust: NOT widening rect of [%c] (next char [%c]) in [%s]", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str());
                            DebugLog("");
                        }
#endif
                    } else if (updateNext) {
                        if (!(narrowRight || narrowLeft)) {
                            DebugLog("spacingAdjust: tested without narrow - widening rect of [%c] (previous char [%c]) in [%s]", (unsigned short)next->ch, (unsigned short)prev->ch, toUTF8(prev->word->text()).c_str());
                        } else {
                            DebugLog("spacingAdjust: widening rect of [%c] (previous char [%c]) in [%s]", (unsigned short)next->ch, (unsigned short)prev->ch, toUTF8(prev->word->text()).c_str());
                        }
                        char newCh = '\0';
                        
                        //  -----------
                        //0|           |
                        //1|       11  |
                        //2|       11  |
                        //3|       111 |
                        //4|       111 |
                        //5|       11  |
                        //6|       11  |
                        //7|   1   11  |
                        //8|   1   11  |
                        //9|  11   11  |
                        //a|  11   11  |
                        //b|  11   11  |
                        //c| 111   11  |
                        //d| 111   11  |
                        //e| 11    11  |
                        //f| 111   111 |
                        //#| 111   111 |
                        //#| 11   1111 |
                        //#|  1   111  |
                        //#|      111  |
                        //#|      111  |
                        //#|      111  |
                        //#|      111  |
                        //#|       11  |
                        //#|           |
                        //  -----------
                        // Need to map the above as '4', not '0'
                        
                        if ((isVerticalLine(next->ch) || (next->ch == 'J')) && !(next->flags4 & FLAGS4_TESTED_AS_DISCONNECTED_DIGIT0)) {
                            next->flags4 |= FLAGS4_TESTED_AS_DISCONNECTED_DIGIT0;
                            OCRWordPtr statsWithoutNext(new OCRWord(line));
                            statsWithoutNext->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            // Set lax = true because we are pretty sure this rect needs to be wider, i.e. can't be a '1'!
                            newCh = testAsDisconnectedDigit(next, rectNext, statsWithoutNext, results, '0', '\0', false, true, true); // Indicate we are working with a single rect
                        }
                        if ((newCh == '\0') && isVerticalLine(next->ch) && !(next->flags3 & FLAGS3_TESTED_AS_DISCONNECTED_DIGIT4)) {
                            next->flags3 |= FLAGS3_TESTED_AS_DISCONNECTED_DIGIT4;
                            OCRWordPtr statsWithoutNext(new OCRWord(line));
                            statsWithoutNext->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            // Set lax = true because we are pretty sure this rect needs to be wider, i.e. can't be a '1'!
                            newCh = testAsDisconnectedDigit(next, rectNext, statsWithoutNext, results, '4', '\0', false, true, true); // Indicate we are working with a single rect
                        }
                        if ((newCh == '\0') && (next->ch == '3') && !(next->flags4 & FLAGS4_TESTED_AS_DISCONNECTED_DIGIT8)) {
                            next->flags4 |= FLAGS4_TESTED_AS_DISCONNECTED_DIGIT8;
                            OCRWordPtr statsWithoutNext(new OCRWord(line));
                            statsWithoutNext->updateStatsAddOrRemoveLetterAddingSpacingOnly(next, false, false);
                            // Set lax = 0 because we already got a digit, don't want to risk changing without cause
                            newCh = testAsDisconnectedDigit(next, rectNext, statsWithoutNext, results, '8', '\0', false, 0, true); // Indicate we are working with a single rect
                            if ((newCh != '\0') && (newCh != next->ch)) {
                                next->flags |= FLAGS_SUSPECT;
                                next->ch2 = '3';
                            }
                        }
                        // Don't update rect if we don't have a replacement char and no reason to think left or right chars have a too narrow rect
                        if ((newCh != '\0') || narrowRight || isVerticalLine(next->ch)) {
#if DEBUG
                        if (newCh != next->ch)
                            ReplacingLog("ValidateLine: rule 0598 replacing [%c] with [%c] in word [%s]", (unsigned short)next->ch, newCh, toUTF8(prev->word->text()).c_str());
#endif
                            next->word->updateLetterWithNewCharAndNewRect(next, ((newCh == '\0')? next->ch:newCh), rectNext);
                            if (newCh == '\0') next->flags |= FLAGS_SUSPECT;
                        }
#if DEBUG
                        else {
                            SingleLetterTests *st = CreateSingleLetterTests(rectNext, results); if (st != NULL) delete st;
                            DebugLog("spacingAdjust: NOT widening rect of [%c] (previous char [%c]) in [%s]", (unsigned short)next->ch, (unsigned short)prev->ch, toUTF8(prev->word->text()).c_str());
                            DebugLog("");
                        }
#endif
                    }
                }
                delete st;
            } // st != NULL
        }
    }
}

// Effective gap is the actual gap minus the average spacing between adjacent letters
// Return true if clear space
bool spacingAdjustCalculateValues(SmartPtr<OCRWord> line, retailer_t *retailerParams, SmartPtr<OCRRect> prev, SmartPtr<OCRRect> next, bool isURLOrEmail, float *gap, float *realGap, float *effectiveGap,
								  float *gapDividedByPreviousCharWidth, float *gapDividedByAverageWidth, float *effectiveGapDividedByAverageWidth,
								  float *realGapDividedByAverageWidth, bool *bothDigits, bool *clearlyNoSpace, bool *reallyClearSpace, OCRResults *results)
{
        *clearlyNoSpace = false;
    
        bool hadSpaceBetween = (prev->next->ch == ' ');
        testSpaceBetweenRects(line, prev, next, results);
    
		*gap = rectLeft(next->rect) - rectRight(prev->rect) - 1;
        *realGap = *gap;
        if (hadSpaceBetween && (prev->next->ch != ' ')) {
            // There was a space, but no more - abort
            *effectiveGap = *realGap = *gap;
            *reallyClearSpace = true;
            return true;
        }
    
        // Around dots, if space is less than the max space observed around dots, declare NO space. Note: on receipts spaces around dots are rare anyhow.
        if ((retailerParams->maxSpaceAroundDotsAsPercentOfWidth > 0)
            && ((((prev->ch == '.') || (prev->ch == ':')) && (next->ch != '.') && (next->ch != ':')) || ((prev->ch != '.') && (prev->ch != ':') && ((next->ch == '.') || (next->ch == ':'))))
            && (*gap < retailerParams->maxSpaceAroundDotsAsPercentOfWidth * results->globalStats.averageWidthDigits.average * 1.15)) {
            *reallyClearSpace = false;
            *clearlyNoSpace = true;
            return false;
        }
        // Around dots, if space is less than the min space observed around dots, declare NO space. This test is mostly for retailers where we haven't set the maxSpaceAroundDots, so we still can declare no space for tight spaces around dots - so tight they even come close to the minSpaceAroundDots. Note: on receipts spaces around dots are rare anyhow.
        if ((retailerParams->minSpaceAroundDotsAsPercentOfWidth > 0)
            && (((prev->ch == '.') && (next->ch != '.')) || ((prev->ch != '.') && (next->ch == '.')))
            && (*gap < retailerParams->minSpaceAroundDotsAsPercentOfWidth * results->globalStats.averageWidthDigits.average)) {
            *reallyClearSpace = false;
            *clearlyNoSpace = true;
            return false;
        }
    
#if DEBUG
// 0795671 1Q05
        if ((prev->ch == '1') && (next->ch == '1') && (prev->previous != NULL) && (prev->previous->ch == '7')) {
            CGRect combinedRect = CreateCombinedRect(prev->rect, next->rect);
            SingleLetterTests *st = CreateSingleLetterTests(combinedRect, results); if (st != NULL) delete st;
            DebugLog("Found in [%s]", toUTF8(prev->word->text()).c_str());
            DebugLog("");
        }
#endif
        // Special test: eliminate narrow space blocking a sequence from making up a product number
        // TODO do we need to do the same for prices? There might already be code to that effect in OCRValidate around "test for price"
        int nBefore = countDigitLookalikes(prev, false, results);
        int nAfter = countDigitLookalikes(next, true, results);
        if ((nBefore > 0) && (nAfter > 0) && (nBefore + nAfter  == results->retailerParams.productNumberLen)) {
            // Accept gap = 5 where height = 23 / width = 10
            if ((results->globalStats.averageHeightDigits.count >= 10)
                && ((*gap < results->globalStats.averageHeightDigits.average * 0.25)
                    // 2016-03-08 see https://drive.google.com/open?id=0B4jSQhcYsC9VdVpqUGJueDdVRW8 2nd product gap = 5 between the two 1's with width = 5.3
                    || (*gap < results->globalStats.averageWidthDigits.average * 1.15))) {
                *clearlyNoSpace = true;
                DebugLog("spacingAdjust: declaring no space between [%c] and [%c] in [%s] to make up product number", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str());
                return false;
            }
        }

        bool isDigitPrev = isDigit(prev->ch, true); bool isDigitNext = isDigit(next->ch, true);
    
        // PQPQ why not good if both prev & next are like 'I'?
		*bothDigits = ((isDigitPrev && (isDigitNext || isLike1(next->ch) || isLike1(next->ch)))
                || (isDigitNext && (isDigitPrev || isLike1(prev->ch) || isLike1(prev->ch))));
    
        //2015-11-05 17:08:06.013 Windfall[18743:1359926] #1138: 2 quality: 87 [341,935 - 348,949] w=8,h=15]
        //2015-11-05 17:08:06.013 Windfall[18743:1359926] #1139: . quality: 91 [352,947 - 354,949] w=3,h=3]
        //2015-11-05 17:08:06.013 Windfall[18743:1359926] #1140: 0 quality: 83 [359,935 - 366,949] w=8,h=15]
        //2015-11-05 17:08:06.013 Windfall[18743:1359926] #1141: 0 quality: 87 [368,935 - 375,949] w=8,h=15]
        // Two digits separated by a gap larger than the width of a digit - clearly a space!!!
        if (*bothDigits && (line->averageWidthDigits.count >=2) && (*gap >= line->averageWidthDigits.average) && (*gap >= next->rect.size.width) && (*gap >= prev->rect.size.width)) {
            *reallyClearSpace = true;
            *clearlyNoSpace = false;
            return true;
        }
    
        AverageWithCount averageSpacingWithoutCurrent;
        if (retailerParams->uniformSpacingDigitsAndLetters) {
            averageSpacingWithoutCurrent = line->averageSpacing;
        } else {
            // PQTODO the below code is faulty because between say 4 and O, we consider it digits even though O is not 0, so we take the average spacing between digits yet right after we substract the current space from the average even though it was NOT taken into account when calculating that average. Also, we may end up with an average with count 0 if there was only one space taken into account ...
            averageSpacingWithoutCurrent = (*bothDigits? line->averageSpacingDigits:line->averageSpacing);
            if (averageSpacingWithoutCurrent.count > 0) {
                averageSpacingWithoutCurrent.sum -= *gap;
                averageSpacingWithoutCurrent.count--;
                if (averageSpacingWithoutCurrent.count > 0)
                    averageSpacingWithoutCurrent.average = averageSpacingWithoutCurrent.sum / averageSpacingWithoutCurrent.count;
                else
                    averageSpacingWithoutCurrent.average = 0;
            }
        }
    
		*effectiveGap = *gap - averageSpacingWithoutCurrent.average;
        float applicableOtherWidth = 0;
        // Evaluate gap relative to adjacent letters
        // & tends to be large and be surrounded by spaces - assess relative to other letter
        if (prev->ch == '&')
            applicableOtherWidth = next->rect.size.width;
        else if (next->ch == '&')
            applicableOtherWidth = prev->rect.size.width;
        else {
            // Seems right to take the largest of the two letters: in say sE with a large 'E' we expect the space to be larger because of the E
            // 09/21/2015 if next or previous is '1', need to try to find another non-1 digit, so we get a normal width! TODO: same reasoning applies to lowercase text where some letters are narrow like 'l'.
            float normalPreviousWidth = prev->rect.size.width;
            if ((prev->ch == '1') && (line->averageWidthDigits.count > 0)) {
                normalPreviousWidth = line->averageWidthDigits.average;
            }
            float normalNextWidth = next->rect.size.width;
            if ((next->ch == '1') && (line->averageWidthDigits.count > 0)) {
                normalNextWidth = line->averageWidthDigits.average;
            }
 
            applicableOtherWidth = MAX(normalPreviousWidth, normalNextWidth);
        }
        *gapDividedByPreviousCharWidth = ((applicableOtherWidth != 0)? *gap/applicableOtherWidth:0);
        float applicableWidth = 0;
		if (*bothDigits && (line->averageWidthDigits.count > 0)) {
            applicableWidth = line->averageWidthDigits.average;
			*gapDividedByAverageWidth = *gap/applicableWidth;
			*effectiveGapDividedByAverageWidth = *effectiveGap/applicableWidth;
			*realGapDividedByAverageWidth = *realGap/applicableWidth;
		} else {
			// First, decide which of the width averages applies - depends on the character before the space
			if (isLower(prev->ch))
				applicableWidth = line->averageWidthNormalLowercase.average;
			if ((applicableWidth == 0) && isUpper(prev->ch))
				applicableWidth = line->averageWidthUppercase.average;
			if ((applicableWidth == 0) && isDigit(prev->ch))
				applicableWidth = line->averageWidthDigits.average;
			if (applicableWidth == 0)
				applicableWidth = line->averageWidthWideChars.average;
			*gapDividedByAverageWidth = ((applicableWidth != 0)? *gap/applicableWidth:0);
			*effectiveGapDividedByAverageWidth = ((applicableWidth != 0)? *effectiveGap/applicableWidth:0);
			*realGapDividedByAverageWidth = ((applicableWidth != 0)? *realGap/applicableWidth:0);
		}
    
    // We are done computing averages, time to make decisions
    
    // Spurious space between '.' and ','
    // realGapDividedByAverageWidth = 0.30, realGap / averageWidthNormalLowercase = 6/18.5 = 0.32, realGap / averageWidthUppercase = 6/21.67 = 0.28
    if (((prev->ch == '.') || (prev->ch == ',')) && ((next->ch == '.') || (next->ch == ','))) {
        if ((*realGapDividedByAverageWidth < 0.25)
            || ((line->averageWidthNormalLowercase.count >= 2) && (*realGap/line->averageWidthNormalLowercase.average < 0.35))
            || ((line->averageWidthUppercase.count >= 2) && (*realGap/line->averageWidthUppercase.average < 0.32))) {
            *clearlyNoSpace = true;
            return false;
        }
    }
    
    if ((*gapDividedByAverageWidth < 0.65) && ((next->ch == ',') || (next->ch == '.'))) {
        // If comma is last on the line and gap not too wide, declare no space
        if (next->next == NULL) {
            if (*gapDividedByAverageWidth < 0.55) {
                *clearlyNoSpace = true;
                return false;
            }
        } else {
            // next->next != NULL
            float nextGap = 0;
            if (next->next->ch != ' ') {
                nextGap = rectSpaceBetweenRects(next->rect, next->next->rect);
            } else if (next->next->ch == ' ') {
                if (next->next->next != NULL) {
                    nextGap = rectSpaceBetweenRects(next->rect, next->next->next->rect);
                } else {
                    // Same as above case where comma is last one line (ignore extra space)
                    if (*gapDividedByAverageWidth < 0.55) {
                        *clearlyNoSpace = true;
                        return false;
                    }
                }
            }
            if ((nextGap > 0) && (nextGap > *gap * 2.5)) {
                *clearlyNoSpace = true;
                return false;
            }
        }
    }
    
    if ((tallHeightTest(line.getPtr(), prev->rect.size.height, '\0', false, 0, true) == 1)
        && (tallHeightTest(line.getPtr(), next->rect.size.height, '\0', false, 0, true) == 1))
    {
#if DEBUG
            if ((prev->ch=='H') && (next->ch=='G')) {
                DebugLog("Found in [%s]", toUTF8(prev->word->text()).c_str());
                DebugLog("");
            }
#endif
        if (line->averageWidthDigits.average > 0) 
        {
            if ((isVerticalLine(prev->ch) && (prev->ch != '/') && (prev->ch != '\\'))
                && (isVerticalLine(next->ch) && (next->ch != '/') && (next->ch != '\\'))) 
            {
                if (*gap < line->averageWidthDigits.average * 0.89) {
                    *clearlyNoSpace = true;
                    return false;
                }
            }
            else if ((isDigit(prev->ch) || (isVerticalLine(prev->ch) && (prev->ch != '/') && (prev->ch != '\\')) || (prev->ch == 'O'))
                && (isDigit(next->ch) || (isVerticalLine(next->ch) && (next->ch != '/') && (next->ch != '\\')) || (next->ch == 'O'))) 
            {
                if ((*gap < line->averageWidthDigits.average * 0.48)
                    || ((*gap < line->averageWidthDigits.average * 0.63) && (*gap < line->averageSpacingDigits.average * 3))) {
                    // Rule 1001
                    *clearlyNoSpace = true;
                    return false;
                }
            }
        }
        // No digits on line but could be something like "IOOOI" - treat vertical tall letters next to 'O' as if if was a digit
        else if ((line->averageWidthUppercase.average > 0)
                && ((isDigit(prev->ch) || (isVerticalLine(prev->ch) && (prev->ch != '/') && (prev->ch != '\\')) || (prev->ch == 'O'))
                && (isDigit(next->ch) || (isVerticalLine(next->ch) && (next->ch != '/') && (next->ch != '\\')) || (next->ch == 'O')))) 
        {
            if (*gap < line->averageWidthUppercase.average * 0.40) {
                *clearlyNoSpace = true;
                return false;
            }
        }
    }
    
    if ( ((prev->ch != '1') || (next->ch != '1'))
        && ((line->averageWidth.count >= 4) && (*realGapDividedByAverageWidth > 0.45))
        // Tolerate large gap around dots
        && (((prev->ch != '.') && (next->ch != '.')) || (*gap > line->averageHeightDigits.average * 0.52)) )
    {
        // e.g. clear space with spacing=19 vs average spacing=4 (x4.75)
        if ((line->averageSpacing.count >= 3) && (*gap > line->averageSpacing.average * 3)) {
            bool sayClearSpace = true;
            if (next->next != NULL) {
                float spaceWithNext;
                if (next->next->ch != ' ') {
                    spaceWithNext = rectSpaceBetweenRects(next->rect, next->next->rect);
                    if (spaceWithNext > *gap * 1.35)
                        sayClearSpace = false;
                } else if (next->next->next != NULL) {
                    spaceWithNext = rectSpaceBetweenRects(next->rect, next->next->next->rect);
                    if (spaceWithNext > *gap * 1.35)
                        sayClearSpace = false;
                }
            }
            if (sayClearSpace) {
                if (*gap > line->averageSpacing.average * 4)
                    *reallyClearSpace = true;
                return true;
            }
        }
        // For dashes apparently first condition a few lines above is enough (no idea why)
        if (isDash(prev->ch) || isDash(next->ch))
            return true;
    }
    
	// Test if gap is so much larger than other spaces in this line that it's clearly a space
	if (*bothDigits
		 && (line->averageSpacingDigits.count > 3)
         // 2015-06-07 got one case where gap was 12 versus average 4.6 => 2.60. Increasing 2.5x to 2.65x
		 && (*gap > line->averageSpacingDigits.average * 2.65)
		 && (*gap > line->averageWidthDigits.average * 0.51)
		) {
		return true;
	} else if (!*bothDigits 
                // Not sure why we hesitate at fewer than 5 spaces
               && ((line->averageSpacing.count >= 5) 
                        // Short word and lowercase followed by uppercase? Very likely a space
                        || (isLower(prev->ch) && isUpper(next->ch)))
               // Require gap > 0 in order not to declare spaces within italics words
               && (*gap > 0)
               ) {
        if ((line->averageSpacing.average < 0)
            && (*gap > applicableWidth * (isURLOrEmail? 0.25:0.23))) {
            return true;
        }
        
		if ((*gap > line->averageSpacing.average * (isURLOrEmail? 2.7:2.5))
		    // Add requirement for some minimum width relative to char width
			&& (*realGapDividedByAverageWidth > (isURLOrEmail? 0.60:0.35))) {
			return true;
		} else if ( (*gap > line->averageSpacing.average * 4)
                 && ((*realGapDividedByAverageWidth > 0.31)
                     || (!isURLOrEmail && (*realGapDividedByAverageWidth > 0.30)))
                   ) {
			return true;
		} else if ((*gap > line->averageSpacing.average * 6)
            && ((*realGapDividedByAverageWidth > 0.30)
                || (!isURLOrEmail && (*realGapDividedByAverageWidth > 0.29)))) {
			return true;
		}
        if ((line->averageHeightDigits.count + line->averageHeightUppercase.count >= 5) && (isUpper(prev->ch) || isDigit(prev->ch)) && (isUpper(next->ch) || isDigit(next->ch))) {
            float maxWidth;
            // Try to use digit width if available, less variable (e.g. 'W' and 'V' are wider than 'S')
            if (line->averageHeightDigits.count >= 5) {
                maxWidth = line->averageWidthDigits.average;
            } else {
                maxWidth = MAX(line->averageWidthDigits.average, line->averageWidthUppercase.average);
            }
            // 2016-02-05 below code inserted a space between product and "KI" in "OOOOO00O4131KI" with gap = 6 and width=11. With ratio set to 0.50 gap was larger - adjusting
            float minRequiredRatioToWidth = 0.60;
            if ((*gap > line->averageSpacing.average * 2)
                && (*gap > maxWidth * minRequiredRatioToWidth)
                && (*effectiveGap > maxWidth * 0.25))  {
                if ((next->previous != NULL) && (next->previous->ch != ' ')) {
                    // Print message only when there wasn't a space already, i.e. the new rule will cause one to be added
                    DebugLog("validate: replacing - new rule declaring a clear space between [%c] and [%c] in [%s], gap=%d, effective gap=%.2f, average uppercase/digits width=%.2f", (unsigned short)prev->ch, (unsigned short)next->ch, toUTF8(prev->word->text()).c_str(), (int)*gap, *effectiveGap, maxWidth);
                }
                return true;
            }
        }
	}
	return false;
} // spacingAdjustCalculateValues

float heightBelow (SmartPtr<OCRRect> r, bool upsideDown) {
	float heightBelow = 0;
	float count = 0;
	// Calculate the average delta by which this i extends below line relative to rect before and/or after
	if ((r->previous != NULL) && isLetterOrDigit(r->previous->ch) && !extendsBelowLine(r->previous->ch)) {
		heightBelow = (upsideDown?  rectTop(r->rect) - rectTop(r->previous->rect) : rectBottom(r->previous->rect) - rectBottom(r->rect));
		count++;
	}
	if ((r->next != NULL) && isLetterOrDigit(r->next->ch) && !extendsBelowLine(r->next->ch)) {
		heightBelow = (heightBelow + (upsideDown? (rectTop(r->rect) - rectTop(r->next->rect)):(rectBottom(r->next->rect) - rectBottom(r->rect))) );
		count++;
	}
	if (count == 0) {
		if ((r->previous != NULL) && (r->previous->previous != NULL) && isLetterOrDigit(r->previous->previous->ch) && !extendsBelowLine(r->previous->previous->ch)) {
			heightBelow = (upsideDown?  rectTop(r->rect) - rectTop(r->previous->previous->rect) : rectBottom(r->previous->previous->rect) - rectBottom(r->rect));
			count++;
		}
		if ((r->next != NULL) && (r->next->next != NULL) && isLetterOrDigit(r->next->next->ch) && !extendsBelowLine(r->next->next->ch)) {
			heightBelow = (heightBelow + (upsideDown? (rectTop(r->rect) - rectTop(r->next->next->rect)):(rectBottom(r->next->next->rect) - rectBottom(r->rect))) );
			count++;
		}
	}
	if (count == 0) {
		return 0;
	} else {
		return (heightBelow/count);
	}
}

// T ' NJ TAX
// If per is true calculates the gap in terms of % height of relevant letters(s)
float gapAboveBaselinePercent (SmartPtr<OCRRect> r, CGRect &rect, bool per, bool skipOne) {
	int numNeigbors = 0;
	float sumAbove = 0;

	if (r->previous != NULL) {
        SmartPtr<OCRRect> rPrev = r->previous;
        bool doit = false;
        if ((rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallAbove(rPrev->ch)) {
            doit = true;
        } else {
            rPrev = r->previous->previous;
            if ((rPrev != NULL) && (rPrev->ch != ' ') && isLetterOrDigit(rPrev->ch) && !isTallAbove(rPrev->ch)) {
                doit = true;
            }
        }
        if (doit) {
            float delta = rectTop(rPrev->rect)-rectTop(rect);
            if (per) {
                sumAbove += delta / rPrev->rect.size.height;
            } else {
                sumAbove += delta;
            }
            numNeigbors++;
        }
	}
    
    if (r->next != NULL) {
        SmartPtr<OCRRect> rNext = r->next;
        bool doit = false;
        if ((rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallAbove(rNext->ch)) {
            doit = true;
        } else {
            rNext = r->next->next;
            if ((rNext != NULL) && (rNext->ch != ' ') && isLetterOrDigit(rNext->ch) && !isTallAbove(rNext->ch)) {
                doit = true;
            }
        }
        if (doit) {
            float delta = rectTop(rNext->rect)-rectTop(rect);
            if (per) {
                sumAbove += delta / rNext->rect.size.height;
            } else {
                sumAbove += delta;
            }
            numNeigbors++;
        }
	}
    
	if (numNeigbors == 0)
		return -1000;
	else {
		return (sumAbove/numNeigbors);
	}
} // gapAboveBaselinePercent

float gapAboveBaselinePercent (SmartPtr<OCRRect> r, ConnectedComponent &cc, bool per, bool skip) {
    CGRect tmpRect(rectLeft(r->rect)+cc.xmin, rectBottom(r->rect)+cc.ymin, cc.getWidth(), cc.getHeight());
    return (gapAboveBaselinePercent(r, tmpRect, per, skip));
}

float gapAboveBaseline (SmartPtr<OCRRect> r) {
	return gapAboveBaselinePercent (r, r->rect, false);
}

float gapAboveBaselineOfOtherRect(SmartPtr<OCRRect> r, SmartPtr<OCRRect> referenceR) {
	return (rectTop(referenceR->rect)-rectTop(r->rect));
}

float gapBelowToplineOfOtherRect(SmartPtr<OCRRect> r, SmartPtr<OCRRect> referenceR) {
	return (rectBottom(r->rect)-rectBottom(referenceR->rect));
}

// Returns -1000 if a determination can't be made because chars around are not letters or are tall below letters ('p', 'q')
// If next or previous is a space or a tall below letter, attempts to skip it. I think it's OK to skip one letter, even on lines with a slope
// nRects means how many letters to the right only we need to ignore (and nRect == 1 mean don't ignore any ... makes no sense but leaving for now)
// Otherwise returns the gap in pixels (could be negative if the letter is ABOVE the baseline
float gapBelow (SmartPtr<OCRRect> r, int nRects, OCRWordPtr statsWithoutCurrent) {
	int numNeigbors = 0;
	float sumBelow = 0;

    SmartPtr<OCRRect> prev = r->previous;
    if ((prev != NULL) && ((prev->ch == ' ') || isTallBelow(prev->ch)))
        prev = prev->previous;
	if ((prev != NULL) && (isLetter(prev->ch) || isDigit(prev->ch) || (prev->ch == '.')) && !isTallBelow(prev->ch)) {
        // Test below essentially ignores the previous letter for the purpose of gap calculation if it's a normal lowercase letter that it abnormally higher than other normal lowercase letters
        if ((statsWithoutCurrent == NULL) || !(isLower(prev->ch) && !isTallAbove(prev->ch) && (statsWithoutCurrent->averageHeightNormalLowercase.count > 1) && (prev->rect.size.height > statsWithoutCurrent->averageHeightNormalLowercase.average * (1 + OCR_ACCEPTABLE_ERROR)))) {
            sumBelow += (rectTop(r->rect)-rectTop(prev->rect));
            numNeigbors++;
        }
	}
    
    SmartPtr<OCRRect> next = r->next;
    if ((nRects > 1) && (next != NULL))
        next = next->next;
    if ((next != NULL) && ((next->ch == ' ') || isTallBelow(next->ch)))
        next = next->next;
	if ((next != NULL) && (isLetter(next->ch) || isDigit(next->ch) || (next->ch == '.')) && !isTallBelow(next->ch)) {
        // Test below essentially ignores the previous letter for the purpose of gap calculation if it's a normal lowercase letter that it abnormally higher than other normal lowercase letters
        if ((statsWithoutCurrent == NULL) || !(isLower(next->ch) && !isTallAbove(next->ch) && (statsWithoutCurrent->averageHeightNormalLowercase.count > 1) && (next->rect.size.height > statsWithoutCurrent->averageHeightNormalLowercase.average * (1 + OCR_ACCEPTABLE_ERROR)))) {
            sumBelow += (rectTop(r->rect)-rectTop(next->rect));
            numNeigbors++;
        }
	}
	if (numNeigbors == 0)
		return -1000;
	else {
		return (sumBelow/numNeigbors);
	}
} // gapBelow

bool looksSameAsUppercase(wchar_t ch, OCRResults *results, bool *isAccentedPtr) {
    if (looksSameAsUppercaseCharset.find(ch) != wstring::npos)
        return true;
    // If not English try some more special chars
    if (results->languageCode != LanguageENG) {
        if (looksSameAsUppercaseCharsetIntlUTF32.length() == 0) {
            if (looksSameAsLowercaseCharsetIntlUTF32.length() == 0)
                looksSameAsLowercaseCharsetIntlUTF32 = toUTF32(looksSameAsLowercaseCharsetIntlUTF8);
            looksSameAsUppercaseCharsetIntlUTF32 = toLower(looksSameAsLowercaseCharsetIntlUTF32);
        }
        if (looksSameAsUppercaseCharsetIntlUTF32.find(ch) != wstring::npos) {
            if (isAccentedPtr != NULL)
                *isAccentedPtr = true;
            return true;
        } else {
            if (isAccentedPtr != NULL)
                *isAccentedPtr = false;
            return false;
        }
    }
    return false;
} // looksSameAsUppercase

// Returns the maximum height we should consider for the font used in the current line. If no tall letters are present, will use the height of normal lowercase letter muptiplied by a factor of 1.36 observed with common fonts
float maxApplicableHeight (OCRWordPtr stats) {
	float referenceHeight = 0;
	if (stats->averageHeightUppercase.count > 2) {
		referenceHeight = MAX(referenceHeight, stats->averageHeightUppercase.average);	
#if DEBUG
		DebugLog("Validate: single-letter, checking height against averageHeightUppercase [%f], count=%d",
				stats->averageHeightUppercase.average, stats->averageHeightUppercase.count);
#endif		
	}
	if (stats->averageHeightTallLowercase.count > 2) {		
		referenceHeight = MAX(referenceHeight, stats->averageHeightTallLowercase.average);
#if DEBUG
		DebugLog("Validate: single-letter, checking height against averageHeightTallLowercase [%f], count=%d",
				stats->averageHeightTallLowercase.average, stats->averageHeightTallLowercase.count);
#endif				
	}
	if (stats->averageHeightDigits.count > 2) {
		referenceHeight = MAX(referenceHeight, stats->averageHeightDigits.average);
#if DEBUG
		DebugLog("Validate: single-letter, checking height against averageHeightDigits [%f], count=%d",
				stats->averageHeightDigits.average, stats->averageHeightDigits.count);		
#endif				
	}
	if (stats->averageHeightNormalLowercase.count > 2) {
        float tmpHeight = stats->averageHeightNormalLowercase.average * 1.36;
		referenceHeight = MAX(referenceHeight, tmpHeight);
#if DEBUG
		DebugLog("Validate: single-letter, checking height against averageHeightNormalLowercase*1.36 [%f], count=%d",
				stats->averageHeightNormalLowercase.average*1.36, stats->averageHeightNormalLowercase.count);
#endif				
	}
	return referenceHeight;
} // maxApplicableHeight

/* TestCriteria:
 -1: success if less than average
 0: success if +/- same as average
 +1: success if greater than average
 Returns 1 if it has enough stats regarding the type of character ch is and its width is within a marging or error, 0 otherwise
 Return -1 if it doesn't enough stats to tell
*/
int widthTest (OCRStats *av, SmartPtr<OCRRect> r, float width, char ch, int certLevel, int testCriteria) {
	if (r != NULL) {
		// First test: do we have the same letter in current line?
		SmartPtr<OCRRect> tmpR = r->previous;
		while ((tmpR != NULL) && (tmpR->ch != '\n')) {
			// Found same char!
			if (tmpR->ch == ch) {
				bool sameOrSmaller = (r->rect.size.width < tmpR->rect.size.width * (1+OCR_ACCEPTABLE_ERROR));
				bool sameOrLarger = (r->rect.size.width > tmpR->rect.size.width * (1-OCR_ACCEPTABLE_ERROR));
				if (testCriteria == -1)
					return sameOrSmaller;
				else if (testCriteria == 1)
					return sameOrLarger;
				else
					return (sameOrLarger && sameOrSmaller);
			}
			tmpR = tmpR->previous;
		}

		tmpR = r->next;
		while ((tmpR != NULL) && (tmpR->ch != '\n')) {
			// Found same char!
			if (tmpR->ch == ch) {
				bool sameOrSmaller = (r->rect.size.width < tmpR->rect.size.width * (1+OCR_ACCEPTABLE_ERROR));
				bool sameOrLarger = (r->rect.size.width > tmpR->rect.size.width * (1-OCR_ACCEPTABLE_ERROR));
				if (testCriteria == -1)
					return sameOrSmaller;
				else if (testCriteria == 1)
					return sameOrLarger;
				else
					return (sameOrLarger && sameOrSmaller);
			}
			tmpR = tmpR->next;
		}
	}

	if (width == 0)
		width = r->rect.size.width;

	if (islower(ch)) {
	// Normal, not wide lowercase char
		if (isNormalWidthLowercase(ch)) {
			if (av->averageWidthNormalLowercase.count <= certLevel)
				return -1;
			bool sameOrSmaller = (width < av->averageWidthNormalLowercase.average * (1+OCR_ACCEPTABLE_ERROR_WIDTH));
			bool sameOrLarger = (width > av->averageWidthNormalLowercase.average * (1-OCR_ACCEPTABLE_ERROR_WIDTH));
			if (testCriteria == -1)
				return sameOrSmaller;
			else if (testCriteria == 1)
				return sameOrLarger;
			else
				return (sameOrLarger && sameOrSmaller);
		} else if (isNarrow(ch)) {
			if (av->averageWidthNarrowChars.count <= certLevel)
				return -1;
			bool sameOrSmaller = (width < av->averageWidthNarrowChars.average * (1+OCR_ACCEPTABLE_ERROR_WIDTH));
			bool sameOrLarger = (width > av->averageWidthNarrowChars.average * (1-OCR_ACCEPTABLE_ERROR_WIDTH));
			if (testCriteria == -1)
				return sameOrSmaller;
			else if (testCriteria == 1)
				return sameOrLarger;
			else
				return (sameOrLarger && sameOrSmaller);
		} else {
			return -1;
		}
	}

	// Uppercase or digit
	if (isupper(ch) || isDigit(ch)) {
		int maxCount = MAX(av->averageWidthUppercase.count, av->averageWidthDigits.count);
		if (maxCount <= certLevel)
			return -1;
		float applicableAv = ((av->averageWidthUppercase.count > av->averageWidthDigits.count)?
							  av->averageWidthUppercase.average:av->averageWidthDigits.average);
		bool sameOrSmaller = (width < applicableAv * (1+OCR_ACCEPTABLE_ERROR_WIDTH));
		bool sameOrLarger = (width > applicableAv * (1-OCR_ACCEPTABLE_ERROR_WIDTH));
		if (testCriteria == -1)
			return sameOrSmaller;
		else if (testCriteria == 1)
			return sameOrLarger;
		else
			return (sameOrLarger && sameOrSmaller);
	}

	return -1;
} // widthTest

int countLongestDigitsSequence (const wstring& text) {
	int cnt = 0; int maxCount = 0;
	for (int i=0; i<text.length(); i++) {
		wchar_t ch = text[i];
		if (isDigit(ch)) {
			cnt++;
		} else {
			if (cnt > maxCount)
				maxCount = cnt;
			cnt = 0;
		}
	}
	if (cnt > maxCount)
		maxCount = cnt;
	return maxCount;
}

int countLongestLettersSequence (const wstring& text) {
	int cnt = 0; int maxCount = 0;
	for (int i=0; i<text.length(); i++) {
		wchar_t ch = text[i];
		if (isLetter(ch)) {
			cnt++;
		} else {
			if (cnt > maxCount)
				maxCount = cnt;
			cnt = 0;
		}
	}
	if (cnt > maxCount)
		maxCount = cnt;
	return maxCount;
}

int countLongestLettersAndDigitsSequenceAfter (SmartPtr<OCRRect> r) {
    if (r == NULL) return 0;
	SmartPtr<OCRRect> tmpR = r->next;
	int count = 0;
    int maxCount = 0;
	while ((tmpR != NULL) && (tmpR->ch != '\n')) {
		if (isLetterOrDigit(tmpR->ch))
			count++;
        else {
            // Sequence interrupted - see if we got a long one
            if (count > maxCount) {
                maxCount = count;
            }
            count = 0;
        }
		tmpR = tmpR->next;
	}
    // Just in case longest sequence ended at the end of line
    if (count > maxCount) {
        maxCount = count;
    }
	return maxCount;
} // countLongestLettersAndDigitsSequenceAfter

int countDigitsBefore(SmartPtr<OCRRect> r) {
    if (r == NULL) return 0;
	SmartPtr<OCRRect> tmpR = r->previous;
	int count = 0;
	while ((tmpR != NULL) && ((tmpR->ch != ' ') && (tmpR->ch != '\n'))) {
		if (isDigit(tmpR->ch))
			count++;
		tmpR = tmpR->previous;
	}
	return count;
}

int countDigitsAfter(SmartPtr<OCRRect> r) {
    int count = 0;
    if (r == NULL) return 0;
	SmartPtr<OCRRect> tmpR = r->next;
	while ((tmpR != NULL) && ((tmpR->ch != ' ') && (tmpR->ch != '\n'))) {
		if (isDigit(tmpR->ch))
			count++;
		tmpR = tmpR->next;
	}
	return count;
}

int countDigitsInWord(SmartPtr<OCRRect> r) {
    if (r == NULL) return 0;
	return (countDigitsBefore(r) + countDigitsAfter(r) + (isDigit(r->ch)? 1:0));
} // countDigitsInWord

int countCharsInWord(SmartPtr<OCRRect> r) {
    if (r == NULL) return 0;
	SmartPtr<OCRRect> tmpR = r;
	int count = 0;
	while ((tmpR != NULL) && ((tmpR->ch != ' ') && (tmpR->ch != '\n'))) {
		count++;
		tmpR = tmpR->previous;
	}
	tmpR = r->next;
	while ((tmpR != NULL) && ((tmpR->ch != ' ') && (tmpR->ch != '\n'))) {
		count++;
		tmpR = tmpR->next;
	}
	return count;
} // countCharsInWord

int countNonSpace(wstring text) {
    int total = 0;
    for (int i=0; i<text.length(); i++) {
        if (text[i] != ' ')
            total++;
    }
    return total;
}

int countDigits (const wstring& text, bool lax) {
	int cnt = 0;
	for (int i=0; i<text.size(); i++) {
        wchar_t ch = text[i];
		if (isDigit(ch)) {
			cnt++;
		} else if (lax && ((ch == 'O') || (ch == '|') || (ch == 'l') || (ch == 'I'))) {
            // TODO accept also close lookalikes in other languages
            cnt++;
        }
	}
	return cnt;
}

int countLettersOrDigits (const wstring& text) {
	int cnt = 0;
	for (int i=0; i<text.size(); i++) {
        wchar_t ch = text[i];
		if (isLetterOrDigit(ch)) {
			cnt++;
		}
	}
	return cnt;
}

int OCRUtilsCountUpperBefore(SmartPtr<OCRRect> r, int &totalCountLetters) {
    totalCountLetters = 0;
    int totalUpper = 0;
    
    r = r->previous;
    
    while ((r != NULL) && (isLetterOrDigit(r->ch) || isDash(r->ch) || (r->ch == '.'))) {
        if (isUpper(r->ch)) {
            totalUpper++;
        }
        if (isLetter(r->ch))
            totalCountLetters++;
        r = r->previous;
    }
    
    return totalUpper;
}

int OCRUtilsCountUpperAfter(SmartPtr<OCRRect> r, int &totalCountLetters) {
    totalCountLetters = 0;
    int totalUpper = 0;
    
    r = r->next;
    
    while ((r != NULL) && (isLetterOrDigit(r->ch) || isDash(r->ch) || (r->ch == '.'))) {
        if (isUpper(r->ch)) {
            totalUpper++;
        }
        if (isLetter(r->ch))
            totalCountLetters++;
        r = r->next;
    }
    
    return totalUpper;
}
    
int OCRUtilsCountUpper(SmartPtr<OCRRect> r, int &totalCountLetters) {
    totalCountLetters = 0;
    int totalUpper = 0;
    
    // Count before
    int countBefore = 0, upperBefore = 0;
    upperBefore = OCRUtilsCountUpperBefore(r, countBefore);
    totalUpper += upperBefore; totalCountLetters += countBefore;
    
    // Count after
    int countAfter = 0, upperAfter = 0;
    upperAfter = OCRUtilsCountUpperBefore(r, countAfter);
    totalUpper += upperAfter; totalCountLetters += countAfter;
    
    // Current char
    if (isUpper(r->ch)) {
        totalUpper++;
    }
    if (isLetter(r->ch))
        totalCountLetters++;
    
    return totalUpper;
}

// Replaces two consecutive OCRRect (given rect + the one following) with a specified new char
void replaceTwo (SmartPtr<OCRRect> r, wchar_t newCh, const char *msg, float minBottom, float newHeight) {
    float newWidth = rectRight(r->next->rect) - rectLeft(r->rect) + 1;
    if (minBottom == -1.0) {
        minBottom = MIN(rectBottom(r->rect), rectBottom(r->next->rect));
    }
    if (newHeight == -1.0) {
        float maxTop = MAX(rectTop(r->rect), rectTop(r->next->rect));
        newHeight = maxTop - minBottom + 1;
    }
    if (newWidth <= 0)
        newWidth = r->rect.size.width;
    CGRect newRect(rectLeft(r->rect),
                   minBottom,
                   newWidth,
                   newHeight);
    ReplacingLog("Validate: %s replacing [%c%c] with [%c] in word [%s]", msg, (unsigned short)r->ch, (unsigned short)r->next->ch, (unsigned short)newCh, toUTF8(r->word->text()).c_str());
    r->word->removeLetter(r->next);
    r->word->updateLetterWithNewCharAndNewRect(r, newCh, newRect);
}

void replaceTwoWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg) {
    if (newRect.size.width <= 0)
        newRect.size.width = r->rect.size.width;
    ReplacingLog("Validate: %s replacing [%c%c] with [%c] in word [%s]", ((msg != NULL)? msg:""), (unsigned short)r->ch, (unsigned short)r->next->ch, (unsigned short)newCh, toUTF8(r->word->text()).c_str());
    r->word->removeLetter(r->next);
    r->word->updateLetterWithNewCharAndNewRect(r, newCh, newRect);
}

void replaceThreeWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg) {
    if ((r->next == NULL) || (r->next->next == NULL))
        return;
    if (newRect.size.width <= 0)
        newRect.size.width = r->rect.size.width;
    ReplacingLog("Validate: %s replacing [%c%c%c] with [%c] in word [%s]", ((msg != NULL)? msg:""), (unsigned short)r->ch, (unsigned short)r->next->ch, (unsigned short)r->next->next->ch, (unsigned short)newCh, toUTF8(r->word->text()).c_str());
    r->word->removeLetter(r->next->next);
    r->word->removeLetter(r->next);
    r->word->updateLetterWithNewCharAndNewRect(r, newCh, newRect);
}

void replaceFourWithRect (SmartPtr<OCRRect> r, wchar_t newCh, CGRect newRect, const char *msg) {
    if ((r->next == NULL) || (r->next->next == NULL) || (r->next->next->next == NULL))
        return;
    if (newRect.size.width <= 0)
        newRect.size.width = r->rect.size.width;
    ReplacingLog("Validate: %s replacing [%c%c%c%c] with [%c] in word [%s]", ((msg != NULL)? msg:""), (unsigned short)r->ch, (unsigned short)r->next->ch, (unsigned short)r->next->next->ch, (unsigned short)r->next->next->next->ch, (unsigned short)newCh, toUTF8(r->word->text()).c_str());
    r->word->removeLetter(r->next->next->next);
    r->word->removeLetter(r->next->next);
    r->word->removeLetter(r->next);
    r->word->updateLetterWithNewCharAndNewRect(r, newCh, newRect);
}

unsigned short glued(OCRRectPtr r, unsigned short req)
{
    unsigned short res = 0;
    
    if ((req & GLUED_LEFT) && (r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0)) {
        res |= GLUED_LEFT;
    }
    if ((req & GLUED_RIGHT) && (r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0)) {
        res |= GLUED_RIGHT;
    }
    return res;
} // glued


CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2) {
    float minY = MIN(rectBottom(rect1), rectBottom(rect2));
    float maxY = MAX(rectTop(rect1), rectTop(rect2));
    CGRect resRect(rectLeft(rect1), minY, rectRight(rect2) - rectLeft(rect1) + 1, maxY - minY + 1);
    return resRect;
}

CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2, CGRect &rect3) {
    float minY = MIN(rectBottom(rect1), rectBottom(rect2));
    minY = MIN(minY, rectBottom(rect3));
    float maxY = MAX(rectTop(rect1), rectTop(rect2));
    maxY = MAX(maxY, rectTop(rect3));
    CGRect resRect(rectLeft(rect1), minY, rectRight(rect3) - rectLeft(rect1) + 1, maxY - minY + 1);
    return resRect;
}

CGRect CreateCombinedRect(CGRect &rect1, CGRect &rect2, CGRect &rect3, CGRect &rect4) {
    float minY = MIN(rectBottom(rect1), rectBottom(rect2));
    minY = MIN(minY, rectBottom(rect3));
    minY = MIN(minY, rectBottom(rect4));
    float maxY = MAX(rectTop(rect1), rectTop(rect2));
    maxY = MAX(maxY, rectTop(rect3));
    maxY = MAX(maxY, rectTop(rect4));
    CGRect resRect(rectLeft(rect1), minY, rectRight(rect4) - rectLeft(rect1) + 1, maxY - minY + 1);
    return resRect;
}

// Checks that we have a single connected components. NOT a good test of course for letters with multiple parts!   
bool validateConnectedComponents (ConnectedComponentList &cpl, CGRect &inputRect)
{
    if ((cpl.size() < 2) || (cpl[1].getWidth() * cpl[1].getHeight() < inputRect.size.width * inputRect.size.height * 0.75)) {
#if DEBUG
        DebugLog("ValidateConnectedCompoments: rejecting connected components");
        if (cpl.size() < 2) {
            DebugLog("Reason: cpl.size()=%d", (unsigned int)cpl.size());
        } else {
            DebugLog("Reason: cpl[1] too small [w=%d, h=%d], inputRect [w=%d, h=%d]", (unsigned int)cpl[1].getWidth(), (unsigned int)cpl[1].getHeight(), (unsigned int)inputRect.size.width, (unsigned int)inputRect.size.height);
        }
#endif    
        return false;
    }
    return true;
}

bool OCRUtilsPossibleDollar(CGRect rect, MatchPtr m, OCRResults *results) {
    if (!results->imageTests) return false;
    SingleLetterTests *st = CreateSingleLetterTests(rect, results);
    if (st == NULL) return false;

    // Now also look for the first non 1 digit in m to get a pattern to compare to
        // Don't cache, because caching would force us to recalculate each time rects change - and in any case we only use this info once per match
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	size_t nRects = *m[matchKeyN].castDown<size_t>();
	if (nRects == 0)
		return false;
    wstring itemText;
    if (m.isExists(matchKeyOrigText)) {
        itemText = getStringFromMatch(m, matchKeyOrigText);
    } else {
        itemText = getStringFromMatch(m, matchKeyText);
    }
    if (itemText.length() != (int)nRects) {
        ErrorLog("OCRUtilsPossibleDollar: mismatch item length [%s, length=%d] versus nRects=%d", toUTF8(itemText).c_str(), (int)itemText.length(), (int)nRects);
        return false;
    }
	for (int i=0; i<(int)nRects; i++) {
		CGRect r = *(*rects)[i];
        wchar_t ch = itemText[i];
        if (isDigit(ch) && (ch != '1')) {
            SingleLetterTests *stDigit = CreateSingleLetterTests(r, results);
            if (stDigit != NULL) {
                // Get main components of each pattern
                ConnectedComponentList cpl = st->getConnectedComponents();
                ConnectedComponentList cplDigit = stDigit->getConnectedComponents();
                if ((cpl.size() > 1) && (cplDigit.size() > 0)) {
                    ConnectedComponent mainCpl = cpl[1];
                    ConnectedComponent mainCplDigit = cplDigit[1];
                    if (mainCpl.area > mainCplDigit.area * 0.85) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

int SingleLetterNumComponents(CGRect rect, OCRResults *results, float tolerance, SingleLetterTests *inputSt, ConnectedComponent *cc1, ConnectedComponent *cc2) {
    if (!results->imageTests) return -1;

    SingleLetterTests *st = ((inputSt != NULL)? inputSt:CreateSingleLetterTests(rect, results));
    
    if (st == NULL)
        return -1;
            
    ConnectedComponentList cpl = st->getConnectedComponents();
        
    if (cpl.size() < 2)
        return 0;
        
    int numComponents = 1;
    if (cc1 != NULL) {
        *cc1 = cpl[1];
    }
    for (int i = 2; i < cpl.size(); i++) {
        if (cpl[i].area > cpl[1].area * tolerance) {
            numComponents++;
            if ((i == 2) && (cc2 != NULL)) {
                *cc2 = cpl[i];
            }
        }
        else 
            break;
    }

    return numComponents;
} // SingleLetterNumComponents

bool validateConnectedComponents (SingleLetterTests *st, CGRect &inputRect)
{
    if (st == NULL)
        return false;
    ConnectedComponentList cpl = st->getConnectedComponents();
    return (validateConnectedComponents(cpl, inputRect));
}

bool testForInvertedText(CGRect letterRect, uint8_t *binaryImageData, size_t binaryImageWidth, size_t binaryImageHeight) {
    if (binaryImageData == NULL)
        return true;
    unsigned short numBlackAboveBelow = 0;
    unsigned short maxTolerated = round(letterRect.size.width * 2 * 0.60);
    
    // Count pixels above
    if (letterRect.origin.y > 0) {
        size_t yAbove = letterRect.origin.y - 1;
        for (int xx=letterRect.origin.x; xx < letterRect.origin.x + letterRect.size.width; xx++) {
            uint8_t value = binaryImageData[yAbove * binaryImageWidth + xx];
            // Allow for some greyish pixels (JPG fuzziness)
            if (value > 120) {
                numBlackAboveBelow++;
                if (numBlackAboveBelow > maxTolerated)
                    break;
            }
        }
    }
    
    // Count pixels below
    if ((numBlackAboveBelow < maxTolerated) && (letterRect.origin.y + letterRect.size.height < binaryImageHeight)) {
        size_t yBelow = letterRect.origin.y + letterRect.size.height;
        for (int xx=letterRect.origin.x; xx < letterRect.origin.x + letterRect.size.width; xx++) {
            uint8_t value = binaryImageData[yBelow * binaryImageWidth + xx];
            // Allow for some greyish pixels (JPG fuzziness)
            if (value > 120) {
                numBlackAboveBelow++;
                if (numBlackAboveBelow > maxTolerated)
                    break;
            }
        }
    }
    
    if (numBlackAboveBelow > maxTolerated) {
        DebugLog("testForInvertedText: not testing, too many black pixels above/below, at least %d (max tolerated = %d)", numBlackAboveBelow, maxTolerated);
        return true;
    } else {
        return false;
    }
} // testForInvertedText

char OCRUtilsSuggestLetterReplacement(CGRect currentRect, wchar_t currentChar, double maxHeight, OCRResults *results, bool force_replace, float* i_body_height) {
	
    if (results->binaryImageData == NULL) return NULL;
	uint8_t *binaryImageData = results->binaryImageData;
	size_t binaryImageWidth= results->binaryImageWidth;
	size_t binaryImageHeight = results->binaryImageHeight;
	
	CGRect letterRect = currentRect;
	// Adjust for the case where we are re-scanning a rect within a larger image
	//letterRect.origin.x -= results->rectAfterRotate.origin.x;
	//letterRect.origin.y -= results->rectAfterRotate.origin.y;
	
	// Rect out of bounds?
	if ((letterRect.origin.x < 0) || (letterRect.origin.y < 0) 
        || (letterRect.size.width <= 0) || (letterRect.size.height <= 0)
		|| ((letterRect.size.width  + letterRect.origin.x) > binaryImageWidth)
		|| ((letterRect.size.height + letterRect.origin.y) > binaryImageHeight)) {
#if DEBUG        
		cout << "OUT OF BOUNDS! " << letterRect.origin.x << "," << letterRect.origin.y << "," << letterRect.size.width << "," << letterRect.size.height << endl;
#endif        
		return currentChar;
	}
    
    // New sanity check to detect cases of inverted text. In these cases, the letter is white and surrounded with black
    if (testForInvertedText(letterRect, binaryImageData, binaryImageWidth, binaryImageHeight)) {
        return '\0';
    }
	
	if (is_in_il_group(currentChar)) {
		// Adjust height to current height + 50% or tallest letter on current line (whichever is less)
		double origHeight = letterRect.size.height;
		if (maxHeight > letterRect.size.height * 1.50) {
			letterRect.size.height *= 1.50;
		} else if (maxHeight > letterRect.size.height) {
			letterRect.size.height = maxHeight;
		}
		letterRect.origin.y -= (letterRect.size.height - origHeight);
        // Attention! This could place us outside of the range (if letter origin was close to 0)
        if (letterRect.origin.y < 0)
            letterRect.origin.y = 0;
	}

#if DEBUG_SINGLELETTER	
	cout << dec << "grayScale: " << binaryImageWidth << "x" << binaryImageHeight << endl;
#endif    
	cimage letterImage(binaryImageData, (int)binaryImageHeight, (int)binaryImageWidth, letterRect, true /*4-neighbors*/);
	SL_RESULTS res = get_single_letter_scores(currentChar, letterImage);
    //TODO: use res.output_params[OUTPUT_PARAM_BODY_HEIGHT] and res.output_params[OUTPUT_PARAM_I_BODY_HEIGHT]
	if (i_body_height != NULL) {
		(*i_body_height) = res.output_params[OUTPUT_PARAM_I_BODY_HEIGHT];
	}

#if DEBUG_SINGLELETTER
	int rectWidth = currentRect.size.width;
	int rectHeight = currentRect.size.height;
	
	const int FACTOR = 1;
	uint8_t* rawLetterImage = new uint8_t[(FACTOR * rectWidth) * (FACTOR * rectHeight)];
	memset(rawLetterImage, sizeof(uint8_t), (FACTOR * rectWidth) * (FACTOR * rectHeight));
	for (int y = 0; y < FACTOR*rectHeight; ++y) {
		for (int x = 0; x < FACTOR*rectWidth; ++x) {
			if (((y - rectHeight*(FACTOR-1)/2 + (int)letterRect.origin.y) < 0) || ((y - rectHeight*(FACTOR-1)/2 + (int)letterRect.origin.y) >= binaryImageHeight)
				|| ((x - rectWidth*(FACTOR-1)/2 + (int)letterRect.origin.x) < 0) || ((x - rectWidth*(FACTOR-1)/2 + (int)letterRect.origin.x) >= binaryImageWidth)) {
				// memset: letterImage[((FACTOR*rectHeight - y - 1) * FACTOR*rectWidth) + x] = 0;
				continue;
			}

			// WITH INV: letterImage[((FACTOR*rectHeight - y - 1) * FACTOR*rectWidth) + x] = grayScaleImageData[((grayScaleHeight - (y - rectHeight*(FACTOR-1)/2 + (int)currentR->rect.origin.y) - 1) * grayScaleWidth) + (x - rectWidth*(FACTOR-1)/2 + (int)currentR->rect.origin.x)];
			rawLetterImage[((FACTOR*rectHeight - y - 1) * FACTOR*rectWidth) + x] = binaryImageData[((y - rectHeight*(FACTOR-1)/2 + (int)currentRect.origin.y) * binaryImageWidth) + (x - rectWidth*(FACTOR-1)/2 + (int)currentRect.origin.x)];
			//				letter[y * rectWidth + x] = 255 - grayScaleImageData[(y + (int)currentR->rect.origin.y) * grayScaleWidth + (x + (int)currentR->rect.origin.x)];
			//				horizontalSum[y] += 255 - grayScaleImageData[(y + (int)currentR->rect.origin.y) * grayScaleWidth + (x + (int)currentR->rect.origin.x)];
		}
	}
	
	static uint32_t counter = 0;
	static time_t 	time_series = time(NULL);
	char letterFileName[1024];
	//int factorWidth = FACTOR*rectWidth;
	//int factorHeight = FACTOR*rectHeight;

	char chName[10];
	if (currentChar == '|') {
		strcpy(chName, "pipe");
	} else {
		sprintf(chName, "%c", currentChar);
	}
	
	sprintf(letterFileName, "/Users/michael/patrick/samples/%s.%d.%d.txt", chName, time_series, counter);
	FILE *letterOut = fopen(letterFileName, "w");
	//fprintf(letterOut, "%d\n", FACTOR*rectWidth);
	for (int y = 0; y < FACTOR*rectHeight; ++y) {
		for (int x = 0; x < FACTOR*rectWidth; ++x) {
			fprintf(letterOut, "%u,", rawLetterImage[(FACTOR*rectWidth * y) + x]);
		}
		fprintf(letterOut, "\n");
	}
	fclose(letterOut);

	/// Binary
	//sprintf(letterFileName, "%c.%d.bin", currentR->ch, counter);
	//letterOut = fopen(letterFileName, "wb");
	//fwrite(&factorWidth, sizeof(int), 1, letterOut);
	//fwrite(letterImage, sizeof(uint8_t), factorWidth*factorHeight, letterOut);
	//fclose(letterOut);
	 

	++counter;
	delete[] rawLetterImage;
	return currentChar;
#endif // DEBUG_SINGLE_LETTER

    return suggest_sl_replacement(currentChar, res.group_scores, force_replace);	
} // OCRUtilsSuggestLetterReplacement


#if DEBUG
void SingleLetterPrint(ConnectedComponentList &cpl, CGRect &rect) {
    if (cpl.size() < 2) {
        DebugLog("validate: connected components has fewer than 2 items (%d)", (unsigned int)cpl.size());
    } 
    for (int i=0; i<cpl.size(); i++) {
        DebugLog("Comp[%d]: [%d,%d - %d,%d], area=%d (%.1f%%)", i, (unsigned int)cpl[i].xmin, (unsigned int)cpl[i].ymin, (unsigned int)cpl[i].xmax, (unsigned int)cpl[i].ymax, (unsigned int)cpl[i].area, (float)cpl[i].area/(rect.size.width * rect.size.height)*100.00);
    }
}
void SingleLetterPrint(ConnectedComponent &cc, CGRect &rect) {
    DebugLog("Comp: [%d,%d - %d,%d], area=%d (%.1f%%)", (unsigned int)cc.xmin, (unsigned int)cc.ymin, (unsigned int)cc.xmax, (unsigned int)cc.ymax, (unsigned int)cc.area, (float)cc.area/(rect.size.width * rect.size.height)*100.00);
}
void SingleLetterPrint(SegmentList &sl, float ref) {
    if (sl.size() < 1) {
        DebugLog("validate: empty segments list");
    } else {
        for (int i=0; i<sl.size(); i++) {
            DebugLog("Segment[%d]: [%d,%d] (%.1f%% - %.1f%%)", i, (unsigned short)sl[i].startPos, (unsigned short)sl[i].endPos, 
                sl[i].startPos*100.0/ref, sl[i].endPos*100.0/ref);
        }
    }
}
#endif // DEBUG

/* Defaults:
    - neighbors4 = false
    - validation = 0
    - float minSize = 0.03
    - acceptInverted = false
*/
SingleLetterTests* CreateSingleLetterTests(CGRect inputRect, OCRResults *results, bool neighbors4, int validation, float minSize, bool acceptInverted) {
    if ((results->binaryImageData == NULL) || !results->imageTests) return NULL;
	uint8_t *grayScaleImageData = results->binaryImageData;
	size_t grayScaleWidth= results->binaryImageWidth;
	size_t grayScaleHeight = results->binaryImageHeight;
	CGRect letterRect = inputRect;
	// PQTODO Adjust for the case where the binary image is smaller / larger than the image the OCR got
	//letterRect.origin.x -= results->rectAfterRotate.origin.x;
	//letterRect.origin.y -= results->rectAfterRotate.origin.y;
	
	// Rect out of bounds?
	if ((letterRect.origin.x < 0) || (letterRect.origin.y < 0) 
        || (letterRect.size.width <= 0) || (letterRect.size.height <= 0)
		|| ((letterRect.size.width  + letterRect.origin.x) > grayScaleWidth)
		|| ((letterRect.size.height + letterRect.origin.y) > grayScaleHeight)) {
#if DEBUG        
		cout << "ERROR! OUT OF BOUNDS! " << letterRect.origin.x << "," << letterRect.origin.y << "," << letterRect.size.width << "," << letterRect.size.height << endl;
#endif        
		return NULL;
	}
    
    // New sanity check to detect cases of inverted text. In these cases, the letter is white and surrounded with black
    if (!acceptInverted && testForInvertedText(letterRect, grayScaleImageData, grayScaleWidth, grayScaleHeight)) {
        return NULL;
    }
    
	cimage* letterImage = new cimage(grayScaleImageData, (int)grayScaleHeight, (int)grayScaleWidth, letterRect, neighbors4);
#if REPLACING_LOG
    letterImage->debug_print();
#endif    
    SingleLetterTests *stRes = new SingleLetterTests(letterImage);
    if ((stRes != NULL) && (validation & SINGLE_LETTER_VALIDATE_COMP_SIZE)) {
        ConnectedComponentList cpl = stRes->getConnectedComponents();
        // Required a main component + at least 75% of total rect area
        if ((cpl.size() < 2) || (cpl[1].getWidth() * cpl[1].getHeight() < letterRect.size.width * letterRect.size.height * 0.75)) {
            delete stRes;
            return NULL;
        }
    }
    if ((stRes != NULL) && (validation & SINGLE_LETTER_VALIDATE_SINGLE_COMP)) {
        ConnectedComponentList cpl = stRes->getConnectedComponents(minSize);
        // Requires a single main component or none larger than 5% of main one
        if ((cpl.size() < 2) || ((cpl.size() >= 3) && (cpl[2].area > cpl[1].area * OCR_RATIO_SIGNIFICANT_AREA))) {
            delete stRes;
            return NULL;
        }
    }
    return stRes;
} // CreateSingleLetterTests


bool OCRUtilsTopTest(CGRect inputRect, OCRResults *results, bool *res) {
    if (results->binaryImageData == NULL) return false;
    uint8_t *grayScaleImageData = results->binaryImageData;
    size_t grayScaleWidth= results->binaryImageWidth;
    size_t grayScaleHeight = results->binaryImageHeight;
    CGRect letterRect = inputRect;
    // PQTODO Adjust for the case where the binary image is smaller / larger than the image the OCR got
    //letterRect.origin.x -= results->rectAfterRotate.origin.x;
    //letterRect.origin.y -= results->rectAfterRotate.origin.y;
    
    // Rect out of bounds?
    if ((letterRect.origin.x < 0) || (letterRect.origin.y < 0) 
        || (letterRect.size.width <= 0) || (letterRect.size.height <= 0)
        || ((letterRect.size.width  + letterRect.origin.x) > grayScaleWidth)
        || ((letterRect.size.height + letterRect.origin.y) > grayScaleHeight)) {
#if DEBUG        
        cout << "ERROR! OUT OF BOUNDS! " << letterRect.origin.x << "," << letterRect.origin.y << "," << letterRect.size.width << "," << letterRect.size.height << endl;
#endif        
        return false;
    }
    
    // New sanity check to detect cases of inverted text. In these cases, the letter is white and surrounded with black
    if (testForInvertedText(letterRect, grayScaleImageData, grayScaleWidth, grayScaleHeight)) {
        return false;
    }
    
    cimage letterImage(grayScaleImageData, (int)grayScaleHeight, (int)grayScaleWidth, letterRect, true /*4-neighbors*/);
#if DEBUG_LOG
    letterImage.debug_print();
#endif    
            
    *res =  letterImage.top_horiz_bar_test();
                
    return true;
} // OCRUtilsTopTest

bool OCRUtilsOpeningsTest(CGRect inputRect, OCRResults *results,
                                      int side, //GET_OPENINGS_TOP/BOTTOM/LEFT/RIGHT
                                      bool neigbors4,
                                      float start_pos, float end_pos,
                                      bool bounds_required,
                                      float letter_thickness,
									  float* outWidth, float* outHeight, 
                                      float* outMaxDepth, float* outAverageWidth, float* outOpeningWidth,
                                      float* outMaxWidth, float* outArea) {
    if (results->binaryImageData == NULL) return false;
	uint8_t *grayScaleImageData = results->binaryImageData;
	size_t grayScaleWidth= results->binaryImageWidth;
	size_t grayScaleHeight = results->binaryImageHeight;
	CGRect letterRect = inputRect;
	// PQTODO Adjust for the case where the binary image is smaller / larger than the image the OCR got
	//letterRect.origin.x -= results->rectAfterRotate.origin.x;
	//letterRect.origin.y -= results->rectAfterRotate.origin.y;
	
	// Rect out of bounds?
	if ((letterRect.origin.x < 0) || (letterRect.origin.y < 0) 
        || (letterRect.size.width <= 0) || (letterRect.size.height <= 0)
		|| ((letterRect.size.width  + letterRect.origin.x) > grayScaleWidth)
		|| ((letterRect.size.height + letterRect.origin.y) > grayScaleHeight)) {
#if DEBUG        
		cout << "ERROR! OUT OF BOUNDS! " << letterRect.origin.x << "," << letterRect.origin.y << "," << letterRect.size.width << "," << letterRect.size.height << endl;
#endif
		return false;
	}
    
    // New sanity check to detect cases of inverted text. In these cases, the letter is white and surrounded with black
    if (testForInvertedText(letterRect, grayScaleImageData, grayScaleWidth, grayScaleHeight)) {
        return false;
    }
    
	cimage letterImage(grayScaleImageData, (int)grayScaleHeight, (int)grayScaleWidth, letterRect, neigbors4);
#if DEBUG_LOG
    letterImage.debug_print();
#endif 
    bool success = get_openings(letterImage, side, start_pos, end_pos, bounds_required, letter_thickness, 
            outWidth, outHeight, outMaxDepth, outAverageWidth, outOpeningWidth, outMaxWidth, outArea);
    return success;
} // OCRUtilsOpeningsTest


wchar_t CircumflexToNormal(wchar_t ch)
{
    bool isUppercase = isUpper(ch);
    wchar_t testedCh = ch;
    if (isUppercase) {
        testedCh = toLower(ch);
    }
    wchar_t repCh = '\0';
    switch (ch) {
        case 0x00ee: // 'î':
            repCh = 'i';
            break;
        case 0x00f4: // 'ô':
            repCh = 'o';
            break;
        case 0x011d: // 'ĝ':
            repCh = 'g';
            break;
        case 0x00ea: // 'ê':
            repCh = 'e';
            break;
        case 0x0109: // 'ĉ':
            repCh = 'c';
            break;
        case 0x00e2: // 'â':
            repCh = 'a';
            break;
        case 0x0135: // 'ĵ':
            repCh = 'j';
            break;
        case 0x00f1: // U+00F1	ñ	c3 b1	LATIN SMALL LETTER N WITH TILDE
            repCh = 'n';
            break;
        case 0x015d: // 'ŝ':
            repCh = 's';
            break;
        case 0x00fb: // 'û':
            repCh = 'u';
            break;
        case 0x0177: // 'ŷ':
            repCh = 'y';
            break;
        case 0x0175: // 'ŵ':
            repCh = 'w';
            break;
         default:
            repCh = '\0';
            break;
    }
    if (repCh == '\0')
        return repCh;
    else 
        return (isUppercase? toUpper(repCh):repCh);
} // CircumflexToNormal

bool SingleLetterTestCircumflex(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_CIRCUMFLEX)
        // 0x00ee: // 'î':
        || (CircumflexToNormal(r->ch) == '\0')) {
        return false;
    }
    r->flags6 |= FLAGS6_TESTED_CIRCUMFLEX; 
    SingleLetterTests *st = NULL;
    
    bool gluedToNext = (((r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0))
                        || ((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0)));
    if (gluedToNext) {
        return false;
    }
    
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return false;
    }
    // Expect 2 components (letter + accent)
    ConnectedComponentList cpl = st->getConnectedComponents();
    if (cpl.size() != 3) {
        if (needToRelease)
            delete st;
        return false;
    }
    
    ConnectedComponent letterCC = cpl[1];
    ConnectedComponent accentCC = cpl[2];
    
    if ((letterCC.ymin - accentCC.ymin < 2) || !OCRUtilsOverlapping(accentCC.xmin, accentCC.xmax, letterCC.xmin, letterCC.xmax))
    {
        if (needToRelease)
            delete st;
        newChar = CircumflexToNormal(r->ch);
        return true;
    }

    CGRect accentRect (rectLeft(rect) + accentCC.xmin, rectBottom(rect) + accentCC.ymin, accentCC.getWidth(), accentCC.getHeight());
    SingleLetterTests *stAccent = CreateSingleLetterTests(accentRect, results);

    // Now test bottom opening
    OpeningsTestResults resBottom;
    bool successBottom = stAccent->getOpenings(resBottom, SingleLetterTests::Bottom, 
        0.00,      // Start of range to search (top/left)
        1.00,    // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );

    delete stAccent;
    
    if (successBottom)
    {
        if (needToRelease)
            delete st;
        newChar = r->ch;    // Confirmed
        return true;
    } else {
        if (needToRelease)
            delete st;
        newChar = CircumflexToNormal(r->ch);
        return true;
    }
} // testCircumflex

bool compareRects (CGRect r1, CGRect r2) {
    if ((r1.origin.x == r2.origin.x)
        && (r1.origin.y == r2.origin.y)
        && (r1.size.width == r2.size.width)
        && (r1.size.height == r2.size.height))
        return true;
    else
        return false;
}

// - targetX/Y: expressed in percentages of the width / height, e.g. 0.50, 0.50 means dead center
CGRect getClosestRect(CGRect baseRect, float targetX, float targetY, ConnectedComponentList &cpl, float xlimit, float ylimit) {
    // Pick the pattern whose sum of % distances away from target is smallest - and smaller than xlimit / ylimit along respective axis
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float centerX = (cc.xmin + cc.xmax) / 2;
        float centerY = (cc.ymin + cc.ymax) / 2;
        float percentFromX = abs(centerX / baseRect.size.width - targetX);
        float percentFromY = abs(centerY / baseRect.size.height - targetY);
        if ((percentFromX < xlimit) && (percentFromY < ylimit) && (percentFromX + percentFromY < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromX + percentFromY;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

// Pick the pattern not further away from right side as the specified limit + pick the tallest of these who match that test
CGRect getTallestRightRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float bestHeight = 0;
    float bestPercentFromRight = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromRight = (baseRect.size.width - cc.xmax - 1) / baseRect.size.width;
        if (percentFromRight <= xlimit) {
            if ((cc.getHeight() > bestHeight) || ((cc.getHeight() == bestHeight) && (percentFromRight < bestPercentFromRight))) {
                bestIndex = i;
                bestHeight = cc.getHeight();
                bestPercentFromRight = percentFromRight;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getTallestRightRect

// Pick the pattern not further away from left side as the specified limit + pick the tallest of these who match that test
CGRect getTallestLeftRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float bestHeight = 0;
    float bestPercentAway = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentAway = cc.xmin / baseRect.size.width;
        if (percentAway <= xlimit) {
            if ((cc.getHeight() > bestHeight) || ((cc.getHeight() == bestHeight) && (percentAway < bestPercentAway))) {
                bestIndex = i;
                bestHeight = cc.getHeight();
                bestPercentAway = percentAway;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getTallestLeftRect

// Pick the pattern not further away from left side as the specified limit + pick the highest of these who match that test
CGRect getHighestLeftRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float highestY = baseRect.size.height + 1;
    float bestPercentAway = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentAway = cc.xmin / baseRect.size.width;
        if (percentAway <= xlimit) {
            if (((cc.ymax < highestY) || (bestIndex < 0))
                || ((cc.ymax == highestY) && (percentAway < bestPercentAway))) {
                bestIndex = i;
                highestY = cc.ymax;
                bestPercentAway = percentAway;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getHighestLeftRect

// Pick the pattern not further away from left side as the specified limit + pick the lowest of these who match that test
CGRect getLowestLeftRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float lowestY = 0;
    float bestPercentAway = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentAway = cc.xmin / baseRect.size.width;
        if (percentAway <= xlimit) {
            if (((cc.ymax > lowestY) || (bestIndex < 0))
                || ((cc.ymax == lowestY) && (percentAway < bestPercentAway))) {
                bestIndex = i;
                lowestY = cc.ymax;
                bestPercentAway = percentAway;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getLowestLeftRect

// Pick the pattern not further away from right side as the specified limit + pick the highest of these who match that test
CGRect getHighestRightRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float highestY = baseRect.size.height + 1;
    float bestPercentAway = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentAway = (baseRect.size.width -1 - cc.xmax) / baseRect.size.width;
        if (percentAway <= xlimit) {
            if (((cc.ymax < highestY) || (bestIndex < 0))
                || ((cc.ymax == highestY) && (percentAway < bestPercentAway))) {
                bestIndex = i;
                highestY = cc.ymax;
                bestPercentAway = percentAway;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getHighestRightRect

// Pick the pattern not further away from right side as the specified limit + pick the lowest of these who match that test
CGRect getLowestRightRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float lowestY = 0;
    float bestPercentAway = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentAway = (baseRect.size.width -1 - cc.xmax) / baseRect.size.width;
        if (percentAway <= xlimit) {
            if (((cc.ymax > lowestY) || (bestIndex < 0))
                || ((cc.ymax == lowestY) && (percentAway < bestPercentAway))) {
                bestIndex = i;
                lowestY = cc.ymax;
                bestPercentAway = percentAway;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getLowestRightRect

// Pick the pattern not further away from bottom specified limit + pick the rightmost of these who match that test
CGRect getRightmostBottomRect(CGRect baseRect, ConnectedComponentList &cpl, float ylimit) {
    int bestIndex = -1;
    float bestRightX = 0;
    float bestPercentAwayFromBottom = 1;
    float bestArea = 0;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float currentPercentAwayFromBottom = (baseRect.size.height - 1 - cc.ymax) / baseRect.size.height;
        float currentArea = cc.area;
        float currentRightX = cc.xmax;
        if (currentPercentAwayFromBottom <= ylimit) {
            if ((bestIndex < 0) // No candidate yet
                // More to the right than existing candidate
                || (currentRightX > bestRightX)
                // As much to the right as existing but closer to bottom
                || ((currentRightX == bestRightX) && (currentPercentAwayFromBottom > bestPercentAwayFromBottom))
                // As much to the right as existing, same distance to bottom but larger
                || ((currentRightX == bestRightX) && (currentPercentAwayFromBottom == bestPercentAwayFromBottom) && (currentArea > bestArea))) {
                bestIndex = i;
                bestRightX = currentRightX;
                bestPercentAwayFromBottom = currentPercentAwayFromBottom;
                bestArea = currentArea;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getRightmostBottomRect

// Pick the pattern not further away from right side as the specified limit + pick the tallest of these who match that test
CGRect getLargestTopRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit) {
    int bestIndex = -1;
    float bestArea = 0;
    float bestPercentFromTop = 1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromTop = cc.ymin / baseRect.size.height;
        if (percentFromTop <= xlimit) {
            if ((cc.area > bestArea) || ((cc.area == bestArea) && (percentFromTop < bestPercentFromTop))) {
                bestIndex = i;
                bestArea = cc.area;
                bestPercentFromTop = percentFromTop;
            }
        }
    }
    if (bestIndex > 0) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
} // getLargestToptRect

CGRect getTopLeftRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit, float ylimit) {
    // Pick the pattern whose sum of % distance away from top + % distance away from left is smallest - and both % smaller than 15%
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromLeft = cc.xmin / baseRect.size.width;
        float percentFromTop = cc.ymin / baseRect.size.height;
        if ((percentFromLeft < xlimit) && (percentFromTop < ylimit) && (percentFromTop + percentFromLeft < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromTop + percentFromLeft;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

CGRect getBottomLeftRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit, float ylimit) {
    // Pick the pattern whose sum of % distance away from bottom + % distance away from left is smallest - and both % smaller than 15%
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromLeft = cc.xmin / baseRect.size.width;
        float percentFromBottom = (baseRect.size.height - cc.ymax - 1) / baseRect.size.height;
        if ((percentFromLeft < xlimit) && (percentFromBottom < ylimit) && (percentFromBottom + percentFromLeft < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromBottom + percentFromLeft;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

CGRect getTopRightRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit, float ylimit) {
    // Pick the pattern whose sum of % distance away from top + % distance away from right is smallest - and both % smaller than 15%
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromRight = (baseRect.size.width - cc.xmax - 1) / baseRect.size.width;
        float percentFromTop = cc.ymin / baseRect.size.height;
        if ((percentFromRight < xlimit) && (percentFromTop < ylimit) && (percentFromTop + percentFromRight < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromTop + percentFromRight;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

CGRect getBottomRightRect(CGRect baseRect, ConnectedComponentList &cpl, float xlimit, float ylimit) {
    // Pick the pattern whose sum of % distance away from bottom + % distance away from right is smallest - and both % smaller than 15%
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromRight = (baseRect.size.width - cc.xmax - 1) / baseRect.size.width;
        float percentFromBottom = (baseRect.size.height - cc.ymax - 1) / baseRect.size.height;
        if ((percentFromRight < xlimit) && (percentFromBottom < ylimit) && (percentFromBottom + percentFromRight < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromBottom + percentFromRight;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

CGRect getBottomRect(CGRect baseRect, ConnectedComponentList &cpl, float ylimit) {
    // Pick the pattern whose sum of % distance away from bottom is smallest
    int bestIndex = -1;
    float bestDistance = 2;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        float percentFromBottom = (baseRect.size.height - cc.ymax - 1) / baseRect.size.height;
        if ((percentFromBottom < ylimit) && (percentFromBottom < bestDistance)) {
            bestIndex = i;
            bestDistance = percentFromBottom;
        }
    }
    if (bestDistance < 2) {
        ConnectedComponent bestCC = cpl[bestIndex];
        return CGRect(rectLeft(baseRect) + bestCC.xmin, rectBottom(baseRect) + bestCC.ymin, bestCC.getWidth(), bestCC.getHeight());
    } else
        return CGRect(0,0,0,0);
}

bool isCurved(CGRect rect, SingleLetterTests *st, SingleLetterTests::OpeningsSide side, SingleLetterTests::BoundsRequired boundStart = SingleLetterTests::Bound, SingleLetterTests::BoundsRequired boundEnd = SingleLetterTests::Bound);

// Side: side where an opening is expected, e.g. "Right" if we test that the pattern points left
bool isCurved(CGRect rect, SingleLetterTests *st, SingleLetterTests::OpeningsSide side, SingleLetterTests::BoundsRequired boundStart, SingleLetterTests::BoundsRequired boundEnd) {
    if (st == NULL) return false;
    bool doit = true;
    // Test opening
    OpeningsTestResults res;
    bool success = st->getOpenings(res,
        side,
        0.00,    // Start of range to search (top/left)
        1.00,    // End of range to search (bottom/right)
        boundStart,   // Require start (top/left) bound
        boundEnd  // Require end (bottom/right) bound
        );
    if (!success) {
        doit = false;
    }
    // Now test that top & bottom are further away from the other side than the middle
    if (doit) {
        SegmentList slTop = st->getHorizontalSegments(0.075, 0.15);
        SegmentList slMiddle = st->getHorizontalSegments(0.50, 0.30);
        SegmentList slBottom = st->getHorizontalSegments(0.925, 0.15);
        if ((slTop.size() >= 1) && (slMiddle.size() >= 1) && (slBottom.size() >= 1)) {
            float distanceTop = ((side == SingleLetterTests::Right)? slTop[0].startPos:rect.size.width - slTop[slTop.size()-1].endPos - 1);
            float distanceMiddle = ((side == SingleLetterTests::Right)? slMiddle[0].startPos:rect.size.width - slMiddle[slMiddle.size()-1].endPos - 1);
            float distanceBottom = ((side == SingleLetterTests::Right)? slBottom[0].startPos:rect.size.width - slBottom[slBottom.size()-1].endPos - 1);
        // if ((distanceBottom < distanceMiddle) || (distanceTop < distanceMiddle))
        //  ------
        //0|      |
        //1|   11 |
        //2|  111 |
        //3|  11  |
        //4| 111  |
        //5|  11  |
        //6|  111 |
        //7|   11 |
        //8|  111 |
        //9|  11  |
        //a|  11  |
        //b| 111  |
        //c|  111 |
        //d|  111 |
        //e|   11 |
        //f|      |
        //  ------
        // Test above allowed this pattern to pass the test - need to require that at least top or bottom be further away than middle
        if (!((distanceBottom > distanceMiddle) || (distanceTop > distanceMiddle)))
                doit = false;
        } else
            doit = false;
    }
    return doit;
}

bool isTopDownDiagonal(CGRect rect, SingleLetterTests *st, bool lax = false);

bool isTopDownDiagonal(CGRect rect, SingleLetterTests *st, bool lax) {
    if (st != NULL) {
        SegmentList slTop = st->getHorizontalSegments(0.10, 0.20);
        SegmentList slBottom = st->getHorizontalSegments(0.90, 0.20);
        if ((slTop.size() >= 1) && (slBottom.size() >= 1)) {
            float topLeftMargin = slTop[0].startPos;
            float topRightMargin = rect.size.width - slTop[slTop.size()-1].endPos - 1;
            float bottomLeftMargin = slBottom[0].startPos;
            float bottomRightMargin = rect.size.width - slBottom[slBottom.size()-1].endPos - 1;
            if (lax) {
                if (((topLeftMargin < bottomLeftMargin) && (topRightMargin == bottomRightMargin))
                    || ((topLeftMargin == bottomLeftMargin) && (topRightMargin > bottomRightMargin)))
                    return true;
                else
                    return false;
            } else {
                if ((topLeftMargin < bottomLeftMargin) && (topRightMargin > bottomRightMargin))
                    return true;
                else
                    return false;
            }
        }
    }
    return false;
}

bool isBottomUpDiagonal(CGRect rect, SingleLetterTests *st, bool lax = false);

bool isBottomUpDiagonal(CGRect rect, SingleLetterTests *st, bool lax) {
    if (st != NULL) {
        SegmentList slTop = st->getHorizontalSegments(0.10, 0.20);
        SegmentList slBottom = st->getHorizontalSegments(0.90, 0.20);
        if ((slTop.size() >= 1) && (slBottom.size() >= 1)) {
            float topLeftMargin = slTop[0].startPos;
            float topRightMargin = rect.size.width - slTop[slTop.size()-1].endPos - 1;
            float bottomLeftMargin = slBottom[0].startPos;
            float bottomRightMargin = rect.size.width - slBottom[slBottom.size()-1].endPos - 1;
            if (lax) {
                if (((topLeftMargin > bottomLeftMargin) && (topRightMargin == bottomRightMargin))
                    || ((topLeftMargin == bottomLeftMargin) && (topRightMargin < bottomRightMargin)))
                    return true;
                else
                    return false;
            } else {
                if ((topLeftMargin > bottomLeftMargin) && (topRightMargin < bottomRightMargin))
                    return true;
                else
                    return false;
            }
        }
    }
    return false;
}


typedef struct _testData {
    ConnectedComponentList cplCombined;
    bool twoRects;
    CGRect leftRect = CGRect (0,0,0,0);
    CGRect rightRect = CGRect (0,0,0,0);
    
    SegmentList slLeft18;               bool gotSlLeft18 = false;
    float combinedLengthSlLeft18 = -1;    bool gotCombinedLengthSlLeft18 = false;
    SegmentList slRight18;              bool gotSlRight18 = false;
    float combinedLengthRight18 = -1;   bool gotCombinedLengthSlRight18 = false;
    SegmentList slRight;                bool gotSlRight = false;
    float combinedLengthSlRight = -1;   bool gotCombinedLengthSlRight = false;
    SegmentList slTop15;                bool gotSlTop15 = false;
    float gapSlTop15 = -1;              bool gotGapSlTop15 = false;
    SegmentList slTop10;                bool gotSlTop10 = false;
    SegmentList slH25w10;               bool gotSlH25w10 = false;
    SegmentList slH38w10;               bool gotSlH38w10 = false;
    float gapSlH38w10 = -1;
    float gapSlH38w10Start = -1;
    float gapSlH38w10End = -1;          bool gotGapSlH38w10 = false;
    SegmentList slTop25;                bool gotSlTop25 = false;
    float gapTop25 = -1;
    float gapTop25Start = -1;
    float gapTop25End = -1;             bool gotGapTop25 = false;
    SegmentList slMiddle50w25;          bool gotSlMiddle50w25 = false;
    float combinedLengthSlMiddle50w25 = -1;    bool gotCombinedLengthSlMiddle50w25 = false;
    SegmentList slH37w20;               bool gotSlH37w20 = false;
    SegmentList slH57w20;               bool gotSlH57w20 = false;
    SegmentList slBottom15;             bool gotSlBottom15 = false;
    float gapSlBottom15 = -1;           bool gotGapSlBottom15 = false;
    SegmentList slBottom25;             bool gotSlBottom25 = false;
    float gapBottom25 = -1;
    float gapBottom25Start = -1;
    float gapBottom25End = -1;          bool gotGapBottom25 = false;
    float widthSlBottom25 = -1;         bool gotWidthSlBottom25 = false;
    float combinedLengthSlBottom25 = -1;  bool gotCombinedLengthSlBottom25 = false;
    SegmentList slH63w10;               bool gotSlH63w10 = false;
    float gapSlH63w10 = -1;
    float gapSlH63w10Start = -1;
    float gapSlH63w10End = -1;          bool gotGapSlH63w10 = false;
    SegmentList slH80w10;               bool gotSlH80w10 = false;
    float gapSlH80w10 = -1;
    float gapSlH80w10Start = -1;
    float gapSlH80w10End = -1;          bool gotGapSlH80w10 = false;
    SegmentList slBottom10;             bool gotSlBottom10 = false;
    SegmentList slW85w30;               bool gotSlW85w30 = false;
    
    SingleLetterTests *stCombined = NULL;
    SingleLetterTests *stLeft = NULL;
    SingleLetterTests *stRight = NULL;
} testData;

#define METRICS_GAP_LETTER_OPENATBOTTOM     0.30    // % of width of bottom opening of N,R,A etc
#define METRICS_GAP_LETTER_CLOSED           0.30    // % of width, e.g. how close do we need the bottom of a disconnected 'D' to be
#define METRICS_GAP_LETTER_CLOSED_LAX       0.34    // Same as above when we are pretty sure of outcome of analysis, e.g. "ID" instead of 'D'
#define METRICS_SIDELENGTH                  0.80    // % of height for straight side of H,R,H

void releaseTestData(testData &t) {
    if (t.stCombined != NULL) {delete t.stCombined; t.stCombined = NULL;}
    if (t.stLeft != NULL) {delete t.stLeft; t.stLeft = NULL;}
    if (t.stRight != NULL) {delete t.stRight; t.stRight = NULL;}
}

/*
    Assumes a vertical white line due to printer issue has split a digit into two.
    At this time detects:
    - 3: two (optional) small comps top-left and bottom-left + one tall comp on the right with indentations top and bottom
    - 0: two halves facing each other, curved, top and bottom more narrow than bottom, no indentations left & right
    - 5: only upon request (onlyTestCh set to '5'), left side has one curved comp on top + small bottom part, right side has little comp top + curved part on bottom
    - 6: only upon request (onlyTestCh set to '6'), left side has 2 intendations on right side, right side has little comp top + curved part on bottom
    - 7: only upon request (onlyTestCh set to '7'), bottom aligned left, element above bottom piece with left indentation, top-left part (roof of the 7)
    - 8
    = 9
    Returns '\0' if it can't make a determination
    Lax: 
    - 0: normal, suggest digit only if good match
    - 1: lax, suggest digit even if not all tests pass
    - 2: character is known to be a digit so be as lax as needed to return a digit match
    NOTE: may adjust the rerefence param rect based on pixels patterns.
*/
wchar_t testAsDisconnectedDigit(OCRRectPtr r, CGRect &rect, OCRWordPtr &stats, OCRResults *results, wchar_t onlyTestCh, wchar_t onlyTestCh2, bool checkWidth, int laxLevel, bool singleRect) {
    wchar_t resCh = '\0';
    
    if (!results->imageTests) return '\0';
    
    bool lax = (laxLevel >= 1);
    bool forceDigit = (laxLevel >= 2);
    
    bool testAllDigits = ((onlyTestCh == '\0') && (onlyTestCh2 == '\0'));
    
    testData t;
    
    SingleLetterTests *stLeft = NULL;
    SingleLetterTests *stRight = NULL;
    SingleLetterTests *stMain = NULL;
    
    // Whatever we do, adjust combined rect
    SingleLetterTests *stCombined = CreateSingleLetterTests(rect, results);
    ConnectedComponentList cplCombined;
    if (stCombined != NULL) {
        cplCombined = stCombined->getConnectedComponents(0.01);
        CGRect newRect = CGRect(rectLeft(rect) + cplCombined[0].xmin, rectBottom(rect) + cplCombined[0].ymin, cplCombined[0].getWidth(), cplCombined[0].getHeight());
        if ((newRect.origin.x != rect.origin.x) || (newRect.origin.y != rect.origin.y) || (newRect.size.width != rect.size.width) || (newRect.size.height != rect.size.height)) {
            rect = newRect;
            stCombined = CreateSingleLetterTests(newRect, results);
            if (stCombined != NULL)
                cplCombined = stCombined->getConnectedComponents(0.01);
        }
    } else {
        return '\0';    // Unlikely we can figure anything out, something is very wrong
    }
    
    if (checkWidth) {
        // Verify not too wide and not too narrow to be a normal digit (we don't detect '1' at this time)
        float referenceWidth = results->globalStats.averageWidthDigits.average;
        if (results->globalStats.averageWidthDigits.count < 10) {
            if (stats->averageWidthDigits.count >= 2)
                referenceWidth = stats->averageWidthDigits.average;
            else
                return '\0';
        }
        // Using 20% margin because of Walmart receipt in https://www.pivotaltracker.com/story/show/110581644
        if ((rect.size.width > referenceWidth * 1.20) || (rect.size.width < referenceWidth * 0.80))
            return '\0';
    }
    
    float maxWidthForLeniency = 10.8;
    
    if ((onlyTestCh == '$') || (onlyTestCh2 == '$')) {
        bool doit = true;

        
        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft;
        if (stRight != NULL) delete stRight;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 4) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            }
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
                if ((stRight == NULL) || (stLeft == NULL))
                    doit = false;
            } else
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        //  --------------
        //0|              |
        //1|     11       |
        //2|    111 111   |
        //3|  11111  111  |
        //4| 111111   111 |
        //5| 111  1   111 |
        //6| 11   1    11 |
        //7| 11           |
        //8| 111          |
        //9| 111111       |
        //a|  11111       |
        //b|    111 111   |
        //c|     11  111  |
        //d|      1  1111 |
        //e| 1        111 |
        //f| 11        11 |
        //#| 111      111 |
        //#| 1111    1111 |
        //#|  11111  111  |
        //#|   1111 1111  |
        //#|              |
        //  --------------
        // Test for above pattern
        if ((stCombined != NULL) && (cplCombined.size() == 5)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.20, 0.15);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.15, 0.15);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.15, 0.20);
            CGRect bottomRightRect = getBottomRightRect(rect, cplCombined, 0.15, 0.20);
            if ((topLeftRect.size.height > rect.size.height * 0.25)
                && (bottomLeftRect.size.height > rect.size.height * 0.15)
                && (topRightRect.size.height > rect.size.height * 0.15)
                && (bottomRightRect.size.height > rect.size.height * 0.25)) {
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results);
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results);
                if ((stTopLeft != NULL) && (stBottomLeft != NULL) && (stTopRight != NULL) && (stBottomRight != NULL)) {
                    if ((isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right)
                        && isCurved(bottomRightRect, stBottomRight, SingleLetterTests::Left))
                        ) {
                        matchingPattern = true;
                    }
                }
                if (stTopLeft != NULL) delete stTopLeft;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (stTopRight != NULL) delete stTopRight;
                if (stBottomRight != NULL) delete stBottomRight;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '$';
                }
            }
        }
        
        //  -------------
        //0|             |
        //1|     11      |
        //2|   1111 11   |
        //3|  11111 111  |
        //4| 11111   111 |
        //5| 11       11 |
        //6| 11       1  |
        //7| 111         |
        //8|  1111       |
        //9|  11111 11   |
        //a|    11  111  |
        //b|     1  111  |
        //c| 1       111 |
        //d| 11      111 |
        //e| 111     11  |
        //f| 11111  111  |
        //#|  111111111  |
        //#|    111 1    |
        //#|             |
        //  -------------
        // Test for above pattern
        if ((stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.20, 0.15);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.15, 0.20);
            CGRect bottomRect = getBottomRect(rect, cplCombined, 0.15);
            if ((topLeftRect.size.height > rect.size.height * 0.25)
                && (topRightRect.size.height > rect.size.height * 0.15)
                && (bottomRect.size.height > rect.size.height * 0.25)
                && (topLeftRect.size.height + bottomRect.size.height >= rect.size.height * 0.75)) {
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results);
                SingleLetterTests *stBottom = CreateSingleLetterTests(bottomRect, results);
                if ((stTopLeft != NULL) && (stTopRight) && (stBottom != NULL)) {
                    if ((isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right)
                        && isCurved(bottomRect, stBottom, SingleLetterTests::Top)
                        && isTopDownDiagonal(topRightRect, stTopRight))
                        ) {
                        matchingPattern = true;
                    }
                }
                if (stTopLeft != NULL) delete stTopLeft;
                if (stBottom != NULL) delete stBottom;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '$';
                }
            }
        }
        
        //  -------------
        //0|             |
        //1|     11 1    |
        //2|   11111111  |
        //3|  11111 111  |
        //4| 1111    111 |
        //5| 11       11 |
        //6| 11          |
        //7| 111         |
        //8|  1111       |
        //9|  1111  11   |
        //a|    11  111  |
        //b|     1  111  |
        //c| 1       111 |
        //d| 11      111 |
        //e| 111 1  111  |
        //f|  1111  111  |
        //#|  11111111   |
        //#|    11       |
        //#|             |
        //  -------------
        // Test for above pattern
        if ((stCombined != NULL) && (cplCombined.size() == 3)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.20, 0.15);
            CGRect bottomRect = getBottomRect(rect, cplCombined, 0.15);
            if ((topLeftRect.size.height > rect.size.height * 0.25)
                && (bottomRect.size.height > rect.size.height * 0.25)
                && (topLeftRect.size.height + bottomRect.size.height >= rect.size.height * 0.75)) {
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                SingleLetterTests *stBottom = CreateSingleLetterTests(bottomRect, results);
                if ((stTopLeft != NULL) && (stBottom != NULL)) {
                    if ((isCurved(topLeftRect, stTopLeft, SingleLetterTests::Bottom)
                        && isCurved(bottomRect, stBottom, SingleLetterTests::Top))
                        ) {
                        matchingPattern = true;
                    }
                }
                if (stTopLeft != NULL) delete stTopLeft;
                if (stBottom != NULL) delete stBottom;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '$';
                }
            }
        }
        
        if (stRight != NULL) delete stRight;
        if (stLeft != NULL) delete stLeft;
        if (stCombined != NULL) delete stCombined;
        return '\0';
    } // onlyTestCh == '$'

    
    if (testAllDigits || (onlyTestCh == '8') || (onlyTestCh2 == '8')) {
        bool doit = true;
        // For small font, need to be more tolerant and accept finding 4 out of 6 indentations
        int numAcceptedFailures = (((results->globalStats.averageWidthDigits.average != 0) && (results->globalStats.averageWidthDigits.average < maxWidthForLeniency))? 2:0);
        int numFailures = 0;
        
        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft;
        if (stRight != NULL) delete stRight;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 4) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            }
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
                if ((stRight == NULL) || (stLeft == NULL))
                    doit = false;
            } else
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        bool abort = false;  // Need another variable because below we have a few tests which don't depend on some requirements controlled by "doit"
        
        // Disqualify the below from getting tagged as a '8' (wide gap in the center)
        //  ---------------
        //0|               |
        //1|   11      1   |
        //2|   11      1   |
        //3|   1        1  |
        //4|   1        1  |
        //5|  11       111 |
        //6|  11       11  |
        //7|  1         1  |
        //8|  1         1  |
        //9|  1        111 |
        //a|  1         1  |
        //b|  11        1  |
        //c|  1         1  |
        //d| 11        11  |
        //e| 11        111 |
        //f|  11       11  |
        //#|  11      11   |
        //#|  11      11   |
        //#|   111  1111   |
        //#|    11  11     |
        //#|               |
        //  ---------------
        // gap = 7/13 (54%)
        if (stCombined != NULL) {
            SegmentList slCenter = stCombined->getHorizontalSegments(0.50, 0.25);
            float gapCenter = largestGap(slCenter);
            if (gapCenter > rect.size.width * 0.40) {
                abort = true;
                doit = false;
            }
        }
        
        // Don't accept the below as '8', return as 'H' instead
        //  ----------------
        //0|                |
        //1|  111        11 |
        //2|  111       111 |
        //3|  111       111 |
        //4|  111       111 |
        //5|  111       111 |
        //6|  111       111 |
        //7| 1111       111 |
        //8| 11111     1111 |
        //9| 11111 11111111 |
        //a|  11        11  |
        //b| 111         1  |
        //c|  111       11  |
        //d|  11        111 |
        //e|  11        111 |
        //f| 1111       11  |
        //#| 111        111 |
        //#|  111       111 |
        //#| 1111       111 |
        //#| 1111       111 |
        //#|   1         1  |
        //#|                |
        //  ----------------
        // Gap top/bottom: 7/14 (50%)
        if (!abort && (stCombined != NULL)) {
            SegmentList slTop25 = stCombined->getHorizontalSegments(0.125, 0.25);
            SegmentList slMiddle = stCombined->getHorizontalSegments(0.50, 0.20);
            SegmentList slBottom25 = stCombined->getHorizontalSegments(0.875, 0.25);
            float gapTop25 = largestGap(slTop25);
            float widthTop25 = totalWidth(slTop25);
            float gapMiddle = largestGap(slMiddle);
            float widthMiddle = totalWidth(slMiddle);
            float gapBottom25 = largestGap(slBottom25);
            float widthBottom25 = totalWidth(slBottom25);
            if ((widthTop25 > rect.size.width * 0.85)
                && (widthBottom25 > rect.size.width * 0.85)
                && (widthMiddle > rect.size.width * 0.85)
                && (gapTop25 > rect.size.width * 0.35)
                && (gapBottom25 > rect.size.width * 0.35)
                && (gapMiddle < rect.size.width * 0.15)) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return 'H';
            }
        }
        
        // TODO: we tag the below as 'H' (see https://drive.google.com/open?id=0B4jSQhcYsC9VSzFfS3hBRk1NR0U)
        //  ----------------
        //0|                |
        //1|   1            |
        //2|  111       1   |
        //3|  111       11  |
        //4|  111       11  |
        //5|  11111      1  |
        //6|  1111      111 |
        //7|  111       111 |
        //8|  111 11    111 |
        //9|  111  11   11  |
        //a|  11   11   111 |
        //b|  111  11   111 |
        //c|  11   111   11 |
        //d|  111  11    11 |
        //e| 111    11   11 |
        //f| 111        111 |
        //#|  11     1  111 |
        //#|  111    11 111 |
        //#| 1111      1111 |
        //#| 1111      1111 |
        //#| 1111       111 |
        //#| 111       1111 |
        //#|  1         11  |
        //#|                |
        //  ----------------
        
        //  ----------------
        //0|                |
        //1|   11 1111  11  |
        //2|  111   11  11  |
        //3| 111        11  |
        //4| 111        111 |
        //5| 111        111 |
        //6| 111        111 |
        //7|  111       111 |
        //8|  111   1   11  |
        //9|   11 1111      |
        //a|      1111      |
        //b|    1   11      |
        //c|   11           |
        //d|  111           |
        //e| 111            |
        //f| 111         11 |
        //#| 111        111 |
        //#| 111        111 |
        //#| 111        111 |
        //#|  111       11  |
        //#|  111       11  |
        //#|  111 1111  11  |
        //#|   11 1111      |
        //#|                |
        //  ----------------
        // Test for above pattern        
        if (!abort && (stCombined != NULL) && (cplCombined.size() == 8)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.15, 0.20);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.15, 0.20);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.15, 0.20);
            CGRect bottomRightRect = getBottomRightRect(rect, cplCombined, 0.15, 0.20);
            if ((topLeftRect.size.height > rect.size.height * 0.27) && (bottomLeftRect.size.height > rect.size.height * 0.27) && (topRightRect.size.height > rect.size.height * 0.27) && (bottomRightRect.size.height > rect.size.height * 0.27)) {
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results);
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results);
                if ((stTopLeft != NULL) && (stBottomLeft != NULL) && (stTopRight != NULL) && (stBottomRight != NULL)) {
                    if ((isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right) && isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Right))
                        || (isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right) && isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Right))) {
                            matchingPattern = true;
                    }
                }
                if (stTopLeft != NULL) delete stTopLeft;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (stTopRight != NULL) delete stTopRight;
                if (stBottomRight != NULL) delete stBottomRight;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '8';
                }
            }
        }
        
        //  -------------
        //0|             |
        //1|    111111   |
        //2|   11111111  |
        //3|    1    111 |
        //4|  1       11 |
        //5|  1       11 |
        //6|          11 |
        //7|   11     11 |
        //8|    11   11  |
        //9|    111 11   |
        //a|   111   11  |
        //b|    1    11  |
        //c|          11 |
        //d|          11 |
        //e|          11 |
        //f|          11 |
        //#|   11    111 |
        //#|   111  111  |
        //#|   11111111  |
        //#|     1111    |
        //#|             |
        //  -------------
        
        //  --------------
        //0|              |
        //1|     11  1    |
        //2|    111  11   |
        //3|          11  |
        //4|           11 |
        //5|           11 |
        //6|    1     11  |
        //7|    11   11   |
        //8|    111 111   |
        //9|    11  111   |
        //a|    11   11   |
        //b|          11  |
        //c|  1        1  |
        //d|  1        11 |
        //e|  1       111 |
        //f|    1    111  |
        //#|   111   111  |
        //#|    1111111   |
        //#|     11 11    |
        //#|              |
        //  --------------
        
        // Test for above pattern(s)
        if (!abort && (r->ch == '3') && (stCombined != NULL) && ((cplCombined.size() == 4) || (cplCombined.size() == 5))) {
            bool matchingPattern = false;
            CGRect rightTallRect = getTallestRightRect(rect, cplCombined, 0.15);
            CGRect middleLeftRect = getClosestRect(rect, 0.15, 0.50, cplCombined, 0.25, 0.20);
            if ((rightTallRect.size.height > rect.size.height * 0.80)
                && (middleLeftRect.size.height > rect.size.height * 0.20)) {
                SingleLetterTests *stMiddleLeft = CreateSingleLetterTests(middleLeftRect, results);
                bool doit = false;
                if (stMiddleLeft != NULL) {
                    // Test middle comp: either curved right or tall enough and on the left side
                    if (isCurved(middleLeftRect, stMiddleLeft, SingleLetterTests::Left))
                        doit = true;
                            // Tall enough
                    else if ((middleLeftRect.size.height > rect.size.height * 0.25)
                            // Mostly on the left side
                            && (rectRight(middleLeftRect) - (rectLeft(rect) + rect.size.width * 0.50) < rect.size.width * 0.15)) {
                        doit = true;
                    }
                }
                if (doit) {
                    // Make sure either top half or bottom half is semi-closed
                    SegmentList slLeft = stCombined->getVerticalSegments(0.20, 0.40);
                    if (slLeft.size() >= 1) {
                        float heightCombinedTop = 0, heightCombinedBottom = 0;
                        for (int i=0; i<slLeft.size(); i++) {
                            Segment s = slLeft[i];
                            if (s.startPos <= rect.size.height * 0.50)
                                heightCombinedTop += s.endPos - s.startPos + 1;
                            if (s.endPos >= rect.size.height * 0.50)
                                heightCombinedBottom += s.endPos - s.startPos + 1;
                        }
                        if ((heightCombinedTop >= rect.size.height * 0.45) || (heightCombinedBottom >= rect.size.height * 0.45))
                            matchingPattern = true;
                    }
                }
                if (stMiddleLeft != NULL) delete stMiddleLeft;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '8';
                }
            } // got relevant comps
        }
        
        //  -----------
        //0|           |
        //1|   1  1    |
        //2|   11  11  |
        //3|  11   11  |
        //4|  11    1  |
        //5| 111    11 |
        //6| 111    11 |
        //7|  11   11  |
        //8|   1       |
        //9|   11  1   |
        //a|   11  11  |
        //b|  11    1  |
        //c|  11    11 |
        //d|  11    11 |
        //e| 111    11 |
        //f|  11    11 |
        //#|  111   1  |
        //#|   111     |
        //#|           |
        //  -----------
        if (!abort && (stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            // Must be narrow openings top / bottom
            if (!t.gotGapSlTop15) {if (!t.gotSlTop15) {t.slTop15=stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15=true;} t.gapSlTop15 = largestGap(t.slTop15); t.gotGapSlTop15=true;}
            if (!t.gotGapSlBottom15) {if (!t.gotSlBottom15) {t.slBottom15=stCombined->getHorizontalSegments(0.925, 0.15); t.gotSlBottom15=true;} t.gapSlBottom15 = largestGap(t.slBottom15); t.gotGapSlBottom15=true;}
            if ((t.slTop15.size() >= 1) && (t.slBottom15.size() >= 1)
                && (t.gapSlTop15 < METRICS_GAP_LETTER_CLOSED_LAX * rect.size.width)
                && (t.gapSlBottom15 < METRICS_GAP_LETTER_CLOSED_LAX * rect.size.width)) {
                CGRect leftSideRect = getHighestLeftRect(rect, cplCombined, 0.15);
                CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.20, 0.20);
                CGRect bottomRightRect = getBottomRightRect(rect, cplCombined, 0.20, 0.20);
                if ((leftSideRect.size.height >= rect.size.height * 0.80)
                    && (topRightRect.size.height >= rect.size.height * 0.25)
                    && (bottomRightRect.size.height >= rect.size.height * 0.25)
                    && (rectBottom(bottomRightRect) >= rectTop(topRightRect))) {
                    SingleLetterTests *stLeftSide = CreateSingleLetterTests(leftSideRect, results);
                    if (stLeftSide != NULL) {
                        // Test if we have an intend around the middle on the left side
                        OpeningsTestResults res;
                        bool success = stLeftSide->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,    // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (success && (res.maxDepthCoord > rect.size.height * 0.35) && (res.maxDepthCoord < rect.size.height * 0.65)) {
                            matchingPattern = true;  // Good enough, no need to test the top right / bottom right comps further
                        } else {
                            SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results);
                            SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results);
                            if ((stTopRight != NULL) && (stBottomRight != NULL)
                                && isCurved(topRightRect, stTopRight, SingleLetterTests::Left)
                                && isCurved(bottomRightRect, stBottomRight, SingleLetterTests::Left)) {
                                matchingPattern = true;
                                delete stTopRight; delete stBottomRight;
                            }
                        }
                        delete stLeftSide;
                    }
                    if (matchingPattern) {
                        if (stCombined != NULL) delete stCombined;
                        if (stRight != NULL) delete stRight;
                        if (stLeft != NULL) delete stLeft;
                        return '8';
                    }
                }
            } // almost closed top/bottom
        }
        
        //  -----------
        //0|           |
        //1|  11       |
        //2|  11   11  |
        //3| 111   11  |
        //4| 11    111 |
        //5| 11    111 |
        //6| 11    111 |
        //7|  1    11  |
        //8|      111  |
        //9|   11 11   |
        //a|   1  11   |
        //b|  1    11  |
        //c| 11    11  |
        //d| 11    11  |
        //e| 11    111 |
        //f| 11    111 |
        //#|  1    11  |
        //#|  11  111  |
        //#|  11  111  |
        //#|           |
        //  -----------
        if (!abort && (stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            CGRect rightSideRect = getTallestRightRect(rect, cplCombined, 0.15);
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.20, 0.20);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.20, 0.20);
            if ((rightSideRect.size.height >= rect.size.height * 0.80)
                && (topLeftRect.size.height >= rect.size.height * 0.25)
                && (bottomLeftRect.size.height >= rect.size.height * 0.25)
                // topLeft comp entire above bottomLeft comp
                && (rectBottom(bottomLeftRect) >= rectTop(topLeftRect))) {
                SingleLetterTests *stRightSide = CreateSingleLetterTests(rightSideRect, results);
                if (stRightSide != NULL) {
                    // Test if we have an intend around the middle on the right side
                    OpeningsTestResults res;
                    bool success = stRightSide->getOpenings(res,
                        SingleLetterTests::Right,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (success && (res.maxDepthCoord > rect.size.height * 0.35) && (res.maxDepthCoord < rect.size.height * 0.65)) {
                        matchingPattern = true;  // Good enough, no need to test the top right / bottom right comps further
                    } else {
                        SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                        SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                        if ((stTopLeft != NULL) && (stBottomLeft != NULL)
                            && isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right)
                            && isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Right)) {
                            matchingPattern = true;
                            delete stTopLeft; delete stBottomLeft;
                        }
                    }
                    delete stRightSide;
                }
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '8';
                }
            }
        }
        
        //  ------------
        //0|            |
        //1|        1   |
        //2|      1 11  |
        //3|   1    111 |
        //4|   11   111 |
        //5|   11    11 |
        //6|   11    11 |
        //7|   11   111 |
        //8|        11  |
        //9|        11  |
        //a|        11  |
        //b|   1    111 |
        //c|   1    111 |
        //d|   1    111 |
        //e|   1    111 |
        //f|   1    111 |
        //#|   1    111 |
        //#|        111 |
        //#|       111  |
        //#|            |
        //  ------------
        // Test for above pattern        
        if (!abort && (stCombined != NULL) && ((cplCombined.size() == 4) || (cplCombined.size() == 5))) {
            bool matchingPattern = false;
            CGRect topLeftRect = getHighestLeftRect(rect, cplCombined, 0.15);
            CGRect bottomLeftRect = getLowestLeftRect(rect, cplCombined, 0.15);
            CGRect rightRect = getTallestRightRect(rect, cplCombined, 0.15);
            if (!compareRects(topLeftRect, bottomLeftRect)
                && !compareRects(topLeftRect, rightRect)
                && !compareRects(bottomLeftRect, rightRect)
                && (topLeftRect.size.height >= rect.size.height * 0.25)
                && (bottomLeftRect.size.height >= rect.size.height * 0.25)
                && (rightRect.size.height > rect.size.height * 0.80)) {
                SingleLetterTests *stRightSide = CreateSingleLetterTests(rightRect, results);
                CGRect topRightRect (rectLeft(rightRect), rectBottom(rightRect), rightRect.size.width, rightRect.size.height * 0.50);
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
                CGRect bottomRightRect (rectLeft(rightRect), rectBottom(rightRect) + rightRect.size.height * 0.50, rightRect.size.width, rightRect.size.height * 0.50);
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
                if ((stRightSide != NULL) && (stTopRight != NULL) && (stBottomRight != NULL)) {
                    // Test indent on right side of large right comp
                    OpeningsTestResults res;
                    bool success = stRightSide->getOpenings(res,
                        SingleLetterTests::Right,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (success) {
                        bool successTop = stTopRight->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,    // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        bool successBottom = stBottomRight->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,    // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (successTop || successBottom)
                            matchingPattern = true;

                    }
                }
                if (stRightSide != NULL) delete stRightSide;
                if (stTopRight != NULL) delete stTopRight;
                if (stBottomRight != NULL) delete stBottomRight;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '8';
                }
            }
        }
        
        if (doit) {
            // Test both halves to be made of a single comp spanning most of the height, having indents top & bottom, from right & left sides respectively
            ConnectedComponentList cplLeft = stLeft->getConnectedComponents();
            if (cplLeft.size() != 2) {
                doit = false;
            } else {
                ConnectedComponent ccMain = cplLeft[1];
                if (ccMain.getHeight() < rect.size.height * 0.80)
                    doit = false;
                }
        
            if (doit) {
                ConnectedComponentList cplRight = stRight->getConnectedComponents();
                if (cplRight.size() != 2) {
                    doit = false;
                } else {
                    ConnectedComponent ccMain = cplRight[1];
                    if (ccMain.getHeight() < rect.size.height * 0.80)
                        doit = false;
                }
            }
        
            // Test indentations on left half, on right side, top
            if (doit) {
                OpeningsTestResults res;
                // Looks like opening test succeeds in the range 0-0.50 even for a straight C! Need to slice off a single letter instance
                CGRect topLeftRect = CGRect(rectLeft(leftRect), rectBottom(leftRect), leftRect.size.width, leftRect.size.height * 0.60);
                // Accept inverted because we are slicing part of a pattern
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results, false, 0, 0.03, true);
                if (stTopLeft != NULL) {
                    bool success = stTopLeft->getOpenings(res,
                        SingleLetterTests::Right,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (!success) {
                        if (++numFailures > numAcceptedFailures)
                            doit = false;
                    }
                    delete stTopLeft;
                } else {
                    doit = false;
                }
            }
            // Test indentations on left half, on right side, bottom
            if (doit) {
                OpeningsTestResults res;
                // Looks like opening test succeeds in the range 0-0.50 even for a straight C! Need to slice off a single letter instance
                // Accept inverted because we are slicing part of a pattern
                CGRect bottomLeftRect = CGRect(rectLeft(leftRect), rectBottom(leftRect) + leftRect.size.height * 0.40, leftRect.size.width, leftRect.size.height * 0.60);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results, false, 0, 0.03, true);
                bool success = stBottomLeft->getOpenings(res,
                    SingleLetterTests::Right,
                    0.00,    // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                delete stBottomLeft;
            }
        
            // Test indentations on right half, on left side, top
            if (doit) {
                OpeningsTestResults res;
                CGRect topRightRect = CGRect(rectLeft(rightRect), rectBottom(rightRect), rightRect.size.width, rightRect.size.height * 0.60);
                // Accept inverted because we are slicing part of a pattern
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
                if (stTopRight != NULL) {
                    bool success = stTopRight->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,      // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (!success) {
                        if (++numFailures > numAcceptedFailures)
                            doit = false;
                    }
                    delete stTopRight;
                } else {
                    doit = false;
                }
            }
            // Test indentations on right half, on left side, bottom
            if (doit) {
                OpeningsTestResults res;
                CGRect bottomRightRect = CGRect(rectLeft(rightRect), rectBottom(rightRect) + rightRect.size.height * 0.40, rightRect.size.width, rightRect.size.height * 0.60);
                // Accept inverted because we are slicing part of a pattern
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results, false, 0, 0.03, true);
                if (stBottomRight != NULL) {
                    bool success = stBottomRight->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,      // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (!success) {
                        if (++numFailures > numAcceptedFailures)
                            doit = false;
                    }
                    delete stBottomRight;
                } else {
                    doit = false;
                }
            }
        }
        
        // Test external indents left & right. 01-11-2016 not sure why we were not testing these until now, presumably because initially we tested only with letters like '3'?
        if (doit) {
            OpeningsTestResults res;
            bool success = stCombined->getOpenings(res,
                    SingleLetterTests::Left,
                    0.15,      // Start of range to search (top/left)
                    0.95,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
            if (!success) {
                if (++numFailures > numAcceptedFailures)
                    doit = false;
            }
        }
        
        if (doit) {
            OpeningsTestResults res;
            bool success = stCombined->getOpenings(res,
                    SingleLetterTests::Right,
                    0.15,      // Start of range to search (top/left)
                    0.95,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
            if (!success) {
                if (++numFailures > numAcceptedFailures)
                    doit = false;
            }
        }
        
        //  -----------
        //0|           |
        //1|     11    |
        //2|   1 111   |
        //3|  11   1   |
        //4|  11   11  |
        //5| 111   111 |
        //6| 11    111 |
        //7| 11    111 |
        //8|  1     1  |
        //9| 11    11  |
        //a| 11    111 |
        //b| 11     11 |
        //c| 11    111 |
        //d| 11    111 |
        //e| 11    111 |
        //f| 11    111 |
        //#| 11    11  |
        //#|  1    11  |
        //#|  11  111  |
        //#|  11 111   |
        //#|      1    |
        //#|           |
        //  -----------
        // We verified that we have two tall halves, now we need to stop evaluating '8' for the above pattern where midsection has a gap of 4 out of 9 pixels width (use 4 out of 10 as limit)
        if (numFailures > 0) {
            SegmentList slMiddle = stCombined->getHorizontalSegments(0.50, 0.40);
            if (slMiddle.size() == 2) {
                float gap = slMiddle[1].startPos - slMiddle[0].endPos - 1;
                if (gap >= rect.size.width * 0.40) {
#if DEBUG
                    if (!singleRect && (r->next != NULL)) {
                        DebugLog("testAsDisconnectedDigit: aborting '8' test for [%c%c], likely a [0] in word [%s]", (unsigned short)r->ch, (unsigned short)(((r->next->ch == ' ') && (r->next->next != NULL))? r->next->next->ch:r->next->ch), toUTF8(r->word->text()).c_str());
                    } else {
                        DebugLog("testAsDisconnectedDigit: aborting '8' test for [%c], likely a [0] in word [%s]", (unsigned short)r->ch, toUTF8(r->word->text()).c_str());
                    }
#endif
                    doit = false;
                }
            }
        }
        
        // Now test combined pattern for narrower gaps top / bottom than 25% and 75% marks
        if (doit && (stCombined != NULL)) {
            SegmentList slTop = stCombined->getHorizontalSegments(0.05, 0.10);
            SegmentList sl25 = stCombined->getHorizontalSegments(0.25, 0.01);
            SegmentList sl75 = stCombined->getHorizontalSegments(0.75, 0.01);
            SegmentList slBottom = stCombined->getHorizontalSegments(0.95, 0.10);
            if (((slTop.size() == 1) || (slTop.size() == 2)) && ((sl25.size() == 1) || (sl25.size() == 2)) && ((sl75.size() == 1) || (sl75.size() == 2)) && ((slBottom.size() == 1) || (slBottom.size() == 2))) {
                int gapTop = ((slTop.size() == 1)? 0:slTop[1].startPos - slTop[0].endPos - 1);
                int gap25 = ((sl25.size() == 1)? 0:sl25[1].startPos - sl25[0].endPos - 1);
                int gap75 = ((sl75.size() == 1)? 0:sl75[1].startPos - sl75[0].endPos - 1);
                int gapBottom = ((slBottom.size() == 1)? 0:slBottom[1].startPos - slBottom[0].endPos - 1);
                if ((gap25 == 0)
                    || (gap75 == 0)
                    || (gap25 < gapTop)
                    || (gap25 < gapBottom)
                    || (gap75 < gapTop)
                    || (gap75 < gapBottom)) {
                    doit = false;
                }
            } else {
                doit = false;
            }
        } else {
            doit = false;
        }
        
        //  -------------
        //0|             |
        //1|     111     |
        //2|    111111   |
        //3|  1 1   111  |
        //4| 11      11  |
        //5| 11       11 |
        //6| 11       11 |
        //7|         11  |
        //8|        111  |
        //9|      1111   |
        //a|       111   |
        //b|         11  |
        //c|         11  |
        //d| 11      11  |
        //e| 11      11  |
        //f| 11      11  |
        //#|  1    1111  |
        //#|    111111   |
        //#|     111     |
        //#|             |
        //  -------------
        // One more test to avoid replacing above legit '3'
        if (doit && (r->ch == '3')) {
            SegmentList slLeft = stLeft->getVerticalSegments(0.50, 1.00);
            if ((slLeft.size() == 2) && (slLeft[1].startPos - slLeft[0].endPos - 1 > rect.size.height * 0.25))
                doit = false;
        }
    
        if (doit) {
            if (stRight != NULL) delete stRight;
            if (stLeft != NULL) delete stLeft;
            if (stCombined != NULL) delete stCombined;
            return '8';
        }
    } // onlyTestCh == '8'


    //  ------------
    //0|            |
    //1|   11  11   |
    //2|  111  111  |
    //3|  111  1111 |
    //4|  111   111 |
    //5|  111   111 |
    //6|  111   111 |
    //7|  111   111 |
    //8|  111   111 |
    //9|  111  1111 |
    //a|  111  1111 |
    //b|   1   1111 |
    //c|       1111 |
    //d|       111  |
    //e|       111  |
    //f|       111  |
    //#|  11  1111  |
    //#|  111 111   |
    //#|  111 111   |
    //#|   1  11    |
    //#|            |
    //  ------------
    if (testAllDigits || (onlyTestCh == '9') || (onlyTestCh2 == '9')) {
        bool doit = true;
        // For small font, need to be more tolerant and accept finding 1 out of 2 indentations (it's OK because '9' test includes several other checks such as curved top-right and bottom-right)
        int numAcceptedFailures = (((results->globalStats.averageWidthDigits.average != 0) && (results->globalStats.averageWidthDigits.average < maxWidthForLeniency))? 1:0);
        int numFailures = 0;
        bool otherOptions = ((onlyTestCh != '9') && (onlyTestCh != '\0')) || ((onlyTestCh2 != '9') && (onlyTestCh2 != '\0')) || testAllDigits;


        //  --------------
        //0|              |
        //1|       11     |
        //2|       111    |
        //3|   11  11111  |
        //4|   11    111  |
        //5|  111     11  |
        //6|  11      111 |
        //7|  11      111 |
        //8|  11      111 |
        //9|  11      111 |
        //a|  111     111 |
        //b|  111   11111 |
        //c|  111 111111  |
        //d|      11  11  |
        //e|          11  |
        //f|         111  |
        //#|  1      11   |
        //#| 111   1111   |
        //#| 111   111    |
        //#|  11  1111    |
        //#|  11 111      |
        //#|              |
        //  --------------
        //0|              |
        //1|     111      |
        //2|   11111      |
        //3|  111         |
        //4|  11       1  |
        //5| 11       111 |
        //6| 11       111 |
        //7| 11        11 |
        //8| 11        11 |
        //9| 111       11 |
        //a|  111      11 |
        //b|  111  11 111 |
        //c|   111111 111 |
        //d|      11   11 |
        //e|           11 |
        //f|           11 |
        //#|           1  |
        //#|  1           |
        //#| 111   111    |
        //#| 111111111    |
        //#|  1111111     |
        //#|              |
        //  --------------
        // Recognize the above
        if ((stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getClosestRect(rect, 0.10, 0.33, cplCombined, 0.40, 0.20);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.15, 0.20);
            CGRect rightRect = getClosestRect(rect, 0.80, 0.50, cplCombined, 0.40, 0.40);
            if ((topLeftRect.size.height > rect.size.height * 0.27)
                && (rightRect.size.height > rect.size.height * 0.27)
                // Bottom curved (from top) part close in width to top left curved part (from right)
                && (abs(bottomLeftRect.size.width - topLeftRect.size.width) <  rect.size.height * 0.17)) {
                SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                if ((stTopLeft != NULL) && (stBottomLeft != NULL)) {
                    if (isCurved(topLeftRect, stTopLeft, SingleLetterTests::Right)) {
                        if (isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Top)) {
                            matchingPattern = true;
                        } else {
                            //  --------------
                            //0|              |
                            //1|       11     |
                            //2|       111    |
                            //3|   11  11111  |
                            //4|   11    111  |
                            //5|  111     11  |
                            //6|  11      111 |
                            //7|  11      111 |
                            //8|  11      111 |
                            //9|  11      111 |
                            //a|  111     111 |
                            //b|  111   11111 |
                            //c|  111 111111  |
                            //d|      11  11  |
                            //e|          11  |
                            //f|         111  |
                            //#|  1      11   |
                            //#| 111   1111   |
                            //#| 111   111    |
                            //#|  11  1111    |
                            //#|  11 111      |
                            //#|              |
                            //  --------------
                            SegmentList slTopBottomPart = stCombined->getHorizontalSegmentsPixels(rectBottom(bottomLeftRect) - rectBottom(rect), rectBottom(bottomLeftRect) - rectBottom(rect));
                            SegmentList slTopTopPart = stCombined->getHorizontalSegmentsPixels(rectTop(bottomLeftRect) - rectBottom(rect) - 1, rectTop(bottomLeftRect) - rectBottom(rect));
                            float gapTopBottom = largestGap(slTopBottomPart), gapTopTop = largestGap(slTopTopPart);
                            if ((gapTopBottom > gapTopTop) && (gapTopTop < rect.size.width * 0.17)) {
                                matchingPattern = true;
                            }
                        }
                    } else {
                        //  -------------
                        //0|             |
                        //1|      11     |
                        //2|     11111   |
                        //3|  11  11111  |
                        //4|  11     111 |
                        //5|  11     111 |
                        //6|  11      11 |
                        //7| 111      11 |
                        //8|  11      11 |
                        //9|  11      11 |
                        //a|  11      11 |
                        //b|  11 1111111 |
                        //c|     1111111 |
                        //d|     11   11 |
                        //e|          11 |
                        //f|        111  |
                        //#| 11     111  |
                        //#| 11     11   |
                        //#| 11  11111   |
                        //#| 11 11111    |
                        //#|     11      |
                        //#|             |
                        //  -------------
                        // Start slTopTop right on top to catch the curving part on top if it helps us reduce the gap (as in above). I think this will not catch too many bad candidates because of the other requirements
                        SegmentList slTopTopPart = stCombined->getHorizontalSegmentsPixels(0, rectBottom(topLeftRect) - rectBottom(rect));
                        SegmentList slMiddleTopPart = stCombined->getHorizontalSegmentsPixels((rectBottom(topLeftRect) + rectTop(topLeftRect))/2 - rectBottom(rect)-1, (rectBottom(topLeftRect) + rectTop(topLeftRect))/2 - rectBottom(rect));
                        SegmentList slBottomTopPart = stCombined->getHorizontalSegmentsPixels(rectTop(topLeftRect) - rectBottom(rect), rectTop(topLeftRect) - rectBottom(rect));
                        SegmentList slTopBottomPart = stCombined->getHorizontalSegmentsPixels(rectBottom(bottomLeftRect) - rectBottom(rect), rectBottom(bottomLeftRect) - rectBottom(rect));
                        SegmentList slBottomBottomPart = stCombined->getHorizontalSegmentsPixels(rectTop(bottomLeftRect) - rectBottom(rect) - 1, rectTop(bottomLeftRect) - rectBottom(rect));
                        float gapTopTop = largestGap(slTopTopPart), gapBottomTop = largestGap(slBottomTopPart), gapMiddleTop = largestGap(slMiddleTopPart);
                        float gapTopBottom = largestGap(slTopBottomPart), gapBottomBottom = largestGap(slBottomBottomPart);
                        if ((gapTopTop < gapMiddleTop) && (gapBottomTop < gapMiddleTop)
                            && (gapTopTop < rect.size.width * 0.17) && (gapBottomTop < rect.size.width * 0.17)
                            && (gapTopBottom > gapBottomBottom) && (gapBottomBottom < rect.size.width * 0.17)) {
                            matchingPattern = true;
                        }
                    }
                }
                if (stTopLeft != NULL) delete stTopLeft;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '9';
                }
            }
        }
        
        //  -------------
        //0|             |
        //1|        111  |
        //2|        1111 |
        //3|    11   111 |
        //4|    11   111 |
        //5|    11   111 |
        //6|    11   111 |
        //7|    11   111 |
        //8|    11   111 |
        //9|    11   111 |
        //a|         111 |
        //b|         111 |
        //c|         111 |
        //d|    1    111 |
        //e|    11   111 |
        //f|    11  1111 |
        //#|    11  1111 |
        //#|        111  |
        //#|             |
        //  -------------
        // Test for above pattern        
        if ((laxLevel >= 1) && (stCombined != NULL) && ((cplCombined.size() == 4) || (cplCombined.size() == 5))) {
            bool matchingPattern = false;
            CGRect topLeftRect = getHighestLeftRect(rect, cplCombined, 0.15);
            CGRect bottomLeftRect = getLowestLeftRect(rect, cplCombined, 0.15);
            CGRect rightRect = getTallestRightRect(rect, cplCombined, 0.15);
            if ((topLeftRect.size.height >= rect.size.height * 0.25)
                && (bottomLeftRect.size.height >= rect.size.height * 0.15)
                && (topLeftRect.size.height > bottomLeftRect.size.height * 1.5)
                && (rightRect.size.height > rect.size.height * 0.85)) {
                SingleLetterTests *stRightSide = CreateSingleLetterTests(rightRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                CGRect topRightCorner (rectLeft(rightRect), rectBottom(rightRect), rightRect.size.width, rightRect.size.height * 0.20);
                CGRect bottomRightCorner (rectLeft(rightRect), rectBottom(rightRect) + rightRect.size.height * 0.80, rightRect.size.width, rightRect.size.height * 0.20);
                SingleLetterTests *stTopRightCorner = CreateSingleLetterTests(topRightCorner, results, false, 0, 0.03, true);
                SingleLetterTests *stBottomRightCorner = CreateSingleLetterTests(bottomRightCorner, results, false, 0, 0.03, true);
                if ((stRightSide != NULL) && (stBottomLeft != NULL) && (stTopRightCorner != NULL) && (stBottomRightCorner != NULL)) {
                    // Test indent on right side of large right comp
                    OpeningsTestResults res;
                    bool success = stRightSide->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (success) {
                        if (isTopDownDiagonal(bottomLeftRect, stBottomLeft, true)
                            && (isTopDownDiagonal(topRightCorner, stTopRightCorner, true)
                                || isBottomUpDiagonal(bottomRightCorner, stBottomRightCorner, true)))
                            matchingPattern = true;

                    }
                }
                if (stRightSide != NULL) delete stRightSide;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (stBottomRightCorner != NULL) delete stBottomRightCorner;
                if (stTopRightCorner != NULL) delete stTopRightCorner;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '9';
                }
            }
        }
        //  ------------
        //0|            |
        //1|    111     |
        //2|  1  11     |
        //3| 11    111  |
        //4| 11     11  |
        //5| 1       1  |
        //6|         11 |
        //7|         11 |
        //8| 1       11 |
        //9| 11      11 |
        //a| 11    1111 |
        //b|    1   111 |
        //c|         11 |
        //d|         11 |
        //e|         1  |
        //f|        11  |
        //#|  1    111  |
        //#| 11    11   |
        //#|  1 1111    |
        //#|            |
        //  ------------
        // h=18, topleft + middle left heights = 7 (38.9%)
        if ((cplCombined.size() == 5) || (cplCombined.size() == 6)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getHighestLeftRect(rect, cplCombined, 0.15);
            CGRect middleLeftRect = getClosestRect(rect, 0.15, 0.40, cplCombined, 0.20, 0.20);
            CGRect bottomLeftRect = getLowestLeftRect(rect, cplCombined, 0.15);
            CGRect rightSideRect = getTallestRightRect(rect, cplCombined, 0.15);
            if ((topLeftRect.size.height > 0)
                && (middleLeftRect.size.height > 0)
                && (bottomLeftRect.size.height > 0)
                && (topLeftRect.size.height + middleLeftRect.size.height > rect.size.height * 0.30)
                && (rightSideRect.size.height > rect.size.height * 0.80)) {
                CGRect topRightHalf (rectLeft(rightSideRect), rectBottom(rightSideRect), rightSideRect.size.width, rightSideRect.size.height * 0.65);
                CGRect bottomRightHalf (rectLeft(rightSideRect), rectBottom(rightSideRect) + rightSideRect.size.height * 0.35, rightSideRect.size.width, rightSideRect.size.height * 0.65);
                SingleLetterTests *stTopRightHalf = CreateSingleLetterTests(topRightHalf, results, false, 0, 0.03, true);
                SingleLetterTests *stBottomRightHalf = CreateSingleLetterTests(bottomRightHalf, results, false, 0, 0.03, true);
                if ((stBottomRightHalf != NULL) && (stTopRightHalf != NULL)) {
                    // Test indent on right side of large right comp
                    OpeningsTestResults res;
                    bool success = stTopRightHalf->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (success) {
                        success = stBottomRightHalf->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,    // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (success) {
                            // Now check we have a large enough opening bottom-right
                            SegmentList slLeft = stCombined->getVerticalSegments(0.125, 0.25);
                            if (slLeft.size() >= 2) {
                                float gapBottomLeft = slLeft[slLeft.size()-1].startPos - slLeft[slLeft.size()-2].endPos - 1;
                                if (gapBottomLeft > rect.size.height * 0.20)
                                    matchingPattern = true;
                            }
                        } // opening bottom-right
                    } // opening top-right
                }
                if (stTopRightHalf != NULL) delete stTopRightHalf;
                if (stBottomRightHalf != NULL) delete stBottomRightHalf;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '9';
                }
            }
        }

        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft; stLeft = NULL;
        if (stRight != NULL) delete stRight; stRight = NULL;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 4) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            } else
                doit = false;
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
            }
            if ((stRight == NULL) || (stLeft == NULL))
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        bool relaxCurvedRightSideRequirement = false;
        // Before continuing, check that the right rect is made of a comp spanning most of the height
        if (doit && (rightRect.size.height < rect.size.height * 0.80)) {
            doit = false;
            // Before we fail the test, check if we are in a situation where we *really* need to make a replacement
            // Is right side with a big vertical gap?
            if (singleRect && !rIsOnRightSide && ((r->ch == 'S') || (r->ch == '5'))) {
                SegmentList slMiddle = stLeft->getVerticalSegments(0.50, 1.00);
                if ((slMiddle.size() == 2) && (slMiddle[1].startPos - slMiddle[0].endPos - 1 > rect.size.height * 0.10) && (rightRect.size.height > rect.size.height * 0.50)) {
                    // Also skip test for curvature of right side in this case
                    doit = relaxCurvedRightSideRequirement = true;
                }
            }
        }
        
        ConnectedComponentList cplLeft;
        ConnectedComponent ccTopLeftCurved;
        ConnectedComponent ccBottomLeftSmall;
        // Analyze components on the left side, we are looking for one on top (taller) and one on the bottom (smaller)
        if (doit) {
            cplLeft = stLeft->getConnectedComponents();
            if (cplLeft.size() < 3)
                doit = false;
            else {
                ccTopLeftCurved = cplLeft[1];
                ccBottomLeftSmall = cplLeft[2];
                if ((ccTopLeftCurved.ymax > rect.size.height * 0.70) || (ccTopLeftCurved.getHeight() < rect.size.height * 0.30))
                    doit = false;
                if (doit && (ccBottomLeftSmall.ymin < rect.size.height * 0.70))
                    doit = false;
            }
        }
        
        if (doit) {
            // Test top indentation on right half, on left side
            OpeningsTestResults res;
            CGRect topRightRect = CGRect(rectLeft(rightRect), rectBottom(rightRect), rightRect.size.width, rightRect.size.height * 0.70);
            // Accept inverted because we are slicing part of a pattern
            SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
            if (stTopRight != NULL) {
                bool success = stTopRight->getOpenings(res,
                    SingleLetterTests::Left,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                delete stTopRight;
            } else {
                doit = false;
            }
        }
        
        // Now test curved top-left part for indentation
        if (doit) {
            CGRect curvedRect (rectLeft(leftRect) + ccTopLeftCurved.xmin, rectBottom(leftRect) + ccTopLeftCurved.ymin, ccTopLeftCurved.getWidth(), ccTopLeftCurved.getHeight());
            SingleLetterTests *stCurved = CreateSingleLetterTests(curvedRect, results);
            if (stCurved == NULL)
                doit = false;
            else {
                OpeningsTestResults res;
                bool success = stCurved->getOpenings(res,
                    SingleLetterTests::Right,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                delete stCurved;
            }
        }
        
        // Test right side that top/bottom are away from right side - unlike middle. This tests curvature
        if (doit && !relaxCurvedRightSideRequirement) {
            SegmentList slTop = stRight->getHorizontalSegments(0.05, 0.10);
            SegmentList slMiddle = stRight->getHorizontalSegments(0.50, 0.01);
            SegmentList slBottom = stRight->getHorizontalSegments(0.95, 0.10);
            if ((slTop.size() < 1) || (slMiddle.size() < 1) || (slBottom.size() < 1))
                doit = false;
            else {
                int topMargin = slTop[slTop.size()-1].endPos; int middleMargin = slMiddle[slMiddle.size()-1].endPos; int bottomMargin = slBottom[slBottom.size()-1].endPos;
                if ((topMargin > middleMargin) || (bottomMargin > middleMargin))
                    doit = false;
            }
        }
        
        if (doit || !otherOptions) {
            if (stRight != NULL) delete stRight;
            if (stLeft != NULL) delete stLeft;
            if (stCombined != NULL) delete stCombined;
            if (doit)
                return '9';
            else if (!otherOptions)
                return '\0';
        }
    } // onlyTestCh == '9'
    
    
    //   ----------
    //0|          |
    //1|      111 |
    //2|     1111 |
    //3|     1111 |
    //4|     1111 |
    //5|   1  111 |
    //6|  11  111 |
    //7|  11  111 |
    //8| 111  111 |
    //9| 111  111 |
    //a| 111  111 |
    //b| 111  111 |
    //c| 111 1111 |
    //d| 11  1111 |
    //e|     1111 |
    //f|     111  |
    //#|     111  |
    //#|     111  |
    //#|          |
    //  ----------
    if (testAllDigits || (onlyTestCh == '4') || (onlyTestCh2 == '4')) {
        bool doit = true;
        // For small font, need to be more tolerant and accept finding 2 out of 4 tests (2 indentations + top-right / bottom-right aligned to right side)
        int numAcceptedFailures = ((((results->globalStats.averageWidthDigits.average != 0) && (results->globalStats.averageWidthDigits.average < maxWidthForLeniency)) || forceDigit)? 2:0);
        int numFailures = 0;
        bool otherOptions = ((onlyTestCh != '4') && (onlyTestCh != '\0')) || ((onlyTestCh2 != '4') && (onlyTestCh2 != '\0')) || testAllDigits;
        
        //  ---------
        //0|         |
        //1|     111 |
        //2|     111 |
        //3|     111 |
        //4|    1111 |
        //5|     111 |
        //6|      11 |
        //7|   1  11 |
        //8|  11  11 |
        //9|  11  11 |
        //a|  11  11 |
        //b| 11   11 |
        //c| 11   1  |
        //d| 111 111 |
        //e| 111 111 |
        //f|      11 |
        //#|      11 |
        //#|      11 |
        //#|      11 |
        //#|     111 |
        //#|      11 |
        //#|         |
        //  ---------
        
        //  --------------
        //0|              |
        //1|          11  |
        //2|          11  |
        //3|           11 |
        //4|          111 |
        //5|     11   111 |
        //6|     11   111 |
        //7|          111 |
        //8|   1      111 |
        //9|   1      11  |
        //a|  11      11  |
        //b|  11      111 |
        //c| 111      111 |
        //d| 11111   1111 |
        //e|  111111 1111 |
        //f|          11  |
        //#|          11  |
        //#|          11  |
        //#|          11  |
        //#|          11  |
        //#|           1  |
        //#|              |
        //  --------------
        // Test for above pattern
        if ((stCombined != NULL) && ((cplCombined.size() == 3) || (cplCombined.size() == 4))) {
            bool matchingPattern = false;
            CGRect leftSideRect = getTallestLeftRect(rect, cplCombined, 0.15);
            CGRect rightSideRect = getTallestRightRect(rect, cplCombined, 0.15);
            if (!compareRects(leftSideRect, rightSideRect)
                && (leftSideRect.size.height >= rect.size.height * 0.25)
                && (rightSideRect.size.height >= rect.size.height * 0.90)
                && (rectTop(leftSideRect) < rectBottom(rect) + rect.size.height * 0.70)) {
                SingleLetterTests *stRightSide = CreateSingleLetterTests(rightSideRect, results);
                SingleLetterTests *stLeftSide = CreateSingleLetterTests(leftSideRect, results);
                if ((stRightSide != NULL) && (stLeftSide != NULL)
                    && isCurved(leftSideRect, stLeftSide, SingleLetterTests::Right)) {
                    matchingPattern = true;
                    // One more thing: if we had an extra comp, check that it fits above the left rect element + to the right
                    if (cplCombined.size() == 4) {
                        matchingPattern = false;
                        ConnectedComponent ccExtra = cplCombined[3];
                        CGRect extraRect(rectLeft(rect)+ccExtra.xmin, rectBottom(rect)+ccExtra.ymin, ccExtra.getWidth(), ccExtra.getHeight());
                        SegmentList slTopLeftRect = stLeftSide->getHorizontalSegments(0.10, 0.20);
                        if (slTopLeftRect.size() == 1) {
                            float gapTopLeft = rectLeft(leftSideRect) - rectLeft(rect) + slTopLeftRect[0].startPos;
                            if ((ccExtra.xmin >= gapTopLeft)
                                && (rectTop(extraRect) <= rectBottom(leftSideRect))) {
                                matchingPattern = true;
                            }
                        }
                    }
                }
                if (stRightSide != NULL) delete stRightSide;
                if (stLeftSide != NULL) delete stLeftSide;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '4';
                }
            }
        }
        
        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft;
        if (stRight != NULL) delete stRight;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 3) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            } else
                doit = false;
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
                if ((stRight == NULL) || (stLeft == NULL))
                    doit = false;
            } else
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        if (doit) {
            // Before continuing, check that the right rect is made of a comp spanning most of the height
            ConnectedComponentList cplRight = stRight->getConnectedComponents();
            if (cplRight.size() < 2) {
                doit = false;
            } else {
                ConnectedComponent ccMain = cplRight[1];
                if (ccMain.getHeight() < rect.size.height * 0.80)
                    doit = false;
            }
        }
        
        ConnectedComponentList cplLeft;
        ConnectedComponent ccTopLeftCurved;
        // Analyze components on the left side, we are looking for one on top
        if (doit) {
            cplLeft = stLeft->getConnectedComponents();
            if (cplLeft.size() != 2)
                doit = false;
            else {
                ccTopLeftCurved = cplLeft[1];
                if ((ccTopLeftCurved.ymax > rect.size.height * 0.73) || (ccTopLeftCurved.getHeight() < rect.size.height * 0.30))
                    doit = false;
            }
        }
        
        if (doit) {
            // Test top indentation on right half, on left side
            OpeningsTestResults res;
            CGRect topRightRect = CGRect(rectLeft(rightRect), rectBottom(rightRect), rightRect.size.width, rightRect.size.height * 0.70);
            // Accept inverted because we are slicing part of a pattern
            SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
            if (stTopRight != NULL) {
                bool success = stTopRight->getOpenings(res,
                    SingleLetterTests::Left,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                delete stTopRight;
            } else {
                doit = false;
            }
        }
        
        // Now test curved top-left part for indentation
        if (doit) {
            CGRect curvedRect (rectLeft(leftRect) + ccTopLeftCurved.xmin, rectBottom(leftRect) + ccTopLeftCurved.ymin, ccTopLeftCurved.getWidth(), ccTopLeftCurved.getHeight());
            SingleLetterTests *stCurved = CreateSingleLetterTests(curvedRect, results, false, 0, 0.03, true);
            if (stCurved == NULL)
                doit = false;
            else {
                OpeningsTestResults res;
                bool success = stCurved->getOpenings(res,
                    SingleLetterTests::Right,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                delete stCurved;
            }
        }
        
        // Test right side that top/bottom are as close to right side as the middle. This tests absence of curvature (i.e. not a '9')
        if (doit) {
            SegmentList slTop = stCombined->getHorizontalSegments(0.05, 0.10);
            SegmentList slMiddle = stCombined->getHorizontalSegments(0.50, 0.15);
            SegmentList slBottom = stCombined->getHorizontalSegments(0.95, 0.10);
            if ((slTop.size() < 1) || (slMiddle.size() < 1) || (slBottom.size() < 1))
                doit = false;
            else {
                int topMargin = rect.size.width - slTop[slTop.size()-1].endPos - 1;
                int middleMargin = rect.size.width - slMiddle[slMiddle.size()-1].endPos - 1;
                int bottomMargin = rect.size.width - slBottom[slBottom.size()-1].endPos - 1;
                if (topMargin > rect.size.width * 0.15) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                if (middleMargin > rect.size.width * 0.15) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
                if (bottomMargin > rect.size.width * 0.15) {
                    if (++numFailures > numAcceptedFailures)
                        doit = false;
                }
            }
        }
        
        if (doit || !otherOptions) {
            if (stRight != NULL) delete stRight;
            if (stLeft != NULL) delete stLeft;
            if (stCombined != NULL) delete stCombined;
            if (doit)
                return '4';
            else if (!otherOptions)
                return '\0';
        }
    } // onlyTestCh == '4'
    
    if (testAllDigits || (onlyTestCh == '6') || (onlyTestCh2 == '6')) {
        bool doit = true;
        // For small font, need to be more tolerant and accept finding 4 out of 6 indentations
        int numAcceptedFailures = (((results->globalStats.averageWidthDigits.average != 0) && (results->globalStats.averageWidthDigits.average < maxWidthForLeniency))? 1:0);
        int numFailures = 0;
        bool otherOptions = ((onlyTestCh != '6') && (onlyTestCh != '\0')) || ((onlyTestCh2 != '6') && (onlyTestCh2 != '\0')) || testAllDigits;
    
        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft;
        if (stRight != NULL) delete stRight;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 4) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            } else
                doit = false;
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
                if ((stRight == NULL) || (stLeft == NULL))
                    doit = false;
            } else
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        //  ----------------
        //0|                |
        //1|   11 1111  11  |
        //2|  111  111  11  |
        //3|  111           |
        //4| 111            |
        //5| 111            |
        //6| 111            |
        //7| 111            |
        //8| 111   111      |
        //9| 111  1111      |
        //a| 111  1111  1   |
        //b| 111   11   11  |
        //c| 111        11  |
        //d| 111        11  |
        //e| 111        11  |
        //f| 111        111 |
        //#| 111        111 |
        //#| 111        111 |
        //#| 111        11  |
        //#| 1111       11  |
        //#|  111       11  |
        //#|   11 1111  11  |
        //#|   1  1111      |
        //#|                |
        //  ----------------
        // Accept the above
        if (doit && (stCombined != NULL) && (cplCombined.size() >= 6)) {
            bool matchingPattern = false;
            CGRect leftRect = getClosestRect(rect, 0.00, 0.50, cplCombined, 0.15, 0.20);
            CGRect topCenterRect = getClosestRect(rect, 0.50, 0.00, cplCombined, 0.30, 0.15);
            CGRect centerRect = getClosestRect(rect, 0.50, 0.40, cplCombined, 0.30, 0.20);
            CGRect bottomCenterRect = getClosestRect(rect, 0.50, 1.00, cplCombined, 0.30, 0.15);
            CGRect bottomRightRect = getClosestRect(rect, 0.90, 0.70, cplCombined, 0.15, 0.20);
            if ((leftRect.size.height > rect.size.height * 0.80) // Most of the height
                && ((topCenterRect.size.height > 0) && (topCenterRect.size.height < rect.size.height * 0.20))
                && ((centerRect.size.height > 0) && (centerRect.size.height < rect.size.height * 0.20))
                && ((bottomCenterRect.size.height > 0) && (bottomCenterRect.size.height < rect.size.height * 0.20))
                && (OCRUtilsOverlappingX(topCenterRect, centerRect) && OCRUtilsOverlappingX(centerRect, bottomCenterRect) && OCRUtilsOverlappingX(topCenterRect, bottomCenterRect))
                && (bottomRightRect.size.height > rect.size.height * 0.30)) {
                    // One more test: check that there is nothing on the right side between topCenterRect and centerRect
                    float segmentY = ((rectTop(topCenterRect) + rectBottom(centerRect)) / 2 - rectBottom(rect)) / rect.size.height;
                    SegmentList sl = stCombined->getHorizontalSegments(segmentY, 0.10);
                    if ((sl.size() == 1) && (sl[0].endPos - leftRect.size.width < 0.01)) {
                        matchingPattern = true;
                    }
            }
            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '6';
            }
        } // special pattern in 3 slices
        
        //  ---------------
        //0|               |
        //1|     11 111    |
        //2|   1111 111    |
        //3|  1111   11    |
        //4|  111          |
        //5| 111           |
        //6| 111           |
        //7| 11            |
        //8| 111    1      |
        //9| 1111   111    |
        //a| 11111  1111   |
        //b| 1111    111   |
        //c| 111      1    |
        //d| 11            |
        //e| 111        11 |
        //f| 11         11 |
        //#| 111        11 |
        //#| 111           |
        //#|  111    111   |
        //#|  1111  1111   |
        //#|   1111 1111   |
        //#|               |
        //  ---------------
        // Accept the above
        if ((stCombined != NULL) && (cplCombined.size() == 6)) {
            bool matchingPattern = false;
            CGRect leftRect = getClosestRect(rect, 0.00, 0.50, cplCombined, 0.30, 0.25);
            CGRect topRightRect = getClosestRect(rect, 0.80, 0.10, cplCombined, 0.30, 0.15);
            CGRect centerRightRect = getClosestRect(rect, 0.70, 0.40, cplCombined, 0.30, 0.20);
            CGRect bottomCenterRect = getClosestRect(rect, 1.00, 0.75, cplCombined, 0.25, 0.20);
            CGRect bottomRightRect = getClosestRect(rect, 0.70, 1.00, cplCombined, 0.30, 0.20);
            if ((leftRect.size.height > rect.size.height * 0.80) // Most of the height
                // topRight found and in upper 25% part
                && ((topRightRect.size.height > 0) && (rectTop(topRightRect) < rectBottom(rect) + rect.size.height * 0.25))
                && ((centerRightRect.size.height > 0) && (bottomCenterRect.size.height > 0) && (bottomRightRect.size.height > 0))
                && ((rectTop(bottomRightRect) > rectTop(bottomCenterRect)) && (rectTop(bottomCenterRect) > rectTop(centerRightRect)))
                && (rectLeft(bottomCenterRect) >= rectRight(centerRightRect))
                && (rectLeft(bottomCenterRect) >= rectRight(bottomRightRect))
                && (centerRightRect.size.height + bottomCenterRect.size.height + bottomRightRect.size.height > rect.size.height * 0.35)) {
                matchingPattern = true;
            }
            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '6';
            }
        } // special pattern in 3 slices
        
        //  ----------
        //0|          |
        //1|     11   |
        //2|     111  |
        //3|    1 11  |
        //4| 11       |
        //5| 11       |
        //6| 11       |
        //7| 11       |
        //8| 11   1   |
        //9| 11   11  |
        //a| 11   11  |
        //b| 11   111 |
        //c| 11   111 |
        //d| 11   111 |
        //e| 11   111 |
        //f| 11   111 |
        //#| 11   111 |
        //#| 11   111 |
        //#| 11  111  |
        //#|     111  |
        //#|          |
        //  ----------
        if ((stCombined != NULL) && ((cplCombined.size() == 4) || (cplCombined.size() == 5))) {
            bool matchingPattern = false;
            CGRect leftRect = getTallestLeftRect(rect, cplCombined, 0.10);
            CGRect topRightRect = getLargestTopRect(rect, cplCombined, 0.15);
            CGRect bottomRightRect = getTallestRightRect(rect, cplCombined, 0.15);
            if ((leftRect.size.height > rect.size.height * 0.70) // Most of the height
                // topRight found and in upper 25% part
                && ((topRightRect.size.height > 0) && (rectTop(topRightRect) <= rectBottom(leftRect)))
                // Found bottom right, tall enough, and it extends all the way to the bottom
                && ((bottomRightRect.size.height > rect.size.height * 0.40) && (rectTop(rect) - rectTop(bottomRightRect) <= rect.size.height * 0.10))) {
                // Check there is a large enough gap top-right
                SegmentList slRight = stCombined->getVerticalSegments(0.875, 0.25);
                if (slRight.size() == 2) {
                    float gapTopRight = slRight[1].startPos - slRight[0].endPos - 1;
                    if (gapTopRight > rect.size.height * 0.20)
                        matchingPattern = true;
                }
            }
            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '6';
            }
        } // special pattern in 3 slices
        
        if (doit) {
             // Make sure the component on the right side is positionned well and not to short
            if (!((rectBottom(rightRect) >= rect.size.height * 0.40) && (rightRect.size.height > rect.size.height * 0.20))) {
                doit = false;
            }
        }
        
        // Before continuing, check that the first rect is made of a comp spanning most of the height (otherwise could be a '5' for example)
        if (doit) {
            ConnectedComponentList cplLeft = stLeft->getConnectedComponents();
            if (cplLeft.size() < 2) {
                delete stLeft; delete stRight;
                return '\0';
            }
            ConnectedComponent ccMain = cplLeft[1];
            if (ccMain.getHeight() < rect.size.height * 0.70)
                doit = false;
        }
        
        // Also check that the right side is NOT one single tall rect (if it is, could be a '8' or '0' but not a '6'!)
        if (doit) {
            ConnectedComponentList cplRight = stRight->getConnectedComponents();
            if ((cplRight.size() == 2) && (cplRight[0].getHeight() >= rect.size.height * 0.80))
                doit = false;
        }
        
        //  ----------------
        //0|                |
        //1| 11             |
        //2|  111 1  1      |
        //3|  11111 11111   |
        //4|  111 1  1111   |
        //5| 1111      111  |
        //6| 111       111  |
        //7|  11        11  |
        //8|  11        11  |
        //9|  11        11  |
        //a|  11        11  |
        //b|  11        111 |
        //c|  11        11  |
        //d|  111      111  |
        //e|  111    11111  |
        //f|  111  1111111  |
        //#|  111   11111   |
        //#|  111     11    |
        //#|  111           |
        //#|  111           |
        //#|  111           |
        //#|  11            |
        //#|  111           |
        //#|  111           |
        //#|  111           |
        //#|  111           |
        //#|  111           |
        //#|                |
        //  ----------------
        // Check bottom-right side is not empty, as in the above
        if (!t.gotSlW85w30) {t.slW85w30 = stCombined->getVerticalSegments(0.85, 0.30); t.gotSlW85w30 = true;}
        if ((t.slW85w30.size() == 0) || (t.slW85w30[t.slW85w30.size()-1].endPos < rect.size.height * 0.80))
            doit = false;
        
        bool leftSideHasTwoIndentations = true;
        if (doit) {
            // Test 2 indentations on left half, on right side
            OpeningsTestResults res;
            CGRect topLeftRect = CGRect(rectLeft(r->rect), rectBottom(r->rect), r->rect.size.width, r->rect.size.height * 0.70);
            // Accept inverted because we are slicing part of a pattern
            SingleLetterTests *stTopLeft = CreateSingleLetterTests(topLeftRect, results, false, 0, 0.03, true);
            if (stTopLeft != NULL) {
                bool success = stTopLeft->getOpenings(res,
                    SingleLetterTests::Right,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        leftSideHasTwoIndentations = false;
                }
                delete stTopLeft;
            } else
                leftSideHasTwoIndentations = false;
    
            CGRect bottomLeftRect = CGRect(rectLeft(leftRect), rectBottom(leftRect) + leftRect.size.height * 0.30, leftRect.size.width, leftRect.size.height * 0.70);
            // Accept inverted because we are slicing part of a pattern
            SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results, false, 0, 0.03, true);
            if (stBottomLeft != NULL) {
                bool success = stBottomLeft->getOpenings(res,
                    SingleLetterTests::Right,
                    0.50,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success) {
                    if (++numFailures > numAcceptedFailures)
                        leftSideHasTwoIndentations = false;
                }
            } else
                leftSideHasTwoIndentations = false;
        }
        
        //  -------------
        //0|             |
        //1|     1111    |
        //2|    111111   |
        //3|  1 11 1111  |
        //4|  1      11  |
        //5|  1          |
        //6| 11          |
        //7| 11          |
        //8| 11  111     |
        //9| 11 111111   |
        //a| 11 111111   |
        //b| 11     111  |
        //c| 11      11  |
        //d| 11       11 |
        //e| 11       11 |
        //f| 11       11 |
        //#| 11      11  |
        //#|  1 1   111  |
        //#|    1111111  |
        //#|     111 1   |
        //#|             |
        //  -------------
        // Need to accept the above, where left side is too narrow to have indentations
        if (!leftSideHasTwoIndentations) {
            if (leftRect.size.width > rect.size.width * 0.21)
                doit = false;
        }
        
        // Now test right half: small top-right part (optional) + curved bottom part
        if (doit) {
            ConnectedComponentList cpl = stRight->getConnectedComponents();
            if (cpl.size() < 2)
                doit = false;
            else {
                ConnectedComponent bottomCurvedClosingCC = cpl[1];
                if (cpl.size() >= 3) {
                    ConnectedComponent smallTopCC = cpl[2];
                    CGRect smallTopRect (rectLeft(rightRect) + smallTopCC.xmin, rectBottom(rightRect) + smallTopCC.ymin, smallTopCC.getWidth(), smallTopCC.getHeight());
                    if (rectTop(smallTopRect) > rectBottom(rightRect) + rect.size.height * 0.40) {
                        doit = false;
                    }
                    if (!leftSideHasTwoIndentations) {
                        // If left side was too narrow to be indented, it must be that small top CC is curved up
                        SingleLetterTests *stSmallTop = CreateSingleLetterTests(smallTopRect, results);
                        if (stSmallTop != NULL) {
                            OpeningsTestResults res;
                            bool success = stSmallTop->getOpenings(res,
                                SingleLetterTests::Bottom,
                                0.00,      // Start of range to search (top/left)
                                1.00,    // End of range to search (bottom/right)
                                SingleLetterTests::Bound,   // Require start (top/left) bound
                                SingleLetterTests::Bound  // Require end (bottom/right) bound
                                );
                            if (!success)
                                doit = false;
                        } else
                            doit = false;
                    }
                }
                if (doit) {
                    float bottomYAdjusted = rectBottom(rightRect) + bottomCurvedClosingCC.ymin;
                    float topYAdjusted = rectBottom(rightRect) + bottomCurvedClosingCC.ymax;
                    if ((topYAdjusted < rectBottom(rect) + rect.size.height * 0.60)
                        || (bottomYAdjusted > rectBottom(rect) + rect.size.height * 0.60)) {
                        doit = false;
                    }
                }
                // Test opening of curved part
                if (doit) {
                    CGRect curvedCC (rectLeft(rightRect) + bottomCurvedClosingCC.xmin, rectBottom(rightRect) + bottomCurvedClosingCC.ymin, bottomCurvedClosingCC.getWidth(), bottomCurvedClosingCC.getHeight());
                    SingleLetterTests *stCurved = CreateSingleLetterTests(curvedCC, results);
                    if (stCurved == NULL)
                        doit = false;
                    else {
                        OpeningsTestResults res;
                        bool success = stCurved->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,      // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (!success) {
                            if (++numFailures > numAcceptedFailures)
                                doit = false;
                        }
                        delete stCurved;
                    }
                }
            } // correct connected comps
        } // digit 6
        
        if (doit) {
            delete stRight; delete stLeft;
            return '6';
        } else {
            if (!otherOptions) {
                delete stRight; delete stLeft;
                return '\0';
            }
        }
    } // onlyTestCh == '6'
    
    // Below also test for '2' (instead of '7')
    //  -----------
    //0|           |
    //1| 111  11   |
    //2| 111 1111  |
    //3| 111  1111 |
    //4| 111  1111 |
    //5|  1   1111 |
    //6|      111  |
    //7|      111  |
    //8|     1111  |
    //9|     111   |
    //a|     111   |
    //b|  11 11    |
    //c|  11       |
    //d| 111       |
    //e| 111       |
    //f| 111 11111 |
    //#| 111 11111 |
    //#|      111  |
    //#|           |
    //  -----------
    if (testAllDigits || (onlyTestCh == '7') || (onlyTestCh2 == '7')) {
        
        //  --------------
        //0|              |
        //1|  11111   11  |
        //2| 1111111 1111 |
        //3| 1111111 1111 |
        //4| 11      1111 |
        //5|         1111 |
        //6|         111  |
        //7|         11   |
        //8|         11   |
        //9|              |
        //a|              |
        //b|      11      |
        //c|     1111     |
        //d|      11      |
        //e|     111      |
        //f|     111      |
        //#|     111      |
        //#|     111      |
        //#|     111      |
        //#|     111      |
        //#|     111      |
        //#|      1       |
        //#|              |
        //  --------------
        
        //  --------------
        //0|              |
        //1| 1111    111  |
        //2| 1111    1111 |
        //3|  11     1111 |
        //4|         1111 |
        //5|         111  |
        //6|         11   |
        //7|        111   |
        //8|        111   |
        //9|        111   |
        //a|        11    |
        //b|         1    |
        //c|              |
        //d|              |
        //e|              |
        //f|              |
        //#|              |
        //#|              |
        //#|              |
        //#|    111       |
        //#|    111       |
        //#|              |
        //  --------------
        
        // Recognize the above
        if ((stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.15, 0.15);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.15, 0.15);
            CGRect centerRect = getClosestRect(rect, 0.50, 0.70, cplCombined, 0.30, 0.25);
                // Two top components span most of the width
            if ((topLeftRect.size.height > 0)
                && (topRightRect.size.height > 0)
                && (centerRect.size.height > 0)
                && !compareRects(topRightRect, topLeftRect)
                && !compareRects(topRightRect, centerRect)
                && !compareRects(topLeftRect, centerRect)
                && (rect.size.width - rectRight(topRightRect) - rectLeft(topLeftRect) + 1 < rect.size.width * 0.10)
                // One of the top comps hugs the top
                && ((rectBottom(topLeftRect) - rectBottom(rect) < 0.01) || (rectBottom(topRightRect) - rectBottom(rect) < 0.01))
                // topRight element drops more down
                && (rectTop(topRightRect) > rectTop(topLeftRect))
                // Top right entirely to the right of top left
                && (rectLeft(topRightRect) >= rectRight(topLeftRect))
                // center element either entirely below topRight or just start 15% above its lowest part
                && (rectBottom(centerRect) > rectTop(topRightRect) - rect.size.height * 0.15)
                && (forceDigit || (centerRect.size.height > rect.size.height * 0.40))
                && (centerRect.size.height + topRightRect.size.height > rect.size.height * (forceDigit? 0.55:0.75))) {
                    matchingPattern = true;
            }
            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '7';
            }
        }
        
        bool doit = true;
        bool testForPossible2 = false;
        if (cplCombined.size() < 4)
            doit = false;
        else {
            // Identify the 3 comps: main CC curved to the right, top-left piece, bottom-left piece
            int mainCCIndex = -1, topLeftSmallCompIndex = -1, bottomLeftSmallCompIndex = -1;
            // Start with mainCC, it's the one with the rightmost X coordinate (probably also the largest). Note: need to ignore possible small comp bottom right (in '2')
            vector<int> highXMax = sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
            if (highXMax.size() == 0)
                doit = false;
            else {
                // Test first comps as long as there is a significant overlap, pick the one with lowest Y
                mainCCIndex = highXMax[0];
                int lowestY = cplCombined[mainCCIndex].ymin;
                ConnectedComponent mainCC = cplCombined[mainCCIndex];
                for (int i=1; i<highXMax.size(); i++) {
                    ConnectedComponent otherCC = cplCombined[highXMax[i]];
                    // Still X overlaps a lot with mainCC candidate?
                    int overlapAmount = otherCC.xmax - mainCC.xmin;
                    if (overlapAmount/mainCC.getWidth() < 0.50)
                        break;
                    
                    testForPossible2 = true;
                    
                    if (cplCombined[highXMax[i]].ymin < lowestY) {
                        mainCCIndex = highXMax[i];
                        mainCC = cplCombined[mainCCIndex];
                        lowestY = mainCC.ymin;
                    }
                }
            }
            int rightMostX = -1;
            for (int i=1; i<cplCombined.size(); i++) {
                ConnectedComponent cc = cplCombined[i];
                if (cc.xmax > rightMostX) {
                    rightMostX = cc.xmax;
                    mainCCIndex = i;
                }
            }
            // Get top left small comp, it's the one with the lowest Y coordinate
            int lowestY = -1;
            for (int i=1; i<cplCombined.size(); i++) {
                if (i == mainCCIndex) continue;
                ConnectedComponent cc = cplCombined[i];
                if ((lowestY < 0) || (cc.ymax < lowestY)) {
                    lowestY = cc.ymax;
                    topLeftSmallCompIndex = i;
                }
            }
            // Get bottom left small comp, it's the only index left
            for (int i=1; i<cplCombined.size(); i++) {
                if ((i == mainCCIndex) || (i == topLeftSmallCompIndex)) continue;
                bottomLeftSmallCompIndex = i;
                break;
            }
            
            // Now test these comps
            ConnectedComponent mainCC = cplCombined[mainCCIndex];
            ConnectedComponent topLeftCC = cplCombined[topLeftSmallCompIndex];
            ConnectedComponent bottomLeftCC = cplCombined[bottomLeftSmallCompIndex];
            
            if ((topLeftCC.xmax - mainCC.xmin > rect.size.width * 0.09)
                // Bottom small comp and main overlap X by more than one pixel (if width is 12)
                || (bottomLeftCC.xmax - mainCC.xmin > rect.size.width * 0.09)
                // Bottom small comp and main overlap Y by more than two pixels (if width is 12)
                || (mainCC.ymax - bottomLeftCC.ymin > rect.size.width * 0.18)
                || (topLeftCC.ymax >= bottomLeftCC.ymin)
                // Require top small comp to hug the top (at most one pixel away if height is 20)
                || (topLeftCC.ymin > rect.size.height * 0.05)
                // Main and top don't overlap Y
                || (topLeftCC.ymax < mainCC.ymin)) {
                doit = false;
            }
            if (doit) {
                // Test main CC for indentation on left side
                CGRect mainCCAdjustedRect (rectLeft(rect) + mainCC.xmin, rectBottom(rect) + mainCC.ymin, mainCC.getWidth(), mainCC.getHeight());
                SingleLetterTests *stTmp = CreateSingleLetterTests(mainCCAdjustedRect, results);
                if (stTmp == NULL)
                    doit = false;
                else {
                    OpeningsTestResults res;
                    bool success = stTmp->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,      // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (!success)
                        doit = false;
                    delete stTmp;
                }
            }
        } // Connected CC's check out

        char newCh = '7';
        
        if (doit && testForPossible2) {
            SegmentList slBottom = stCombined->getHorizontalSegments(0.925, 0.15);
            if (slBottom.size() >= 1) {
                if (slBottom[slBottom.size()-1].endPos > rect.size.width * 0.65)
                    newCh = '2';
            } else {
                doit = false;
            }
        }
        if (doit) {
            if (stCombined != NULL) delete stCombined;
            if (stLeft != NULL) delete stLeft;
            if (stRight != NULL) delete stRight;
            return newCh;
        }
    } // onlyTestCh == '7'
    
    if (testAllDigits || (onlyTestCh == '2') || (onlyTestCh2 == '2')) {
        int numAcceptedFailures = ((((results->globalStats.averageWidthDigits.average != 0) && (results->globalStats.averageWidthDigits.average < maxWidthForLeniency)) || lax)? 1:0);
        int numFailures = 0;
        
        //  -------------
        //0|             |
        //1|  11    1    |
        //2|  11    11   |
        //3|        11   |
        //4|        11   |
        //5|        11   |
        //6|        11   |
        //7|       11    |
        //8|       11    |
        //9|      1      |
        //a|             |
        //b|    1        |
        //c|   11        |
        //d|   11        |
        //e|  11         |
        //f|  11    1    |
        //#| 111    11   |
        //#| 111         |
        //#|             |
        //  -------------
        
        //  -----------
        //0|           |
        //1|     1     |
        //2|     111   |
        //3|       11  |
        //4|  11   11  |
        //5|  11   111 |
        //6|       111 |
        //7|       111 |
        //8|       111 |
        //9|       11  |
        //a|       11  |
        //b|       1   |
        //c|           |
        //d|           |
        //e|   11      |
        //f|   1       |
        //#|   1       |
        //#|  11       |
        //#| 111       |
        //#| 111 11    |
        //#| 111 111   |
        //#|           |
        //  -----------
        
        if ((stCombined != NULL) && (cplCombined.size() == 5)) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.20, 0.20);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.20, 0.20);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.20, 0.20);
            CGRect bottomRightRect = getRightmostBottomRect(rect, cplCombined, 0.15);
            if ((topLeftRect.size.height > 0)
                && (bottomRightRect.size.height > 0)
                && (bottomLeftRect.size.height >= rect.size.height * 0.25)
                && (topRightRect.size.height >= rect.size.height * 0.25)
                && (bottomLeftRect.size.height + topRightRect.size.height >= rect.size.height * 0.75)) {
                SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                if ((stTopRight != NULL) && (stBottomLeft != NULL)
                    && (isBottomUpDiagonal(bottomLeftRect, stBottomLeft)
                        || isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Right, SingleLetterTests::Bound, SingleLetterTests::Any)
                        || isCurved(bottomLeftRect, stBottomLeft, SingleLetterTests::Right, SingleLetterTests::Any, SingleLetterTests::Bound))
                    && (isBottomUpDiagonal(topRightRect, stTopRight)
                        || isCurved(topRightRect, stTopRight, SingleLetterTests::Left, SingleLetterTests::Bound, SingleLetterTests::Any)
                        || isCurved(topRightRect, stTopRight, SingleLetterTests::Left, SingleLetterTests::Any, SingleLetterTests::Bound))) {
                    matchingPattern = true;
                }
                if (stTopRight != NULL) delete stTopRight;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '2';
                }
            }
        }
    
        bool doit = true;
        
        if (cplCombined.size() < 4)
            doit = false;
        else {
            // Identify the 4 comps: top-right CC curved right, top-left piece, , bottom-left CC curved left, bottom-right piece
            int topRightCurvedCompIndex = -1, topLeftSmallCompIndex = -1, bottomLeftCurvedCompIndex = -1, bottomRightSmallCompIndex = -1;
            // Order comps from lowest X to highest X and lowest Y to highest Y
            int X1 = -1, X2 = -1, X3 = -1, X4 = -1;
            int Y1 = -1, Y2 = -1, Y3 = -1, Y4 = -1;
            for (int i=1; i<cplCombined.size(); i++) {
                ConnectedComponent cc = cplCombined[i];

                if ((X1 < 0) || (cc.xmax < cplCombined[X1].xmax)) {
                    // Insert at X1
                    X4 = X3; X3 = X2; X2 = X1;
                    X1 = i;
                } else if ((X2 < 0) || (cc.xmax < cplCombined[X2].xmax)) {
                    // Insert at X2
                    X4 = X3; X3 = X2; X2 = i;
                } else if ((X3 < 0) || (cc.xmax < cplCombined[X3].xmax)) {
                    // Insert at X2
                    X4 = X3; X3 = i;
                } else
                    X4 = i;
                
                if ((Y1 < 0) || (cc.ymax < cplCombined[Y1].ymax)) {
                    // Insert at Y1
                    Y4 = Y3; Y3 = Y2; Y2 = Y1;
                    Y1 = i;
                } else if ((Y2 < 0) || (cc.ymax < cplCombined[Y2].ymax)) {
                    // Insert at Y2
                    Y4 = Y3; Y3 = Y2; Y2 = i;
                } else if ((Y3 < 0) || (cc.ymax < cplCombined[Y3].ymax)) {
                    // Insert at X2
                    Y4 = Y3; Y3 = i;
                } else
                    Y4 = i;
            }
            
            // topRightCurvedCompIndex: Y1 or Y2 (lowest Y's), pick the one with the higher X value
            // topLeftSmallCompIndex: Y1 or Y2 (lowest Y's), pick the one with the lower X value
            if (cplCombined[Y1].xmax > cplCombined[Y2].xmax) {
                topRightCurvedCompIndex = Y1;
                topLeftSmallCompIndex = Y2;
            } else {
                topRightCurvedCompIndex = Y2;
                topLeftSmallCompIndex = Y1;
            }
            
            // bottomLeftCurvedCompIndex: Y3 or Y4 (highest Y's), pick the one with the lower X value
            // bottomRightSmallCompIndex: Y3 or Y4 (highest Y's), pick the one with the higher X value
            if (cplCombined[Y3].xmax > cplCombined[Y4].xmax) {
                bottomRightSmallCompIndex = Y3;
                bottomLeftCurvedCompIndex = Y4;
            } else {
                bottomRightSmallCompIndex = Y4;
                bottomLeftCurvedCompIndex = Y3;
            }
            
            // Check we got all four comps and all are different
            if ((topRightCurvedCompIndex >= 0) && (topLeftSmallCompIndex >= 0) && (bottomLeftCurvedCompIndex >= 0) && (bottomRightSmallCompIndex >= 0)
                && (topRightCurvedCompIndex != topLeftSmallCompIndex) && (topRightCurvedCompIndex != bottomRightSmallCompIndex) && (topRightCurvedCompIndex != bottomLeftCurvedCompIndex) && (topLeftSmallCompIndex != bottomRightSmallCompIndex) && (topLeftSmallCompIndex != bottomLeftCurvedCompIndex) && (bottomRightSmallCompIndex != bottomLeftCurvedCompIndex)) {
                ConnectedComponent topRightCurvedCompCC = cplCombined[topRightCurvedCompIndex];
                ConnectedComponent topLeftSmallCompCC = cplCombined[topLeftSmallCompIndex];
                ConnectedComponent bottomLeftCurvedCompCC = cplCombined[bottomLeftCurvedCompIndex];
                ConnectedComponent bottomRightSmallCompCC = cplCombined[bottomRightSmallCompIndex];
                
                        // topRightCurvedComp to the right of topLeftSmallComp
                if (topRightCurvedCompCC.xmin < topLeftSmallCompCC.xmax)
                    doit = false;
                        // bottomRightSmallComp to right of bottomLeftCurvedComp
                else if (bottomRightSmallCompCC.xmin < bottomLeftCurvedCompCC.xmax)
                    doit = false;
                        // topRightCurvedCC close to right side of rect
                else if (topRightCurvedCompCC.xmax < rect.size.width * 0.90)
                    doit = false;
                        // topLeftSmallComp close to left side of rect
                else if (topLeftSmallCompCC.xmin > rect.size.width * 0.10)
                    doit = false;
                        // BottomRightSmallCompCC close to right side of rect (but maybe not flush)
                else if (bottomRightSmallCompCC.xmax < rect.size.width * 0.75)
                    doit = false;
                        // topLeftSmallCompCC close to left side of rect (but maybe not flush)
                else if (topLeftSmallCompCC.xmin > rect.size.width * 0.25)
                    doit = false;
                else if (topLeftSmallCompCC.getHeight() >= rect.size.height * 0.36)
                    doit = false;
                else if (bottomRightSmallCompCC.getHeight() >= rect.size.height * 0.30)
                    doit = false;
       
                
                if (doit) {
                    // Now test indents on topRightCurved and bottomLeftCurved
                    CGRect topRightCurvedRect (rectLeft(rect) + topRightCurvedCompCC.xmin, rectBottom(rect) + topRightCurvedCompCC.ymin, topRightCurvedCompCC.getWidth(), topRightCurvedCompCC.getHeight());
                    SingleLetterTests *stTopRightCurved = CreateSingleLetterTests(topRightCurvedRect, results);
                    CGRect bottomLeftCurvedRect (rectLeft(rect) + bottomLeftCurvedCompCC.xmin, rectBottom(rect) + bottomLeftCurvedCompCC.ymin, bottomLeftCurvedCompCC.getWidth(), bottomLeftCurvedCompCC.getHeight());
                    SingleLetterTests *stBottomLeftCurved = CreateSingleLetterTests(bottomLeftCurvedRect, results);
                    if ((stTopRightCurved != NULL) && (stBottomLeftCurved != NULL)) {
                        OpeningsTestResults res;
                        bool successTopRightCurvedIndent = stTopRightCurved->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,      // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        bool successBottomLeftCurvedIndent = stBottomLeftCurved->getOpenings(res,
                                SingleLetterTests::Right,
                                0.00,      // Start of range to search (top/left)
                                1.00,    // End of range to search (bottom/right)
                                SingleLetterTests::Bound,   // Require start (top/left) bound
                                SingleLetterTests::Bound  // Require end (bottom/right) bound
                                );
                        if (!successBottomLeftCurvedIndent) {
                            numFailures++;
                            if (numFailures > numAcceptedFailures)
                                doit = false;
                        }
                        if (!successTopRightCurvedIndent) {
                            numFailures++;
                            if (numFailures > numAcceptedFailures)
                                doit = false;
                        }
                        delete stTopRightCurved; delete stBottomLeftCurved;
                    }
                }
            } else {
                doit = false;
            }
        }

        if (doit) {
            if (stCombined != NULL) delete stCombined;
            if (stLeft != NULL) delete stLeft;
            if (stRight != NULL) delete stRight;
            return '2';
        }
    } // onlyTestCh == '2'

    
    if (testAllDigits || (onlyTestCh == '5') || (onlyTestCh2 == '5')) {
        bool doit = true;
        bool otherOptions = ((onlyTestCh != '5') && (onlyTestCh != '\0')) || ((onlyTestCh2 != '5') && (onlyTestCh2 != '\0')) || testAllDigits;
        
        //  -----------
        //0|           |
        //1|  11 1111  |
        //2| 111   1   |
        //3| 111       |
        //4| 111       |
        //5| 111       |
        //6| 111       |
        //7|  11       |
        //8| 111    1  |
        //9|  11    11 |
        //a|        11 |
        //b|       111 |
        //c|        11 |
        //d|        1  |
        //e|        11 |
        //f|  1    111 |
        //#|  11   111 |
        //#|  11   11  |
        //#|  11 1 11  |
        //#|           |
        //  -----------
        // Ignoring possible 6th small component middle-bottom (as in the 1-pixel above pattern)
        if ((stCombined != NULL) && ((cplCombined.size() == 5) || (cplCombined.size() == 6))) {
            bool matchingPattern = false;
            CGRect topLeftRect = getTopLeftRect(rect, cplCombined, 0.15, 0.15);
            CGRect bottomLeftRect = getBottomLeftRect(rect, cplCombined, 0.20, 0.20);
            CGRect topRightRect = getTopRightRect(rect, cplCombined, 0.20, 0.20);
            CGRect bottomRightRect = getTallestRightRect(rect, cplCombined, 0.20);
            if ((topRightRect.size.height > 0)
                && (topLeftRect.size.height > rect.size.height * 0.25)
                && (bottomLeftRect.size.height > 0)
                && (bottomRightRect.size.height >= rect.size.height * 0.25)) {
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(bottomLeftRect, results);
                if ((stBottomRight != NULL) && (stBottomLeft != NULL)
                    && isTopDownDiagonal(bottomLeftRect, stBottomLeft, true)
                    && isCurved(bottomRightRect, stBottomRight, SingleLetterTests::Left)) {
                    matchingPattern = true;
                }
                if (stBottomRight != NULL) delete stBottomRight;
                if (stBottomLeft != NULL) delete stBottomLeft;
                if (matchingPattern) {
                    if (stCombined != NULL) delete stCombined;
                    if (stRight != NULL) delete stRight;
                    if (stLeft != NULL) delete stLeft;
                    return '5';
                }
            }
        }
        
        CGRect leftRect, rightRect;
        if (stLeft != NULL) delete stLeft;
        if (stRight != NULL) delete stRight;
        stLeft = stRight = NULL;
        bool rIsOnRightSide = false;
        
        if (singleRect) {
            if (stCombined != NULL) {
                if (cplCombined.size() >= 4) {
                    vector<int> sortedComps =  sortConnectedComponents(cplCombined, CONNECTEDCOMPORDER_XMAX_DESC);
                    if (rectLeft(rect) + cplCombined[sortedComps[0]].xmax > rectRight(r->rect)) {
                        leftRect = r->rect;
                        rightRect = computeRightRect(rect, cplCombined, rectRight(leftRect));
                        rIsOnRightSide = false;
                    } else {
                        rightRect = r->rect;
                        leftRect = computeLeftRect(rect, cplCombined, rectLeft(rightRect));
                        rIsOnRightSide = true;
                    }
                   if ((leftRect.size.width > 0) && (rightRect.size.width > 0)) {
                        stLeft = CreateSingleLetterTests(leftRect, results);
                        stRight = CreateSingleLetterTests(rightRect, results);
                    } else
                        doit = false;
                } else
                    doit = false;
            } else
                doit = false;
        } else {
            if (r->next != NULL) {
                leftRect = r->rect;
                if ((r->next->ch == ' ') && (r->next->next != NULL))
                    rightRect = r->next->next->rect;
                else
                    rightRect = r->next->rect;
                stLeft = CreateSingleLetterTests(leftRect, results);
                stRight = CreateSingleLetterTests(rightRect, results);
            } else
                doit = false;
        }
    
        if ((stLeft == NULL) || (stRight == NULL))
            doit = false;
        
        // Test left half: small bottom-left part (optional) + curved top part
        if (doit) {
            ConnectedComponentList cpl = stLeft->getConnectedComponents();
            if (cpl.size() < 2)
                doit = false;
            else {
                ConnectedComponent topCurvedClosingCC = cpl[1];
                if (cpl.size() >= 3) {
                    // Check small comp at the bottom
                    ConnectedComponent smallBottomCC = cpl[2];
                    float bottomYAdjusted = rectBottom(leftRect) + smallBottomCC.ymin;
                    float topYAdjusted = rectBottom(leftRect) + smallBottomCC.ymax;
                    if ((bottomYAdjusted < rectBottom(rect) + rect.size.height * 0.50)
                        || (topYAdjusted < rectBottom(rect) + rect.size.height * 0.50)) {
                        doit = false;
                    }
                }
                if (doit) {
                    float bottomYAdjusted = rectBottom(r->rect) + topCurvedClosingCC.ymin;
                    float topYAdjusted = rectBottom(r->rect) + topCurvedClosingCC.ymax;
                        // Not tall enough?
                    if ((topYAdjusted < rectBottom(rect) + rect.size.height * 0.30)
                        // Starts too low (supposed to hug the top)?
                        || (bottomYAdjusted > rectBottom(rect) + rect.size.height * 0.15)) {
                        doit = false;
                    }
                }
                // Test opening of curved part
                if (doit) {
                    CGRect curvedCC (rectLeft(leftRect) + topCurvedClosingCC.xmin, rectBottom(r->rect) + topCurvedClosingCC.ymin, topCurvedClosingCC.getWidth(), topCurvedClosingCC.getHeight());
                    SingleLetterTests *stCurved = CreateSingleLetterTests(curvedCC, results);
                    if (stCurved == NULL)
                        doit = false;
                    else {
                        OpeningsTestResults res;
                        bool success = stCurved->getOpenings(res,
                            SingleLetterTests::Right,
                            0.00,      // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (!success) {
                            doit = false;
                        }
                    }
                }
            } // correct connected comps
        } // digit 5
        
       // Test right half: small top-right part (optional) + curved bottom part
        if (doit) {
            ConnectedComponentList cpl = stRight->getConnectedComponents();
            if (cpl.size() < 2)
                doit = false;
            else {
                ConnectedComponent bottomCurvedClosingCC = cpl[1];
                if (cpl.size() >= 3) {
                    // Check small comp at the top
                    ConnectedComponent smallTopCC = cpl[2];
                    float bottomYAdjusted = rectBottom(rightRect) + smallTopCC.ymin;
                    float topYAdjusted = rectBottom(rightRect) + smallTopCC.ymax;
                    if ((bottomYAdjusted > rectBottom(rect) + rect.size.height * 0.15)
                        || (topYAdjusted > rectBottom(rect) + rect.size.height * 0.50)) {
                        doit = false;
                    }
                }
                if (doit) {
                    float bottomYAdjusted = rectBottom(rightRect) + bottomCurvedClosingCC.ymin;
                    float topYAdjusted = rectBottom(rightRect) + bottomCurvedClosingCC.ymax;
                        // Starts too high?
                    if ((bottomYAdjusted < rectBottom(rect) + rect.size.height * 0.20)
                        // Ends too high (supposed to hug the bottom)?
                        || (topYAdjusted < rectBottom(rect) + rect.size.height * 0.85)) {
                        doit = false;
                    }
                }
                // Test opening of curved part
                if (doit) {
                    CGRect curvedCC (rectLeft(rightRect) + bottomCurvedClosingCC.xmin, rectBottom(rightRect) + bottomCurvedClosingCC.ymin, bottomCurvedClosingCC.getWidth(), bottomCurvedClosingCC.getHeight());
                    SingleLetterTests *stCurved = CreateSingleLetterTests(curvedCC, results);
                    if (stCurved == NULL)
                        doit = false;
                    else {
                        OpeningsTestResults res;
                        bool success = stCurved->getOpenings(res,
                            SingleLetterTests::Left,
                            0.00,      // Start of range to search (top/left)
                            1.00,    // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        if (!success) {
                            doit = false;
                        }
                        delete stCurved;
                    }
                }
            } // correct connected comps
        } // digit 5
        
        if (doit || !otherOptions) {
            if (stRight != NULL) delete stRight;
            if (stLeft != NULL) delete stLeft;
            if (stCombined != NULL) delete stCombined;
            if (doit)
                return '5';
            else if (!otherOptions)
                return '\0';
        }
    } // onlyTestCh == '5'
    
    if ((testAllDigits || (onlyTestCh == '3') || (onlyTestCh2 == '3')) && (cplCombined.size() > 1)) {
        /* Check for '3':
         - one tall main CC with indentations on the left side top/bottom
         - if additional comps, expect only small top-left and bottom-left comps
         */
        bool doit = true;
        bool mainCCHasTopLeftIndentation = false, mainCCTestedTopLeftIndentation = false;
        bool mainCCHasBottomLeftIndentation = false, mainCCTestedBottomLeftIndentation = false;
        bool mainCCHasMiddleRightIndentation = false, mainCCTestedMiddleRightIndentation = false;
        int middleRightIndentationDepth = 0;
        bool foundTopLeftSmallComp = false;
        bool foundBottomLeftSmallComp = false;
        ConnectedComponent mainCC = cplCombined[1];
        CGRect mainRect(rectLeft(rect) + mainCC.xmin, rectBottom(rect) + mainCC.ymin, mainCC.getWidth(), mainCC.getHeight());
        if (stMain == NULL)
            stMain = CreateSingleLetterTests(mainRect, results);
        if (stMain == NULL)
            doit = false;
        else {
            // 2016-04-11 we were ignoring top/bottom left idents, now requiring both. We may decide to relax this to require only one indent
            // Test top-right indentation from left side
            CGRect topRightRect (rectLeft(mainRect), rectBottom(mainRect), mainRect.size.width, mainRect.size.height * 0.70);
            SingleLetterTests *stTopRight = CreateSingleLetterTests(topRightRect, results, false, 0, 0.03, true);
            if (stTopRight != NULL) {
                OpeningsTestResults resLeft;
                bool successTopLeft = stTopRight->getOpenings(resLeft,
                    SingleLetterTests::Left,
                    0.00,      // Start of range to search (top/left)
                    1.00,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                    mainCCTestedTopLeftIndentation = true;
                if (!successTopLeft) {
                    mainCCHasTopLeftIndentation = false;
                    doit = false;
                } else
                    mainCCHasTopLeftIndentation = true;
                delete stTopRight;
            } else {
                doit = false;
            }
            if (doit) {
                CGRect bottomRightRect (rectLeft(mainRect), rectBottom(mainRect) + mainRect.size.height * 0.30, mainRect.size.width, mainRect.size.height * 0.70);
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(bottomRightRect, results, false, 0, 0.03, true);
                if (stBottomRight != NULL) {
                    OpeningsTestResults resLeft;
                    bool successBottomLeft = stMain->getOpenings(resLeft,
                        SingleLetterTests::Left,
                        0.00,      // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                    mainCCTestedBottomLeftIndentation = true;
                    if (!successBottomLeft) {
                        mainCCHasBottomLeftIndentation = false;
                        doit = false;
                    } else
                        mainCCHasBottomLeftIndentation = true;
                    delete stBottomRight;
                } else {
                    doit = false;
                }
            }
            if (doit) {
                OpeningsTestResults resRight;
                bool successMiddleRight = stMain->getOpenings(resRight,
                    SingleLetterTests::Right,
                    0.30,      // Start of range to search (top/left)
                    0.70,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound,  // Require end (bottom/right) bound
                    true,
                    true // Require bounds of the opening to be in the specified range
                    );
                mainCCTestedMiddleRightIndentation = true;
                if (!successMiddleRight) {
                    mainCCHasMiddleRightIndentation = false;
                    doit = false;
                } else {
                    mainCCHasMiddleRightIndentation = true;
                    middleRightIndentationDepth = resRight.maxDepth;
                }
            }
            // TODO test mainCCHasTopLeftIndentation + mainCCHasBottomLeftIndentation
        } // Got stMain
        if (doit) {
            // Now test other comps
            CGRect topLeftRect, bottomLeftRect;
            for (int i=2; i<cplCombined.size(); i++) {
                ConnectedComponent cc = cplCombined[i];
                if (cc.xmax <= mainCC.xmin) {
                    if (cc.ymax < mainCC.ymin + mainCC.getHeight() * 0.35) {
                        foundTopLeftSmallComp = true;
                        topLeftRect = CGRect (rectLeft(rect) + cc.xmin, rectBottom(rect) + cc.ymin, cc.getWidth(), cc.getHeight());
                    } else if (cc.ymin > mainCC.ymin + mainCC.getHeight() * 0.65) {
                        foundBottomLeftSmallComp = true;
                        bottomLeftRect = CGRect (rectLeft(rect) + cc.xmin, rectBottom(rect) + cc.ymin, cc.getWidth(), cc.getHeight());
                    } else {
                        // Comp in 35% - 65% range, abort
                        doit = false;
                    }
                } else {
                    // Non-main CC extending into mainCC - not a vertically disconnected case, abort
                    doit = false;
                    break;
                }
            }
            if (doit && foundBottomLeftSmallComp && foundTopLeftSmallComp) {
                // Either we found indentations top-right and bottom-right earlier, or we need to take a closer look at the small comp top/bottom
                if (!(mainCCHasTopLeftIndentation && mainCCHasBottomLeftIndentation)) {
                    doit = false;
                    SingleLetterTests *stTopLeftSmallComp = CreateSingleLetterTests(topLeftRect, results);
                    SingleLetterTests *stBottomLeftSmallComp = CreateSingleLetterTests(bottomLeftRect, results);
                    if ((stTopLeftSmallComp != NULL) && (stBottomLeftSmallComp != NULL)) {
                        SegmentList slTopLeftSmallComp10 = stTopLeftSmallComp->getHorizontalSegments(0.05, 0.10);
                        SegmentList slTopLeftSmallComp90 = stTopLeftSmallComp->getHorizontalSegments(0.95, 0.10);
                        SegmentList slBottomLeftSmallComp10 = stBottomLeftSmallComp->getHorizontalSegments(0.05, 0.10);
                        SegmentList slBottomLeftSmallComp90 = stBottomLeftSmallComp->getHorizontalSegments(0.95, 0.10);
                        if ((slTopLeftSmallComp10.size() == 1) && (slTopLeftSmallComp90.size() == 1) && (slBottomLeftSmallComp10.size() == 1) && (slBottomLeftSmallComp90.size() == 1)) {
                            //  -----------
                            //0|           |
                            //1|  11   1   |
                            //2| 111   11  |
                            //3| 111   111 |
                            //4| 111   111 |
                            //5| 11    111 |
                            //6|       111 |
                            //7|       111 |
                            //8|      111  |
                            //9|      111  |
                            //a|      11   |
                            //b|      111  |
                            //c|       111 |
                            //d|       111 |
                            //e|       111 |
                            //f| 1     111 |
                            //#| 11    111 |
                            //#| 111   111 |
                            //#| 111  1111 |
                            //#| 111  111  |
                            //#|  1    11  |
                            //#|           |
                            //  -----------
                            // OK, so now test that the top-left element is slanted top-right to bottom-left and bottom-left element is slanted top-left tp bottom-right
                            if ((slTopLeftSmallComp10[0].startPos > slTopLeftSmallComp90[0].startPos)
                                && (slTopLeftSmallComp10[0].endPos > slTopLeftSmallComp90[0].endPos)
                                && (slBottomLeftSmallComp10[0].startPos < slBottomLeftSmallComp90[0].startPos)
                                && (slBottomLeftSmallComp10[0].endPos < slBottomLeftSmallComp90[0].endPos)) {
                                doit = true;
                            }
                        } // got horizontal segments
                        delete stTopLeftSmallComp; delete stBottomLeftSmallComp;
                    } // got singleLetters for small comps
                } // could be trouble, not indentations on right side on left
            }
        }
        
        if (doit) {
            resCh = '3';
            if (stCombined != NULL) delete stCombined;
            if (stMain != NULL) delete stMain;
            return resCh;
        }
    } // test for '3'
    
    
    if (testAllDigits || (onlyTestCh == '0') || (onlyTestCh2 == '0')) {
    
        // TODO recognize the below as '0', see https://drive.google.com/open?id=0B4jSQhcYsC9VSzFfS3hBRk1NR0U in word [0710-1I1O87], testing "I1"
        //  0123456789abcde
        //  ---------------
        //0|               |
        //1|   11      1   |
        //2|   11      1   |
        //3|   1        1  |
        //4|   1        1  |
        //5|  11       111 |
        //6|  11       11  |
        //7|  1         1  |
        //8|  1         1  |
        //9|  1        111 |
        //a|  1         1  |
        //b|  11        1  |
        //c|  1         1  |
        //d| 11        11  |
        //e| 11        111 |
        //f|  11       11  |
        //#|  11      11   |
        //#|  11      11   |
        //#|   111  1111   |
        //#|    11  11     |
        //#|               |
        //  ---------------
    
        //  ---------------
        //0|               |
        //1|     111111    |
        //2|    1111111    |
        //3|   111   111   |
        //4|  111     111  |
        //5|  11      111  |
        //6|  11       11  |
        //7| 111       111 |
        //8| 111       11  |
        //9| 111       11  |
        //a| 111       11  |
        //b| 111       11  |
        //c| 111       11  |
        //d| 111       11  |
        //e| 111       11  |
        //f| 111       11  |
        //#|  111     111  |
        //#|  111     111  |
        //#|  1111111111   |
        //#|   11111111    |
        //#|    11  11     |
        //#|               |
        //  ---------------
        if ((stCombined != NULL) && (cplCombined.size() == 2)) {
            bool matchingPattern = false;
            // Check one large inverted comp
            ConnectedComponentList cplInverted = stCombined->getInverseConnectedComponents();
            if ((cplInverted.size() == 2) && (cplInverted[1].getHeight() > rect.size.height * 0.60) && (cplInverted[1].getWidth() > rect.size.width * 0.30)) {
                // Check narrow/centered top/bottom (compared to mid-section
                SegmentList slTop = stCombined->getHorizontalSegments(0.075, 0.15);
                SegmentList slMiddle = stCombined->getHorizontalSegments(0.50, 0.10);
                SegmentList slBottom = stCombined->getHorizontalSegments(0.925, 0.15);
                if ((slTop.size() == 1) && (slMiddle.size() == 2) && (slBottom.size() == 1)) {
                    float leftGapTop = slTop[0].startPos;
                    float rightGapTop = rect.size.width - 1 - slTop[0].endPos;
                    float leftGapCenter = slMiddle[0].startPos;
                    float rightGapCenter = rect.size.width - 1 - slMiddle[1].endPos;
                    float leftGapBottom = slBottom[0].startPos;
                    float rightGapBottom = rect.size.width - 1 - slBottom[0].endPos;
                    if ((leftGapTop >= leftGapCenter) && (rightGapTop >= rightGapCenter) && (leftGapBottom >= leftGapCenter) && (rightGapBottom >= rightGapCenter))
                        matchingPattern = true;
                }
            }

            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '0';
            }
        } // matches num of connected comps
        
        
        //  -----------
        //0|           |
        //1|   11 11   |
        //2|   11  1   |
        //3|   1    1  |
        //4|  11    11 |
        //5|  11    11 |
        //6|  11    11 |
        //7|  11    11 |
        //8|  11    1  |
        //9| 111    11 |
        //a| 111    11 |
        //b| 111    11 |
        //c| 111    11 |
        //d|  11    11 |
        //e|  11    11 |
        //f| 11     11 |
        //#|  11   111 |
        //#|  11   11  |
        //#|  11  111  |
        //#|      11   |
        //#|           |
        //  -----------
        
        //  ------------
        //0|            |
        //1|    11      |
        //2|   1111     |
        //3|   111      |
        //4|  111       |
        //5|  111     1 |
        //6| 111     11 |
        //7| 111     11 |
        //8| 111     11 |
        //9| 111     11 |
        //a| 111     11 |
        //b| 111     11 |
        //c| 111     11 |
        //d| 111     11 |
        //e| 111     11 |
        //f| 111     11 |
        //#| 111     11 |
        //#| 111     11 |
        //#| 111     11 |
        //#| 111     11 |
        //#|  111    11 |
        //#|  1111      |
        //#|  1111      |
        //#|   111      |
        //#|    1       |
        //#|            |
        //  ------------
        
        //  --------------
        //0|              |
        //1|      111     |
        //2|    111111    |
        //3|   11111111   |
        //4|   11    11   |
        //5|  1           |
        //6|  1           |
        //7|  1         1 |
        //8| 11        11 |
        //9| 11        11 |
        //a| 11        11 |
        //b| 11        11 |
        //c| 11        11 |
        //d| 11        11 |
        //e| 11        11 |
        //f| 11        11 |
        //#|  1           |
        //#|   1     11   |
        //#|   11111111   |
        //#|    111111    |
        //#|              |
        //  --------------
        // In the above case be lenient with right side (require 40% of height) as long as left side is curved top & bottom
        if ((stCombined != NULL) && (cplCombined.size() == 3)) {
            bool matchingPattern = false, abort = false;
            CGRect leftSideRect = getTallestLeftRect(rect, cplCombined, 0.10);
            CGRect rightSideRect = getTallestRightRect(rect, cplCombined, 0.10);
            SingleLetterTests *stRightSide = NULL; SingleLetterTests *stLeftSide = NULL;
            if ((rightSideRect.size.height > rect.size.height * 0.80)
                && (leftSideRect.size.height > rect.size.height * 0.80)) {
                if (stRightSide == NULL)
                    stRightSide = CreateSingleLetterTests(rightSideRect, results);
                if (stRightSide != NULL) {
                    if (isCurved(rightSideRect, stRightSide, SingleLetterTests::Left))
                        matchingPattern = true;
                    else {
                        if (stLeftSide == NULL)
                            stLeftSide = CreateSingleLetterTests(leftSideRect, results);
                        if (stLeftSide != NULL) {
                            if (isCurved(leftSideRect, stLeftSide, SingleLetterTests::Right))
                                matchingPattern = true;
                        } else
                            abort = true;
                    }
                } else
                    abort = true;
            }
            // Now test if either right or left is 80% of height, if so expect that side to be curved but accept shorter pattern on other side
            else if ((rightSideRect.size.height > rect.size.height * 0.80)
                || (leftSideRect.size.height > rect.size.height * 0.80)) {
                bool leftSideTall = (leftSideRect.size.height > rect.size.height * 0.80);
                if (leftSideTall) {
                    bool rightSideChecksOut = (rightSideRect.size.height > rect.size.height * 0.60);
                    if (rightSideChecksOut) {
                        // Just requires that left side be curved
                        if (stLeftSide == NULL)
                            stLeftSide = CreateSingleLetterTests(leftSideRect, results);
                        if (stLeftSide != NULL) {
                            if (isCurved(leftSideRect, stLeftSide, SingleLetterTests::Right))
                                matchingPattern = true;
                        } else
                            abort = true;
                    }
                    else if (rightSideRect.size.height > rect.size.height * 0.40) {
                        // Give this shorter right side a chance if top/bottom are curved
                        CGRect topLeftCornerRect (rectLeft(leftSideRect), rectBottom(leftSideRect), leftSideRect.size.width, leftSideRect.size.height * 0.25);
                        CGRect bottomLeftCornerRect (rectLeft(leftSideRect), rectBottom(leftSideRect) + leftSideRect.size.height * 0.75, leftSideRect.size.width, leftSideRect.size.height * 0.25);
                        SingleLetterTests *stTopLeftCorner = CreateSingleLetterTests(topLeftCornerRect, results, false, 0, 0.03, true);
                        SingleLetterTests *stBottomLeftCorner = CreateSingleLetterTests(bottomLeftCornerRect, results, false, 0, 0.03, true);
                        if ((stTopLeftCorner != NULL) && (stBottomLeftCorner != NULL)
                            && isCurved(topLeftCornerRect, stTopLeftCorner, SingleLetterTests::Bottom)
                            && isCurved(bottomLeftCornerRect, stBottomLeftCorner, SingleLetterTests::Top)) {
                            matchingPattern = true;
                            delete stTopLeftCorner; delete stBottomLeftCorner;
                        }
                    }
                } else {
                    bool leftSideChecksOut = (leftSideRect.size.height > rect.size.height * 0.60);
                    if (leftSideChecksOut) {
                        // Just requires that right side be curved
                        if (stRightSide == NULL)
                            stRightSide = CreateSingleLetterTests(rightSideRect, results);
                        if (stRightSide != NULL) {
                            if (isCurved(rightSideRect, stRightSide, SingleLetterTests::Left))
                                matchingPattern = true;
                        } else
                            abort = true;
                    }
                    else if (leftSideRect.size.height > rect.size.height * 0.40) {
                        // Give this shorter left side a chance if on right side top/bottom are curved
                        CGRect topRightCornerRect (rectLeft(rightSideRect), rectBottom(rightSideRect), rightSideRect.size.width, rightSideRect.size.height * 0.25);
                        CGRect bottomRightCornerRect (rectLeft(rightSideRect), rectBottom(rightSideRect) + rightSideRect.size.height * 0.75, rightSideRect.size.width, rightSideRect.size.height * 0.25);
                        SingleLetterTests *stTopRightCorner = CreateSingleLetterTests(topRightCornerRect, results, false, 0, 0.03, true);
                        SingleLetterTests *stBottomRightCorner = CreateSingleLetterTests(bottomRightCornerRect, results, false, 0, 0.03, true);
                        if ((stTopRightCorner != NULL) && (stBottomRightCorner != NULL)
                            && isCurved(topRightCornerRect, stTopRightCorner, SingleLetterTests::Bottom)
                            && isCurved(bottomRightCornerRect, stBottomRightCorner, SingleLetterTests::Top)) {
                            matchingPattern = true;
                            delete stTopRightCorner; delete stBottomRightCorner;
                        }
                    }
                }
            }
            
            //  ------------
            //0|            |
            //1|  11     11 |
            //2| 111    111 |
            //3| 111    11  |
            //4| 111   111  |
            //5|  11    11  |
            //6|  11    111 |
            //7|  11     11 |
            //8|  11    111 |
            //9|  11    11  |
            //a|  11    11  |
            //b|  11   111  |
            //c|  11    111 |
            //d| 1111   111 |
            //e|  1111   11 |
            //f|   1        |
            //#|            |
            //  ------------
            // One last check: rule out above pattern with very wide gaps top or bottom
            if (matchingPattern) {
                SegmentList slTop = stCombined->getHorizontalSegments(0.075, 0.15);
                SegmentList slBottom = stCombined->getHorizontalSegments(0.925, 0.15);
                int maxGap = 0;
                for (int i=0; i<(int)slTop.size() - 1; i++) {
                    int gap = slTop[i+1].startPos - slTop[i].endPos - 1;
                    if (gap > maxGap)
                        maxGap = gap;
                }
                for (int i=0; i<(int)slBottom.size() - 1; i++) {
                    int gap = slBottom[i+1].startPos - slBottom[i].endPos - 1;
                    if (gap > maxGap)
                        maxGap = gap;
                }
                if (maxGap > rect.size.width * 0.40) {
                    DebugLog("testAsDisconnectedDigit: aborting '0' test due to large gap top or bottom in word [%s]", toUTF8(r->word->text()).c_str());
                    matchingPattern = false;
                }
            }
            
            if (stRightSide != NULL) delete stRightSide;
            if (stLeftSide != NULL) delete stLeftSide;

            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '0';
            }
        } // matches num of connected comps
        
        //  -----------
        //0|           |
        //1|    11111  |
        //2|   111  1  |
        //3|   11      |
        //4|  11       |
        //5|  11       |
        //6| 111      1|
        //7|  1       1|
        //8|  1       1|
        //9| 11       1|
        //a| 11       1|
        //b| 11       1|
        //c| 11       1|
        //d|  1       1|
        //e|          1|
        //f|           |
        //#|    1111   |
        //#|    11111  |
        //#|     111   |
        //#|           |
        //  -----------
        if (forceDigit && (stCombined != NULL) && (cplCombined.size() == 4)) {
            bool matchingPattern = false;
            CGRect topLeftSideRect = getTallestLeftRect(rect, cplCombined, 0.10);
            CGRect rightSideRect = getTallestRightRect(rect, cplCombined, 0.10);
            CGRect bottomRect = getBottomRect(rect, cplCombined, 0.15);
            //SingleLetterTests *stRightSide = NULL;
            SingleLetterTests *stTopLeftSide = NULL;
            if ((rightSideRect.size.height > rect.size.height * 0.50)
                && (topLeftSideRect.size.height > rect.size.height * 0.60)
                && (bottomRect.size.height > 0)) {
                stTopLeftSide = CreateSingleLetterTests(topLeftSideRect, results);
                //PQ911
                if (stTopLeftSide != NULL) {
                    OpeningsTestResults res;
                    bool success = stTopLeftSide->getOpenings(res,
                        SingleLetterTests::Left,
                        0.00,    // Start of range to search (top/left)
                        1.00,    // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Any      // Require end (bottom/right) bound
                        );
                    if (success)
                        matchingPattern = true;
                }
            }
            
            if (stTopLeftSide != NULL) delete stTopLeftSide;

            if (matchingPattern) {
                if (stCombined != NULL) delete stCombined;
                if (stRight != NULL) delete stRight;
                if (stLeft != NULL) delete stLeft;
                return '0';
            }
        } // matches num of connected comps
    }
    
    //  --------------
    //0|              |
    //1|    1  111    |
    //2|   111   11   |
    //3|   11    11   |
    //4|   1      1   |
    //5|          11  |
    //6|          11  |
    //7|  1        1  |
    //8|  1           |
    //9| 11        1  |
    //a| 111      11  |
    //b|  11      11  |
    //c|           11 |
    //d|           1  |
    //e|  1        11 |
    //f|  11       1  |
    //#|  111     11  |
    //#|   11     1   |
    //#|      1111    |
    //#|      1111    |
    //#|              |
    //  --------------
    // h = 19, combined left comps h = 13, gap = 5 or 6 out of width 11
    if (cplCombined.size() == 6) {
        bool matchingPattern = false;
        
        CGRect topLeftRec = getHighestLeftRect(rect, cplCombined, 0.25);
        CGRect middleLeftRec = getClosestRect(rect, 0.15, 0.50, cplCombined, 0.15, 0.20);
        CGRect bottomLeftRec = getLowestLeftRect(rect, cplCombined, 0.25);
        CGRect topRightRec = getHighestRightRect(rect, cplCombined, 0.25);
        CGRect bottomRightRec = getLowestRightRect(rect, cplCombined, 0.25);
        if ((topLeftRec.size.height > 0)
            && (middleLeftRec.size.height > 0)
            && (bottomLeftRec.size.height > 0)
            && (topRightRec.size.height > 0)
            && (bottomRightRec.size.height > 0)
            && (topLeftRec.size.height + middleLeftRec.size.height + bottomLeftRec.size.height > rect.size.height * 0.60)
            && (topRightRec.size.height + bottomRightRec.size.height > rect.size.height * 0.60)) {
                // Check for wide gap in center section
                SegmentList slMiddle = stCombined->getHorizontalSegments(0.50, 0.60);
            if (slMiddle.size() == 2) {
                float gapMiddle = slMiddle[1].startPos - slMiddle[0].endPos - 1;
                if (gapMiddle > rect.size.width * 0.40)
                    matchingPattern = true;
            }
        }
        if (matchingPattern) {
            if (stCombined != NULL) delete stCombined;
            if (stRight != NULL) delete stRight;
            if (stLeft != NULL) delete stLeft;
            return '0';
        }
    }
    
    
    if (stCombined != NULL) delete stCombined;
    if (stMain != NULL) delete stMain;
    
    return '\0';
} // testAsDisconnectedDigit

float combinedLengths(SegmentList sl, float minV, float maxV) {
    float res = 0;
    for (int i=0; i<(int)sl.size(); i++) {
        Segment s = sl[i];
        float vStart = s.startPos;
        float vEnd = s.endPos;
        if ((minV >=0 ) && (vEnd < minV))
            continue; // This segment is entirely left of allowed range
        else if (minV >= 0) {
            if (vStart < minV)
                vStart = minV;
        }
        if ((maxV >=0 ) && (vStart > maxV))
            continue; // This segment is entirely right of allowed range
        else if (maxV >= 0) {
            if (vEnd > maxV)
                vEnd = maxV;
        }
        res += vEnd - vStart + 1;
    }
    return res;
}

// Tests a pattern for possible L, taking into account that the given rect may include a '.' after the 'L'
// Limited - just tests the bottom part for now, checking it's wide enough and that the pattern bottom-right is not far enough that it could be a dot
bool testAsL (OCRRectPtr r, CGRect rect, OCRResults *results, bool singleRect) {
    if (!results->imageTests) return false;
    
    //  ------------
    //0|            |
    //1| 1          |
    //2| 11         |
    //3| 11         |
    //4| 11         |
    //5| 11         |
    //6| 11         |
    //7| 11         |
    //8| 11         |
    //9| 11         |
    //a| 11         |
    //b| 11         |
    //c| 11         |
    //d| 11         |
    //e| 11         |
    //f| 11         |
    //#| 11         |
    //#| 11         |
    //#| 11         |
    //#| 111        |
    //#| 11111 111  |
    //#| 111 1 111  |
    //#|  1         |
    //#|            |
    //  ------------
    
    // Not too narrow? Had to use 80% because of the above with width = 9 while av. digit width = 10.65 (85% = 9.05). Don't know why these L's got to be so narrow, see https://drive.google.com/open?id=0B4jSQhcYsC9VODZYYk5XbjFvQXM
    //  ------------
    //0|            |
    //1| 11         |
    //2| 11         |
    //3| 11         |
    //4| 11         |
    //5| 111        |
    //6| 111        |
    //7| 111        |
    //8| 111        |
    //9| 111        |
    //a| 111        |
    //b| 111        |
    //c| 111        |
    //d| 111        |
    //e| 111        |
    //f|  11        |
    //#|   1        |
    //#|  11        |
    //#|  11        |
    //#|  11        |
    //#|  11        |
    //#|  111   111 |
    //#|  11111 111 |
    //#|  111       |
    //#|            |
    //  ------------
    // And this one came up at less than 80% but has two rects so we need to make an effort to absorb the dot into a 'L' (rather than just throw the dot out later)
    if ((rect.size.width < results->globalStats.averageWidthDigits.average * 0.80)
        && (singleRect || (rect.size.width < results->globalStats.averageWidthDigits.average * 0.75)))
        return false;
    
    bool doit = false;
    
    //  -----------
    //0|           |
    //1| 11        |
    //2| 11        |
    //3| 11        |
    //4| 11        |
    //5| 11        |
    //6| 11        |
    //7| 11        |
    //8| 11        |
    //9| 11        |
    //a| 11        |
    //b| 111       |
    //c| 11        |
    //d| 11        |
    //e| 11        |
    //f| 11        |
    //#| 11        |
    //#| 11        |
    //#| 11        |
    //#| 11111     |
    //#| 11111 1   |
    //#| 11111 1 1 |
    //#|           |
    //  -----------
    
    SingleLetterTests *st = CreateSingleLetterTests(rect, results, false, 0, 0.01, true); // Accept even small connected comps, to accept the above bottom part
    if (st != NULL) {
        SegmentList slBottom = st->getHorizontalSegments(0.925, 0.15);
        float combinedLength = combinedLengths(slBottom);
        float gap = largestGap(slBottom);
            // Testing against rect width in case average digit width is a bit wider (e.g. 10.65) and out rect is narrower (e.g. 9): a base of say 7 pixels is still OK (77% of actual rect) even though it's less than 70% of digit width)
        if ((combinedLength >= rect.size.width * 0.70)
            && (gap < results->retailerParams.minSpaceAroundDotsAsPercentOfWidth * results->globalStats.averageWidthDigits.average * 0.85)) {
            doit = true;
        }
        delete st;
    }
    
    return doit;
}

// Returns a score for a letter as a ':'. Called with all narrow & tall letters.
float SingleLetterTestAsColumn(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, CGRect &newRect, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_COLUMN))
        return -1.0;
    // Don't replace if glued to next/previous!
    if ((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0))
        return -1.0;
    // For next, test only if rect is the same as r->rect (otherwise we are probably testing multiple chars and then OK if glued)
    if ((rect.size.width == r->rect.size.width) && (r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0))
        return -1.0;    
    r->flags6 |= FLAGS6_TESTED_AS_COLUMN; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    wchar_t repCh = '\0';
    float resScore = -1.0; 
    newRect = r->rect;
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    bool got1Dot = false;
    ConnectedComponent letterCC;
    CGRect letterRect;
    if (cpl.size() == 3) {
        ConnectedComponent dot1 = cpl[1];
        CGRect dot1Rect (rectLeft(r->rect) + dot1.xmin, rectBottom(r->rect) + dot1.ymin, dot1.getWidth(), dot1.getHeight());
        float dot1Valid = true;
        ConnectedComponent dot2 = cpl[2];
        CGRect dot2Rect (rectLeft(r->rect) + dot2.xmin, rectBottom(r->rect) + dot2.ymin, dot1.getWidth(), dot2.getHeight());
        float dot2Valid = true;
        got1Dot = false;
        if ((r->previous != NULL) && isLetterOrDigit(r->previous->ch)) {
            // Far above?
            if (rectBottom(r->previous->rect) - rectTop(dot1Rect)  > r->previous->rect.size.height * 0.50)
                dot1Valid = false;
            // Far below
            else if (rectBottom(dot1Rect) - rectTop(r->previous->rect) > r->previous->rect.size.height * 0.50)
                dot1Valid = false;
            if (!dot1Valid) {
                got1Dot = true;
                letterCC = dot2;
                letterRect = dot2Rect;
            } else {
                // Far above?
                if (rectBottom(r->previous->rect) - rectTop(dot2Rect) > r->previous->rect.size.height * 0.50)
                    dot2Valid = false;
                // Far below?
                else if (rectBottom(dot2Rect) - rectTop(r->previous->rect) > r->previous->rect.size.height * 0.50)
                    dot2Valid = false;
                if (!dot2Valid) {
                    got1Dot = true;
                    letterCC = dot1;
                    letterRect = dot1Rect;
                }
            }
        }
        if (!got1Dot && (r->next != NULL) && isLetterOrDigit(r->next->ch)) {
            // Far above?
            if (rectBottom(r->next->rect) - rectTop(dot1Rect) > r->next->rect.size.height * 0.50)
                dot1Valid = false;
            // Far below
            else if (rectBottom(dot1Rect) - rectTop(r->next->rect) > r->next->rect.size.height * 0.50)
                dot1Valid = false;
            if (!dot1Valid) {
                got1Dot = true;
                letterCC = dot2;
            } else {
                // Far above?
                if (rectBottom(r->next->rect) - rectTop(dot2Rect) > r->next->rect.size.height * 0.50)
                    dot2Valid = false;
                else if (rectBottom(dot2Rect) - rectTop(r->next->rect) > r->next->rect.size.height * 0.50)
                    dot2Valid = false;
                if (!dot2Valid) {
                    got1Dot = true;
                    letterCC = dot1;
                }
            }
        }
    }
    // Must have 2 connected components
    if ((cpl.size() == 2) || got1Dot) {
        // Could it be a '.' or '-'?
        if (!got1Dot)
            letterCC = cpl[1];
        if ( ((r->previous == NULL) || !isLetter(r->previous->ch) || (letterCC.getHeight() < OCR_RATIO_DOT_HEIGHT_TO_NORMAL_LETTER_MAX * r->previous->rect.size.height))
            && ((r->next == NULL) || !isLetter(r->next->ch) || (letterCC.getHeight() < OCR_RATIO_DOT_HEIGHT_TO_NORMAL_LETTER_MAX * r->next->rect.size.height))
            && ( ((letterCC.getHeight() < letterCC.getWidth() * OCR_MAX_H_TO_W_RATIO_DOT)
                   && (letterCC.getWidth() < letterCC.getHeight() * OCR_MAX_H_TO_W_RATIO_DOT))
                 || (letterCC.getWidth() > letterCC.getHeight() * 2)
               )
           ) {
            repCh = '.';
            newRect = letterRect;
            resScore = 0.75;
            if (letterCC.getWidth() > letterCC.getHeight() * 2)
                repCh = '-';
            else {
                if (gapAboveBaselinePercent(r, letterCC, true, false) > 0.25)
                    repCh = '-';
            }
            newChar = repCh;
        }
        if (needToRelease)
            delete st;
        return resScore;
    }
    
    if (cpl.size() < 3) {
        if (needToRelease)
            delete st;
        return resScore;
    }
    
    // Require above each other
    SingleLetterPrint(cpl, rect);
    ConnectedComponent large = cpl[1];
    ConnectedComponent small = cpl[2];
    ConnectedComponent top = ((large.ymin < small.ymin)? large:small);
    ConnectedComponent bottom = ((large.ymin < small.ymin)? small:large);
    
    if (top.ymax >= bottom.ymin) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    // 0.20: similar size (25% away)
    // 0.10: both dot not too wide or too tall (at most 1.9x)
    // 0.40: above each other with at least 15% of height between them
    // 0.30: x overlap
    
    if ((large.getWidth() < large.getHeight() * 1.9)
        && (small.getWidth() < large.getHeight() * 1.9)
        && (small.getHeight() < small.getWidth() * 1.9)
        && (large.getHeight() < large.getWidth() * 1.9))
    {
        resScore += 0.10;
    }
    
    if (large.area < small.area * 1.25) {
        resScore += 0.20;
    }
    
    if ((bottom.ymin - top.ymax - 1) >= rect.size.height * 0.15) {
        resScore += 0.40;
    }
    
    if (OCRUtilsOverlapping(bottom.xmin, bottom.xmax, top.xmin, top.xmax)) {
        resScore += 0.30;
    }

    if (needToRelease)
        delete st;
    if (resScore >= 0.50) {
        newChar = ':';
    }
    return resScore;
} // SingleLetterTestAsColumn


// Test a letter as 'b'
float SingleLetterTestAsb(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_b))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_AS_b;
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE | SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    // Start in a state where we have no information about the expected letter
    newChar = '\0';
    
    // Step 1: expect a single invert component
    ConnectedComponentList inverts = st->getInverseConnectedComponents();
    if ((inverts.size() != 2) || ((inverts.size() > 2) && (inverts[2].area < inverts[1].area * 0.05)))
        return 0.0;
        
    ConnectedComponent holeCC = inverts[1];
    
    // Need the hole to be centered somewhere below the 50% line
    float holeCenterY = (holeCC.ymin + holeCC.ymax) / 2;
    
    // Top of hole past 30% mark
    if ((holeCC.ymin < rect.size.height * 0.30)
        // Center of hole past mid-point
        || (holeCenterY < rect.size.height * 0.50)) {
        if (needToRelease)
            delete st;        
        return 0.0;
    }
    
    // Now check that we have a vertical bar on the left side - allow for some italic slant
    SegmentList slLeftTop = st->getVerticalSegments(0.125, 0.25);
    //pq11
    if (slLeftTop.size() != 1) {
        if (needToRelease)
            delete st;
        return 0.0;
    }
        
    Segment leftBar = slLeftTop[0];
    
    // We expect the left side to go from top to bottom (wide vertical slice)
    if (leftBar.endPos - leftBar.startPos + 1 < rect.size.height * 0.90) {
        if (needToRelease)
            delete st;
        return 0.0;
    }
    
    // Now check that we have nothing past mid-point on top
    SegmentList slTop = st->getHorizontalSegments(0.05, 0.10);
    if (slTop.size() != 1) {
        if (needToRelease)
            delete st;
        return 0.0;
    }
        
    Segment topBar = slTop[0];

    if (topBar.endPos > rect.size.width * 0.50) {
        if (needToRelease)
            delete st;
        return 0.0;
    }
    
    newChar = 'b';
    
    if (needToRelease)
        delete st;
    return 1.0;
} // testAsb


// Returns a score for a letter alternative to open or close square bracket. Called with '[' for now.
float SingleLetterTestAsSquareBracket(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_SQUARE_BRACKET))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_SQUARE_BRACKET; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE | SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    SegmentList slTop = st->getHorizontalSegments(0.05, 0.10);
    if (slTop.size() < 1) {
       return -1.0; 
    }
    float slTopWidth = ((slTop.size() == 1)? (slTop[0].endPos - slTop[0].startPos + 1):-1.0);
    SegmentList slBottom = st->getHorizontalSegments(0.95, 0.10);
    if (slBottom.size() < 1) {
        return -1.0;
    }
    float slBottomWidth = ((slBottom.size() == 1)? (slBottom[0].endPos - slBottom[0].startPos + 1):-1.0);
    SegmentList slMiddle = st->getHorizontalSegments(0.45, 0.10);
    if (slMiddle.size() < 1) {
        return -1.0;
    }
    float slMiddleWidth = ((slMiddle.size() == 1)? (slMiddle[0].endPos - slMiddle[0].startPos + 1):-1.0);
    if ((slTopWidth > rect.size.width * 0.90) && (slBottomWidth > rect.size.width * .90)
        && (slMiddleWidth > 0) && (slMiddle[0].startPos - slTop[0].startPos > rect.size.width * 0.15) && (slTop[0].endPos - slMiddle[0].endPos > rect.size.width * 0.15) && (slMiddle[0].startPos - slBottom[0].startPos > rect.size.width * 0.15) && (slBottom[0].endPos - slMiddle[0].endPos > rect.size.width * 0.15))
    {
        newChar = 'I';
        if (needToRelease)
            delete st;
        return 0.90; 
    }
    
    // L ?
    if ((slBottomWidth > rect.size.width * 0.85) && (slTopWidth > 0)
        && (slMiddleWidth > 0) 
        // Top about as thin as middle
        && ((abs(slTopWidth - slMiddleWidth) < slMiddleWidth * 0.20)
            // Or spreads left & right
            || ((slMiddle[0].startPos - slTop[0].startPos > 0) && (slTop[0].endPos - slMiddle[0].endPos > 0)))
        // Bottom at least twice as large as middle
        && (slBottomWidth > slMiddleWidth * 2.0)
        && (slBottomWidth >= slTopWidth * 1.40)) 
    {
        // Middle ends much before bottom
        if ((slBottom[0].endPos - slMiddle[0].endPos + 1) > rect.size.width * 0.25) {
            newChar = 'L';
            if (needToRelease)
                delete st;
            return 0.75;
        } 
        // Middle ends much after bottom
        else if ((slMiddle[0].endPos - slBottom[0].endPos + 1) > rect.size.width * 0.25) {
            newChar = 'J';
            if (needToRelease)
                delete st;
            return 0.75;
        }
    }
    
    // l ?
         
    if ((slBottomWidth > 0) && (slTopWidth > 0) && (slMiddleWidth > 0) 
        // Top about as thin as middle
        && (abs(slTopWidth - slMiddleWidth) < slMiddleWidth * 0.20)
        && (abs(slBottomWidth - slMiddleWidth) < slMiddleWidth * 0.20)
        // Not curved top & bottom
        && !((abs(slBottom[0].endPos - slMiddle[0].endPos) + 1 > rect.size.width * 0.20) && (abs(slTop[0].endPos - slMiddle[0].endPos) + 1 > rect.size.width * 0.20))
        )
    {
        // Don't override '1', fits all tested criteria below just like 'l'
        if (r->ch != '1') {
            newChar = 'l';
        }
        if (needToRelease)
            delete st;
        return 0.75; 
    }
    
    SingleLetterPrint(slTop, rect.size.width);
    SingleLetterPrint(slMiddle, rect.size.width);
    SingleLetterPrint(slBottom, rect.size.width);
    
    // ) or ( (and not '1')?
         // curved top & bottom
    if (((abs(slBottom[0].endPos - slMiddle[0].endPos) + 1 > rect.size.width * 0.25) 
        || (abs(slBottom[0].startPos - slMiddle[0].startPos) + 1 > rect.size.width * 0.25))
         && ((abs(slTop[0].endPos - slMiddle[0].endPos) + 1 > rect.size.width * 0.25)
            || (abs(slTop[0].startPos - slMiddle[0].startPos) + 1 > rect.size.width * 0.25))
        ) {
        if (r->ch == '1') {
            // Has opening on top (from bottom)?
            CGRect topRect(rectLeft(rect), rectBottom(rect), rect.size.width, rect.size.height * 0.40);
            SingleLetterTests *stTop = CreateSingleLetterTests(topRect, results);
            if (stTop != NULL) {
                OpeningsTestResults resTop;
                bool successTop = st->getOpenings(resTop, SingleLetterTests::Bottom, 
                    0.10,      // Start of range to search (top/left)
                    0.95,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                );
                if (successTop && (resTop.maxDepth > topRect.size.height * 0.20)) {
                    delete stTop;
                    if (needToRelease)
                        delete st;
                    // We are not saying it's a '1' but not overriding either
                    return 0; 
                }
                delete stTop;
            }
        }
        if ((slBottom[0].endPos > slMiddle[0].endPos) 
         &&  (slBottom[0].startPos > slMiddle[0].startPos)
         && (slTop[0].endPos > slMiddle[0].endPos)
         && (slTop[0].startPos > slMiddle[0].startPos)
         ) {
            newChar = '(';
            if (needToRelease)
                delete st;
            return 0.75; 
         } else if ((slBottom[0].endPos < slMiddle[0].endPos) 
         && (slBottom[0].startPos < slMiddle[0].startPos) 
         && (slTop[0].endPos < slMiddle[0].endPos)
         && (slTop[0].startPos < slMiddle[0].startPos)
         ) {
            newChar = ')';
            if (needToRelease)
                delete st;
            return 0.75; 
         }
    }
    
    if (needToRelease)
        delete st;
    return 0; 
} // testasSquareBracket

// Tests a letter for &
// Return '\0' if no opinion or error
wchar_t SingleLetterTestAsAndSign(OCRRectPtr r, CGRect rect, bool testNextSize, OCRWordPtr &stats, OCRResults *results) {

    if (!results->imageTests) return '\0';
    
    if (testNextSize
        //[8 (0x38)] at [555,505 - 567,522] [w=13,h=18]
        //[: (0x3a)] at [565,505 - 570,514] [w=6,h=10]
        
        // [8 (0x38)] at [677,512 - 692,532] [w=16,h=21]
        // [: (0x3a)] at [690,512 - 698,527] [w=9,h=16] => 76%
        && ((r->next == NULL) || (r->next->rect.size.height > r->rect.size.height * 0.77))) {
        return '\0';
    }
    
    SingleLetterTests *st = CreateSingleLetterTests(rect, results);
    if (st == NULL) {
        return '\0';
    }
    
    // Require single connected component
    ConnectedComponentList cpl = st->getConnectedComponents();
    if (cpl.size() != 2) {
        return '\0';
    }

    // Look for the little '<' pattern bottom-right
    SegmentList slBottomRight = st->getVerticalSegments(0.95, 0.10);
    if (slBottomRight.size() < 2) {
        return '\0';
    }
    
    Segment topBranch = slBottomRight[slBottomRight.size()-2];
    Segment bottomBranch = slBottomRight[slBottomRight.size()-1];
    
    // Require top branch below 23% mark
    if ((topBranch.startPos > rect.size.height * 0.23)
        && (bottomBranch.startPos > rect.size.height * 0.80)) {
        // Now test for a bounded opening from the right
        OpeningsTestResults resRight;
        bool success = st->getOpenings(resRight, SingleLetterTests::Right,
                                       topBranch.startPos / rect.size.height,      // Start of range to search (top/left)
                                       bottomBranch.startPos / rect.size.height,      // End of range to search (bottom/right)
                                       SingleLetterTests::Bound,   // Require start (top/left) bound
                                       SingleLetterTests::Bound  // Require end (bottom/right) bound
                                       );
        if (!success || (resRight.maxDepth > rect.size.width * 0.10)) {
            delete st;
            return '&';
        }
    }
    
    delete st;
    return '\0';
} // SingleLetterTestAsAndSign

// Returns '\0' is it has no opinion one way or the other
char SingleLetterTestoAsa (OCRRectPtr r,OCRResults *results, SingleLetterTests **inputStPtr) {
    if ((r->flags6 & FLAGS6_TESTED_o_AS_a) || !results->imageTests)
        return '\0';    // already tested or can't test, express no opinion and return
    
    r->flags6 |= FLAGS6_TESTED_o_AS_a;
    
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(r->rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    
    if (st == NULL) {
        return '\0';
    }
    
    bool hasRightBar = st->getSide(SingleLetterTests::Right, 0.90, true);
    if (hasRightBar) {
        // Presence of flatness on the left is suspect (but not fatal)
        bool hasLeftBar = st->getSide(SingleLetterTests::Left, 0.75, true);
        
        // Now also require a little notch above or below
        OpeningsTestResults top;
        bool success = st->getOpenings(top, 
            SingleLetterTests::Top,
            0.65,      // Start of range to search (top/left)
            1.00,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound);  // Require end (bottom/right) bound
        bool hasTopNotch = false;
        if (success && (top.maxDepth > r->rect.size.height * 0.01)) {
            if (!hasLeftBar) {
                // If no left bar, one opening is enough
                if (needToRelease) delete st;
                return 'a';
            } else {
                hasTopNotch = true;
            }
        }
        
        bool hasBottomNotch = false;
        OpeningsTestResults bottom;
        success = st->getOpenings(bottom, 
            SingleLetterTests::Bottom, 
            0.40,      // Start of range to search (top/left)
            1.00,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound);  // Require end (bottom/right) bound

        
        // Only a notch below but a very convincing one => accept
        // Important: flatish bottom half and notch below could be a part of a next letter => reject if glued to next
        if (success && (bottom.maxDepth > r->rect.size.height * 0.10)
                && ((r->next == NULL) || (rectSpaceBetweenRects(r->rect, r->next->rect) > 0))) {
            SegmentList slBottom = st->getHorizontalSegments(0.975, 0.05);
#if DEBUG
            SingleLetterPrint(slBottom, r->rect.size.width);
#endif
            if ((slBottom.size() == 2) 
                // Starts past middle
                && (slBottom[1].endPos > r->rect.size.width * 0.50) 
                // Ends past 60% mark
                && (slBottom[1].startPos > r->rect.size.width * 0.60) 
                // Wider than 15% of width
                && (slBottom[1].startPos - slBottom[0].endPos - 1 > r->rect.size.width * 0.15)) {
                    if (needToRelease) delete st;
                    return 'a';
            }
        }
        
        // When testing 'u' as potential 'o', OK to find a left bar and looks like top notch is enough
        if (hasTopNotch && (r->ch == 'u')) {
            if (needToRelease) delete st;
            return 'a';
        }

        // We still have a chance if we have a top notch and has left bar or if no left bar at all
        if (hasTopNotch || !hasLeftBar) {
            if (success && (bottom.maxDepth > r->rect.size.height * 0.01)) {
                if (!hasLeftBar) {
                    // If no left bar, one opening is enough
                    if (needToRelease) delete st;
                    return 'a';
                } else {
                    hasBottomNotch = true;
                }
            }
        }
        
        if (hasTopNotch && hasBottomNotch) {
            // If we are here, we found notches but left is flat: test for center inverted component
            ConnectedComponentList inverts = st->getInverseConnectedComponents();
            if ((inverts.size() == 2) && (inverts[1].getHeight() > r->rect.size.height * 0.50)) {
                if (needToRelease) delete st;
                return 'a';
            }
        }
    } // has right bar
    else // no right bar
    {
        // No right bar (only right bar starting from mid-height) and only a notch below but a very convincing one => accept
        if (st->getSide(SingleLetterTests::Right, 0.40, true)) 
        {
            OpeningsTestResults bottom;
            bool success = st->getOpenings(bottom, 
                SingleLetterTests::Bottom, 
                0.60,      // Start of range to search (top/left)
                1.00,      // End of range to search (bottom/right)
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound);  // Require end (bottom/right) bound

            // 
            if (success && (bottom.maxDepth > r->rect.size.height * 0.06)) {
                SegmentList slBottom = st->getHorizontalSegments(0.975, 0.05);
#if DEBUG
                SingleLetterPrint(slBottom, r->rect.size.width);
#endif                        
                if ((slBottom.size() == 2) 
                    // Starts past middle
                    && (slBottom[1].endPos > r->rect.size.width * 0.50) 
                    // Ends past 60% mark
                    && (slBottom[1].startPos > r->rect.size.width * 0.60) 
                    // Wider than 15% of width
                    && (slBottom[1].startPos - slBottom[0].endPos - 1 > r->rect.size.width * 0.14)) {
                    if (needToRelease) delete st;
                    return 'a';
                } else {
                    OpeningsTestResults topRight;
                    bool success = st->getOpenings(topRight,
                                                   SingleLetterTests::Top,
                                                   0.60,      // Start of range to search (top/left)
                                                   0.95,      // End of range to search (bottom/right)
                                                   SingleLetterTests::Bound,   // Require start (top/left) bound
                                                   SingleLetterTests::Bound);  // Require end (bottom/right) bound
                    if (success && (topRight.maxDepth > r->rect.size.height * 0.06)) {
                        if (needToRelease) delete st;
                        return 'a';
                    }
                } // alternative chance to call it a 'a'
            } // has opening on top
        } // Has small right bar
    } // No large right bar

    if (needToRelease) delete st;
    return 'o';
} // SingleLetterTestoAsa

float SingleLetterTestU(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar1, wchar_t &newChar2, CGRect &newRect1, CGRect &newRect2, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_U))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_AS_U;
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    newChar1 = newChar2 = '\0';
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    SingleLetterPrint(cpl, rect);
    // Case where we have 3 connected components (L+i)
    if ((cpl.size() == 4) || ((cpl.size() > 4) && (cpl[4].area < cpl[3].area * 0.05))) {
        ConnectedComponent letterLCC = cpl[1];
        ConnectedComponent letteriCC = cpl[2];
        ConnectedComponent dotiCC = cpl[3];
        
            // L to the left of i
        if ((letterLCC.xmax <= letteriCC.xmin)
            // i shorter than L
            && (letteriCC.getHeight() < letterLCC.getHeight())
            // dot above i
            && (dotiCC.ymax < letteriCC.ymin)
            // X overlap between dot and i
            && OverlappingX(letteriCC, dotiCC)
            // dot not too large
            && (dotiCC.area < letteriCC.area * OCR_RATIO_DOT_AREA_TO_i_AREA_MAX)
            // dot not too small
            && (dotiCC.area > letteriCC.area * OCR_RATIO_DOT_AREA_TO_i_AREA_MIN))
        {
            newChar1 = 'L';
            newChar2 = 'i';
            newRect1.origin.x = rectLeft(rect) + letterLCC.xmin;
            newRect1.origin.y = rectBottom(rect) + letterLCC.ymin;
            newRect1.size.width = letterLCC.getWidth();
            newRect1.size.height = letterLCC.getHeight();
            
            float minX = MIN(letteriCC.xmin, dotiCC.xmin);
            float maxX = MAX(letteriCC.xmax, dotiCC.xmax);
            float minY = MIN(letteriCC.ymin, dotiCC.ymin);
            float maxY = MAX(letteriCC.ymax, dotiCC.ymax);
            newRect2.origin.x = rectLeft(rect) + minX;
            newRect2.origin.y = rectBottom(rect) + minY;
            newRect2.size.width = maxX - minX + 1;
            newRect2.size.height = maxY - minY + 1;

            if (needToRelease)
                delete st; 
            return 1.0;
        }
        
                // No other ideas in the case of 3 components, return -1
        if (needToRelease)
            delete st;
        return -1.0;
    }
        
    // Case with one connected component
    // 4 instead of U (David Gomez)
    // a instead of u (CBRE Richard)
    if ((cpl.size() == 2) || ((cpl.size() > 2) && (cpl[2].area < cpl[1].area * 0.05))) {
        // Verify opening on top
        OpeningsTestResults topOpening;
        bool successTop = st->getOpenings(topOpening, SingleLetterTests::Top, 
        0.10,      // Start of range to search (top/left)
        0.95,    // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
    
        bool hasOpeningTop = (successTop && (topOpening.maxDepth > rect.size.height * 0.15));
        
        if (hasOpeningTop) {
            // Now test if we have a tall leg on right side => '4'
            SegmentList slBottom = st->getHorizontalSegments(0.925, 0.15);
            SegmentList slTop = st->getHorizontalSegments(0.10, 0.10);
            float widthTop = -1.0;
            if (slTop.size() >= 2) {
                widthTop = slTop[slTop.size()-1].endPos - slTop[0].startPos + 1;
            }
            float widthBottom = -1.0;
            if (slBottom.size() >= 1) {
                widthBottom = slBottom[slBottom.size()-1].endPos - slBottom[0].startPos + 1;
            }
            if ((widthTop > 0) && (widthBottom > 0)
                && (widthTop > widthBottom * 2.0)
                && (slBottom[0].startPos - slTop[0].startPos > rect.size.width * 0.15))
            {
                newChar1 = '4';
                float gap = gapBelow(r, 1);
                if (gap > rect.size.height * OCR_MIN_GAP_BELOW_LINE_g)
                    newChar1 = 'y';
                if (needToRelease)
                    delete st; 
                return 1.0;
            }
        }
    }
    
    if (needToRelease)
        delete st;
    return -1.0;
} // testU


// Testing 8, & and B
// Returns '\0' if it can't decide anything (e.g. single letter test not possible), same char as input if not indications to the contrary (which doesn't mean it's not a mistake, only that we can't tell)
wchar_t SingleLetterTest8(OCRRectPtr r,OCRResults *results) {
    if (!results->imageTests)
        return '\0';
    if (r->flags2 & FLAGS2_TESTED)
        return (r->ch);

    SingleLetterTests *st = CreateSingleLetterTests(r->rect, results);
    
    r->flags2 |= FLAGS2_TESTED;
    
    if (st == NULL)
        return '\0';
            
    // Make sure there is only one main component, otherwise abstain from correcting anything
    ConnectedComponentList cpl = st->getConnectedComponents();
        
    if ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.10)) {
        delete st;
        return '\0';
    }
            
    wchar_t resChar = '\0';
    
    bool gluedToPrevious = false;
    if ((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0))
        gluedToPrevious = true;
        
    OpeningsTestResults bottomLeftOpening;
    bool gotBottomLeftOpening = false;
    
    OpeningsTestResults topLeftOpening;
    bool gotTopLeftOpening = false;
            
    // First test: is it a '3'?
    if (!gluedToPrevious) {
        // Look for two openings to the left
        bool success = st->getOpenings(bottomLeftOpening, 
                    SingleLetterTests::Left, 
                    0.40,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
        if (success && (bottomLeftOpening.maxDepth > cpl[1].getWidth() * 0.30)) {
            gotBottomLeftOpening = true;
        }
               
        if (gotBottomLeftOpening) {
            // Workaround
            CGRect newRect(rectLeft(r->rect), rectBottom(r->rect),
                r->rect.size.width,
                bottomLeftOpening.maxDepthCoord);
            SingleLetterTests *topSt = CreateSingleLetterTests(newRect, results);
            bool success = false;
            if (topSt != NULL) {
                success = topSt->getOpenings(topLeftOpening, 
                    SingleLetterTests::Left, 
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    ); 
                
                // End workaround
                if (success) {
                    if (topLeftOpening.maxDepth > cpl[1].getWidth() * 0.30) {
                        gotTopLeftOpening = true;
                    } else if (topLeftOpening.maxDepth >= cpl[1].getWidth() * 0.23) {
                        // With the cut-off top half of the '3' the API returns just the depth of the opening limited by the width of the middle part of the '3', which makes it smaller than if we included the whole letter. Attempt to get horiz. segment showing only a thin part on the right side
                        SegmentList sl = topSt->getHorizontalSegments(topLeftOpening.maxDepthCoord/newRect.size.width, 0.00);
                        if (((sl.size() == 1) && (sl[0].startPos > newRect.size.width * 0.65))
                            // Case where we interesect with pixels above on the left. Check that the leftmost segment is NOT the one which gave us maxDepth
                            || ((sl.size() == 2) && (sl[0].startPos < topLeftOpening.maxDepth) &&  (sl[1].startPos > newRect.size.width * 0.65))) {
                            gotTopLeftOpening = true;
                        }
                    }
                }
                delete topSt;
            } // topSt != NULL
        }
        
        if (gotBottomLeftOpening && gotTopLeftOpening) {
            delete st;
            return '3';
        }
    }
    
    if ((r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0))
    {
        // Glued to next, can't test right side, we are done
        delete st;
        return r->ch;
    }
    
    OpeningsTestResults topRightOpening;
    bool gotTopRightOpening = false;
    CGRect topHalfRect (rectLeft(r->rect), rectBottom(r->rect), r->rect.size.width, r->rect.size.height * 0.50);
    // Accept inverted because we are slicing a pattern in two
    SingleLetterTests *stTopHalf = CreateSingleLetterTests(topHalfRect, results, false, 0, 0.03, true);
    if (stTopHalf != NULL) {
        bool success = st->getOpenings(topRightOpening,
                    SingleLetterTests::Right, 
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
        if (success && (topRightOpening.maxDepth > cpl[1].getWidth() * 0.30)) {
            gotTopRightOpening = true;
        }
                
        // e.g. depth = 2, width = 13 (15.3%), testing opening width with thickness 2 (15.x%) we get 4 (out of height 19, i.e. > 20%)
        if (success && (r->ch == '&') && (topRightOpening.maxDepth > cpl[1].getWidth() * 0.15) 
            && (topRightOpening.maxWidth > cpl[1].getHeight() * 0.18)) 
        {
            r->confidence = 0;
            resChar = 0xa3; // £ - to indicate this is a real '&', no need to suspect it and don't map it to a '8' in a phone number;
        }

        // e.g. real B with depth = 13 out of width = 43 => 0.30x
        // TODO get information about enclosed holes in main component! If not two large holes, not a B
        if ((resChar == '\0') && success && (topRightOpening.maxDepth > cpl[1].getWidth() * 0.50) 
            && (topRightOpening.maxWidth > cpl[1].getHeight() * 0.10)) 
        {
            // We found an opening to the right, must be a '6'!
            // Could still be a bogus small opening created by imperfect image processing - check if there is a large dent middle/left (as in an '8'
            // e.g. receipt where left opening was 2 pixels out of width 13 => a little over 15%
            OpeningsTestResults middleLeftOpening;
            bool success = st->getOpenings(middleLeftOpening,
                    SingleLetterTests::Left,
                    0.25,      // Start of range to search (top/left)
                    0.75,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
            if (!success || (middleLeftOpening.maxDepth < cpl[1].getWidth() * 0.10)) {
                resChar = '6';
            } else {
                ReplacingLog("SingleLetterTest8: aborting [%c -> 6] replacing because of depth %.0f opening on middle left (out of width %.0f) in word [%s]", (unsigned short)r->ch, middleLeftOpening.maxDepth, cpl[1].getWidth(), toUTF8(r->word->text()).c_str());
            }
        }
        delete stTopHalf;
    } // stTopHalf != NULL

    /* If we are here and there is no char suggested in resChar, then:
        - we failed to find the pattern on the right matching '3' (opening top left and bottom left)
        - we failed to find a top right opening ('6')
        => must be a '8'
    */
    // Don't make a suggestion if char is '&' because it may look just like a '8'
    if ((resChar == '\0') && (r->ch != '&'))
        resChar = '8';
                        
    delete st;
    return resChar;
} // test8

/* Returns a score for a letter like H. Possible returned letters:
- H
- K
Called with:
- H
*/
float SingleLetterTestH(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_H))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_AS_H; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE|SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    float resScore = 0;
    
    // 0.50: top & bottom vertical segments on right
    // 0.25: large opening on the right
    // 0.25: max depth found near mid-height

    LimitedOpeningsTestResults resRight;
    bool hasOpeningRight = false;

    // TODO: use regular openings test when I fix the bug that returns sub-optimal openings
    bool successRight = st->getOpeningsLimited(resRight, SingleLetterTests::Right, 
        0.30,      // Start of range to search (top/left)
        0.70,    // End of range to search (bottom/right)
        0.15,
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
        
    hasOpeningRight = (successRight && (resRight.maxDepth > rect.size.width * 0.15));
    if (hasOpeningRight) {
        resScore += 0.25;
        if ((resRight.maxDepthCoord > rect.size.height * 0.40) && (resRight.maxDepthCoord < rect.size.height * 0.60))
            resScore += 0.25;
    }

    SegmentList slRight = st->getVerticalSegments(0.90, 0.05);
    if ((slRight.size() == 2) 
        // Top element near top
        && (slRight[0].startPos < rect.size.height * 0.10)
        // Bottom element near bottom
        && (slRight[1].endPos > rect.size.height * 0.90)
        // Gap at least 20% of height
        && (slRight[1].startPos - slRight[0].endPos + 1 > rect.size.height * 0.20)) {
            resScore += 0.50;
    }
    
    if (resScore >= 0.50) {
        if (tallHeightTest(stats.getPtr(), rect.size.height, 'K', false, 0, true) == 1)
            newChar = 'K';
        else
            newChar = 'k';
    }
    
    if (needToRelease)
        delete st;            
    return resScore;
} // testH

/* Tests: y or g
 Returns:
    - 'y' if found a hole on top (deep and wide) and no closed holes
    - 'g' if no hole on top or narrow hole but has a closed hole
*/
wchar_t SingleLetterTestAsgOry(OCRRectPtr r, OCRResults *results, SingleLetterTests **inputStPtr) {
    if (((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0))
        || ((r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0)))
        return '\0';
    wchar_t resChar = r->ch;
    if (r->flags & TESTED_OPENING_TOP)
        return '\0';
    r->flags |= TESTED_OPENING_TOP;
    if (!results->imageTests)
        return '\0';
    if ((r->ch != 'y') && (r->ch != 'g'))
        return '\0';    // No opinion, this test only differentiates between y & g
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(r->rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    ConnectedComponentList cpl = st->getConnectedComponents();
#if DEBUG
    if (cpl.size()>=2) {
        DebugLog("SingleLetterTestAsgOry: cpl[1]: [%d,%d] - [%d,%d] [w=%d,h=%d], area=%d", (unsigned int)cpl[1].xmin,  (unsigned int)cpl[1].ymin, (unsigned int)cpl[1].xmax,  (unsigned int)cpl[1].ymax, (unsigned int)cpl[1].getWidth(), (unsigned short)cpl[1].getHeight(), (unsigned int)cpl[1].area);
    }
    if (cpl.size()>=3) {
        DebugLog("SingleLetterTestAsgOry: cpl[2]: [%d,%d] - [%d,%d] [w=%d,h=%d], area=%d", (unsigned int)cpl[2].xmin,  (unsigned int)cpl[2].ymin, (unsigned int)cpl[2].xmax,  (unsigned int)cpl[2].ymax, (unsigned int)cpl[2].getWidth(), (unsigned short)cpl[2].getHeight(), (unsigned int)cpl[2].area);
    }
#endif
    if (validateConnectedComponents(cpl, r->rect))
    {
        LimitedOpeningsTestResults resTopLimited;
        bool successTopLimited = st->getOpeningsLimited(resTopLimited, SingleLetterTests::Top,
            0.20,      // Start of range to search (top/left)
            1.00,    // End of range to search (bottom/right)
            0.25,
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
        
        float requiredDepth = ((r->rect.size.height > r->rect.size.width * 1.4)? 0.15:0.07);
        float requiredDepthDeep = 0.15;
        if (!successTopLimited || (resTopLimited.maxDepth < r->rect.size.height * requiredDepth)) {
            // No opening deep enough - BUT might be because of the OpeningsLimited 0.25 limitation. Try the normal opening test
            LimitedOpeningsTestResults resTop;
            bool successTop = st->getOpenings(resTop, SingleLetterTests::Top,
                0.20,      // Start of range to search (top/left)
                1.00,    // End of range to search (bottom/right)
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound  // Require end (bottom/right) bound
                );
            if (!successTop || (resTop.maxDepth < r->rect.size.height * requiredDepth)) {
                // No opening on top, assume 'g'
                resChar = 'g';
            } else if (successTop && (resTop.maxDepth > r->rect.size.height * requiredDepth)) {
                // We found a deep opening, but not with the limited test. Try a different way, utilizing segments
                SegmentList slTop = st->getHorizontalSegments(0.125, 0.25);

                if ((slTop.size() == 2) && (resTop.maxDepth > r->rect.size.height * requiredDepthDeep) && (slTop[1].startPos - slTop[0].endPos > r->rect.size.width * 0.10)) {
                    resChar = 'y';
                }
            }
        } else if (successTopLimited && (resTopLimited.maxDepth > r->rect.size.height * requiredDepthDeep) && (resTopLimited.minWidth > r->rect.size.width * 0.10)) {
            // Deep and wide hole on top, assume 'y' (within y/g space)
            resChar = 'y';
        }
        
        ConnectedComponentList invertedCpl = st->getInverseConnectedComponents();
#if DEBUG
        SingleLetterPrint(invertedCpl, r->rect);
#endif
      
        if ((resChar == 'g') && (invertedCpl.size() >= 2)) {
            // Search for a hole above mid-point (to exclude a possible hole at the bottom of some g & y's
            bool found = false;
            for (int k=1; k<invertedCpl.size(); k++) {
                ConnectedComponent cc = invertedCpl[k];
                if (cc.ymax + cc.ymin / 2 < r->rect.size.height * 0.50) {
                    found = true;
                    break;
                }
            }
            if (!found)
                resChar = '\0'; // Decline to make a call - we first called it a 'g' (based on not finding a deep enough opening on top) but then again we don't have a hole on the top half so it might not be a 'g'
        }
        // One more chance: if there is a large inverted component on top half, must be a 'g'
        else if (resChar == 'y') {
            ConnectedComponentList invertedCpl = st->getInverseConnectedComponents();
            SingleLetterPrint(cpl[1], r->rect);
            SingleLetterPrint(invertedCpl, r->rect);
            float requiredHoleSize = ((requiredDepth > 0.12)? 0.06:0.04);
            if ((invertedCpl.size() >= 2) && (invertedCpl[1].area > cpl[1].getWidth() * cpl[1].getHeight() * requiredHoleSize) 
                // Hole is proof of 'g' only if on top half
                && (((invertedCpl[1].ymin + invertedCpl[1].ymax)/2 < cpl[1].getHeight() * 0.50)
                    // But accept it if in addition the opening on top was narrow or non-existent
                    || (!successTopLimited || (resTopLimited.minWidth < r->rect.size.width * 0.10)))
                ) 
            {
                resChar = 'g';
                DebugLog("validate: found hole in [%c] at [%d,%d] - [%d,%d] area=%d vs rect [w=%d, h=%d], assuming it's a [%c]", (unsigned short)r->ch, (unsigned int)invertedCpl[1].xmin,  (unsigned int)invertedCpl[1].ymin, (unsigned int)invertedCpl[1].xmax,  (unsigned int)invertedCpl[1].ymax, (unsigned int)invertedCpl[1].area, (unsigned int)cpl[1].getWidth(), (unsigned int)cpl[1].getHeight(), (unsigned short)resChar);
            }
        }
    } // validated components
    else {
        resChar = '\0'; // No opinion
    }
    
    if (needToRelease && (st != NULL)) {
        delete st;
    }
    return resChar;
} // SingleLetterTestAsgOry

/* Returns '\0' if it can't decide anything (e.g. single letter test not possible)
   So far used to test:
   - J instead of 3
   - ? instead of 2
   - b instead of 5 or 6
*/
wchar_t SingleLetterTestAsDigit(OCRRectPtr r,OCRResults *results, OCRWordPtr &stats, bool forceDigit) {
    if (r->flags2 & FLAGS2_TESTED_AS_DIGIT)
        return (r->ch);
    
    // Figure out if this is a probable digit 1 based on it being narrow like other '1' digits on this line / page
    bool probableDigit1 = false; bool probableNormalDigit = false;
    if ((stats->averageWidthDigits.average > 0) && (stats->averageHeightDigits.average > 0)) {
        float normalDigitRatio = 0;
        float averageDigitsHeight = 0;
        if (stats->averageWidthDigits.count >= 2) {
            averageDigitsHeight = stats->averageHeightDigits.average;
            normalDigitRatio = averageDigitsHeight / stats->averageWidthDigits.average;
        } else if (results->globalStats.averageWidthDigits.count >= 2) {
            averageDigitsHeight = results->globalStats.averageHeightDigits.average;
            normalDigitRatio = averageDigitsHeight / results->globalStats.averageWidthDigits.average;
        }
        float digit1Ratio = 0;
        if (stats->averageWidthDigit1.count >= 2)
            digit1Ratio = stats->averageHeightDigit1.average / stats->averageWidthDigit1.average;
        else if (results->globalStats.averageWidthDigit1.count >= 2)
            // No need to use the average height of digits 1 since 1's have same heights as all other digits
            digit1Ratio = averageDigitsHeight / results->globalStats.averageWidthDigit1.average;
        // Require that height of current rect is close to average digit height otherwise we shouldn't say anything
        if (!((r->rect.size.height < stats->averageHeightDigits.average * 1.15) && (r->rect.size.height > stats->averageHeightDigits.average * 0.85)))
            return '\0';
        float currentRatio = stats->averageHeightDigits.average / r->rect.size.width;   // Using the average height so that if current rect is a bit off height-wise, we don't skew the ration comparison. After all current is SUPPOSED to have same height as other digits.
        // 2.625 for '1' on a Target receipt versus 1.9 for normal digit => x1.36
        // If '1's are sufficiently different than other digits
        if (digit1Ratio > normalDigitRatio * 1.25) {
            if (currentRatio > digit1Ratio * 0.85) {
                // Narrow like a '1', must be a '1'!
                probableDigit1 = true;
                // For some values of r->ch, and if forcing digit, we want to stop here and return '1'
                if (r->ch == 'b') {
                    return '1';
                }
            } else if (currentRatio < normalDigitRatio * 1.15) {
                probableNormalDigit = true;
            }
        }
    }
    
    wchar_t resChar = '\0';
    // Even if image tests fail later
    if (forceDigit) {
        // Never risk spoiling a digit (even if replacing with '1' which is also a digit, just too dangerous)
        if (probableDigit1 && !isDigitLookalike(r->ch))
            resChar = '1'; // Regardless of what character it it!
        else if (r->ch == '?')
            resChar = '2'; // Best guess for '?' (2015-05-14 Blink returned '?' instead of '2')
    }

    SingleLetterTests *st = CreateSingleLetterTests(r->rect, results, SINGLE_LETTER_VALIDATE_SINGLE_COMP);
    r->flags2 |= FLAGS2_TESTED_AS_DIGIT;
    
    if (st == NULL) {
        return (resChar);
    }
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    if (!validateConnectedComponents(cpl, r->rect))
        return resChar;
    
    // If 'b' return '5' if large opening on left side / bottom, otherwise '6'
    if (r->ch == 'b') {
        OpeningsTestResults bottomLeft;
        bool success = st->getOpenings(bottomLeft,
            SingleLetterTests::Left,
            0.30,      // Start of range to search (top/left)
            0.95,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
        if (success && (bottomLeft.maxDepth > cpl[1].getWidth() * 0.30)) {
            if (forceDigit) {
                delete st;
                return '5';
            } else {
                // If not forcing digit replacement, need to test the opening some more, to make sure it's a wide opening
                LimitedOpeningsTestResults leftBottomOpening;
                bool successLeftBottom = st->getOpeningsLimited(leftBottomOpening,
                    SingleLetterTests::Left,
                    0.30,      // Start of range to search (top/left)
                    0.95,      // End of range to search (bottom/right)
                    0.60,
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                );
                if (successLeftBottom && (leftBottomOpening.maxDepth > cpl[1].getWidth() * 0.30)
                    && (leftBottomOpening.minWidth > cpl[1].getHeight() * 0.20)) {
                    delete st;
                    return '5';
                }
                delete st;
                return '\0';
            }
        } else {
            delete st;
            if (forceDigit)
                return '6';
            else
                return '\0';
        }
        
    }
    
    /* Test for possible 0:
        - two vertical segments left and right with wide gap near top, center and bottom
        - deep opening from bottom or from top (to allow for he absence of any inverted connected component because of a slight opening top of bottom
        - no openings left or right
    */
    if ((resChar != '1') && (forceDigit || (r->ch == '6') || (r->ch == '9') || (r->ch == '8'))) {
        SegmentList slTop = st->getHorizontalSegments(0.20, 0.05);
        SegmentList slMiddle = st->getHorizontalSegments(0.50, 0.10);
        SegmentList slBottom = st->getHorizontalSegments(0.80, 0.05);
        if ((slTop.size() == 2) && (slMiddle.size() == 2) && (slBottom.size() == 2)
            && (slTop[0].endPos < cpl[1].getWidth() * 0.50) && (slTop[1].startPos > cpl[1].getWidth() * 0.50)
            && (slMiddle[0].endPos < cpl[1].getWidth() * 0.50) && (slMiddle[1].startPos > cpl[1].getWidth() * 0.50)
            && (slBottom[0].endPos < cpl[1].getWidth() * 0.50) && (slBottom[1].startPos > cpl[1].getWidth() * 0.50)) {
            // Now look for deep opening from bottom or top
            bool foundDeepOpening = false;
            CGRect topRect;
            topRect.origin.x = r->rect.origin.x + cpl[1].xmin;
            topRect.size.width = cpl[1].getWidth();
            topRect.origin.y = r->rect.origin.y + cpl[1].ymin;
            topRect.size.height = cpl[1].getHeight() * 0.80;
            SingleLetterTests *stTop = CreateSingleLetterTests(topRect, results);
            if (stTop != NULL) {
                OpeningsTestResults bottom;
                bool success = stTop->getOpenings(bottom,
                    SingleLetterTests::Bottom,
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (success && (bottom.maxDepth > cpl[1].getHeight() * 0.40))
                    foundDeepOpening = true;
                delete stTop;
            }
            if (!foundDeepOpening) {
                CGRect bottomRect;
                bottomRect.origin.x = r->rect.origin.x + cpl[1].xmin;
                bottomRect.size.width = cpl[1].getWidth();
                bottomRect.origin.y = r->rect.origin.y + cpl[1].ymin + cpl[1].getHeight() * 0.20;
                bottomRect.size.height = cpl[1].getHeight() * 0.80;
                SingleLetterTests *stBottom = CreateSingleLetterTests(topRect, results);
                if (stBottom != NULL) {
                    OpeningsTestResults top;
                    bool success = stBottom->getOpenings(top,
                        SingleLetterTests::Bottom,
                        0.00,      // Start of range to search (top/left)
                        1.00,      // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (success && (top.maxDepth > cpl[1].getHeight() * 0.40))
                        foundDeepOpening = true;
                    delete stBottom;
                }
            }
            if (foundDeepOpening) {
                // Now test sides for openings
                OpeningsTestResults left;
                bool success = st->getOpenings(left,
                    SingleLetterTests::Left,
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success || (left.maxDepth < cpl[1].getWidth() * 0.15)) {
                    OpeningsTestResults right;
                    bool success = st->getOpenings(right,
                        SingleLetterTests::Right,
                        0.00,      // Start of range to search (top/left)
                        1.00,      // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    if (!success || (right.maxDepth < cpl[1].getWidth() * 0.15)) {
                        return '0';
                    }
                }
            }
        }
    }
    
    CGRect rect = r->rect;
    if ((rect.size.width != cpl[1].getWidth()) || (rect.size.height != cpl[1].getHeight())) {
        // Adjust rect
        rect.origin.x += cpl[1].xmin;
        rect.size.width = cpl[1].getWidth();
        rect.origin.y += cpl[1].ymin;
        rect.size.height = cpl[1].getHeight();
        r->rect = rect; // Note: if caller accepts suggested replacement, new rect will also update averages
        delete st;
        st = CreateSingleLetterTests(rect, results);
        if (st == NULL)
            return resChar;
        else {
            cpl = st->getConnectedComponents();
            if (!validateConnectedComponents(cpl, rect))
                return resChar;
        }
    }
    
    int top20Width = -1, bottom20Width = -1;
    // When I get around to it, could be good to test ALL '?' characters as possible digit (even if not forceDigit). For now however on receipts we'll be in forceDigits mode most of the time (e.g. known product number zone or price zone)
    if ((r->ch == '?') && forceDigit) {
        // We probably already assume it's a '2' but could also be '7'
        // Fairly wide on top & bottom?
        SegmentList slTop = st->getHorizontalSegments(0.10, 0.20);
        SegmentList slBottom = st->getHorizontalSegments(0.90, 0.20);
        if (slTop.size() == 1) {
            top20Width = slTop[0].endPos - slTop[0].startPos + 1;
        }
        if (slBottom.size() == 1) {
            bottom20Width = slBottom[0].endPos - slBottom[0].startPos + 1;
        }
        // Decide on '2' if base is wide (on receipts '7' have no base at all). Even if we go with '7' we do additional testing to revert to '2' in some cases
        if ((top20Width > 0) && (bottom20Width > 0) && (bottom20Width > cpl[1].getWidth() * 0.70))
            return '2';
        else {
            resChar = '7';
            // Still possible bottom of a '2' got damaged (shrunk), test for rounded hat on top (as in a '2')
            CGRect top25Rect = rect;
            top25Rect.size.height = rect.size.height * 0.25; // Top 25%
            SingleLetterTests *stTop25 = CreateSingleLetterTests(top25Rect, results);
            if (stTop25 != NULL) {
                OpeningsTestResults bottom;
                bool success = stTop25->getOpenings(bottom,
                    SingleLetterTests::Bottom,
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (success && (bottom.maxDepth > cpl[1].getHeight() * 0.15))
                    resChar = '2';
                delete stTop25;
            }
            delete st;
            return resChar;
        }
    }
    
    if ((r->ch == 'J') || (r->ch == 'j')) 
    {    
        // Test for opening on top left
        OpeningsTestResults topLeft;
        bool success = st->getOpenings(topLeft, 
            SingleLetterTests::Left, 
            0.00,      // Start of range to search (top/left)
            0.40,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
        if (success && (topLeft.maxDepth > r->rect.size.width * 0.05)) 
        {
            // Openings test returns depth as bounded by entire height, we need to restrict to the bottom
            CGRect bottomRect(rectLeft(r->rect), rectBottom(r->rect) + topLeft.maxDepthCoord, r->rect.size.width, rectTop(r->rect) - (rectBottom(r->rect) + topLeft.maxDepthCoord) + 1);
            SingleLetterTests *stBottom = CreateSingleLetterTests(bottomRect, results);
            if (stBottom != NULL) 
            {
                OpeningsTestResults bottomLeft;
                bool success = stBottom->getOpenings(bottomLeft, 
                    SingleLetterTests::Left, 
                    0.00,      // Start of range to search (top/left)
                    0.95,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (success && (bottomLeft.maxDepth > r->rect.size.width * 0.05)) {
                    // Openings test returns depth as bounded by entire height, we need to restrict to the top
                    CGRect topRect(rectLeft(r->rect), rectBottom(r->rect), r->rect.size.width, rectBottom(bottomRect) + bottomLeft.maxDepthCoord - rectBottom(r->rect) + 1);
                    SingleLetterTests *stTop = CreateSingleLetterTests(topRect, results);
                    if (stTop != NULL) 
                    {
                        OpeningsTestResults topTopLeft;
                        bool success = stTop->getOpenings(topTopLeft, 
                            SingleLetterTests::Left, 
                            0.00,      // Start of range to search (top/left)
                            0.95,      // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                        // Require that at least one hole be 15% deep
                        if (success && ((topTopLeft.maxDepth > r->rect.size.width * 0.15) || (bottomLeft.maxDepth > r->rect.size.width * 0.15))) {
                            resChar = '3';
                        } 
                        delete stTop;
                    } // stTop != NULL
                }
                delete stBottom;
            } // stBottom != NULL
        } // Found opening on top 
    }
    delete st;
    return resChar;
} // test as digit

// Just tests for a little ledge on top / bottom
bool SingleLetterTestAsI(OCRRectPtr r, CGRect rect, OCRResults *results) {
    
    bool res = false;
    CGRect rTop (rectLeft(r->rect), rectBottom(r->rect), r->rect.size.width, r->rect.size.height * 0.50);
    CGRect rBottom (rectLeft(r->rect), rectBottom(r->rect) + r->rect.size.height * 0.50, r->rect.size.width, r->rect.size.height * 0.50);
    SingleLetterTests *stTop = CreateSingleLetterTests(rTop, results);
    SingleLetterTests *stBottom = CreateSingleLetterTests(rBottom, results);
    if ((stTop != NULL) && (stBottom != NULL)) {
        OpeningsTestResults resLeftBottom;
        bool successLeftBottom = stBottom->getOpenings(resLeftBottom, SingleLetterTests::Left,
                                                       0.50,
                                                       1.00,
                                                       SingleLetterTests::Unbound,
                                                       SingleLetterTests::Bound);
        if (successLeftBottom) {
            OpeningsTestResults resRightBottom;
            bool successRightBottom = stBottom->getOpenings(resRightBottom, SingleLetterTests::Right,
                                                            0.50,
                                                            1.00,
                                                            SingleLetterTests::Unbound,
                                                            SingleLetterTests::Bound);
            if (successRightBottom) {
                OpeningsTestResults resLeftTop;
                bool successLeftTop = stTop->getOpenings(resLeftTop, SingleLetterTests::Left,
                                                         0.00,
                                                         0.50,
                                                         SingleLetterTests::Bound,
                                                         SingleLetterTests::Unbound);
                if (successLeftTop) {
                    OpeningsTestResults resRightTop;
                    bool successRightTop = stTop->getOpenings(resRightTop, SingleLetterTests::Right,
                                                              0.00,
                                                              0.50,
                                                              SingleLetterTests::Bound,
                                                              SingleLetterTests::Unbound);
                    if (successRightTop) {
                        res = true;
                    }
                }
            }
        }
        delete stTop; delete stBottom;
    }
    
    return res;
} // SingleLetterTestAsI

// Returns a score for a letter as B. Called with 'E' for now
float SingleLetterTestAsB(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_B))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_AS_B; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    ConnectedComponentList cpl = st->getConnectedComponents();
    // Do we have a single connected component ?
    if (!validateConnectedComponents(cpl, rect) || ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.05))) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    ConnectedComponentList invertCpl = st->getInverseConnectedComponents();
    if ((invertCpl.size() < 3) 
        || ((invertCpl.size() > 3) && (invertCpl[3].area > invertCpl[2].area * 0.05))) {
        if (needToRelease)
            delete st;
        return 0;
    } 
    // Require above each other + X overlap
    ConnectedComponent large = invertCpl[1];
    ConnectedComponent small = invertCpl[2];
    ConnectedComponent top = ((large.ymin < small.ymin)? large:small);
    ConnectedComponent bottom = ((large.ymin < small.ymin)? small:large);
    
    // Require no Y overlap
    if (top.ymax >= bottom.ymin) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    // Holes need to be not too far in size
    if (large.area > small.area * 1.75) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    if (!OCRUtilsOverlapping(bottom.xmin, bottom.xmax, top.xmin, top.xmax)) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    // 2 segments on top + 2 segments on bottom
    SegmentList slTop = st->getHorizontalSegments(0.22, 0.03);
    SegmentList slBottom = st->getHorizontalSegments(0.72, 0.03);
    if ((slTop.size() != 2) || (slBottom.size() != 2)) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    
    float resScore = 0.90;
    newChar = 'B';
    
    if (needToRelease)
        delete st;

    return resScore;
} // testasB


/* Returns 'o' if this letters looks like an 'o' based on:
- no opening on left side
- don't have 2 holes (inverted components)
Otherwise returns 'a'. If it can't decide because it can't perform the tests, returns '\0'
 For now called only when we see a 'o' (if we found open o's on the line) or a 'a'
- lineHasRealAs: indicates if we found a 'a' with a real opening on the left side. In this case, we'll assume 'a's are not the round kind and any round pattern will be assumed to be a 'o' even if it has a dent on the bottom right
 */
bool SingleLetterTestAsa(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, wchar_t &newCh, bool lineHasRealAs, bool *isRealA, bool strict, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags2 & FLAGS2_TESTED_AS_a))
        return false;
    
    r->flags2 |= FLAGS2_TESTED_AS_a;
    
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(r->rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE|SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return false;
    }
            
    ConnectedComponentList cpl = st->getConnectedComponents();
        // No components found (should always be 2 or more)
    if ((cpl.size() < 2)
        // Too many (and 3rd is significant)
        || ((cpl.size() >= 3) && (cpl[2].area > cpl[1].area * 0.10))
        // Main comp is less than 75% of area
        || (cpl[1].getWidth() * cpl[1].getHeight() < r->rect.size.width * r->rect.size.height * 0.75)) {
        if (needToRelease)
            delete st;
        return false;
    }
    
    ConnectedComponent mainCC = cpl[1];
    SingleLetterPrint(cpl, r->rect);
    
    wchar_t resCh = '\0';
    if (isRealA != NULL) *isRealA = false;
    
    bool gluedToPrevious = false, gluedToNext = false;

    if ((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0))
        gluedToPrevious = true;
    
    if ((r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0))
        gluedToNext = true;
    
    // If we have a very convincing inverted component, it's a 'o'
    ConnectedComponentList invertCpl = st->getInverseConnectedComponents();
    SingleLetterPrint(invertCpl, r->rect);
    if (invertCpl.size() == 2) {
        ConnectedComponent hole = invertCpl[1];
        SingleLetterPrint(hole, r->rect);
        if ((mainCC.getWidth() - hole.getWidth() < mainCC.getWidth() * 0.65)
            && (mainCC.getHeight() - hole.getHeight() < mainCC.getHeight() * 0.45)
            && (hole.area > mainCC.getWidth() * mainCC.getHeight() * 0.20))
        {
            newCh = 'o';
            // Not so fast! Even with a nice hole, could be a 'a' of the kind that looks like a 'o' but with a little dent at the bottom on the right side
            // If line has real a's (with an opening on the left side), don't risk calling this closed pattern a 'a' - unlikely
            // Abort if glued to next, could get notches just from glued pixels from the next letter. TODO: improve by testing if the next pattern actually touches of rects overlap without touching.
            if (!lineHasRealAs && !gluedToNext) {
                char tmpCh = SingleLetterTestoAsa(r, results, &st);
                if (tmpCh == 'a')
                    newCh = 'a';
            }
            
            if (needToRelease)
                delete st;
            return true;
        }
    }
    
    // Abort if glued to left, can't reliably find an opening
    if (gluedToPrevious) {
        newCh = '\0';
        if (needToRelease)
            delete st;
        return false;
    }
    
    bool lax = false;
    if (r->ch == 'a')
        lax = true; // If testing a 'a' look for an opening in a wider range, to give it a maximum chance of passing the opening test
    
    OpeningsTestResults leftOpening;
    bool successLeft = st->getOpenings(leftOpening, 
                    SingleLetterTests::Left, 
                    (lax? 0.0:0.10),      // Start of range to search (top/left)
                    (lax? 0.70:0.50),      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
    if (successLeft && (leftOpening.maxDepth > cpl[1].getWidth() * 0.10)) {
        // One more test: look for at least 3 segments along the X axis
        SegmentList sl = st->getVerticalSegments(0.50, 0.10);
        if (sl.size() >= 3) {
            // Opening on the left side + 3 vertical segments, looks like a 'a'
            if (isRealA != NULL) *isRealA = true;
            resCh = 'a';
        }
    } else {
        // No opening on the left side, likely a 'o'
        // Don't include background and accept any hole greater than 1% in size
        ConnectedComponentList invertCpl = st->getInverseConnectedComponents(false, 0.01);
        if (invertCpl.size() < 3) {
            // One last chance: if there is no hole or there is one but it's at the bottom and there is a tiny opening on top, don't change 'a' to 'o', just say we don't know
            if (invertCpl.size() < 2) {
                if (needToRelease)
                    delete st;
                return false;
            } else if (successLeft && ((invertCpl[1].ymin + invertCpl[1].ymax) * 0.50 > cpl[1].getHeight() * 0.55)) {
                // Opening (even if less than 10%) and a hole much below mid-height => probably a 'a' damaged by image processing but let's just say we don't know
                if (needToRelease)
                    delete st;
                return false;
            }
            DebugLog("Validate: SingleLetterTestAsa, testing [%c], no opening on left side, return as [o]", (unsigned short)r->ch);
            resCh = 'o';
            // TODO could it be right to test the below as 'a' always (and not just when replacing a 'u'
            if (!lineHasRealAs && !gluedToNext && (r->ch == 'u')) {
                char tmpCh = SingleLetterTestoAsa(r, results, &st);
                if (tmpCh == 'a')
                    resCh = 'a';
            }
        } else {
            // Instead of declaring that we don't know what it is, call it a 'a' if low letter and narrower on top than bottom
            resCh = '\0';
            if ((invertCpl.size() == 3) && (tallHeightTest(statsWithoutCurrent.getPtr(), r->rect.size.height, '\0', false, 1, true) == 0)) {
                // Also check holes are above each other
                ConnectedComponent topHole;
                ConnectedComponent bottomHole;
                if (invertCpl[1].ymin < invertCpl[2].ymin) {
                    topHole = invertCpl[1];
                    bottomHole = invertCpl[2];
                } else {
                    topHole = invertCpl[2];
                    bottomHole = invertCpl[1];
                }
                    // Entirely above
                if ((topHole.ymax < bottomHole.ymin)
                    // Horizontal overlap
                    && OCRUtilsOverlapping(topHole.xmin, topHole.xmax, bottomHole.xmin, bottomHole.xmax)
                   )
                {
                    SegmentList slTop = st->getHorizontalSegments(0.15, 0.30);
                    SegmentList slBottom = st->getHorizontalSegments(0.85, 0.30);
                    if ((slTop.size() >= 1) && (slBottom.size() >= 1) && (slTop[0].endPos - slTop[0].startPos < slBottom[0].endPos - slBottom[0].startPos)) {
                        resCh = 'a';
                    }
                } // horizontal overlap + above each other 
            } // not tall
#if DEBUG            
            if (resCh == '\0') {
                DebugLog("Validate: SingleLetterTestAsa - cancelling [%c] to [o] replace, found %d invert connected components", (unsigned short)r->ch, (unsigned short)invertCpl.size());
            } else {
                DebugLog("Validate: SingleLetterTestAsa - special closed [a] detection (two holes + narrower on top)");
            }
#endif // DEBUG            
        }
    }
    
    if (needToRelease)
        delete st;
    if (resCh != '\0') {
        newCh = resCh;
        return true;
    }
    return false;
}

// Returns '\0' if it can't decide anything (e.g. single letter test not possible)
// lax is not used at this time, nRects should just be 1
// So far used to test:
// - S ans s
// - weird multi-char combo instead of '3'
// - 3 (instead of 8)
// - tall 'a'
// - 5 vs 6
// - 21 instead of 8
char SingleLetterTestSAsDigit(OCRRectPtr r, CGRect rect, int nRects, OCRResults *results, bool lax) {
    if (!results->imageTests)
        return '\0';
    if (r->flags2 & FLAGS2_TESTED_S_AS_DIGIT)
        return (r->ch);

    // If original char is a 'S' use neighbor4=true because we want to give 'S' the benefit of the doubt by treating a diagonal connection as open, i.e. a legit 'S'
    SingleLetterTests *st = CreateSingleLetterTests(rect, results, ((r->ch == 'S')? true:false));
    
    r->flags2 |= FLAGS2_TESTED_S_AS_DIGIT;
    
    if (st == NULL)
        return '\0';
            
    ConnectedComponentList cpl = st->getConnectedComponents();
    
    ConnectedComponent mainCC;
    // If there is a significant component other than main, abort
    if ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.10)) {
        DebugLog("SingleLetterTestAsDigit: aborting because 2nd comp has area=%d vs 1st comp area=%d [%.2f%%]", cpl[2].area, cpl[1].area, (float)cpl[2].area/(float)cpl[1].area);
        if (st != NULL) delete st;
        return '\0';
    } else if (cpl.size() < 2) {
        DebugLog("SingleLetterTestAsDigit: aborting because lacking normal comps");
        if (st != NULL) delete st;
        return '\0';
    }
    
    mainCC = cpl[1];
    char resChar = '\0';
    
    // Start with test if we have two inverted components above each other (=> 8)
    ConnectedComponentList invertcpl = st->getInverseConnectedComponents();
    bool got2Holes = false;
    bool got1Hole = false;
    bool oneHoleAbove = false;
    ConnectedComponent hole1;
    float centerYHole1 = -1.0;
    if (invertcpl.size() == 2) {
        got1Hole = true;
        hole1 = invertcpl[1];
        centerYHole1 = (hole1.ymin + hole1.ymax) / 2;
        if (centerYHole1 < mainCC.getHeight() * 0.50)
            oneHoleAbove = true;
        else
            oneHoleAbove = false;
    }
    // Check if we have just 2 inverted components => OK
    else if (invertcpl.size() == 3)
        got2Holes = true;
    else if (invertcpl.size() > 3) {
        // Give it a chance if all inverted comps after the 2nd one are contained within main comps we decided to ignore
        got2Holes = true;
        for (int i=3; i<invertcpl.size(); i++) {
            ConnectedComponent invToTest = invertcpl[i];
            for (int j=1; j<3 && j<cpl.size(); j++) {
                // Is this hole contained in the first 2 connected comps?
                ConnectedComponent containingComp = cpl[j];
                if ((invToTest.xmin >= containingComp.xmin) && (invToTest.xmax <= containingComp.xmax)
                    && (invToTest.ymin >= containingComp.ymin) && (invToTest.ymax <= containingComp.ymax)) {
                    got2Holes = false;
                    break;
                }
            }
            if (!got2Holes)
                break;
        } 
    }
    ConnectedComponent topHole;
    ConnectedComponent bottomHole;
    if (got2Holes) {
        if (invertcpl[1].ymin > invertcpl[2].ymin) {
            topHole = invertcpl[2];
            bottomHole = invertcpl[1];
        } else {
            topHole = invertcpl[1];
            bottomHole = invertcpl[2];
        }
        // At least one pixel separating entire top from entire bottom (and of course no overlap)
        if (topHole.ymax < bottomHole.ymin - 1) {
            float topHoleCenterY = (topHole.ymin + topHole.ymax) / 2;
            float bottomHoleCenterY = (bottomHole.ymin + bottomHole.ymax) / 2;
            int holesTotalSize = invertcpl[0].area;
            // Holes cover at least 10% of total main comp rect
            if ((holesTotalSize >= cpl[0].getHeight() * cpl[0].getWidth() * ((rect.size.height <= 10)? 0.08:0.10))
                // No hole greater than 3x the other hole
                && (((topHole.area > holesTotalSize * 0.25) && (bottomHole.area > holesTotalSize * 0.25))
                    // If very small font, accept cases where top hole has area 1 versus bottom hole area 3 - see Walmart frame https://drive.google.com/open?id=0B4jSQhcYsC9VUGdOMEZxTEFnTWM
                    || ((rect.size.height <= 10) && (topHole.area >= holesTotalSize * 0.25) && (bottomHole.area >= holesTotalSize * 0.25)))
                && (topHoleCenterY < cpl[0].getHeight() / 2)
                && (bottomHoleCenterY > cpl[0].getHeight() / 2)
                ) 
            {
                char newCh = '8';
                // NOT DONE - in some cases it's a '9' closed at the bottom
                // For now only test '6' as possible '8' so in that case don't run the below test
                if ((r->ch != '6') && (bottomHole.area < topHole.area * 0.60)) {
                    OpeningsTestResults leftOpening;
                    bool success = st->getOpenings(leftOpening, 
                        SingleLetterTests::Left, 
                        0.30,      // Start of range to search (top/left)
                        0.90,      // End of range to search (bottom/right)
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                        );
                    // Must have a opening on the left
                    if (success && (leftOpening.maxDepth > r->rect.size.width * 0.10)) {
                        OpeningsTestResults rightOpening;
                        bool success = st->getOpenings(rightOpening, 
                            SingleLetterTests::Right, 
                            0.30,      // Start of range to search (top/left)
                            0.90,      // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );       
                        if (!success 
                            || ((rightOpening.maxDepth < r->rect.size.width * 0.10) && (rightOpening.maxDepth < leftOpening.maxDepth))) {
                            newCh = '9';
                        }
                    }
                }
                // Same as '9' test but this time for '6'
                else if (topHole.area < bottomHole.area * 0.60) {
                    OpeningsTestResults rightOpening;
                    bool success = st->getOpenings(rightOpening,
                                       SingleLetterTests::Right,
                                       0.03,      // Start of range to search (top/left)
                                       0.40,      // End of range to search (bottom/right)
                                       SingleLetterTests::Bound,   // Require start (top/left) bound
                                       SingleLetterTests::Bound  // Require end (bottom/right) bound
                                       );
                    // Must have a opening on the right.
                    if (success && (rightOpening.maxDepth > r->rect.size.width * 0.10)) {
                        OpeningsTestResults leftOpening;
                        bool success = st->getOpenings(leftOpening,
                                                       SingleLetterTests::Left,
                                                       0.03,      // Start of range to search (top/left)
                                                       0.40,      // End of range to search (bottom/right)
                                                       SingleLetterTests::Bound,   // Require start (top/left) bound
                                                       SingleLetterTests::Bound  // Require end (bottom/right) bound
                                                       );
                        if (!success 
                            || ((leftOpening.maxDepth < r->rect.size.width * 0.10) && (leftOpening.maxDepth < rightOpening.maxDepth))) {
                            newCh = '6';
                            // We almost tagged this as an '8' so remember this
                            r->flags |= FLAGS_SUSPECT;
                            r->ch2 = '8';
                        }
                    }
                }
                delete st;
                return newCh;
            }
        }
    }
    
    // For now only test '6' as possible '8' (above), so if it didn't happen abort here
    if (r->ch == '6') {
        if (st != NULL) delete st;
        return '\0';
    }
    
    // Testing '9' (only if not testing an existing digit, because this test below is very partial and doesn't check the opening top-right at all
    if (got1Hole && (r->ch != 'S') && (r->ch != 's') && !isDigit(r->ch)) {
        if (centerYHole1 < rect.size.height * (1-OCR_YMIN_CENTER_HOLE_9)) {
            LimitedOpeningsTestResults leftBottomOpening;
            bool successLeftBottom = st->getOpeningsLimited(leftBottomOpening, 
                SingleLetterTests::Left, 
                (hole1.ymax + 1) / rect.size.height,      // Start of range to search (top/left)
                0.95,      // End of range to search (bottom/right)
                0.60,
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
            if (successLeftBottom && (leftBottomOpening.maxDepth > rect.size.width * 0.20)
                && (leftBottomOpening.minWidth > rect.size.height * 0.10)) {
                wchar_t newCh = '9';
                if (gapBelow(r, nRects) > OCR_MIN_GAP_BELOW_LINE_g * rect.size.height)
                    newCh = 'g';
                return newCh;
            }
        }
    }
    
    if (r->ch == '3') {
        // Demand one hole (top or bottom)
        if (!got1Hole) {
            delete st;
            return (r->ch);
        }
        
        // 2015-05-11 Blink actually returned '3' for a perfect '0', test for it now
        if ((invertcpl[1].getHeight() > mainCC.getHeight() * 0.65) // Tall
            && (invertcpl[1].xmin + invertcpl[1].getHeight()/2 > mainCC.getHeight() * 0.35) // Center not close to bottom
            && (invertcpl[1].xmin + invertcpl[1].getHeight()/2 < mainCC.getHeight() * 0.65)) { // Center not close to top
            // One more test: check mniddle horizontal segment and demand that center of main CC be between left and right segments
            SegmentList slMiddle = st->getHorizontalSegments(0.50, 0.20);
            if ((slMiddle.size() == 2) && (mainCC.xmin + mainCC.getWidth()/2 > slMiddle[0].endPos) && (mainCC.xmin + mainCC.getWidth()/2 < slMiddle[1].startPos)) {
                delete st;
                return ('0');
            }
        }
        
        // Get thickness of bottom/top stroke
        SegmentList slMiddleVert = st->getVerticalSegments(0.50, 0.15);
        
        float doit = false;
        // Expect at least 3 - bottom, middle, top
        if (slMiddleVert.size() >= 3) {
            if (oneHoleAbove) {
                Segment slBottom = slMiddleVert[slMiddleVert.size()-1];
                if (slBottom.endPos > rect.size.height * 0.90) {
                    SegmentList slMiddle1 = st->getHorizontalSegments(0.63, 0.01);
                    SegmentList slMiddle2 = st->getHorizontalSegments(0.77, 0.01);
                    if ((slMiddle1.size() == 2) && (slMiddle2.size() == 2) && (slMiddle1[0].startPos < r->rect.size.width * 0.25) && (slMiddle2[0].startPos < r->rect.size.width * 0.25)) {
                        doit = true;
                    }
                }
            } else {
                Segment slTop = slMiddleVert[0];
                if (slTop.startPos < rect.size.height * 0.10) {
                    SegmentList slMiddle1 = st->getHorizontalSegments(0.37, 0.01);
                    SegmentList slMiddle2 = st->getHorizontalSegments(0.23, 0.01);
                    if ((slMiddle1.size() == 2) && (slMiddle2.size() == 2) && (slMiddle1[0].startPos < r->rect.size.width * 0.25) && (slMiddle2[0].startPos < r->rect.size.width * 0.25)) {
                        doit = true;
                    }
                }
            }
        }
        delete st;
        if (doit) {
            return '8';
        } else {
            return (r->ch);
        }
    }
    
    //if (lax && ((r->ch =='s') || (r->ch =='S'))) {
    // Always test for '3'
    // Possible '3'
    // Bad '5' that was actually a '3'
    
    // Expect and locate the indentation of the '3' on the right
    OpeningsTestResults rightOpening;
    bool success = st->getOpenings(rightOpening, 
                    SingleLetterTests::Right, 
                    0,      // Start of range to search (top/left)
                    0.50,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
    // Indentation on the right had depth of just 1 pixel vs width=16 (=>6.2%)
    if (success && (rightOpening.maxDepth > cpl[1].getWidth() * 0.05)) {
        // OK, now look for an indentation on the left ABOVE the one we just found
        OpeningsTestResults leftOpening;
        float maxYTop = rightOpening.maxDepthCoord - 1;
        if (maxYTop <= 0)
            maxYTop = 1;
        bool success = st->getOpenings(leftOpening, 
                    SingleLetterTests::Left, 
                    0,      // Start of range to search (top/left)
                    maxYTop / r->rect.size.height,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
        if (success && (leftOpening.maxDepth > cpl[1].getWidth() * 0.40)) {
            // If orig char was a 'S' no need to test an opening on bottom. But if '8' we do need to test
            if (r->ch != '8') {
                delete st;
                return '3';
            } else {
                OpeningsTestResults leftBottomOpening;
                float minYBottom = rightOpening.maxDepthCoord + 1;
                if (minYBottom >= r->rect.size.height)
                    minYBottom = r->rect.size.height - 1;
                bool success = st->getOpenings(leftBottomOpening, 
                            SingleLetterTests::Left, 
                            minYBottom / r->rect.size.height,      // Start of range to search (top/left)
                            1.00,      // End of range to search (bottom/right)
                            SingleLetterTests::Bound,   // Require start (top/left) bound
                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                            );
                if (success && (leftBottomOpening.maxDepth > cpl[1].getWidth() * 0.40)) {
                    delete st;
                    return '3';                            
                }
            }
        }
    }
    
    // Not a '3'
            
    OpeningsTestResults leftBottomOpening;
    bool successLeftBottom = st->getOpenings(leftBottomOpening, 
            SingleLetterTests::Left, 
            // Using 0.50 and not 0.35 to avoid detecting the small gap in the middle of '8' as a valid opening
            // If orig ch is '5', start higher to make sure not to miss the opening
            ((r->ch == '5')? 0.40:0.50),      // Start of range to search (top/left)
            1,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
    // Legit '5' had a leftDepth of 4 with width 17 => 23.5%
    if (!successLeftBottom || (leftBottomOpening.maxDepth < cpl[1].getWidth() * 0.12)) {
        // If '5' and we found ANY opening, let it be. Test below for '8' will fail because it will find an opening on top
        if (r->ch == '5') {
            if (successLeftBottom) {
                resChar = '5';
                if (got1Hole)
                    r->flags |= FLAGS_SUSPECT;  // Mark as such for the sake of the UPC fixing code later
            }
            // 1 pixel opening out of width 15 in a '6' with a perfect hole
            else if ((leftBottomOpening.maxDepth < cpl[1].getWidth() * 0.10) && got1Hole
                && (centerYHole1 > cpl[1].getHeight() * 0.60)
                && (invertcpl[1].getHeight() > cpl[1].getHeight() * 0.20)) {
                resChar = '6';
            } else if (got1Hole) {
                r->ch2 = '6';
            }
        }
        // If source character is a 'a', absence of an opening left bottom is expected and NOT a reason to make a change
        else if ((r->ch != 'a') && got1Hole && !oneHoleAbove) {
            // Could still be an '8' but certainly not a '5' so mark a suggested change
            resChar = '6';
        }
        // Possible '8'
        // TODO: not sure why we are still testing for '8' here even though we already did at the top of this function ...
        else if (got2Holes && (invertcpl[2].area > invertcpl[1].area * 0.25)) {
            float centerTop = (topHole.ymin + topHole.ymax) / 2;
            float centerBottom = (bottomHole.ymin + bottomHole.ymax) / 2;
            // One hole above middle, other below middle
            if ((centerBottom > mainCC.getHeight() * 0.50) && (centerTop < mainCC.getHeight() * 0.50))
            {
                OpeningsTestResults rightTopOpening;
                bool success = st->getOpenings(rightTopOpening, 
                    SingleLetterTests::Right, 
                    0,      // Start of range to search (top/left)
                    // 0.40 to avoid matching the gap on the left side of 8 as an opening
                    0.29,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (!success || (rightTopOpening.maxDepth < cpl[1].getWidth() * 0.12)) {
                    // Note: here we may convert a 'a' into a '8'. A bit risky because some 'a's are closed on top and could create two inverted components - but remember we evaluate 'a' as digit only when there is some cause for it.
                    resChar = '8';
                }
            }
        } else {
            // Possible E
            SegmentList sl = st->getVerticalSegments(0.50, 0.01);

            // Expect 3 segments
            if (sl.size() == 3) {
                Segment sMiddle = sl[1];
                if (sMiddle.endPos > r->rect.size.height * 0.50) {
                    LimitedOpeningsTestResults rightBottomOpening;
                    bool success = st->getOpeningsLimited(rightBottomOpening, 
                        SingleLetterTests::Right, 
                        sMiddle.endPos / r->rect.size.height,      // Start of range to search (top/left)
                        0.95,      // End of range to search (bottom/right)
                        0.30,
                        SingleLetterTests::Bound,   // Require start (top/left) bound
                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                    // Require a wide opening
                    if (success && (rightBottomOpening.maxDepth > cpl[1].getWidth() * 0.20) && (rightBottomOpening.minWidth > cpl[1].getHeight() * 0.07)) {
                        resChar = 'E';
                    }
                } // got middle segment
            }
        } // open on top right 
    } else {
        if ((r->ch == '5') && got1Hole) {
            r->flags |= FLAGS_SUSPECT;  // Eeven though we decided not to replace, mark as suspect for the sake of a possible UPC fix later
            r->ch2 = '6';
        }
    }
                    
    delete st;
    return resChar;
} // SingleLetterTestSAsDigit


/* Returns a score for a letter like W. Possible returned letters:
- W or w
Called with:
- U+vertical line
*/
float SingleLetterTestW(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags5 & FLAGS5_TESTED_AS_w))
        return -1.0;
    r->flags5 |= FLAGS5_TESTED_AS_w; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE|SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    SegmentList sl = st->getHorizontalSegments(0.50, 0.05);
    SingleLetterPrint(sl, rect.size.width);
    if (sl.size() < 3) {
        if (needToRelease)
            delete st;            
        return -1.0;
    }
    
    float resScore = 0.0;
    
    float startGap1 = sl[0].endPos;
    float endGap1 = sl[1].startPos;
    float startGap2 = sl[sl.size()-2].endPos;
    float endGap2 = sl[sl.size()-1].startPos;
    
    // Test openings
    OpeningsTestResults openingTopLeft;
    bool successTopLeft = st->getOpenings(
        openingTopLeft, SingleLetterTests::Top, 
        startGap1 / rect.size.width, 
        endGap1 / rect.size.width, 
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound 
        );  
    if (successTopLeft) {
        OpeningsTestResults openingTopRight;
        bool successTopRight = st->getOpenings(
            openingTopRight, SingleLetterTests::Top, 
            startGap2 / rect.size.width, 
            endGap2 / rect.size.width, 
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound  // Require end (bottom/right) bound 
            );     
        if (successTopRight) {
            // U + I touched above. Need to also make sure there is no deep opening from the bottom
            OpeningsTestResults openingBottom;
            bool successBottom = st->getOpenings(
                openingBottom, SingleLetterTests::Bottom,
                0.05,
                0.95,
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound  // Require end (bottom/right) bound
            );
            if (!successBottom || (openingBottom.maxDepth < rect.size.height * 0.25)) {
                newChar = 'W';
                if (tallHeightTest(stats.getPtr(), rect.size.height, newChar, false, 0, true) == 0) {
                    newChar = 'w';
                }
                resScore = 1.0;
            }
#if DEBUG
            else {
                DebugLog("SingleLetterTestW: NOT validating 'W', found deep gap at bottom [%%%.2f of height]", (openingBottom.maxDepth / rect.size.height) * 100);
            }
#endif
        }
    }
    
   if (needToRelease)
        delete st;            

    return resScore;
} // testW

wchar_t SingleLetterTestAsi(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results) {

    SingleLetterTests *st = CreateSingleLetterTests(rect, results);
    if (st == NULL) {
        return '\0';
    }
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    if (cpl.size() != 3) {
        return '\0';
    }
    
    // We have two components!
    ConnectedComponent stemCC = cpl[1];
    ConnectedComponent dotCC = cpl[2];
    
    bool doit = true;
    
    // Dot not below stem
    if (dotCC.ymax >= stemCC.ymin)
        doit = false;
    
    // Stem at least 2x dot
    if (doit && dotCC.getHeight() > stemCC.getHeight() * 0.50)
        doit = false;
    
    // Dot not more than 1.5 x wider than tall (unless very small). OK if small dot (say 3x2 but not 3x1 or 4x2)
    if (doit && (dotCC.getWidth() > dotCC.getHeight() * 1.5) && ((dotCC.getWidth() > 3) || (dotCC.getWidth() >= dotCC.getHeight() * 2))) {
        doit = false;
    }
    
    // Gap with dot not more than height of the stem
    if (doit && (stemCC.ymin - dotCC.ymax > stemCC.getHeight()))
        doit = false;
    
    // Require that the stem be of height similar to normal lowercase letters
    if (doit && tallHeightTest(stats.getPtr(), stemCC.getHeight(), 0, false, 3, true) == 1)
        doit = false;
    
    // It's important not to mistakenly tag as 'i' a case where we have a slanted 'l' and a spot above from a previous letter overlapping
    // No worries if much heigher than it is wide (means not very slanted)
    if (doit && !(rect.size.height > rect.size.width * 5)) {
        SegmentList slStemTop = st->getHorizontalSegments(stemCC.ymin / rect.size.height + 0.05, 0.10);
        SegmentList slStemBottom = st->getHorizontalSegments(0.95, 0.10);
        if (slStemTop.size() != 1)
            doit = false;
        if (slStemBottom.size() != 1)
            doit = false;
        Segment stemTop;
        Segment stemBottom;
        bool slantedRight;
        if (doit) {
            stemTop = slStemTop[0];
            stemBottom = slStemBottom[0];
            slantedRight = (stemTop.endPos > stemBottom.endPos) && (stemTop.startPos > stemBottom.startPos);
            if (slantedRight && dotCC.xmax < stemTop.startPos)
                doit = false;
        }
    }
    
    delete st;
    
    if (doit) {
        return 'i';
    } else {
        return '\0';
    }
} // testAsi


// Returns 'G' or 'C' (curved) if it detects it, '\0' otherwise
// - forceChange = true when caller is certain OCR guess is wrong, i.e. we should suggest a replacement even if not perfect
wchar_t SingleLetterTestAsGC(OCRRectPtr r, CGRect rect, OCRResults *results, bool forceChange, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags3 & FLAGS3_TESTED_AS_G))
        return '\0';
    r->flags3 |= FLAGS3_TESTED_AS_G;
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    ConnectedComponentList cpl = st->getConnectedComponents();
    // Do we have a single connected component ?
    if (!validateConnectedComponents(cpl, rect) || ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.05))) {
        if (needToRelease)
            delete st;
        return '\0';
    }
    
    wchar_t resChar = '\0';
    
    bool hasOpeningRight = false;

    SegmentList slRight = st->getVerticalSegments(0.925, 0.15);
#if DEBUG
    SingleLetterPrint(slRight, rect.size.height);
#endif
    if (slRight.size() == 2) {
        // Now also require an opening from the right side
        OpeningsTestResults resRight;
        bool successRight = st->getOpenings(resRight, SingleLetterTests::Right,
                                            slRight[0].startPos / rect.size.height,      // Start of range to search (top/left)
                                            slRight[1].endPos / rect.size.height,    // End of range to search (bottom/right)
                                            SingleLetterTests::Bound,   // Require start (top/left) bound
                                            SingleLetterTests::Bound  // Require end (bottom/right) bound
                                            );
        
        hasOpeningRight = (successRight && (resRight.maxDepth > rect.size.width * (forceChange? 0.10:0.15)));
    }
    
    if (!hasOpeningRight) {
        if (needToRelease)
            delete st;
        return '\0';
    }
    
    CGRect upperHalf (rectLeft(rect), rectBottom(rect), rect.size.width, slRight[0].endPos + 1);
    CGRect lowerHalf (rectLeft(rect), rectBottom(rect) + slRight[1].startPos, rect.size.width, rect.size.height - slRight[1].startPos - 1);
    
    SingleLetterTests *stUpper = CreateSingleLetterTests(upperHalf, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE|SINGLE_LETTER_VALIDATE_SINGLE_COMP);
    SingleLetterTests *stLower = NULL;
    // Expect top half to have an oppening from bottom
    bool hasOpeningTopHalf = false;
    if (stUpper != NULL) {
        OpeningsTestResults resTopBottom;
        bool successTopBottom = stUpper->getOpenings(resTopBottom, SingleLetterTests::Bottom,
                                        0,      // Start of range to search (top/left)
                                        1,    // End of range to search (bottom/right)
                                        SingleLetterTests::Bound,   // Require start (top/left) bound
                                        SingleLetterTests::Bound  // Require end (bottom/right) bound
                                        );
    
        hasOpeningTopHalf = (successTopBottom && (resTopBottom.maxDepth > rect.size.height * (forceChange? 0.15:0.20)));
    }
    
    bool hasOpeningBottomHalf = false;
    if (hasOpeningTopHalf) {
        stLower = CreateSingleLetterTests(lowerHalf, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE|SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        // Expect lower half to have an oppening from top
        if (stLower != NULL) {
            OpeningsTestResults resBottomTop;
            bool successBottomTop = stLower->getOpenings(resBottomTop, SingleLetterTests::Top,
                                                         0,      // Start of range to search (top/left)
                                                         1,    // End of range to search (bottom/right)
                                                         SingleLetterTests::Bound,   // Require start (top/left) bound
                                                         SingleLetterTests::Bound  // Require end (bottom/right) bound
                                                         );
            
            hasOpeningBottomHalf = (successBottomTop && (resBottomTop.maxDepth > rect.size.height * (forceChange? 0.15:0.20)));
        }
    }
    
    if (hasOpeningTopHalf && hasOpeningBottomHalf) {
        SegmentList slTopBottom = stUpper->getHorizontalSegments(0.92, 0.16);
#if DEBUG
        SingleLetterPrint(slTopBottom, rect.size.width);
#endif
        SegmentList slBottomTop = stLower->getHorizontalSegments(0.08, 0.16);
#if DEBUG
        SingleLetterPrint(slBottomTop, rect.size.width);
#endif
        // Require that the bottom half has a wider ledge than the right leg of the upper part
        if ((slTopBottom.size() >= 2) && (slBottomTop.size() >= 2) && (slBottomTop[slBottomTop.size()-1].endPos - slBottomTop[slBottomTop.size()-1].startPos + 1 > (slTopBottom[slTopBottom.size()-1].endPos - slTopBottom[slTopBottom.size()-1].startPos + 1) * 1.10)) {
            resChar = 'G';
        } else {
            // Not larger - probably a 'C' with curved top & bottom
            resChar = 'C';
        }
    }

    if (needToRelease)
        delete st;
    if (stUpper != NULL)
        delete stUpper;
    if (stLower != NULL)
        delete stLower;
    
    return resChar;
} // testAsGC

// Returns 'P' or 'F' if it detects these, '\0' otherwise
// - forceChange = true when caller is certain OCR guess is wrong, i.e. we should suggest a replacement even if not perfect
// Called with '?'
// Sometimes 'P' and a 'F' are returned as "I'"
wchar_t SingleLetterTestAsFP(OCRRectPtr r, CGRect rect, OCRResults *results, bool forceChange, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_P) || (r->flags2 & FLAGS2_TESTED_AS_F))
        return '\0';
    r->flags6 |= FLAGS6_TESTED_AS_P;
    r->flags2 |= FLAGS2_TESTED_AS_F;
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    ConnectedComponentList cpl = st->getConnectedComponents();
    // Do we have a single connected component ?
    if (!validateConnectedComponents(cpl, rect) || ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.05))) {
        if (needToRelease)
            delete st;
        return '\0';
    }
    
    bool failedTest = false;    // Set this to true if we decide at any point that the test failed (to allow it to skip further tests and just clean up & exit)
    
    // Expect left side to be from top to bottom
    bool requiredHasStraightLeftSide = false;
    SegmentList slLeft = st->getVerticalSegments(0.125, 0.25);
    if ((slLeft.size() != 1) || ((slLeft[0].endPos - slLeft[0].startPos + 1) < rect.size.height * 0.90)) {
        // Be conservative - no straight left side, assume it's not 'P' or 'F'
        failedTest = true;
    } else {
        requiredHasStraightLeftSide = true;
    }
    
    // Expect bottom to stop before 70% mark
    bool requiredBottomOnLeftSide = false;
    if (!failedTest) {
        SegmentList slBottom = st->getHorizontalSegments(0.95, 0.10);
#if DEBUG
        SingleLetterPrint(slBottom, rect.size.width);
#endif
        if ((slBottom.size() == 1) && (slBottom[0].endPos < rect.size.width * 0.70)) {
            requiredBottomOnLeftSide = true;
        } else {
            failedTest = true;
        }
    }

    // Bubble top-right (i.e. 'P')?
    bool hasBubbleTopRight = false;
    bool hasBubble = false;
    if (!failedTest) {
        ConnectedComponentList invertCpl = st->getInverseConnectedComponents();
#if DEBUG
        SingleLetterPrint(invertCpl, rect);
#endif
        if ((invertCpl.size() < 2) || ((invertCpl.size() > 2) && invertCpl[2].area >  invertCpl[1].area * (forceChange? 0.10:0.05))) {
            hasBubbleTopRight = false;
        } else {
            hasBubble = true;
                // Hole below mid-height
            if (((invertCpl[1].ymin + invertCpl[1].ymax)/2 > rect.size.height * 0.50)
                // Hole before mid-width
                || ((invertCpl[1].xmin + invertCpl[1].xmax)/2 < rect.size.width * 0.50)) {
                hasBubbleTopRight = false;
            } else {
                hasBubbleTopRight = true;
            }
        }
    }
    
    bool hasOpeningRight = false;
    if (!failedTest && !hasBubble) {
        // Not a 'P', could be a 'F'
        SegmentList slRight = st->getVerticalSegments(0.925, 0.15);
#if DEBUG
        SingleLetterPrint(slRight, rect.size.height);
#endif
        // Bottom of lower fork above 20% mark
        if ((slRight.size() == 2) && (slRight[1].endPos < rect.size.height * 0.80)) {
            // Now also require an opening from the right side
            OpeningsTestResults resRight;
            bool successRight = st->getOpenings(resRight, SingleLetterTests::Right,
                    slRight[0].startPos / rect.size.height,      // Start of range to search (top/left)
                    slRight[1].endPos / rect.size.height,    // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
            
            hasOpeningRight = (successRight && (resRight.maxDepth > rect.size.width * (forceChange? 0.10:0.15)));
        }
    }
    
    if (needToRelease)
        delete st;
    if (!failedTest) {
        if (hasBubbleTopRight || (hasBubble && forceChange))
            return 'P';
        else if (!hasBubble && hasOpeningRight)
            return 'F';
    }

    return '\0';
} // testAsFP


// Testing various chars as '6' by looking for an opening top right
// For now only used to test 'G'
// Params:
// - strict: if set, also expect an inverted connected component
// Returns:
// -1 if it can't decide anything (e.g. single letter test not possible), same char as input if not indications to the contrary (which doesn't mean it's not a mistake, only that we didn't find a flaw)
// 1 looks like a '6'
// 0 doesn't look like a '6'
int SingleLetterTestAs6(OCRRectPtr r,OCRResults *results, bool strict, SingleLetterTests *inputST) {
    if (!results->imageTests)
        return -1;
    if (r->flags3 & FLAGS3_TESTED_AS_6)
        return -1;
        
    r->flags3 |= FLAGS3_TESTED_AS_6;

    SingleLetterTests *st = ((inputST != NULL)? inputST:CreateSingleLetterTests(r->rect, results));
    
    if (st == NULL)
        return -1;
            
    // Make sure there is only one main component, otherwise abstain from correcting anything
    ConnectedComponentList cpl = st->getConnectedComponents();
        
    if ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.10))
        return -1;
        
    if (strict) {
        ConnectedComponentList invertedCpl = st->getInverseConnectedComponents();
        
        if (invertedCpl.size() < 2)
            return 0;
    }
    
    // Look for opening to the right
    OpeningsTestResults topRightOpening;
    bool success = st->getOpenings(topRightOpening, 
                    SingleLetterTests::Right, 
                    0.00,      // Start of range to search (top/left)
                    0.50,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
    
    if (!inputST) {
        delete st;
    } 
    
    if (success && (topRightOpening.maxDepth > cpl[1].getWidth() * 0.30)) {
        return 1;
    }
    
    //else
    return 0;
    
} // testAs6

/* Returns a score for a letter like O. Possible returned letters:
- O
- U
- R
Called with:
-  "II" or "il"
*/
float SingleLetterTestAsO(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrentAndNext, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_AS_O))
        return -1.0;
    r->flags6 |= FLAGS6_TESTED_AS_O; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1.0;
    }
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    // Do we have a single connected component ?
    if (!validateConnectedComponents(cpl, rect) || ((cpl.size() > 2) && (cpl[2].area > cpl[1].area * 0.05))) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    float resScore = 0;
    
    // 0.50: single hole
    // 0.25: spanning at least 50% of height
    // 0.125: center below 60% mark
    // 0.125: center above 40% mark
    
    bool hasHole;
    ConnectedComponent hole;
    float holeCenterY = 0.0;

    ConnectedComponentList invertCpl = st->getInverseConnectedComponents();
    if ((invertCpl.size() < 2) || ((invertCpl.size() > 2) && (invertCpl[2].area > invertCpl[1].area * 0.05)))
    {
        hasHole = false;
    } else {
        hasHole = true;
        hole = invertCpl[1];
        holeCenterY = (hole.ymin + hole.ymax) / 2;
    }   

    bool successTop = false;
    bool successBottom = false;
    LimitedOpeningsTestResults resTop;
    LimitedOpeningsTestResults resBottom;
    
    // Could be U or H!

    // TODO: use regular openings test when I fix the bug that returns sub-optimal openings
    successTop = st->getOpeningsLimited(resTop, SingleLetterTests::Top, 
        0.20,      // Start of range to search (top/left)
        0.95,    // End of range to search (bottom/right)
        0.35,
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
        
    bool hasOpeningTop = (successTop && (resTop.maxDepth > rect.size.height * 0.25));
    bool hasSmallOpeningTop = (successTop && (resTop.maxDepth > rect.size.height * 0.10));


    successBottom = st->getOpeningsLimited(resBottom, SingleLetterTests::Bottom, 
        0.20,      // Start of range to search (top/left)
        0.95,    // End of range to search (bottom/right)
        0.35,
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
        
    bool hasOpeningBottom = (successBottom && (resBottom.maxDepth > rect.size.height * 0.25));
    bool hasSmallOpeningBottom = (successBottom && (resBottom.maxDepth > rect.size.height * 0.10));
    
    bool successSproketBottomLeft = false;
    bool hasSproketBottomLeft = false; 
    
    bool successSproketBottomRight = false;
    bool hasSproketBottomRight = false; 
    // "11" or "ll" - need to test that we don't have small indents bottom left & right
    CGRect bottomRect(rectLeft(rect), rectBottom(rect) + rect.size.height * 0.70, rect.size.width, rect.size.height * 0.30);
    SingleLetterTests *stBottom = CreateSingleLetterTests(bottomRect, results);
    if (stBottom != NULL)
    {
        OpeningsTestResults resSprocketBottomRight;
        successSproketBottomRight = stBottom->getOpenings(resSprocketBottomRight, SingleLetterTests::Right,
                                                          0.00,      // Start of range to search (top/left)
                                                          1.00,    // End of range to search (bottom/right)
                                                          SingleLetterTests::Unbound,   // Require start (top/left) bound
                                                          SingleLetterTests::Bound  // Require end (bottom/right) bound
                                                          );
        hasSproketBottomRight = (successSproketBottomRight && (resSprocketBottomRight.maxDepth > rect.size.width * 0.06));
        
        if (hasSproketBottomRight) {
            OpeningsTestResults resSprocketBottomLeft;
            successSproketBottomLeft = stBottom->getOpenings(resSprocketBottomLeft, SingleLetterTests::Left,
                                                             0.00,      // Start of range to search (top/left)
                                                             1.00,    // End of range to search (bottom/right)
                                                             SingleLetterTests::Unbound,   // Require start (top/left) bound
                                                             SingleLetterTests::Bound  // Require end (bottom/right) bound
                                                             );
            hasSproketBottomLeft = (successSproketBottomLeft && (resSprocketBottomLeft.maxDepth > rect.size.width * 0.06));    
        }
        delete stBottom;
    }
    
    int tallTest = tallHeightTest(statsWithoutCurrentAndNext.getPtr(), rect.size.height, '\0', false, 0, false);
    if (!hasHole && hasOpeningTop && !hasOpeningBottom) {
        resScore = 0.90;
        newChar = 'u';
        if (tallTest) {
            if (hasSproketBottomRight && hasSproketBottomLeft) {
                if (needToRelease)
                    delete st;    
                return 0;
            }
            
            newChar = 'U';
        }
        if (needToRelease)
            delete st;            
        return resScore;
    } else if (!hasHole && !hasOpeningTop && hasOpeningBottom) {
        resScore = 0.90;
        newChar = 'n';
        if (needToRelease)
            delete st;        
        return resScore;
    } else if (!hasHole && hasOpeningTop && !hasOpeningBottom) {
        resScore = 0.90;
        newChar = 'u';
        if (tallTest)
            newChar = 'U';
        if (needToRelease)
            delete st;            
        return resScore;
    }
    
    // R
    if (hasOpeningBottom && hasHole && (holeCenterY < rect.size.height * 0.40) && (hole.getHeight() < rect.size.height * 0.55)) {
        resScore = 0.90;
        newChar = 'R';
        if (needToRelease)
            delete st;            
        return resScore;
    }
    
    // No hole and not the right top/bottom openings, bail out
    if (!hasHole) {
        if (needToRelease)
            delete st;
        return 0;
    }
    
    // Test for little bar in top-right corner
    SegmentList slBottom = st->getHorizontalSegments(0.96, 0.08);    
    SingleLetterPrint(slBottom, rect.size.width);
    if (((slBottom.size() == 1) || ((slBottom.size() == 2) && (slBottom[0].endPos-slBottom[0].startPos < rect.size.width * 0.05)))
         && (slBottom[slBottom.size()-1].startPos > rect.size.width * 0.60) && (slBottom[slBottom.size()-1].endPos > rect.size.width * 0.90)) {
        resScore = 0.90;
        newChar = 'Q';
        if (needToRelease)
            delete st;            
        return resScore;
    }
    
    resScore += 0.50;
    
    if (hole.getHeight() > rect.size.height * 0.50) {
        resScore += 0.25;
    }
    
    // Add to the score if center of hole is between 40% and 60%
    if ((holeCenterY > rect.size.height * 0.40) && (holeCenterY < rect.size.height * 0.60)) {
        resScore += 0.25;
    }
    
    // Remove 0.30 if top of hole is below 50% line or bottom of hole is above 50% from bottom
    // Tests that we are not looking at a 'b'
    if ((hole.ymin > rect.size.height * 0.50) || (hole.ymax < rect.size.height * 0.50)) {
        resScore -= 0.30; // Will make score fall below 0.50 unless large tall hole added 0.25 AND above rule caught
    }
    
    if (needToRelease)
        delete st;
    
    // Before returning 'o' (or variants), avoid if we have sprokets at the bottom
    if (hasSproketBottomLeft && hasSproketBottomRight) {
        return 0;
    }
    
    if (hasSmallOpeningBottom || hasSmallOpeningTop) {
        return 0;
    }
    
    if (resScore >= 0.50) {
        // From here we always return variants of o/0.
        int tallTest = tallHeightTest(statsWithoutCurrentAndNext.getPtr(), rect.size.height, 'O', false, 0, false);                               
        if (rect.size.height > rect.size.width * OCR_MAX_H_TO_W_RATIO_O) {
            if (tallTest == 0)
                tallTest = tallHeightTest(statsWithoutCurrentAndNext.getPtr(), rect.size.height, 'O', false, 0, true);
            if (tallTest != 0)
                newChar = '0';
            else 
                newChar = 'o';
        } else {
            if (tallTest > 0)
                newChar = 'O';
            else
                newChar = 'o';
        }
    } else {
        return 0;
    }
    return resScore;
} // testasO

// Only called with 'O' for now
// Returns 1 and sets output char to 'D' if clear 'D' (IMPORTANT: assumes letter is tall and looks like 'O')
int SingleLetterTestAsD(OCRRectPtr r, CGRect rect, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags4 & FLAGS4_TESTED_AS_D))
        return -1.0;
    r->flags4 |= FLAGS4_TESTED_AS_D;
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true, SINGLE_LETTER_VALIDATE_COMP_SIZE | SINGLE_LETTER_VALIDATE_SINGLE_COMP);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return -1;
    }
    
    int res = -1;
    
    // Top / bottom: slice of thickness 2 (out of height 25 => 0.08%) would yield a horiz segment 22 wide (out of 25) => 88% (use 85%)
    // Left / right: slice of thickness 2 (out of width 33 => 0.06%, use 0.08%) would yield a vert segment 27 wide (out of width 33) => 81.8%, use 80%
    SegmentList slTop = st->getHorizontalSegments(0.04, 0.08);
    SegmentList slVeryTop = st->getHorizontalSegments(0.025, 0.05);
    SegmentList slBottom = st->getHorizontalSegments(0.96, 0.08);
    SegmentList slVeryBottom = st->getHorizontalSegments(0.985, 0.05);
    SegmentList slLeft = st->getVerticalSegments(0.04, 0.08);
    SegmentList slRight = st->getVerticalSegments(0.96, 0.08);

    if ((slTop.size()==1) && (slVeryTop.size()==1) && (slBottom.size()==1) && (slVeryBottom.size()==1) && (slLeft.size()==1) && (slRight.size()==1)) {
        float leftHeight = slLeft[0].endPos - slLeft[0].startPos + 1;
        float rightHeight = slRight[0].endPos - slRight[0].startPos + 1;
        float topWidth = slTop[0].endPos - slTop[0].startPos + 1;
        float veryTopWidth = slVeryTop[0].endPos - slVeryTop[0].startPos + 1;
        float topRightOffset = r->rect.size.width - slTop[0].endPos - 1;
        float veryTopRightOffset = r->rect.size.width - slVeryTop[0].endPos - 1;
        float bottomWidth = slBottom[0].endPos - slBottom[0].startPos + 1;
        float veryBottomWidth = slVeryBottom[0].endPos - slVeryBottom[0].startPos + 1;
        float bottomRightOffset = r->rect.size.width - slBottom[0].endPos - 1;
        float veryBottomRightOffset = r->rect.size.width - slVeryBottom[0].endPos - 1;
            // Not too wide on top
        if (((topWidth < r->rect.size.width * 0.84)
             || ((topWidth < r->rect.size.width * 0.90) && (veryTopWidth < r->rect.size.width * 0.84) && (veryTopRightOffset > topRightOffset)))
             // Right side much more detached than left side
            && (topRightOffset > slTop[0].startPos * 2)
            && ((bottomWidth < r->rect.size.width * 0.84)
                || ((bottomWidth < r->rect.size.width * 0.90) && (veryBottomWidth < r->rect.size.width * 0.84) && (veryBottomRightOffset > bottomRightOffset)))
            // Right side much more detached than left sid
            && (bottomRightOffset > slBottom[0].startPos * 2)
            && (rightHeight < leftHeight * 0.80)) {
            res = 1;
            newChar = 'D';
        }
    }
    if (needToRelease)
        delete st;
    return res;
} // testAsD

// Tests a letter for either 7 or T
// TODO: the test doesn't worry about there being a single connected component in the input rect, might lead to unsafe replacements since we are not geared to expect multiple connected components here.
char SingleLetterTestAs17T(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, SingleLetterTests **inputStPtr) {
    // Test only '1' that are too wide relative to height to really be a '1'
    // Exclude '7' and 'T' only if w < h/2 AND width is also less than 85% of a regular digit width
    // Or w < h * 0.38 (that's too narrow no matter what)
    if ( ((rect.size.width <= rect.size.height * 0.50) && (stats->averageWidthDigits.count >= 2) && (rect.size.width < stats->averageWidthDigits.average * 0.80))
        || (rect.size.width < rect.size.height * 0.38))
    {
        return ('1');
    }
    
    if (!results->imageTests || (r->flags3 & FLAGS3_TESTED_AS_7)) {
        return '\0';
    }
    
    char newCh = '\0';
    
    r->flags3 |= FLAGS3_TESTED_AS_7;
    
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    
    bool hasTop = st->getSide(SingleLetterTests::Top, 0.90, true);
    if (hasTop) {
        // Also verify that the bottom part is not aligned right
        SegmentList slBottom = st->getHorizontalSegments(0.95, 0.05);
        if (slBottom.size() >= 1) {
            Segment lastSegment = slBottom[slBottom.size() - 1];
            if (lastSegment.endPos < rect.size.width * 0.80) {
                newCh = '7';
                // One more check! 1 instead of T - check if we have an intersection in the middle along 3 points => vertical bar of the T
                float bottomMidpoint = (lastSegment.startPos + lastSegment.endPos) / 2;
                float minX = rect.size.width * 0.35;
                float maxX = rect.size.width * 0.65;
                if ((bottomMidpoint > minX) && (bottomMidpoint < maxX)) {
                    SegmentList slTop = st->getHorizontalSegments(0.20, 0.01);
                    SegmentList slMiddle = st->getHorizontalSegments(0.50, 0.01);
                    if ((slTop.size() == 1) && (slMiddle.size() == 1)) 
                    {
                        float topMidpoint = (slTop[0].startPos + slTop[0].endPos) / 2;
                        float middleMidpoint = (slMiddle[0].startPos + slMiddle[0].endPos) / 2;
                        if ((topMidpoint > minX) && (topMidpoint < maxX) && (middleMidpoint > minX) && (middleMidpoint < maxX)) 
                        {
                            newCh = 'T';
                        }
                    }
                }
            } else {
                SegmentList slTop = st->getHorizontalSegments(0.025, 0.05);
                SegmentList sl20BelowTop = st->getHorizontalSegments(0.20, 0.01);
                if ((slTop.size() == 1) && (sl20BelowTop.size() == 1)
                    // Right end of 20% below is beyond 90% of top segment
                    && (sl20BelowTop[0].endPos > slTop[0].endPos - (slTop[0].endPos - slTop[0].startPos) * 0.10)) {
                    newCh = 'l';
                }
            } // bottom aligned right
        } // segment found at bottom
    } // has top
    if (needToRelease) delete st;
    
    return (newCh);
} // SingleLetterTestAs17T


bool SingleLetterTestAsA(OCRRectPtr r, CGRect rect, CGRect *outRect, OCRWordPtr &stats, OCRResults *results, SingleLetterTests **inputStPtr) {
    if (r->flags2 & FLAGS2_TESTED_AS_A)
        return '\0';
    r->flags2 |= FLAGS2_TESTED_AS_A;
    
    if (!results->imageTests)
        return false;

    SingleLetterTests *origSt = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        origSt = *inputStPtr;
        needToRelease = false;
    } else {
        origSt = CreateSingleLetterTests(rect, results);
        if ((origSt != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = origSt;
            needToRelease = false; // Caller will release
        }
    }
    if (origSt == NULL) {
        return '\0';
    }
    
    ConnectedComponentList cpl = origSt->getConnectedComponents();
    if (cpl.size() < 2) {
        if (needToRelease) delete origSt;
        return false;
    }
    
    CGRect adjustedRect = CGRect (rectLeft(rect) + cpl[1].xmin, rectBottom(rect) + cpl[1].ymin, cpl[1].getWidth(), cpl[1].getHeight());
    // Require that the main component be tall enough
    if ((stats->averageHeightUppercase.count > 0) && (adjustedRect.size.height < stats->averageHeightUppercase.average * 0.90)) {
        if (needToRelease) delete origSt;
        return false;
    }
    
    SingleLetterTests *st;
    
    if ((adjustedRect.size.height != rect.size.height) || (adjustedRect.size.width != rect.size.width)) {
        st = CreateSingleLetterTests(adjustedRect, results);
        if (st == NULL) {
            if (needToRelease) delete origSt;
            return false;
        }
        needToRelease = true;
    } else {
        st = origSt;
    }
    *outRect = adjustedRect;
    rect = adjustedRect;
    
    SegmentList slTop = st->getHorizontalSegments(0.05, 0.10);
    SegmentList slBottom = st->getHorizontalSegments(0.95, 0.10);
#if DEBUG
    SingleLetterPrint(slTop, rect.size.width);
    SingleLetterPrint(slBottom, rect.size.width);
#endif
    
    if ((slTop.size() < 1)
        // Require 2 segments at bottom
        || (slBottom.size() != 2))
    {
        if (needToRelease) delete st;
        return false;
    }
    
    // Narrow on top, wide at bottom
    float topWidth = slTop[slTop.size()-1].endPos - slTop[0].startPos;
    float bottomWidth = slBottom[slBottom.size()-1].endPos - slBottom[0].startPos;
    if ((topWidth < rect.size.width * 0.30)
        && (slTop[0].startPos > rect.size.width * 0.25)
        && (slTop[slTop.size()-1].endPos < rect.size.width * 0.75)
        && (bottomWidth > rect.size.width * 0.75)) {
        if (needToRelease) delete st;
        return false;
    }
    
    SegmentList slMiddle = st->getVerticalSegments(0.475, 0.04);
#if DEBUG
    SingleLetterPrint(slMiddle, rect.size.height);
#endif
    
    if (slMiddle.size() != 2) {
        if (needToRelease) delete st;
        return false;
    }
    
    // Inverted component above mid-point
    ConnectedComponentList invertCpl = st->getInverseConnectedComponents();
    if (invertCpl.size() < 2) {
        if (needToRelease) delete st;
        return false;
    }
    
    float holeCenter = (invertCpl[1].xmin + invertCpl[1].xmax) / 2;
    if (holeCenter > rect.size.height * 0.75) {
        if (needToRelease) delete st;
        return false;
    }
    
    if (needToRelease) delete st;
    
    return true;
} // SingleLetterTestAsA


// Tests a letter for M
// Return value:
// - -1: no opinion
// - 0: not a M
// - 1: M
int SingleLetterTestAsM(OCRRectPtr r, CGRect rect, OCRResults *results) {
    // Expect wider than tall
    if (rect.size.width < rect.size.height) {
        return 0;
    }
    
    if (!results->imageTests) {
        return -1;
    }
    
    SingleLetterTests *st = CreateSingleLetterTests(rect, results);
    if (st == NULL)
        return -1;
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    if ((cpl.size() < 2) || ((cpl.size() >= 3) && (cpl[2].area > cpl[1].area * 0.05))) {
        return -1;
    }
    
    OpeningsTestResults resTop;
    bool success = st->getOpenings(resTop, SingleLetterTests::Top, 
        0.20,      // Start of range to search (top/left)
        0.90,      // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
    if (!success || (resTop.maxDepth < rect.size.height * 0.32)) {
        return 0;
    }
    
    OpeningsTestResults resBottomLeft;
    success = st->getOpenings(resBottomLeft, SingleLetterTests::Bottom, 
        0.10,      // Start of range to search (top/left)
        resTop.maxDepthCoord / rect.size.width,      // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
    if (!success || (resBottomLeft.maxDepth < rect.size.height * 0.10)) {
        return 0;
    }
    
    OpeningsTestResults resBottomRight;
    success = st->getOpenings(resBottomRight, SingleLetterTests::Bottom, 
        resTop.maxDepthCoord / rect.size.width,      // Start of range to search (top/left)
        0.95,      // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
    if (!success || (resBottomRight.maxDepth < rect.size.height * 0.10)) {
        return 0;
    }
    
    return 1;
} // testAsM

// Returns false if no opinion
// *ch1, *ch2 is where we store our suggested char(s)
bool SingleLetterTestAsm(OCRRectPtr r, OCRResults *results, char *ch1, CGRect *rect1, char *ch2, CGRect *rect2) {
    
    if (!results->imageTests) {
        return false;    // No opinion
    }
    
    // Test for possible "rn" or "nr"
    SingleLetterTests* st = CreateSingleLetterTests(r->rect, results);
    
    if (st == NULL) {
        return false; // no recommendation
    }
    
    bool doit = false;
    char newCh1 = '\0';
    char newCh2 = '\0';
    CGRect newRect1, newRect2;
    *ch1 = '\0';
    *ch2 = '\0';
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    // Check that we have TWO main components
    if (cpl.size() <= 2) {
        // Only one connected component
        *ch1 = r->ch;
        delete st;
        return true;    // Probably a 'm'. Note: I think it assumes that we think it's a 'm', that's why it doesn't prode much further. Caller needs to more testing if intending to make a replacement
    }
 
    // Get main component dimensions
    ConnectedComponent mainComp = cpl[1];
    ConnectedComponent minorComp = cpl[2];
    
    int firstCompIndex = ((cpl[1].xmin < cpl[2].xmin)? 1:2);
    int secondCompIndex = ((cpl[1].xmin < cpl[2].xmin)? 2:1);
    
    float mainCompRangeSize = mainComp.getHeight() * mainComp.getWidth();
    float minorCompRangeSize = minorComp.getHeight() * minorComp.getWidth();
    
    // Do we have two letter roughly equivalent in size?
    if ((minorCompRangeSize > mainCompRangeSize * 0.5) 
        // Below guards against the case of a 'm' disconnected in two places above
        && ((cpl.size() < 4) || (cpl[3].getHeight() * cpl[3].getWidth() < minorCompRangeSize * 0.30))
        // Test also that the two components are about the same height and about the same baseline
        && (cpl[1].getHeight() > cpl[2].getHeight() * 0.85) && (cpl[1].getHeight() < cpl[2].getHeight() * 1.15)
        )
    {
        // Comp must be close to same height for 'n'+'r' combinations 
        if (abs(cpl[secondCompIndex].ymin - cpl[firstCompIndex].ymin) < r->rect.size.height * 0.10)
        {
            // Test that we the two components are separated at least till the middle of the blob
            float startX, endX;
            if (mainComp.xmin < minorComp.xmin) {
                startX = mainComp.xmax - 1;
                endX = minorComp.xmin + mainComp.getWidth() / 4;
            } else {
                startX = minorComp.xmax - 1;
                endX = mainComp.xmin + mainComp.getWidth() / 4;
            }

            CGRect tryRect(rectLeft(r->rect),
                rectBottom(r->rect),
                r->rect.size.width,
                r->rect.size.height / 2);
            SingleLetterTests *stTry = CreateSingleLetterTests(tryRect, results);
            if (stTry == NULL) {
                delete st;
                return false; // Can't decide
            }
            float gapStart = -1; float gapEnd = -1;
            ConnectedComponentList cplTry = stTry->getConnectedComponents();
            if (cplTry.size() >= 3) {
                // Changing width requirement to accept = 1 because we know there are 2 comps so if they touch it must mean it's in diagonal, i.e. consider separate
                if ((cplTry[1].xmin < cplTry[2].xmin) && (cplTry[2].xmin - cpl[1].xmax >= 1)) {
                    gapStart = cpl[1].xmax;
                    gapEnd = cplTry[2].xmin;
                } else if ((cplTry[2].xmin < cplTry[1].xmin) && (cplTry[1].xmin - cpl[2].xmax >= 1)) {
                    gapStart = cpl[2].xmax;
                    gapEnd = cplTry[1].xmin;
                }

                if (gapStart < 0) {
                    *ch1 = 'm';
                    *rect1 = r->rect;
                    delete stTry;
                    delete st;
                    return true;
                }
            }
            
            OpeningsTestResults resBottom1;
            OpeningsTestResults resBottom2;
            CGRect newRect1(rectLeft(r->rect),
                rectBottom(r->rect),
                gapStart + (gapEnd - gapStart)/2,
                r->rect.size.height);
            CGRect newRect2(rectLeft(r->rect) + gapStart + (gapEnd - gapStart)/2,
                rectBottom(r->rect),
                r->rect.size.width - (gapEnd + gapStart)/2,
                r->rect.size.height);
            
            SingleLetterTests *st1 = CreateSingleLetterTests(newRect1, results);
            
            SingleLetterTests *st2 = CreateSingleLetterTests(newRect2, results);
                
            if ((st1 == NULL) || (st2 == NULL)) {
                *ch1 = 'm';
                *rect1 = r->rect;
                delete stTry;
                delete st;
                if (st1 != NULL)
                    delete st1;
                if (st2 != NULL)
                    delete st2;
                return true;
            }
            
            // Before even testing the bottom openings, check if we don't have what appears like n+r but where the 'r' is leaning left instead of right. If so, it's a 'm' after all. This probably happens only if the two are touching but test in all cases
            // Check that we have large then small
            if (cplTry[1].xmin < cplTry[2].xmin) 
            {
                OpeningsTestResults resLeft;
                OpeningsTestResults resRight;
                bool successLeft = st2->getOpenings(resLeft, SingleLetterTests::Left, 
                    0.00,      // Start of range to search (top/left)
                    0.50,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Any  // Require end (bottom/right) bound
                    );
                bool successRight = st2->getOpenings(resRight, SingleLetterTests::Right, 
                    0.00,      // Start of range to search (top/left)
                    0.50,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Any  // Require end (bottom/right) bound
                    );                
                    // If there is a gap on the left
                if (successLeft && (resLeft.maxDepth > newRect2.size.width * 0.10) 
                    // And there is no gap on the right or else a smaller one
                    && (!successRight || (resRight.maxDepth < resLeft.maxDepth))) {
                    // Abort! It's probably a real 'm'
                    *ch1 = 'm';
                    *rect1 = r->rect;
                    delete stTry;
                    delete st;
                    delete st1;
                    delete st2;
                    return true;
                }
            }
            
            bool successBottomR1 = st1->getOpenings(resBottom1, SingleLetterTests::Bottom, 
                0.00,      // Start of range to search (top/left)
                1.00,      // End of range to search (bottom/right)
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound  // Require end (bottom/right) bound
                );
            
            bool successBottomR2 = st2->getOpenings(resBottom2, SingleLetterTests::Bottom, 
                0.00,      // Start of range to search (top/left)
                1.00,      // End of range to search (bottom/right)
                SingleLetterTests::Bound,   // Require start (top/left) bound
                SingleLetterTests::Bound  // Require end (bottom/right) bound
                );
            
            if (successBottomR2 && (resBottom2.maxDepth > newRect2.size.height * 0.30)) {
                    doit = true;
                    newCh2 = 'n';
            }
            
            if (successBottomR1 && (resBottom1.maxDepth > newRect1.size.height * 0.30)) {
                    doit = true;
                    newCh1 = 'n';
            }
        
            // If neither was a clear 'n' but one has a non-zero opening at bottom, pick the one with the largest opening
            if ((newCh1 != 'n') && (newCh1 != 'n')) {
                if (successBottomR2 && (resBottom2.maxDepth > 0)) {
                    doit = true;
                    newCh2 = 'n';
                } else if (successBottomR1 && (resBottom1.maxDepth > 0)) {
                    doit = true;
                    newCh1 = 'n';
                }
            }
                
            if (newCh1 == 'n') {
                doit = true;
                if (newCh2 == '\0')
                    newCh2 = 'r';
            } else if (newCh2 == 'n') {
                doit = true;
                if (newCh1 == '\0')
                    newCh1 = 'r';
            }
            
            if (doit) {
                *ch1 = newCh1;
                *rect1 = newRect1;
                *ch2 = newCh2;
                *rect2 = newRect2; 
            }
        } // two comps roughly same size + roughly same baseline + roughly same height
        // 'n'+'t': e.g. 'n' has ymin=3, w=17, 't' has ymin=0, w=8
                // Smaller comp is 2nd (i.e. large followed by small)
        else if ((secondCompIndex == 2)
                // Smaller comp more narrow
                && (minorComp.getWidth() < mainComp.getWidth() * 0.75)
                && (mainComp.ymin - minorComp.ymin > r->rect.size.height * 0.10))
        {
            // Test 'n' opening from bottom
            CGRect firstRect (rectLeft(r->rect) + mainComp.xmin, rectBottom(r->rect) + mainComp.ymin, mainComp.getWidth(), mainComp.getHeight());
            SingleLetterTests *stFirst = CreateSingleLetterTests(firstRect, results);
            if (stFirst != NULL) {
                OpeningsTestResults resBottom1;
                bool successBottomR1 = stFirst->getOpenings(resBottom1, SingleLetterTests::Bottom,
                    0.00,      // Start of range to search (top/left)
                    1.00,      // End of range to search (bottom/right)
                    SingleLetterTests::Bound,   // Require start (top/left) bound
                    SingleLetterTests::Bound  // Require end (bottom/right) bound
                    );
                if (successBottomR1 && (resBottom1.maxDepth > firstRect.size.height * 0.30)) {
                    *ch1 = 'n';
                    *rect1 = firstRect;
                    *ch2 = 't';
                    *rect2 = CGRect (rectLeft(r->rect) + minorComp.xmin, rectBottom(r->rect) + minorComp.ymin, minorComp.getWidth(), minorComp.getHeight());
                    // Flow out, will release st and return true
                }
                delete stFirst;
            }
        }
    }

    delete st;
    
    if (*ch1 == '\0') {
        *ch1 = r->ch;   // Validate 'm' hypothesis
    }
    
    return true;
} // testasm


wchar_t SingleLetterTestAsnTilde(OCRRectPtr r, CGRect rect, OCRResults *results, SingleLetterTests **inputStPtr) {

    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    
    ConnectedComponentList cpl = st->getConnectedComponents();
    if (cpl.size() != 3) {
        if (needToRelease) delete st;
        return '\0';
    }
    
    ConnectedComponent nComp = cpl[1];
    ConnectedComponent tilde = cpl[2];
    
    // Check for opening from bottom
    OpeningsTestResults resBottom;
    bool success = st->getOpenings(resBottom,
            SingleLetterTests::Bottom,
            0.15,      // Start of range to search (top/left)
            0.85,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound);  // Require end (bottom/right) bound
    if (!success || (resBottom.maxDepth < rect.size.height * 0.25)) {
        if (needToRelease) delete st;
        return '\0';
    }
    
    // Small opening on top (tilde)
    // Need to create a new st because API works only on main comp
    CGRect newRect (rectLeft(rect) + tilde.xmin, rectBottom(rect) + tilde.ymin, tilde.getWidth(), tilde.getHeight());
    SingleLetterTests *stTilde = CreateSingleLetterTests(newRect, results);
    if (stTilde == NULL) {
        if (needToRelease) delete st;
        return '\0';
    }
    
    OpeningsTestResults resTop;
    success = stTilde->getOpenings(resTop,
            SingleLetterTests::Top,
            0.00,      // Start of range to search (top/left)
            1.00,      // End of range to search (bottom/right)
            SingleLetterTests::Bound,   // Require start (top/left) bound
            SingleLetterTests::Bound);  // Require end (bottom/right) bound
    delete stTilde;
    if (!success) {
        if (needToRelease) delete st;
        return '\0';
    }
    
            // Test tilde is above n
    if (! ((tilde.ymax <= nComp.ymin)
            // Wide enough
            && (tilde.getWidth() > rect.size.width * 0.30)) ) {
        if (needToRelease) delete st;
        return '\0';
    }
    
    if (needToRelease) delete st;
    return (0x00F1); // 'ñ'
} // SingleLetterTestAsnTilde

bool SingleLetterTestAsV(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrentAndNext, OCRResults *results, wchar_t &newChar, SingleLetterTests **inputStPtr, CGRect *outRec) {
    if (!results->imageTests || (r->flags5 & FLAGS5_TESTED_AS_v))
        return false;
    r->flags5 |= FLAGS5_TESTED_AS_v; 
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(rect, results, true);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return false;
    }
    ConnectedComponentList cpl = st->getConnectedComponents();

    bool ccOK = false;
    if (cpl.size() == 2) {
        ccOK = true;
        if (outRec != NULL)
            *outRec = rect;
    } else if (cpl.size() == 3) {
        // Allow small portion of next letter on bottom-right side of current rect - happens if you have say VA
        ConnectedComponent minorCC = cpl[2];
        ConnectedComponent majorCC = cpl[1];
        float gapAbove = majorCC.ymin - minorCC.ymax - 1;
        if ((minorCC.area < majorCC.area * 0.20)
            && (minorCC.ymin > majorCC.getHeight() * 0.25)
            // Past the middle
            && (minorCC.xmin > majorCC.getWidth() * 0.65))
            ccOK = true;
        // Noise above
        else if ((minorCC.area < majorCC.area * 0.15)
                 && (gapAbove > majorCC.getHeight() * 0.50))
            ccOK = true;
        if (ccOK && (outRec != NULL)) {
            outRec->origin.x = rectLeft(rect) + majorCC.xmin;
            outRec->origin.y = rectBottom(rect) + majorCC.ymin;
            outRec->size.width = majorCC.getWidth();
            outRec->size.height = majorCC.getWidth();
        }
        // Re-create st
        if (needToRelease)
            delete st;
        rect = *outRec;
        st = CreateSingleLetterTests(rect, results, true);
        needToRelease = true;
        if (st == NULL) {
            return false;
        }
    }
    
    if (!ccOK) {
        if (needToRelease)
            delete st;
        return false;
    }
    ConnectedComponent mainCC = cpl[1];

    // Allow for a delta of one pixel out of height 19
    SegmentList slTop = st->getHorizontalSegments(0.055, 0.11);
    if (slTop.size() != 2) {
        if (needToRelease)
            delete st;
        return false;
    }
    float widthTop = slTop[1].endPos - slTop[0].startPos + 1;
    if (widthTop < rect.size.width * 0.90) {
        if (needToRelease)
            delete st;
        return false;
    }
    // Now test top opening
    OpeningsTestResults resTop;
    bool successTop = st->getOpenings(resTop, SingleLetterTests::Top, 
        0.20,      // Start of range to search (top/left)
        0.90,    // End of range to search (bottom/right)
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound  // Require end (bottom/right) bound
        );
    wchar_t resChar = '\0';
    if (successTop && (resTop.maxDepth > rect.size.height * 0.20)) {
        resChar = 'v';
        if ((r->ch == 'Y') || (r->ch == 'V') || (tallHeightTest(statsWithoutCurrentAndNext.getPtr(), rect.size.height, 'V', false, 0, false) == 1)) {
            resChar = 'V';
        }
        // Maybe a 'Y'
        // Determine depth of max opening relative to top of rect (in case it gets us a deeper depth
        SegmentList slTopMiddle = st->getVerticalSegments(resTop.maxDepthCoord / rect.size.width, 0.01);

        float maxDepth = resTop.maxDepth;
        if (slTopMiddle.size() >= 1) {
            if (slTopMiddle[0].startPos + 1 > maxDepth)
                maxDepth = slTopMiddle[0].startPos + 1;
        }
        if ((maxDepth < rect.size.height * 0.55)
            // Leave 'v' if part of VP
            && !(((r->previous == NULL) || (r->previous->ch == ' '))
                 && ((r->next != NULL) && ((r->next->ch == 'p') || (r->next->ch == 'P')) && ((r->next->next == NULL) || !isLetter(r->next->next->ch))))) {
            SegmentList bottomSl = st->getHorizontalSegments(0.95, 0.095);
            SegmentList bottom20Sl = st->getHorizontalSegments(0.80, 0.05); // 75% - 85%
            SingleLetterPrint(bottom20Sl, rect.size.width);
            SingleLetterPrint(bottomSl, rect.size.width);
            if ((bottomSl.size() == 1) && (bottom20Sl.size() == 1)) {
                float bottomWidth = bottomSl[0].endPos - bottomSl[0].startPos + 1;
                float bottom20Width = bottom20Sl[0].endPos - bottom20Sl[0].startPos + 1;
                float widthStroke = ((slTop[0].endPos - slTop[0].startPos + 1) + (slTop[1].endPos - slTop[1].startPos + 1)) / 2;
                DebugLog("testAsV: stroke=%.0f, bottom20Width=%.0f, bottomWidth=%.0f", widthStroke, bottom20Width, bottomWidth);
                if (((abs(bottomWidth - bottom20Width) < mainCC.getWidth() * 0.10) 
                    && (abs(bottom20Width - widthStroke) < widthStroke * 0.10))
                        // Or quite short opening on top
                    || ((maxDepth < rect.size.height * 0.30)
                        // And still not too much wider in the middle
                        && (abs(bottomWidth - bottom20Width) < mainCC.getWidth() * 0.20)))
                {
                    resChar = 'Y';
                }
            }
        }
    }
    if (needToRelease)
        delete st;
    // If we assumed 'y', don't change letter (usually to 'V' or 'Y'), leave as-is
    if (r->ch == 'y')
        resChar = 'y';
    newChar = resChar;
    return true;
} // testAsV

// Check if the tested letter is adjacent to digits
// If the adjacent character is a digit that looks very similar to a lowercase letter, make sure it's tall as a digit! The only such example that comes to mind at this time is '0'
bool digitsNearby(OCRRectPtr r, bool strict) {
    if (strict) {
        // Require that both neibors be digits
        if (((r->previous == NULL) || (isDigit(r->previous->ch) && ((r->previous->ch != '0') || (r->previous->rect.size.height > r->word->averageHeightDigits.average * (1 - OCR_ACCEPTABLE_ERROR)))))
          && ((r->next == NULL) || (isDigit(r->next->ch) && ((r->next->ch != '0') || (r->next->rect.size.height > r->word->averageHeightDigits.average * (1 - OCR_ACCEPTABLE_ERROR)))))) {
             return true;
        }
    } else {
        // Requires that any one neigbor be a digit, even with a space between
        bool laxDigit = !strict;
        if ( ((r->previous != NULL) && isDigit(r->previous->ch, laxDigit))
            || ((r->next != NULL) && isDigit(r->next->ch, laxDigit))
             || ((r->previous != NULL) && (r->previous->previous != NULL) && isDigit(r->previous->previous->ch, laxDigit) && ((r->previous->previous->ch != '0') || (r->previous->previous->rect.size.height > r->word->averageHeightDigits.average * (1 - OCR_ACCEPTABLE_ERROR))))
             || ((r->next != NULL) && (r->next->next != NULL) && isDigit(r->next->next->ch, laxDigit) && ((r->next->next->ch != '0') || (r->next->next->rect.size.height > r->word->averageHeightDigits.average * (1 - OCR_ACCEPTABLE_ERROR)))) 
           ) {
             return true;
        }
    }
    return false;
} // digitsNearby

bool validateDot(float xminDot, float yminDot, float xmaxDot, float ymaxDot, float xminRef, float yminRef, float xmaxRef, float ymaxRef, OCRWordPtr &statsWithoutCurrent)
{
    float widthDot = xmaxDot - xminDot + 1;
    float heightDot = ymaxDot - yminDot + 1;
    float widthRef = xmaxRef - xminRef + 1;
    if (statsWithoutCurrent->averageWidthNormalLowercase.count >= 2)
        widthRef = statsWithoutCurrent->averageWidthNormalLowercase.average;
    float heightRef = ymaxRef - yminRef + 1;
    if (statsWithoutCurrent->averageHeightNormalLowercase.count >= 2)
        heightRef = statsWithoutCurrent->averageHeightNormalLowercase.average;
    if (// Big enough
        (widthDot * heightDot > widthRef * heightRef * OCR_RATIO_DOT_SIZE_TO_NORMAL_LETTER_MIN)
        && (widthDot * heightDot < widthRef * heightRef * OCR_RATIO_DOT_SIZE_TO_NORMAL_LETTER_MAX)
        // Top of dot below 40% height
        && (yminDot > yminRef + heightRef * (1 - OCR_RATIO_DOT_HEIGHT_TO_NORMAL_LETTER_MAX))
        // Much less wide
        && (widthDot < widthRef * OCR_RATIO_DOT_WIDTH_TO_NORMAL_LETTER_MAX)
        // Not much below baseline
        && (ymaxDot - ymaxRef < heightDot * 0.25)
        && (heightDot < heightRef * 0.35)
        && (heightDot <= widthDot * 2.0)
        && (widthDot <= heightDot * 2.0)
        ) {
            return true;
        }
    return false;
} // validateDot

// Returns possible dot before (if not glued to previous) or after (if not glued to next)
// Adjusts 's' to 'c' in some cases
bool needToInsertDot (OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, CGRect &rect1, CGRect &rect2, wchar_t &newCh1, wchar_t &newCh2, SingleLetterTests **inputStPtr) {
    if (!results->imageTests || (r->flags6 & FLAGS6_TESTED_DOT))
        return false;
    
    r->flags6 |= FLAGS6_TESTED_DOT;
    
    SingleLetterTests *st = NULL;
    bool needToRelease = true;
    if ((inputStPtr != NULL) && (*inputStPtr != NULL)) {
        // We were passed a st pointer, use it
        st = *inputStPtr;
        needToRelease = false;
    } else {
        st = CreateSingleLetterTests(r->rect, results);
        if ((st != NULL) && (inputStPtr != NULL)) {
            *inputStPtr = st;
            needToRelease = false; // Caller will release
        }
    }
    if (st == NULL) {
        return '\0';
    }
    
    float acceptableRatio = (isTallLowercase(r->ch)? 0.0123:0.02);
    ConnectedComponentList cpl = st->getConnectedComponents(acceptableRatio);
    SingleLetterPrint(cpl, r->rect);

    // Require 2 components, or 3+ with 3rd+ smaller than half the dot
    if ((cpl.size() < 3)
        // More than 2 and 3rd is more than 50% of the 2nd (dot)
        || ((cpl.size() >= 4) && (cpl[3].area > cpl[2].area * 0.50))
        // Dot too small
        || ((cpl.size() >= 3) && (cpl[2].area < r->rect.size.height * r->rect.size.width * acceptableRatio))
        ) {
        if (needToRelease) delete st;
        return false;
    }
        
    ConnectedComponent letterCC = cpl[1];
    ConnectedComponent dotCC = cpl[2];
    float letterCenterX = ((float)(letterCC.xmin + letterCC.xmax))/2;
    float dotCenterX = ((float)(dotCC.xmin + dotCC.xmax))/2;
    bool dotAfterLetter = (dotCenterX > letterCenterX);
    
    bool gluedToPrevious = false, gluedToNext = false;
    
    if ((r->previous != NULL) && (r->previous->ch != ' ') && (rectSpaceBetweenRects(r->previous->rect, r->rect) <= 0))
        gluedToPrevious = true;
    
    if ((r->next != NULL) && (r->next->ch != ' ') && (rectSpaceBetweenRects(r->rect, r->next->rect) <= 0))
        gluedToNext = true;
    
    // For now don't take the risk if dot is glued to next or previous
    // TODO determine is dot is actually touching any pixels in the adjacent letter
    if ( (dotAfterLetter && gluedToNext)
        || (!dotAfterLetter && gluedToPrevious) ) {
        if (needToRelease) delete st;
        return false;
    }
        
    // For now expect the dot to be either entirely before/after or almost (90%)
    float maxDistanceFromLetterEdge = 0.10; // 10% short of clearing the letter edge
    if ((r->ch == 'R') && dotAfterLetter)
        maxDistanceFromLetterEdge = 0.33; // Actually a 'P' which has lots of space under to host the dot
    if (abs(dotCenterX - letterCenterX) < letterCC.getWidth() * 0.50 * (1 - maxDistanceFromLetterEdge)) {
        if (needToRelease) delete st;
        return false;
    }
        
    DebugLog("needToInsertDot: dot [w=%d, h=%d] %s letter [w=%d, h=%d]", (unsigned int)dotCC.getWidth(), (unsigned int)dotCC.getHeight(), (dotAfterLetter? "after":"before"), (unsigned int)letterCC.getWidth(), (unsigned int)letterCC.getHeight());
    
    if (validateDot((float)dotCC.xmin, (float)dotCC.ymin, (float)dotCC.xmax, (float)dotCC.ymax,
            (float)letterCC.xmin, (float)letterCC.ymin, (float)letterCC.xmax, (float)letterCC.ymax, statsWithoutCurrent))
    {
        CGRect r1(rectLeft(r->rect) + (dotAfterLetter? letterCC.xmin:dotCC.xmin), 
                  rectBottom(r->rect) + (dotAfterLetter? letterCC.ymin:dotCC.ymin),
                  (dotAfterLetter? letterCC.getWidth():dotCC.getWidth()), 
                  (dotAfterLetter? letterCC.getHeight():dotCC.getHeight()));
        CGRect r2(rectLeft(r->rect) + (dotAfterLetter? dotCC.xmin:letterCC.xmin), 
                  rectBottom(r->rect) + (dotAfterLetter? dotCC.ymin:letterCC.ymin),
                  (dotAfterLetter? dotCC.getWidth():letterCC.getWidth()), 
                  (dotAfterLetter? dotCC.getHeight():letterCC.getHeight()));
        rect1 = r1;
        rect2 = r2;
        
        // Dot or comma?
        char dotCh = '.';
        // Is it a comma?
        if ((dotCC.getHeight() > dotCC.getWidth() * OCR_MAX_H_TO_W_RATIO_DOT)
             && (dotCC.ymax > letterCC.ymax)) 
        {
            dotCh = ',';
        }
        newCh1 = (dotAfterLetter? r->ch:dotCh);
        newCh2 = (dotAfterLetter? dotCh:r->ch);
        
        // Test for ".s"
        if ((r->ch == 's') && !dotAfterLetter) {
            // Test left opening - if 's' there must be an opening at the bottom or 3 vertical segments in the middle of the main CC
            OpeningsTestResults resLeft;
            bool successTop = st->getOpenings(resLeft, SingleLetterTests::Left,
                                          0.40,      // Start of range to search (top/left)
                                          dotCC.ymin / r->rect.size.height,    // End of range to search (bottom/right)
                                          SingleLetterTests::Bound,   // Require start (top/left) bound
                                          SingleLetterTests::Bound  // Require end (bottom/right) bound
                                          );

            if (!successTop || resLeft.maxDepth == 0) {
                // Now try vertical segments (in case dot blocked the opening at the bottom)
                SegmentList middleSL = st->getVerticalSegments((letterCC.xmin + letterCC.xmax)/2/r->rect.size.width, 0.05);
                if ((middleSL.size() != 0) && (middleSL.size() < 3))
                    newCh2 = 'c';
            }
        }
        // Test for "l."
        else if ((r->ch == 'L') && dotAfterLetter)
        {
            // Bob Shaw: h=28,w=6 => 4.6x
            if (letterCC.getHeight() > letterCC.getWidth() * 3)
                newCh1 = 'l';
        }
        // Special test for 'R' (mapping to "P.")
        else if ((r->ch == 'R') && dotAfterLetter) {
            // Check a horizontal segment a little above the dot, expect just one segment ending way to the left
            float barVPos = dotCC.ymin - r->rect.size.height * 0.10;
            SegmentList slMiddle = st->getHorizontalSegments(barVPos/r->rect.size.height, 0.00);
            if ((slMiddle.size() == 1) && (slMiddle[0].endPos < r->rect.size.width * 0.40))
                newCh1 = 'P';
        }
    
        if (needToRelease) delete st;
        return true;
    }
    if (needToRelease) delete st;
    return false;
} // needToInsertDot

// Returns true if a dot should be inserted here
bool needToInsertDot (OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, ConnectedComponentList &cpl, CGRect &newRect1, CGRect &newRect2) {
    if (cpl.size() < 3)
        return false;
    
    if (validateDot((float)cpl[2].xmin, (float)cpl[2].ymin, (float)cpl[2].xmax, (float)cpl[2].ymax,
        (float)cpl[1].xmin, (float)cpl[1].ymin, (float)cpl[1].xmax, (float)cpl[1].ymax, statsWithoutCurrent)
        // Dot starts left of letter
        && (cpl[2].xmin < cpl[1].xmin))
    {
        // Insert dot
        CGRect r1(rectLeft(r->rect) + cpl[2].xmin, rectBottom(r->rect) + cpl[2].ymin, cpl[2].getWidth(), cpl[2].getHeight());
        CGRect r2(rectLeft(r->rect) + cpl[1].xmin, rectBottom(r->rect) + cpl[1].ymin, cpl[1].getWidth(), cpl[1].getHeight());
        newRect1 = r1;
        newRect2 = r2;
        return true;
    }
    return false;
} // needToInsertDot

bool isCloseOrSmallerThanAverage(const AverageWithCount& av, float otherValue, float tolerance) {
	if ((av.average == 0) || (otherValue == 0)) {
#if DEBUG
		if (otherValue == 0) {
			ErrorLog("isClose: ERROR, got zero value!");
		}
#endif
		return false;
	}

	if (otherValue/av.average < (1+tolerance))
		return true;
	else
		return false;
}

bool isCloseOrGreaterThanAverage(const AverageWithCount& av, float otherValue, float tolerance) {
	if ((av.average == 0) || (otherValue == 0)) {
#if DEBUG
		if (otherValue == 0) {
			ErrorLog("isClose: ERROR, got zero value!");
		}
#endif
		return false;
	}

	if (otherValue/av.average > (1-tolerance))
		return true;
	else
		return false;
} // isCloseOrGreaterThanAverage

float abs_float(float f) {
	if (f >= 0)
		return (f);
	else
		return (-1.0 * f);
}

bool isCloseVal1ToVal2(float val1, float val2, float tolerance) {
	if ((val1 == 0) || (val2 == 0)) {
		return false;
	}

	if ((val1/val2 > (1-tolerance)) && (val1/val2 < (1+tolerance)))
		return true;
	else
		return false;
}

int indexCloserValToAv1OrAv2(float val, AverageWithCount av1, AverageWithCount av2) {
	if (abs(val - av1.average) < abs(val - av2.average))
		return 1;
	else
		return 2;
}

int indexCloserValToVal1OrVal2(float val, float val1, float val2) {
	if ((val1 == 0) || (val2 == 0))
		return 0;
	if (abs(val-val1) < abs(val-val2))
		return 1;
	else
		return 2;
}

bool isClose(float v1, float v2, float tolerance) {
	if ((v1 == 0) || (v2 == 0)) {
#if DEBUG
		ErrorLog("isClose: got zero value(s)!");
#endif
		return false;
	}

	if ((v1/v2 > (1-tolerance)) && (v1/v2 < (1+tolerance)))
		return true;
	else
		return false;
}

bool isClose(const AverageWithCount& av, float otherValue, float tolerance) {
	if ((av.average == 0) || (otherValue == 0)) {
#if DEBUG
		if (otherValue == 0) {
			ErrorLog("isClose: ERROR, got zero value!");
		}
#endif
		return false;
	}

	if ((otherValue/av.average > (1-tolerance)) && (otherValue/av.average < (1+tolerance)))
		return true;
	else
		return false;
}

SmartPtr<OCRStats> countLetterWordLine (SmartPtr<OCRRect> r, wchar_t letter, bool entireLine) {
    
    SmartPtr<OCRStats> stats = SmartPtr<OCRStats>(new OCRStats());
    
    if (r == NULL)
        return stats;
    
    // Search back
    SmartPtr<OCRRect> p = r->previous;
    // Consider only spaces because commas and dashes imply word next is same size
    while ((p != NULL) && (entireLine || (p->ch != ' '))) {
        if (p->ch == letter) {
            stats->averageHeight.count++;
            stats->averageHeight.sum += p->rect.size.height;
            
            stats->averageWidth.count++;
            stats->averageWidth.sum += p->rect.size.width;
        }
        p = p->previous;
    }
        
    // Search forward
    p = r->next;
    // Consider only spaces because commas and dashes imply word next is same size
    while ((p != NULL) && (entireLine || (p->ch != ' '))) {
        if (p->ch == letter) {
            // stats->updateStatsAddOrRemoveLetterAddingSpacingOnly(p, false, false);
            stats->averageHeight.count++;
            stats->averageHeight.sum += p->rect.size.height;
            
            stats->averageWidth.count++;
            stats->averageWidth.sum += p->rect.size.width;
        }
        p = p->next;
    }
        
    if (stats->averageHeight.count != 0)
        stats->averageHeight.average = stats->averageHeight.sum / stats->averageHeight.count;
    if (stats->averageWidth.count != 0)
        stats->averageWidth.average = stats->averageWidth.sum / stats->averageWidth.count;
    
    return stats;
}

void split(const std::wstring& source, wchar_t separator, std::vector<std::wstring>& result) {
    size_t pos = 0, prev_pos = 0;
	while( (pos = source.find(separator, pos)) != wstring::npos ) {
		wstring subS = source.substr(prev_pos, pos-prev_pos);

		result.push_back(subS);
		prev_pos = ++pos;
	}
	wstring subS( source.substr(prev_pos, pos-prev_pos) ); // Last word
	result.push_back(subS);
}

void split(const std::wstring& source, wstring separators, std::vector<std::wstring>& result) {
    size_t pos = 0, prev_pos = 0;
	while( (pos = source.find_first_of(separators, pos)) != wstring::npos ) {
		wstring subS = source.substr(prev_pos, pos-prev_pos);

		result.push_back(subS);
		prev_pos = ++pos;
	}
	wstring subS( source.substr(prev_pos, pos-prev_pos) ); // Last word
	result.push_back(subS);
}

// Inserts a float value to a vector, lowest values first, and return the insertion position (v.size() if at the end)
int insertToFloatVectorInOrder (vector<float> &v, float val) {
    int pos = 0;
    bool foundBigger = false;
    for (int i=0; i<v.size(); i++) {
        if (val < v[i]) {
            pos = i; foundBigger = true;
            break;
        }
    }
    if (!foundBigger) {
        v.push_back(val);
        return ((int)(v.size()));
    } else {
        v.insert(v.begin()+pos, val);
        return pos;
    }
}

bool findDashBetween(OCRRectPtr previous, OCRRectPtr next, SmartPtr<OCRWord> &statsWithoutCurrent, CGRect &dashRect, OCRResults *results) {

    CGRect gapRect;
    if ((previous != NULL) && (next != NULL)) {
        float minY = MIN(rectBottom(previous->rect), rectBottom(next->rect));
        float maxY = MAX(rectTop(previous->rect), rectTop(next->rect));
        gapRect = CGRect(rectRight(previous->rect)+1, minY, rectLeft(next->rect) - rectRight(previous->rect) - 1, maxY - minY + 1);
    } else if ((previous == NULL) && ((next != NULL) && (next->ch != ' '))) {
        CGRect refDigit = findClosestDigitLookAlike(next, statsWithoutCurrent, results);
        if (refDigit.size.width > 0) {
            float minY = rectBottom(refDigit);
            float height = refDigit.size.height * 1.10;
            float maxX = rectLeft(next->rect) - 1;
            float width = round(results->globalStats.averageWidthDigits.average * 2.0);
            gapRect = CGRect (maxX - width + 1, minY, width, height);
            if (rectLeft(gapRect) < rectLeft(results->imageRange)) {
                gapRect.origin.x = rectLeft(results->imageRange);
                gapRect.size.width = maxX - gapRect.origin.x + 1;
            }
        } else {
            return false;
        }
    }
    // Make sure gapRect is not too wide (in lines where a large gap exists between text segments)
    if (gapRect.size.width > results->globalStats.averageWidthDigits.average * 2.0) {
        gapRect.origin.x = round(rectRight(gapRect) - results->globalStats.averageWidthDigits.average * 2.0 + 1);
        gapRect.size.width = round(results->globalStats.averageWidthDigits.average * 2.0);
    }
    SingleLetterTests *st = CreateSingleLetterTests(gapRect, results);
    if (st != NULL) {
        ConnectedComponentList cpl = st->getConnectedComponents();
        if (cpl.size() >= 2) {
// 2016-03-15 16:26:05.941 Windfall[28547:1617849] #908: 0 quality: 86 [283,797 - 287,806] w=5,h=10]
// 2016-03-15 16:26:05.941 Windfall[28547:1617849] #909: - quality: 99 [290,801 - 294,802] w=5,h=2]
            float minY = gapRect.size.height * 0.25;   // dash must be in bottom 75% of height
            float maxY = gapRect.size.height * 0.75;   // dash must be above bottom 25% of height
            float minWidth = results->globalStats.averageWidthDigits.average * 0.50; // dash is usually as wide as a digit but sometimes image processing hurts it as in the "3.00-F" coupon in https://drive.google.com/open?id=0B4jSQhcYsC9VdVpqUGJueDdVRW8
            float minWidthToHeightRatio = 2.0; // Was 2.5 in the above example
            for (int k=1; k<cpl.size(); k++) {
                ConnectedComponent cc = cpl[k];
                if ((cc.ymin > minY) && (cc.ymax < maxY) && (cc.getWidth() > minWidth) && (cc.getWidth() > cc.getHeight() * minWidthToHeightRatio)) {
                    dashRect = CGRect (rectLeft(gapRect) + cc.xmin, rectBottom(gapRect) + cc.ymin, cc.getWidth(), cc.getHeight());
                    return true;
                }
            }
        }
        delete st;
    }

    return false;
} // findDashBetween

bool findDotBetween(OCRRectPtr previous, OCRRectPtr next, SmartPtr<OCRWord> &statsWithoutCurrent, CGRect &dotRect, OCRResults *results) {

    CGRect gapRect;
    if ((previous != NULL) && (next != NULL)) {
        float minY = MIN(rectBottom(previous->rect), rectBottom(next->rect));
        float maxY = MAX(rectTop(previous->rect), rectTop(next->rect));
        gapRect = CGRect(rectRight(previous->rect)+1, minY, rectLeft(next->rect) - rectRight(previous->rect) - 1, maxY - minY + 1);
    } else if ((previous == NULL) && ((next != NULL) && (next->ch != ' '))) {
        CGRect refDigit = findClosestDigitLookAlike(next, statsWithoutCurrent, results);
        if (refDigit.size.width > 0) {
            float minY = rectBottom(refDigit);
            float height = refDigit.size.height * 1.10;
            float maxX = rectLeft(next->rect) - 1;
            float width = round(results->globalStats.averageWidthDigits.average * 2.0);
            gapRect = CGRect (maxX - width + 1, minY, width, height);
            if (rectLeft(gapRect) < rectLeft(results->imageRange)) {
                gapRect.origin.x = rectLeft(results->imageRange);
                gapRect.size.width = maxX - gapRect.origin.x + 1;
            }
        } else {
            return false;
        }
    }
    // Make sure gapRect is not too wide (in lines where a large gap exists between text segments)
    if (gapRect.size.width > results->globalStats.averageWidthDigits.average * 2.0) {
        gapRect.origin.x = round(rectRight(gapRect) - results->globalStats.averageWidthDigits.average * 2.0 + 1);
        gapRect.size.width = round(results->globalStats.averageWidthDigits.average * 2.0);
    }
    SingleLetterTests *st = CreateSingleLetterTests(gapRect, results);
    if (st != NULL) {
        ConnectedComponentList cpl = st->getConnectedComponents();
        if (cpl.size() >= 2) {
            float minBottom = gapRect.size.height * 0.60;   // dot must be in bottom 30% of height
            // Typical is 8 pixels away from left/right where gap = 20 (0.40)
            float minX = gapRect.size.width * 0.20;
            float maxX = gapRect.size.width * 0.80;
            for (int k=1; k<cpl.size(); k++) {
                ConnectedComponent cc = cpl[k];
                if ((cc.ymin > minBottom) && (cc.xmin > minX) && (cc.xmax < maxX)) {
                    dotRect = CGRect (rectLeft(gapRect) + cc.xmin, rectBottom(gapRect) + cc.ymin, cc.getWidth(), cc.getHeight());
                    return true;
                }
            }
        }
        delete st;

#if DEBUG
        if ((previous != NULL) && (next != NULL)) {
            DebugLog("possiblePrice: failed to find decimal point between [%c] and [%c] in word [%s]", previous->ch, next->ch, toUTF8(next->word->text()).c_str());
            DebugLog("");
        }
#endif
    }

    return false;
} // findDotBetween

/* Tests a sequence of characters starting at r for a possible price, allowing for:
    - missing decimal dot with a space instead of the dot
    - missing leading $ sign (space instead)
    - must end with a space or end of line
    - must have at least 3 characters (x yy is the shortest price), all of which are either digits or could be a digit (e.g. C instead of 0)
   If satisfied it is looking at a price, will effect the required replacements/insertions before returning true.
   In order to be efficient, the code will first run the quick tests (which don't require image inspections) and only then proceed with more complicated tests.
   NOTE: r must point to the first non-blank character where it is thought a price may begin (optionally a $ sign may be preceding)
   
   Patterns fixed:
   - 2 99 (missing leading $ because decimal dot wasn't found so we didn't search for the $)
   - NOT testing "$4 . 99" because Regexes catch this one
*/

bool possiblePrice(SmartPtr<OCRRect> r, SmartPtr<OCRRect> *lastR, SmartPtr<OCRWord> line, OCRResults *results) {
    // Analyze letters ahead and determine if we have enough characters ahead + if there are 2 chars exactly after the decimal point
    SmartPtr<OCRRect> p = r;
    SmartPtr<OCRRect> decimalPointR;
    SmartPtr<OCRRect> lastPriceRect;
    SmartPtr<OCRRect> dollarRect;
    int numDigitsOrLookAlikeAfterDot = 0, numDigitsOrLookAlikeBeforeDot = 0, numDigits = 0;
    bool foundDecimalPoint = false;
    bool foundDollar = false;
    while (p != NULL) {
        // 2016-03-10 no longer accepting ' ' as decimal point (which caused us to automatically insert a dot), wreaked havoc with the quantity of all products in https://drive.google.com/open?id=0B4jSQhcYsC9VaUk0ZXN0RjdOUTg
        // 2016-03-11 actually reverting to accepting ' ' as decimal point but conditionally: right below we will then look for a '.' pattern in the gap
        if (!foundDecimalPoint && (isDecimalPointLookalike(p->ch) || (p->ch == ' '))) {
            foundDecimalPoint = true;
            decimalPointR = p;
            lastPriceRect = p;
            // If first digit is 0 and next char is not a dot or space (which we will map to a dot if everything else fits), abort because we don't expect prices to start with 0 unless it's 0.XX
            if ((numDigitsOrLookAlikeBeforeDot == 1) && (p->previous != NULL) && (digitForExtendedLookalike(p->previous->ch) == '0')) {
                if (lastR != NULL)
                    *lastR = p;
                return false;
            }
            p = p->next; continue;
        }
        if (p->ch == '$') {
            if (p == r) {
                // Start of pattern
                foundDollar = true;
                dollarRect = p;
                p = p->next; continue;
            } else {
                // Unexpected $ inside price, abort
                return false;
            }
        }
        if (p->ch == '-') {
            // If we are here is means we already found a decimal point so this dash can only be at the end of a price (for discount amounts at Target)
            // Check that we are at the end of the line or there is a space after (but allow ONE additional char since discounts often do have one, such as "1.00-T"
            if ((p->next == NULL) || (p->next->next == NULL) || (p->next->next->ch == ' ')) {
                lastPriceRect = p;
                break;
            } else {
                // Something is wrong, abort
                return false;
            }
        }
        if (isDigitLookalikeExtended(p->ch)) {
            if (foundDecimalPoint) {
                numDigitsOrLookAlikeAfterDot++;
            } else {
                numDigitsOrLookAlikeBeforeDot++;
            }
            if (isDigit(p->ch)) {
                numDigits++;
            }
            lastPriceRect = p;
            p = p->next; continue;
        }
        if (p->ch == ' ') {
            // Could be the end of a legit price, we'll test that after the loop - just break
            break;
        }
        // Not the decimal point, not a digit or digit lookalike, not a space - must be a glued character: don't allow, abort
        return false;
    }
    
    bool madeChanges = false;
    
    // Check for cases where an abnormally low I/1/l (rect possibly fixed 
    
    // Don't fix prices when > $999.99, too risky
    if ((numDigitsOrLookAlikeBeforeDot < 1) || (numDigitsOrLookAlikeBeforeDot > 3) || (numDigitsOrLookAlikeAfterDot != 2))
        return false;
    
    if (foundDecimalPoint && (decimalPointR->ch == ' ')) {
        // Must find '.' pattern in the gap
        CGRect dotRect;
        foundDecimalPoint = findDotBetween(decimalPointR->previous, decimalPointR->next, line, dotRect, results);
        decimalPointR->previous->flags |= FLAGS_TESTED_DOT_AFTER;
        if (foundDecimalPoint) {
            madeChanges = true;
            r->word->updateLetterWithNewCharAndNewRect(decimalPointR, '.', dotRect);
        }
    }
    
    // 2016-03-10 no longer automatically inserting decimal point, wreaked havoc with the quantity of all products
    if (!foundDecimalPoint)
        return false;

    // Got the right number of digits before / after decimal point - look for $ sign preceding
    if (!foundDollar && results->retailerParams.pricesHaveDollar) {
        if (r->flags5 & FLAGS5_TESTED_DOLLAR_BEFORE) {
            // No preceding dollar, and already looked for one - abort
            return false;
        } else if ((r->previous == NULL) || (r->previous->ch == ' ')) {
            CGRect rect = r->rect;
            if ((line->averageHeightDigits.count > 0) && (line->averageWidthDigits.count > 0)) {
                float relevantSpacing = -1;
                if (line->averageSpacingDigits.count > 0)
                    relevantSpacing = line->averageSpacingDigits.average;
                else if (line->averageSpacing.count > 0)
                    relevantSpacing = line->averageSpacing.average;
                if (relevantSpacing > 0) {
                    rect.origin.x -= (line->averageWidthDigits.average + line->averageSpacingDigits.average * 2);
                    rect.size.width = line->averageWidthDigits.average + line->averageSpacingDigits.average * 2;
                    if (rect.origin.x > 0) {
                        r->flags5 |= FLAGS5_TESTED_DOLLAR_BEFORE;
                        SingleLetterTests *stDollar = CreateSingleLetterTests(rect, results);
                        if (stDollar != NULL) {
                            ConnectedComponentList cpl = stDollar->getConnectedComponents();
                            int totalPixels = 0;
                            float minX = 10000, maxX = -1;
                            for (int i=1; i<cpl.size(); i++) {
                                totalPixels += cpl[i].area;
                                if (cpl[i].xmin < minX)
                                    minX = cpl[i].xmin;
                                if (cpl[i].xmax > maxX)
                                    maxX = cpl[i].xmax;
                            }
                            CGRect newRect = rect;
                            newRect.origin.x = rect.origin.x + minX;
                            newRect.size.width = maxX - minX + 1;
                            // Testing for 35% of actual rect around pattern (actually usually around 50%) + test width / height.
                            if ((totalPixels > newRect.size.width * newRect.size.height * 0.35)
                                && (newRect.size.width > line->averageWidthDigits.average * 0.65)
                                && (newRect.size.height > line->averageWidthDigits.average * 0.75)) {
                                // Significant! Assume it's a $ sign
                                ReplacingLog("Validate: replacing, possiblePrice inserting [$] before [%c] in word [%s]", (unsigned short)r->ch, toUTF8(r->word->text()).c_str());
                                r->word->addLetterWithRectConfidenceAfterRect(r->ch, r->rect, r->confidence, r);
                                r->word->updateLetterWithNewCharAndNewRect(r, '$', newRect);
                                foundDollar = true;
                                dollarRect = r;
                                madeChanges = true;
                            }
                            delete stDollar;
                        } // stDollar != NULL
                    } // able to create a rect to the left within bounds of image
                } // Got spacing stats
            } // Got digits stats
        } // previous rect indicates might be a dollar to the left
    }
    
    // If still not found dollar abort
    if (!foundDollar && results->retailerParams.pricesHaveDollar)
        return false;
    
#if DEBUG
    wstring textBefore = r->word->text();
#endif
    
    // Now iterate over all digits lookalikes and replace with corresponding digit!
    p = r;
    int nDigits = 0;
    while (p != NULL) {
        if (!isDigit(p->ch) && ((p->ch != '$') || (nDigits > 0)) && isDigitLookalikeExtended(p->ch)) {
            nDigits++;
            wchar_t ch = digitForExtendedLookalike(p->ch);
            if ((ch != '\0') && (ch != p->ch)) {
                madeChanges = true;
                r->word->updateLetterWithNewCharAndNewRect(p, ch, p->rect);
            }
        }
        if (p == lastPriceRect)
            break;
        p = p->next; continue;
    }
    
    // Fix decimal dot
    if (decimalPointR->ch != '.') {
        madeChanges = true;
        r->word->updateLetterWithNewCharAndNewRect(decimalPointR, '.', decimalPointR->rect);
    }
    
    if (madeChanges) {
        ReplacingLog("Validate: replacing, possiblePrice fixed price into word [%s] --> [%s]", toUTF8(textBefore).c_str(), toUTF8(r->word->text()).c_str());
        ReplacingLog("");
    }
    
    if (lastR != NULL) {
        *lastR = lastPriceRect;
    }
    
    return true;
}

wstring convertDigitsLookalikesToDigits (wstring text) {
    wstring res;
    for (int i=0; i<text.length(); i++) {
        wchar_t ch = text[i];
        wchar_t repCh = digitForExtendedLookalike(ch);
        if (repCh != '\0')
            res += repCh;
        else
            res += ch;
    }
    return res;
}

/* Check if a rect is wide enough to be a normal digit (otherwise it might a '1')
  Return:
    - 1: yes
    - 0: no
    - -1: can't (not enough data)
*/
int isNormalDigitWidth(OCRRectPtr r, CGRect rect, OCRWordPtr &stats, OCRResults *results, bool lax) {
    if ((rect.size.width == 0) || (rect.size.height == 0)) return -1;
    // 1 width is between 5-8 pixels when normal digits are 11-12 => a '1' should be no wider than 8/11 = 73%, anything beyond is a normal digit
    float limit = 0;
    if (stats->averageWidthDigits.count > 0)
        limit = (lax? stats->averageWidthDigits.average * 0.75:stats->averageWidthDigits.average * 0.85);
    if (stats->averageWidthDigits.count >= 2) {
        if (rect.size.width > limit)
            return 1;
        else
            return 0;
    }
    // Not enough digits on the line, try checking height vs height. Normal digit has around w=12 / h=20 (h = 1.7 w), 1 is around w=8 / h=20 (h = 2.5 w). The ratio between ratios is 2.5 / 1.7 = 1.5.
    if ((rect.size.height > rect.size.width * 2.2)
        && ((stats->averageWidthDigits.count == 0) || (rect.size.width > limit)))
        return 1;
    if ((rect.size.height < rect.size.width * 1.5)
        && ((stats->averageWidthDigits.count == 0) || (rect.size.width < limit)))
        return 0;
    return -1;
}

bool foundInVector (vector<int> v, int value) {
    for (int i=0; i<v.size(); i++) {
        if (v[i] == value) return true;
    }
    return false;
}

float myAtoF(const char *text) {
    if (text == NULL)
        return 0;
    float sum = 0;
    int len = (int)strlen(text);
    int numDigits = 0;
    bool foundDot = false;
    int locDot = -1, locFirstDigit = -1;
    // Find location of the dot (if any) and first digit
    for (int i=0; i<=len - 1; i++) {
        char ch = *(text + i);
        if (ch == '.') {
            locDot = i;
            break;
        }
        if ((locFirstDigit == -1) && isDigit(ch)) {
            locFirstDigit = false;
        }
    }
    if ((locDot < 0) || (locFirstDigit < 0))
        return 0;
    
    int numLeadingSpaces = 0, numTrailingSpaces = 0;
    for (int i=0; i<=len - 1; i++) {
        if (*(text + i) == ' ')
            numLeadingSpaces++;
        else
            break;
    }
    for (int i=len - 1; i>=0; i--) {
        if (*(text + i) == ' ')
            numTrailingSpaces++;
        else
            break;
    }
    
    float multiplier = 1;
    // If delta between first digit and dot is 1 don't adjust multiplier. If 1, set to 10 etc
    for (int i=locFirstDigit; i<locDot-1; i++) {
        multiplier *= 10;
    }
    for (int i=numLeadingSpaces; i<=len - 1 - numTrailingSpaces; i++) {
        char ch = *(text + i);

        if (ch == '$') {
            if (i == numLeadingSpaces) // Accept only as leading char
                continue;
            return 0;
        }
        if (ch == '.') {
            if (foundDot || (numDigits == 0))
                return 0;
            foundDot = true;
            continue;
        }
        if (!isDigit(ch))
            return 0;
        if (isDigit(ch)) {
            numDigits++;
            sum += (ch - '0') * multiplier;
            multiplier /= 10;
        }
    }
    return sum;
}

/*
    Checks if a subrect is really its own rect within a larger rect, or else if it touches another set of pixels
*/
bool isGluedRectInCombinedRect (CGRect combinedRect, CGRect subRect, OCRResults *results) {
    SingleLetterTests *st = CreateSingleLetterTests(combinedRect, results);
    if (st != NULL) {
        ConnectedComponentList cl = st->getConnectedComponents();
        bool found = false;
        for (int i=1; i<cl.size(); i++) {
            ConnectedComponent cc = cl[i];
            CGRect adjustedRect;
            adjustedRect.origin.x = rectLeft(combinedRect) + cc.xmin;
            adjustedRect.origin.y = rectBottom(combinedRect) + cc.ymin;
            adjustedRect.size.width = cc.getWidth();
            adjustedRect.size.height = cc.getHeight();
            if ((rectLeft(adjustedRect) == rectLeft(subRect)) && (rectBottom(adjustedRect) == rectBottom(subRect)) && (adjustedRect.size.width == subRect.size.width) && (adjustedRect.size.height == subRect.size.height)) {
                found = true;
                break;
            }
        }
        delete st;
        return (!found);
    }
    return false;
}

int countDiscrepancies (wstring text1, wstring text2) {
    if (text1.length() != text2.length())
        return ((int)max(text1.length(), text2.length()));

    int numDiscrepancies = 0;
    for (int k=0; k<text1.length(); k++) {
        if (text1[k] != text2[k])
            numDiscrepancies++;
    }
    return (numDiscrepancies);
}

int countDiscrepancies (vector<wstring> text1List, vector<wstring> text2List) {
    int bestDiscrepancy = -1;
    size_t maxLength = 0;
    for (int i=0; i<text1List.size(); i++) {
        wstring text1 = text1List[i];
        if (text1.length() > maxLength)
            maxLength = text1.length();
        for (int j=0; j<text2List.size(); j++) {
            wstring text2 = text2List[j];
            if (text2.length() > maxLength)
                maxLength = text2.length();
            int currentDiscrepancy = countDiscrepancies(text1, text2);
            if ((bestDiscrepancy < 0) || (currentDiscrepancy < bestDiscrepancy))
                bestDiscrepancy = currentDiscrepancy;
        }
    }
    if (bestDiscrepancy < 0)
        return ((int)maxLength);
    else
        return (bestDiscrepancy);
}

// Extends current rect on the right side such that it is as wide as a digits, limited by next char or image size. Does not trim to actual pixel patterns
CGRect computeCapturingRectRight(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool trim) {
    CGRect newRect;
    newRect.origin.y = rectBottom(r->rect);
    newRect.size.height = r->rect.size.height;
    newRect.origin.x = rectLeft(r->rect);
    if (r->next != NULL) {
        if (r->next->ch != ' ') {
            newRect.size.width = rectLeft(r->next->rect) - rectLeft(r->rect) - 1;
        } else {
            if (statsWithoutCurrent->averageWidthDigits.average > 0)
                newRect.size.width = statsWithoutCurrent->averageWidthDigits.average;
            else
                newRect.size.width = results->globalStats.averageWidthDigits.average;
            if ((r->next->next != NULL) && (r->next->next->ch != ' ')) {
                int maxSize = rectLeft(r->next->next->rect) - rectLeft(r->rect) - 1;
                if (newRect.size.width > maxSize)
                    newRect.size.width = maxSize;
            }
        }
    } else {
        if (statsWithoutCurrent->averageWidthDigits.average > 0)
            newRect.size.width = statsWithoutCurrent->averageWidthDigits.average;
        else
            newRect.size.width = results->globalStats.averageWidthDigits.average;
    }
    if (rectRight(newRect) > rectRight(results->OCRRange))
        newRect.size.width = rectRight(results->OCRRange) - rectRight(r->rect) + 1;
    newRect.origin.x = roundf(newRect.origin.x);
    newRect.size.width = roundf(newRect.size.width);
    newRect.origin.y = roundf(newRect.origin.y);
    newRect.size.height = roundf(newRect.size.height);
    if (trim) {
        SingleLetterTests *st = CreateSingleLetterTests(newRect, results);
        if (st != NULL) {
            ConnectedComponentList cpl = st->getConnectedComponents(0.01);
            if (cpl.size() >= 1) {
                newRect.origin.x = rectLeft(newRect) + cpl[0].xmin;
                newRect.origin.y = rectBottom(newRect) + cpl[0].ymin;
                newRect.size.width = cpl[0].getWidth();
                newRect.size.height = cpl[0].getHeight();
            }
            delete st;
        }
    }
    return (newRect);
} // computeCapturingRectRight

/*
    Checks the rect to the left of a given OCRRectPr
    - trim: if set, returns only the smallest rect containing all patterns found
    - includeCurrent: if false, returned rect excludes the current pattern
*/
CGRect computeCapturingRectLeft(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool trim, bool includeCurrent) {
    CGRect newRect;
    newRect.origin.y = rectBottom(r->rect);
    newRect.size.height = r->rect.size.height;
    float rightMostX = (includeCurrent? rectRight(r->rect):rectLeft(r->rect) - 1);
    
    if (r->previous != NULL) {
        if (r->previous->ch != ' ') {
            newRect.origin.x = rectRight(r->previous->rect) + 2; // Adding an extra 1 to be on the safe side
            newRect.size.width = rightMostX - newRect.origin.x + 1;
        } else {
            if (statsWithoutCurrent->averageWidthDigits.average > 0)
                newRect.size.width = statsWithoutCurrent->averageWidthDigits.average;
            else
                newRect.size.width = results->globalStats.averageWidthDigits.average;
            // If not including current, we are "fishing" for an entire pattern: need to make it a bit wider
            if (!includeCurrent) {
                if (statsWithoutCurrent->averageSpacing.count > 0)
                    newRect.size.width += statsWithoutCurrent->averageSpacing.average * 3;
                else
                    newRect.size.width *= 1.30;
            }
            if ((r->previous->previous != NULL) && (r->previous->previous->ch != ' ')) {
                int maxSize = rightMostX - rectRight(r->previous->previous->rect); // Not adding 1 to be on the safe side
                if (newRect.size.width > maxSize)
                    newRect.size.width = maxSize;
            }
            newRect.origin.x = rightMostX - newRect.size.width + 1;
        }
    } else {
        if (statsWithoutCurrent->averageWidthDigits.average > 0) {
            newRect.size.width = statsWithoutCurrent->averageWidthDigits.average;
        } else {
            newRect.size.width = results->globalStats.averageWidthDigits.average;
        }
        // If not including current, we are "fishing" for an entire pattern: need to make it a bit wider
        if (!includeCurrent) {
            if (statsWithoutCurrent->averageSpacing.count > 0)
                newRect.size.width += statsWithoutCurrent->averageSpacing.average * 3;
            else
                newRect.size.width *= 1.30;
        }
        newRect.origin.x = rightMostX - newRect.size.width + 1;
    }
    if (rectLeft(newRect) < rectLeft(results->OCRRange)) {
        newRect.origin.x = rectLeft(results->OCRRange);
        newRect.size.width = rightMostX - newRect.origin.x + 1;
    }
    newRect.origin.x = roundf(newRect.origin.x);
    newRect.size.width = roundf(newRect.size.width);
    newRect.origin.y = roundf(newRect.origin.y);
    newRect.size.height = roundf(newRect.size.height);
    if (trim) {
        SingleLetterTests *st = CreateSingleLetterTests(newRect, results);
        if (st != NULL) {
            ConnectedComponentList cpl = st->getConnectedComponents(0.01);
            if (cpl.size() >= 1) {
                newRect.origin.x = rectLeft(newRect) + cpl[0].xmin;
                newRect.origin.y = rectBottom(newRect) + cpl[0].ymin;
                newRect.size.width = cpl[0].getWidth();
                newRect.size.height = cpl[0].getHeight();
            }
            delete st;
        }
    }
    return (newRect);
} // computeCapturingRectLeft

// Calculate the rect composed of all comps to the left of a given rightmost X (included, to allow for cases where left & right sides are touching)
CGRect computeLeftRect(CGRect largerRect, ConnectedComponentList &cpl, int rightMostX) {
    int minX = -1, maxX = -1, minY = -1, maxY = -1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        if (rectLeft(largerRect) + cc.xmax > rightMostX)
            continue;
        if ((minX == -1) || (cc.xmin < minX))
            minX = cc.xmin;
        if ((maxX == -1) || (cc.xmax > maxX))
            maxX = cc.xmax;
        if ((minY == -1) || (cc.ymin < minY))
            minY = cc.ymin;
        if ((maxY == -1) || (cc.ymax > maxY))
            maxY = cc.ymax;
    }
    if ((minX >= 0) && (maxX >= minX) && (minY >=0) && (maxY >= minY))
        return CGRect(rectLeft(largerRect) + minX, rectBottom(largerRect) + minY, maxX - minX + 1, maxY - minY + 1);
    else
        return CGRect(0,0,0,0);
} // computeLeftRect

// Calculate the rect composed of all comps to the right of a given leftmost X (included, to allow for cases where left & right sides are touching)
CGRect computeRightRect(CGRect largerRect, ConnectedComponentList &cpl, int leftMostX) {
    int minX = -1, maxX = -1, minY = -1, maxY = -1;
    for (int i=1; i<cpl.size(); i++) {
        ConnectedComponent cc = cpl[i];
        if (rectLeft(largerRect) + cc.xmin < leftMostX)
            continue;
        if ((minX == -1) || (cc.xmin < minX))
            minX = cc.xmin;
        if ((maxX == -1) || (cc.xmax > maxX))
            maxX = cc.xmax;
        if ((minY == -1) || (cc.ymin < minY))
            minY = cc.ymin;
        if ((maxY == -1) || (cc.ymax > maxY))
            maxY = cc.ymax;
    }
    if ((minX >= 0) && (maxX >= minX) && (minY >=0) && (maxY >= minY))
        return CGRect(rectLeft(largerRect) + minX, rectBottom(largerRect) + minY, maxX - minX + 1, maxY - minY + 1);
    else
        return CGRect(0,0,0,0);
} // computeRightRect


// Return the rect containing all connected comps, i.e. trim empty space. Used when casting a net over empty space before/after chars
CGRect computeTrimmedRect(CGRect largerRect, OCRResults *results) {
    CGRect newRect = largerRect;
    
    SingleLetterTests *st = CreateSingleLetterTests(largerRect, results);
    if (st != NULL) {
        ConnectedComponentList cpl = st->getConnectedComponents();
        if (cpl.size() > 0) {
            newRect = CGRect((CGRect(rectLeft(largerRect) + cpl[0].xmin, rectBottom(largerRect) + cpl[0].ymin, cpl[0].getWidth(), cpl[0].getHeight())));
            delete st;
        }
    }
    
    return (newRect);
}

// Tests top 10% of a rect for a single horizontal segment spanning at least 85% of the width
bool hasFlatTop(CGRect rect, OCRResults *results) {
    bool success = false;
    
    SingleLetterTests *st = CreateSingleLetterTests(rect, results);
    if (st != NULL) {
        SegmentList slTop = st->getHorizontalSegments(0.05, 0.00);
        if ((slTop.size() == 1) && (slTop[0].endPos - slTop[0].startPos >= rect.size.width * 0.90))
            success = true;
        delete st;
    }
    
    return (success);
}

CGRect findClosestDigitLookAlike(OCRRectPtr r, OCRWordPtr &statsWithoutCurrent, OCRResults *results, bool forwardOnly) {
    OCRRectPtr previous = r->previous;
    OCRRectPtr next = r->next;
    while (((previous != NULL) && !forwardOnly) || (next != NULL)) {
        if (!forwardOnly && (previous != NULL) && isDigitLookalikeExtended(previous->ch) && (previous->rect.size.height > statsWithoutCurrent->averageHeightDigits.average * 0.85) && (previous->rect.size.height < statsWithoutCurrent->averageHeightDigits.average * 1.15))
            return previous->rect;
        if ((next != NULL) && isDigitLookalikeExtended(next->ch) && (next->rect.size.height > statsWithoutCurrent->averageHeightDigits.average * 0.85) && (next->rect.size.height < statsWithoutCurrent->averageHeightDigits.average * 1.15))
            return next->rect;
        if (!forwardOnly && (previous != NULL)) previous = previous->previous;
        if (next != NULL) next = next->next;
    }
    // If no luck, try again using global height average
    previous = r->previous;
    next = r->next;
    while (((previous != NULL) && !forwardOnly) || (next != NULL)) {
        if (!forwardOnly && (previous != NULL) && isDigitLookalikeExtended(previous->ch) && (previous->rect.size.height > results->globalStats.averageHeightDigits.average * 0.85) && (previous->rect.size.height < results->globalStats.averageHeightDigits.average * 1.15))
            return previous->rect;
        if ((next != NULL) && isDigitLookalikeExtended(next->ch) && (next->rect.size.height > results->globalStats.averageHeightDigits.average * 0.85) && (next->rect.size.height < results->globalStats.averageHeightDigits.average * 1.15))
            return next->rect;
        if (!forwardOnly && (previous != NULL)) previous = previous->previous;
        if (next != NULL) next = next->next;
    }
    return CGRect(0,0,0,0);
}

// Determines the actual (taller) rect encompassing a pattern. Only if pattern is 20%+ shorter.
CGRect computeTallerRect(OCRRectPtr r, CGRect rect, OCRWordPtr &statsWithoutCurrent, OCRResults *results) {
    CGRect res = rect;
    
    if ((statsWithoutCurrent->averageHeightDigits.count == 0) || (statsWithoutCurrent->averageHeightDigits.average > results->globalStats.averageHeightDigits.average * 1.25) || (statsWithoutCurrent->averageHeightDigits.average < results->globalStats.averageHeightDigits.average * 0.75))
        return res;
    CGRect closestDigit = findClosestDigitLookAlike(r, statsWithoutCurrent, results);
    if (closestDigit.size.width > 0) {
        // Bottom part missing
        if (rectTop(closestDigit) - rectTop(rect) > statsWithoutCurrent->averageHeightDigits.average * 0.20) {
            res = rect;
            res.size.height = statsWithoutCurrent->averageHeightDigits.average;
            if (closestDigit.size.height > res.size.height)
                res.size.height = closestDigit.size.height;
            if (rectTop(res) > rectTop(results->imageRange))
                res.size.height = rectTop(results->imageRange) - rectBottom(res) + 1;
        // Top part missing
        } else if (rectBottom(rect) - rectBottom(closestDigit) > statsWithoutCurrent->averageHeightDigits.average * 0.20) {
            res = rect;
            float newHeight = MAX(statsWithoutCurrent->averageHeightDigits.average, closestDigit.size.height);
            res.origin.y = rectTop(res) - newHeight + 1;
            res.size.height = newHeight;
            if (rectBottom(res) < rectBottom(results->imageRange)) {
                float oldTop = rectTop(res);
                res.origin.y = rectBottom(res);
                res.size.height = oldTop - rectBottom(res) + 1;
            }
        }
    }
    if (res.size.height > 0) {
        SingleLetterTests *st = CreateSingleLetterTests(res, results);
        if (st != NULL) {
            ConnectedComponentList cpl = st->getConnectedComponents(0.01);
            if (cpl.size() >= 1) {
                res.origin.x = rectLeft(res) + cpl[0].xmin;
                res.origin.y = rectBottom(res) + cpl[0].ymin;
                res.size.width = cpl[0].getWidth();
                res.size.height = cpl[0].getHeight();
            }
            delete st;
        }
    }
    return res;
}

// Parses text like 2/2.30 or 2/.60 (Kmart and Walgreens)
bool checkForSlashPricePerItem (wstring pricePerItemText, wstring &actualPricePerItemText, float &actualPricePerItemValue) {
    if (pricePerItemText.length() >= 5) {
        // 2/.60
        wchar_t slashDigit = pricePerItemText[1];
        if (slashDigit == '7') {
            wstring numItemsString = pricePerItemText.substr(0,1);
            int numItems = atoi(toUTF8(numItemsString).c_str());
            if (numItems > 0) {
                wstring totalPriceString = pricePerItemText.substr(2,pricePerItemText.length()-2);
                if (totalPriceString.length() == 3)
                    totalPriceString = L"0" + totalPriceString;
                float totalPriceValue = atof(toUTF8(totalPriceString).c_str());
                if (totalPriceValue > 0) {
                    float actualPricePerItem = totalPriceValue / numItems;
                    char *priceCStr = (char *)malloc(100);
                    sprintf(priceCStr, "%.2f", actualPricePerItem);
                    wstring priceStr = toUTF32(priceCStr);
                    free(priceCStr);
                    actualPricePerItemText = priceStr;
                    actualPricePerItemValue = actualPricePerItem;
                    return true;
                }
            }
        }
    }
    
    return false;
}

wstring canonicalString(wstring text) {
    wstring res;
    for (int i=0; i<text.length(); i++) {
        wchar_t ch = text[i];
        wchar_t newCh = ch;
        switch (ch) {
            case '2':
                newCh = 'Z';
                break;
            case '5':
                newCh = 'S';
                break;
            case '6':
                newCh = 'G';
                break;
            case '8':
                newCh = 'B';
                break;
            case '0':
            case 'D':
            case 'Q':
                newCh = 'O';
                break;
            case '1':
                newCh = 'I';
                break;
            case 'G':
                newCh = 'C';
                break;
            case 'R':
                newCh = 'A';
                break;
            case 'H': // H looks like 'M' actually, we don't think H looks like V but it looks like M and sometimes Blink maps V to M so ...
            case 'Y':
            case 'M':
                newCh = 'V';
                break;
            case '%':
                newCh = 'X';
                break;
            default:
                newCh = ch;
        }
        res = res + newCh;
    }
    return res;
}

bool OCRAwareDigitCompare(wchar_t ch1, wchar_t ch2) {
    if ((ch1 == ch2)
        || ((ch1 == '5') && (ch2 == '6'))
        || ((ch1 == '6') && (ch2 == '5'))
        || ((ch1 == '0') && (ch2 == '8'))
        || ((ch1 == '8') && (ch2 == '0')))
        return true;
    else
        return false;
}

bool OCRAwarePriceCompare(wstring price1Text, wstring price2Text) {
    if (price1Text.length() != price2Text.length()) return false;
    
    bool equal = true;
    for (int i=0; i<price2Text.length(); i++) {
        wchar_t ch1 = price1Text[i], ch2 = price2Text[i];
        if (OCRAwareDigitCompare(ch1, ch2))
            continue;
        equal = false;
        break;
    }
    return equal;
}

// Returns the percentage of matching digits
float OCRAwareProductNumberCompare(wstring text1, wstring text2) {
    if ((text1.length() != text2.length()) || (text1.length() == 0) || (text2.length() == 0)) return false;
    
    int countMatching = 0;
    int n = MIN((int)text1.length(), (int)text2.length());
    for (int i=0; i<n; i++) {
        wchar_t ch1 = text1[i], ch2 = text2[i];
        if (OCRAwareDigitCompare(ch1, ch2))
            countMatching++;
    }
    return ((float)countMatching / (float)n);
}

float totalWidth(SegmentList sl) {
    if (sl.size() >= 1)
        return (sl[sl.size()-1].endPos - sl[0].startPos + 1);
    else
        return 0;
}

float largestGap(SegmentList sl, float *gapStart, float *gapEnd) {
    float maxGap = 0;
    for (int i=0; i<(int)sl.size()-1; i++) {
        float currentGap = sl[i+1].startPos - sl[i].endPos - 1;
        if (currentGap > maxGap) {
            maxGap = currentGap;
            if (gapStart != NULL)
                *gapStart = sl[i].endPos + 1;
            if (gapEnd != NULL)
                *gapEnd = sl[i+1].startPos - 1;
        }
    }
    return maxGap;
}

// Return -1 if fails
float leftMargin(SegmentList sl) {
    if (sl.size() == 0) return -1;
    return (sl[0].startPos);
}

// Return -1 if fails
float rightMargin(SegmentList sl, CGRect rect) {
    if (sl.size() == 0) return -1;
    return (rect.size.width - sl[(int)sl.size()-1].endPos -1);
}

int validateFuzzyResults(vector<wstring> fuzzyTextList, float index0Score, wstring scannedText, bool *exact) {

    if (fuzzyTextList.size() == 0) return -1;
    
    DebugLog("validateFuzzyResults [%s] called with top fuzzy [%s]", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());

//#if DEBUG
//    if (scannedText == L"AG GRAD COUNTER CR 0369") {
//        DebugLog("Top fuzzy for [%s] is [%s]", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());
//        DebugLog("");
//    }
//#endif


//#if DEBUG
//    if ((fuzzyTextList[0] == L"GEA CHIA BARS W/RF") || (scannedText == L"GEA CHIA BARS W/RF")) {
//        DebugLog("Top fuzzy for [%s] is [%s]", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());
//        DebugLog("");
//    }
//    if ((fuzzyTextList[0].find(L"CHIA") != wstring::npos) || (scannedText.find(L"CHIA") != wstring::npos)) {
//        DebugLog("Top fuzzy for [%s] is [%s]", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());
//        DebugLog("");
//    }
//
//#endif
    
    // Even if index 0 is acceptable, try to find an exact match in the list - better
    // Before failing see if there is in the list an exact match using cannonical forms
    wstring canonicalScannedText = canonicalString(scannedText);
    
    int bestIndex = -1;
    int bestMistakes = -1;
    for (int i=0; i<fuzzyTextList.size(); i++) {
        wstring fuzzyText = canonicalString(fuzzyTextList[i]);
        if (fuzzyText == canonicalScannedText) {
            // That must be it!
            bestIndex = 0; bestMistakes = 0;
            break;
        }
        // Not exact match, try testing the number of mistakes
        if (fuzzyText.length() == canonicalScannedText.length()) {
            int currentMistakes = countMistakes(fuzzyText, canonicalScannedText);
            if (currentMistakes < (int)canonicalScannedText.length() * 0.10) {
                if ((bestIndex < 0) || (currentMistakes < bestMistakes)) {
                    bestIndex = i; bestMistakes = currentMistakes;
                }
            }
        }
    }
    
    if (bestIndex >= 0) {
        if (bestIndex != 0) {
            if (bestMistakes > 0) {
                DebugLog("Selected fuzzy[%d] = [%s] (instead of [%s]) for [%s] WITH %d MISTAKES", bestIndex, toUTF8(fuzzyTextList[bestIndex]).c_str(), toUTF8(fuzzyTextList[0]).c_str(), toUTF8(scannedText).c_str(), bestMistakes);
            } else {
                DebugLog("Selected fuzzy[%d] = [%s] (instead of [%s]) for [%s]", bestIndex, toUTF8(fuzzyTextList[bestIndex]).c_str(), toUTF8(fuzzyTextList[0]).c_str(), toUTF8(scannedText).c_str());
            }
        }
        if (exact != NULL) {
            if (bestMistakes == 0)
                *exact = true;
            else
                *exact = false;
        }
        return bestIndex;
    }
    
    if (exact != NULL)
        *exact = false;
    bool index0Acceptable = false;
    // For now don't take for granted even if score > 2
    /* if (index0Score > 2) {
        index0Acceptable = true;
    } else */ {
        /* Accept top result if:
            - same number of words as scanned text or +/- 1
            - at least 2/3 of words have identical length
            - at least 1/3 of words identical
         */
        wstring fuzzyText0 = fuzzyTextList[0];
        vector<wstring> fuzzyTextWords;
        split(canonicalString(fuzzyText0), ' ', fuzzyTextWords);
        vector<wstring> scannedTextWords;
        split(canonicalString(scannedText), ' ', scannedTextWords);

        bool accept = true;
        if (abs((int)fuzzyTextWords.size() - (int)scannedTextWords.size()) > 1) {
            accept = false;
        }
        if (accept) {
            vector<wstring> shortList = ((scannedTextWords.size() >= fuzzyTextWords.size())? fuzzyTextWords:scannedTextWords);
            vector<wstring> longList = ((scannedTextWords.size() >= fuzzyTextWords.size())? scannedTextWords:fuzzyTextWords);
            bool deltaLocked = false;   // If we ever need to use the difference in words count to get a match for the text, stick to that difference from now on, so that for example, when comparing THE FOX RAN to FOX THE RAN LOOP, you don't score a point for THE = THE (assumes short list is missing the first word) and RAN = RAN (assumes the short list is missing the last word) - can't have both assumptions at the same time!
            int deltaValue = 0;
            int countMatching = 0, countIndenticalLengths = 0;
            for (int i=0; i<shortList.size(); i++) {
                if (deltaLocked) {
                    if ((i + deltaValue < longList.size())
                        && (i + deltaValue >= 0)) {
                        if (shortList[i] == longList[i+deltaValue]) {
                            countMatching++;
                            countIndenticalLengths++;
                        } else if (shortList[i].length() == longList[i+deltaValue].length()) {
                            countIndenticalLengths++;
                        }
                    }
                } else {
                    if (shortList[i] == longList[i]) {
                        countMatching++;
                        deltaValue = 0; deltaLocked = true;
                        // Now we must revisit the count of indentical lenghts now that we know the delta
                        countIndenticalLengths = 1;
                        for (int j=0; j<i; j++) {
                            if ((j + deltaValue >= 0)
                                && (j + deltaValue < longList.size())) {
                                if (shortList[j].length() == longList[j+deltaValue].length())
                                    countIndenticalLengths++;
                            }
                        }
                    } else if ((i>0) && (shortList[i] == longList[i-1])) {
                        countMatching++;
                        deltaValue = -1; deltaLocked = true;
                    } else if ((i + 1 < longList.size()) && (shortList[i] == longList[i+1])) {
                        countMatching++;
                        deltaValue = 1; deltaLocked = true;
                    }
                }
            }
            if ((countMatching >= shortList.size() * 0.33)
                && (countIndenticalLengths >= shortList.size() * 0.66))
                accept = true;
            else {
                DebugLog("REJECTING top fuzzy for [%s] is NOT [%s]  (matching words=%d, matching lenghts=%d)", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str(), countMatching, countIndenticalLengths);
                accept = false;
            }
        } // if accept
        if (accept)
            index0Acceptable = true;
    }
    if (index0Acceptable) {
        DebugLog("Accepting top fuzzy results for [%s] = [%s] (even though not an exact match)", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());
        return 0;
    } else {
        DebugLog("REJECTING all fuzzy results for [%s] (top is [%s])", toUTF8(scannedText).c_str(), toUTF8(fuzzyTextList[0]).c_str());
        return -1;
    }
}

bool fuzzyCompare(wstring text1, wstring text2) {
    vector<wstring> words1;
    split(canonicalString(text1), ' ', words1);
    vector<wstring> words2;
    split(canonicalString(text2), ' ', words2);
    
    if (abs((int)words1.size() - (int)words2.size()) >= 2)
        return false;
    vector<wstring> wordsLongText = ((words1.size()>=words2.size())? words1:words2);
    vector<wstring> wordsShortText = ((words1.size()<words2.size())? words1:words2);
    
    int deltaWordCounts = (int)wordsLongText.size() - (int)wordsShortText.size();
    bool matching = false;
    for (int i=0; i<wordsShortText.size(); i++) {
        if (wordsShortText[i].length() >= 3) {
            if ((wordsShortText[i] == wordsLongText[i])
                || ((deltaWordCounts > 0)
                    && (((i>0) && (wordsShortText[i] == wordsLongText[i-1]))
                        || (wordsShortText[i] == wordsLongText[i+1])))) {
#if DEBUG
                DebugLog("fuzzyCompare [%s] ~= [%s] (based on [%s])", toUTF8(text1).c_str(), toUTF8(text2).c_str(), toUTF8(wordsShortText[i]).c_str());
#endif
                matching = true;
                break;
            }
        }
    }
    return matching;
}

// Compares two strings, using canonical form, allowing for a possible situation whereby in one string a character got matched as two chars
float OCRAwareTextCompare(wstring text1, wstring text2) {

    vector<wstring> words1;
    split(canonicalString(text1), ' ', words1);
    vector<wstring> words2;
    split(canonicalString(text2), ' ', words2);
    
    // Eliminate words with only one punctuation sign (e.g. .,/,* etc)
    for (int i=0; i<words1.size(); i++) {
        wstring t = words1[i];
        if ((t.length() == 1) && !isLetterOrDigit(t[0])) {
            words1.erase(words1.begin() + i);
            i--;
        }
    }
    
    for (int i=0; i<words2.size(); i++) {
        wstring t = words2[i];
        if ((t.length() == 1) && !isLetterOrDigit(t[0])) {
            words2.erase(words2.begin() + i);
            i--;
        }
    }
    
    if ((words1.size() == 0) || (words2.size() == 0) || (words1.size() != words2.size()))
        return 0;
    
    int countRelevant = 0; int countMatching = 0;
    for (int i=0; i<words1.size(); i++) {
        wstring t1 = words1[i]; wstring t2 = words2[i];
        wstring shortWord = ((t1.length() < t2.length())? t1:t2);
        wstring longWord = ((t1.length() < t2.length())? t2:t1);
        bool deltaLocked = false;   // When set. means all compares will use the delta value
        int delta = 0;
        if (shortWord.length() == longWord.length())
            deltaLocked = true; // Never try to use a delta
        countRelevant += shortWord.length();
        for (int j=0; j<shortWord.length(); j++) {
            wchar_t ch1 = shortWord[j];
            if (longWord[j+delta] == ch1)
                    countMatching++;
            else if (!deltaLocked) {
                delta = 1; // Can't match this one, assume it's because the other word mapped this char to 2 chars, starting with next char, compare with +1 in the other string. Example: we are comparing BOA to BIIA, already matched B, now we test O: doesn't match, but set up delta such that we try A == A next
                deltaLocked = true;
            }
        }
    }

    if (countRelevant > 0)
        return ((float)countMatching / (float)countRelevant);
    else {
        return 0;
    }
}

int countMistakes(wstring text1, wstring text2) {
    int totalMistakes = 0;
    
    for (int i=0; i<text1.length() && i<text2.length(); i++) {
        if (text1[i] != text2[i]) totalMistakes++;
    }
    
    totalMistakes += (abs((int)text1.length() - (int)text2.length()));
    
    return totalMistakes;
}

/* Calculates the tighest rect encompassing the given connected components list (i.e. trims empty space) and updates combinedRect.
   Returns true if it needed to update combinedRect
 */
bool computeTrimmedRect(CGRect &combinedRect, ConnectedComponentList cpl) {

    if (cpl.size() < 2)
        return false;
    
    if ((cpl[0].xmin > 0)
        || (cpl[0].ymin > 0)
        || (cpl[0].getWidth() < combinedRect.size.width)
        || (cpl[0].getHeight() < combinedRect.size.height)) {
        CGRect newRect = combinedRect;
        newRect.origin.x = rectLeft(combinedRect) + cpl[0].xmin;
        newRect.size.width = cpl[0].getWidth();
        newRect.origin.y = rectBottom(combinedRect) + cpl[0].ymin;
        newRect.size.height = cpl[0].getHeight();
        combinedRect = newRect;
        return true;
    }
    
    return false;
}

vector<CGRect> CGRectsForCpl(ConnectedComponentList cpl) {
    vector<CGRect> rects;
    
    for (int i=1; i<cpl.size(); i++) {
        CGRect r (cpl[i].xmin, cpl[i].ymin, cpl[i].getWidth(), cpl[i].getHeight());
        rects.push_back(r);
    }
    
    return rects;
}

// Adjust a sub-rect to a rect with World coordinates
CGRect adjustedRect(CGRect r, CGRect largerRect) {
    return (CGRect(rectLeft(largerRect) + rectLeft(r), rectBottom(largerRect) + rectBottom(r), r.size.width, r.size.height));
}

/*  parseRectIntoLeftRight
    - if it determines it's really just one set of connected comps, sets leftRect / rightRect to zero GGRect
    - otherwise creates two CGRects (in World coordinates( with left/right sides not touching
    Return value: 
    - true if all OK (even if no left/right rects set, false if something went wrong
    - adjusts combinedRect if necessary (trims empty space). Means caller may want to re-compute his SingleLetterTest class to match
 */
bool parseRectIntoLeftRight(CGRect &combinedRect, ConnectedComponentList cplCombined, CGRect &leftRect, CGRect &rightRect, bool trimRect) {

    if (cplCombined.size() < 2)
        return false;
    
    // Trim combinedRect if necessary
    if (trimRect)
        computeTrimmedRect(combinedRect, cplCombined);
    
    if (cplCombined.size() == 2) {
        rightRect = CGRect(0,0,0,0); // No right rect
        leftRect = CGRect(0,0,0,0); // No left rect
        return true;
    }
    
    // Import cplCombined into a vector of rects
    vector<CGRect> combinedRects = CGRectsForCpl(cplCombined);
    
    // Find rightmost / leftmost rect
    
    // First find leftmost rect: pick the one with lowest rectLeft() and if multiple with same rectLeft(), pick the one extending the least to the right
    int indexLeftMostRect = -1;
    CGRect leftMostRect (0,0,0,0);
    for (int i=0; i<combinedRects.size(); i++) {
        CGRect currentRect = combinedRects[i];
        if ((indexLeftMostRect < 0)
            || (rectLeft(currentRect) < rectLeft(leftMostRect))
            || ((rectLeft(currentRect) == rectLeft(leftMostRect)) && (rectRight(currentRect) < rectRight(leftMostRect)))) {
            indexLeftMostRect = i;
            leftMostRect = currentRect;
            continue;
        }
    }
    
    // Now get the rightmost rect: pick the one with highest rectRight() and if multiple with same rectRight(), pick the one extending the least to the left
    int indexRightMostRect = -1;
    CGRect rightMostRect (0,0,0,0);
    for (int i=0; i<combinedRects.size(); i++) {
        if (i == indexLeftMostRect) continue;
        CGRect currentRect = combinedRects[i];
        if ((indexRightMostRect < 0)
            || (rectRight(currentRect) > rectRight(rightMostRect))
            || ((rectRight(currentRect) == rectRight(rightMostRect)) && (rectLeft(currentRect) > rectLeft(rightMostRect)))) {
            indexRightMostRect = i;
            rightMostRect = currentRect;
            continue;
        }
    }
    
    if ((indexLeftMostRect <= 0) || (indexRightMostRect <= 0))
        return false;
    
    // Remove leftmost / right most from list (make sure to remove the one with higher index first so that the lower index remains valid)
    if (indexRightMostRect > indexLeftMostRect) {
        combinedRects.erase(combinedRects.begin() + indexRightMostRect);
        combinedRects.erase(combinedRects.begin() + indexLeftMostRect);
    } else {
        combinedRects.erase(combinedRects.begin() + indexLeftMostRect);
        combinedRects.erase(combinedRects.begin() + indexRightMostRect);
    }
    
    // Now go over remaining rects and add left or right according to which side they touch. Repeat iteration until the number of elements in the list of rects doesn't change: we do this in case say a rect on the right touches a rect which touches the rightmost rect - can't handle the first rect until we add the one to its right, i.e. need multiple passes
    while (combinedRects.size() > 0) {
        size_t lastSize = combinedRects.size();
        for (int i=0; i<combinedRects.size(); i++) {
            CGRect currentRect = combinedRects[i];
            float overlapWithLeft = rectRight(leftMostRect) - rectLeft(currentRect);
            float overlapWithRight = rectRight(currentRect) - rectLeft(rightMostRect);
            // Overlapping or just adjacent
            if ((overlapWithLeft >= 0) && (overlapWithLeft > overlapWithRight)) {
                leftMostRect = CreateCombinedRect(leftMostRect, currentRect);
                combinedRects.erase(combinedRects.begin() + i);
                i--; continue;
            } else if ((overlapWithRight >= 0) && (overlapWithRight > overlapWithLeft)) {
                rightMostRect = CreateCombinedRect(currentRect, rightMostRect);
                combinedRects.erase(combinedRects.begin() + i);
                i--; continue;
            }
        }
        if (combinedRects.size() == lastSize)
            break;
    }
  
    //  --------------
    //0|              |
    //1|           1  |
    //2|          11  |
    //3| 11       11  |
    //4| 11       111 |
    //5| 11       111 |
    //6| 11      111  |
    //7| 111     111  |
    //8| 111     111  |
    //9| 111     1111 |
    //a| 1111    1111 |
    //b|  11    11111 |
    //c| 111    1 111 |
    //d| 11     1 111 |
    //e| 11  1111 111 |
    //f| 11  111  111 |
    //#| 11   11  111 |
    //#| 11   11  111 |
    //#| 111  11  111 |
    //#| 111       11 |
    //#| 11        11 |
    //#| 111      111 |
    //#| 111      111 |
    //#| 111      111 |
    //#| 111      111 |
    //#| 111       11 |
    //#|              |
    //  --------------
    // If left & right actually overlap (i.e. accept the above where they just touch in terms of X coords but are distinct)
    if (rectRight(leftMostRect) > rectLeft(rightMostRect) - 1) {
        // This is just one single rect!
        return false;
    }
    
    if (combinedRects.size() > 0) {
        // We have a middle element - for now just fail the call
        return false;
    }
    
    // Success, we have left & right elements
    rightRect = adjustedRect(rightMostRect, combinedRect);
    leftRect = adjustedRect(leftMostRect, combinedRect);
    
    return true;
}

bool hasRightRIndent(CGRect rect, SingleLetterTests *st, bool lax) {
    LimitedOpeningsTestResults resRight;

    bool successRight = st->getOpeningsLimited(resRight, SingleLetterTests::Right,
        0.30,      // Start of range to search (top/left)
        0.85,    // End of range to search (bottom/right)
        0.40,    // Depth from right we wish to test
        SingleLetterTests::Bound,   // Require start (top/left) bound
        SingleLetterTests::Bound,  // Require end (bottom/right) bound
        false,  // Not only main comp
        true    // Within range only
        );
        
    if (successRight && (resRight.minWidthDepth > rect.size.width * (lax? 0.12:0.18))
        && (resRight.minWidthStartCoord > rect.size.height * 0.40)
        && (resRight.minWidthEndCoord < rect.size.height * 0.75)) {
        return true;
    } else {
        return false;
    }
}

//  --------------
//0|              |
//1|   1       1  |
//2|  11       11 |
//3|  11       11 |
//4|  11       11 |
//5|  11       11 |
//6|  11       11 |
//7|  11       11 |
//8|  11       11 |
//9|  11       11 |
//a|  11       11 |
//b|  11 1 111111 |
//c|  1111 111111 |
//d|  11 1   1111 |
//e|  11      111 |
//f|  11      111 |
//#|  11      111 |
//#|  11      111 |
//#|  11      111 |
//#|  11      111 |
//#| 111      111 |
//#| 111      111 |
//#|  11      111 |
//#|  11       1  |
//#|              |
//  --------------
// 'H'
wchar_t testH (testData &t, CGRect &combinedRect) {
    if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; }
    if (!t.gotCombinedLengthSlLeft18) { t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
    if (!t.gotSlRight18) { t.slRight18 = t.stCombined->getVerticalSegments(0.91, 0.18); t.gotSlRight18 = true; }
    if (!t.gotCombinedLengthSlRight18) { t.combinedLengthRight18 = combinedLengths(t.slRight18); t.gotCombinedLengthSlRight18 = true; }
    if ((t.combinedLengthSlLeft18 > combinedRect.size.height * METRICS_SIDELENGTH)
        && (t.combinedLengthRight18 > combinedRect.size.height * METRICS_SIDELENGTH)) {
        if (!t.gotGapBottom25) { if (!t.gotSlBottom25) { t.slBottom25 = t.stCombined->getHorizontalSegments(0.875, 0.25); t.gotSlBottom25 = true; } t.gapBottom25 = largestGap(t.slBottom25, &t.gapBottom25Start, &t.gapBottom25End); t.gotGapBottom25 = true; }
        if (!t.gotGapTop25) { if (!t.gotSlTop25) { t.slTop25 = t.stCombined->getHorizontalSegments(0.125, 0.25); t.gotSlTop25 = true; } t.gapTop25 = largestGap(t.slTop25, &t.gapTop25Start, &t.gapTop25End); t.gotGapTop25 = true; }
        if ((t.gapTop25 > combinedRect.size.width * METRICS_GAP_LETTER_OPENATBOTTOM)
            && (t.gapBottom25 > combinedRect.size.width * METRICS_GAP_LETTER_OPENATBOTTOM)) {
            // Check almost closed middle section
            if (!t.gotCombinedLengthSlMiddle50w25) { if (!t.gotSlMiddle50w25) { t.slMiddle50w25 = t.stCombined->getHorizontalSegments(0.50, 0.25); t.gotSlMiddle50w25 = true; } t.combinedLengthSlMiddle50w25 = combinedLengths(t.slMiddle50w25); t.gotCombinedLengthSlMiddle50w25 = true; }
            float widthSlMiddle50w25 = totalWidth(t.slMiddle50w25);
            if ((widthSlMiddle50w25 > 0.80 * combinedRect.size.width)
                && ((t.combinedLengthSlMiddle50w25 > 0.85 * widthSlMiddle50w25)
                    || (t.combinedLengthSlMiddle50w25 > 0.80 * combinedRect.size.width))) {
                return 'H';
            } // solid middle
            else {
                //  ---------------
                //0|               |
                //1|  11        1  |
                //2|  11        11 |
                //3|  1         11 |
                //4|  11       111 |
                //5|  11       111 |
                //6|  11       111 |
                //7|  11       111 |
                //8|  11        1  |
                //9|  11       11  |
                //a|  11       111 |
                //b|  11       111 |
                //c|  1111 1  1111 |
                //d| 11111 1  1111 |
                //e|  111  1  1111 |
                //f|  11        11 |
                //#| 111           |
                //#|  11       111 |
                //#|  11       111 |
                //#|  11       111 |
                //#| 111       111 |
                //#| 111       111 |
                //#| 111       111 |
                //#| 111       111 |
                //#| 111       111 |
                //#| 111       111 |
                //#|  1            |
                //#|               |
                //  ---------------
                // Still accept if middle has thick left/right sides compared to top/bottom
                if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15 = true;}
                if (!t.gotSlBottom15) { t.slBottom15 = t.stCombined->getHorizontalSegments(0.925, 0.15); t.gotSlBottom15 = true;}
                if ((t.slTop15.size() == 2) && (t.slBottom15.size() == 2) && (t.slMiddle50w25.size() >= 2)) {
                    float widthTopLeft15 = t.slTop15[0].endPos - t.slTop15[0].startPos + 1;
                    float widthTopRight15 = t.slTop15[1].endPos - t.slTop15[1].startPos + 1;
                    float widthMiddleLeft = t.slMiddle50w25[0].endPos - t.slMiddle50w25[0].startPos + 1;
                    float widthMiddleRight = t.slMiddle50w25[t.slMiddle50w25.size()-1].endPos - t.slMiddle50w25[t.slMiddle50w25.size()-1].startPos + 1;
                    float widthBottomLeft15 = t.slBottom15[0].endPos - t.slBottom15[0].startPos + 1;
                    float widthBottomRight15 = t.slBottom15[1].endPos - t.slBottom15[1].startPos + 1;
                    if ((MAX(widthTopLeft15, widthTopRight15) < MIN(widthMiddleLeft, widthMiddleRight))
                        && (MAX(widthBottomLeft15, widthBottomRight15) < MIN(widthMiddleLeft, widthMiddleRight))
                        && (widthSlMiddle50w25 > 0.80 * combinedRect.size.width)
                        && ((t.combinedLengthSlMiddle50w25 > 0.75 * widthSlMiddle50w25)
                            || (t.combinedLengthSlMiddle50w25 > 0.70 * combinedRect.size.width))) {
                            return 'H';
                    }
                }
            }
        } // wide openings top & bottom
    } // Tall left & right sides

    return '\0';
} // testH

//  ----------------
//0|                |
//1| 11             |
//2|  111 1  1      |
//3|  11111 11111   |
//4|  111 1  1111   |
//5| 1111      111  |
//6| 111       111  |
//7|  11        11  |
//8|  11        11  |
//9|  11        11  |
//a|  11        11  |
//b|  11        111 |
//c|  11        11  |
//d|  111      111  |
//e|  111    11111  |
//f|  111  1111111  |
//#|  111   11111   |
//#|  111     11    |
//#|  111           |
//#|  111           |
//#|  111           |
//#|  11            |
//#|  111           |
//#|  111           |
//#|  111           |
//#|  111           |
//#|  111           |
//#|                |
//  ----------------
// 'P'
wchar_t testP (testData &t, CGRect &combinedRect) {
    if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; }
    if (!t.gotCombinedLengthSlLeft18) { t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
    if (t.combinedLengthSlLeft18 > combinedRect.size.height * METRICS_SIDELENGTH) {
        if (!t.gotSlW85w30) { t.slW85w30 = t.stCombined->getVerticalSegments(0.85, 0.30); t.gotSlW85w30 = true;}
        // Right side starts high and doesn't drop too low
        if ((t.slW85w30.size() == 1) && (t.slW85w30[0].startPos < combinedRect.size.height * 0.25) && (t.slW85w30[0].endPos < combinedRect.size.height * 0.75)) {
            if ((t.twoRects) && isCurved(t.rightRect, t.stRight, SingleLetterTests::Left))
            return 'P';
        }
    } // Tall left side

    return '\0';
} // testP

//  ----------
//0|          |
//1| 1     1  |
//2| 1     11 |
//3| 1     11 |
//4| 11    11 |
//5| 1     11 |
//6| 1     11 |
//7| 1     11 |
//8| 1  11 11 |
//9| 1  1  11 |
//a| 1  1  11 |
//b| 1  11 11 |
//c| 1  11 1  |
//d| 11  111  |
//e|  11  11  |
//f|  11  1   |
//#|          |
//  ----------
// Top opening = 5 out of 8 (62.5%)
// Bottom opening = 2 out of 8 (25%)
// Middle element [7-11] midpoint = 8.5 out of 15 (57%) width = 5 (30%)

//  -------------
//0|             |
//1|  1       11 |
//2| 11       11 |
//3| 11      111 |
//4| 11      111 |
//5| 11       11 |
//6| 11       11 |
//7| 11   1   11 |
//8| 11  11   11 |
//9| 11   1   11 |
//a| 11   1   11 |
//b| 11  111  11 |
//c| 11  111  1  |
//d| 11  1 1  1  |
//e| 11 1     1  |
//f| 11      11  |
//#| 1111   111  |
//#|  111   111  |
//#|  111   111  |
//#|  11    11   |
//#|  11    11   |
//#|  11    11   |
//#|  11    11   |
//#|         1   |
//#|             |
//  -------------
// Top opening = 7 out of 11 (64%)
// Bottom opening = 4 out of 11 (36%%)
// Middle element [6-12] midpoint = 8.5 out of 23 (37%) width = 7 (30%)

// 'W'
wchar_t testW(testData &t, CGRect &combinedRect, OCRResults *results) {

    if (!t.gotGapSlTop15) { if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15 = true; } t.gapSlTop15 = largestGap(t.slTop15); t.gotGapSlTop15 = true; }
    if (!t.gotGapSlBottom15) { if (!t.gotSlBottom15) { t.slBottom15 = t.stCombined->getHorizontalSegments(0.925, 0.15); t.gotSlBottom15 = true; } t.gapSlBottom15 = largestGap(t.slBottom15); t.gotGapSlBottom15 = true; }
    if ((t.gapSlTop15 > METRICS_GAP_LETTER_OPENATBOTTOM * combinedRect.size.width)
        && (t.gapSlBottom15 > 0.20 * combinedRect.size.width)
        && (t.gapSlBottom15 < t.gapSlTop15)) {
        // Bottom part indented left & right compared to top part
        bool bottomPartIndented = false;
        if ((t.slTop15.size() >= 2) && (t.slBottom15.size() >= 2)
            && (t.slBottom15[0].startPos > t.slTop15[0].startPos)
            && (t.slBottom15[(int)t.slBottom15.size()-1].endPos < t.slTop15[(int)t.slTop15.size()-1].endPos)) {
            bottomPartIndented = true;
        } else if ((t.slTop15.size() >= 2) && (t.slBottom15.size() >= 2)
            && (t.slBottom15[0].startPos >= t.slTop15[0].startPos)
            && (t.slBottom15[(int)t.slBottom15.size()-1].endPos <= t.slTop15[(int)t.slTop15.size()-1].endPos)) {
            if (!t.gotSlBottom10) { t.slBottom10 = t.stCombined->getHorizontalSegments(0.95, 0.10); t.gotSlBottom10 = true; }
            // Try with smaller bottom section
            if ((t.slBottom10.size() >= 2)
                && (t.slBottom10[0].startPos > t.slTop15[0].startPos)
                && (t.slBottom10[(int)t.slBottom10.size()-1].endPos < t.slTop15[(int)t.slTop15.size()-1].endPos)) {
                bottomPartIndented = true;
            } else if (((t.slBottom15[0].startPos >= t.slTop15[0].startPos)
            && (t.slBottom15[(int)t.slBottom15.size()-1].endPos < t.slTop15[(int)t.slTop15.size()-1].endPos))
                || ((t.slBottom15[0].startPos > t.slTop15[0].startPos)
            && (t.slBottom15[(int)t.slBottom15.size()-1].endPos <= t.slTop15[(int)t.slTop15.size()-1].endPos))) {
                // One last chance: only one side is idented at bottom (other side is flush), add another test to make sure: presence of two indent from top on left/right
                //  --------------
                //0|              |
                //1|  11       11 |
                //2|  11      111 |
                //3|  11      111 |
                //4|  11      111 |
                //5|  11      111 |
                //6|  11       1  |
                //7|  11      11  |
                //8| 111      11  |
                //9|  11  11  111 |
                //a|  1   11  11  |
                //b|  1   1   11  |
                //c|  1  111  11  |
                //d|  1  11   11  |
                //e| 11  1  1 11  |
                //f|  1  1  1111  |
                //#|  1111  1111  |
                //#|   111  1111  |
                //#|  1111  1111  |
                //#|  111    11   |
                //#|  111    11   |
                //#|  111    11   |
                //#|   11         |
                //#|              |
                //  --------------
                CGRect rectBottomLeft (rectLeft(combinedRect), rectBottom(combinedRect) + combinedRect.size.height * 0.55, combinedRect.size.width * 0.50, combinedRect.size.height * 0.45);
                CGRect rectBottomRight (rectLeft(combinedRect) + combinedRect.size.width * 0.50, rectBottom(combinedRect) + combinedRect.size.height * 0.55, combinedRect.size.width * 0.50, combinedRect.size.height * 0.45);
                SingleLetterTests *stBottomLeft = CreateSingleLetterTests(rectBottomLeft, results, false, 0, 0.03, true);
                SingleLetterTests *stBottomRight = CreateSingleLetterTests(rectBottomRight, results, false, 0, 0.03, true);
                if ((stBottomLeft != NULL) && (stBottomRight != NULL)) {
                    OpeningsTestResults resTop;
                    bool success = stBottomLeft->getOpenings(resTop, SingleLetterTests::Top,
                         0.00,      // Start of range to search (top/left)
                         1.00,      // End of range to search (bottom/right)
                         SingleLetterTests::Bound,   // Require start (top/left) bound
                         SingleLetterTests::Bound  // Require end (bottom/right) bound
                         );
                    if (success) {
                        bool successRight = stBottomRight->getOpenings(resTop, SingleLetterTests::Top,
                         0.00,      // Start of range to search (top/left)
                         1.00,      // End of range to search (bottom/right)
                         SingleLetterTests::Bound,   // Require start (top/left) bound
                         SingleLetterTests::Bound  // Require end (bottom/right) bound
                         );
                        if (successRight) {
                            bottomPartIndented = true;
                        }
                    }
                    delete stBottomLeft; delete stBottomRight;
                }
            }
        }
        if (bottomPartIndented) {
            if (!t.gotSlH57w20) { t.slH57w20 = t.stCombined->getHorizontalSegments(0.57, 0.20); t.gotSlH57w20 = true; }
            if ((t.slH57w20.size() == 3)
                && (t.slH57w20[0].startPos < combinedRect.size.width * 0.15)
                && (t.slH57w20[2].endPos + 1 > combinedRect.size.width * 0.85)) {
                return 'W';
            } else {
                if (!t.gotSlH37w20) { t.slH37w20 = t.stCombined->getHorizontalSegments(0.37, 0.20); t.gotSlH37w20 = true; }
                if ((t.slH37w20.size() == 3)
                    && (t.slH37w20[0].startPos < combinedRect.size.width * 0.15)
                    && (t.slH37w20[2].endPos + 1 > combinedRect.size.width * 0.85)) {
                    return 'W';
                }
            }
        } // Indents at bottom
    } // Suitable gaps top/bottom
    
    return '\0';
} // testW


    //  --------------
    //0|              |
    //1|          11  |
    //2| 111      11  |
    //3| 111      11  |
    //4| 111     111  |
    //5| 111     11   |
    //6| 1111    11   |
    //7| 1111    111  |
    //8|  111    111  |
    //9|  1111    11  |
    //a| 111   11 11  |
    //b|  11 1111 11  |
    //c| 111  11  11  |
    //d| 111  11  11  |
    //e| 111      11  |
    //f|  11      11  |
    //#|  11      11  |
    //#| 111      11  |
    //#| 111      11  |
    //#| 111      11  |
    //#| 111      11  |
    //#| 111      111 |
    //#|  11       1  |
    //#|              |
    //  --------------
    // Middle comp: [4,9 - 7,12] w=4,h=4 vs height 22 (Y range 41%-55%, X range 33%-58%)
    // 'M', see https://drive.google.com/open?id=0B4jSQhcYsC9VdWtwVEtGakpwbmc
wchar_t testM (testData &t, CGRect &combinedRect, OCRResults *results) {
    if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; }
    if (!t.gotSlRight18) { t.slRight18 = t.stCombined->getVerticalSegments(0.91, 0.18); t.gotSlRight18 = true; }
    if (!t.gotCombinedLengthSlLeft18) { t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
    if (!t.gotCombinedLengthSlRight18) { t.combinedLengthRight18 = combinedLengths(t.slRight18); t.gotCombinedLengthSlRight18 = true; }
    if ((t.combinedLengthSlLeft18 > combinedRect.size.height * METRICS_SIDELENGTH)
        && (t.combinedLengthRight18 > combinedRect.size.height * METRICS_SIDELENGTH)) {
        // Is there a middle component?
        SegmentList slMiddleElement;
        CGRect box = t.stCombined->getBoxForNHorizontalSegments(3, 0.48, 0.40, slMiddleElement);
        if ((box.size.width > 0) && (box.size.height > combinedRect.size.height * 0.16) && (slMiddleElement.size() == 3)) {
            // Calculate box for middle element
            CGRect middleElementRect (rectLeft(combinedRect) + slMiddleElement[1].startPos, rectBottom(combinedRect) + rectBottom(box), slMiddleElement[1].endPos - slMiddleElement[1].startPos + 1, box.size.height);
            SingleLetterTests *stMiddle = CreateSingleLetterTests(middleElementRect, results, false, 0, 0.03, true);
            if (stMiddle != NULL) {
                //  ------
                //0|      |
                //1|   11 |
                //2| 1111 |
                //3|  11  |
                //4|  11  |
                //5|      |
                //  ------
                // Require shape to be wider on top half
                SegmentList slTop50 = stMiddle->getHorizontalSegments(0.25, 0.50);
                SegmentList slBottom25 = stMiddle->getHorizontalSegments(0.875, 0.25);
                if ((slTop50.size() >= 1) && (slBottom25.size() >= 1)
                    && (totalWidth(slTop50) > totalWidth(slBottom25))
                    && (((slTop50[0].startPos < slBottom25[0].startPos) && (slTop50[slTop50.size()-1].endPos >= slBottom25[slBottom25.size()-1].endPos))
                        || ((slTop50[0].startPos <= slBottom25[0].startPos) && (slTop50[slTop50.size()-1].endPos > slBottom25[slBottom25.size()-1].endPos)))) {
                    // And require that shape does not go from top-left to bottom-right (as in N)
                    SegmentList slTop25 = stMiddle->getHorizontalSegments(0.125, 0.25);
                        // Two branches (left & right, as in M)
                    if ((slTop25.size() == 2)
                        // Or just one branch but aligned right
                        || ((slTop25.size() == 1) && (slTop25[0].endPos == middleElementRect.size.width - 1))) {
                        return 'M';
                    }
                }
                delete stMiddle;
            }
            // Couldn't decide on 'M' - try one more thing: are both sides above the middle element wider than both sides at the bottom AND wider than the very top?
            //  ------------
            //0|            |
            //1| 11       1 |
            //2| 11      11 |
            //3| 11      11 |
            //4| 11     111 |
            //5| 11     111 |
            //6| 111    111 |
            //7| 111    111 |
            //8| 1111   111 |
            //9| 1111    11 |
            //a| 11 1  1 11 |
            //b| 11 11   11 |
            //c| 11 111  11 |
            //d| 11  11  11 |
            //e| 11  11   1 |
            //f| 11   1  11 |
            //#| 11      11 |
            //#| 11      11 |
            //#| 11      11 |
            //#| 11      11 |
            //#| 11      11 |
            //#| 11       1 |
            //#| 11       1 |
            //#|            |
            //  ------------
            // wider area is 4 pixels tall (18%)
            //  -------------
            //0|             |
            //1|  11      11 |
            //2|  11      11 |
            //3| 111     111 |
            //4|  11     111 |
            //5|  11     111 |
            //6|  111    111 |
            //7|  111   1111 |
            //8|  111   1111 |
            //9|  11 1    11 |
            //a|  11 1 1  11 |
            //b|  1  11   11 |
            //c|  1   11  11 |
            //d|  11  11  11 |
            //e|  11  11  11 |
            //f| 111      11 |
            //#| 111      11 |
            //#| 111      11 |
            //#| 111      11 |
            //#| 111      11 |
            //#| 111      11 |
            //#| 11       11 |
            //#|  1          |
            //#|             |
            //  -------------
            int slRangeEnd = rectBottom(middleElementRect) - rectBottom(combinedRect) - 1;
            if (slRangeEnd > combinedRect.size.height * 0.25) {
                // Start a touch above, to ignore the complicated pattern right above the middle element
                SegmentList slAboveMiddleElement = t.stCombined->getHorizontalSegmentsPixels(slRangeEnd - combinedRect.size.height * 0.25, slRangeEnd - combinedRect.size.height * 0.10);
                if (!t.gotSlTop10) {t.slTop10 = t.stCombined->getHorizontalSegments(0.05, 0.10); t.gotSlTop10 = true;}
                if (!t.gotSlBottom25) {t.slBottom25 = t.stCombined->getHorizontalSegments(0.875, 0.25); t.gotSlBottom25 = true;}
                if ((t.slTop10.size() == 2) && (slAboveMiddleElement.size() >= 2) && (t.slBottom25.size() == 2)) {
                    float topLeftWidth = t.slTop10[0].endPos - t.slTop10[0].startPos + 1;
                    float topRightWidth = t.slTop10[1].endPos - t.slTop10[1].startPos + 1;
                    float middleLeftWidth = slAboveMiddleElement[0].endPos - slAboveMiddleElement[0].startPos + 1;
                    float middleRightWidth = slAboveMiddleElement[(int)slAboveMiddleElement.size()-1].endPos - slAboveMiddleElement[(int)slAboveMiddleElement.size()-1].startPos + 1;
                    float bottomLeftWidth = t.slBottom25[0].endPos - t.slBottom25[0].startPos + 1;
                    float bottomRightWidth = t.slBottom25[1].endPos - t.slBottom25[1].startPos + 1;
                    if ((topLeftWidth < middleLeftWidth) && (topRightWidth < middleRightWidth)
                        && (topLeftWidth < middleRightWidth) && (topRightWidth < middleLeftWidth)
                        // A thick bottom-left leg is less suspect, because we worry more about the bottom-right leg being thick (as in N), hence the <= rather than <
                        && (bottomLeftWidth <= middleLeftWidth) && (bottomRightWidth < middleRightWidth)
                        && (bottomLeftWidth <= middleRightWidth) && (bottomRightWidth < middleLeftWidth)) {
                        return 'M';
                    }
                }
            }
        }
    }
    return '\0';
} // testM

/* Parameters:
 - twoRects: means OCR returned two elements, e.g. we may be testing "IJ" for a possible 'N' (as opposed to testing H for a possible M). When twoRects is true we may take into account what character the OCR thought follows r
 Return value: \0 is no determination can be made, valid character otherwise
*/
wchar_t testForUppercase(OCRRectPtr r, CGRect &combinedRect, OCRResults *results, bool twoRects, wchar_t assumedChar, bool lax) {

    wchar_t newCh = '\0';

    testData t;
    t.twoRects = twoRects;

    // Start with generic analysis of the pattern helpful to all tests
    // neighbors4 = false, validation = 0, minSize = 0.01, acceptInverted = false
    t.stCombined = CreateSingleLetterTests(combinedRect, results, false, 0, 0.01, false);
    if (t.stCombined == NULL) {
        releaseTestData(t);
        return '\0';
    }
    
    // Trim combinedRect if necessary
    if (computeTrimmedRect(combinedRect, t.cplCombined)) {
        delete t.stCombined; t.stCombined = NULL;
        SingleLetterTests *newStCombined = CreateSingleLetterTests(combinedRect, results, false, 0, 0.01, false);
        if (newStCombined == NULL) {
            releaseTestData(t);
            return '\0';
        }
    }
    
    t.cplCombined = t.stCombined->getConnectedComponents(0.01);
    if (t.cplCombined.size() < 2) {
        releaseTestData(t);
        return '\0';
    }
    
    ConnectedComponentList cplLeft, cplRight;
    int numElements = 0;    // 2 if we have a left & right side, 1 otherwise
    if (twoRects) {
        t.leftRect = r->rect;
        t.rightRect = r->next->rect;
        numElements = 2;
    }
    else {
        bool success = parseRectIntoLeftRight(combinedRect, t.cplCombined, t.leftRect, t.rightRect, false);
        if (success && (t.leftRect.size.width > 0) && (t.rightRect.size.width > 0)) {
            numElements = 2;
        } else {
            numElements = 1;
        }
    }
    
    if (numElements == 2) {
        t.stLeft = CreateSingleLetterTests(t.leftRect, results, false, 0, 0.01, false);
        if (t.stLeft == NULL) {
            releaseTestData(t);
            return '\0';
        }
        cplLeft = t.stLeft->getConnectedComponents();
        t.stRight = CreateSingleLetterTests(t.rightRect, results, false, 0, 0.01, false);
        if (t.stRight == NULL) {
            releaseTestData(t);
            return '\0';
        }
        cplRight = t.stRight->getConnectedComponents();
    }
    
    // If we are looking at a single 'H', before testing 'W' ascertain it's not actually a 'H'
    if (!twoRects && (r->ch == 'H')) {
        newCh = testH(t, combinedRect);
    }
    if (newCh == '\0')
        newCh = testW(t, combinedRect, results);
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    newCh = testM(t, combinedRect, results);
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    //  ------------
    //0|            |
    //1| 11      11 |
    //2| 11      11 |
    //3| 111     1  |
    //4| 111    111 |
    //5| 111     11 |
    //6| 1111    11 |
    //7| 1111    11 |
    //8| 11111   11 |
    //9| 11 11   11 |
    //a| 11 11   11 |
    //b| 11 11   11 |
    //c| 11  1   11 |
    //d| 11      11 |
    //e| 11   1 111 |
    //f| 11   11111 |
    //#| 11    1111 |
    //#| 11    1111 |
    //#| 11    1111 |
    //#| 11    1111 |
    //#| 11     111 |
    //#| 11     111 |
    //#| 11     111 |
    //#|         1  |
    //#|            |
    //  ------------
    // Top thicker: 5-10 middle 9 out of 23 (39%), width = 6 (26%)
    // Bottom thicker: 13-16 middle 14.5 out of 23 (63%), width = 4 (17%)
    
    //  ------------
    //0|            |
    //1|  1      11 |
    //2|  11     11 |
    //3| 111     11 |
    //4|  111    11 |
    //5|  111    11 |
    //6| 1111    11 |
    //7| 11111   11 |
    //8| 11111   11 |
    //9| 111 1   11 |
    //a| 111 1   11 |
    //b|  11 1   11 |
    //c| 111     11 |
    //d| 111     11 |
    //e| 111   1 11 |
    //f| 111  11111 |
    //#| 111   1111 |
    //#| 111    111 |
    //#| 11     111 |
    //#| 111    111 |
    //#| 111    111 |
    //#| 111     11 |
    //#| 11     111 |
    //#|         11 |
    //#|            |
    //  ------------
    // Top thicker: 6-10 middle 8.5 out of 23 (37%), width = 5 (22%)
    // Bottom thicker: 14-15 middle 14.5 out of 23 (63%), width = 2 (8.7%)
    
    // 'N'
    if (twoRects
        && (t.leftRect.size.height > combinedRect.size.height * 0.85)
        && (t.rightRect.size.height > combinedRect.size.height * 0.85)) {
        /* Require:
            - left and right bars with possible gaps but combined length > 80% of height
            - large gap top & bottom
            - top-mid-left thicker than top-mid-right (and vice-versa at bottom)
         */
        if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; }
        if (!t.gotSlRight18) { t.slRight18 = t.stCombined->getVerticalSegments(0.91, 0.18); t.gotSlRight18 = true; }
        if (!t.gotCombinedLengthSlLeft18) { t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
        if (!t.gotCombinedLengthSlRight18) { t.combinedLengthRight18 = combinedLengths(t.slRight18); t.gotCombinedLengthSlRight18 = true; }
        if ((t.combinedLengthSlLeft18 > combinedRect.size.height * METRICS_SIDELENGTH)
            && (t.combinedLengthRight18 > combinedRect.size.height * METRICS_SIDELENGTH)) {
            if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15 = true; }
            SegmentList slBottom15 = t.stCombined->getHorizontalSegments(0.925, 0.15);
            if ((t.slTop15.size() == 2) && (slBottom15.size() == 2)){
                float gapTop15 = largestGap(t.slTop15);
                float gapBottom15 = largestGap(slBottom15);
                if ((gapTop15 > combinedRect.size.width * 0.30)
                    && (gapBottom15 > combinedRect.size.width * METRICS_GAP_LETTER_OPENATBOTTOM)) {
                    if (!t.gotSlH38w10) { t.slH38w10 = t.stCombined->getHorizontalSegments(0.38, 0.10); t.gotSlH38w10 = true; }
                    if (!t.gotSlH63w10) { t.slH63w10 = t.stCombined->getHorizontalSegments(0.63, 0.10); t.gotSlH63w10 = true; }
                    if ((t.slH38w10.size() >= 2) && (t.slH63w10.size() >= 2)) {
                        if (!t.gotGapSlH38w10) { t.gapSlH38w10 = largestGap(t.slH38w10, &t.gapSlH38w10Start, &t.gapSlH38w10End); t.gotGapSlH38w10 = true; }
                        if (!t.gotGapSlH63w10) { if (!t.gotSlH63w10) { t.slH63w10 = t.stCombined->getHorizontalSegments(0.63, 0.10); t.gotSlH63w10 = true; } t.gapSlH63w10 = largestGap(t.slH63w10, &t.gapSlH63w10Start, &t.gapSlH63w10End); t.gotGapSlH63w10 = true; }
                        if ((t.gapSlH38w10Start >= 0) && (t.gapSlH38w10End >= 0) && (t.gapSlH63w10Start >= 0) && (t.gapSlH63w10End >= 0)) {
                            float topLeftWidth = combinedLengths(t.slH38w10, 0, t.gapSlH38w10End);
                            float topRightWidth = combinedLengths(t.slH38w10, t.gapSlH38w10End, combinedRect.size.width);
                            float bottomLeftWidth = combinedLengths(t.slH63w10, 0, t.gapSlH63w10End);
                            float bottomRightWidth = combinedLengths(t.slH63w10, t.gapSlH63w10End, combinedRect.size.width);
                            if ((topLeftWidth > topRightWidth)
                                && (topLeftWidth > bottomLeftWidth)
                                && (bottomRightWidth > bottomLeftWidth)
                                && (bottomRightWidth > topRightWidth)) {
                                newCh = 'N';
                            } else {
                                //  ------------
                                //0|            |
                                //1| 11      11 |
                                //2| 111     11 |
                                //3| 111     1  |
                                //4| 111     11 |
                                //5| 111     11 |
                                //6| 1111    11 |
                                //7| 1111    11 |
                                //8| 11111   11 |
                                //9| 11 11   11 |
                                //a| 11 11   11 |
                                //b| 11  1   11 |
                                //c| 11      11 |
                                //d| 11      11 |
                                //e| 1       1  |
                                //f| 11     11  |
                                //#| 11     111 |
                                //#| 11    1111 |
                                //#| 11    1111 |
                                //#| 11    1111 |
                                //#| 11     111 |
                                //#| 11     111 |
                                //#|  1         |
                                //#|            |
                                //  ------------
                                // Top thicker: 5-11 middle 8 out of 22 (36%), width = 6 (27%)
                                // Bottom thicker: 16-19 middle 17.5 out of 23 (80%), width = 3 (13.6%)
                                if (!t.gotSlH80w10) { t.slH80w10 = t.stCombined->getHorizontalSegments(0.80, 0.10); t.gotSlH80w10 = true; }
                                if ((t.slH38w10.size() >= 2) && (t.slH80w10.size() >= 2)) {
                                    if (!t.gotGapSlH38w10) { t.gapSlH38w10 = largestGap(t.slH38w10, &t.gapSlH38w10Start, &t.gapSlH38w10End); t.gotGapSlH38w10 = true; }
                                    if (!t.gotGapSlH80w10) { if (!t.gotSlH80w10) { t.slH80w10 = t.stCombined->getHorizontalSegments(0.80, 0.10); t.gotSlH80w10 = true; } t.gapSlH80w10 = largestGap(t.slH80w10, &t.gapSlH80w10Start, &t.gapSlH80w10End); t.gotGapSlH80w10 = true; }
                                    if ((t.gapSlH38w10Start >= 0) && (t.gapSlH38w10End >= 0) && (t.gapSlH80w10Start >= 0) && (t.gapSlH80w10End >= 0)) {
                                        float topLeftWidth = combinedLengths(t.slH38w10, 0, t.gapSlH38w10End);
                                        float topRightWidth = combinedLengths(t.slH38w10, t.gapSlH38w10End, combinedRect.size.width);
                                        float bottomLeftWidth = combinedLengths(t.slH80w10, 0, t.gapSlH80w10End);
                                        float bottomRightWidth = combinedLengths(t.slH80w10, t.gapSlH80w10End, combinedRect.size.width);
                                        if ((topLeftWidth > topRightWidth)
                                            && (topLeftWidth > bottomLeftWidth)
                                            && (bottomRightWidth > bottomLeftWidth)
                                            && (bottomRightWidth > topRightWidth)) {
                                            newCh = 'N';
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             }
         }
    }
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    if ((newCh = testP(t, combinedRect)) != '\0') {
        releaseTestData(t);
        return newCh;
    }
    //  ---------------
    //0|               |
    //1| 111        1  |
    //2|  11       111 |
    //3| 111       111 |
    //4| 111       111 |
    //5|  11       111 |
    //6| 111       111 |
    //7|  11       111 |
    //8|  11       111 |
    //9| 111       111 |
    //a| 111       111 |
    //b|  11       111 |
    //c| 111       111 |
    //d| 111      1111 |
    //e|  111     1111 |
    //f|  111  1111111 |
    //#|   11111111111 |
    //#|    111    111 |
    //#|               |
    //  ---------------
    // 'U', 'D', 'O' with opening on top
    if (twoRects) {
        // Bottom is almost closed and wide
        if (!t.gotSlBottom25) { t.slBottom25 = t.stCombined->getHorizontalSegments(0.875, 0.25); t.gotSlBottom25 = true; }
        if (!t.gotWidthSlBottom25) { t.widthSlBottom25 = totalWidth(t.slBottom25); t.gotWidthSlBottom25 = true; }
        if (!t.gotCombinedLengthSlBottom25) { t.combinedLengthSlBottom25 = combinedLengths(t.slBottom25); t.gotCombinedLengthSlBottom25 = true; };
        if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); }
        if (!t.gotGapSlTop15) { t.gapSlTop15 = largestGap(t.slTop15); t.gotGapSlTop15 = true; }
        SegmentList slTop30 = t.stCombined->getHorizontalSegments(0.20, 0.10);
        SegmentList slTop60 = t.stCombined->getHorizontalSegments(0.60, 0.10);
        if ((t.widthSlBottom25 >= combinedRect.size.width * 0.80)
            && (t.combinedLengthSlBottom25 >= combinedRect.size.width * 0.75)
            && (t.slTop15.size() == 2) && (slTop30.size() == 2) && (slTop60.size() == 2)
            // Wide gap at 30% mark
            && (slTop30[1].startPos - slTop30[0].endPos > combinedRect.size.width * 0.25)
            // Wide gap at 60% mark
            && (slTop60[1].startPos - slTop60[0].endPos > combinedRect.size.width * 0.25)) {
            if ((t.gapSlTop15 >= combinedRect.size.width * 0.25)
            ) {
                newCh = 'U';
                if (combinedRect.size.height < results->globalStats.averageHeightDigits.average * 0.75)
                    newCh = 'u';
            } else {
                //  -------------
                //0|             |
                //1| 111  111    |
                //2| 11    111   |
                //3| 1      11   |
                //4| 11      11  |
                //5| 11      11  |
                //6| 11      111 |
                //7| 11      111 |
                //8| 11      111 |
                //9| 11       11 |
                //a| 11       11 |
                //b| 11      111 |
                //c| 11      111 |
                //d| 11       11 |
                //e| 111      11 |
                //f| 11       11 |
                //#| 11      111 |
                //#| 111     111 |
                //#| 111    1111 |
                //#| 111 11111   |
                //#| 111 111 1   |
                //#|  11         |
                //#|             |
                //  -------------
                if (!t.gotSlBottom15) { t.slBottom15 = t.stCombined->getHorizontalSegments(0.925, 0.15); t.gotSlBottom15 = true; }
                float leftMarginTop = leftMargin(t.slTop15);
                float rightMarginTop = rightMargin(t.slTop15, combinedRect);
                float leftMarginBottom = leftMargin(t.slBottom15);
                float rightMarginBottom = rightMargin(t.slBottom15, combinedRect);
                if (((leftMarginTop > combinedRect.size.width * 0.20) && (leftMarginTop >= rightMarginTop))
                    || ((leftMarginBottom > combinedRect.size.width * 0.20) && (leftMarginBottom >= rightMarginBottom)))
                    newCh = 'O';
                 else
                    newCh = 'D';
            }
        }

    }
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    //  --------------
    //0|              |
    //1|  1111 1      |
    //2|  1 1  1111   |
    //3|        111   |
    //4|  11     11   |
    //5|  11      1   |
    //6| 111      11  |
    //7| 111      11  |
    //8| 111      111 |
    //9| 111      11  |
    //a| 111      11  |
    //b| 111      11  |
    //c| 111      11  |
    //d| 111      11  |
    //e| 11       11  |
    //f| 11       11  |
    //#| 11       11  |
    //#| 11      111  |
    //#| 11      11   |
    //#| 11    1111   |
    //#| 111   1111   |
    //#| 111     1    |
    //#| 111          |
    //#|              |
    //  --------------
    // 'D'
    if (numElements == 2) {
        // Tall straight left side
        if (!t.gotCombinedLengthSlLeft18) { if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; } t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
        if (!t.gotCombinedLengthSlRight) { if (!t.gotSlRight) { t.slRight = t.stRight->getVerticalSegments(0.50, 1.00); t.gotSlRight = true; } t.combinedLengthSlRight = combinedLengths(t.slRight); t.gotCombinedLengthSlRight = true; }
        if ((t.combinedLengthSlLeft18 >= METRICS_SIDELENGTH * combinedRect.size.height)
            // Right side may be a tad shorter (vertical line may chop some of the height)
            && (t.combinedLengthSlRight >= 0.75 * combinedRect.size.height)) {
            // Make sure top/bottom don't have too wide a gap
            if (!t.gotGapSlTop15) { if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15 = true; } t.gapSlTop15 = largestGap(t.slTop15); t.gotGapSlTop15 = true; }
            if (!t.gotGapSlBottom15) { if (!t.gotSlBottom15) { t.slBottom15 = t.stCombined->getHorizontalSegments(0.925, 0.15); t.gotSlBottom15 = true; } t.gapSlBottom15 = largestGap(t.slBottom15); t.gotGapSlBottom15 = true; }
            float gapLimit = (((assumedChar == 'D') && lax)? METRICS_GAP_LETTER_CLOSED_LAX:METRICS_GAP_LETTER_CLOSED);
            if ((t.gapSlTop15 < gapLimit * combinedRect.size.width)
                && (t.gapSlBottom15 < gapLimit * combinedRect.size.width)) {
                if (isCurved(t.rightRect, t.stRight, SingleLetterTests::Left)) {
                    newCh = 'D';
                }
            }
        }
    }
    
    //  --------------
    //0|              |
    //1|  11          |
    //2|  111  111    |
    //3| 11111   11   |
    //4| 111     111  |
    //5| 111      11  |
    //6|  11      111 |
    //7| 111      111 |
    //8| 111      111 |
    //9| 111      111 |
    //a| 111      111 |
    //b| 111     111  |
    //c| 111  1 1111  |
    //d| 111   11     |
    //e| 111   11     |
    //f| 111   11     |
    //#| 11     111   |
    //#| 111    111   |
    //#| 111     111  |
    //#| 111     111  |
    //#| 111     111  |
    //#| 111      11  |
    //#| 111      11  |
    //#| 11       11  |
    //#|          1   |
    //#|              |
    //  --------------
    
    //  -------------
    //0|             |
    //1|  111        |
    //2| 1111   111  |
    //3| 111     11  |
    //4|  11      11 |
    //5|  11      11 |
    //6|  11      11 |
    //7| 111      11 |
    //8| 111      11 |
    //9| 111     111 |
    //a| 111    111  |
    //b| 111  1111   |
    //c| 111   1     |
    //d| 111   1     |
    //e| 11    11    |
    //f| 11     11   |
    //#| 111    111  |
    //#| 111    111  |
    //#| 111     11  |
    //#| 11      11  |
    //#| 111     11  |
    //#| 11      11  |
    //#|  1          |
    //#|             |
    //  -------------
    // Here top opening is 3 vs width 11 (27%)
    
    if ((numElements == 1) || (numElements == 2)) {
    
        if (!t.gotSlLeft18) { t.slLeft18 = t.stCombined->getVerticalSegments(0.09, 0.18); t.gotSlLeft18 = true; }
        if (!t.gotCombinedLengthSlLeft18) { t.combinedLengthSlLeft18 = combinedLengths(t.slLeft18); t.gotCombinedLengthSlLeft18 = true; }
        if (t.combinedLengthSlLeft18 > combinedRect.size.height * METRICS_SIDELENGTH) {
            // Check no or narrow opening on top
            if (!t.gotGapSlTop15) { if (!t.gotSlTop15) { t.slTop15 = t.stCombined->getHorizontalSegments(0.075, 0.15); t.gotSlTop15 = true; } t.gapSlTop15 = largestGap(t.slTop15); t.gotGapSlTop15 = true; }
            if (!t.gotGapBottom25) { if (!t.gotSlBottom25) { t.slBottom25 = t.stCombined->getHorizontalSegments(0.75, 0.10); t.gotSlBottom25 = true; } t.gapBottom25 = largestGap(t.slBottom25, &t.gapBottom25Start, &t.gapBottom25End); t.gotGapBottom25 = true; }
            if (!t.gotSlH25w10) { t.slH25w10 = t.stCombined->getHorizontalSegments(0.25, 0.10); t.gotSlH25w10 = true; }
            if ((t.gapSlTop15 < combinedRect.size.width * 0.18)
                // Or, accept larger gap on top as long as 25% mark is closer to right edge than top part
                || ((t.gapSlTop15 < combinedRect.size.width * 0.30)
                    && ((t.slH25w10.size() >= 2) && (t.slH25w10[t.slH25w10.size()-1].endPos > t.slTop15[t.slTop15.size()-1].endPos)))) {
                // Wide gap at bottom
                if (t.gapBottom25 > combinedRect.size.width * METRICS_GAP_LETTER_OPENATBOTTOM) {
                    // Finally, test for wide indent on right side
                    if (hasRightRIndent(combinedRect, t.stCombined)) {
                        newCh = 'R';
                    }
                } // bottom opening larger than METRICS_GAP_LETTER_OPENATBOTTOM
            } // small or no gap on top
        } // left side 18% tall enough
    }

    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    newCh = testH(t, combinedRect);
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }

    // Test for 'O'/'0'
    OpeningsTestResults resTop;
    bool success = t.stCombined->getOpenings(resTop, SingleLetterTests::Top,
                     0.20,      // Start of range to search (top/left)
                     0.90,      // End of range to search (bottom/right)
                     SingleLetterTests::Bound,   // Require start (top/left) bound
                     SingleLetterTests::Bound  // Require end (bottom/right) bound
                     );
    if (!(success && (resTop.maxDepth > combinedRect.size.height * 0.15))) {
        // No opening on top, could be a 'O' or '0'!
        ConnectedComponentList invertCpl = t.stCombined->getInverseConnectedComponents();
        if (invertCpl.size() == 2) {
            // Make sure it's a large hole, as in a 'O'
            if ((invertCpl[1].getHeight() > combinedRect.size.height * 0.55)
                && (invertCpl[1].getWidth() > combinedRect.size.width * 0.40)) {
                // Go with '0' when next to digits
                if (((r->previous != NULL) && isDigit(r->previous->ch))
                    || ((r->next != NULL) && (r->next->next != NULL) && isDigit(r->next->next->ch))) {
                    releaseTestData(t);
                    return '0';
                }
            }
        }
    }
    
    if (newCh != '\0') {
        releaseTestData(t);
        return newCh;
    }
    
    return '\0';
} // testForUppercase

} // namespace ocrparser
