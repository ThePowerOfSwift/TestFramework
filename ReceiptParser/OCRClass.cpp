//
//  OCRResults.cpp
//
//  Created by Patrick Questembert on 3/17/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//
// Represents a class that contains all data relevant to the scanning of a page of text
//

#define CODE_CPP 1

#include "common.hh"
#include "OCRPage.hh"
#include "OCRClass.hh"
#include "OCRUtils.hh"
#include "RegexUtils.hh"
#include <string.h>

#if ANDROID
#define atof(x) myAtoF(x)
#endif

namespace ocrparser {

OCRResults::OCRResults() {
    // Initialization code
    languageCode = LanguageENG; // We don't support non-US receipts at this time
    matchesAddressBook = MatchVector();
    
    binaryImageData = NULL;
    imageTests = false;
    meaningfulFrame = false;
    frameNumber = 0;
    lastFrame = false;
    gotSubtotal = gotTotal = subtotalMatchesSumOfPricesMinusDiscounts = false;
    numProducts = 0;
    numProductsWithoutProductNumbers = 0;
    numProductsWithQuantities = 0;
    
    letterAndDigitsOnBaseline = true;
    receiptImagesIndex = -1;
    lastConfirmedReceiptImageHighestProductLineNumber = -1;
    currentSavedReceiptImageHighestProductLineNumber = -1;
    currentLowestMatchingProductLineNumber = -1;
    currentHighestProductLineNumber = -1;
    numProductsWithPrice = 0;
    
    
    productPricesRange.size.width = typedResultsRange.size.width = realResultsRange.size.width = productsRange.size.width = productsWithPriceRange.size.width = 0;
    productPricesRange.size.height = typedResultsRange.size.height = realResultsRange.size.height = productsRange.size.height = productsWithPriceRange.size.height = 0;
    productPricesRange.origin.x = typedResultsRange.origin.x = realResultsRange.origin.x = productsRange.origin.x = productsWithPriceRange.origin.x = 20000;
    productPricesRange.origin.y = typedResultsRange.origin.y = realResultsRange.origin.y = productsRange.origin.y = productsWithPriceRange.origin.y = 20000;
    productWidth = productDigitWidth = 0;
    
    retailerLineComponents = vector<OCRElementType>();
}

bool OCRResults::closeMatchPriceToPartialPrice(float price, wstring partialPrice) {
    // Strip leading dollar sign
    if (partialPrice[0] == L'$') {
        partialPrice = partialPrice.substr(1, wstring::npos);
    }
    // Strip space(s)
    RegexUtilsStripLeading(partialPrice, L" ");
    
    int roundedPrice = round(price * 100);
    if (roundedPrice < 10)
        return false;   // We don't really expect prices lower than $0.10, best to ignore these (simplifies length tests below)
    
    char *roundedPriceCStr = (char *)malloc(50);
    sprintf(roundedPriceCStr, "%d", roundedPrice);
    wstring priceStr = toUTF32(roundedPriceCStr);
    free(roundedPriceCStr);

    // If less than $1.00 need to prepend "0", i.e. "99" -> "099"
    if (roundedPrice < 100)
        priceStr = L"0" + priceStr;
    // Insert decimal dot
    priceStr = priceStr.substr(0,priceStr.length()-2) + L"." + priceStr.substr(priceStr.length()-2, wstring::npos);
    
    // Missing 2nd decimal (e.g. X.Y)?
    if ((partialPrice.length() >= 3) && (partialPrice.substr(0,3) == priceStr.substr(0,3))) {
        return true;
    }
    
    // 3 chars after decimal point (one too many) and either first digit or last digit match?
    if (partialPrice.length() >= 5) {
        size_t rangeDot = partialPrice.find('.');
        if (rangeDot == partialPrice.length() - 4)
        {
            int firstDecimalPartialPrice = atoi(toUTF8(partialPrice.substr(rangeDot+1,1)).c_str());
            int firstDecimalPrice = (int)(price * 10 + 0.50) % 10;
            if (firstDecimalPartialPrice == firstDecimalPrice) {
                return true;
            }
            int secondDecimalPartialPrice = atoi(toUTF8(partialPrice.substr(rangeDot+2,1)).c_str());
            int secondDecimalPrice = (int)(price * 10 + 0.50) % 10;
            if (secondDecimalPartialPrice == secondDecimalPrice) {
                return true;
            }
        }
    }
    
    return false;
} // closeMatchPriceToPartialPrice

void OCRResults::checkForTotal() {

    bool gotTotal = false;
    // Check if we already have a subtotal (we actually have a variable for that but just in case it wasn't set)
    for (int i=(int)matchesAddressBook.size() - 1; (i>0); i--) {
        MatchPtr d = matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        
        if (tp == OCRElementTypeSubtotal) {
            // Get subtotal amount on same line. For now we assume it must be right after
            int subtotalLine = getIntFromMatch(d, matchKeyLine);
            if (i >= (int)this->matchesAddressBook.size() - 1)
                break;
            MatchPtr subTotalAmountItem = this->matchesAddressBook[i+1];
            if ((getIntFromMatch(subTotalAmountItem, matchKeyLine) != subtotalLine) || (getElementTypeFromMatch(subTotalAmountItem, matchKeyElementType) != OCRElementTypePrice))
                break;
            float subtotalAmountValue = atof(toUTF8(getStringFromMatch(subTotalAmountItem, matchKeyText)).c_str());
            // Now iterate through the next few lines, collecting possible tax amounts and checkout discounts (aligned with subtotal price), until we find a price that's the sum of subtotal + all these amounts - that's our total
            CGRect subtotalAmountRange = OCRResults::matchRange(subTotalAmountItem);
            float subtotalPlusTaxAndDiscounts = subtotalAmountValue;
            int lastLine = subtotalLine; // PQTODO check no blank lines
            int j = OCRResults::indexLastElementOnLineAtIndex(i+1, &this->matchesAddressBook) + 1;
            bool abort = false;
            while ((j<this->matchesAddressBook.size()) && !abort) {
                int newLine = getIntFromMatch(this->matchesAddressBook[j], matchKeyLine);
                if (newLine != lastLine + 1) {
                    abort = true; break;
                } else
                    lastLine = newLine;
                int endCurrentLineIndex = OCRResults::indexLastElementOnLineAtIndex(j, &this->matchesAddressBook);
                bool foundPriceOnCurrentLine = false;
                for (int k=endCurrentLineIndex; k>=j;k--) {
                    MatchPtr dd = this->matchesAddressBook[k];
                    if (getElementTypeFromMatch(dd, matchKeyElementType) == OCRElementTypePrice) {
                        CGRect currentRange = OCRResults::matchRange(dd);
                        if (OCRUtilsOverlappingX(currentRange, subtotalAmountRange)) {
                            // Extend range (to account for slant)? Not necessary as long as we just require some X overlap (as opposed to strict alignement)
                            float currentValue = atof(toUTF8(getStringFromMatch(dd, matchKeyText)).c_str());
                            // PQTODO check for discounts (see example on Google drive)
                            // PQTODO create tax property
                            // PQTODO track all prices we include to set their confidence to 0 to protect them from change
                            // PQTODO handle likewise when we missed the subtotal but tagged TOTAL
                            if (currentValue < subtotalAmountValue * MAX_TAX) {
                                subtotalPlusTaxAndDiscounts += currentValue; foundPriceOnCurrentLine = true;
                            } else {
                                // It's either the total we seek or we failed and need to abort
                                if (abs(currentValue - subtotalPlusTaxAndDiscounts) < 0.01) {
                                    // Got it!
                                    // Mark previous element as our total (if there IS a previous element on this line)
                                    if (k > j) {
                                        MatchPtr totalTextItem = this->matchesAddressBook[k-1];
                                        totalTextItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotal)).castDown<EmptyType>();
                                    }
                                    // Also add a tag to the price
                                    unsigned long currentFlags = getUnsignedLongFromMatch(dd, matchKeyStatus);
                                    currentFlags |= matchKeyStatusTotal;
                                    dd[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(currentFlags)).castDown<EmptyType>();
                                    gotTotal = true;
                                }
                                abort = true; // Whether we found the total or not
                            } // total?
                            break; // done with this line, we found the price we were looking for
                        } // overlaps with subtotal price
                    } // found a price
                    if (abort)
                        break;
                } // Check one line
                if (!foundPriceOnCurrentLine) {
                    abort = true; break;    // One line below subtotal has no price - abort
                }
                // Move to next line
                j = endCurrentLineIndex + 1;
            } // Check lines after subtotal
            break;
        } // found subtotal
    } // iterate over all matches
    
    if (gotTotal) return;

    bool abort = false;
    for (int i=(int)matchesAddressBook.size() - 1; (i>0) && (!abort); i--) {
        MatchPtr d = matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        
        if (tp != OCRElementTypeUnknown) {
            if ((tp == OCRElementTypeProductNumber) || (this->retailerParams.noProductNumbers && (tp == OCRElementTypeProductPrice))) {
                break; // Not expecting products past the total!
            }
            continue;
        }
        
        wstring itemText = getStringFromMatch(d, matchKeyText);
#if DEBUG
        if (itemText.substr(0,5) == L"TOT L") {
            DebugLog("");
        }
#endif
        if ((itemText.length() >= 5)
            && testForTotal(itemText, this)) {
            int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matchesAddressBook, NULL);
            if (priceIndex > i) {
                // Found it! Can we find a price just above (and no product number on that line)? If so, it's the tax amount, substract from total and label it a subtotal!
                if (this->retailerParams.hasSubtotal && this->retailerParams.totalIncludesTaxAbove) {
                    int lineNumberTotal = getIntFromMatch(d, matchKeyLine);
                    MatchPtr totalPriceItem = matchesAddressBook[priceIndex];
                    CGRect totalPriceRange = OCRResults::matchRange(totalPriceItem);
                    int endIndexTaxLine = OCRResults::indexFirstElementOnLineAtIndex(priceIndex, &matchesAddressBook) - 1;
                    // Check there IS a line above and its line number is one less than total line
                    if ((endIndexTaxLine > 0) && (getIntFromMatch(matchesAddressBook[endIndexTaxLine], matchKeyLine) == lineNumberTotal - 1)) {
                        // Make sure no product numbers on this line
                        if (OCRResults::elementWithTypeInlineAtIndex(endIndexTaxLine, OCRElementTypeProductNumber, &matchesAddressBook, &this->retailerParams) == -1) {
                            int startIndexTaxLine = OCRResults::indexFirstElementOnLineAtIndex(endIndexTaxLine, &matchesAddressBook);
                            // Now look for a price, starting with the end
                            for (int j=endIndexTaxLine; j>= startIndexTaxLine; j--) {
                                MatchPtr possibleTaxItem = matchesAddressBook[j];
                                OCRElementType tp = getElementTypeFromMatch(possibleTaxItem, matchKeyElementType);
                                if (tp == OCRElementTypePrice) {
                                    // Check X overlap with our total price!
                                    CGRect possibleTaxItemRange = OCRResults::matchRange(matchesAddressBook[priceIndex]);
                                    if (OCRUtilsOverlappingX(totalPriceRange, possibleTaxItemRange)) {
                                        wstring taxText = getStringFromMatch(possibleTaxItem, matchKeyText);
                                        float taxValue = atof(toUTF8(taxText).c_str());
                                        wstring totalText = getStringFromMatch(totalPriceItem, matchKeyText);
                                        float totalValue = atof(toUTF8(totalText).c_str());
                                        // Accept the tax value if less than 10% of total, or if it is the same (which means there were no taxable items hence no tax line, see https://drive.google.com/open?id=0B4jSQhcYsC9VelVnNjBvLTdOd3M
                                        int taxValue100 = (int)(taxValue * 100);
                                        int totalValue100 = (int)(totalValue * 100);
                                        bool foundTaxValue = false;
                                        if (taxValue100 == totalValue100)
                                            foundTaxValue = true;
                                        else if (totalValue > taxValue * 10) {
                                            totalValue -= taxValue;
                                            foundTaxValue = true;
                                        }
                                        if (foundTaxValue) {
                                            char *newTotalCSTR = (char *)malloc(100);
                                            sprintf(newTotalCSTR, "%.2f", totalValue);
                                            WarningLog("checkForTotal: found total (%s) and tax above (%s) => net total=%s", toUTF8(totalText).c_str(), toUTF8(taxText).c_str(), newTotalCSTR);
                                            totalText = wstring(toUTF32(newTotalCSTR));
                                            free(newTotalCSTR);
                                            totalPriceItem[matchKeyText] = SmartPtr<wstring> (new wstring(totalText)).castDown<EmptyType>();
                                            unsigned long flags = 0;
                                            if (totalPriceItem.isExists(matchKeyStatus))
                                                flags = getUnsignedLongFromMatch(totalPriceItem, matchKeyStatus);
                                            flags |= matchKeyStatusHasTaxAbove;
                                            totalPriceItem[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(flags)).castDown<EmptyType>();
                                            d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotal)).castDown<EmptyType>();
                                            this->gotTotal = true;
                                            abort = true; break;
                                        } // valid tax value above total price
                                    } // tax item X overlaps total price
                                } // found price on line above
                            } // iterate over items on line above looking for tax item
                            if (abort)
                                break;
                        } // no product on line above
                    } // endIndex valid
                } // Has tax above total
                // If we didn't find the tax above, still set item type for the sake of merging!
                d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotalWithoutTax)).castDown<EmptyType>();
//                    curResults.lastFrame = true;
//                    WarningLog("processBlinkOCRResults: found total (%s)", toUTF8(getStringFromMatch(curResults.matchesAddressBook[priceIndex], matchKeyText)).c_str());
//                    curResults.gotSubtotal = true;
//                    break;
            } // found price on same line
        } // matched total regex
    } // searching for total

}

/*
    Search for TAX 1.79 BAL 4.75
*/
void OCRResults::checkForTaxBal() {

    if (!this->retailerParams.hasTaxBalTotal)
        return;

    bool abort = false;
    for (int i=(int)matchesAddressBook.size() - 1; (i>0) && (!abort); i--) {
        int indexLineEnd = i;
        int indexLineStart = indexFirstElementOnLineAtIndex(indexLineEnd, &this->matchesAddressBook);
        if (indexLineEnd - indexLineStart + 1 < 4) {
            i = indexLineStart; continue;
        }
        // Now look for unknow + price + unknown + price
        int indexTaxItem = -1, indexTaxPrice = -1, indexBalItem = -1, indexBalPrice = -1;
        MatchPtr taxItem, taxPriceItem, balItem, balPriceItem;
        OCRElementType taxPriceType = OCRElementTypeUnknown, balPriceType = OCRElementTypeUnknown;
        for (int j=indexLineStart+1; j<=indexLineEnd; j++) {
            MatchPtr d = matchesAddressBook[j];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if ((tp == OCRElementTypePrice) || (tp == OCRElementTypeTaxPrice) || (tp == OCRElementTypeBalPrice)) {
                if ((indexTaxPrice < 0) && ((tp == OCRElementTypePrice) || (tp == OCRElementTypeTaxPrice))) {
                    indexTaxPrice = j;
                    taxPriceItem = d;
                    taxPriceType = tp;
                } else if ((tp == OCRElementTypePrice) || (tp == OCRElementTypeBalPrice)){
                    indexBalPrice = j;
                    balPriceItem = d;
                    balPriceType = tp;
                    break;
                }
            }
        }
        
        if ((indexTaxPrice < 0) || (indexBalPrice < 0)) {
            i = indexLineStart; continue;
        }
        
        indexTaxItem = indexTaxPrice - 1;
        taxItem = matchesAddressBook[indexTaxItem];
        OCRElementType tpTaxItem = getElementTypeFromMatch(taxItem, matchKeyElementType);
        if ((tpTaxItem != OCRElementTypeUnknown) && (tpTaxItem != OCRElementTypeTax)) {
            i = indexLineStart; continue;
        }
        if (tpTaxItem == OCRElementTypeUnknown) {
            if (!testForTax(getStringFromMatch(taxItem, matchKeyText), this)) {
                i = indexLineStart; continue;
            }
        }
        
        indexBalItem = indexBalPrice - 1;
        balItem = matchesAddressBook[indexBalItem];
        OCRElementType tpBalItem = getElementTypeFromMatch(balItem, matchKeyElementType);
        if ((tpBalItem != OCRElementTypeUnknown) && (tpBalItem != OCRElementTypeTax)) {
            i = indexLineStart; continue;
        }
        if (tpBalItem == OCRElementTypeUnknown) {
            if (!testForBal(getStringFromMatch(balItem, matchKeyText), this)) {
                i = indexLineStart; continue;
            }
        }

        // That's it, it's a match!
        if (tpTaxItem == OCRElementTypeUnknown) {
            // Set the type right
            taxItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTax)).castDown<EmptyType>();
        }
        
        if (taxPriceType != OCRElementTypeTaxPrice) {
            taxPriceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTaxPrice)).castDown<EmptyType>();
        }

        if (tpBalItem == OCRElementTypeUnknown) {
            // Set the type right
            balItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeBal)).castDown<EmptyType>();
        }
        
        if (balPriceType != OCRElementTypeBalPrice) {
            balPriceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeBalPrice)).castDown<EmptyType>();
        }
        
        break;
    } // iterate over all matches
}// checkForTaxBal


void OCRResults::checkForCoupons() {

    // Special pass: now that all basic types got recognized search for possible coupons associated with products (which makes them not eligible for price-matching
    
    if (!retailerParams.hasCoupons)
        return;
    
    for (int i=0; i<matchesAddressBook.size(); i++) {
        MatchPtr productMatch = matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(productMatch, matchKeyElementType);
        if (tp != OCRElementTypeProductNumber) {
            continue;
        }
        unsigned long productStatus = 0;
        if (productMatch.isExists(matchKeyStatus))
            productStatus = getUnsignedLongFromMatch(productMatch, matchKeyStatus);
        if (productStatus & matchKeyStatusHasCoupon) {
            // No need to search for this one, skip line
            i = indexLastElementOnLineAtIndex(i, &matchesAddressBook);
            continue;
        }
        int productLine = getIntFromMatch(productMatch, matchKeyLine);
        // Now look at next lines until line number is numLinesBetweenProductAndCoupon away, and there test for coupon pattern

        int indexPossibleCouponLine = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
        while (indexPossibleCouponLine > 0) {
            int lineNumberPossibleCouponLine = getIntFromMatch(matchesAddressBook[indexPossibleCouponLine], matchKeyLine);
            if (lineNumberPossibleCouponLine - productLine > retailerParams.numLinesBetweenProductAndCoupon) {
                // We went to far, stop
                break;
            }
            if (lineNumberPossibleCouponLine - productLine < retailerParams.numLinesBetweenProductAndCoupon) {
                // Skip this line
                indexPossibleCouponLine = indexOfElementPastLineOfMatchAtIndex(indexPossibleCouponLine, &matchesAddressBook);
                continue;
            }
            // If we get here, it means we are finally looking at the right line
            int indexEndPossibleCouponLine = indexLastElementOnLineAtIndex(indexPossibleCouponLine, &matchesAddressBook);
            for (int j=indexPossibleCouponLine; j<=indexEndPossibleCouponLine; j++) {
                MatchPtr possibleCouponMatch = matchesAddressBook[j];
                wstring possibleCouponText = getStringFromMatch(possibleCouponMatch, matchKeyText);
                if (testForCoupon(possibleCouponText, this)) {
                    // Found it!
                    LayoutLog("checkForCoupons: setting product [%s] as having a coupon", toUTF8(getStringFromMatch(productMatch, matchKeyText)).c_str());
                    productMatch[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(productStatus | matchKeyStatusHasCoupon)).castDown<EmptyType>();
                    break;
                }
            }
            // Whether or not we tagged the product as having a coupon, we are done with this product
            break;
        }
    } // for i, iterate over all matches
} // checkForCoupons

void OCRResults::checkForSubtotalTaggedAsProduct() {
    if ((!this->retailerParams.noProductNumbers) || (this->gotSubtotal))
        return;

    for (int i=0; i<this->matchesAddressBook.size(); i++) {
        MatchPtr d = this->matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if (tp == OCRElementTypeProductDescription) {
            int productPriceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypeProductPrice, &this->matchesAddressBook, &this->retailerParams);
            if (productPriceIndex >= 0) {
                bool justFoundSubtotal = false;
                if (!this->gotSubtotal) {
                    wstring descriptionText = getStringFromMatch(d, matchKeyText);
                    if (testForSubtotal(descriptionText, this)) {
                        this->gotSubtotal = justFoundSubtotal = true;
                    }
                }
                if (this->gotSubtotal) {
                    // Undo!
                    MatchPtr productPriceItem = this->matchesAddressBook[productPriceIndex];
                    if (justFoundSubtotal) {
                        LayoutLog("checkForSubtotalTaggedAsProduct: untagging [%s]-[%s] product/price, setting as subtotal instead", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), toUTF8(getStringFromMatch(productPriceItem, matchKeyText)).c_str());
                        d[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypeSubtotal)).castDown<EmptyType>();
                    } else {
                        LayoutLog("checkForSubtotalTaggedAsProduct: untagging [%s]-[%s] product/price", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), toUTF8(getStringFromMatch(productPriceItem, matchKeyText)).c_str());
                        d[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                    }
                    productPriceItem[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
                   this->gotSubtotal = true;
                }
            }
            // Whether or not we adjusted anything on this line, skip to the end of the line
            i = OCRResults::indexLastElementOnLineAtIndex(i, &this->matchesAddressBook);
            if (i >= 0)
                continue;
            else
                break;
        }
    }
}

/*
    Expect either "X Y" (where X is an integer and Y is any char, assumed to be the '@') or "X@"
    TODO: need to tolerate cases where the '@' was returned as multiple chars
*/
int OCRResults::extractQuantity(MatchPtr d, wstring &quantityTextOut, OCRResults * results) {
    wstring itemText = getStringFromMatch(d, matchKeyText);
    if (itemText.length() < 3) return -1;
    wstring quantityText;
    if (itemText[itemText.length()-2] == ' ')
        quantityText = itemText.substr(0,itemText.length()-2);
    else if (itemText[itemText.length()-1] == '@')
        quantityText = itemText.substr(0,itemText.length()-1);
    if (quantityText.length() > 0) {
        wstring quantityTextClean = convertDigitsLookalikesToDigits(quantityText);
        int quantityValue = atoi(toUTF8(quantityTextClean).c_str());
        if (quantityValue > 0) {
            quantityTextOut = quantityTextClean;
            return quantityValue;
        }
    }
    
    return -1;
}

void OCRResults::checkForQuantities() {

    // Special pass: now that all basic types got recognized search for possible quantity + price per item situation
    
    // e.g. Staples
    if (retailerParams.quantityAtDescriptionStart) {
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp != OCRElementTypeProductDescription) {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                continue;
            }
            wstring itemText = getStringFromMatch(d, matchKeyText);
            if (itemText.length() < 3) {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                continue;
            }
            // Isolate quantity number, expected to be between start and first space
            size_t loc = itemText.find(' ');
            // Assuming MAX_QUANTITY is less than 100, quantity can't have more than 2 digits. TODO: revisit that when we implement the story to allow high quantities when price per item and total match
            if ((loc == wstring::npos) || (loc > 2)) {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                continue;
            }
            wstring quantityString = itemText.substr(0,loc);
            // Don't accept digits lookalikes for now, to be on the conservative side and avoid bogus quantities, so no need to convert the string. PQTODO: consider revisiting that. Still, make an exception for 'I' and 'O' (convert to '1' and '0')
            findAndReplace(quantityString, L"O", L"0");
            findAndReplace(quantityString, L"I", L"1");
            findAndReplace(quantityString, L"S", L"5");
            int quantityValue = atoi(toUTF8(quantityString).c_str());
            // TODO: revisit the below when we implement the story to allow high quantities when price per item and total match
            if ((quantityValue <= 0) || (quantityValue > MAX_QUANTITY)) {
                // Give it one more chance, for CVS some lines start with "F " and then "1 XYZ ..."
                if (loc == 1) {
                    // Isolate quantity number, expected to be between start and first space
                    wstring newText = itemText.substr(loc+1,itemText.length());
                    size_t loc2 = newText.find(' ');
                    // Assuming MAX_QUANTITY is less than 100, quantity can't have more than 2 digits. TODO: revisit that when we implement the story to allow high quantities when price per item and total match
                    if ((loc2 == wstring::npos) || (loc2 > 2)) {
                        i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                        continue;
                    }
                    quantityString = newText.substr(0,loc2);
                    // Don't accept digits lookalikes for now, to be on the conservative side and avoid bogus quantities, so no need to convert the string. PQTODO: consider revisiting that. Still, make an exception for 'I' and 'O' (convert to '1' and '0')
                    findAndReplace(quantityString, L"O", L"0");
                    findAndReplace(quantityString, L"I", L"1");
                    findAndReplace(quantityString, L"S", L"5");
                    quantityValue = atoi(toUTF8(quantityString).c_str());
                    loc += (1+loc2);
                }
                if ((quantityValue <= 0) || (quantityValue > MAX_QUANTITY)) {
                    i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                    continue;
                }
            }
//#if DEBUG
//            if (quantityValue == 4) {
//                DebugLog("");
//            }
//#endif
            // Got quantity!
            // Truncate description
            d[matchKeyText] = SmartPtr<wstring>(new wstring(itemText.substr(loc+1,itemText.length()))).castDown<EmptyType>();
            // Even if quantity is 1, remember that we found one
            unsigned long matchStatus = 0;
            if (d.isExists(matchKeyStatus)) {
                matchStatus = getUnsignedLongFromMatch(d, matchKeyStatus);
            }
            matchStatus |= matchKeyStatusHasQuantity;
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(matchStatus)).castDown<EmptyType>();
            // Create an address book item for it - only if quantity is not 1!
            if (quantityValue != 1) {
                MatchPtr newItem = OCRResults::copyMatch(d);
                newItem[matchKeyText] = SmartPtr<wstring>(new wstring(quantityString)).castDown<EmptyType>();
                newItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();
                matchesAddressBook.insert(matchesAddressBook.begin()+i, newItem);
                
                // Now look for the price per item!
                if (retailerParams.pricePerItemAfterProduct) {
                    int productIndex = OCRResults::productIndexForDescriptionAtIndex(i, matchesAddressBook, &retailerParams);
                    if ((productIndex >= 0) && (productIndex < (int)matchesAddressBook.size()-1)) {
                        // Price per item is the next item
                        int pricePerItemIndex = productIndex + 1;
                        MatchPtr pricePerItemMatch = matchesAddressBook[pricePerItemIndex];
                        if (getIntFromMatch(pricePerItemMatch, matchKeyElementType) == OCRElementTypeUnknown) {
                            wstring pricePerItemText = getStringFromMatch(pricePerItemMatch, matchKeyText);
                            wstring pricePerItemActualText = pricePerItemText;
                            if (retailerParams.extraCharsAfterPricePerItem > 0) {
                                if (pricePerItemText.length() >= retailerParams.extraCharsAfterPricePerItem + 4) {
                                    pricePerItemActualText = pricePerItemText.substr(0,pricePerItemText.length() - retailerParams.extraCharsAfterPricePerItem);
                                } else {
                                    // Give up on trying to find a price per item
                                    i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                                    continue;
                                }
                            } // need to truncate chars after price
                            // We have potential price per item text, need to validate it
                            // PQTODO911 do we need to format the price for digits lookalike + strip $ sign?
                            // PQTODO911 add OCRElementTypePricePerItem to list of types for merging
                            if (testForPrice(pricePerItemActualText, this) == OCRElementTypePrice) {
                                // Update item text
                                LayoutLog("processBlinkOCRResults: setting [%s] as price per item", toUTF8(pricePerItemActualText).c_str());
                                pricePerItemMatch[matchKeyText] = SmartPtr<wstring>(new wstring(pricePerItemActualText)).castDown<EmptyType>();
                                pricePerItemMatch[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePricePerItem)).castDown<EmptyType>();
                            }
                        } // potential price per item has unknown type
                    }
                }
            }
            i = OCRResults::indexLastElementOnLineAtIndex(i, &matchesAddressBook);
            continue;
        }
    } // quantityAtDescriptionStart is set
    else if (retailerParams.hasQuantities && !retailerParams.quantityPPItemTotal) {
    
        // This is the Target case - prices situated around the middle of the width, under a product line, indicating a quantity, where quantity x price = product price above it, e.g.
        //   210110031  OSCAR MAYER     FN      $4.00
        //              4 @ $1.00 ea
        // But also cases where there is a line between product and quantity line, e.g.:
        // 210110031  OSCAR MAYER     FN      $22.00
        //           Regular price $13.99
        //           2 @ $11.00 ea
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            // Is there a product on this line?
            int productLine = getIntFromMatch(d, matchKeyLine);
            int productIndex = -1;
            if (this->retailerParams.noProductNumbers)
                productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductDescription, &matchesAddressBook, NULL);
            else
                productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
            if (productIndex < 0) {
                // Skip line and continue
                int nextIndex = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
                if (nextIndex < 0) break; // No next line, we are done with this pass
                i = nextIndex - 1;
                continue;
            }
            // Found a product! Test one or two lines below for possible quantity lines
#if DEBUG
            wstring tmpItemText = getStringFromMatch(matchesAddressBook[productIndex], matchKeyText);
            if (tmpItemText == L"003070245") {
                DebugLog("");
            }
#endif
            bool abort = false; // Set below if need to end the entire pass
            int indexPossibleQuantityLine = i;
            for (int nonProductLineBelow = 1; nonProductLineBelow <= retailerParams.maxLinesBetweenProductAndQuantity; nonProductLineBelow++) {
                int itemOnNextLine = indexOfElementPastLineOfMatchAtIndex(indexPossibleQuantityLine, &matchesAddressBook);
                if (itemOnNextLine < 0) {
                    abort = true;   // To end outside loop iterating over all matches
                    break;
                }
                int newLineNumber = getIntFromMatch(matchesAddressBook[itemOnNextLine], matchKeyLine);
                if (newLineNumber != productLine+nonProductLineBelow) {
                    // Not the expected line number, means there are blank lines. Could be a case like Home Depot where the line below the product is treated as "meaningless" (no typed data) but there IS a quantity just below. Test for this case.
                    if ((newLineNumber - productLine > nonProductLineBelow) && (newLineNumber - productLine <= retailerParams.maxLinesBetweenProductAndQuantity)) {
                        // Give the next line(s) a chance
                        nonProductLineBelow = newLineNumber - productLine;
                    } else {
                        // Abort (but give current line a chance to contain a product with possible quantity below it
                        break;
                    }
                }
                // We have a product, and there is a line just below, test it for a quantity line
                // First make sure it's not a product line
                int productIndex2 = -1;
                if (this->retailerParams.noProductNumbers)
                    elementWithTypeInlineAtIndex(itemOnNextLine, OCRElementTypeProductDescription, &matchesAddressBook, NULL);
                else
                    elementWithTypeInlineAtIndex(itemOnNextLine, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
                if (productIndex2 >= 0) {
                    // This line can't be a quantity line, it's a product line. Abort looking for quantity for current product
                    break;
                }
                
                int quantityPriceIndex = elementWithTypeInlineAtIndex(itemOnNextLine, OCRElementTypePrice, &matchesAddressBook, NULL);
                
                if (quantityPriceIndex < 0) {
                    // No price on this line, try next line if that was the first line
                    indexPossibleQuantityLine = itemOnNextLine;
                    continue; // Loop will start by going to the line past the line at itemOnNextLine
                }
                
                // Verify that the price ends to the left of where the product price starts
                MatchPtr quantityPriceMatch = matchesAddressBook[quantityPriceIndex];
//#if DEBUG
//                wstring tmpPriceText = getStringFromMatch(quantityPriceMatch, matchKeyText);
//                if (tmpPriceText == L"23.73") {
//                    DebugLog("");
//                }
//#endif
                CGRect rectQuantityPrice = matchRange(quantityPriceMatch);
                CGRect rectProductPrice;
                // TODO I think there are cases where total price is not on the same line as the product
                int productPriceIndex = elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matchesAddressBook, NULL);
                if (productPriceIndex >= 0) {
                    rectProductPrice = matchRange(matchesAddressBook[productPriceIndex]);
                        // If can't get rects, don't test positions and assume legit
                    if ((rectProductPrice.size.width > 0) && (rectQuantityPrice.size.width > 0)
                    //PQTODO avoid cases where the product price above overlaps slightly with suspected quantity below (i.e. $ sign is above the end of quantity price). Require say 50% overlap.
                        && (rectLeft(rectProductPrice) <= rectRight(rectQuantityPrice))) {
                        // Abort!
                        DebugLog("processBlinkOCRResults: rejecting quantity & price [%s]-[%s] below [%s] from possible quantity based on rects", toUTF8(getStringFromMatch(matchesAddressBook[quantityPriceIndex-1], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[quantityPriceIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
                        indexPossibleQuantityLine = itemOnNextLine;
                        continue; // Loop will start by going to the line past the line at itemOnNextLine
                    }
                } // found product price
                // Note: if we didn't find a product price, assume OCR missed it and trust the quantity line below it. PQTODO: need to do some sanity testing on the quantity price (since we couldn't compare position of product price vs quantity price)
                // Now find the quantity itself, to the left of quantity price. MERCHANTSPECIFIC
                int indexOfQuantity = quantityPriceIndex - 1;
                if (getIntFromMatch(matchesAddressBook[indexOfQuantity], matchKeyLine) != productLine + nonProductLineBelow) {
                    // No quantity, skip this line
                    indexPossibleQuantityLine = itemOnNextLine;
                    continue; // Loop will start by going to the line past the line at itemOnNextLine
                }
                // Now get quantity in this item: just collect all digits at the start of the text until a space. PQTODO: accept digits lookalikes here.
                MatchPtr quantityMatch = matchesAddressBook[indexOfQuantity];
                wstring quantityText = getStringFromMatch(quantityMatch, matchKeyText);
                size_t posSpace = quantityText.find(L" ");
                // Why insist on finding a space? What about "2@"?! Fixing here:
                if (posSpace == wstring::npos)
                    posSpace = quantityText.find(L"@");
                if (posSpace == wstring::npos) {
                    // No quantity, skip this line
                    DebugLog("processBlinkOCRResults: rejecting [%s] below [%s] from possible quantity, failed to find quantity", toUTF8(quantityText).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
                    indexPossibleQuantityLine = itemOnNextLine;
                    continue; // Loop will start by going to the line past the line at itemOnNextLine
                }
                wstring quantityNumber = quantityText.substr(0, posSpace);
                // Accept only spaces and digits in quantities. For example this test weeds out "15%" which happens to still succeed with atoi
                bool reject = false;
                for (int kk=0; kk<quantityNumber.length(); kk++) {
                    wchar_t ch = quantityNumber[kk];
                    if ((ch != ' ') && !isDigit(ch)) {
                        reject = true;
                        break;
                    }
                }
                
                // Check that there aren't too many chars after the alleged price per item, for example a case at Target with "4 Bd $31.51 Ca T T $114 . 99"
                // Important: for retailers with multiple price per items (e.g. Walgreens) it's important to set maxCharsOnLineAfterPricePerItem to 0 so that the line doesn't get rejected
                if (!reject && (retailerParams.maxCharsOnLineAfterPricePerItem >= 0) && (quantityMatch.isExists(matchKeyFullLineText))) {
                    wstring fullLineText = getStringFromMatch(quantityMatch, matchKeyFullLineText);
                    // Estimate how many of these chars are used up by quantity and price per item
                    int nCharsQuantity = (int)quantityText.length() + 1;
                    int nCharsPricePerItem;
                    if (quantityPriceMatch.isExists(matchKeyOrigText)) {
                        wstring quantityPriceOrigText = getStringFromMatch(quantityPriceMatch, matchKeyOrigText);
                        nCharsPricePerItem = (int)quantityPriceOrigText.length();
                    } else {
                        nCharsPricePerItem = (int)getStringFromMatch(quantityPriceMatch, matchKeyText).length() + 1; // Add 1 for the space following it
                        if (retailerParams.pricesHaveDollar)
                            nCharsPricePerItem++;
                    }
                    if ((int)fullLineText.length() - nCharsPricePerItem - nCharsQuantity > retailerParams.maxCharsOnLineAfterPricePerItem) {
                        DebugLog("processBlinkOCRResults: rejecting [%s] in [%s] below [%s] from possible quantity, too many extra chars after price per item (%d)", toUTF8(quantityText).c_str(), toUTF8(fullLineText).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str(), (int)fullLineText.length() - nCharsPricePerItem - nCharsQuantity);
                        indexPossibleQuantityLine = itemOnNextLine;
                        continue; // Loop will start by going to the line past the line at itemOnNextLine
                    }
                }
                
                if (reject) {
                    DebugLog("checkForQuantities: rejected quantity [%s] within [%s] @ price [%s] below [%s] because has non-digit(s)", toUTF8(quantityNumber).c_str(), toUTF8(quantityText).c_str(), toUTF8(getStringFromMatch(quantityPriceMatch, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
                } else {
                    // RETAILERSPECIFIC - works only on Target
                    // Also reject if more than at most two adjacent non-space characters. Should be like "4 @ " (accept 2 in case OCR split the '@' in two)
                    int numChars = 0;
                    for (int kk=(int)posSpace; kk<quantityText.length(); kk++) {
                        wchar_t ch = quantityText[kk];
                        if ((ch != ' ') && (ch != '@')) {
                            numChars++;
                            if (numChars > 2) {
                                // Too many, bail out
                                reject = true;
                                break;
                            } else if (numChars == 2) {
                                // Accept (for now) only if the other non-space was just preceding, otherwise reject
                                if (quantityText[kk-1] == ' ') {
                                    reject = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (reject) {
                        DebugLog("checkForQuantities: rejected quantity [%s] within [%s] @ price [%s] below [%s] because has too many non-space chars after quantity", toUTF8(quantityNumber).c_str(), toUTF8(quantityText).c_str(), toUTF8(getStringFromMatch(quantityPriceMatch, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
                    }
                } // check no more than 2 non-space chars
                
               
                if (reject) {
                    indexPossibleQuantityLine = itemOnNextLine;
                    continue; // Loop will start by going to the line past the line at itemOnNextLine
                } else {
                    int quantityValue = atoi(toUTF8(quantityText).c_str());
                    wstring quantityPriceText = getStringFromMatch(quantityPriceMatch, matchKeyText);
                    float quantityPriceValue = trunc(atof(toUTF8(quantityPriceText).c_str()) * 100) / 100;
                    if ((quantityValue > 1) && (quantityPriceValue > 0.0)) {
//#if DEBUG
//                        if (quantityValue == 4) {
//                            DebugLog("");
//                        }
//#endif
                        DebugLog("checkForQuantities: found quantity [%d] @ price [%.2f] below [%s]", quantityValue, quantityPriceValue, toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
//#if DEBUG
//                    wstring txt = getStringFromMatch(curResults.matchesAddressBook[productIndex], matchKeyText);
//                    if (txt == L"009031891") {
//                        DebugLog("");
//                    }
//#endif
                        // Check if there is ANOTHER price per item on this line
                        if (this->retailerParams.hasAdditionalPricePerItemWithSlash) {
                            // No sense checking other price per item values if we don't have a product price to be used for chosing. TODO need to store both values anyhow in case we get the product price from a previous frame or later using more frames
                            if (productPriceIndex > 0) {
                                MatchPtr productPriceItem = matchesAddressBook[productPriceIndex];
                                wstring productPriceText = getStringFromMatch(productPriceItem, matchKeyText);
                                float productPriceValue = atof(toUTF8(productPriceText).c_str());
                                int indexEndQuantityLine = OCRResults::indexLastElementOnLineAtIndex(indexOfQuantity, &this->matchesAddressBook);
                                // Review all prices past the quantity item. This means it will work even if for some reason the first price per item we picked is the 2nd of the pair
                                for (int kk=indexOfQuantity+1; kk<=indexEndQuantityLine; kk++) {
                                    if (kk == quantityPriceIndex) continue;
                                    MatchPtr otherPriceItem = matchesAddressBook[kk];
                                    if (getElementTypeFromMatch(otherPriceItem, matchKeyElementType) != OCRElementTypePrice)
                                        continue;
                                    wstring pricePerItemText = getStringFromMatch(otherPriceItem, matchKeyText);
                                    wstring actualPricePerItemText;
                                    float actualPricePerItemValue;
                                    if (checkForSlashPricePerItem(pricePerItemText, actualPricePerItemText, actualPricePerItemValue)) {
                                        if (abs(quantityValue * actualPricePerItemValue - productPriceValue) < 0.001) {
                                            // Found a better value!
                                            otherPriceItem[matchKeyText] = SmartPtr<wstring>(new wstring(actualPricePerItemText)).castDown<EmptyType>();
                                            otherPriceItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                                            productPriceItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                                            quantityMatch[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                                            // Remove other price
                                            matchesAddressBook.erase(matchesAddressBook.begin() + quantityPriceIndex);
                                            if (quantityPriceIndex < kk)
                                                quantityPriceIndex = kk - 1; // Removing caused KK to shift
                                            else
                                                quantityPriceIndex = kk;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        quantityMatch[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();
                        quantityMatch[matchKeyText] = SmartPtr<wstring>(new wstring(quantityNumber)).castDown<EmptyType>();
                        // Done handling quantity, skip to next line and continue looking for products
                        int nextIndex = indexOfElementPastLineOfMatchAtIndex(quantityPriceIndex, &matchesAddressBook);
                        if (nextIndex < 0) {
                            // No next line, we are done with this pass
                            abort = true;
                            break;
                        } else {
                            i = nextIndex - 1; // Loop will increment i
                            continue;
                        }
                    } else {
                        DebugLog("checkForQuantities: rejected quantity [%s] @ price [%s] below [%s]", toUTF8(quantityNumber).c_str(), toUTF8(getStringFromMatch(quantityPriceMatch, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[productIndex], matchKeyText)).c_str());
                        indexPossibleQuantityLine = itemOnNextLine;
                        continue; // Loop will start by going to the line past the line at itemOnNextLine
                    }
                }
            } // Testing 1 and 2 lines below
            
            if (abort)
                break;
            
            // We shouldn't really get here but if we do, move to next line
            // Whether or not we found a quantity, move to next line
            int nextIndex = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
            if (nextIndex < 0)
                break; // No next line, we are done with this pass
            i = nextIndex - 1;
        } // look for quantity line below product line
    } // Retailer has quantities (Target model)
    else if (retailerParams.hasQuantities && retailerParams.quantityPPItemTotal && !retailerParams.pricePerItemTaggedAsPrice) {
    
        // Test for the situation where line below product is like this Home Depot situation:
        // 	039257934004 KNOB <A>
        // 2@0.98						1.96
        // This will be returned as "200.98  1.96" because our regex accepts '@' as a substitute for '0'
        // Need to undo the price attribute of the leftmost price and make a quantity + price-per-item thing
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            // Is there a product on this line?
            int productLine = getIntFromMatch(d, matchKeyLine);
            int productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
            if (productIndex < 0) {
                // Skip line and continue
                int nextIndex = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
                if (nextIndex < 0) break; // No next line, we are done with this pass
                i = nextIndex - 1;
                continue;
            }
            // In this model product line has no price - make sure it's the case
            int priceIndex = elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matchesAddressBook, NULL);
            if (priceIndex >= 0) {
                // Skip current line
                i = indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                continue; // loop will increment past the end of current line
            }
            // Found a product! Test one or two lines below for possible quantity lines
            bool abort = false; // Set below if need to end the entire pass
            int indexPossibleQuantityLine = indexLastElementOnLineAtIndex(i, &matchesAddressBook) + 1;
            int nextI = indexPossibleQuantityLine; // This is the index where the quantity search loop below suggests continuing (e.g. line just after product, 2 lines below or 3 lines below)
            for (int nonProductLineBelow = 1; (indexPossibleQuantityLine < matchesAddressBook.size()) && (nonProductLineBelow <= retailerParams.maxLinesBetweenProductAndQuantity); nonProductLineBelow++) {
            
                int newLineNumber = getIntFromMatch(matchesAddressBook[indexPossibleQuantityLine], matchKeyLine);
                if (newLineNumber != productLine+nonProductLineBelow) {
                    // Not the expected line number, means there are blank lines. Could be a case like Home Depot where the line below the product is treated as "meaningless" (no typed data) but there IS a quantity just below. Test for this case.
                    if ((newLineNumber - productLine > nonProductLineBelow) && (newLineNumber - productLine <= retailerParams.maxLinesBetweenProductAndQuantity)) {
                        // Give the next line(s) a chance
                        nonProductLineBelow = newLineNumber - productLine;
                    } else {
                        nextI = indexPossibleQuantityLine;
                        break;
                    }
                }
            
//                if (getIntFromMatch(matchesAddressBook[indexPossibleQuantityLine], matchKeyLine) != productLine+nonProductLineBelow) {
//                    // Not the expected line number, means there are blank lines - abort
//                    nextI = indexPossibleQuantityLine;
//                    break;
//                }
                // We have a product, and there is a line just below, test it for a quantity line
                // First make sure it's not a product line
                int productIndex2 = elementWithTypeInlineAtIndex(indexPossibleQuantityLine, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
                if (productIndex2 >= 0) {
                    // This line can't be a quantity line, it's a product line. Abort looking for quantity for current product
                    nextI = indexPossibleQuantityLine;
                    break;
                }
                
                int quantityAndPriceItemIndex = indexPossibleQuantityLine;
                int priceItemIndex = indexPossibleQuantityLine+1;
                if ((priceItemIndex < (int)matchesAddressBook.size()-1) && (getElementTypeFromMatch(matchesAddressBook[quantityAndPriceItemIndex], matchKeyElementType) == OCRElementTypePrice) && (getElementTypeFromMatch(matchesAddressBook[priceItemIndex], matchKeyElementType) == OCRElementTypePrice)) {
                    MatchPtr quantityAndPriceItem = matchesAddressBook[quantityAndPriceItemIndex];
                    MatchPtr priceItem = matchesAddressBook[priceItemIndex];
                    // Verify same line
                    // TODO need to verify in products column or price column
                    if ((getIntFromMatch(quantityAndPriceItem, matchKeyLine) == productLine+nonProductLineBelow) && (getIntFromMatch(priceItem, matchKeyLine) == productLine+nonProductLineBelow)) {
                        wstring quantityAndPriceText = getStringFromMatch(quantityAndPriceItem, matchKeyText);
                        wstring priceText = getStringFromMatch(priceItem, matchKeyText);
                        if (quantityAndPriceText.length() >= 6) {
                            size_t locDotquantityAndPrice = quantityAndPriceText.find(L'.');
                            if (locDotquantityAndPrice == quantityAndPriceText.length() - 3) {
                                float totalPrice = atof(toUTF8(priceText).c_str());
                                long totalPriceValueInt100 = (long)(round(totalPrice * 100));
                                for (int j=1; j<=locDotquantityAndPrice-2; j++) {
                                    wstring quantityText = quantityAndPriceText.substr(0,j);
                                    int quantityValue = atoi(toUTF8(quantityText).c_str());
                                    wstring pricePerItemText = quantityAndPriceText.substr(j+1, wstring::npos);
                                    float pricePerItemValue = atof(toUTF8(pricePerItemText).c_str());
                                    long pricePerItemInt100 = (long)(round(pricePerItemValue * 100));
                                    if (quantityValue * pricePerItemInt100 == totalPriceValueInt100) {
                                        DebugLog("checkForQuantities: adjusting [%s] to [%d] x [%.2f] = [%.2f] quantity & price", toUTF8(quantityAndPriceText).c_str(), quantityValue, pricePerItemValue, totalPrice);
                                        // Fix the match formerly holding both quantity & price per item
                                        quantityAndPriceItem[matchKeyText] = SmartPtr<wstring>(new wstring(pricePerItemText)).castDown<EmptyType>();
                                        // Create a new match to hold the quantity
                                        MatchPtr newItem = OCRResults::copyMatch(quantityAndPriceItem);
                                        newItem[matchKeyText] = SmartPtr<wstring>(new wstring(quantityText)).castDown<EmptyType>();
                                        newItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();
                                        matchesAddressBook.insert(matchesAddressBook.begin() + quantityAndPriceItemIndex, newItem);
                                        // Skip past the quanity line for the sake of the outside i loop
                                        nextI = indexLastElementOnLineAtIndex(priceItemIndex, &matchesAddressBook) + 1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                // Try next line
                indexPossibleQuantityLine = indexLastElementOnLineAtIndex(indexPossibleQuantityLine, &matchesAddressBook) + 1;
            } // Testing 1 and 2 lines below
            
            if (abort)
                break;
            
            // Skip current line
            i = nextI - 1;
        } // look for quantity line below product line
    } // retailer has quantities (Home Depot model)
    else if (retailerParams.hasQuantities && retailerParams.quantityPPItemTotal && retailerParams.pricePerItemTaggedAsPrice) {
    
        // Kmart is an example:
        // 	0500003622     SOFT SCRUB
        //             3 @ 1/1.20    A  3.60
        // This will yield two price matches "171.20" and "3.60"
        // Need to undo the price attribute of the leftmost price and make a quantity + price-per-item thing
        // WALMART (see https://drive.google.com/open?id=0B4jSQhcYsC9VTDJXM1RjVEoteUE)
        // 2 AT   1 FOR     2.50  5.00 O
        
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            // Is there a product on this line?
            int productLine = getIntFromMatch(d, matchKeyLine);
            int productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
            if (productIndex < 0) {
                // Skip line and continue
                int nextIndex = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
                if (nextIndex < 0) break; // No next line, we are done with this pass
                i = nextIndex - 1;
                continue;
            }
            // In this model product line has no price - make sure it's the case
            int priceIndex = elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matchesAddressBook, NULL);
            if (priceIndex >= 0) {
                // Skip current line
                i = indexLastElementOnLineAtIndex(i, &matchesAddressBook);
                continue; // loop will increment past the end of current line
            }
            // Found a product! Test one or two lines below for possible quantity lines
            bool abort = false; // Set below if need to end the entire pass
            int indexPossibleQuantityLine = indexLastElementOnLineAtIndex(i, &matchesAddressBook) + 1;
            int nextI = indexPossibleQuantityLine; // This is the index where the quantity search loop below suggests continuing (e.g. line just after product, 2 lines below or 3 lines below)
            // Need to find at least 2 items on the line (3 if we require to find a total price too)
            for (int nonProductLineBelow = 1; (indexPossibleQuantityLine < matchesAddressBook.size() - 2) && (nonProductLineBelow <= retailerParams.maxLinesBetweenProductAndQuantity); nonProductLineBelow++) {
            
                int newLineNumber = getIntFromMatch(matchesAddressBook[indexPossibleQuantityLine], matchKeyLine);
                if (newLineNumber != productLine+nonProductLineBelow) {
                    // Not the expected line number, means there are blank lines. Could be a case like Home Depot where the line below the product is treated as "meaningless" (no typed data) but there IS a quantity just below. Test for this case.
                    if ((newLineNumber - productLine > nonProductLineBelow) && (newLineNumber - productLine <= retailerParams.maxLinesBetweenProductAndQuantity)) {
                        // Give the next line(s) a chance
                        nonProductLineBelow = newLineNumber - productLine;
                    } else {
                        nextI = indexPossibleQuantityLine;
                        break;
                    }
                }
            
                // We have a product, and there is a line just below, test it for a quantity line
                // First make sure it's not a product line
                int productIndex2 = elementWithTypeInlineAtIndex(indexPossibleQuantityLine, OCRElementTypeProductNumber, &matchesAddressBook, NULL);
                if (productIndex2 >= 0) {
                    // This line can't be a quantity line, it's a product line. Abort looking for quantity for current product
                    nextI = indexPossibleQuantityLine;
                    break;
                }
                
                // Need to find unknown item (the quantity, e.g. "3 @") + price (price per item) + price (total price)
                int quantityItemIndex = -1, pricePerItemIndex = -1, totalPriceIndex = -1;
                OCRElementType tpQuantityItem;
                int indexEndPossibleQuantityLine = indexLastElementOnLineAtIndex(indexPossibleQuantityLine, &matchesAddressBook);
                if (indexEndPossibleQuantityLine < indexPossibleQuantityLine + 2) {
                    // Not enough elements to parse into quantity + price per item + total
                    indexPossibleQuantityLine = indexEndPossibleQuantityLine + 1;
                    continue;
                }
                // First element on the line is the quantity item, possibly 2nd etc if we merged multiple unknown items. Note: true for both Kmart ("3 @ 1/1.20    A  3.60") and Walmart ("2 AT   1 FOR     2.50  5.00")
                for (int k=indexPossibleQuantityLine + 1; k<=indexEndPossibleQuantityLine; k++) {
                    if (getElementTypeFromMatch(matchesAddressBook[k], matchKeyElementType) == OCRElementTypePrice) {
                        if (pricePerItemIndex == -1) {
                            pricePerItemIndex = k;
                        } else {
                            totalPriceIndex = k;
                            break;
                        }
                    }
                }
                // For Walmart the item just before the pricePerItem has something like "2 AT 1 FOR"
                if (retailerParams.code == RETAILER_WALMART_CODE) {
                    /* In order to mesh with the code following, need to:
                     - leave in the quantityItem only "2 AT"
                     - fix the price per item to account for the number before "FOR"
                     */
                    //PQ11
                    if ((pricePerItemIndex > 0) && (totalPriceIndex > 0)) {
                        int quantityAndMoreItemIndex = pricePerItemIndex - 1;
                        OCRElementType tpQuantityAndMoreItem = getElementTypeFromMatch(matchesAddressBook[quantityAndMoreItemIndex], matchKeyElementType);
                        if (tpQuantityAndMoreItem == OCRElementTypeUnknown) {
                            MatchPtr quantityAndMoreItem = matchesAddressBook[quantityAndMoreItemIndex];
                            wstring quantityAndMoreText = getStringFromMatch(quantityAndMoreItem, matchKeyText);
                            vector<wstring> comps;
                            split(quantityAndMoreText, ' ', comps);
                            if (comps.size() == 4) {
                                wstring quantityText = comps[0];
                                wstring numQuantityPricePerItemText = comps[2];
                                int quantityValue = atoi(toUTF8(quantityText).c_str());
                                int numQuantityPricePerItemValue = atoi(toUTF8(numQuantityPricePerItemText).c_str());
                                if ((quantityValue > 0) && (numQuantityPricePerItemValue > 0)
                                    // expected "AT", allow 3 chars
                                    && (comps[1].length() <= 3)
                                    // expected "FOR", allow 4 chars
                                    && (comps[3].length() <= 4)) {
                                    // That's it, we got everything!
                                    // Fix quantity item
                                    quantityAndMoreItem[matchKeyText] = SmartPtr<wstring>(new wstring(quantityText)).castDown<EmptyType>();
                                    quantityAndMoreItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();
                                    // Adjust price per item
                                    MatchPtr pricePerItemItem = matchesAddressBook[pricePerItemIndex];
                                    wstring pricePerItemText = getStringFromMatch(pricePerItemItem, matchKeyText);
                                    float pricePerItemValue = atof(toUTF8(pricePerItemText).c_str());
                                    if (pricePerItemValue > 0) {
                                        float actualPricePerItemValue = (pricePerItemValue / numQuantityPricePerItemValue);
                                        char *priceCStr = (char *)malloc(100);
                                        sprintf(priceCStr, "%.2f", actualPricePerItemValue);
                                        wstring priceStr = toUTF32(priceCStr);
                                        free(priceCStr);
                                        pricePerItemItem[matchKeyText] = SmartPtr<wstring>(new wstring(priceStr)).castDown<EmptyType>();
                                        DebugLog("checkForQuantities: fixing price per item [%s] -> [%s]", toUTF8(pricePerItemText).c_str(), toUTF8(priceStr).c_str());
                                        // Skip past the quantity line for the sake of the outside i loop
                                        nextI = indexLastElementOnLineAtIndex(quantityItemIndex, &matchesAddressBook) + 1;
                                        break;
                                    } // pricePerItemValue valid
                                } // comps of Walmart weird quantity combo legit
                            } // found expected 4 comps of Walmart weird quantity combo
                        } // item left of pricePerItem is unknown
                    } // found price per item and total price
                    // If we get here it means we failed to find the expected structure for quantities at Walmart, try next line
                    indexPossibleQuantityLine = indexEndPossibleQuantityLine + 1;
                    continue;
                } else {
                    // TODO for now we require a total price to be found, need to relax that while testing the position of the price per item to be close enough to the quantity
                    if ((pricePerItemIndex > 0) && (totalPriceIndex > 0)) {
                        quantityItemIndex = pricePerItemIndex - 1;
                        tpQuantityItem = getElementTypeFromMatch(matchesAddressBook[quantityItemIndex], matchKeyElementType);
                    }
                    
                    if ((quantityItemIndex < 0) || (pricePerItemIndex < 0)) {
                        indexPossibleQuantityLine = indexEndPossibleQuantityLine + 1;
                        continue;
                    }
                } // not Walmart (e.g. Kmart)
                
                // Now find and format the quantity and price per item!
                wstring quantityText;
                MatchPtr quantityItem = matchesAddressBook[quantityItemIndex];
                int quantityValue = OCRResults::extractQuantity(quantityItem, quantityText, this);
                if (quantityValue > 0) {
                    // That's it, we found it
                    
                    MatchPtr pricePerItemItem = matchesAddressBook[pricePerItemIndex];
                    wstring pricePerItemText = getStringFromMatch(pricePerItemItem, matchKeyText);
                    if (this->retailerParams.code == RETAILER_KMART_CODE) {
                        // Kmart price per item are of the form "1/1.50" or "2/3.00" or "1/.60" and the "/" would have been interpreted as a '7'.
                        wstring actualPricePerItemText;
                        float actualPricePerItemValue;
                        if (checkForSlashPricePerItem(pricePerItemText, actualPricePerItemText, actualPricePerItemValue)) {
                            pricePerItemItem[matchKeyText] = SmartPtr<wstring>(new wstring(actualPricePerItemText)).castDown<EmptyType>();
                            DebugLog("checkForQuantities: fixing price per item [%s] -> [%s]", toUTF8(pricePerItemText).c_str(), toUTF8(actualPricePerItemText).c_str());
                        } else {
                            DebugLog("checkForQuantities: failed to fix price per item [%s]", toUTF8(pricePerItemText).c_str());
                            indexPossibleQuantityLine = indexEndPossibleQuantityLine + 1;
                            continue;
                        }
                        
                        /*
                        bool validPrice = false;
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
                                        pricePerItemItem[matchKeyText] = SmartPtr<wstring>(new wstring(priceStr)).castDown<EmptyType>();
                                        DebugLog("checkForQuantities: fixing price per item [%s] -> [%s]", toUTF8(pricePerItemText).c_str(), toUTF8(priceStr).c_str());
                                        validPrice = true;
                                    }
                                }
                            }
                        }
                        if (!validPrice) {
                            DebugLog("checkForQuantities: failed to fix price per item [%s]", toUTF8(pricePerItemText).c_str());
                            indexPossibleQuantityLine = indexEndPossibleQuantityLine + 1;
                            continue;
                        } */
                    } // special Kmart fix to price per item
                    
                    quantityItem[matchKeyText] = SmartPtr<wstring>(new wstring(quantityText)).castDown<EmptyType>();
                    quantityItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();

                    // Skip past the quantity line for the sake of the outside i loop
                    nextI = indexLastElementOnLineAtIndex(quantityItemIndex, &matchesAddressBook) + 1;
                    break;
                }

                // Try next line
                indexPossibleQuantityLine = indexLastElementOnLineAtIndex(indexPossibleQuantityLine, &matchesAddressBook) + 1;
            } // Testing 1 or 2 lines below
            
            if (abort)
                break;
            
            // Skip current line
            i = nextI - 1;
        } // look for quantity line below product line
    } // like Kmart
} // checkForQuantities


void OCRResults::computePageAverages(SmartPtr<OCRPage> &currentPage, OCRStats &stats, CGRect validRange) {

    vector<float> heights;                  // Digits & uppercase letters height (assumed to be the same)
    vector<float> digitsWidths;
    vector<float> uppercaseWidths;
    
    for (int i=0; i<currentPage->rects.size(); i++) {
        OCRRectPtr r = currentPage->rects[i];
//#if DEBUG
//    if (i == 140) {
//        DebugLog("");
//    }
//#endif
        if (!overlappingRectsXAxisAnd(r->rect, validRange) || !overlappingRectsYAxisAnd(r->rect, validRange))
            continue;
        wchar_t currentChar = r->ch;
        if (isDigit(currentChar)) {
            insertToFloatVectorInOrder(heights, r->rect.size.height);
            insertToFloatVectorInOrder(digitsWidths, r->rect.size.width);
        } else if (isUpper(currentChar)) {
            insertToFloatVectorInOrder(heights, r->rect.size.height);
            insertToFloatVectorInOrder(uppercaseWidths, r->rect.size.width);
        }
    }
    
    // Eliminate top/bottom 20% of averages
    if (heights.size() >= 5) {
        int percent20 = (int)floor((float)(heights.size()/5));
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            heights.erase(heights.begin());
            heights.erase(heights.end()-1);
        }
    }
    
    if (digitsWidths.size() >= 5) {
        int percent20 = (int)floor((float)(digitsWidths.size()/5));
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            digitsWidths.erase(digitsWidths.begin());
            digitsWidths.erase(digitsWidths.end()-1);
        }
    }
    
    if (uppercaseWidths.size() >= 5) {
        int percent20 = (int)floor((float)(uppercaseWidths.size()/5));
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            uppercaseWidths.erase(uppercaseWidths.begin());
            uppercaseWidths.erase(uppercaseWidths.end()-1);
        }
    }
    
    // Calculate one average from all elements
    float sumHeights = 0;
    int numHeights = 0;
    for (int i=0; i<heights.size(); i++) {
        sumHeights += heights[i];
        numHeights ++;
    }
    
    float overallHeightAverage = ((numHeights > 0)? sumHeights / numHeights : 0);
    // Note: assumes digits and uppercase letters have same height
    stats.averageHeightDigits.average = overallHeightAverage;
    stats.averageHeightDigits.count = numHeights;
    stats.averageHeightDigits.sum = sumHeights;
    stats.averageHeightUppercase.average = overallHeightAverage;
    stats.averageHeightUppercase.count = numHeights;
    stats.averageHeightUppercase.sum = sumHeights;
    stats.averageHeight.average = overallHeightAverage;
    stats.averageHeight.count = numHeights;
    stats.averageHeight.sum = sumHeights;
    
    float sumWidths = 0;
    int numWidths = 0;
    for (int i=0; i<digitsWidths.size(); i++) {
        sumWidths += digitsWidths[i];
        numWidths ++;
    }
    
    float overallWidthAverage = ((numWidths > 0)? sumWidths / numWidths : 0);
    stats.averageWidthDigits.average = overallWidthAverage;
    stats.averageWidthDigits.count = numWidths;
    stats.averageWidthDigits.sum = sumWidths;

    sumWidths = 0;
    numWidths = 0;
    for (int i=0; i<uppercaseWidths.size(); i++) {
        sumWidths += uppercaseWidths[i];
        numWidths ++;
    }
    
    overallWidthAverage = ((numWidths > 0)? sumWidths / numWidths : 0);
    stats.averageWidthUppercase.average = overallWidthAverage;
    stats.averageWidthUppercase.count = numWidths;
    stats.averageWidthUppercase.sum = sumWidths;
}

void OCRResults::buildLines() {

    // Need some stats about letter height
    CGRect validRange;
    // Define center 50% of image (where we expect the actual receipt)
    validRange.origin.x = this->imageRange.origin.x + this->imageRange.size.width * 0.25;
    validRange.origin.y = this->imageRange.origin.y + this->imageRange.size.height * 0.25;
    validRange.size.width = this->imageRange.size.width * 0.50;
    validRange.size.height = this->imageRange.size.height * 0.50;
    computePageAverages(this->page, this->globalStats, validRange);
    
    // If too few letters went into stats, re-calculate on entire image
    if (globalStats.averageHeight.count < 10)
        computePageAverages(this->page, this->globalStats, this->imageRange);
    
    // TODO need to remove outliers otherwise they will totally screw lines (e.g. very large noise letter will capture all kinds of letters in its "line")
    if (this->globalStats.averageHeight.count >= 10) {
        for (int i=0; i<page->rects.size(); i++) {
            OCRRectPtr r = page->rects[i];
            if ((r->ch != '\n') && (r->rect.size.height >= 1.5 * this->globalStats.averageHeight.average)) {
                LayoutLog("buildLines: removing outlier [%c] at index [%d], height=%.0f versus average [%.0f]", r->ch, i, r->rect.size.height, this->globalStats.averageHeight.average);
                page->removeRectAtIndex(i);
                i--; continue;
            }
        }
    }
    
    float referenceWidthForSpaces = 0;
    if ((globalStats.averageWidthUppercase.count > 10) || (globalStats.averageWidthDigits.count > 10)) {
        referenceWidthForSpaces = max (globalStats.averageWidthUppercase.average, globalStats.averageWidthDigits.average);
    }

    // Allocate a page to store the rects organized in lines
    SmartPtr<OCRPage> newPage = SmartPtr<OCRPage>(new OCRPage(0));
    
    while (this->page->rects.size() > 0) {
        // Pick the character most to the left - it must be the start of a line (then we can collect the characters on that line by looking to the right)
        float minX = 10000;
        int indexMinX = -1;
        for (int i=0; i<page->rects.size(); i++) {
            OCRRectPtr r = page->rects[i];
            if (rectLeft(r->rect) < minX) {
                minX = rectLeft(r->rect);
                indexMinX = i;
            }
        }
        // We picked the start of the next line, add it to the new page
        OCRRectPtr startLineExistingRectR = page->rects[indexMinX];
        SmartPtr<OCRRect> startNewLineR = SmartPtr<OCRRect>(new OCRRect);
        startNewLineR->ch = startLineExistingRectR->ch;
        startNewLineR->rect = startLineExistingRectR->rect;
        startNewLineR->confidence = startLineExistingRectR->confidence;
        newPage->addRect(startNewLineR);
        // Remove from existing rects
        page->removeRectAtIndex(indexMinX);
        
        while (true) {
            // Now add all rects on the same line
            // We need to maintain the notion of the reference latest rect to use to test overlap. This is because when we meet say a dot, we don't want to then test additional boxes based on the overlap with the box of the dot, we'd rather use the latest normal letter we found.
            float bestXDistance = 10000;
            int bestNextItemIndex = -1;
            for (int i=0; i<page->rects.size(); i++) {
                OCRRectPtr r = page->rects[i];
                // Possibly next on that line if greater left X and has Y overlap
                if (rectLeft(r->rect) < rectLeft(startNewLineR->rect))
                    continue;
                float currentYOverlap = overlapPercentRectsYAxisAnd(r->rect, startNewLineR->rect);
                if (currentYOverlap <= 0)
                    continue;
                float currentXDistance = rectLeft(r->rect) - rectRight(startNewLineR->rect);
                if (currentXDistance < bestXDistance) {
                    bestXDistance = currentXDistance;
                    bestNextItemIndex = i;
                }
            }
            if (bestNextItemIndex >= 0) {
                // Add it!
                OCRRectPtr nextItem = page->rects[bestNextItemIndex];
                
                // If next item is clearly one space beyond, create a space here
                if (referenceWidthForSpaces > 0) {
                    float spacing = rectSpaceBetweenRects(startNewLineR->rect, nextItem->rect);
                    if (spacing > referenceWidthForSpaces) {
                        // Create and add a space
                        SmartPtr<OCRRect> nextItemNewPage = SmartPtr<OCRRect>(new OCRRect);
                        nextItemNewPage->ch = L' ';
                        nextItemNewPage->rect = startNewLineR->rect;
                        nextItemNewPage->rect.origin.x = startNewLineR->rect.origin.x + 1;
                        nextItemNewPage->rect.size.width = spacing - 1;
                        nextItemNewPage->rect.size.height = 1;
                        nextItemNewPage->confidence = startNewLineR->confidence;
                        newPage->addRect(nextItemNewPage);
                    }
                }
                
                SmartPtr<OCRRect> nextItemNewPage = SmartPtr<OCRRect>(new OCRRect);
                nextItemNewPage->ch = nextItem->ch;
                nextItemNewPage->rect = nextItem->rect;
                nextItemNewPage->confidence = nextItem->confidence;
                newPage->addRect(nextItemNewPage);
                startNewLineR = nextItemNewPage;
                // Remove from existing rects
                page->removeRectAtIndex(bestNextItemIndex);
            } else {
                // End of line
                SmartPtr<OCRRect> nextItemNewPage = SmartPtr<OCRRect>(new OCRRect);
                nextItemNewPage->ch = 0xa;
                nextItemNewPage->rect = startNewLineR->rect;
                nextItemNewPage->rect.origin.x = startNewLineR->rect.origin.x + 1;
                nextItemNewPage->rect.size.width = 1;
                nextItemNewPage->confidence = startNewLineR->confidence;
                newPage->addRect(nextItemNewPage);
                break;
            }
        } // infinite loop building current line
    } // while there are rects left in existing page
    this->page = newPage;
}

void OCRResults::adjustAllLinesToHighStartIfNeeded(MatchVector &matches) {
    if (matches.size() == 0)
        return;
    // First determine if we need to do anything at all
    int highestLineNumber = 0;
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        int currentLine = getIntFromMatch(d, matchKeyLine);
        if (currentLine > highestLineNumber) {
            highestLineNumber = currentLine;
            if (currentLine >= 1000) {
                break;
            }
        }
    }
    if (highestLineNumber >= 1000)
        return;
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        int newLine = 1000 + getIntFromMatch(d, matchKeyLine);
        d[matchKeyLine] = SmartPtr<int>(new int(newLine)).castDown<EmptyType>();
    }
} // adjustAllLinesToHighStartIfNeeded

// Sets the priceLineNumberPtr output param only if the description was found on a line different than the product line!
int OCRResults::descriptionIndexForProductAtIndex(int productIndex, MatchVector &matches, int *descriptionLineNumberPtr, retailer_t *retailerParams) {
    MatchPtr productItem = matches[productIndex];
    int descriptionIndex = -1;
    if (retailerParams->descriptionAboveProduct) {
        int indexStartProductLine = OCRResults::indexFirstElementOnLineAtIndex(productIndex, &matches);
        if (indexStartProductLine > 0) {
            descriptionIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartProductLine-1, OCRElementTypeProductDescription, &matches, retailerParams);
            if ((descriptionIndex >= 0) && (descriptionLineNumberPtr != NULL)) {
                MatchPtr descriptionItem = matches[descriptionIndex];
                *descriptionLineNumberPtr = getIntFromMatch(descriptionItem, matchKeyLine);
            }
        }
    } else if (retailerParams->descriptionBelowProduct) {
        int indexStartProductLine = OCRResults::indexLastElementOnLineAtIndex(productIndex, &matches);
        if ((indexStartProductLine > 0) && (indexStartProductLine < ((int)matches.size()-1))) {
            descriptionIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartProductLine+1, OCRElementTypeProductDescription, &matches, retailerParams);
            if ((descriptionIndex >= 0) && (descriptionLineNumberPtr != NULL)) {
                MatchPtr descriptionItem = matches[descriptionIndex];
                *descriptionLineNumberPtr = getIntFromMatch(descriptionItem, matchKeyLine);
            }
        }
    } else {
        descriptionIndex = OCRResults::elementWithTypeInlineAtIndex(productIndex, OCRElementTypeProductDescription, &matches, retailerParams);
        if ((descriptionIndex >= 0) && (descriptionLineNumberPtr != NULL)) {
            *descriptionLineNumberPtr = -1;
        }
    }
    
    return (descriptionIndex);
}

// Should only be called for retailers without product numbers
int OCRResults::descriptionIndexForProductPriceAtIndex(int productPriceIndex, MatchVector &matches, retailer_t *retailerParams) {
    if ((productPriceIndex < 1) || (productPriceIndex >= (int)matches.size())) return -1;
    MatchPtr productItem = matches[productPriceIndex];
    // Ideally the description item should be just before the product price, but allow for the possible presence of a spurious price element due to a merging bug, so iterate back on that same line
    int indexStartProductPriceLine = OCRResults::indexFirstElementOnLineAtIndex(productPriceIndex, &matches);
    for (int i=productPriceIndex-1; i>=indexStartProductPriceLine; i--) {
        if (getElementTypeFromMatch(matches[i], matchKeyElementType) == OCRElementTypeProductDescription) {
#if DEBUG
            if (i < productPriceIndex - 1) {
                LayoutLog("descriptionIndexForProductPriceAtIndex: WARNING selected [%s] as description for [%s], skipping [%s]", toUTF8(getStringFromMatch(matches[i], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[productPriceIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[i+1], matchKeyText)).c_str());
                DebugLog("");
            }
#endif
            return i;
        }
    }
    
    return -1;
}

int OCRResults::productIndexForDescriptionAtIndex(int descriptionIndex, MatchVector &matches, retailer_t *retailerParams) {
    MatchPtr descriptionItem = matches[descriptionIndex];
    int productIndex = -1;
    // e.g. Staples
    if (retailerParams->descriptionAboveProduct) {
        int indexEndDescriptionLine = OCRResults::indexLastElementOnLineAtIndex(descriptionIndex, &matches);
        if (indexEndDescriptionLine < (int)matches.size()-1) {
            int indexStartProductLine = indexEndDescriptionLine + 1;
            productIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartProductLine, OCRElementTypeProductNumber, &matches, retailerParams);
        }
    } else if (retailerParams->descriptionBelowProduct) {
        int indexStartDescriptionLine = OCRResults::indexFirstElementOnLineAtIndex(descriptionIndex, &matches);
        if (indexStartDescriptionLine > 0) {
            int indexEndProductLine = indexStartDescriptionLine - 1;
            productIndex = OCRResults::elementWithTypeInlineAtIndex(indexEndProductLine, OCRElementTypeProductNumber, &matches, retailerParams);
        }
    } else {
        // Just look for it on the same line
        productIndex = OCRResults::elementWithTypeInlineAtIndex(descriptionIndex, OCRElementTypeProductNumber, &matches, retailerParams);
    }
    
    return (productIndex);
}

int OCRResults::productIndexForQuantityAtIndex(int quantityIndex, MatchVector &matches, retailer_t *retailerParams) {
    if (!retailerParams->hasQuantities)
        return -1;
    int productIndex = -1;
    MatchPtr quantityItem = matches[quantityIndex];
    int quantityLine = getIntFromMatch(quantityItem, matchKeyLine);
    int possibleProductLineIndex = OCRResults::indexFirstElementOnLineAtIndex(quantityIndex, &matches) - 1;
    for (int i=1; (i<=retailerParams->maxLinesBetweenProductAndQuantity) && (possibleProductLineIndex >= 0); i++) {
        MatchPtr lastItemOnPossibleProductLine = matches[possibleProductLineIndex];
        int lineNumberPossibleProduct = getIntFromMatch(lastItemOnPossibleProductLine, matchKeyLine);
        if (lineNumberPossibleProduct > quantityLine + i) {
            // Uh oh. Maybe this is a case of a blank line above quantity?
            if (lineNumberPossibleProduct - quantityLine <= retailerParams->maxLinesBetweenProductAndQuantity) {
                // Stay on this line but increment i to the right value
                i = retailerParams->maxLinesBetweenProductAndQuantity - 1; continue;
            } else
                break;
        }
        int tmpProducIndex = OCRResults::elementWithTypeInlineAtIndex(possibleProductLineIndex, OCRElementTypeProductNumber, &matches, retailerParams);
        if (tmpProducIndex >= 0) {
            if (retailerParams->quantityPPItemTotal) {
                // Check product does NOT have a total price (expected on the quantity line)
                int indexTotalPrice = OCRResults::priceIndexForProductAtIndex(tmpProducIndex, matches, NULL, retailerParams);
                if (indexTotalPrice < 0)
                    return tmpProducIndex;
            } else
                return tmpProducIndex;
        }
        possibleProductLineIndex = OCRResults::indexFirstElementOnLineAtIndex(possibleProductLineIndex, &matches) - 1;
    }
    
    return productIndex;
}

// Sets the priceLineNumberPtr output param only if the price was found on a line different than the product line!
int OCRResults::priceOrPartialPriceIndexForProductAtIndex(int productIndex, OCRElementType tp, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams) {
    MatchPtr productItem = matches[productIndex];
//#if DEBUG
//    wstring tmpItemText = getStringFromMatch(productItem, matchKeyText);
//    if (tmpItemText == L"044600011950") {
//        DebugLog("");
//    }
//#endif
    int priceIndex = -1;
    if (retailerParams->priceAboveProduct) {
        int indexStartProductLine = OCRResults::indexFirstElementOnLineAtIndex(productIndex, &matches);
        if (indexStartProductLine > 0) {
            priceIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartProductLine-1, tp, &matches, retailerParams);
            if ((priceIndex >= 0) && (priceLineNumberPtr != NULL)) {
                MatchPtr priceItem = matches[priceIndex];
                *priceLineNumberPtr = getIntFromMatch(priceItem, matchKeyLine);
            }
        }
    } else {
        unsigned long status = 0;
        if (productItem.isExists(matchKeyStatus))
            status = getUnsignedLongFromMatch(productItem, matchKeyStatus);
        if (status & matchKeyStatusProductWithPriceOnNextLine) {
            int nextLineIndex = OCRResults::indexOfElementPastLineOfMatchAtIndex(productIndex, &matches);
            if (nextLineIndex != -1) {
                priceIndex = OCRResults::elementWithTypeInlineAtIndex(nextLineIndex, tp, &matches, retailerParams);
                if (priceIndex >= 0) {
                    unsigned long status = 0;
                    MatchPtr priceItem = matches[priceIndex];
                    if (priceItem.isExists(matchKeyStatus)) {
                        status = getUnsignedLongFromMatch(priceItem, matchKeyStatus);
                    }
                    if ((status & matchKeyStatusInProductsColumn) || (status & matchKeyStatusBeforeProductsColumn)) {
                        LayoutLog("priceOrPartialPriceIndexForProduct: rejecting price [%s], in products column", toUTF8(getStringFromMatch(priceItem, matchKeyText)).c_str());
                        // Is there another price on the same line without this issue?
                        bool found = false;
                        vector<int> listIndices = OCRResults::elementsListWithTypeInlineAtIndex(nextLineIndex, tp, &matches);
                        if (listIndices.size() > 1) {
                            for (int j=0; j<listIndices.size(); j++) {
                                int currentIndex = listIndices[j];
                                if (currentIndex == priceIndex)
                                    continue;
                                MatchPtr dd = matches[j];
                                status = 0;
                                if (dd.isExists(matchKeyStatus))
                                    status = getUnsignedLongFromMatch(dd, matchKeyStatus);
                                if (!((status & matchKeyStatusInProductsColumn) || (status & matchKeyStatusBeforeProductsColumn))) {
                                    priceIndex = currentIndex;
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (!found)
                            priceIndex = -1;
                    }
                    if (priceIndex >=0) {
                        // Sanity check: make sure there is no product number on the price line, with index less than the price index: if so, it's probably a bug, better let this product grab that price later
                        int otherProductIndex = OCRResults::elementWithTypeInlineAtIndex(nextLineIndex, OCRElementTypeProductNumber, &matches, retailerParams);
                        if ((otherProductIndex >= 0) && (productIndex < priceIndex)) {
                            LayoutLog("priceOrPartialPriceInxexForProductAtIndex: ERROR, not returning price [%s] at index %d for product [%s] because we found product [%s] on the price line", toUTF8(getStringFromMatch(matches[priceIndex], matchKeyText)).c_str(), priceIndex, toUTF8(getStringFromMatch(matches[productIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[otherProductIndex], matchKeyText)).c_str());
                            priceIndex = -1;
                        }
                    }
                }
                if ((priceIndex >= 0) && (priceLineNumberPtr != NULL)) {
                    MatchPtr priceItem = matches[priceIndex];
                    *priceLineNumberPtr = getIntFromMatch(priceItem, matchKeyLine);
                }
            }
        } else {
            // If price is after product (true of all retailers as of 9/20/2015) require that price be after product, and pick the one furthest away (to overcome cases where say "18-OZ" in the middle of a description got tagged as a price)
            // 10-2-2015 just in case a merging error caused a non-product price to get merged after the price, check the flags situation
            if (retailerParams->pricesAfterProduct) {
                int indexBestPrice = -1;
                int pointsBestPrice = 0;
                int indexEndLine = OCRResults::indexLastElementOnLineAtIndex(productIndex, &matches);
                for (int i=indexEndLine; i > productIndex; i--) {
                    MatchPtr possiblePriceItem = matches[i];
                    unsigned long currentStatusFlags = 0;
                    if (possiblePriceItem.isExists(matchKeyStatus))
                        currentStatusFlags = getUnsignedLongFromMatch(possiblePriceItem, matchKeyStatus);
                    if ((currentStatusFlags & matchKeyStatusInProductsColumn) || (currentStatusFlags & matchKeyStatusInProductsColumn)) {
                        LayoutLog("priceOrPartialPriceIndexForProduct: rejecting price [%s], in products column", toUTF8(getStringFromMatch(possiblePriceItem, matchKeyText)).c_str());
                    } else if (getIntFromMatch(possiblePriceItem, matchKeyElementType) == OCRElementTypePrice) {
                        int pointsCurrent = 0;
                        if (currentStatusFlags & matchKeyStatusInPricesColumn)
                            pointsCurrent++;
                        if (currentStatusFlags & matchKeyStatusPriceCameWithProduct)
                            pointsCurrent++;
                        if ((indexBestPrice < 0) || (pointsCurrent > pointsBestPrice)) {
                            // Update!
                            indexBestPrice = i;
                            pointsBestPrice = pointsCurrent;
                        }
                    }
                }
                if (indexBestPrice >= 0) {
                    priceIndex = indexBestPrice;
                } else {
                    priceIndex = -1;
                }
            } else {
                priceIndex = OCRResults::elementWithTypeInlineAtIndex(productIndex, tp, &matches, retailerParams);
            }
            if ((priceIndex >= 0) && (priceLineNumberPtr != NULL)) {
                *priceLineNumberPtr = -1;
            }
        }
    }
    
    return (priceIndex);
}

// Note: only meant for retailers like CVS where the price follows the description on the same line
int OCRResults::productPriceIndexForDescriptionAtIndex(int descriptionIndex, OCRElementType tp, MatchVector &matches, retailer_t *retailerParams) {
    MatchPtr descriptionItem = matches[descriptionIndex];
//#if DEBUG
//    wstring tmpItemText = getStringFromMatch(productItem, matchKeyText);
//    if (tmpItemText == L"044600011950") {
//        DebugLog("");
//    }
//#endif
    // Because of the limited scope of this API, just find the rightmost item of type OCRElementTypeProductPrice
    int indexLastItemOnLine = OCRResults::indexLastElementOnLineAtIndex(descriptionIndex, &matches);
    int priceIndex = -1;
    for (int i=descriptionIndex+1; i<=indexLastItemOnLine; i++) {
        OCRElementType currentTp = getElementTypeFromMatch(matches[i], matchKeyElementType);
        if (currentTp == tp)
            priceIndex = i;
    }
    
    return priceIndex;
} // productPriceIndexForDescriptionAtIndex

// Sets the productLineNumberPtr output param to the line number of the matching product (if found)
int OCRResults::productIndexForPriceAtIndex(int priceIndex, MatchVector &matches, retailer_t *retailerParams, int *productLineNumberPtr) {
    MatchPtr priceItem = matches[priceIndex];
//#if DEBUG
//    wstring tmpItemText = getStringFromMatch(productItem, matchKeyText);
//    if (tmpItemText == L"044600011950") {
//        DebugLog("");
//    }
//#endif
    int productIndex = -1;
    if (retailerParams->priceAboveProduct) {
        int indexStartProductLine = OCRResults::indexLastElementOnLineAtIndex(priceIndex, &matches);
        if (indexStartProductLine > 0) {
            productIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartProductLine+1, OCRElementTypeProductNumber, &matches, retailerParams);
        }
    } else {
        // All other cases, assume product is on the same line as the price. Note: this is not true in many cases of quantities, OK for now since productIndexForPriceAtIndex is only used by isWithinPriceColumn for now, which may skip some prices above/below
        productIndex = OCRResults::elementWithTypeInlineAtIndex(priceIndex, OCRElementTypeProductNumber, &matches, retailerParams);
    }
    
    if ((productIndex >= 0) && (productLineNumberPtr != NULL)) {
        MatchPtr priceItem = matches[priceIndex];
        *productLineNumberPtr = getIntFromMatch(matches[productIndex], matchKeyLine);
    }
    return (productIndex);
} // productIndexForPriceAtIndex

int OCRResults::priceIndexForProductAtIndex(int productIndex, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams) {
    return OCRResults::priceOrPartialPriceIndexForProductAtIndex(productIndex, OCRElementTypePrice, matches, priceLineNumberPtr, retailerParams);
}

int OCRResults::partialPriceIndexForProductAtIndex(int productIndex, MatchVector &matches, int *priceLineNumberPtr, retailer_t *retailerParams) {
    return OCRResults::priceOrPartialPriceIndexForProductAtIndex(productIndex, OCRElementTypePartialProductPrice, matches, priceLineNumberPtr, retailerParams);
}

void OCRResults::checkForProductsOnDiscountLines(MatchVector& matches) {
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if (tp != OCRElementTypeProductNumber)
            continue;
        
        unsigned long matchStatus = 0;
        if (d.isExists(matchKeyStatus))
            matchStatus = getUnsignedLongFromMatch(d, matchKeyStatus);

        if (matchStatus & matchKeyStatusNotAProduct)
            continue;
        
        int discountIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePriceDiscount, &matches, NULL);
        if (discountIndex >= 0) {
            // This is NOT a product, mark it
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(matchStatus | matchKeyStatusNotAProduct)).castDown<EmptyType>();
        }
        
        int nextLine = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
        if (nextLine >= 0)
            i = nextLine; // Loop will increment
    }
} // checkForProductsOnDiscountLines

// Check a partial price to determine if it can now be deemed a valid price. For now what we check is just spaces that are redundant and need to be removed
bool OCRResults::checkPartialPrice (MatchPtr m, OCRResults *results) {
    if (!m.isExists(matchKeyN) || !m.isExists(matchKeyRects))
        return false;
    int nRects = (int)(*m[matchKeyN].castDown<size_t>());
    SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
    wstring itemText = getStringFromMatch(m, matchKeyText);
    if ((int)(itemText.length()) != nRects) {
        ErrorLog("textAndRectsInRange: ERROR, mismatch between nRect=%d and text length=%d for [%s]", (int)nRects, (int)itemText.length(), toUTF8(itemText).c_str());
        return false;
    }
    if (itemText.length() == 0)
        return false;
    
    // Before we test spaces, we need stats on digit height
    float digitWidth, digitHeight, digitSpacing;
    bool uniform; int lengthNormalSequence; float averageHeightDigits;
    bool uniformPriceSequence; int lengthPriceSequence; float averageHeightPriceSequence;
    float productWidth = 0;
    OCRResults::averageDigitHeightWidthSpacingInMatch(m, digitWidth, digitHeight, digitSpacing, uniform, lengthNormalSequence, averageHeightDigits, lengthPriceSequence, uniformPriceSequence, averageHeightPriceSequence, productWidth, &results->retailerParams);
    if (digitWidth <= 0)
        return false;

    bool changesMade = false;
    // Now test (and optionally remove) all spaces. Also remove any leading spaces in the process.
    for (int k=0; k<nRects && k<rects->size(); k++) {
        wchar_t ch = itemText[k];
        if (ch != ' ')
            continue;
        if ((k == 0) || (k == rects->size()-1)) {
            // Remove it
            if (k == 0)
                itemText = itemText.substr(1, wstring::npos);
            else {
                if (k < rects->size()-1)
                    itemText = itemText.substr(0, k) + itemText.substr(k+1, wstring::npos);
                else
                    itemText = itemText.substr(0, k);
            }
            // Remove the rect as well
            rects->erase(rects->begin()+k);
            changesMade = true;
            k--;
            continue;
        }
        // Now test that space
        if (k == rects->size()-1)
            break;  // Done
        CGRect rectBefore = *(*rects)[k-1];
        wchar_t chBefore = itemText[k-1];
        CGRect rectAfter = *(*rects)[k+1];
        wchar_t chAfter = itemText[k+1];
        if ((chBefore != ' ') && (chAfter != ' ')) {
            float spaceBetween = rectSpaceBetweenRects(rectBefore, rectAfter);
            // Target receipt, had spurious space 8 versus height 22 (36%)
            // Target receipt, had spurious space 9 versus height 19 (47%)
            if (spaceBetween < 0.50 * digitHeight) {
                // Remove!
                itemText = itemText.substr(0, k) + itemText.substr(k+1, wstring::npos);
                // Remove the rect as well
                rects->erase(rects->begin()+k);
                changesMade = true;
                k--;
                continue;
            }
        }
    }
    
    if (changesMade) {
        LayoutLog("checkPartialPrice: adjusting [%s] to [%s]", toUTF8(getStringFromMatch(m, matchKeyText)).c_str(), toUTF8(itemText).c_str());
        // Update match text (rects were updated ongoing)
        m[matchKeyText] = SmartPtr<wstring> (new wstring(itemText)).castDown<EmptyType>();
        if (testForPrice(itemText, results) == OCRElementTypePrice) {
            // Upgrade type!
            LayoutLog("checkPartialPrice: upgrading partial price [%s] to [OCRElementPrice]!", toUTF8(itemText).c_str());
            m[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
            return true;
        }
    }

    return false;
}

/* If the first character overlapping target range is not the first in the text string, or is preceded by a space, abort the whole thing: we want only text contained cleanly in the target range, not slicing text from a longer string!
*/
bool OCRResults::textAndRectsInRange(MatchPtr m, CGRect range, int *entirelyContainedOrNoOverlap, bool xRange, bool yRange, vector<SmartPtr<CGRect> > *rectsPtr, wstring *textPtr) {
    CGRect matchRange = OCRResults::matchRange(m);
    bool xContained = false, yContained = false;
    if ((rectLeft(matchRange) >= rectLeft(range)) && (rectRight(matchRange) <= rectRight(range))) {
        xContained = true;
    }
    if ((rectBottom(matchRange) >= rectBottom(range)) && (rectTop(matchRange) <= rectTop(range))) {
        yContained = true;
    }

    if ((!xRange || xContained) && (!yRange || yContained)) {
        *entirelyContainedOrNoOverlap = 1;
        return true;
    }

    if (!m.isExists(matchKeyN) || !m.isExists(matchKeyRects))
        return false;
    size_t nRects = *m[matchKeyN].castDown<size_t>();
    wstring itemText = getStringFromMatch(m, matchKeyText);
    if (itemText.length() != nRects) {
        ErrorLog("textAndRectsInRange: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
        return false;
    }
    
    SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
    wstring resText;
    vector<SmartPtr<CGRect> > resRects;

    for (int k=0; k<nRects && k<rects->size(); k++) {
        CGRect rect = *(*rects)[k];
        if ((!xRange || overlappingRectsXAxisAnd(rect, range))
            && (!yRange || overlappingRectsYAxisAnd(rect, range))) {
            if ((resText.length() == 0) && (k>0)) {
                // If the first character overlapping target range is not the first in the text string, or is preceded by a space, abort the whole thing: we want only text contained cleanly in the target range, not slicing text from a longer string!
                if ((itemText[k] != ' ') && (itemText[k-1] != ' ')) {
                    LayoutLog("textAndRectsInRange: rejecting [%s] from yielding a partial price because text starts before prices columnn, at line %d", toUTF8(itemText).c_str(), getIntFromMatch(m, matchKeyLine));
                    return false;
                }
            }
            // Don't add leading spaces
            if ((resText.length() > 0) || (itemText[k] != ' ')) {
                resRects.push_back(SmartPtr<CGRect>(new CGRect(rect) ));
                resText += itemText[k];
            }
        }
    }

    *rectsPtr = resRects;
    *textPtr = resText;
    
    return true;
}

bool OCRResults::textAndRectsOutsideRange(MatchPtr m, CGRect range, int *entirelyContained, bool xRange, bool yRange, vector<SmartPtr<CGRect> > *rectsBeforePtr, wstring *textBeforePtr, vector<SmartPtr<CGRect> > *rectsAfterPtr, wstring *textAfterPtr) {
    CGRect matchRange = OCRResults::matchRange(m);
    bool xContained = false, yContained = false;
    if ((rectLeft(matchRange) >= rectLeft(range)) && (rectRight(matchRange) <= rectRight(range))) {
        xContained = true;
    }
    if ((rectBottom(matchRange) >= rectBottom(range)) && (rectTop(matchRange) <= rectTop(range))) {
        yContained = true;
    }

    if ((!xRange || xContained) && (!yRange || yContained)) {
        *entirelyContained = 1;
        return true;
    }

    if (!m.isExists(matchKeyN) || !m.isExists(matchKeyRects))
        return false;
    size_t nRects = *m[matchKeyN].castDown<size_t>();
    wstring itemText = getStringFromMatch(m, matchKeyText);
    if (itemText.length() != nRects) {
        ErrorLog("textAndRectsOutsideRange: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
        return false;
    }
    
    SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
    wstring resTextBefore;
    vector<SmartPtr<CGRect> > resRectsBefore;
    wstring resTextAfter;
    //SmartPtr<vector<SmartPtr<CGRect> > > resRectsAfter(new vector<SmartPtr<CGRect> >());
    vector<SmartPtr<CGRect> > resRectsAfter;

    for (int k=0; k<nRects && k<rects->size(); k++) {
        CGRect rect = *(*rects)[k];
        // Is this rect outside the range?
            // Test X: either X is not important or there is no X overlap
        if ((!xRange || !overlappingRectsXAxisAnd(rect, range))
            // Test Y: either Y is not important or there is no Y overlap
            && (!yRange || !overlappingRectsYAxisAnd(rect, range))) {
            bool endsBefore = rectRight(rect) < rectLeft(range);
            bool startsAfter = rectLeft(rect) > rectRight(range);
            if (endsBefore) {
                resRectsBefore.push_back(SmartPtr<CGRect>(new CGRect(rect) ));
                resTextBefore += itemText[k];
            } else if (startsAfter) {
                resRectsAfter.push_back(SmartPtr<CGRect>(new CGRect(rect) ));
                resTextAfter += itemText[k];
            }
        }
    }
    *rectsBeforePtr = resRectsBefore;
    *textBeforePtr = resTextBefore;
    *rectsAfterPtr = resRectsAfter;
    *textAfterPtr = resTextAfter;
    
    return true;
}

int OCRResults::highestProductLineNumber (MatchVector matches) {
    int highestProdLineNumber = -1000;
    for (int i=(int)matches.size()-1; i>=0; i--) {
        int productIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matches, NULL);
        if (productIndex >= 0) {
            MatchPtr d = matches[productIndex];
            if (d.isExists(matchKeyLine)) {
                // Found it!
                int line = getIntFromMatch(d, matchKeyLine);
                if (line > highestProdLineNumber) {
#if DEBUG
                    if (highestProdLineNumber >= 0) {
                        wstring text = getStringFromMatch(d, matchKeyText);
                        MergingLog("highestProductLineNumber: WARNING, found item [%s] product line=%d at index %d, lower than product line number found later in the list", toUTF8(text).c_str(), line, i);
                    }
#endif
                    highestProdLineNumber = line;
                }
            }
        }
    }
    if (highestProdLineNumber < 0)
        return -1;
    else
        return highestProdLineNumber;
}

/* Returns TRUE only if if has definite reason that existing or new matches is better than the other, FALSE otherwise
*/
bool OCRResults::winnerExistingVersusNewMatch (MatchPtr existingM, MatchPtr newM, bool *newWins, bool *replace) {
    *replace = false;
    float existingConfidence = getFloatFromMatch(existingM, matchKeyConfidence);
    float newConfidence = getFloatFromMatch(newM, matchKeyConfidence);
    if ((round(existingConfidence) == 100) && (round(newConfidence) < 100)) {
        if (newWins != NULL)
            *newWins = false;
        *replace = true;
        return true;
    } else if ((round(newConfidence) == 100) && (round(existingConfidence) < 100)) {
        if (newWins != NULL)
            *newWins = true;
        *replace = true;
        return true;
    } else if ((round(existingConfidence) == 100) && (round(newConfidence) == 100)) {
        return false; // Can't decide, both at 100% confidence!
    }
    
//#if DEBUG
//    wstring existingText = getStringFromMatch(existingM, matchKeyText);
//    wstring newText = getStringFromMatch(newM, matchKeyText);
//    if ((existingText == L"13.99") && (newText == L"13.29")) {
//        DebugLog("");
//    }
//#endif
    
    OCRElementType tpExisting = getElementTypeFromMatch(existingM, matchKeyElementType);
    OCRElementType tpNew = getElementTypeFromMatch(newM, matchKeyElementType);
    if ((tpExisting != OCRElementTypePrice) || (tpNew != OCRElementTypePrice)) {
        return false;   // No insights for items other than price at this point
    }
    
    unsigned long existingStatus = 0, newStatus = 0;
    if (existingM.isExists(matchKeyStatus))
        existingStatus = getUnsignedLongFromMatch(existingM, matchKeyStatus);
    if (newM.isExists(matchKeyStatus))
        newStatus = getUnsignedLongFromMatch(newM, matchKeyStatus);
    
    if ((existingStatus & matchKeyStatusHasTaxAbove)
        && ((newStatus == 0) || !(newStatus & matchKeyStatusHasTaxAbove))) {
        if (newWins != NULL)
            *newWins = false;
        return true;
    } else if (((existingStatus == 0) || !(existingStatus & matchKeyStatusHasTaxAbove))
        && (newStatus & matchKeyStatusHasTaxAbove)) {
        if (newWins != NULL)
            *newWins = true;
        return true;
    }
    
    if ((existingStatus & matchKeyStatusPriceCameWithProduct)
        && !(newStatus & matchKeyStatusPriceCameWithProduct)) {
        if (newWins != NULL)
            *newWins = false;
        return true;
    } else if (!(existingStatus & matchKeyStatusPriceCameWithProduct)
        && (newStatus & matchKeyStatusPriceCameWithProduct)) {
        if (newWins != NULL)
            *newWins = true;
        return true;
    }

    if ((existingStatus & matchKeyStatusInPricesColumn)
        && !(newStatus & matchKeyStatusInPricesColumn)) {
        if (newWins != NULL)
            *newWins = false;
        return true;
    } else if (!(existingStatus & matchKeyStatusInPricesColumn)
        && (newStatus & matchKeyStatusInPricesColumn)) {
        if (newWins != NULL)
            *newWins = true;
        return true;
    }

    return false;
} // winnerExistingVersusNewMatch

bool OCRResults::isMeaningfulLineAtIndex(int lineIndex, OCRResults *results) {
    MatchVector matches = results->matchesAddressBook;
    int lineStart = indexFirstElementOnLineAtIndex(lineIndex, &matches);
    int lineEnd = indexLastElementOnLineAtIndex(lineIndex, &matches);
    if ((lineStart < 0) || (lineStart >= matches.size()) || (lineEnd < 0) || (lineEnd >= matches.size()))
        return false;
    for (int i=lineStart; i<=lineEnd; i++) {
        MatchPtr d = matches[i];
        if ((getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeUnknown) && (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeInvalid))
            return true;
// 09-03-2015 retiring the below, was creating tons of bogus lines when processing blurry frames with no meaningful data. The only reason for the below (creating a line so that we can find the quantity two lines below) can be handled differently.
//        // Even if untyped, consider an item meaningful if significantly overlapping the typed range. For example, it means we'll accept a description line on a Home Depot receipt below a product line (and possibly above the quantity line)
//        if (results->typedResultsRange.size.width > 0) {
//            CGRect itemRange = matchRange(d);
//            float xOverlap = overlapAmountRectsXAxisAnd(itemRange, results->typedResultsRange);
//            if (xOverlap > itemRange.size.width * 0.50)
//                return true;
//        }
    }
    return false;
}


// Looks before or/and after a given match to find a product price, then returns true if ranges overlap (indicating items are in the same column)
bool OCRResults::isWithinProductNumbersColumn(int matchIndex, MatchVector matches, OCRResults *results) {
    
    MatchPtr d = matches[matchIndex];
    CGRect itemRect = matchRange(d);
    
    // Do we have the product numbers range?
    if (results->productsRange.size.width > 0) {
        return (overlappingRectsXAxisAnd(itemRect, results->productsRange));
    }
    
    int productIndex = -1;
    CGRect productRect;
    for (int i=matchIndex; i>=0; i--) {
        productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matches, NULL);
        if (productIndex == -1) {
            // Try previous line
            int startLine = indexFirstElementOnLineAtIndex(i, &matches) - 1;
            if ((startLine < 0) || (startLine > i))
                return false; // Abort, we are stuck in place!
            i = startLine;
            continue; // Will decrement i to be on previous line
        }
        // Got product, we are done!
        productRect = matchRange(matches[productIndex]);
        break;
    }
    
    if (productIndex < 0) {
        for (int i=matchIndex; i < matches.size(); i++) {
            productIndex = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matches, NULL);
            if (productIndex < 0) {
                // Try next line
                int endLine = indexLastElementOnLineAtIndex(i, &matches);
                if ((endLine < 0) || (endLine < i))
                    return false; // Abort, we are stuck in place!
                i = endLine;
                continue; // Will increment i to be on next line
            }
            // Got product, we are done!
            productRect = matchRange(matches[productIndex]);
            break;
        }
    }
    
    if (productIndex < 0)
        return false;
    
    return (overlappingRectsXAxisAnd(itemRect, productRect));
} // isWithinProductNumbersColumn

// Used only by isWithinPricesColumn below at this time
bool OCRResults::priceRectsXOverlap (MatchPtr potentialPriceItem, CGRect productPriceRect, CGRect potentialProductPriceRect, OCRResults *results) {
    if (overlappingRectsXAxisAnd(potentialProductPriceRect, productPriceRect)) {
        // Still need to check that price ends no further left than 2x the width of a digit. Reason: lines like "Cartwheel 5% off $13.99" appear on Target receipts under the '3' of a "$13.29" price above it
        if (results->globalStats.averageHeightDigits.average > 0) {
            int numDigits = (rectRight(productPriceRect) - rectRight(potentialProductPriceRect)) / results->globalStats.averageHeightDigits.average;
            if (numDigits < 2)
                return true;
            else
                return false;
        }
    }
    return false;
}

// Looks before or/and after a given match to find a product price, then returns true if ranges overlap (indicating items are in the same column)
// PQTODO this function works only when prices are on the same line as products, need to implement productIndexForPriceAtIndex then we can properly extend the below to all retailers. Until then, caller needs to call this function only after checking retailerParams
bool OCRResults::isWithinPricesColumn(int matchIndex, MatchVector &matches, OCRResults *results) {

    if ((matchIndex < 0) || (matchIndex > matches.size()-1))    return false;
    
    MatchPtr potentialProductPriceItem = matches[matchIndex];
    CGRect potentialProductPriceRect = matchRange(potentialProductPriceItem);
    OCRElementType tpPotentialProductPrice = getElementTypeFromMatch(potentialProductPriceItem, matchKeyElementType);
    
    if ((results->retailerParams.hasDiscountsWithoutProductNumbers) && ((tpPotentialProductPrice == OCRElementTypePriceDiscount) || (tpPotentialProductPrice == OCRElementTypePotentialPriceDiscount))) {
        // True for Kmart. In this case just require finding another price anywhere above/below (if it's associated with a product) or another price discount anywhere above/below (no need to test further because discounts are harder to match the regex)
        int currentLine = getIntFromMatch(potentialProductPriceItem, matchKeyLine);
        for (int i=0; i<matches.size(); i++) {
            MatchPtr d = matches[i];
            if (getIntFromMatch(d, matchKeyLine) == currentLine) {
                // Skip that line, it's the one containing the price being tested
                i = indexLastElementOnLineAtIndex(i, &matches); continue;
            }
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp == OCRElementTypePrice) {
                // Make sure it's a product price
                int otherPriceProductIndex = productIndexForPriceAtIndex(i, matches, &results->retailerParams);
                if (otherPriceProductIndex >= 0) {
                    CGRect otherPriceRect = matchRange(d);
                    if (priceRectsXOverlap(potentialProductPriceItem, otherPriceRect, potentialProductPriceRect, results))
                        return true;
                }
            } else if ((tp == OCRElementTypePriceDiscount) || (tp == OCRElementTypePotentialPriceDiscount)) {
                CGRect otherPriceRect = matchRange(d);
                if (priceRectsXOverlap(potentialProductPriceItem, otherPriceRect, potentialProductPriceRect, results))
                    return true;
            }
        }
    }
    
    CGRect productPriceRect;
    // First look above current line: we are looking for a line where there is a product, and on that line, if there is a price with a range overlapping current price rect. Note: we look at all prices till we find an overlap, to protect against a situation where line above has a bogus price found in the description (then to the right, the actual price). In that case, allow the 2nd price to be used for the purpose of overlap. In the case where the line being examined also has bogus + real prices, it means BOTH will be tagged as being in the prices column: no big deal since we scan lines for prices starting on the right, and only revise that if we find one to the left that has superior flags
    // Start with line just above
    int lineEnd = indexFirstElementOnLineAtIndex(matchIndex, &matches) - 1;
    while (lineEnd >= 0) {
        int lineStart = indexFirstElementOnLineAtIndex(lineEnd, &matches);
        // Is there a product on this line?
        if (elementWithTypeInlineAtIndex(lineStart, OCRElementTypeProductNumber, &matches, NULL) >= 0) {
            // Iterate through all prices on this line, testing each price for possible X overlap
            bool foundPriceOnOtherLine = false;
            for (int i=lineStart; i<=lineEnd; i++) {
                MatchPtr priceOnOtherLine = matches[i];
                if (getIntFromMatch(priceOnOtherLine, matchKeyElementType) == OCRElementTypePrice) {
                    foundPriceOnOtherLine = true;
                    CGRect priceOnOtherLineRect = matchRange(priceOnOtherLine);
                    if (priceRectsXOverlap(potentialProductPriceItem, priceOnOtherLineRect, potentialProductPriceRect, results))
                        return true;
                }
            } // iterate over all prices on other line, look for one that overlaps
            if (foundPriceOnOtherLine)
                return false;   // There was a price on the other line with a product
        } // found product on other line
        // Move to previous line
        lineEnd = lineStart - 1;
    } // iterate backward looking for lines with a product and one or more price on the same line
    
    
    // Now check forward with line just above
    int lineStart = indexLastElementOnLineAtIndex(matchIndex, &matches) + 1;
    while (lineStart < matches.size()) {
        int lineEnd = indexLastElementOnLineAtIndex(lineStart, &matches);
        // Is there a product on this line?
        if (elementWithTypeInlineAtIndex(lineStart, OCRElementTypeProductNumber, &matches, NULL) >= 0) {
            // Iterate through all prices on this line, testing each price for possible X overlap
            bool foundPriceOnOtherLine = false;
            for (int i=lineStart; i<=lineEnd; i++) {
                MatchPtr priceOnOtherLine = matches[i];
                if (getIntFromMatch(priceOnOtherLine, matchKeyElementType) == OCRElementTypePrice) {
                    foundPriceOnOtherLine = true;
                    CGRect priceOnOtherLineRect = matchRange(priceOnOtherLine);
                    if (priceRectsXOverlap(potentialProductPriceItem, priceOnOtherLineRect, potentialProductPriceRect, results))
                        return true;
                }
            } // iterate over all prices on other line, look for one that overlaps
            if (foundPriceOnOtherLine)
                return false;   // There was a price on the other line with a product
        } // found product on other line
        // Move to next line
        lineStart = lineEnd + 1;
    } // iterate forward looking for lines with a product and one or more price on the same line

    // Looks like we tried all lines above & below and failed to find another overlapping price on a product line
    return false;
}

// Used after updating one of matchKeyText, matchKeyText2 or matchKeyText3 and if confidences / counts warrant it, re-order
void OCRResults::sortText123(MatchPtr m) {
    bool gotMatchKey2 = m.isExists(matchKeyText2);
    bool gotMatchKey3 = m.isExists(matchKeyText3);
    // Use a bubble sort approach:
    // Step 1: compare text with text2 (swap if needed) then compare text2 with text3 (swap if needed). This step ensures that text3 is the lowest
    // Step 2: compare text with text2 (swap if needed). This step ensures that text2 is lower than text (and text3 is already the lowest so we are done
    if (gotMatchKey2
        && ((getIntIfExists(m, matchKeyCountMatched, 1) < getIntIfExists(m, matchKeyCountMatched2, 1)) || ((getIntIfExists(m, matchKeyCountMatched2, 1) == getIntIfExists(m, matchKeyCountMatched, 1)) && (getFloatFromMatch(m, matchKeyConfidence) < getFloatFromMatch(m, matchKeyConfidence2))))) {
        // Swap them!
        wstring text = getStringFromMatch(m, matchKeyText);
        wstring fullText;
        if (m.isExists(matchKeyFullLineText)) fullText = getStringFromMatch(m, matchKeyFullLineText);
        int quantity = -1;
        if (m.isExists(matchKeyQuantity)) quantity = getIntFromMatch(m, matchKeyQuantity);
        int pricePerItem = -1;
        if (m.isExists(matchKeyPricePerItem)) pricePerItem = getFloatFromMatch(m, matchKeyPricePerItem);
        int count = getIntIfExists(m, matchKeyCountMatched, 1);
        float confidence = getFloatFromMatch(m, matchKeyConfidence);
        wstring text2 = getStringFromMatch(m, matchKeyText2);
        wstring fullText2;
        if (m.isExists(matchKeyFullLineText2)) fullText2 = getStringFromMatch(m, matchKeyFullLineText2);
        int quantity2 = -1;
        if (m.isExists(matchKeyQuantity2)) quantity2 = getIntFromMatch(m, matchKeyQuantity2);
        int pricePerItem2 = -1;
        if (m.isExists(matchKeyPricePerItem2)) pricePerItem2 = getFloatFromMatch(m, matchKeyPricePerItem2);
        int count2 = getIntIfExists(m, matchKeyCountMatched2, 1);
        float confidence2 = getFloatFromMatch(m, matchKeyConfidence2);
//#if DEBUG
//        if ((text2 == L"065676136851") || (text2 == L"065876136851")) {
//            DebugLog("");
//        }
//#endif
        MergingLog("importNewResults: upgrading item [%s] [type %s] count %d conf %.0f from text2 to text - replacing [%s] count %d conf %.0f", (stringForOCRElementType(getElementTypeFromMatch(m, matchKeyElementType))).c_str(), toUTF8(text2).c_str(), getIntIfExists(m, matchKeyCountMatched2, 1), getFloatFromMatch(m, matchKeyConfidence2), toUTF8(text).c_str(), getIntIfExists(m, matchKeyCountMatched, 1), getFloatFromMatch(m, matchKeyConfidence));
        m[matchKeyText2] = (SmartPtr<wstring> (new wstring(text))).castDown<EmptyType>();
        if (fullText.length()>0) m[matchKeyFullLineText2] = (SmartPtr<wstring> (new wstring(fullText))).castDown<EmptyType>();
        if (quantity >= 0) m[matchKeyQuantity2] = (SmartPtr<int> (new int(quantity))).castDown<EmptyType>();
        if (pricePerItem >= 0) m[matchKeyPricePerItem2] = (SmartPtr<float> (new float(pricePerItem))).castDown<EmptyType>();
        m[matchKeyConfidence2] = (SmartPtr<float> (new float(confidence))).castDown<EmptyType>();
        m[matchKeyCountMatched2] = (SmartPtr<int> (new int(count))).castDown<EmptyType>();
        m[matchKeyText] = (SmartPtr<wstring> (new wstring(text2))).castDown<EmptyType>();
        if (fullText2.length()>0) m[matchKeyFullLineText] = (SmartPtr<wstring> (new wstring(fullText2))).castDown<EmptyType>();
        if (quantity2 >= 0) m[matchKeyQuantity] = (SmartPtr<int> (new int(quantity2))).castDown<EmptyType>();
        if (pricePerItem2 >= 0) m[matchKeyPricePerItem] = (SmartPtr<float> (new float(pricePerItem2))).castDown<EmptyType>();
        m[matchKeyConfidence] = (SmartPtr<float> (new float(confidence2))).castDown<EmptyType>();
        m[matchKeyCountMatched] = (SmartPtr<int> (new int(count2))).castDown<EmptyType>();
    }
    if (gotMatchKey3
        && ((getIntIfExists(m, matchKeyCountMatched2, 1) < getIntIfExists(m, matchKeyCountMatched3, 1)) || ((getIntIfExists(m, matchKeyCountMatched2, 1) == getIntIfExists(m, matchKeyCountMatched3, 1)) && (getFloatFromMatch(m, matchKeyConfidence2) < getFloatFromMatch(m, matchKeyConfidence3))))) {
        // Swap text2 & text3!
        wstring text2 = getStringFromMatch(m, matchKeyText2);
        wstring fullText2;
        if (m.isExists(matchKeyFullLineText2)) fullText2 = getStringFromMatch(m, matchKeyFullLineText2);
        int quantity2 = -1;
        if (m.isExists(matchKeyQuantity2)) quantity2 = getIntFromMatch(m, matchKeyQuantity2);
        int pricePerItem2 = -1;
        if (m.isExists(matchKeyPricePerItem2)) pricePerItem2 = getFloatFromMatch(m, matchKeyPricePerItem2);
        int count2 = getIntIfExists(m, matchKeyCountMatched2, 1);
        float confidence2 = getFloatFromMatch(m, matchKeyConfidence2);
        wstring text3 = getStringFromMatch(m, matchKeyText3);
        wstring fullText3;
        if (m.isExists(matchKeyFullLineText3)) fullText3 = getStringFromMatch(m, matchKeyFullLineText3);
        int quantity3 = -1;
        if (m.isExists(matchKeyQuantity3)) quantity3 = getIntFromMatch(m, matchKeyQuantity3);
        int pricePerItem3 = -1;
        if (m.isExists(matchKeyPricePerItem3)) pricePerItem3 = getFloatFromMatch(m, matchKeyPricePerItem3);
        int count3 = getIntIfExists(m, matchKeyCountMatched3, 1);
        float confidence3 = getFloatFromMatch(m, matchKeyConfidence3);
        m[matchKeyText2] = (SmartPtr<wstring> (new wstring(text3))).castDown<EmptyType>();
        if (fullText3.length()>0) m[matchKeyFullLineText2] = (SmartPtr<wstring> (new wstring(fullText3))).castDown<EmptyType>();
        if (quantity3 >= 0) m[matchKeyQuantity2] = (SmartPtr<int> (new int(quantity3))).castDown<EmptyType>();
        if (pricePerItem3 >= 0) m[matchKeyPricePerItem2] = (SmartPtr<float> (new float(pricePerItem3))).castDown<EmptyType>();
        m[matchKeyConfidence2] = (SmartPtr<float> (new float(confidence3))).castDown<EmptyType>();
        m[matchKeyCountMatched2] = (SmartPtr<int> (new int(count3))).castDown<EmptyType>();
        m[matchKeyText3] = (SmartPtr<wstring> (new wstring(text2))).castDown<EmptyType>();
        if (fullText2.length()>0) m[matchKeyFullLineText3] = (SmartPtr<wstring> (new wstring(fullText2))).castDown<EmptyType>();
        if (quantity2 >= 0) m[matchKeyQuantity3] = (SmartPtr<int> (new int(quantity2))).castDown<EmptyType>();
        if (pricePerItem2 >= 0) m[matchKeyPricePerItem3] = (SmartPtr<float> (new float(pricePerItem2))).castDown<EmptyType>();
        m[matchKeyConfidence3] = (SmartPtr<float> (new float(confidence2))).castDown<EmptyType>();
        m[matchKeyCountMatched3] = (SmartPtr<int> (new int(count2))).castDown<EmptyType>();
    }
    
    // Now step2: comparing text & text2
    if (gotMatchKey2
        && ((getIntIfExists(m, matchKeyCountMatched, 1) < getIntIfExists(m, matchKeyCountMatched2, 1)) || ((getIntIfExists(m, matchKeyCountMatched2, 1) == getIntIfExists(m, matchKeyCountMatched, 1)) && (getFloatFromMatch(m, matchKeyConfidence) < getFloatFromMatch(m, matchKeyConfidence2))))) {
        // Swap text & text2!
        wstring text = getStringFromMatch(m, matchKeyText);
        wstring fullText;
        if (m.isExists(matchKeyFullLineText)) fullText = getStringFromMatch(m, matchKeyFullLineText);
        int quantity = -1;
        if (m.isExists(matchKeyQuantity)) quantity = getIntFromMatch(m, matchKeyQuantity);
        int pricePerItem = -1;
        if (m.isExists(matchKeyPricePerItem)) pricePerItem = getFloatFromMatch(m, matchKeyPricePerItem);
        int count = getIntIfExists(m, matchKeyCountMatched, 1);
        float confidence = getFloatFromMatch(m, matchKeyConfidence);
        wstring text2 = getStringFromMatch(m, matchKeyText2);
        wstring fullText2;
        if (m.isExists(matchKeyFullLineText2)) fullText2 = getStringFromMatch(m, matchKeyFullLineText2);
        int quantity2 = -1;
        if (m.isExists(matchKeyQuantity2)) quantity2 = getIntFromMatch(m, matchKeyQuantity2);
        int pricePerItem2 = -1;
        if (m.isExists(matchKeyPricePerItem2)) pricePerItem2 = getFloatFromMatch(m, matchKeyPricePerItem2);
        int count2 = getIntIfExists(m, matchKeyCountMatched2, 1);
        float confidence2 = getFloatFromMatch(m, matchKeyConfidence2);
        m[matchKeyText2] = (SmartPtr<wstring> (new wstring(text))).castDown<EmptyType>();
        if (fullText.length()>0) m[matchKeyFullLineText2] = (SmartPtr<wstring> (new wstring(fullText))).castDown<EmptyType>();
        if (quantity >= 0) m[matchKeyQuantity2] = (SmartPtr<int> (new int(quantity))).castDown<EmptyType>();
        if (pricePerItem >= 0) m[matchKeyPricePerItem2] = (SmartPtr<float> (new float(pricePerItem))).castDown<EmptyType>();
        m[matchKeyConfidence2] = (SmartPtr<float> (new float(confidence))).castDown<EmptyType>();
        m[matchKeyCountMatched2] = (SmartPtr<int> (new int(count))).castDown<EmptyType>();
        m[matchKeyText] = (SmartPtr<wstring> (new wstring(text2))).castDown<EmptyType>();
        if (fullText2.length()>0) m[matchKeyFullLineText] = (SmartPtr<wstring> (new wstring(fullText2))).castDown<EmptyType>();
        if (quantity2 >= 0) m[matchKeyQuantity] = (SmartPtr<int> (new int(quantity2))).castDown<EmptyType>();
        if (pricePerItem2 >= 0) m[matchKeyPricePerItem] = (SmartPtr<float> (new float(pricePerItem2))).castDown<EmptyType>();
        m[matchKeyConfidence] = (SmartPtr<float> (new float(confidence2))).castDown<EmptyType>();
        m[matchKeyCountMatched] = (SmartPtr<int> (new int(count2))).castDown<EmptyType>();
    }
} // sortText123

/* Takes the index of a price we just updated (or considering updating with the newText value) and:
- checks if the price is one associated with a product: if so, will check for a possible quantity/price-per-item below
- or if the price is associated with a quantity: if so will check for a product price above
*/
//PQTODO need to handle the case where 2nd frame starts with quantity / price, I think then we only tag the price as a possible price per item
bool OCRResults::checkPriceQuantityLinkage (int index, MatchVector matches, wstring newText, retailer_t *retailerParams) {
    if (!retailerParams->hasQuantities)
        return false;
    // Get indices / matches for all 3 elements: product price, quantity and price per item
    int productPriceIndex = -1, productQuantityIndex = -1, productPricePerItemIndex = -1;
    MatchPtr mProductPrice, mProductQuantity, mPricePerItem;
    bool currentIsProductPrice = false;
    
    // Is the element a product price?
    int productIndex = elementWithTypeInlineAtIndex(index, OCRElementTypeProductNumber, &matches, NULL);
    if (productIndex >= 0) {
        // Case where we point at the product line and quantity might be below it
        currentIsProductPrice = true;
        productPriceIndex = index;
        mProductPrice = matches[index];
        // Look for quantity / price per item on line below
        int indexNextLine = indexLastElementOnLineAtIndex(index, &matches) + 1;
        if ((indexNextLine < 0) || (indexNextLine > (int)matches.size()-2))
            return false;
        // Reject quantity matches if there also is a product number on that same line!
        int tmpProductIndexNextLine = elementWithTypeInlineAtIndex(indexNextLine, OCRElementTypeProductNumber, &matches, NULL);
        if (tmpProductIndexNextLine >= 0)
            return false;
        if (!mProductPrice.isExists(matchKeyLine))
            return false;
        int productPriceLineNumber = getIntFromMatch(mProductPrice, matchKeyLine);
        productQuantityIndex = elementWithTypeInlineAtIndex(indexNextLine, OCRElementTypeProductQuantity, &matches, NULL);
        if (productQuantityIndex < 0)
            return false;
        mProductQuantity = matches[productQuantityIndex];
        if (!mProductQuantity.isExists(matchKeyLine))
            return false;
        int productQuantityLineNumber = getIntFromMatch(mProductQuantity, matchKeyLine);
        if (productQuantityLineNumber != productPriceLineNumber + 1)
            return false;
        productPricePerItemIndex = elementWithTypeInlineAtIndex(indexNextLine, OCRElementTypePrice, &matches, NULL);
        if (productPricePerItemIndex < 0)
            return false;
        mPricePerItem = matches[productPricePerItemIndex];
    } else {
        // Current item must be the price per item then => quantity expected on the same line
        currentIsProductPrice = false;
        productPricePerItemIndex = index;
        mPricePerItem = matches[productPricePerItemIndex];
        // Reject if there is a product number on that line
        int tmpProductIndex = elementWithTypeInlineAtIndex(index, OCRElementTypeProductNumber, &matches, NULL);
        if (tmpProductIndex >= 0)
            return false;
        productQuantityIndex = elementWithTypeInlineAtIndex(index, OCRElementTypeProductQuantity, &matches, NULL);
        if (productQuantityIndex < 0)
            return false;             
        mProductQuantity = matches[productQuantityIndex];
        // Look for product / price per item on line above
        int indexPreviousLine = indexFirstElementOnLineAtIndex(index, &matches) - 1;
        if (indexPreviousLine >= 1) {
            if (!mProductQuantity.isExists(matchKeyLine))
                return false;
            int productQuantityLineNumber = getIntFromMatch(mProductQuantity, matchKeyLine);
            int productNumberIndex = elementWithTypeInlineAtIndex(indexPreviousLine, OCRElementTypeProductNumber, &matches, NULL);
            if (productNumberIndex < 0)
                return false;
            // Got a product on previous line, need price too
            productPriceIndex = elementWithTypeInlineAtIndex(indexPreviousLine, OCRElementTypePrice, &matches, retailerParams, true);
            if (productPriceIndex < 0)
                return false;
            mProductPrice = matches[productPriceIndex];
            // Still need to check product / price are on adjacent line above
            if (!mProductPrice.isExists(matchKeyLine))
                return false;
            int productPriceLineNumber = getIntFromMatch(mProductPrice, matchKeyLine);
            if (productPriceLineNumber != productQuantityLineNumber - 1)
                return false;
        }
    }
    
    // Here we have indices for all 3 elements + matches pointing to them
    // For each of price, quantity and price per item, we have up to 3 possible values (text, text2, text3) - 4 for current item if newText is set. Need to test all combinations of quantityxprice per item == product price then, if a match is found, do the right thing (setting correct values as "text" everywhere + setting confidence to 100)
    vector<int> quantity;
    vector<wstring> quantityText;
    vector<int> priceperitem;
    vector<wstring> priceperitemText;
    vector<int> productprice;
    vector<wstring> productpriceText;
    
    // Quantity
    wstring tmpStr = getStringFromMatch(mProductQuantity, matchKeyText);
    int tmpInt = atoi(toUTF8(tmpStr).c_str());
    if (tmpInt <= 0)
        return false;
    quantity.push_back(tmpInt);
    quantityText.push_back(tmpStr);
    if (mProductQuantity.isExists(matchKeyText2)) {
        tmpStr = getStringFromMatch(mProductQuantity, matchKeyText2);
        tmpInt = atoi(toUTF8(tmpStr).c_str());
        if (tmpInt <= 0)
            return false;
        quantity.push_back(tmpInt);
        quantityText.push_back(tmpStr);
    }
    if (mProductQuantity.isExists(matchKeyText3)) {
        tmpStr = getStringFromMatch(mProductQuantity, matchKeyText3);
        tmpInt = atoi(toUTF8(tmpStr).c_str());
        if (tmpInt <= 0)
            return false;
        quantity.push_back(tmpInt);
        quantityText.push_back(tmpStr);
    }
    
    // Price per item
    tmpStr = getStringFromMatch(mPricePerItem, matchKeyText);
    tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
    if (tmpInt <= 0)
        return false;
    priceperitem.push_back(tmpInt);
    priceperitemText.push_back(tmpStr);
    if (mPricePerItem.isExists(matchKeyText2)) {
        tmpStr = getStringFromMatch(mPricePerItem, matchKeyText2);
        tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        priceperitem.push_back(tmpInt);
        priceperitemText.push_back(tmpStr);
    }
    if (mPricePerItem.isExists(matchKeyText3)) {
        tmpStr = getStringFromMatch(mPricePerItem, matchKeyText3);
        tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        priceperitem.push_back(tmpInt);
        priceperitemText.push_back(tmpStr);
    }
    if (!currentIsProductPrice && (newText.length() > 0)) {
        tmpInt = round(atof(toUTF8(newText).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        priceperitem.push_back(tmpInt);
        priceperitemText.push_back(newText);
    }
    
    // Product price
    tmpStr = getStringFromMatch(mProductPrice, matchKeyText);
    tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
    if (tmpInt <= 0)
        return false;
    productprice.push_back(tmpInt);
    productpriceText.push_back(tmpStr);
    if (mProductPrice.isExists(matchKeyText2)) {
        tmpStr = getStringFromMatch(mProductPrice, matchKeyText2);
        tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        productprice.push_back(tmpInt);
        productpriceText.push_back(tmpStr);
    }
    if (mProductPrice.isExists(matchKeyText3)) {
        tmpStr = getStringFromMatch(mProductPrice, matchKeyText3);
        tmpInt = round(atof(toUTF8(tmpStr).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        productprice.push_back(tmpInt);
        productpriceText.push_back(tmpStr);
    }
    if (currentIsProductPrice && (newText.length() > 0)) {
        tmpInt = round(atof(toUTF8(newText).c_str()) * 100);
        if (tmpInt <= 0)
            return false;
        productprice.push_back(tmpInt);
        productpriceText.push_back(newText);
    }
    
    for (int i=0; i<quantity.size(); i++) {
        int quantityValue = quantity[i];
        for (int j=0; j<priceperitem.size(); j++) {
            int pricePerItemValue = priceperitem[j];
            for (int k=0; k<productprice.size(); k++) {
                int productPriceValue = productprice[k];
                if (quantityValue * pricePerItemValue == productPriceValue) {
                    // Found a match!
                    MergingLog("importNewResults: found perfect fit between quantity=%s, price-per-item=%s, total product price=%s", toUTF8(quantityText[i]).c_str(), toUTF8(priceperitemText[j]).c_str(), toUTF8(productpriceText[k]).c_str());
                    mProductQuantity[matchKeyText] = (SmartPtr<wstring> (new wstring(quantityText[i]))).castDown<EmptyType>();
                    mProductQuantity[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                    if (mProductQuantity.isExists(matchKeyText2)) {
                        mProductQuantity->erase(matchKeyText2);
                        mProductQuantity->erase(matchKeyConfidence2);
                        mProductQuantity->erase(matchKeyCountMatched2);
                    }
                    if (mProductQuantity.isExists(matchKeyText3)) {
                        mProductQuantity->erase(matchKeyText3);
                        mProductQuantity->erase(matchKeyConfidence3);
                        mProductQuantity->erase(matchKeyCountMatched3);
                    }
                    mPricePerItem[matchKeyText] = (SmartPtr<wstring> (new wstring(priceperitemText[j]))).castDown<EmptyType>();
                    mPricePerItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                    if (mPricePerItem.isExists(matchKeyText2)) {
                        mPricePerItem->erase(matchKeyText2);
                        mPricePerItem->erase(matchKeyConfidence2);
                        mPricePerItem->erase(matchKeyCountMatched2);
                    }
                    if (mPricePerItem.isExists(matchKeyText3)) {
                        mPricePerItem->erase(matchKeyText3);
                        mPricePerItem->erase(matchKeyConfidence3);
                        mPricePerItem->erase(matchKeyCountMatched3);
                    }
                    mProductPrice[matchKeyText] = (SmartPtr<wstring> (new wstring(productpriceText[k]))).castDown<EmptyType>();
                    mProductPrice[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                    if (mProductPrice.isExists(matchKeyText2)) {
                        mProductPrice->erase(matchKeyText2);
                        mProductPrice->erase(matchKeyConfidence2);
                        mProductPrice->erase(matchKeyCountMatched2);
                    }
                    if (mProductPrice.isExists(matchKeyText3)) {
                        mProductPrice->erase(matchKeyText3);
                        mProductPrice->erase(matchKeyConfidence3);
                        mProductPrice->erase(matchKeyCountMatched3);
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

#if DEBUG
wstring OCRResults::textForLineAtIndex(int lineIndex, MatchVector &matches) {
    if ((lineIndex < 0) || (lineIndex >= matches.size()))
        return wstring();
    int startLineIndex = OCRResults::indexFirstElementOnLineAtIndex(lineIndex, &matches);
    int endLineIndex = OCRResults::indexLastElementOnLineAtIndex(lineIndex, &matches);
    wstring result;
    for (int i=startLineIndex; i<=endLineIndex; i++) {
        if (result.length() > 0) result += L" ";
        result += getStringFromMatch(matches[i], matchKeyText);
    }
    return result;
}
#endif // DEBUG

bool OCRResults::deltaLineNumbersNewResultsToExistingResults(MatchVector &matches, int &deltaLines, retailer_t *retailerParams) {
    
    vector<matchedProduct> matchingOnceProducts;

    // First pass: make list of all products in new results for which there is a matching product (and only one) in existing results
    for (int i=0; i<matches.size(); i++) {
        MatchPtr newM = matches[i];
        OCRElementType tp = getElementTypeFromMatch(newM, matchKeyElementType);
        if ((tp != OCRElementTypeProductNumber) && (tp != OCRElementTypeSubtotal) && (tp != OCRElementTypePartialSubtotal)
            // For dates, skip if multiple dates are possible
            && ((tp != OCRElementTypeDate) || this->retailerParams.hasMultipleDates)
            && ((tp != OCRElementTypeTotalWithoutTax) && (tp != OCRElementTypeTotal))
            && (tp != OCRElementTypePhoneNumber)
            && (tp != OCRElementTypeMergeablePattern1))
            continue; // Can't match that product
        if (tp == OCRElementTypePartialSubtotal)
            tp = OCRElementTypeSubtotal;    // So that we treat both as equivalent
        if (tp == OCRElementTypeTotalWithoutTax)
            tp = OCRElementTypeTotal;    // So that we treat both as equivalent
        wstring newText = getStringFromMatch(newM, matchKeyText);
        // Check if found in new results more than once
        bool foundAgain = false;
        for (int j=0; j<matches.size(); j++) {
            if (j == i) continue;
            MatchPtr otherM = matches[j];
            OCRElementType otherTp = getElementTypeFromMatch(otherM, matchKeyElementType);
            if (otherTp == OCRElementTypePartialSubtotal)
                otherTp = OCRElementTypeSubtotal;
            if (otherTp == OCRElementTypeTotalWithoutTax)
                otherTp = OCRElementTypeTotal;
            if (otherTp != tp) continue;
            // Tolerate subtotal (because we will test the price value before declaring a match). We already skip dates if multiple dates.
            if (tp == OCRElementTypeProductNumber) {
                if (getStringFromMatch(otherM, matchKeyText) == newText) {
                    // Matching!
                    foundAgain = true;
                    break;
                }
            }
        }
        if (foundAgain)
            continue;
        
        matchedProduct matchedP;
        wstring newSubTotalPrice;
        if ((retailerParams->hasMultipleSubtotals) && (tp == OCRElementTypeSubtotal) && (i < matches.size()-1)) {
            MatchPtr priceMatch = matches[i+1];
            // Test type just in case, element following subtotal is expected to be a price (otherwise we wouldn't have tagged it a subtotal)
            if (getElementTypeFromMatch(priceMatch, matchKeyElementType) == OCRElementTypePrice)
                newSubTotalPrice = getStringFromMatch(matches[i+1], matchKeyText);
        }
            
        // Check if found in existing results (and only one time)
        int countFound = 0;
        for (int j=0; j<this->matchesAddressBook.size(); j++) {
            MatchPtr existingM = this->matchesAddressBook[j];
            OCRElementType tpExisting = getElementTypeFromMatch(existingM, matchKeyElementType);
            if (tpExisting == OCRElementTypePartialSubtotal)
                tpExisting = OCRElementTypeSubtotal;
            if (tpExisting == OCRElementTypeTotalWithoutTax)
                tpExisting = OCRElementTypeTotal;
            if (tpExisting != tp) continue;
            // For the date finding an item with same type is a match. For products need to match text. For subtotal next item must be a price and match.
            if ((tp == OCRElementTypeProductNumber) && (getStringFromMatch(existingM, matchKeyText) != newText))
                continue; // Not a match
            if ((tp == OCRElementTypeSubtotal) && (this->retailerParams.hasMultipleSubtotals)) {
                if (j == this->matchesAddressBook.size() - 1)
                    continue;
                MatchPtr existingPriceMatch = this->matchesAddressBook[j+1];
                if (getElementTypeFromMatch(existingPriceMatch, matchKeyElementType) != OCRElementTypePrice)
                    continue;
                if ((retailerParams->hasMultipleSubtotals) && (getStringFromMatch(existingPriceMatch, matchKeyText) != newSubTotalPrice))
                    continue;
            }
            if (tpExisting)
            if (++countFound > 1)
                break;  // We only look for items found once in existing results
            // Record this match
            matchedP.newLineNumber = getIntFromMatch(newM, matchKeyLine);
            matchedP.newIndex = i;
            matchedP.newText = getStringFromMatch(newM, matchKeyText);
            matchedP.existingLineNumber = getIntFromMatch(existingM, matchKeyLine);
            matchedP.existingIndex = j;
            matchedP.existingText = getStringFromMatch(existingM, matchKeyText);
            matchedP.tp = getElementTypeFromMatch(newM, matchKeyElementType);
        }
        if (countFound != 1)
            continue;
        
        //LayoutLog("deltaLineNumbersNewResultsToExistingResults: new line [%s] at line %d matches existing line [%s] at line %d (delta=%d)", toUTF8(textForLineAtIndex(matchedP.newIndex, matches)).c_str(), matchedP.newLineNumber, toUTF8(textForLineAtIndex(matchedP.existingIndex, this->matchesAddressBook)).c_str(), matchedP.existingLineNumber, matchedP.existingLineNumber - matchedP.newLineNumber);
        matchingOnceProducts.push_back(matchedP);
    } // iterate over new results

    LayoutLog("deltaLineNumbersNewResultsToExistingResults: found %d products matching once in existing results", (int)matchingOnceProducts.size());
    
    // Now make a list of all possible delta values and how many times each appears
    int numDeltaValues = 0;
    //deltaLinesValue *deltaValues = new deltaLinesValue[(int)matchingOnceProducts.size()];
    vector<deltaLinesValue> deltaValues;
    for (int i=0; i<matchingOnceProducts.size(); i++) {
        matchedProduct p = matchingOnceProducts[i];
        int delta = p.existingLineNumber - p.newLineNumber;
        bool found = false;
        for (int j=0; j<numDeltaValues; j++) {
            deltaLinesValue v = deltaValues[j];
            if (v.value == delta) {
                found = true;
                v.count++;
                v.matchingList.push_back(p);
                deltaValues[j] = v;
                break;
            }
        } // search for this delta value
        if (!found) {
            deltaLinesValue v;
            v.value = delta;
            v.count = 1;
            v.matchingList.push_back(p);
            deltaValues.push_back(v);
            numDeltaValues++;
        }
    }
    
    /* Special test: if we have one overwhelming candidate for a unique delta (list with a high number of items) and one or more lists with just one item, ignore the outliers and declare that we have a unique delta. This help with this Walmart receipt: https://drive.google.com/open?id=0B4jSQhcYsC9VUzNyZzFucDJjejA - scan first 6 frames, then 7th frame includes one product identical to another we already added so we have 10 products that say delta is 1024 and just one that says delta is 996 (negative, essentially) which should be ignored
    */
    if (numDeltaValues > 1) {
        // First step: find the list with the largest number of values
        int sizeLongestList = 0;
        int indexLongestList = -1;
        int deltaLongestList = -1;
        for (int i=0; i<numDeltaValues; i++) {
            deltaLinesValue v = deltaValues[i];
            if (v.matchingList.size() > sizeLongestList) {
                sizeLongestList = (int)v.matchingList.size();
                indexLongestList = i;
                deltaLongestList = v.value; // Delta indicated by that list
            }
        }
        bool assumeUniqueDelta = true;  // Until proven otherwise
        for (int i=0; i<numDeltaValues; i++) {
            if (i == indexLongestList) continue;
            deltaLinesValue v = deltaValues[i];
            // Ignore if assumed unique delta is > 1000 and current list is same or shorter lenght and its delta is < 1000 (which means user is scanning back up instead of down - unlikely)
            if ((v.matchingList.size() <=  sizeLongestList)
                && (deltaLongestList >= 1000)
                // 2016-05-01 prior we just tested that v.value < 1000 BUT if the new frame has a noise line just before a matching product, the delta WILL be less than 1000 because it would drive to lowering the line number to fit into existing results - yet it is not cause to ignore if if differs only by 1 or 2 with dominant delta value. Test on Receipts->Target->Bogus line + merge
                && (abs(v.value - deltaLongestList) > 3)) {
                    continue;   // which means OK to ignore
            }
            // Overwhelmingly more in the longest list?
            if ((v.matchingList.size() * 6 < deltaLongestList)
                // 2016-05-01 adding a condition that deltas be meaningfully different (see above comment).
                && (abs(v.value - deltaLongestList) > 3)) {
                continue;
            }
            assumeUniqueDelta = false;
            break;
        }
        if (assumeUniqueDelta) {
#if DEBUG
            LayoutLog("deltaLineNumbersNewResultsToExistingResults: found %d unique delta values but assuming only ONE matters (delta = %d)", numDeltaValues, deltaLongestList);
            for (int i=0; i<numDeltaValues; i++) {
                deltaLinesValue v = deltaValues[i];
                LayoutLog("deltaLineNumbersNewResultsToExistingResults: value #%d: %d (count = %d)", i, v.value, v.count);
                for (int j=0; j<v.matchingList.size(); j++) {
                    matchedProduct p = v.matchingList[j];
                    LayoutLog("  new line %d [text = %s] matches existing line %d [text =%s]", p.newLineNumber, toUTF8(p.newText).c_str(), p.existingLineNumber, toUTF8(p.existingText).c_str());
                }
            }
#endif
            deltaLines = deltaLongestList;
            return true;
        }
    }
    
    // Special test: if we have two unique delta values, one (or both of them) having only one item in the list, check if we can't change that item's delta to match the 2nd group but comparing its text to the text2 or text3 of existing items - see https://www.pivotaltracker.com/story/show/110799316
    if (numDeltaValues == 2) {
        for (int i=0; i<numDeltaValues; i++) {
            deltaLinesValue v = deltaValues[i];
            if (v.count == 1) {
                // Try to find an alternate match for this item (only for product numbers)!
                if (v.matchingList[0].tp != OCRElementTypeProductNumber)
                    continue;
                MatchPtr newMatch = matches[v.matchingList[0].newIndex];
                wstring newText = getStringFromMatch(newMatch, matchKeyText), newText2, newText3;
                if (newMatch.isExists(matchKeyText2))
                    newText2 = getStringFromMatch(newMatch, matchKeyText2);
                if (newMatch.isExists(matchKeyText3))
                    newText3 = getStringFromMatch(newMatch, matchKeyText3);
                deltaLinesValue otherV = deltaValues[(i==0)? 1:0];
                int desiredDeltaLine = otherV.value;
                int desiredExistingLineNumber = desiredDeltaLine + getIntFromMatch(newMatch, matchKeyLine);
                // Now search all existing items for one who can yield desiredDeltaLine (excluding items already in the otherV)
                for (int j=0; j<this->matchesAddressBook.size(); j++) {
                    // Check if that item is potentially helpful
                    MatchPtr existingM = this->matchesAddressBook[j];
                    // If it won't yield the desired delta, skip line
                    int existingLineNumber = getIntFromMatch(existingM, matchKeyLine);
                    if (existingLineNumber != desiredExistingLineNumber) {
                        // Skip line
                        j = OCRResults::indexLastElementOnLineAtIndex(j, &this->matchesAddressBook); continue;
                    }
                    OCRElementType tp = getElementTypeFromMatch(existingM, matchKeyElementType);
                    if (tp != OCRElementTypeProductNumber)
                        continue;
                    // Make sure j is not in otherV already
                    bool found = false;
                    for (int k=0; k<otherV.matchingList.size(); k++) {
                        int alreadyMatchedExistingIndex = otherV.matchingList[k].existingIndex;
                        if (j == alreadyMatchedExistingIndex) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        // Skip to the end of this line
                        j = OCRResults::indexLastElementOnLineAtIndex(j, &matches);
                        continue;
                    }
                    wstring existingText = getStringFromMatch(existingM, matchKeyText), existingText2, existingText3;
                    if (existingM.isExists(matchKeyText2))
                        existingText2 = getStringFromMatch(existingM, matchKeyText2);
                    if (existingM.isExists(matchKeyText3))
                        existingText3 = getStringFromMatch(existingM, matchKeyText3);
                    if (((existingText2.length() > 0) && (existingText2 == newText))
                        || ((existingText2.length() > 0) && (existingText2 == newText2))
                        || ((existingText2.length() > 0) && (existingText2 == newText3))
                        || ((existingText3.length() > 0) && (existingText3 == newText))
                        || ((existingText3.length() > 0) && (existingText3 == newText2))
                        || ((existingText3.length() > 0) && (existingText3 == newText3))
                        || ((newText2.length() > 0) && (newText2 == existingText))
                        || ((newText3.length() > 0) && (newText3 == existingText))) {
                            // Match!
                            LayoutLog("deltaLineNumbersNewResultsToExistingResults: MOVING new item [%s] away from matching existing item [%s] at existing line %d, found better match with existing item [%s] at existing line %d", toUTF8(newText).c_str(), toUTF8(getStringFromMatch(this->matchesAddressBook[v.matchingList[0].existingIndex], matchKeyText)).c_str(), v.matchingList[0].existingLineNumber, toUTF8(existingText).c_str(), existingLineNumber);
                            // Add this element to the otherV
                            matchedProduct p = v.matchingList[0];
                            p.existingLineNumber = existingLineNumber;
                            p.existingIndex = j;
                            p.existingText = existingText;
                            int otherListIndex = ((i==0)? 1:0);
                            vector<matchedProduct> tmpList = deltaValues[otherListIndex].matchingList;
                            tmpList.push_back(p);
                            deltaValues[otherListIndex].matchingList = tmpList;
                            deltaValues[otherListIndex].count++;
                            // No need to add this mapping anywhere, just kill this deltaValue variation
                            numDeltaValues = 1;
                            deltaValues.erase(deltaValues.begin() + i);
                        }
                    } // for j, search existing items
            }
        }
    } // if exactly 2 delta value
    
    LayoutLog("deltaLineNumbersNewResultsToExistingResults: found %d unique delta values", numDeltaValues);
#if DEBUG
    for (int i=0; i<numDeltaValues; i++) {
        deltaLinesValue v = deltaValues[i];
        LayoutLog("deltaLineNumbersNewResultsToExistingResults: value #%d: %d (count = %d)", i, v.value, v.count);
        for (int j=0; j<v.matchingList.size(); j++) {
            matchedProduct p = v.matchingList[j];
            LayoutLog("  new line %d [text = %s] matches existing line %d [text =%s]", p.newLineNumber, toUTF8(p.newText).c_str(), p.existingLineNumber, toUTF8(p.existingText).c_str());
        }
    }
#endif

    // If it's a retailer without product numbers, try matching based on prices (even if we also matched based in special items like margeable pattern 1, subtotal or total) - because sometimes matching based on products is better
    //if (retailerParams->noProductNumbers) {
        /* For these retailers above code only found matching lines for subtotal, total, date or phone number. Need to also try to match product lines using prices. Here is the strategy:
            - create an array of product prices in new results, e.g. "line 2 - $2.40, line 3 - $3.00, line 5 - $4.20"
            - create an array of product prices in existing results, e.g. "line 1000 - $0.99, line 1001 - $1.99, line 1002 - $2.40, line 1003 - $3.10, line 1005 - $4.20"
            - iterate from delta = 995 to 1005, check which delta creates the most price matching. Here the answer is 1000 (line 2 and line 5 match). This IS our unique delta
            - TODO: what if that unique delta conflicts with other markers (date, phone, subtotal, total)? Who wins?
            - TODO: receipts with adjacent products with same price may confuse us. Require prices to be different from one another?
        */
        vector<matchedProduct> newMatcheableElements;
        for (int j=1; j<matches.size(); j++) {
            MatchPtr m = matches[j];
            OCRElementType tp = getElementTypeFromMatch(m, matchKeyElementType);
            
            int itemLineNumber = getIntFromMatch(m, matchKeyLine);
            matchedProduct newProduct;
            
            if (retailerParams->noProductNumbers) {
                // For retailers without product numbers, match based on lines with product prices
                if (tp != OCRElementTypeProductPrice)
                    continue;
                int descriptionIndex = -1;
                MatchPtr descriptionItem;
                // Require a description (all product prices are supposed to have one)
                descriptionIndex = OCRResults::descriptionIndexForProductPriceAtIndex(j, matches, &this->retailerParams);
                if (descriptionIndex < 0) continue;
                descriptionItem = matches[descriptionIndex];
                if (getIntFromMatch(descriptionItem, matchKeyLine) != itemLineNumber) continue;

                newProduct.newIndex = j;
                newProduct.newText = getStringFromMatch(m, matchKeyText);
                newProduct.newPriceText = newProduct.newText;
                newProduct.newDescriptionText = getStringFromMatch(descriptionItem, matchKeyText);
                newProduct.newLineNumber = itemLineNumber;
            } else {
                // All others, match based on lines with product numbers
                if (tp != OCRElementTypeProductNumber)
                    continue;
                int priceLineNumber = -1;
                int priceIndex = OCRResults::priceIndexForProductAtIndex(j, matches, &priceLineNumber, &this->retailerParams);
                newProduct.newIndex = j;
                newProduct.newText = getStringFromMatch(m, matchKeyText);
                newProduct.newProductNumber = newProduct.newText;
                if (priceIndex >= 0)
                    newProduct.newPriceText = getStringFromMatch(matches[priceIndex], matchKeyText);
                newProduct.newLineNumber = itemLineNumber;
                int descriptionIndex = OCRResults::descriptionIndexForProductPriceAtIndex(j, matches, &this->retailerParams);
                if (descriptionIndex >= 0) {
                    newProduct.newDescriptionText = getStringFromMatch(matches[descriptionIndex], matchKeyText);
                }
            }
            
            newMatcheableElements.push_back(newProduct);
            
            j = OCRResults::indexLastElementOnLineAtIndex(j, &matches); continue;
        }
        
        vector<matchedProduct> existingMatcheableElements;
        for (int j=1; j<this->matchesAddressBook.size(); j++) {
            MatchPtr m = this->matchesAddressBook[j];
            OCRElementType tp = getElementTypeFromMatch(m, matchKeyElementType);
            
            int itemLineNumber = getIntFromMatch(m, matchKeyLine);
            matchedProduct newProduct;
            
            if (retailerParams->noProductNumbers) {
                // For retailers without product numbers, match based on lines with product prices
                if (tp != OCRElementTypeProductPrice)
                    continue;
                int descriptionIndex = -1;
                MatchPtr descriptionItem;
                // Require a description (all product prices are supposed to have one)
                descriptionIndex = OCRResults::descriptionIndexForProductPriceAtIndex(j, this->matchesAddressBook, &this->retailerParams);
                if (descriptionIndex < 0) continue;
                descriptionItem = this->matchesAddressBook[descriptionIndex];
                if (getIntFromMatch(descriptionItem, matchKeyLine) != itemLineNumber) continue;

                newProduct.existingIndex = j;
                newProduct.existingText = getStringFromMatch(m, matchKeyText);
                newProduct.existingPriceText = newProduct.newText;
                newProduct.existingDescriptionText = getStringFromMatch(descriptionItem, matchKeyText);
                newProduct.existingLineNumber = itemLineNumber;
            } else {
                // All others, match based on lines with product numbers
                if (tp != OCRElementTypeProductNumber)
                    continue;
                int priceLineNumber = -1;
                int priceIndex = OCRResults::priceIndexForProductAtIndex(j, this->matchesAddressBook, &priceLineNumber, &this->retailerParams);
                newProduct.existingIndex = j;
                newProduct.existingText = getStringFromMatch(m, matchKeyText);
                newProduct.existingProductNumber = newProduct.existingText;
                if (priceIndex >= 0)
                    newProduct.existingPriceText = getStringFromMatch(this->matchesAddressBook[priceIndex], matchKeyText);
                newProduct.existingLineNumber = itemLineNumber;
                int descriptionIndex = OCRResults::descriptionIndexForProductPriceAtIndex(j, this->matchesAddressBook, &this->retailerParams);
                if (descriptionIndex >= 0) {
                    newProduct.existingDescriptionText = getStringFromMatch(this->matchesAddressBook[descriptionIndex], matchKeyText);
                }
            }
            
            existingMatcheableElements.push_back(newProduct);
            
            j = OCRResults::indexLastElementOnLineAtIndex(j, &this->matchesAddressBook); continue;
        }
        
        // Quick check: if all existing prices fall between the line numbers of special items contributing to a unique delta above, go with the special items, because such a bracket must be reliable
        if ((numDeltaValues == 1) && (deltaValues[0].count >= 2) && (existingMatcheableElements.size()>0)) {
            int lowestSpecialLine = deltaValues[0].matchingList[0].existingLineNumber;
            int highestSpecialLine = deltaValues[0].matchingList[(int)deltaValues[0].matchingList.size()-1].existingLineNumber;
            int lowestExistingPriceLine = existingMatcheableElements[0].existingLineNumber;
            int highestExistingPriceLine = existingMatcheableElements[(int)existingMatcheableElements.size()-1].existingLineNumber;
            if ((lowestExistingPriceLine > lowestSpecialLine) && (highestExistingPriceLine < highestSpecialLine)) {
                deltaLines = deltaValues[0].value;
                LayoutLog("deltaLineNumbersNewResultsToExistingResults: returning delta %d based on special items (lines range [%d - %d] rather than using products/prices in range [%d - %d] (frame %d)", deltaLines, lowestSpecialLine, highestSpecialLine, lowestExistingPriceLine, highestExistingPriceLine, this->frameNumber);
                return true;
            }
        }
        
        if ((newMatcheableElements.size() >= 1) && (existingMatcheableElements.size() >= 1)) {
            int lowestDelta = existingMatcheableElements[0].existingLineNumber - newMatcheableElements[(int)newMatcheableElements.size()-1].newLineNumber;
            int highestDelta = existingMatcheableElements[(int)existingMatcheableElements.size()-1].existingLineNumber - newMatcheableElements[0].newLineNumber;
            
            // Now look for the delta that scores best in terms of overlap with new prices
            int bestDelta = 0;
            int bestScore = -1000;
            int bestPenalties = 0;  // Counts the number of mismatching info in proposed overlapping range. This is important because when it comes time to declare a match based on best score we want to disqualify overlaps with a relatively high score but also a high penalties score - just indicate frames with many products (more chances to match)
            int bestLastMatchingNewIndex = -1, bestLastMatchingExistingIndex = -1;
            for (int delta=highestDelta; delta>=lowestDelta; delta--) {
                int currentScore = 0; int currentPenalties = 0;
                int currentLastMatchingNewIndex = -1, currentLastMatchingExistingIndex = -1;
                for (int k=0; k<newMatcheableElements.size(); k++) {
                    matchedProduct newP = newMatcheableElements[k];
                    int targetExistingLineNumber = newP.newLineNumber + delta;
                    // Find the existing price at that existing line
                    for (int m=0; m<existingMatcheableElements.size(); m++) {
                        matchedProduct existingP = existingMatcheableElements[m];
                        if (existingP.existingLineNumber == targetExistingLineNumber) {
                            if (retailerParams->noProductNumbers) {
                                if (OCRAwarePriceCompare(existingP.existingText,newP.newText)) {
                                    currentLastMatchingNewIndex = k;
                                    currentLastMatchingExistingIndex = m;
                                    currentScore++;
                                    // Also compare descriptions: if they are a possible match, increment score
                                    if (fuzzyCompare(existingP.existingDescriptionText, newP.newDescriptionText))
                                        currentScore++;
                                    // Check if there is an empty line just above/below in both cases
                                    // Above
                                    // Existing
                                    int indexFirstItemOnPriceLineExisting = OCRResults::indexFirstElementOnLineAtIndex(existingP.existingIndex, &this->matchesAddressBook);
                                    int indexLastElementOnPreviousLineExisting = ((indexFirstItemOnPriceLineExisting>0)? indexFirstItemOnPriceLineExisting-1:-1);
                                    int lineNumberPreviousLineExisting = ((indexLastElementOnPreviousLineExisting>=0)? getIntFromMatch(this->matchesAddressBook[indexLastElementOnPreviousLineExisting], matchKeyLine):-1);
                                    // New
                                    int indexFirstItemOnPriceLineNew = OCRResults::indexFirstElementOnLineAtIndex(newP.newIndex, &matches);
                                    int indexLastElementOnPreviousLineNew = ((indexFirstItemOnPriceLineNew>0)? indexFirstItemOnPriceLineNew-1:-1);
                                    int lineNumberPreviousLineNew = ((indexLastElementOnPreviousLineNew>=0)? getIntFromMatch(matches[indexLastElementOnPreviousLineNew], matchKeyLine)+delta:-1);
                                    // Below
                                    // Existing
                                    int indexLastItemOnPriceLineExisting = OCRResults::indexLastElementOnLineAtIndex(existingP.existingIndex, &this->matchesAddressBook);
                                    int indexFirstElementOnNextLineExisting = ((indexLastItemOnPriceLineExisting<this->matchesAddressBook.size())? indexLastItemOnPriceLineExisting+1:-1);
                                    int lineNumberNextLineExisting = ((indexFirstElementOnNextLineExisting<this->matchesAddressBook.size())? getIntFromMatch(this->matchesAddressBook[indexFirstElementOnNextLineExisting], matchKeyLine):-1);
                                    // New
                                    int indexLastItemOnPriceLineNew = OCRResults::indexLastElementOnLineAtIndex(newP.newIndex, &matches);
                                    int indexFirstElementOnNextLineNew = ((indexLastItemOnPriceLineNew<matches.size())? indexLastItemOnPriceLineNew+1:-1);
                                    int lineNumberNextLineNew = ((indexFirstElementOnNextLineNew<matches.size())? getIntFromMatch(matches[indexFirstElementOnNextLineNew], matchKeyLine)+delta:-1);
                                    // Increment score if we found an empty line above or below on both frames, or if there is no line above/below on both frames
                                    if (((lineNumberPreviousLineNew == lineNumberPreviousLineExisting)
                                            && ((lineNumberPreviousLineExisting == -1) || ((lineNumberPreviousLineExisting != -1) && (lineNumberPreviousLineExisting != targetExistingLineNumber-1))))
                                        || ((lineNumberNextLineNew == lineNumberNextLineExisting)
                                            && ((lineNumberNextLineExisting == -1) || ((lineNumberNextLineExisting >= 0) && (lineNumberNextLineExisting != targetExistingLineNumber + 1)))))
                                        currentScore++;
                                } // Prices match (allowing for OCR errors)
                            } else {
                                // Retailers with product numbers
                                int addToScore = 0;
                                bool productsMatch = false;
                                
                                // Compare products
                                if ((existingP.existingProductNumber.length() > 0) && (newP.newProductNumber.length() > 0) && existingP.existingProductNumber == newP.newProductNumber) {
                                    addToScore += 2;
                                    productsMatch = true;
                                } else if ((existingP.existingProductNumber.length() > 0) || (newP.newProductNumber.length() > 0)) {
                                    float p = OCRAwareProductNumberCompare(existingP.existingProductNumber, newP.newProductNumber);
                                    // e.g. 9 out of 12
                                    if (p > 0.74) {
                                        addToScore++;
                                        productsMatch = true;
                                    } else if (p < 0.21)
                                        currentPenalties++;
                                }
                                
                                if (productsMatch > 0) {
                                    // Compare descriptions only when product numbers match
                                    if ((existingP.existingDescriptionText.length() > 0) && (newP.newDescriptionText.length() > 0)) {
                                        float p = OCRAwareTextCompare(existingP.existingDescriptionText, newP.newDescriptionText);
                                        if (p > 0.79) {
                                            LayoutLog("deltaLineNumbersNewResultsToExistingResults: increasing score based on desc [%s] ~= [%s] for product [%s]", toUTF8(newP.newDescriptionText).c_str(), toUTF8(existingP.existingDescriptionText).c_str(), toUTF8(newP.newProductNumber).c_str());
                                            addToScore++;
                                        }
                                    }
                                }
                                
                                // Now compare prices
                                if (existingP.existingPriceText == newP.newPriceText) {
                                    if (productsMatch)
                                        addToScore += 2;  // Exact price match, amplified because products matched
                                    else
                                        addToScore++;
                                } else if (OCRAwarePriceCompare(existingP.existingPriceText,newP.newPriceText)) {
                                    if (productsMatch)
                                        addToScore++;     // Approximate price match (credits only if products matched)
                                } else if ((existingP.existingPriceText.length() > 0) && (newP.newPriceText.length() > 0))
                                    currentPenalties++; // Prices exist yet don't match - penalize
                                
                                currentScore += addToScore;
                            }
                            break;
                        } // Line number aligned between new & existing matches
                        if (existingP.existingLineNumber > targetExistingLineNumber) {
                            // Failed to find a price at targetExistingLineNumber
                            currentPenalties++; break;
                        }
                    } // try to find price in target existing line
                } // apply delta to all new prices
                if (currentScore - currentPenalties > bestScore) {
                    bestScore = currentScore - currentPenalties;
                    bestPenalties = currentPenalties;
                    bestDelta = delta;
                    bestLastMatchingNewIndex = currentLastMatchingNewIndex;
                    bestLastMatchingExistingIndex = currentLastMatchingExistingIndex;
                }
            } // try all deltas
            
            // Adopt this method if we got 3 prices to match (or 2 + descr matched). NOTE: even if we got a unique value based on special items - this is more reliable because of potential skew of where we placed the subtotal line, see https://drive.google.com/open?id=0B4jSQhcYsC9VcHhtS0hoNTVOVWs
            if ((bestScore >= 3)
                    // Only one product matches up, but still OK if it's the last one in existing set and the first one (or only one) in new set
                    || ((bestScore > 0)
                        && (highestDelta - bestDelta < 1))) {
                deltaLines = bestDelta;
                LayoutLog("deltaLineNumbersNewResultsToExistingResults: delta based on prices = %d based on score = %d (penalties=%d) for products/prices matching (frame %d)", deltaLines, bestScore, bestPenalties, this->frameNumber);
                return true;
            } else {
                LayoutLog("deltaLineNumbersNewResultsToExistingResults: failing delta based on prices = %d based on score = %d (penalties=%d) for products/prices matching (frame %d)", bestDelta, bestScore, bestPenalties, this->frameNumber);
            }
        } // we have at least 1 prices
    //} // retailer has no product numbers

    if ((numDeltaValues == 1)
        && ((deltaValues[0].count >= 2)
            // When no product numbers, don't be picky! Accept even if we were only able to find one match (e.g. the phone number)
            || ((deltaValues[0].count >= 1) && retailerParams->noProductNumbers))) {
        deltaLines = deltaValues[0].value;
        LayoutLog("deltaLineNumbersNewResultsToExistingResults: found unique delta=%d (frame %d)", deltaValues[0].value, this->frameNumber);
        return true;
    } else {
        LayoutLog("deltaLineNumbersNewResultsToExistingResults: failed to find unique delta (frame %d)", this->frameNumber);
        return false;
    }
}

/* Imports a set of scan results into the set of results we already have. While doing so, sets the newInfoAdded iVar to TRUE as soon as it updates the text of an existing item or creates a new item
 */
bool OCRResults::importNewResults (OCRResults *newResults) {

    MergingLog("importNewResults: checking frame %d", frameNumber+1);
    
//#if DEBUG
//    if (frameNumber == 5) {
//        DebugLog("");
//    }
//#endif
    
    MatchVector matches = newResults->matchesAddressBook;
    
    bool gotUniqueDelta = false;
    int deltaLines;
    
    gotUniqueDelta = deltaLineNumbersNewResultsToExistingResults(matches, deltaLines, &this->retailerParams);

    currentLowestMatchingProductLineNumber = 20000;
    // First step is to identify identical products in the new set: this will tell us what kind of overlap exists between the new scanned frame and the existing frame
    int deltaToAddToNewLines = 0;    // Increment by which to adjust lines in the new results to match the lines in existing results. For example if we scanned 20 products so far and last two products are one lines 34 and 35, if we have the same two products in the new results at line 0 and 1, the delta is 34
    float sumDeltas = 0;
    int numDeltas = 0;
    int minDelta = 10000, maxDelta = -10000;
    int deltaVariance = 0;          // Different than 0 if we found several matching products, but with a different delta!
    vector<int> matchedIndexes; // Indexes already associated with a new product, so that the same index is not used twice

// Rethinking the entire code section below: it was relevant only when not finding a "unique delta" just prior, and its purpose was always to try to find matching products between new and existing and, if so, mark these pairs as needing to merge. But does this make sense if the products following do not match? It will only cause massive overwriting of one set of info with a completely different set of info. It seems we must find a "unique delta" or else just append the new frame without any merging!
#if 0
    // Try finding an overlap twice: 2nd time we will allow product numbers to differ by one digit, to account for a possible OCR issue - if new set only has 1 or 2 products (otherwise unlikely they are all dups yet all different numbers)
    for (int matchAttempt=0; matchAttempt<2; matchAttempt++) {
    
        for (int i=0; i<matches.size(); i++) {
            MatchPtr d = matches[i];
            OCRElementType tpNew = getElementTypeFromMatch(d, matchKeyElementType);
            if (tpNew == OCRElementTypePartialSubtotal)
                tpNew = OCRElementTypeSubtotal;
            if (tpNew == OCRElementTypeTotalWithoutTax)
                tpNew = OCRElementTypeTotal;
    //#if PQ_MULTI_QUANTITIES_TEST
    //        if ((tp == OCRElementTypeProductQuantity) && (getStringFromMatch(d, matchKeyText) == L"4") && (frameNumber == 2)) {
    //            MatchPtr dd = matches[i+1];
    //            dd[matchKeyConfidence] = (SmartPtr<float> (new float(99.9))).castDown<EmptyType>();
    //        }
    //#endif
            if ((tpNew != OCRElementTypeProductNumber) && (tpNew != OCRElementTypeSubtotal) && (tpNew != OCRElementTypeTotal) && (this->retailerParams.hasMultipleDates || (tpNew != OCRElementTypeDate)))
                continue;
            // Try to find this product in the existing set
            wstring newProduct = getStringFromMatch(d, matchKeyText);
            wstring newSubTotalPrice;
            if ((this->retailerParams.hasMultipleSubtotals) && ((tpNew == OCRElementTypeSubtotal) || (tpNew == OCRElementTypeTotal)) && (i < matches.size() - 1)) {
                MatchPtr priceMatch = matches[i+1];
                if (getElementTypeFromMatch(priceMatch, matchKeyElementType) == OCRElementTypePrice) {
                    newSubTotalPrice = getStringFromMatch(priceMatch, matchKeyText);
                }
            }
    #if DEBUG
            if (newProduct == L"212030384") {
                DebugLog("");
            }
    #endif
            int newLineNumber = getIntFromMatch(d, matchKeyLine);
            // A product may appear twice: pick the one whose distance is closest to the average delta we have so far! TODO: this is obviously risky if the multiple product is first in the frame, i.e. no average delta just yet. Also, what about situations where we have product A in the first frame, then again several frames later: it would be very wrong to stich the new frame with the first frame in that case, need to actually just add as 2nd product. Possible solution: give priority to overlapping products with a smaller (and positive) delta and assume thus that the 2nd product is indeed a new entry?
            int bestExistingLineNumber = -1;
            int bestDelta = 0;
            int bestMatchingExistingIndex = -1;
            for (int j=0; j<matchesAddressBook.size(); j++) {
                // Ignore if already matched to a product!
                bool found = false;
                for (int jj=0; jj<matchedIndexes.size(); jj++) {
                    if (matchedIndexes[jj] == j) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;
                MatchPtr dd = matchesAddressBook[j];
                OCRElementType tptp = getElementTypeFromMatch(dd, matchKeyElementType);
                if (tptp == OCRElementTypePartialSubtotal)
                    tptp = OCRElementTypeSubtotal;
                if (tptp == OCRElementTypeTotalWithoutTax)
                    tptp = OCRElementTypeTotal;
                if (tptp != tpNew)
                    continue;
                wstring existingProduct = getStringFromMatch(dd, matchKeyText);
                bool matching = false;
                if (((tpNew == OCRElementTypeSubtotal) || (tpNew == OCRElementTypeTotal)) && (j < matchesAddressBook.size() - 1)) {
                    MatchPtr priceMatch = matchesAddressBook[j+1];
                    if (getElementTypeFromMatch(priceMatch, matchKeyElementType) == OCRElementTypePrice) {
                        if (!this->retailerParams.hasMultipleSubtotals || (tpNew == OCRElementTypeTotal))
                            matching = true;
                        else {
                            wstring existingSubTotalPrice = getStringFromMatch(priceMatch, matchKeyText);
                            if (existingSubTotalPrice == newSubTotalPrice)
                                matching = true;
                        }
                    }
                } else if (tpNew == OCRElementTypeDate) {
                    matching = true;
                } else if (tpNew == OCRElementTypeProductNumber) {
                    if (matchAttempt == 0) {
                        if (existingProduct == newProduct)
                            matching = true;
                    } else {
                        // Accept up to 2 discrepancies compared to existing text
                        if (countDiscrepancies(existingProduct, newProduct) <= 1) {
                            matching = true;
                        } else {
                            // Or 1 discrepancy comparing alternative texts
                            vector<wstring> list1; list1.push_back(existingProduct);
                            vector<wstring> list2; list2.push_back(newProduct);
                            if (dd.isExists(matchKeyText2)) list1.push_back(getStringFromMatch(dd, matchKeyText2));
                            if (dd.isExists(matchKeyText3)) list1.push_back(getStringFromMatch(dd, matchKeyText3));
                            if (d.isExists(matchKeyText2)) list2.push_back(getStringFromMatch(d, matchKeyText2));
                            if (d.isExists(matchKeyText3)) list2.push_back(getStringFromMatch(d, matchKeyText3));
                            if (countDiscrepancies(list1, list2) <= 1)
                                matching = true;
                        }
                    }
                }
                if (matching) {
                    int existingLineNumber = getIntFromMatch(dd, matchKeyLine);
                    // TODO all deltas should be in the direction, i.e. usually positive (user moving down the receipt) or negative (user moving back up) but not both positive AND negative. Test for this situation and advise.
                    int currentDelta = existingLineNumber - newLineNumber;
                    if (bestExistingLineNumber < 0) {
                        bestExistingLineNumber = existingLineNumber;
                        bestDelta = currentDelta;
                        bestMatchingExistingIndex = j;
                    } else {
                        // Already got a match, see if this one is better based on delta closer to the average we have so far. TODO: what if the double product situation occurs right at the start of the frame? No delta average yet.
                        if (numDeltas > 0) {
                            float averageDelta = sumDeltas / numDeltas;
                            if (abs(currentDelta - averageDelta) < abs(bestDelta - averageDelta)) {
                                bestExistingLineNumber = existingLineNumber;
                                bestDelta = currentDelta;
                                bestMatchingExistingIndex = j;
                            } else if (abs(currentDelta - averageDelta) == abs(bestDelta - averageDelta)) {
                                // Update existing choice if the new one is a positive difference and old one was negative - because typical scanning is such that the delta is positive
                                if ((currentDelta - averageDelta > 0) && (bestDelta - averageDelta < 0)) {
                                    bestExistingLineNumber = existingLineNumber;
                                    bestDelta = currentDelta;
                                    bestMatchingExistingIndex = j;
                                }
                            }
                            
                        }
                        continue;
                    }
                } // found a matching product
            } // Try to find matching product in existing results
            if (bestExistingLineNumber >= 0) {
                sumDeltas += bestDelta;
                numDeltas++;
                if (bestDelta < minDelta)
                    minDelta = bestDelta;
                if (bestDelta > maxDelta)
                    maxDelta = bestDelta;
                // Remember mapping in new item
                d[matchKeyGlobalLine] = SmartPtr<int>(new int(bestExistingLineNumber)).castDown<EmptyType>();
                if (bestExistingLineNumber < currentLowestMatchingProductLineNumber)
                    currentLowestMatchingProductLineNumber = bestExistingLineNumber;
                matchedIndexes.push_back(bestMatchingExistingIndex);
            }
        }
        
#if DEBUG
        if ((matchAttempt == 1) && (numDeltas > 0)) {
            MergingLog("importNewResults: WARNING, merging using product numbers mismatching by 1");
        }
#endif        
        // We don't try the relaxed merging when new frame has more than 2 products because if that's the case, we would expect at least one of them to match using exact comparison. That test was added to prevent excessive merging between 2 frames with many products where one of them happens to be present in both (yet these are distinct frames).
        // TODO add test that existing results have many products too, otherwise 
        if ((numDeltas > 0) || (newResults->numProducts > 2)) {
            break;  // No need to try again with relaxed product numbers comparing
        }
    } // two rounds trying to merge
#endif // 0
    
//    // Need to handle situations where we have a NEGATIVE delta somewhere. This means that if we proceed we would end up with a negative adjusted line number. Need to adjust all line numbers in existing results to add that delta, plus adjust the delta stats (min/max/sumdeltas) as well as the global line number properties (when set)
//    if (minDelta < 0) {
//        // Adjust line number in existing matches
//        for (int i=(int)matchesAddressBook.size()-1; i>=0; i--) {
//            MatchPtr d = matchesAddressBook[i];
//            int newLine = getIntFromMatch(d, matchKeyLine) - minDelta;
//            d[matchKeyLine] = SmartPtr<int>(new int(newLine)).castDown<EmptyType>();
//        }
//        // Adjust global line numbers in
//    }

    
    // Get the highest existing line number. All additions will have line number set to lastExistingLineNumber + 1 + new line number
    int lastExistingLineNumber = 0;
    for (int i=(int)matchesAddressBook.size()-1; i>=0; i--) {
        MatchPtr d = matchesAddressBook[i];
        int line = getIntFromMatch(d, matchKeyLine);
        if (line > lastExistingLineNumber) {
            lastExistingLineNumber = line;
            MergingLog("importNewResults: lastExistingLineNumber=%d", lastExistingLineNumber);
            break;
        }
    }
    
    if (numDeltas == 1) {
        int deltaToAddToNewLines = sumDeltas / numDeltas;
        DebugLog("");
        // Suspicious, could indicate a duplicate product found on a frame without any overlap and it would be dangerous to force a merge on that basis, see https://www.pivotaltracker.com/n/projects/1118860/stories/110343774
        // Check all other products and figure out where they would get merged on that assumption. If any of these gets merged onto a line with a completely different product number, or onto a quantity line without a product number, ignore that so-called matching product and merged as a new frame without overlap
        int indexMatchingProduct = -1;
        bool abort = false;
        for (int i=0; i<matches.size(); i++) {
            int indexLineStart = i;
            int indexLineEnd = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
            int productIndex = OCRResults::elementWithTypeInlineAtIndex(indexLineStart, OCRElementTypeProductNumber, &matches, &this->retailerParams);
            if (productIndex < 0) {
                i = indexLineEnd;
                continue;
            }
            MatchPtr productMatch = matches[productIndex];
            if (productMatch.isExists(matchKeyGlobalLine)) {
                // Remember its index so we can unmap its matchKeyGlobalLine if deemed bogus as the result of this loop
                indexMatchingProduct = productIndex;
                continue;   // No need to test the matching product itself
            }
            int newLineNumber = getIntFromMatch(productMatch, matchKeyLine);
            int targetExistingLineNumber = newLineNumber + deltaToAddToNewLines;
            // If we are already past the last existing line, we are done testing, all good (no conflicts can target new lines)
            if (targetExistingLineNumber > lastExistingLineNumber)
                break;
            
            int insertionIndex = indexFirstElementForLine(targetExistingLineNumber, &this->matchesAddressBook);
            // Is the target line a quantity line?
            int indexExistingQuantity = OCRResults::elementWithTypeInlineAtIndex(insertionIndex, OCRElementTypeProductQuantity, &this->matchesAddressBook, &this->retailerParams);
            if (indexExistingQuantity > 0) {
                // Verify no product on this line
                int indexExistingProduct = OCRResults::elementWithTypeInlineAtIndex(insertionIndex, OCRElementTypeProductNumber, &this->matchesAddressBook, &this->retailerParams);
                if (indexExistingProduct < 0) {
                    DebugLog("importNewProduct: single matching product, suspect merge of product [%s] at line %d into existing line %d with quantity [%s]", toUTF8(getStringFromMatch(productMatch, matchKeyText)).c_str(), newLineNumber, targetExistingLineNumber, toUTF8(getStringFromMatch(this->matchesAddressBook[indexExistingQuantity], matchKeyText)).c_str());
                    abort = true;
                    break;
                }
            }
            int indexExistingProduct = OCRResults::elementWithTypeInlineAtIndex(insertionIndex, OCRElementTypeProductNumber, &this->matchesAddressBook, &this->retailerParams);
            if (indexExistingProduct >= 0) {
                // Before declaring a problem, check if that product may differ from the new product by at most 2 digits - if so, could just be OCR variations
                wstring newProduct = getStringFromMatch(productMatch, matchKeyText);
                wstring existingProduct = getStringFromMatch(this->matchesAddressBook[indexExistingProduct], matchKeyText);
                if (newProduct.length() == existingProduct.length()) {
                    int numMismatches = 0;
                    for (int j=0; j<newProduct.length(); j++) {
                        if (newProduct[j] != existingProduct[j]) {
                            if (++numMismatches > 2)
                                break;
                        }
                    }
                    if (numMismatches > 2) {
                        DebugLog("importNewProduct: single matching product, suspect merge of product [%s] at line %d into existing product [%s] at line %d", toUTF8(newProduct).c_str(), newLineNumber, toUTF8(existingProduct).c_str(), targetExistingLineNumber);
                    abort = true;
                    break;
                    }
                }
            }
        } // Check all new matches insertions
        if (abort) {
            if (indexMatchingProduct >= 0) {
                MatchPtr d = matches[indexMatchingProduct];
                d->erase(matchKeyGlobalLine);
            }
            numDeltas = 0;
        }
    }
    
    if ((numDeltas > 0) || gotUniqueDelta) {
        if (gotUniqueDelta) {
            deltaVariance = 0;
            deltaToAddToNewLines = deltaLines;
        } else {
            deltaVariance = maxDelta - minDelta;
            if (deltaVariance == 0)
                deltaToAddToNewLines = sumDeltas / numDeltas;
        }
        // Add all new elements one by one (except products we already got)
        int closestMatchingProductNewLine = -1, closestMatchingProductExistingLine = -1;
        for (int i=0; i<matches.size(); i++) {
            // Before doing anything with that line, check it has at least one interesting element
            if (!isMeaningfulLineAtIndex(i, newResults)) {
                int indexNewEndLine = indexLastElementOnLineAtIndex(i, &matches);
                MergingLog("importNewResults: skipping new line with %d items [%s]-[%s] - no interesting elements", indexNewEndLine - i + 1, toUTF8(getStringFromMatch(matches[i], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[indexNewEndLine], matchKeyText)).c_str());
                // Done with current line in new matches, move to start of the next line
                i = indexNewEndLine;
                continue;
            }
        
            MatchPtr d = matches[i];
            int targetExistingLineNumber = -1;
            int currentNewLineNumber = getIntFromMatch(d, matchKeyLine);
//#if DEBUG
//            wstring tmpItemText = getStringFromMatch(d, matchKeyText);
//            if ((tmpItemText == L"3 1 $1.25 oE3") || (currentNewLineNumber == 6)) {
//                DebugLog("");
//            }
//#endif
            if (gotUniqueDelta) {
                targetExistingLineNumber = currentNewLineNumber + deltaLines;
            } else {
                // Is there a pre-established matching between this new line and existing lines?
                int globalLine = globalLineForLineAtIndex(i, matches);
                if (globalLine >= 0) {
                    // Remember this matching product for the sake of new products later (figuring out lines correspondence)
                    closestMatchingProductNewLine = getIntFromMatch(d, matchKeyLine);
                    targetExistingLineNumber = closestMatchingProductExistingLine = globalLine;
                }
            }

            int indexNewEndLine = indexLastElementOnLineAtIndex(i, &matches); // We will need it when copying all elements of new lines + to skip to the next line for the next loop
            // Determine where to insert/update
            // First step is to determine what's the expected line number in existing matches - look for closest matching product
            if (targetExistingLineNumber < 0) {
                // Is the closest new line for which there is a corresponding existing line BEFORE current new line (or just initialized to -1 as is the case when processing the first item of the new results)?
                if (closestMatchingProductNewLine < currentNewLineNumber) {
                    // Take a look ahead, in case there is a matching product ahead closer to current
                    for (int k=i+1; k<matches.size(); k++) {
                        MatchPtr dd = matches[k];
                        if (dd.isExists(matchKeyGlobalLine)) {
                            int nextMatchingProductNewLine = getIntFromMatch(dd, matchKeyLine);
                            if ((closestMatchingProductNewLine < 0) || (nextMatchingProductNewLine - currentNewLineNumber < currentNewLineNumber - closestMatchingProductNewLine)) {
                                // Closer! That's our new closest
                                closestMatchingProductNewLine = nextMatchingProductNewLine;
                                closestMatchingProductExistingLine = getIntFromMatch(dd, matchKeyGlobalLine);
                            }
                            break;  // In any case, stop looking forward - this match is the closest to current line with globalLine set
                        }
                    }
                }
                // Note that the below always sets targetExistingLineNumber to a meaninful number because the above must set closestMatchingProductExistingLine and closestMatchingProductNewLine, since we are in the scenario where at least one product matched between new and existing. That value is always positive too because existing results have a base line number of 1,000 while new results start at line = 0 so even if say first line of new results matches 3 lines before the end of the existing set, the difference will still be say 1017 - 1 = 1016
                targetExistingLineNumber = currentNewLineNumber + (closestMatchingProductExistingLine - closestMatchingProductNewLine);
            }
            MergingLog("importNewResults: preparing to insert [%s] / line %d at existing line %d", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), currentNewLineNumber, targetExistingLineNumber);
            /* Replace all items in existing at line targetExistingLineNumber with the items in new results at line currentNewLineNumber - unless a type we have in existing is not found in new, e.g. if existing has a price but new doesn't, keep the existing price. This helps when existing got the price (but not the product) and new got the product (but not the price)
             */
             // Find index in existing where we are inserting
             int insertionIndex = indexFirstElementForLine(targetExistingLineNumber, &matchesAddressBook);
            
             // Special case: if target existing line exists, and has only unknown items, while new line has typed items, nuke existing line and just adopt the entire new line. This helps when existing results missed a price per item (in a quantity pair) while the new set of results did: we want to get the right (trimmed) unknown text to the left of the quantity price.
                          // For lines beyond existing lines targetExistingLineNumber won't exist so insertionIndex will be set to -1
             if (insertionIndex >= 0) {
                int indexFirstItemOnExistingLine = insertionIndex;
                int indexLastItemOnExistingLine = indexLastElementOnLineAtIndex(insertionIndex, &matchesAddressBook);
                int indexFirstItemOnNewLine = i;
                int indexLastItemOnNewLine = indexLastElementOnLineAtIndex(i, &matches);
                bool existingOnlyUnknown = true;
                for (int n=indexFirstItemOnExistingLine; n<=indexLastItemOnExistingLine; n++) {
                    MatchPtr dd = matchesAddressBook[n];
                    if (getElementTypeFromMatch(dd, matchKeyElementType) != OCRElementTypeUnknown) {
                        existingOnlyUnknown = false;
                        break;
                    }
                }
                if (existingOnlyUnknown) {
                    bool newOnlyUnknown = true;
                    bool newHasPrice = false;
                    bool newHasSubTotalOrTotal = false;
                    for (int n=indexFirstItemOnNewLine; n<=indexLastItemOnNewLine; n++) {
                        MatchPtr dd = newResults->matchesAddressBook[n];
                        OCRElementType tptp = getElementTypeFromMatch(dd, matchKeyElementType);
                        if (tptp != OCRElementTypeUnknown) {
                            newOnlyUnknown = false;
                            if ((tptp == OCRElementTypeSubtotal) || (tptp == OCRElementTypeTotal))
                                newHasSubTotalOrTotal = true;
                            if (tptp == OCRElementTypePrice)
                                newHasPrice = true;
                        }
                    }
                    if (!newOnlyUnknown) {
                        // Special case: if existing has only unknown and new has price (but not subtotal or total), it is possible that existing unknow matches the subtotal or total regex and we should preserve it! See merge between 1st and 2nd frame for https://drive.google.com/open?id=0B4jSQhcYsC9VZDFtUVY1NWdKQUU
                        bool removeExistingLine = true;
                        if (!newHasSubTotalOrTotal && newHasPrice) {
                            // Check all existing unknown elements in existing line and test them for subtotal / total
                            for (int n=indexFirstItemOnExistingLine; n<=indexLastItemOnExistingLine; n++) {
                                MatchPtr dd = matchesAddressBook[n];
                                if (getElementTypeFromMatch(dd, matchKeyElementType) == OCRElementTypeUnknown) {
                                    wstring itemText = getStringFromMatch(dd, matchKeyText);
                                    // testForSubtotal/Total expect to find only the subtotal string. In this case the unknown item likely includes misrecognized digits so need to tighten the text
                                    RegexUtilsStripLeading(itemText, L" ");
                                    size_t locSpace = itemText.find(L' ');
                                    if (locSpace != wstring::npos)
                                        itemText = itemText.substr(0,locSpace);
                                    if (testForSubtotal(itemText, this)) {
                                        dd[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeSubtotal)).castDown<EmptyType>();
                                        removeExistingLine = false; break;
                                    } else if (testForTotal(itemText, this)) {
                                        dd[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotal)).castDown<EmptyType>();
                                        removeExistingLine = false; break;
                                    }
                                }
                            }
                        }
                        if (removeExistingLine) {
                            // Simply remove all items on existing line
                            MergingLog("importNewResults: removing all items from existing line %d at index %d, new line has typed items", targetExistingLineNumber, indexFirstItemOnExistingLine);
                            int numItemsToRemove = indexLastItemOnExistingLine - indexFirstItemOnExistingLine + 1;
                            for (int n=0; n<numItemsToRemove; n++) {
                                this->matchesAddressBook.erase(this->matchesAddressBook.begin() + indexFirstItemOnExistingLine);
                            }
                            insertionIndex = -1; // That's all we need to do to tell the code that follow to insert on a new line!
                        }
                    }
                } // only unknown items on existing line
            }

             // For lines beyond existing lines targetExistingLineNumber won't exist so insertionIndex will be set to -1
             if (insertionIndex >= 0) {
                // We are not creating a new line, there is a line where we want to add the elements
                
//                // RETAILERSPECIFIC on Wallmart receipts the product number is 2nd (after the description), not first
//                /* Ignore any unknown element until the first type which starts a logical set of elements. For target this is:
//                    1. Product number + descr + price
//                    2. Quantity + price
//                    3. Subtotal + price
//                */
//                if ((tp != RETAILER_GROUP_START1) && (tp != RETAILER_GROUP_START2) && (tp != RETAILER_GROUP_START3)) {
//                    //PQTODO - also capture prices w/o a product number? Dangerous if it's a price other than a product?
//                    continue;
//                }
                
                int currentInsertionIndex = insertionIndex; // Start of line (regardless of what component(s) are or aren't present on that line
                
                /* Before even handling structured types below, give unknown types a chance to update existing results such that we can improve their accuracy. Only if:
                    - the unknown item in new results has an item following it and it's a typed item
                    - in existing results, there is somewhere an unknown item followed by an item of the same typed type
                    Note: this helps with Target where we may have found "X @ $1.20" where "X" is a bad quantity then when next frame(s) DO yield say "2 @ $1.20" we don't override the "X" with "2" (both unknown till later when we detect quantities)
                    2015-10-13 in one case existing results got us a price per item and a single char before it ('@'). That one got eliminated (single-char item), then another frame contributed both "2 I@" and a price, but we didn't merge in the unknown "2 I@". Need to insert the unknown when existing results on that line has a price and no item prior to it
                */
                int indexFirstItemOnExistingLine = insertionIndex;
                int indexLastItemOnExistingLine = indexLastElementOnLineAtIndex(insertionIndex, &matchesAddressBook);
                int indexFirstItemOnNewLine = i;
                int indexLastItemOnNewLine = indexLastElementOnLineAtIndex(i, &matches);
                for (int j=indexFirstItemOnNewLine; j < indexLastItemOnNewLine; j++) {
                    if (getIntFromMatch(matches[j], matchKeyElementType) != OCRElementTypeUnknown)
                        continue;
                    OCRElementType tpNewNext = (OCRElementType)getIntFromMatch(matches[j+1], matchKeyElementType);
                    if (tpNewNext != OCRElementTypeUnknown) {
                    
                        // First check: see if the very first element on the line is the typed one, in that case insert the unknown from new results at the start of the line
                        if (getIntFromMatch(matchesAddressBook[indexFirstItemOnExistingLine], matchKeyElementType) == tpNewNext) {
                            // Create new match based on the unknown item in new results
                            MatchPtr newItem = copyMatch(matches[j]);
                            // Adjust line number
                            newItem[matchKeyLine] = SmartPtr<int>(new int(targetExistingLineNumber)).castDown<EmptyType>();
                            matchesAddressBook.insert(matchesAddressBook.begin() + indexFirstItemOnExistingLine, newItem);
                            // Update indexLastItemOnExistingLine because of the insertion
                            indexLastItemOnExistingLine++;
                            MergingLog("importNewResults: inserted new unknown item [%s] found before item [%s] of type [%s] at line %d",  toUTF8(getStringFromMatch(matches[j], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexFirstItemOnExistingLine+1], matchKeyText)).c_str(), (stringForOCRElementType(tpNewNext)).c_str(), getIntFromMatch(matches[j], matchKeyLine));
                        } else {
                            // Now find first pair of items in existing matches of type unknown + above type
                            for (int k=indexFirstItemOnExistingLine; k < indexLastItemOnExistingLine; k++) {
                                OCRElementType tpK = getElementTypeFromMatch(matchesAddressBook[k], matchKeyElementType);
                                if ((tpK == OCRElementTypeUnknown)
                                    && (getElementTypeFromMatch(matchesAddressBook[k+1], matchKeyElementType) == tpNewNext)) {
                                    // Found it! Don't set meaningfulFrame bc this is an unknown item (less important)
                                    if ((!this->subtotalMatchesSumOfPricesMinusDiscounts)
                                            || ((tpK != OCRElementTypeProductNumber) && (tpK != OCRElementTypePrice))) {
                                        wstring existingText = getStringFromMatch(matchesAddressBook[k], matchKeyText);
#if DEBUG
                                        bool madeChanges =
#endif
                                        mergeNewItemWithExistingItem(j, matches, k, matchesAddressBook);
#if DEBUG
                                        if (madeChanges) {
                                            MergingLog("importNewResults: updated unknown existing item [%s] with new unknown item [%s] found before item of type [%s] at line %d",  toUTF8(existingText).c_str(), toUTF8(getStringFromMatch(matches[j], matchKeyText)).c_str(), (stringForOCRElementType(tpNewNext)).c_str(), getIntFromMatch(matches[j], matchKeyLine));
                                            DebugLog("");
                                        }
#endif
                                    }
                                    // Since we used the existing items at j/j+1, make sure next time we consider a new item, we only look at existing line past past these two
                                    indexFirstItemOnExistingLine = k+2;
                                    // Skip the non-unknown item
                                    k++;
                                    break;  // Done with this unknown+type in new results
                                }
                                continue;
                            } // Search for unknown + typed in existing results
                            if (indexFirstItemOnExistingLine >= indexLastItemOnExistingLine - 1)
                                break;
                        } // If we didn't find the typed item first on the existing line
                    } // found unknown + typed in new results
                }
                
                // Create or replace all 3 components of a product (for target that's product number + product description + product price). Note: for target we have 4 components even though a line will have at most 3 (product + desc + price) because we include quantity + price.
                for (int lineComponent=0; lineComponent <= this->retailerLineComponents.size(); lineComponent++) {
                    OCRElementType tpToCreate = this->retailerLineComponents[lineComponent];

                    // 2015-10-15 have to put aside the new multiInstancesMode, it wrecked havok on most receipts with many frames - I think because of lines getting merged at the wrong line numbers: eventually things fall in place, but meanwhile it creates lines with prices tacked on from adjacent lines, which we were somehow resilient to until the multi-instances code

                    int indexNewComponent = elementWithTypeInlineAtIndex(i, tpToCreate, &matches, NULL);

                        // Do we have that component in existing results?
                        int indexExistingComponent = elementWithTypeInlineAtIndex(insertionIndex, tpToCreate, &matchesAddressBook, NULL);
                        if (indexNewComponent < 0) {
                            // Not present - continue to other components
                            if (indexExistingComponent >= 0) {
                                // Even though we are not updating that component, it's there in existing so update insertionIndex
                                currentInsertionIndex = indexExistingComponent + 1;
                            }
                            continue;
                        }
                        // Update or insert in existing results?
                        if (indexExistingComponent >= 0) {
                            // No matter how we merge, next insertion point will be past this existing item
                            currentInsertionIndex = indexExistingComponent + 1;
                            if ((!this->subtotalMatchesSumOfPricesMinusDiscounts)
                                            // Allow updates to product numbers even past the point where sum == subtotal
                                            || ((tpToCreate != OCRElementTypePrice) && (tpToCreate != OCRElementTypeProductPrice))) {
                                wstring existingItemTextBeforeMerge = getStringFromMatch(matchesAddressBook[indexExistingComponent], matchKeyText);
                                MergingLog("importNewResults: merging new item [%s] of type [%s] with existing item [%s] at existing line %d", toUTF8(getStringFromMatch(matches[indexNewComponent], matchKeyText)).c_str(),(stringForOCRElementType(tpToCreate)).c_str(), toUTF8(existingItemTextBeforeMerge).c_str(),  targetExistingLineNumber);
//#if DEBUG
//                                if (((existingItemTextBeforeMerge == L"055090161") && (getStringFromMatch(matches[indexNewComponent], matchKeyText) == L"055093161"))
//                                    || ((existingItemTextBeforeMerge == L"055093161") && (getStringFromMatch(matches[indexNewComponent], matchKeyText) == L"055090161"))) {
//                                    DebugLog("");
//                                }
//#endif
                                bool changesMadeToItem = mergeNewItemWithExistingItem(indexNewComponent, matches, indexExistingComponent, matchesAddressBook);
                                // Only set newInfoAdded when a text update occured to typed items other than descriptions (unless retailer has no product numbers)
                                if ((tpToCreate != OCRElementTypeUnknown) && (tpToCreate != OCRElementTypeNotAnalyzed) && (tpToCreate != OCRElementTypePotentialPriceDiscount) && (tpToCreate != OCRElementTypePartialProductPrice)
                                    && ((tpToCreate != OCRElementTypeProductDescription) || (this->retailerParams.noProductNumbers))) {
                                
                                    if (changesMadeToItem)
                                        meaningfulFrame = true;
                                    if (!this->newInfoAdded) {
                                        if ((tpToCreate == OCRElementTypeProductDescription) && this->retailerParams.noProductNumbers) {
                                                if (canonicalString(getStringFromMatch(matchesAddressBook[indexExistingComponent], matchKeyText)) != canonicalString(existingItemTextBeforeMerge))
                                                this->newInfoAdded = true;
                                        } else {
                                            if (getStringFromMatch(matchesAddressBook[indexExistingComponent], matchKeyText) != existingItemTextBeforeMerge)
                                                this->newInfoAdded = true;
                                        }
                                    }
                                }
                            }
                            continue;
                        } // End type matches an existing component (indexExistingComponent >= 0)
                        // New component not found in existing line, just add
                        else {
                            if ((this->subtotalMatchesSumOfPricesMinusDiscounts) && (!this->retailerParams.hasMultipleSubtotals)
                                && ((tpToCreate == OCRElementTypeProductNumber)
                                    || (this->retailerParams.noProductNumbers && ((tpToCreate == OCRElementTypeProductDescription) || (tpToCreate == OCRElementTypeProductPrice))))) {
                                MergingLog("importNewResults: skipping type [%s] item [%s] at existing line %d already matched subtotal!", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(getStringFromMatch(matches[indexNewComponent], matchKeyText)).c_str(),  targetExistingLineNumber);
                                continue;
                            }
                            MergingLog("importNewResults: adding type [%s] item [%s] at existing line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(getStringFromMatch(matches[indexNewComponent], matchKeyText)).c_str(),  targetExistingLineNumber);
                            //if (tpToCreate == OCRElementTypeProductNumber) meaningfulFrame = newInfoAdded = true;
                            // 10-02-2015 why only for products? Other types are significant too
                            if ((tpToCreate != OCRElementTypeUnknown) && (tpToCreate != OCRElementTypeNotAnalyzed) && (tpToCreate != OCRElementTypePotentialPriceDiscount) && (tpToCreate != OCRElementTypePartialProductPrice) && ((tpToCreate != OCRElementTypeProductDescription) || this->retailerParams.noProductNumbers)) {
                                this->meaningfulFrame = this->newInfoAdded = true;
                            }
                            MatchPtr newItem = copyMatch(matches[indexNewComponent]);
                            newItem[matchKeyLine] = SmartPtr<int>(new int(targetExistingLineNumber)).castDown<EmptyType>();
                            checkForSpecificMatch(newItem, matches, indexNewComponent);
                            matchesAddressBook.insert(matchesAddressBook.begin() + currentInsertionIndex, newItem);
                            if (tpToCreate == OCRElementTypePrice)
                                checkPriceQuantityLinkage(currentInsertionIndex, matchesAddressBook, wstring(), &this->retailerParams);
                            currentInsertionIndex++;   // For the next components move beyond what we just inserted
                        }
                  
                } // iterating over line components

                // Done adding / replacing product components on this line
                // For now ignore & leave unknown items
             } // Updating existing line
             // Creating a new line
             else {
                // Not found, insert just before the existing line closest (after) targetExistingLineNumber
                int lineNumberForNewAddedLine = targetExistingLineNumber;
                insertionIndex = indexJustBeforeLine(targetExistingLineNumber, &matchesAddressBook);
                if (insertionIndex < 0) {
                    insertionIndex = (int)matchesAddressBook.size();
                    lineNumberForNewAddedLine = targetExistingLineNumber;
                }
                // Copy all items on the line in new matches (product, description, price)
                int indexNewStartLine = indexFirstElementOnLineAtIndex(i, &matches);
                // Before adding that line, check it has at least one interesting element
                if (!isMeaningfulLineAtIndex(i, newResults)) {
                    MergingLog("importNewResults: skipping new line with %d items [%s]-[%s] at existing line %d - no interesting elements", indexNewEndLine - indexNewStartLine + 1, toUTF8(getStringFromMatch(matches[indexNewStartLine], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[indexNewEndLine], matchKeyText)).c_str(), lineNumberForNewAddedLine);
                    // Done with current line in new matches, move to start of the next line
                    i = indexLastElementOnLineAtIndex(i, &matches);
                    continue;
                }
                if (this->subtotalMatchesSumOfPricesMinusDiscounts) {
                    MergingLog("importNewResults: skipping new line with %d items [%s]-[%s] at existing line %d - already matching subtotal", indexNewEndLine - indexNewStartLine + 1, toUTF8(getStringFromMatch(matches[indexNewStartLine], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[indexNewEndLine], matchKeyText)).c_str(), lineNumberForNewAddedLine);
                    // Done with current line in new matches, move to start of the next line
                    i = indexLastElementOnLineAtIndex(i, &matches); continue;
                }
                MergingLog("importNewResults: adding new line with %d items [%s]-[%s] at existing line %d", indexNewEndLine - indexNewStartLine + 1, toUTF8(getStringFromMatch(matches[indexNewStartLine], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[indexNewEndLine], matchKeyText)).c_str(), lineNumberForNewAddedLine);
                for (int kk=indexNewStartLine; kk<=indexNewEndLine; kk++) {
                    MatchPtr dd = matches[kk];
                    //if (getElementTypeFromMatch(dd, matchKeyElementType) == OCRElementTypeProductNumber)
                    OCRElementType tp = getElementTypeFromMatch(dd, matchKeyElementType);
                    if ((tp != OCRElementTypeUnknown) && (tp != OCRElementTypeNotAnalyzed) && (tp != OCRElementTypePotentialPriceDiscount) && (tp != OCRElementTypePartialProductPrice) && ((tp != OCRElementTypeProductDescription) || this->retailerParams.noProductNumbers)) {
                        this->meaningfulFrame = this->newInfoAdded = true;
                    }
                    MatchPtr newItem = copyMatch(dd);
                    newItem[matchKeyLine] = SmartPtr<int>(new int(lineNumberForNewAddedLine)).castDown<EmptyType>();
                    checkForSpecificMatch(newItem, matches, kk);
                    matchesAddressBook.insert(matchesAddressBook.begin() + insertionIndex + (kk - indexNewStartLine), newItem);
                    //checkLines(matchesAddressBook);
                }
            }
            // Done with current line in new matches, move to start of the next line
            i = indexLastElementOnLineAtIndex(i, &matches);
        } // for i, iterating over new matches
    } else {
        // Only add new lines if we didn't already reach the state where subtotal matches sum of products!
        if (!this->subtotalMatchesSumOfPricesMinusDiscounts) {
            // No identical products, assume new frame has no overlap with current frame. Note: if existing results are worthless (no products), it will mean keeping them + tacking on the new results, which should be fine since the app will ignore lines without a product number
            // Now add all new elements one by one
            int indexEndMeaningfulLine = -1;
            for (int i=0; i<matches.size(); i++) {
                MatchPtr d = matches[i];
                
                // If we are in the midst of a line deemed meaningfull, no need to test the below, carry on
                if (!((indexEndMeaningfulLine >= 0) && (i <= indexEndMeaningfulLine))) {
                    // New untested line: before doing anything with that line, check it has at least one interesting element
                    if (!isMeaningfulLineAtIndex(i, newResults)) {
                        int indexNewEndLine = indexLastElementOnLineAtIndex(i, &matches);
                        MergingLog("importNewResults: skipping new line with %d items [%s]-[%s] - no interesting elements", indexNewEndLine - i + 1, toUTF8(getStringFromMatch(matches[i], matchKeyText)).c_str(), toUTF8(getStringFromMatch(matches[indexNewEndLine], matchKeyText)).c_str());
                        // Done with current line in new matches, move to start of the next line
                        i = indexNewEndLine;
                        indexEndMeaningfulLine = -1;
                        continue;
                    } else {
                        // Set the end of this line so we don't keep testing the current line to see if meaningful
                        indexEndMeaningfulLine = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
                    }
                }
                
                MatchPtr newItem = copyMatch(d);
                if (getIntFromMatch(d, matchKeyElementType) != OCRElementTypePartialProductPrice) {
                    this->meaningfulFrame = this->newInfoAdded = true;
                }
                int lineInNewResults = getIntFromMatch(d, matchKeyLine);
                int newLineInExisting = lineInNewResults + lastExistingLineNumber + 1;
                newItem[matchKeyLine] = SmartPtr<int>(new int(newLineInExisting)).castDown<EmptyType>();
                
                // Special case: if we are looking at the first line of the new results and there is a price on it, could be a quantity/price-per-item combo. If that line then completes a line above in within existing results, we need to verify the position of this price relative to product prices and if to the left, tag it as possible price-per-item. Look for the next product + price line
                if (lineInNewResults == 0) {
                    OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
                    if (tp == OCRElementTypePrice) {
                        int indexProductCurrentLine = elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &matches, NULL);
                        if (indexProductCurrentLine < 0) {
                            // Not a product line!
                            int j = indexLastElementOnLineAtIndex(i, &matches) + 1;
                            for (; j<matches.size(); j++) {
                                int indexProduct = elementWithTypeInlineAtIndex(j, OCRElementTypeProductNumber, &matches, NULL);
                                if (indexProduct > 0) {
                                    // Found a product! Now get the price on that line
                                    int indexPrice = elementWithTypeInlineAtIndex(j, OCRElementTypePrice, &matches, NULL);
                                    if (indexPrice > 0) {
                                        // Finally, get rects for this price and compare to the rect for the possible quantity price
                                        CGRect rectQuantityPrice = OCRResults::matchRange(d);
                                        MatchPtr productPriceMatch = matches[indexPrice];
                                        CGRect rectProductPrice = OCRResults::matchRange(productPriceMatch);
                                        if ((rectProductPrice.size.width > 0) && (rectQuantityPrice.size.width > 0)
                                                && (rectLeft(rectProductPrice) > rectRight(rectQuantityPrice))) {
                                            unsigned long matchStatus = 0;
                                            if (newItem.isExists(matchKeyStatus)) {
                                                matchStatus = getUnsignedLongFromMatch(newItem, matchKeyStatus);
                                            }
                                            matchStatus |= matchKeyStatusPossiblePricePerItem;
                                            newItem[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(matchStatus)).castDown<EmptyType>();
                                        }
                                        break; // Whether or not we tagged this price as possible price-per-item, we made a decision, stop looking
                                    }
                                } else {
                                    // Skip that entire line
                                    j = indexLastElementOnLineAtIndex(j, &matches);
                                    continue;
                                }
                            }
                        }
                    }
                } // first line in new results
                newItem[matchKeyLine] = (SmartPtr<int> (new int(newLineInExisting))).castDown<EmptyType>();
                matchesAddressBook.push_back(newItem);
                //checkLines(matchesAddressBook);
            }
        } // subtotal doesn't yet match sum of products
    } // Case with no overlap, i.e. just appending new lines
    
    OCRResults::checkForProductsOnDiscountLines(matchesAddressBook);
    
    // Set gotSubtotal and gotTotal
    this->gotTotal = false;
    for (int i=0; i<matchesAddressBook.size(); i++) {
        MatchPtr d = matchesAddressBook[i];
        if (getElementTypeFromMatch(d, matchKeyElementType) == OCRElementTypeTotal) {
            this->gotTotal = true;
            break;
        }
    }
    
    // Search for total again after merging, for the case where one frame got the total + tax above while another frame got us the "TOTAL"
    if (!this->gotTotal) {
        OCRResults::checkForQuantities();
    }

#if DEBUG
    if (meaningfulFrame) {
        MergingLog("importNewResults: done importing frame %d - meaningful", frameNumber);
    } else {
        MergingLog("importNewResults: done importing frame - ignored - last meaningfulframe=%d", frameNumber);
    }
#endif

    return true;
}

vector<SmartPtr<CGRect> > copyRects(CGRect* rects, int n) {

			vector< SmartPtr<CGRect> > tmpRects;
			for(int i = 0; i < n; ++i){
			    tmpRects.push_back(SmartPtr<CGRect>(new CGRect(rects[i])));
			}

			return tmpRects;
}

vector<SmartPtr<CGRect> > copyRects(vector<SmartPtr<CGRect> >& adjustedRects) {
    vector<SmartPtr<CGRect> > v;
    for (int i = 0; i < adjustedRects.size(); ++i) {
        v.push_back(SmartPtr<CGRect>(new CGRect(*adjustedRects[i])));
    }

    return v;
}


void copyRects(vector<SmartPtr<CGRect> >& from, vector<SmartPtr<CGRect> >& to) {
    vector<SmartPtr<CGRect> >& v=to;
    for (int i = 0; i < from.size(); ++i){
        v.push_back(SmartPtr<CGRect>(new CGRect(*from[i])));
    }
}


void copyRects(vector<SmartPtr<CGRect> >::iterator from, int howMuch, vector<SmartPtr<CGRect> >& to) {
    vector<SmartPtr<CGRect> >& v=to;
    for (int i = 0; i < howMuch; ++i, ++from){
        v.push_back(SmartPtr<CGRect>(new CGRect(**from)));
    }
}

vector<SmartPtr<CGRect> > copyRects(vector<SmartPtr<CGRect> >::iterator start,vector<SmartPtr<CGRect> >::iterator end) {
    vector<SmartPtr<CGRect> > v;
    for (;start != end; ++start){
        v.push_back(SmartPtr<CGRect>(new CGRect(**start)));
    }

    return v;
}

float OCRResults::overlapAmountRectsYAxisAnd(CGRect r1, CGRect r2, float minOverlap) {
	if ((r2.size.height == 0) || (r1.size.height == 0))
		return 0;

	float overlapYHeight = min(rectTop(r1), rectTop(r2)) - max(rectBottom(r1), rectBottom(r2));

	if ((overlapYHeight < 0) || (overlapYHeight < r1.size.height*minOverlap) || (overlapYHeight < r2.size.height*minOverlap))
		return 0;
	else
		return overlapYHeight;
}

float OCRResults::overlapPercentRectsYAxisAnd(CGRect r1, CGRect r2) {
    float overlapAmount = overlapAmountRectsYAxisAnd(r1, r2, 0);
    if (overlapAmount <= 0)
        return 0;
    float referenceHeight = min(r1.size.height, r2.size.height);
    return (overlapAmount / referenceHeight);
}

float OCRResults::overlapAmountRectsXAxisAnd(CGRect r1, CGRect r2) {
	if ((r2.size.width == 0) || (r1.size.width == 0))
		return 0;

	float overlap = min(rectRight(r1), rectRight(r2)) - max(rectLeft(r1), rectLeft(r2)) + 1;

	if (overlap < 0)
		return 0;
	else
		return overlap;
}

bool OCRResults::overlappingRectsYAxisAnd(CGRect r1, CGRect r2){

    return (overlapAmountRectsYAxisAnd(r1, r2, 0) > 0);
}

bool OCRResults::overlappingRectsXAxisAnd(CGRect r1, CGRect r2){

    return (overlapAmountRectsXAxisAnd(r1, r2) > 0);
}

void OCRResults::checkForSpecificMatch(MatchPtr m, MatchVector& matches, int index) {
#if DEBUG
//    wstring& itemText = getStringFromMatch(m, matchKeyText);
//    CGRect itemRange = OCRResults::matchRange(m);
//    int line = getIntFromMatch(m,matchKeyLine);
//    if ((itemText == L"007874208717") || (itemText == L"3.98")) {
//        DebugLog("");
//    }
//    if ((itemRange.size.width <= 1) && (itemRange.size.height <= 1)) {
//        int indexOfM = index;
//        if (index < 0) {
//            for (int i=0; i<matches.size(); i++) {
//                if (matches[i] == m) {
//                    indexOfM = i;
//                    break;
//                }
//            }
//        }
//        
//        DebugLog("checkForSpecificMatch: found item [%s] within matches at index=%d (out of %d), range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), indexOfM, (int)matches.size(), itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
//        DebugLog("");
//    }
    
//    if (itemText == L"8 4% 04 .") {
//        DebugLog("checkForSpecificMatch: found item [%s] within matches at index=%d (out of %d), line=%d, range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), index, (int)matches.size(), line, itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
//        DebugLog("");
//    } else if (line == 28) {
//        DebugLog("checkForSpecificMatch: found item [%s] within matches at index=%d (out of %d), line=%d, range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), index, (int)matches.size(), line, itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
//        DebugLog("");
//    }
#endif
}

void OCRResults::checkForSpecificMatches(MatchVector& matches) {
#if DEBUG
    for (int i=0; i<matches.size(); i++) {
        checkForSpecificMatch(matches[i], matches, i);
    }
#endif
}

void OCRResults::checkLines(MatchVector& matches) {
#if DEBUG
    int lastLine = -1000;
    for (int i=0; i<matches.size(); i++) {
        MatchPtr m = matches[i];
        CGRect itemRange = OCRResults::matchRange(m);
        wstring& itemText = getStringFromMatch(m, matchKeyText);
        if (!m.isExists(matchKeyLine)) {
            DebugLog("checkLines: WARNING item [%s] within matches at index=%d (out of %d) has no line! range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), i, (int)matches.size(), itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
            DebugLog("");
        }
        int currentLine = getIntFromMatch(m, matchKeyLine);
        if ((currentLine < 0) || (currentLine > 1000)) {
            DebugLog("checkLines: WARNING item [%s] within matches at index=%d (out of %d) has line=%d range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), i, (int)matches.size(), currentLine, itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
            DebugLog("");
        }
        if (currentLine < lastLine) {
            DebugLog("checkLines: WARNING item [%s] within matches at index=%d (out of %d) has line=%d LOWER than previous line=%d, range = [%.0f,%.0f - w=%d,h=%d]", toUTF8(itemText).c_str(), i, (int)matches.size(), currentLine, lastLine, itemRange.origin.x, itemRange.origin.y, (int)itemRange.size.width, (int)itemRange.size.height);
            DebugLog("");
        }
        lastLine = currentLine;
    }
#endif
}

// From index start to index end (included)
// Meant to be used for adding fragments (which are not yet into the official matches array)
MatchPtr OCRResults::addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex(MatchVector& targetMatches,
						wstring newText, OCRElementType tp, RectVectorPtr rects, int start, int end, float conf , int index) {
	if (rects != NULL) {
		if (end >= rects->size()) {
			DebugLog("OCRGResults::addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex: WARNING, index %d beyond rects array length (%lu), trimming",
				 end, rects->size());
			end = (int)rects->size()-1;
		}
	} else {
		if (end >= page->coordinates()->size()) {
			DebugLog("OCRResults::addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex: WARNING, index %d beyond page array length (%lu), trimming",
								end, page->coordinates()->size());
			end = (int)page->coordinates()->size()-1;
		}
	}
	int nRects = (end - start) + 1;
	SmartPtr<wstring> newItemText(new wstring(newText));
	SmartPtr<size_t> newItemN(new size_t(nRects));
	vector<SmartPtr<CGRect> > newItemRects;
	if (rects != NULL) {
        newItemRects = copyRects(rects->begin()+start,rects->begin()+start+nRects);
	}
	else {
		newItemRects = copyRects(page->coordinates()->begin()+start, page->coordinates()->begin()+start+nRects);
	}
	SmartPtr<vector<SmartPtr<CGRect> > > newItemRectsValue(new vector<SmartPtr<CGRect> >(newItemRects));
	SmartPtr<float> newItemConfidence(new float(conf));
	SmartPtr<OCRElementType> newItemType(new OCRElementType(tp));
	SmartPtr<bool> newItemInMatches(new bool(false));

	MatchPtr newItem(true);

	newItem[matchKeyElementType] = newItemType.castDown<EmptyType>();
	newItem[matchKeyText] = newItemText.castDown<EmptyType>();

	newItem[matchKeyN] = newItemN.castDown<EmptyType>();
	newItem[matchKeyRects] = newItemRectsValue.castDown<EmptyType>();
	newItem[matchKeyConfidence] = newItemConfidence.castDown<EmptyType>();

	if (index != -1) {
		targetMatches.insert(targetMatches.begin() + index, newItem);
	} else {
        // Add at the end
        targetMatches.insert(targetMatches.end(), newItem);
	}

	return (newItem);
}

// Creates a copy of an existing match
MatchPtr OCRResults::copyMatch(MatchPtr d) {
    MatchPtr newItem(true);

    // Item text (always exists)
    newItem[matchKeyText] = (SmartPtr<wstring> (new wstring(getStringFromMatch(d, matchKeyText)))).castDown<EmptyType>();
    
    if (d.isExists(matchKeyText2)) {
        newItem[matchKeyText2] = (SmartPtr<wstring> (new wstring(getStringFromMatch(d, matchKeyText2)))).castDown<EmptyType>();
        if (d.isExists(matchKeyConfidence2))
            newItem[matchKeyConfidence2] = (SmartPtr<float> (new float(getFloatFromMatch(d, matchKeyConfidence2)))).castDown<EmptyType>();
        else
            newItem[matchKeyConfidence2] = (SmartPtr<float> (new float(1.00))).castDown<EmptyType>(); // No confidence2 - set to low confidence
        if (d.isExists(matchKeyCountMatched2))
            newItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(getIntFromMatch(d, matchKeyCountMatched2)))).castDown<EmptyType>();
        else
            newItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
    }
    
    if (d.isExists(matchKeyFullLineText)) {
        newItem[matchKeyFullLineText] = (SmartPtr<wstring> (new wstring(getStringFromMatch(d, matchKeyFullLineText)))).castDown<EmptyType>();
    }
    
    // Item type (always exists)
    newItem[matchKeyElementType] = (SmartPtr<OCRElementType> (new OCRElementType(getElementTypeFromMatch(d, matchKeyElementType)))).castDown<EmptyType>();
    
    // Confidence
    float confidence = 0;
    if (d.isExists(matchKeyConfidence)) {
        confidence = getFloatFromMatch(d, matchKeyConfidence);
    }
	newItem[matchKeyConfidence] = (SmartPtr<float> (new float(confidence))).castDown<EmptyType>();
    
    // Block
    if (d.isExists(matchKeyBlock)) {
        int block = getIntFromMatch(d, matchKeyBlock);
        newItem[matchKeyBlock] = (SmartPtr<int> (new int(block))).castDown<EmptyType>();
    }
    
    // Line
    if (d.isExists(matchKeyLine)) {
        int line = getIntFromMatch(d, matchKeyLine);
        newItem[matchKeyLine] = (SmartPtr<int> (new int(line))).castDown<EmptyType>();
    }
    
    // Column
    if (d.isExists(matchKeyColumn)) {
        int column = getIntFromMatch(d, matchKeyColumn);
        newItem[matchKeyColumn] = (SmartPtr<int> (new int(column))).castDown<EmptyType>();
    }
    
    // Status
    if (d.isExists(matchKeyStatus)) {
        unsigned long status = getUnsignedLongFromMatch(d, matchKeyStatus);
        newItem[matchKeyStatus] = (SmartPtr<unsigned long> (new unsigned long(status))).castDown<EmptyType>();
    }
    
    // Quantity
    if (d.isExists(matchKeyQuantity)) {
        int quant = getIntFromMatch(d, matchKeyQuantity);
        newItem[matchKeyQuantity] = (SmartPtr<int> (new int(quant))).castDown<EmptyType>();
    }
    
    // Price per item
    if (d.isExists(matchKeyPricePerItem)) {
        float ppi = getFloatFromMatch(d, matchKeyPricePerItem);
        newItem[matchKeyPricePerItem] = (SmartPtr<float> (new float(ppi))).castDown<EmptyType>();
    }
    
    int numNonSpaceLetters = (d.isExists(matchKeyNumNonSpaceLetters)? getIntFromMatch(d, matchKeyNumNonSpaceLetters):0);
    float percentVerticalLines = (d.isExists(matchKeyPercentVerticalLines)? getFloatFromMatch(d, matchKeyPercentVerticalLines):-1);
    newItem[matchKeyPercentVerticalLines] = SmartPtr<float>(new float(percentVerticalLines)).castDown<EmptyType>();
    newItem[matchKeyNumNonSpaceLetters] = SmartPtr<int>(new int(numNonSpaceLetters)).castDown<EmptyType>();
    
    // Rects (if exist)
    //PQTODO crashes - ignoring rects for now
//    size_t nRects = 0;
//    RectVectorPtr rects;
//    if (d.isExists(matchKeyRects) && d.isExists(matchKeyN)) {
//        nRects = *d[matchKeyN].castDown<size_t>();
//        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
//    }
//    if (nRects > 0) {
//        vector<SmartPtr<CGRect> > newItemRects;
//        for (int i=0; i<rects->size(); i++) {
//            newItemRects.push_back((*rects)[i]);
//        }
//
//        newItem[matchKeyN] = (SmartPtr<size_t> (new size_t(nRects))).castDown<EmptyType>();
//        newItem[matchKeyRects] = (SmartPtr<vector<SmartPtr<CGRect> > > (new vector<SmartPtr<CGRect> >(newItemRects))).castDown<EmptyType>();
//    }

	return (newItem);
} // copyMatch

// From index start to index end (included)
// Meant to be used for adding fragments (which are not yet into the official matches array)
MatchPtr OCRResults::addSubFragmentWithTextAndTypeAndRectsFromIndexToIndexWithConfidenceBlockLineColumnAtIndex(MatchVector& targetMatches,
						wstring newText, OCRElementType tp, RectVectorPtr rects, int start, int end, float conf , int block, int line, int column, int index) {
    if (rects == NULL) return MatchPtr();
    if (end >= rects->size()) {
        DebugLog("OCRGResults::addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex: WARNING, index %d beyond rects array length (%lu), trimming",
             end, rects->size());
        end = (int)rects->size()-1;
    }
	int nRects = (end - start) + 1;
	SmartPtr<wstring> newItemText(new wstring(newText));
	SmartPtr<size_t> newItemN(new size_t(nRects));
	vector<SmartPtr<CGRect> > newItemRects;
    newItemRects = copyRects(rects->begin()+start,rects->begin()+start+nRects);
	SmartPtr<vector<SmartPtr<CGRect> > > newItemRectsValue(new vector<SmartPtr<CGRect> >(newItemRects));
	SmartPtr<float> newItemConfidence(new float(conf));
	SmartPtr<OCRElementType> newItemType(new OCRElementType(tp));
	SmartPtr<bool> newItemInMatches(new bool(false));
#if DEBUG
    if ((block < 0) || (line < 0) || (column < 0)) {
        DebugLog("addSubFragmentWithTextAndTypeAndRectsFromIndexToIndexWithConfidenceAtIndex: WARNING, called for item [%s] with invalid position values block=%d, line=%d, column=%d", toUTF8(newText).c_str(), block, line, column);
    }
#endif
    if (block < 0) block = 0; if (line < 0) line = 0; if (column < 0) column = 0;
    SmartPtr<int> newItemBlock(new int(block));
    SmartPtr<int> newItemLine(new int(line));
    SmartPtr<int> newItemColumn(new int(column));

	MatchPtr newItem(true);

	newItem[matchKeyElementType] = newItemType.castDown<EmptyType>();
	newItem[matchKeyText] = newItemText.castDown<EmptyType>();

	newItem[matchKeyN] = newItemN.castDown<EmptyType>();
	newItem[matchKeyRects] = newItemRectsValue.castDown<EmptyType>();
	newItem[matchKeyConfidence] = newItemConfidence.castDown<EmptyType>();
    newItem[matchKeyBlock] = newItemBlock.castDown<EmptyType>();
    newItem[matchKeyLine] = newItemLine.castDown<EmptyType>();
    newItem[matchKeyColumn] = newItemColumn.castDown<EmptyType>();

	if (index != -1) {
		targetMatches.insert(targetMatches.begin() + index, newItem);
	} else {
        // Add at the end
        targetMatches.insert(targetMatches.end(), newItem);
	}

	return (newItem);
} // addSubFragmentWithTextAndTypeAndRectsFromIndexToIndexWithConfidenceBlockLineColumnAtIndex

// From index start to index end (not included)
// Meant to be used for adding fragments (which are not yet into the official matches array)
MatchPtr OCRResults::addNewFragmentToWithTextAndTypeFromIndexToIndexAtIndex(MatchVector& targetMatches, const wstring& newText, OCRElementType tp, int start, int end, int index){
	float totalConfidence=0; int numRealConfidence=0;
	for (int i=start; i<=end; i++)
	{
		SmartPtr<OCRRect> o_r = page->rects[i];
		if (o_r->confidence != 0) {
			totalConfidence += o_r->confidence;
			numRealConfidence++;
		}
	}
    
    // Assign to the fragment the average confidence of the characters in it
	if (numRealConfidence > 0)
		totalConfidence /= numRealConfidence;
        
    return addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex(targetMatches, newText, tp, SmartPtr<vector<SmartPtr<CGRect> > >((std::vector<SmartPtr<CGRect> > *)NULL), start, end, totalConfidence, index);
}

/* Called for each partial scan after processing all characters, inserting newlines and spaces. We take the processed array of all characters and make matches out of them
 */
 void OCRResults::buildAllWords () {
	MatchVector res;

	int startCurrentItem = 0;
	bool overlappingState = false;
	int lineNumber = 0;

	// Go over all rects and build string fragments out of them
	for (int i=0; i<page->rects.size(); i++) {
		OCRRectPtr o_r = page->rects[i];
		wchar_t currentChar = o_r->ch;

		if ((currentChar == '\n') || (currentChar == '\t')) {
			// If not in overlapping state, time to add a fragment
			if (!overlappingState) {
				if (i - startCurrentItem > 0) {
					wstring newText = page->text().substr(startCurrentItem, i - startCurrentItem);	 // Not including the '\n'
					this->addNewFragmentToWithTextAndTypeFromIndexToIndexAtIndex(res, newText, OCRElementTypeNotAnalyzed, startCurrentItem, i-1, -1);
				}
			}
			startCurrentItem = i+1;	// Start a new string (after the '\n' or '\t')
			if (currentChar == '\n')
				lineNumber++;
			continue;
		}
	}

	// Just remove all entries from current addressBookMatches w/o releasing rect arrays, because all entries were copied
	// to new list (res)
	matchesAddressBook.clear();

	matchesAddressBook = res;
} // buildAllWords

void OCRResults::swapLines(int firstLineIndex, MatchVector &matches) {
    int startFirstLine = OCRResults::indexFirstElementOnLineAtIndex(firstLineIndex, &matches);
    int endFirstLine = OCRResults::indexLastElementOnLineAtIndex(firstLineIndex, &matches);
    int startSecondLine = endFirstLine + 1;
    int endSecondLine = OCRResults::indexLastElementOnLineAtIndex(startSecondLine, &matches);
    int firstLineNumber = getIntFromMatch(matches[startFirstLine], matchKeyLine);
    int secondLineNumber = getIntFromMatch(matches[startSecondLine], matchKeyLine);
    
    int nItemsSecondLine = endSecondLine - startSecondLine + 1;
    for (int i=0; i<nItemsSecondLine; i++) {
        MatchPtr d = matches[startSecondLine + i];
        matches.insert(matches.begin() + startFirstLine + i, d);
        // Delete item from original line
        matches.erase(matches.begin() + startSecondLine + i + 1);
    }
    // Set prior 2nd line to the line number of the first line
    for (int i=0; i<nItemsSecondLine; i++) {
        MatchPtr d = matches[startFirstLine + i];
        d[matchKeyLine] = SmartPtr<int>(new int(firstLineNumber)).castDown<EmptyType>();
    }
    // Set prior 1st line to the line number of the 2nd line
    int nItemsFirstLine = endFirstLine - startFirstLine + 1;
    for (int i=0; i<nItemsFirstLine; i++) {
        MatchPtr d = matches[startFirstLine + nItemsSecondLine + i];
        d[matchKeyLine] = SmartPtr<int>(new int(secondLineNumber)).castDown<EmptyType>();
    }
}

// PQTODO setBlocks is too liberal in adding lines to a block: on most receipts even a slight blank line IS a blank line
// PQTODO on Target receipts long product descriptions are so close to price that it gets lumped into a single element - need to fixed either through strict space detection (err on the side of declaring a space when in doubt), or by detecting price-related characters (iffy since $ is often returned as 'S' and conversely legit letters are sometimes returned as digits), or by understanding receipt layout (where the price column starts)
/*
    Strategy:
    - Iterate while matches is not empty:
    - Find the topmost, leftmost item in what remains and make it the start of the first block (block = 1, 0 is reserved for items not physically part of a block), first line in that block (line = 0)
    - Add all items on that line one by one by searching for items to the right
    - When no more items on current line, search again for topmost/leftmost item: if there is a blank line between new line and last line, increment block number
*/
 void OCRResults::setBlocks (bool adjustLinesOnly) {
    
    if (!adjustLinesOnly) {
        // Before doing anything, eliminate items that are all blank. They present multiple issues, including the fact they don't have a valid range
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            wstring& itemText = getStringFromMatch(d, matchKeyText);
            if (isBlank(itemText)) {
                DebugLog("setBlocks: removing all blank item at index [%d]", i);
                matchesAddressBook.erase(matchesAddressBook.begin()+i);
                i--;
            }
        }
        
        if (matchesAddressBook.size() == 0)
            return; // Nothing to do
        
        MatchVectorPtr newMatches(new MatchVector);
        // TODO: important to set lines at 1,000+ at the level of each frame for the sake of multiple products: if we don't, then the 2nd matching product in new results will pair up with the FIRST existing product because the delta will be say 1,020 - 14 with 1st existing versus 1,020 - 18 for the 2nd new => smaller and we take it
        int pos = 0, block = 1, line = 0, vloc = 0;
        // Gather all blocks
        MatchPtr itemAtLineStart = this->findStartOfTopmostLine(matchesAddressBook, -1);
        while ((itemAtLineStart != NULL) && (matchesAddressBook.size() > 0)) {

            MatchPtr nextItemOnLine = itemAtLineStart;
            // Gather all items on current line
            while (nextItemOnLine != NULL) {
                CGRect nextItemOnLineRect = OCRResults::matchRange(nextItemOnLine);
                DebugLog("SetBlocks: adding item [%s] at block=%d, line=%d, column=%d, range [%d,%d - %d,%d]", toUTF8(getStringFromMatch(nextItemOnLine, matchKeyText)).c_str(), block, line, vloc, (int)nextItemOnLineRect.origin.x, (int)nextItemOnLineRect.origin.y, (int)(nextItemOnLineRect.origin.x + nextItemOnLineRect.size.width - 1), (int)(nextItemOnLineRect.origin.y + nextItemOnLineRect.size.height - 1));
    //#if DEBUG
    //wstring tmp = getStringFromMatch(nextItemOnLine, matchKeyText);
    //if (tmp == L"wT ..'$$. '") {
    //    DebugLog("Found it");
    //}
    //#endif
                
                this->setBlockandLineandColumnandPositioninMatch(block, line, vloc, pos, nextItemOnLine);
                pos++;
                newMatches->push_back(nextItemOnLine);

    // No derived items for now
    //				// Look for any derived fragments which may follow and add them
    //				for (int p = (int)(std::find(matchesAddressBook.begin(), matchesAddressBook.end(), nextItemOnLine)-matchesAddressBook.begin()) + 1; p < matchesAddressBook.size(); p++) {
    //					MatchPtr nextD = matchesAddressBook[p];
    //					float con = *nextD[matchKeyConfidence].castDown<float>();
    //					if ((con != -1) && (con != -2) && (con != -3))
    //						break;
    //					this->setBlockandLineandColumnandPositioninMatch(block, line, vloc, pos, nextD);
    //					pos++;
    //					newMatches->push_back(nextD);
    //					matchesAddressBook.erase(matchesAddressBook.begin() + p);
    //					p--;
    //				}
                

                MatchPtr oldItemOnLine = nextItemOnLine;
    #if DEBUG
                if (getStringFromMatch(oldItemOnLine, matchKeyText) == L"007074209129") {
                    DebugLog("");
                }
    #endif
                nextItemOnLine = this->findItemJustAfterMatchInMatchesExcluding(oldItemOnLine, matchesAddressBook, newMatches, -1, false, this);
    #if DEBUG
                if (nextItemOnLine != NULL) {
                    CGRect nextItemOnLineRect = OCRResults::matchRange(nextItemOnLine);
                    wstring nextItemString = getStringFromMatch(nextItemOnLine, matchKeyText);
                    ReplacingLog("setBlocks: considering item [%s] at index [%d] range [%d,%d - %d,%d] to the right of [%s]", toUTF8(nextItemString).c_str(), this->indexOfMatchInMatches(nextItemOnLine, matchesAddressBook), (int)nextItemOnLineRect.origin.x, (int)nextItemOnLineRect.origin.y, (int)(nextItemOnLineRect.origin.x + nextItemOnLineRect.size.width - 1), (int)(nextItemOnLineRect.origin.y + nextItemOnLineRect.size.height - 1), toUTF8(getStringFromMatch(oldItemOnLine, matchKeyText)).c_str());
                    // SetBlocks: adding item [LEL CRITTER . I .IJ. F] at block=1, line=13, column=1, range [269,394 - 515,421]
                    if (nextItemString == L"LEL CRITTER . I .IJ. F") {
                        DebugLog("");
                    }
                }
    #endif
    #if !OCR_ENTIRE_LINE_RELATED 
                MatchPtr itemAboveNextItemOnLine;
                if (nextItemOnLine != NULL) {
                    itemAboveNextItemOnLine = this->findItemJustAboveMatchInMatchesExcluding(nextItemOnLine, matchesAddressBook, newMatches);
                    if (itemAboveNextItemOnLine != NULL) {
                        DebugLog("setBlocks: not using [%s] as item to the right of [%s] because there is an item above right",
                            toUTF8(getStringFromMatch(nextItemOnLine, matchKeyText)).c_str(),
                            toUTF8(getStringFromMatch(oldItemOnLine, matchKeyText)).c_str());
                        nextItemOnLine = MatchPtr();
                    }
                    else {
                        // Another special test: check if the item we just found as next on line is not more closely matching other items just below it => if so, let it get associated with them, not with us, i.e. treat as different blocks
                        MatchPtr itemBelowNextItemOnLine = this->findItemJustBelowMatchInMatchesFindBestExcluding(nextItemOnLine, matchesAddressBook, true, newMatches); 
                        if (itemBelowNextItemOnLine != NULL) {
                            CGRect rangeOldItemOnLine = OCRResults::matchRange(oldItemOnLine);
                            CGRect rangeNextItemOnLine = OCRResults::matchRange(nextItemOnLine);
                            CGRect rangeItemBelowNextItemOnLine = OCRResults::matchRange(itemBelowNextItemOnLine);
                            
                            float letterHeightOldItemOnLine = OCRResults::averageHeightTallLetters(oldItemOnLine);
                            if (letterHeightOldItemOnLine <= 0)
                                letterHeightOldItemOnLine = rangeOldItemOnLine.size.height;
                            
                            float letterHeightNextItemOnLine = OCRResults::averageHeightTallLetters(nextItemOnLine);
                            if (letterHeightNextItemOnLine <= 0)
                                letterHeightNextItemOnLine = rangeNextItemOnLine.size.height;    
                            
                            float letterHeightItemBelowNextItemOnLine = OCRResults::averageHeightTallLetters(itemBelowNextItemOnLine);
                            if (letterHeightItemBelowNextItemOnLine <= 0)
                                letterHeightItemBelowNextItemOnLine = rangeItemBelowNextItemOnLine.size.height;                                                            

                            float heightRatioOldItemOnLine = letterHeightOldItemOnLine/letterHeightNextItemOnLine;
                            float heightRatioItemBelowNextItemOnLine = letterHeightItemBelowNextItemOnLine/letterHeightNextItemOnLine;
                            float gapItemBelowNextItemOnLine = (rectBottom(rangeItemBelowNextItemOnLine) + rectTop(rangeItemBelowNextItemOnLine))/2 - (rectBottom(rangeNextItemOnLine) + rectTop(rangeNextItemOnLine))/2 - 1 - ( letterHeightNextItemOnLine + letterHeightItemBelowNextItemOnLine)/2;
                            //gapRatioBelow = gapItemBelowNextItemOnLine / letterHeightNextItemOnLine;                                
                            
                            if ((OCRResults::matchesAligned(nextItemOnLine, itemBelowNextItemOnLine, ALIGNED_LEFT) || OCRResults::matchesAligned(nextItemOnLine, itemBelowNextItemOnLine, ALIGNED_CENTER))
                                // Item below next close below next item
                                && (gapItemBelowNextItemOnLine > 0) && (gapItemBelowNextItemOnLine < letterHeightNextItemOnLine * 1.20)
                                // Item below next and item next have similar heights
                                && (letterHeightItemBelowNextItemOnLine < letterHeightNextItemOnLine * 1.66) && (letterHeightItemBelowNextItemOnLine > letterHeightNextItemOnLine * 0.66)
                                // Item below next closer in height than oldItemOnLine
                                && (abs(heightRatioItemBelowNextItemOnLine-1) < abs(heightRatioOldItemOnLine-1))
                               ) {
                               // Just skip this next item and go to next line instead
                               DebugLog("setBlocks: not using [%s] as item to the right of [%s] because there is an item close below next item on line [%s]", toUTF8(getStringFromMatch(nextItemOnLine, matchKeyText)).c_str(), toUTF8(getStringFromMatch(oldItemOnLine, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemBelowNextItemOnLine, matchKeyText)).c_str());
                                nextItemOnLine = MatchPtr();
                            }
                        } // itemBelowNextItemOnLine != NULL
                   }
                }
    #endif // !OCR_ENTIRE_LINE_RELATED
                vloc++;
            }

            // If we get here it means we finally failed to find more items on current line - advance to next line
            // PQTODO need to advance block number when necessary but better do this after setBlock completes because there are cases when it helps to hev the entire next line before deciding if it's a new block
            vloc = 0;
            MatchPtr oldItemAtLineStart = itemAtLineStart;
            MatchVector::iterator pos = std::find(newMatches->begin(), newMatches->end(), oldItemAtLineStart);
            int indexAtLineStart = (int)(pos - newMatches->begin());
            if (pos == newMatches->end()) indexAtLineStart = -1;
            // Remove all items from this line from source matches
            matchesAddressBook.erase(std::find(matchesAddressBook.begin(), matchesAddressBook.end(), oldItemAtLineStart));
            for (int hh=indexAtLineStart+1; hh<newMatches->size(); hh++) {
                MatchPtr d =(*newMatches)[hh];
                MatchVector::iterator item = std::find(matchesAddressBook.begin(), matchesAddressBook.end(), d);
                if (item != matchesAddressBook.end())
                    matchesAddressBook.erase(item);
            }
            DebugLog("setBlocks: moving to next line after item [%s]", toUTF8(getStringFromMatch(oldItemAtLineStart,matchKeyText)).c_str());
            itemAtLineStart = this->findStartOfTopmostLine(matchesAddressBook, -1);
    #if DEBUG
            if (itemAtLineStart != NULL) {
                CGRect itemAtLineStartRect = OCRResults::matchRange(itemAtLineStart);
                ReplacingLog("setBlocks: continuing with item [%s] just below at index [%d] range [%d,%d - %d,%d]", toUTF8(getStringFromMatch(itemAtLineStart, matchKeyText)).c_str(), this->indexOfMatchInMatches(itemAtLineStart, matchesAddressBook), (int)itemAtLineStartRect.origin.x, (int)itemAtLineStartRect.origin.y, (int)(itemAtLineStartRect.origin.x + itemAtLineStartRect.size.width - 1), (int)(itemAtLineStartRect.origin.y + itemAtLineStartRect.size.height - 1));
    //            wstring txt = getStringFromMatch(itemAtLineStart, matchKeyText);
    //            if ((txt.length() >= 9) && (txt.substr(txt.length()-3,3) == L"987")) {
    //                DebugLog("setBlock: found it!");
    //            }
            }
    #endif
            line++;
        }
         
        // Replace matches with new one
        matchesAddressBook = *newMatches;
        
        //checkLines(matchesAddressBook);
    } // !adjustLinesOnly
    
    // Next pass adjusts line number by calculating spacing between lines - but it can't be done on bogus frames with no valid characters even for having a letter height! Note: the global averages are set based on sequences of 5+ digits or uppercase, meaning most bogus frames will have *some* letters yet still no global stats because they are scattered.
    if (globalStats.averageHeight.count > 0) {
       /* Everything is in one single block with incrementing line numbers throughout (ignoring blank lines). Need to set 2 things:
        1. Correctly separate items into blocks
        2. Have line numbers correctly indicate location of a line, including blank lines, i.e. lines separated by a blank line should have line numbers different by 2 (not 1). Note: line numbers will not reset at the start of a block
        */
        
        // Need to calculate the average slant on this image, as the factor by which to multiply a delta between X values to get the delta in their Y values. This will be needed later to calculate if there is an empty line between two lines without any X intersect.
        float slantsSum = 0, slantsWeights = 0;
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr dLineStart = matchesAddressBook[i];
            CGRect firstRect, lastRect;
            float lineHeight;   // Not used
            bool success = getLineInfo(i, -1, &firstRect, &lastRect, &lineHeight, &matchesAddressBook);
            if (success && (rectRight(firstRect) < rectLeft(lastRect))) {
                int nLetters = (rectRight(lastRect) - rectLeft(firstRect)) / this->globalStats.averageWidthDigits.average;
                if (nLetters >= 4) {
                    slantsWeights += nLetters;
                    slantsSum += nLetters * (rectTop(lastRect) - rectTop(firstRect)) / (rectRight(lastRect) - rectLeft(firstRect));
                }
            }
        }
        
        if ((slantsSum != 0) && (slantsWeights != 0)) {
            this->slant = slantsSum / slantsWeights;
            LayoutLog("setBlocks: found slant=%.2f", this->slant);
        }
#if DEBUG
        else {
            ErrorLog("setBlocks: ERROR, no slant [slantssSum=%.2f, slantsWeights=%.0f", slantsSum, slantsWeights);
        }
#endif
        
        int newlineNumberToSet = 0; // Last new line number set into the matches (new means after taking into account blank lines)
        int numNotSwappingIterations = 0;   // To prevent infinite loops, if set indicates we should NOT swap no matter what for this number of times through the loop
#if DEBUG
        float sumLineGaps = 0, sumLeftLineGaps = 0;
        int numLineGaps = 0, numLeftLineGaps = 0;
#endif
        // At each iteration we know to which new lines number to set the current line (because we already tested it against the previous line)
        // i points to the start of the line we call "top line" and next line is the one with which we need to calculate the gap
        // indexNexLineStart points to the start of the next line
        for (int i=0; i<matchesAddressBook.size(); i++) {
            if (numNotSwappingIterations >= 0) numNotSwappingIterations--;
            MatchPtr dTopLineStart = matchesAddressBook[i]; // We'll set this line to newLineNumberToSet
#if DEBUG
            int topOldLineNumber = getIntFromMatch(dTopLineStart, matchKeyLine);
#endif
            int indexNextLineStart = indexOfElementPastLineOfMatchAtIndex(i, &matchesAddressBook);
//#if DEBUG
//if ((indexNextLineStart >= 0) && (indexNextLineStart < matchesAddressBook.size())) {
//    MatchPtr itemOnNextLine = matchesAddressBook[indexNextLineStart];
//    wstring tmp = getStringFromMatch(itemOnNextLine, matchKeyText);
//    if (tmp.substr(0,8) == L"SUBTOTRL") {
//        DebugLog("Found it");
//    }
//}
//#endif
            if (i == 0)
                setNewLineNumberStartingAtIndex(i, newlineNumberToSet, &matchesAddressBook);
            
            if (indexNextLineStart < 0) {
                // No next line! We are done, bail out
                break;
            }
            
            int gapNumLines = -1;
            bool nextLineBogus = true;
            bool continueILoop = false; // If we break out of the "nextLineBogus", will indicate we need to continue i loop
            while (nextLineBogus) {
                LayoutLog("setBlock: testing gap between lines %d-%d [%s]-[%s]", topOldLineNumber, getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), toUTF8(getStringFromMatch(dTopLineStart,matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str());
                
#if DEBUG
                if ((getStringFromMatch(dTopLineStart,matchKeyText) == L"R-BLUEBERRIES 2 LB") && (getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText) == L"BANANAS"))
                    DebugLog("");
#endif

                nextLineBogus = false; // Until proven otherwise
                
                CGRect firstRectTop, lastRectTop, firstRectNext, lastRectNext;
                float topLineHeight = 0, nextLineHeight = 0;
                bool successTop = getLineInfo(i, indexNextLineStart-1, &firstRectTop, &lastRectTop, &topLineHeight, &matchesAddressBook);
                bool successBottom = getLineInfo(indexNextLineStart, -1, &firstRectNext, &lastRectNext, &nextLineHeight, &matchesAddressBook);
                if (successBottom && successTop) {
                    // We used to take the average between Y delta bween left rect of line above and left rect of line below, same for right rect (with slant adjustment) but that it wrong when say we have a long line above, starting at mid width, and a short line below aligned left: in this case if the receipt drops down on the right side, the line below doesn't get a chance to "present" its own (dropping below) letter because it has nothing there
                    float averageGap = 0;
#if DEBUG
                    float lineGapLeft = rectBottom(firstRectNext) - rectTop(firstRectTop) -1 - this->slant * (rectLeft(firstRectNext) - rectLeft(firstRectTop));
#endif
                    if (rectLeft(firstRectTop) > rectRight(lastRectNext)) {
                        // Top:          XXXXXXXX
                        // Next: XXXXXX
                        // Line above starts to the right of the end of the line below: just compare last below to first above
                        averageGap = rectBottom(lastRectNext) - rectTop(firstRectTop) - 1 + this->slant * (rectLeft(firstRectTop) - rectLeft(lastRectNext));
                    } else if (rectLeft(firstRectNext) > rectRight(lastRectTop)) {
                        // Top:  XXXXXXXX
                        // Next:           XXXXXX
                        // Line below starts to the right of the end of the line above: just compare last above to first below
                        averageGap = rectBottom(firstRectNext) - rectTop(lastRectTop) - 1 - this->slant * (rectLeft(firstRectNext) - rectLeft(lastRectTop));
                    } else if (abs(rectLeft(firstRectNext) - rectLeft(firstRectTop)) < abs(rectLeft(lastRectNext) - rectLeft(lastRectTop)) * 0.50) {
                        // Top:  XXXXXXXX
                        // Next:    XXXXXXXXXXXXXXXX
                        // Left sides much closer than right sides, ignore right sides
                        averageGap = rectBottom(firstRectNext) - rectTop(firstRectTop) - 1 - this->slant * (rectLeft(firstRectNext) - rectLeft(firstRectTop));
                    } else if (abs(rectLeft(lastRectNext) - rectLeft(lastRectTop)) <  abs(rectLeft(firstRectNext) - rectLeft(firstRectTop)) * 0.50) {
                        // Top:  XXXXXXXXXXXXXXXXX
                        // Next:            XXXX
                        // Right sides much closer than left sides, ignore left sides
                        averageGap = rectBottom(lastRectNext) - rectTop(lastRectTop) - this->slant * (rectLeft(lastRectNext) - rectLeft(lastRectTop));
                    } else {
                        float lineGapLeft = rectBottom(firstRectNext) - rectTop(firstRectTop) - 1 - this->slant * (rectLeft(firstRectNext) - rectLeft(firstRectTop));
                        float lineGapRight = rectBottom(lastRectNext) - rectTop(lastRectTop) - 1 - this->slant * (rectLeft(lastRectNext) - rectLeft(lastRectTop));
                        averageGap = (lineGapLeft + lineGapRight) / 2;
                    }
                    float referenceLineHeight = this->globalStats.averageHeight.average;
                    // 2016-05-30 removing all the below, we ALWAYS want to use letter height as our benchmark for line height EVEN between say two line with huge letters!
                    // Require heights to be within a reasonable margin of the global average line height: on one receipt is ranged from 17.5 to 23.5 (34% more)
                    // Because of some bogus matches we haven't been able to eliminate (e.g. "..'-JI"), need to be very careful NOT to consider the stats of the bogus one (likely noise). We used to take the average between the two, which was a mistake

                    LayoutLog("setBlocks: gap between lines %d-%d [%s]-[%s] is %.0f (reference height=%.1f [%%%.0f])", topOldLineNumber, getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), toUTF8(getStringFromMatch(dTopLineStart,matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str(),averageGap, referenceLineHeight, averageGap/referenceLineHeight*100);
                    
                    // IMPORTANT: in some cases of slants, we may find a highly negative gap: this means we need to switch these two lines, we got them in the wrong order
                    //TODO add code to prevent infinite loops bc of bug
                    if (averageGap < -(referenceLineHeight * (1 + this->retailerParams.lineSpacing))) {
                        if (numNotSwappingIterations >= 0) {
                            LayoutLog("setblocks: lines are switched AGAIN - refusing to swap (to prevent infinite loop)");
                            // Fall through, will set gapNumLines to negative, then correct to 0, and just increase newlineNumberToSet by 1
                        } else {
                            LayoutLog("setblocks: lines are SWITCHED");
                            OCRResults::swapLines(i, matchesAddressBook);
                            // Now reset i to point to the line above these two, so that we may correctly calculate the spacing with the new line just switched higher
                            if (i == 0) {
                                // Switched line is already the top line, just set it's new line value to 0 when we continue the loop
                                i = -1;
                                newlineNumberToSet = 0;
                                setNewLineNumberStartingAtIndex(0, newlineNumberToSet, &matchesAddressBook);
                                numNotSwappingIterations = 1; // To prevent us from swapping line 0 with the next one BACK (infinite loop)
                                continueILoop = true; break;
                            } else {
                                i = OCRResults::indexFirstElementOnLineAtIndex(i-1, &matchesAddressBook) - 1;
                                // Reset newLineNumberToSet
                                MatchPtr dd = matchesAddressBook[i+1];
                                newlineNumberToSet = getIntFromMatch(dd, matchKeyNewLine);
                                numNotSwappingIterations = 2; // To prevent us from swapping line above with either of the two lines that follow (infinite loop)
                                continueILoop = true; break;
                            }
                        }
                    }
                    // See https://www.pivotaltracker.com/story/show/110799254
                    
                    gapNumLines = (int)((referenceLineHeight + averageGap) / (referenceLineHeight * (1 + this->retailerParams.lineSpacing)) + 0.50) - 1;
                    if (gapNumLines < 0) {
                        LayoutLog("setBlock: WARNING, tried to set gapNumLines to %d, reseting to 0", gapNumLines);
                        // DANGER: there is a good chance the new line is problematic. Before just adding it as the next line (for lack of knowing what to do), consider just removing that entire line
                        // Can't check if there is nothing meaningful on this line when setBlock called before semantic analysis, just kill it
                        //if (!OCRResults::isMeaningfulLineAtIndex(indexNextLineStart, this)) {
                        // Remove!
                        int indexEndNextLine = OCRResults::indexLastElementOnLineAtIndex(indexNextLineStart, &this->matchesAddressBook);
#if DEBUG
                        if (indexEndNextLine == indexNextLineStart) {
                            LayoutLog("setBlocks: removing bogus line %d [index %d] with single element [%s]", getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), indexNextLineStart, toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str());
                        } else {
                            LayoutLog("setBlocks: removing bogus line %d [indices %d-%d] [%s]-[%s]", getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), indexNextLineStart, indexEndNextLine, toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexEndNextLine],matchKeyText)).c_str());
                        }
#endif
                        for (int pp=0; pp<indexEndNextLine - indexNextLineStart + 1; pp++) {
                        this->matchesAddressBook.erase(this->matchesAddressBook.begin()+indexNextLineStart);
                        }
                        // After removal of bogus line, indexNextLineStart ALREADY point where it should (next line), so just re-try this block
                        if (indexNextLineStart >= this->matchesAddressBook.size()) {
                            indexNextLineStart = -1;
                            break; // Out of bogus line loop, this will also exit the i loop
                        } else {
                            nextLineBogus = true;
                            continue; // Start at top of bogus line loop with new indexNextLineStart
                        }

                        // Didn't remove that suspicious line, continue but TODO need to continue to work on eliminating these lines for additional reasons
                    }
                    
    //#if DEBUG
    //                // Test alternate method for computing the gap between lines
    //                float gap;
    //                if (OCRResults::gapBetweenLines(indexNextLineStart-1, indexNextLineStart, gap, matchesAddressBook)) {
    //                    LayoutLog("setBlock: new gap method found gap=%.0f", gap);
    //                    if ((gap > averageGap * 1.5) || (gap < averageGap * 0.50)) {
    //                        ErrorLog("setBlock: new gap=%.0f very different than old gap=%.0f", gap, averageGap);
    //                        DebugLog("");
    //                    }
    //                }
    //#endif
                    
    #if DEBUG
                    if (gapNumLines == 0) {
                        sumLineGaps += averageGap/referenceLineHeight;
                        numLineGaps ++;
                    }
                    // Also keep track of the more accurate gap when both lines are aligned left
                    if ((lineGapLeft < referenceLineHeight * 0.50) && (overlapAmountRectsXAxisAnd(firstRectTop, firstRectNext))) {
                        sumLeftLineGaps += lineGapLeft/referenceLineHeight;
                        numLeftLineGaps ++;
                    }
    #endif
                } // got coordinates info for top & bottom lines and set gap between the lines
            } // bogus next line
            
            if (continueILoop) continue;
            
            if (indexNextLineStart < 0)
                break;
            
            if (gapNumLines >= 0) {
                newlineNumberToSet += 1 + gapNumLines;
#if DEBUG
                if (gapNumLines >= 1) {
                    if ((topOldLineNumber == 7) && (getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine) == 8)) {
                        DebugLog("");
                    }
                    LayoutLog("setBlock: found %d blank lines between lines %d-%d [%s]-[%s]", gapNumLines, topOldLineNumber, getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), toUTF8(getStringFromMatch(dTopLineStart,matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str());
                }
#endif
            }  else {
                LayoutLog("setBlock: warning, failed to get gap between lines %d-%d [%s]-[%s]", topOldLineNumber, getIntFromMatch(matchesAddressBook[indexNextLineStart], matchKeyLine), toUTF8(getStringFromMatch(dTopLineStart,matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchesAddressBook[indexNextLineStart],matchKeyText)).c_str());
                newlineNumberToSet++;   // Failed to get info on top or bottom lines, assume no blank line
            }
            
            
            setNewLineNumberStartingAtIndex(indexNextLineStart, newlineNumberToSet, &matchesAddressBook);
            i = indexNextLineStart - 1; // For loop will increment by 1
        } // Iterate over all matches
#if DEBUG
        LayoutLog("setBlocks: average line spacing=%.2f & average left side spacing=%.2f", sumLineGaps/numLineGaps, ((numLeftLineGaps > 0)? sumLeftLineGaps/numLeftLineGaps:0));
#endif
        // Now set lineNumber to newLineNumber for all matches
        for (int i=0; i<matchesAddressBook.size(); i++) {
            MatchPtr d = matchesAddressBook[i];
            int newLineNumber = getIntFromMatch(d, matchKeyNewLine);
            d[matchKeyLine] = SmartPtr<int>(new int(newLineNumber)).castDown<EmptyType>();
        }
    } // got average height for this set of results
    //checkLines(matchesAddressBook);
} // setBlocks

vector<int> OCRResults::elementsListWithTypeInlineAtIndex(int matchIndex, OCRElementType tp, MatchVector *matches) {
    vector<int> results;
    
    if ((matchIndex < 0) || (matchIndex >= matches->size()))
        return results;
    
    int lineStart = indexFirstElementOnLineAtIndex(matchIndex, matches);
    int lineEnd = indexLastElementOnLineAtIndex(matchIndex, matches);
    for (int i=lineStart; i<=lineEnd; i++) {
        MatchPtr d = (*matches)[i];
        if (getElementTypeFromMatch(d, matchKeyElementType) == tp) {
            results.push_back(i);
        }
    }
    
    return results;
}

// Returns -1 if not found
// If testOrder is set, will verify that the request type is found in the right order relative to other elements, according to the retailer format
int OCRResults::elementWithTypeInlineAtIndex(int matchIndex, OCRElementType tp, MatchVector *matches, retailer_t *retailerParams, bool testOrder) {
    if ((matchIndex >= matches->size()) || (matchIndex < 0)) {
        ErrorLog("elementWithTypeInlineAtIndex: ERROR, called with index=%d in matches array of size=%d", matchIndex, (int)matches->size());
        return -1;
    }
    MatchPtr d = (*matches)[matchIndex];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    int startLine = indexFirstElementForLine(currentLine, matches);
    int endLine = indexLastElementOnLineAtIndex(startLine, matches);
    
    if ((startLine < 0) || (endLine < 0) || (endLine < startLine)) {
        ErrorLog("elementWithTypeInlineAtIndex: ERROR, failed to find start/end for line=%d, got indices [%d - %d] instead", currentLine, startLine, endLine);
        return -1;
    }
    int bestIndex = -1; // Used only when looking for price
    unsigned long bestStatus = 0; // Used only when looking for price
    int productIndex = -1;  // Used for the sake of prices, to try to return the price situated correctly on the line relative to the product
    for (int j=startLine; j<=endLine; j++) {
        MatchPtr dd = (*matches)[j];
        if (getIntFromMatch(dd, matchKeyLine) != currentLine) {
            ErrorLog("elementWithTypeInlineAtIndex: ERROR, found line=%d in the middle of what we thought was line=%d between indices [%d - %d]", getIntFromMatch(dd, matchKeyLine), currentLine, startLine, endLine);
            return -1;
        }
        OCRElementType currentTp = getElementTypeFromMatch(dd, matchKeyElementType);
        if (currentTp == OCRElementTypeProductNumber) {
            productIndex = j;
        }
        if (currentTp == tp) {
            if (tp != OCRElementTypePrice)
                return (j);    // Found it, other than prices, we expect only one of each of the other types on any given line
            if (bestIndex < 0) {
                bestIndex = j;
                if (dd.isExists(matchKeyStatus))
                    bestStatus = getUnsignedLongFromMatch(dd, matchKeyStatus);
            } else {
                // Already have a bestIndex. Update only with valid reason, i.e. that we found another price with good status
                unsigned long status = 0;
                if (dd.isExists(matchKeyStatus))
                    status = getUnsignedLongFromMatch(dd, matchKeyStatus);
                if ((retailerParams != NULL) && (retailerParams->pricesAfterProduct) && (productIndex >= 0)
                    // Either j is the only one past the product, or it's closer to the product
                    && ((j > productIndex) && ((bestIndex < productIndex) || ((j - productIndex) < (bestIndex - productIndex))))) {
                    bestIndex = j;
                    bestStatus = status;
                } else if ((status & matchKeyStatusInPricesColumn)
                        && !(bestStatus & matchKeyStatusInPricesColumn)) {
                    bestIndex = j;
                    bestStatus = status;
                }
                // If prices are aligned right, since we scan from left to right, give priority to item to the right (higher index)
                else if ((retailerParams != NULL) && (retailerParams->productPricesAttributes & RETAILER_FLAG_ALIGNED_RIGHT)
                        && (!(bestStatus & matchKeyStatusInPricesColumn) || (status & matchKeyStatusInPricesColumn))
                        && (j > bestIndex)) {
                    bestIndex = j;
                    bestStatus = status;
                }
            }
        }
    }
    
    if (bestIndex >= 0) {
        if (tp == OCRElementTypePrice) {
            if ((productIndex > 0) && (retailerParams != NULL) && (retailerParams->pricesAfterProduct) && (bestIndex < productIndex)) {
                WarningLog("elementWithTypeInlineAtIndex: ERROR, found price [%s] at index=%d BEFORE a product at index [%d] on line=%d between indices [%d - %d]", toUTF8(getStringFromMatch((*matches)[bestIndex], matchKeyText)).c_str(), bestIndex, productIndex, currentLine, startLine, endLine);
                return -1;
            } else
                return bestIndex;
        } else
            return bestIndex;
    } else
        return -1;
} // elementWithTypeInlineAtIndex

// Return -1 if no next line, valid index of start of the next line otherwise
int OCRResults::indexOfElementPastLineOfMatchAtIndex(int matchIndex, MatchVector *matches) {
    if (matchIndex >= matches->size()) {
        ErrorLog("indexOfElementPastLineOfMatchAtIndex: ERROR, called with index=%d in matches array of size=%d", matchIndex, (int)matches->size());
        return -1;
    }
    MatchPtr d = (*matches)[matchIndex];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    for (int j=matchIndex+1; j<matches->size(); j++) {
        if (getIntFromMatch((*matches)[j], matchKeyLine) != currentLine)
            return j;
    }
    return -1;
} // indexOfElementPastLineOfMatchAtIndex


int OCRResults::indexFirstElementOnLineAtIndex(int indexOnLine, MatchVector *matches) {
    if ((indexOnLine >= matches->size()) || (indexOnLine < 0)) {
        ErrorLog("indexFirstElementOnLineAtIndex: ERROR, called with index=%d in matches array of size=%d", indexOnLine, (int)matches->size());
        return -1;
    }
    MatchPtr d = (*matches)[indexOnLine];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    int resultIndex = indexOnLine;
    for (int j=indexOnLine-1; j>=0; j--) {
        if (getIntFromMatch((*matches)[j], matchKeyLine) != currentLine)
            break;
        else
            resultIndex = j;
    }
    return resultIndex;
} // indexFirstElementOnLineAtIndex

int OCRResults::indexLastElementOnLineAtIndex(int lineStartIndex, MatchVector *matches) {
    if (lineStartIndex >= matches->size()) {
        ErrorLog("indexLastElementOnLine: ERROR, called with index=%d in matches array of size=%d", lineStartIndex, (int)matches->size());
        return -1;
    }
    MatchPtr d = (*matches)[lineStartIndex];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    int indexLastElement = lineStartIndex;
    for (int j=lineStartIndex+1; j<matches->size(); j++) {
        if (getIntFromMatch((*matches)[j], matchKeyLine) != currentLine)
            break;
        else
            indexLastElement = j;
    }
    return indexLastElement;
}

int OCRResults::indexFirstElementForLine(int lineNumber, MatchVector *matches) {
    for (int i=0; i<matches->size(); i++) {
        MatchPtr d = (*matches)[i];
        int currentLine = getIntFromMatch(d, matchKeyLine);
        if (currentLine == lineNumber) {
            return i;
        }
    }
    return -1;
} // indexFirstElementForLine

// Returns -1 if lineNumber is greater than anything in matches (i.e. need to insert at the end)
int OCRResults::indexJustBeforeLine(int lineNumber, MatchVector *matches) {
    for (int i=0; i<matches->size(); i++) {
        MatchPtr d = (*matches)[i];
        int currentLine = getIntFromMatch(d, matchKeyLine);
        if (currentLine >= lineNumber) {
            return i;
        }
    }
    return -1;
} // indexJustBeforeLine

void OCRResults::setLineNumberStartingAtIndex(int matchIndex, int lineNumber, MatchVector *matches) {
    if (matchIndex >= matches->size()) {
        ErrorLog("setLineNumberStartingAtIndex: ERROR, called with index=%d in matches array of size=%d", matchIndex, (int)matches->size());
        return;
    }
    MatchPtr d = (*matches)[matchIndex];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    for (int j=matchIndex; j<matches->size(); j++) {
        if (getIntFromMatch((*matches)[j], matchKeyLine) != currentLine)
            break;
        (*matches)[j][matchKeyLine] = SmartPtr<int>(new int(lineNumber)).castDown<EmptyType>();
    }
}

void OCRResults::setNewLineNumberStartingAtIndex(int matchIndex, int lineNumber, MatchVector *matches) {
    if (matchIndex >= matches->size()) {
        ErrorLog("setNewLineNumberStartingAtIndex: ERROR, called with index=%d in matches array of size=%d", matchIndex, (int)matches->size());
        return;
    }
    MatchPtr d = (*matches)[matchIndex];
    int currentLine = getIntFromMatch(d, matchKeyLine);
    for (int j=matchIndex; j<matches->size(); j++) {
        if (getIntFromMatch((*matches)[j], matchKeyLine) != currentLine)
            break;
        (*matches)[j][matchKeyNewLine] = SmartPtr<int>(new int(lineNumber)).castDown<EmptyType>();
    }
}

bool OCRResults::gapBetweenLines (int indexLineTop, int indexLineBottom, float &gap, MatchVector &matches) {
    if ((indexLineTop < 0) || (indexLineTop > (int)matches.size() - 1) || (indexLineBottom < 0) || (indexLineTop > (int)matches.size() - 1) || (indexLineBottom <= indexLineTop))
        return false;
    
//#if DEBUG
//    if ((indexLineTop == 2) && (indexLineBottom == 3)) {
//        DebugLog("");
//    }
//#endif
    
    int indexStartTopLine = OCRResults::indexFirstElementOnLineAtIndex(indexLineTop, &matches);
    int indexEndTopLine = OCRResults::indexLastElementOnLineAtIndex(indexLineTop, &matches);
    int indexStartBottomLine = OCRResults::indexFirstElementOnLineAtIndex(indexLineBottom, &matches);
    int indexEndBottomLine = OCRResults::indexLastElementOnLineAtIndex(indexLineBottom, &matches);
    
    // Find leftmost usable item on line below. The variable below always point to the place to compare with rects above
    int currentItemBottomIndex = indexStartBottomLine;
    MatchPtr currentItemBottom;         // The match being used on line below
    wstring currentItemBottomText;      // Text of the item below
    CGRect currentItemBottomRect;       // The range of that match being used on line below
    int currentItemBottomNRects = 0;    // The number of rects & letter in that match
    int currentItemBottomRectIndex = -1; // Current index into rects/letters
    SmartPtr<vector<SmartPtr<CGRect> > > currentItemBottomRects;
    
    bool abort = false; // Set to TRUE when we exhausted the line below
    vector<float> gapsArray;
    
    // Iterate over all items of top line
    for (int i=indexStartTopLine; i <= indexEndTopLine; i++) {
        MatchPtr itemAbove = matches[i];
        
        if (!itemAbove.isExists(matchKeyRects) || !itemAbove.isExists(matchKeyN))
            continue;

        size_t nRects = getIntFromMatch(itemAbove, matchKeyN);
        wstring itemAboveText = getStringFromMatch(itemAbove, matchKeyText);
        if (itemAboveText.length() != nRects) {
            continue;
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = itemAbove[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        
        for (int j=0; j<nRects; j++) {
            wchar_t ch = itemAboveText[j];
            if (!(isUpper(ch) || isDigit(ch)))
                continue;

            CGRect rectAbove = *(*rects)[j];
            
            // If we don't yet have a suitable match below, find one!
            if (currentItemBottomRectIndex < 0) {
                // Now try to find first match in the line below with a X overlap such that they may contribute gap values with letters/digits in itemAbove
                for (; currentItemBottomIndex <= indexEndBottomLine; currentItemBottomIndex++) {
                    currentItemBottom = matches[currentItemBottomIndex];
                    if (!currentItemBottom.isExists(matchKeyRects) || !currentItemBottom.isExists(matchKeyN))
                        continue;
                    currentItemBottomNRects = getIntFromMatch(currentItemBottom, matchKeyN);
                    currentItemBottomText = getStringFromMatch(currentItemBottom, matchKeyText);
                    if (currentItemBottomText.length() != currentItemBottomNRects) {
                        // Inconsistent item, skip it
                        continue;
                    }
                    currentItemBottomRect = OCRResults::matchRange(currentItemBottom);
                    if (OCRResults::overlappingRectsXAxisAnd(currentItemBottomRect, rectAbove)) {
                        // Found it!
                        currentItemBottomRectIndex = 0;
                        currentItemBottomRects = currentItemBottom[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
                        // All other 4 variables for current bottom item are set
                        break;
                    } else if (rectLeft(currentItemBottomRect) > rectRight(rectAbove)) {
                        // Next usable item below is past current rect above. We need to stop iterating over items below, and no need to iterate over its rects
                        currentItemBottomRectIndex = 0;
                        currentItemBottomRects = currentItemBottom[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
                        // All other 4 variables for current bottom item are set
                        break;
                    }
                } // look for first suitable
            } // don't have suitable item below
            
            if (currentItemBottomRectIndex < 0) {
                // No overlapping item below, and no item below which could overlap with future rects above (meaning line below ends to the left of current rect above). We are done.
                abort = true;
            } else {
                // We have a current overlapping item below, iterate over its rects to find X overlap
                for (; currentItemBottomRectIndex < currentItemBottomNRects; currentItemBottomRectIndex++) {
                    wchar_t chBelow = currentItemBottomText[currentItemBottomRectIndex];
                    if (!(isUpper(chBelow) || isDigit(chBelow)))
                        continue;
                    CGRect rectBelow = *(*currentItemBottomRects)[currentItemBottomRectIndex];
                    if (OCRResults::overlappingRectsXAxisAnd(rectAbove, rectBelow)) {
                        // Found a match between chars above & below!
                        float gap = rectBottom(rectBelow) - rectTop(rectAbove);
                        gapsArray.push_back(gap);
                        break;
                    } else if (rectLeft(rectBelow) > rectRight(rectAbove)) {
                        // Stop iterating so that current rect below may be used for next rects above later
                        break;
                    }
                } // Iterate over the rects of the item below
                if (currentItemBottomRectIndex >= currentItemBottomNRects) {
                    // We exhausted this match, time to move on to the next matches below, if any
                    currentItemBottomIndex++;
                    currentItemBottomRectIndex = -1;
                }
            } // Have suitable item below
        } // for j, iterate over rects of item above
        
        if (abort)
            break;
    } // for i, iterate over items of line above
    
    if (gapsArray.size() == 0) {
        gap = 0;
        return false;
    }
    
    // Calculate average gap
    float sumGaps = 0;
    for (int i=0; i<gapsArray.size(); i++) {
        sumGaps += gapsArray[i];
    }
    gap = sumGaps/gapsArray.size();
    return true;
} // gapBetweenLines

#define NEWGETLINEINFO 1

#if NEWGETLINEINFO

// lineEndIndex (optional) is necessary because setBlocks calls this function while the top line ALREADY may have had its line incremented by 1. If so, it would make its line number the same as the line below, we we would erroneously go all the way to the end of THAT line ...
bool OCRResults::getLineInfo(int lineStartIndex, int lineEndIndex, CGRect *firstRect, CGRect *lastRect, float *height, MatchVector *matches) {
    if (lineStartIndex >= matches->size()) {
        ErrorLog("getLineInfo: ERROR, called with index=%d in matches array of size=%d", lineStartIndex, (int)matches->size());
        return false;
    }
    
//#if DEBUG
//    if (lineStartIndex == 14) {
//        DebugLog("");
//    }
//#endif
    
    bool needToCalculateLineHeight = true, useText = true;
    bool foundFirstRect = false, foundLastRect = false;
    // First get range of indexes for this line
    MatchPtr dStartLine = (*matches)[lineStartIndex];
    if (lineEndIndex < 0)
        lineEndIndex = indexLastElementOnLineAtIndex(lineStartIndex, matches);
    // OK, now get the rect for the first uppercase/digit on that line
    // Also, compute the average height of uppercase & digits
    float sumHeights = 0;
    int numHeights = 0;
    if (dStartLine.isExists(matchKeyAverageHeightTallLetters)) {
        *height = getFloatFromMatch(dStartLine, matchKeyAverageHeightTallLetters);
        needToCalculateLineHeight = false;
    }
    
    int numTries = 3;   // We are looking to pick the tallest from the first 3 qualifying items
    float tallestHeight = 0;
    for (int j=lineStartIndex; (j<=lineEndIndex) && (!foundFirstRect || (numTries > 0) || needToCalculateLineHeight); j++) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            if (!needToCalculateLineHeight) {
                useText = false;
            } else {
                ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s] and no tall letters height - failing", (int)nRects, toUTF8(itemText).c_str());
                return false;
            }
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=0; k<nRects && k<rects->size(); k++) {
            bool useThisChar = false;
            CGRect currentRect = *(*rects)[k];
            if (useText) {
                wchar_t ch = itemText[k];
                useThisChar = (isUpper(ch) || isDigit(ch) || (isTallAbove(ch) && !isTallBelow(ch)));
            } else {
                useThisChar = ((currentRect.size.height > (*height) * 0.85) && ((currentRect.size.height < (*height) * 1.15)));
            }
            if (useThisChar) {
                if (!foundFirstRect || (currentRect.size.height > tallestHeight)) {
                    // Found it!
                    *firstRect = currentRect;
                    foundFirstRect = true;
                    tallestHeight = currentRect.size.height;
                }
                numTries--;
                if (needToCalculateLineHeight) {
                    sumHeights += (*(*rects)[k]).size.height;
                    numHeights++;
                } else if (numTries == 0)
                    break;
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    // Set in match so we have it ready for next time
    if (needToCalculateLineHeight) {
        // Ignore average height derived from just a few letters, could be an abberation
        if (numHeights > 4) {
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(sumHeights/numHeights)).castDown<EmptyType>();
            *height = sumHeights/numHeights;
        } else
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(0)).castDown<EmptyType>();
    }
    
    // Now get the rect for the last uppercase/digit on that line
    numTries = 3;   // We are looking to pick the tallest from the first 3 qualifying items
    tallestHeight = 0;
    for (int j=lineEndIndex; (j>=lineStartIndex) && (!foundLastRect || (numTries > 0)); j--) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            if (!needToCalculateLineHeight) {
                useText = false;
            } else {
                ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s] and no tall letters height - failing", (int)nRects, toUTF8(itemText).c_str());
                return false;
            }
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=(int)rects->size() - 1; (k >= 0) && k<rects->size(); k--) {
            bool useThisChar = false;
            CGRect currentRect = *(*rects)[k];
            if (useText) {
                wchar_t ch = itemText[k];
                useThisChar = (isUpper(ch) || isDigit(ch)); // Why test different that the first rect loop prior?
            } else {
                useThisChar = ((currentRect.size.height > (*height) * 0.85) && ((currentRect.size.height < (*height) * 1.15)));
            }
            if (useThisChar) {
                // Found it!
                if (!foundLastRect || (currentRect.size.height > tallestHeight)) {
                    *lastRect = currentRect;
                    foundLastRect = true;
                    tallestHeight = currentRect.size.height;
                    if (--numTries == 0)
                        break;
                }
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    if (foundFirstRect && foundLastRect)
        return true;
    else
        return false;
} // getLineInfo

#else
// lineEndIndex (optional) is necessary because setBlocks calls this function while the top line ALREADY may have had its line incremented by 1. If so, it would make its line number the same as the line below, we we would erroneously go all the way to the end of THAT line ...
bool OCRResults::getLineInfo(int lineStartIndex, int lineEndIndex, CGRect *firstRect, CGRect *lastRect, float *height, MatchVector *matches) {
    if (lineStartIndex >= matches->size()) {
        ErrorLog("getLineInfo: ERROR, called with index=%d in matches array of size=%d", lineStartIndex, (int)matches->size());
        return false;
    }
    bool needToCalculateLineHeight = true;
    bool foundFirstRect = false, foundLastRect = false;
    // First get range of indexes for this line
    MatchPtr dStartLine = (*matches)[lineStartIndex];
    if (lineEndIndex < 0)
        lineEndIndex = indexLastElementOnLineAtIndex(lineStartIndex, matches);
    // OK, now get the rect for the first uppercase/digit on that line
    // Also, compute the average height of uppercase & digits
    float sumHeights = 0;
    int numHeights = 0;
    if (dStartLine.isExists(matchKeyAverageHeightTallLetters)) {
        *height = getFloatFromMatch(dStartLine, matchKeyAverageHeightTallLetters);
        needToCalculateLineHeight = false;
    }
    
    for (int j=lineStartIndex; (j<=lineEndIndex) && (!foundFirstRect || needToCalculateLineHeight); j++) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
            return false;
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=0; k<nRects && k<rects->size(); k++) {
            wchar_t ch = itemText[k];
            if (isUpper(ch) || isDigit(ch) || (isTallAbove(ch) && !isTallBelow(ch))) {
                if (!foundFirstRect) {
                    // Found it!
                    *firstRect = *(*rects)[k];
                    foundFirstRect = true;
                }
                if (needToCalculateLineHeight) {
                    sumHeights += (*(*rects)[k]).size.height;
                    numHeights++;
                } else
                    break;
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    // Set in match so we have it ready for next time
    if (needToCalculateLineHeight) {
        // Ignore average height derived from just a few letters, could be an abberation
        if (numHeights > 4) {
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(sumHeights/numHeights)).castDown<EmptyType>();
            *height = sumHeights/numHeights;
        } else
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(0)).castDown<EmptyType>();
    }
    
    // Now get the rect for the last uppercase/digit on that line
    for (int j=lineEndIndex; (j>=lineStartIndex) && !foundLastRect; j--) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
            return false;
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=(int)rects->size() - 1; (k >= 0) && k<rects->size(); k--) {
            wchar_t ch = itemText[k];
            if (isUpper(ch) || isDigit(ch)) {
                // Found it!
                *lastRect = *(*rects)[k];
                foundLastRect = true;
                break;
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    if (foundFirstRect && foundLastRect)
        return true;
    else
        return false;
} // getLineInfo
#endif // !NEWGETLINEINFO

// lineEndIndex (optional) is necessary because setBlocks calls this function while the top line ALREADY may have had its line incremented by 1. If so, it would make its line number the same as the line below, we we would erroneously go all the way to the end of THAT line ...
bool OCRResults::getLineInfoIgnoringLetters(int lineStartIndex, int lineEndIndex, CGRect *firstRect, CGRect *lastRect, float *height, MatchVector *matches) {
    if (lineStartIndex >= matches->size()) {
        ErrorLog("getLineInfo: ERROR, called with index=%d in matches array of size=%d", lineStartIndex, (int)matches->size());
        return false;
    }
    bool needToCalculateLineHeight = true;
    bool foundFirstRect = false, foundLastRect = false;
    // First get range of indexes for this line
    MatchPtr dStartLine = (*matches)[lineStartIndex];
    if (lineEndIndex < 0)
        lineEndIndex = indexLastElementOnLineAtIndex(lineStartIndex, matches);
    // OK, now get the rect for the first uppercase/digit on that line
    // Also, compute the average height of uppercase & digits
    float sumHeights = 0;
    int numHeights = 0;
    if (dStartLine.isExists(matchKeyAverageHeightTallLetters)) {
        *height = getFloatFromMatch(dStartLine, matchKeyAverageHeightTallLetters);
        needToCalculateLineHeight = false;
    }
    
    for (int j=lineStartIndex; (j<=lineEndIndex) && (!foundFirstRect || needToCalculateLineHeight); j++) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
            return false;
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=0; k<nRects && k<rects->size(); k++) {
            wchar_t ch = itemText[k];
            if (isUpper(ch) || isDigit(ch) || (isTallAbove(ch) && !isTallBelow(ch))) {
                if (!foundFirstRect) {
                    // Found it!
                    *firstRect = *(*rects)[k];
                    foundFirstRect = true;
                }
                if (needToCalculateLineHeight) {
                    sumHeights += (*(*rects)[k]).size.height;
                    numHeights++;
                } else
                    break;
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    // Set in match so we have it ready for next time
    if (needToCalculateLineHeight) {
        // Ignore average height derived from just a few letters, could be an abberation
        if (numHeights > 4) {
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(sumHeights/numHeights)).castDown<EmptyType>();
            *height = sumHeights/numHeights;
        } else
            dStartLine[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(0)).castDown<EmptyType>();
    }
    
    // Now get the rect for the last uppercase/digit on that line
    for (int j=lineEndIndex; (j>=lineStartIndex) && !foundLastRect; j--) {
        MatchPtr d = (*matches)[j];
        if (!(d.isExists(matchKeyRects)) || !(d.isExists(matchKeyN))) {
            ErrorLog("getLineInfo: ERROR, no rects for item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
            return false;
        }
        size_t nRects = *d[matchKeyN].castDown<size_t>();
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (itemText.length() != nRects) {
            ErrorLog("getLineInfo: ERROR, mismatch between nRect=%d and text length for [%s]", (int)nRects, toUTF8(itemText).c_str());
            return false;
        }
        
        SmartPtr<vector<SmartPtr<CGRect> > > rects = d[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
        for (int k=(int)rects->size() - 1; (k >= 0) && k<rects->size(); k--) {
            wchar_t ch = itemText[k];
            if (isUpper(ch) || isDigit(ch)) {
                // Found it!
                *lastRect = *(*rects)[k];
                foundLastRect = true;
                break;
            }
        } // iterate over rects
    } // iterate over all matches on the line
    
    if (foundFirstRect && foundLastRect)
        return true;
    else
        return false;
} // getLineInfoIgnoringLetters

float OCRResults::heightTallestChar(MatchPtr m) {
	if ((m == NULL) || !(m.isExists(matchKeyRects)) || !(m.isExists(matchKeyN)))
		return 0;
    
    // Need to calculate
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	size_t nRects = *m[matchKeyN].castDown<size_t>();
    if (nRects < 1)
        return 0;
    
    float highest = -1;
	for (int i=0; i<nRects && i<rects->size(); i++) {
		CGRect r = *(*rects)[i];
        if (r.size.height > highest)
            highest = r.size.height;
    }
	
    return highest;
} // heightTallestChar


// Calculates the Y overlap between the last letter of m1 and the first letter of m1: this is more reliable when (long) lines are slanted, because m2 could start entirely BELOW m1 yet end ABOVE m2 (i.e. 100% Y overlap, by error)
// To cater for cases where a match starts/ends with an abnormally low digit or letter, look up to 3 chars back to pick the tallest
bool OCRResults::yOverlapFacingSides(MatchPtr m1, MatchPtr m2, float &overlap, float &percent, bool useSlant) {
	if ((m1 == NULL) || !(m1.isExists(matchKeyRects)) || !(m1.isExists(matchKeyN)))
		return false;
	if ((m2 == NULL) || !(m2.isExists(matchKeyRects)) || !(m2.isExists(matchKeyN)))
		return false;
    
	SmartPtr<vector<SmartPtr<CGRect> > > rects1 = m1[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	int nRects1 = (int)(*m1[matchKeyN].castDown<size_t>());
    if (nRects1 < 1)
        return false;
	SmartPtr<vector<SmartPtr<CGRect> > > rects2 = m2[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	int nRects2 = (int)(*m2[matchKeyN].castDown<size_t>());
    if (nRects2 < 1)
        return false;
    
    wstring itemText1;
    wstring itemText2;
    
#if DEBUG
    bool gotOrigText1 = false, gotOrigText2 = false;
#endif

    // Check if we have the original text. If so, use it for the height calculation, better in case we corrected letters
    if (m1.isExists(matchKeyOrigText)) {
        itemText1 = getStringFromMatch(m1, matchKeyOrigText);
#if DEBUG
        gotOrigText1 = true;
#endif
    } else {
        itemText1 = getStringFromMatch(m1, matchKeyText);
    }
    if (m2.isExists(matchKeyOrigText)) {
        itemText2 = getStringFromMatch(m2, matchKeyOrigText);
#if DEBUG
        gotOrigText2 = true;
#endif
    } else {
        itemText2 = getStringFromMatch(m2, matchKeyText);
    }
                       
    // Abort if rects count doesn't match letter count!
    if (nRects1 != itemText1.length()) {
#if DEBUG
        DebugLog("OCRGlobalResults::averageHeightTallLetters: aborting parsing of [%s] nRects=[%d] vs string length=[%d] (got orig text:%s)", toUTF8(itemText1).c_str(), (unsigned short)nRects1, (unsigned short)itemText1.length(), (gotOrigText1? "Yes":"No"));
#endif
        return false;
    }
    if (nRects2 != itemText2.length()) {
#if DEBUG
        DebugLog("OCRGlobalResults::averageHeightTallLetters: aborting parsing of [%s] nRects=[%d] vs string length=[%d] (got orig text:%s)", toUTF8(itemText2).c_str(), (unsigned short)nRects2, (unsigned short)itemText2.length(), (gotOrigText2? "Yes":"No"));
#endif
        return false;
    }
    
    float height1 = OCRResults::averageHeightTallLetters(m1);
    float height2 = OCRResults::averageHeightTallLetters(m2);
    float averageHeightTallLettersAverage = -1;
    if ((height1 > 0) && (height2 > 0))
        averageHeightTallLettersAverage = (height1 * nRects1 + height2 * nRects2) / (nRects1 + nRects2);
    else if (height1 > 0)
        averageHeightTallLettersAverage = height1;
    else if (height2 > 0)
        averageHeightTallLettersAverage = height2;
    
    // Find last uppercase / digit in m1
    CGRect lastM1Rect;
    bool foundRelevantRectM1 = false;
    int tries = 3;
    float lastM1Height = -1;
    for (int i=nRects1-1; i>=0; i--) {
        wchar_t ch = itemText1[i];
        if (isDigit(ch) || isUpper(ch)) {
            CGRect currentRect = *(*rects1)[i];
            if ((lastM1Height < 0)
                // New rect has normal height and previous candidate was too tall
                || ((lastM1Height > averageHeightTallLettersAverage * 1.15) &&  (currentRect.size.height < averageHeightTallLettersAverage * 1.15))
                // New rect has normal height and previous candidate was too short
                || ((lastM1Height < averageHeightTallLettersAverage * 0.85) &&  (currentRect.size.height > averageHeightTallLettersAverage * 0.85))
                // Both too high but new shorter
                || ((lastM1Height > averageHeightTallLettersAverage * 1.15) &&  (currentRect.size.height > averageHeightTallLettersAverage * 1.15) && (currentRect.size.height < lastM1Height))) {
                lastM1Rect = currentRect;
                lastM1Height = currentRect.size.height;
                foundRelevantRectM1 = true;
            }
            if (--tries == 0)
                break;
        }
    }
    if (!foundRelevantRectM1) {
        lastM1Rect = OCRResults::matchRange(m1);
    }
    
    // Find first uppercase / digit in m2
    CGRect firstM2Rect;
    bool foundRelevantRectM2 = false;
    tries = 3;
    float lastM2Height = -1;
    for (int i=0; i<nRects2; i++) {
        wchar_t ch = itemText2[i];
        if (isDigit(ch) || isUpper(ch)) {
            CGRect currentRect = *(*rects2)[i];
            if ((lastM2Height < 0)
                // New rect has normal height and previous candidate did not
                || ((lastM2Height > averageHeightTallLettersAverage * 1.15) &&  (currentRect.size.height < averageHeightTallLettersAverage * 1.15))
                // New rect has normal height and previous candidate was too short
                || ((lastM2Height < averageHeightTallLettersAverage * 0.85) &&  (currentRect.size.height > averageHeightTallLettersAverage * 0.85))
                // Both too high but new shorter
                || ((lastM2Height > averageHeightTallLettersAverage * 1.15) &&  (currentRect.size.height > averageHeightTallLettersAverage * 1.15) && (currentRect.size.height < lastM2Height))) {
                firstM2Rect = currentRect;
                lastM2Height = currentRect.size.height;
                foundRelevantRectM2 = true;
            }
            if (--tries == 0)
                break;
        }
    }
    if (!foundRelevantRectM2) {
        firstM2Rect = OCRResults::matchRange(m2);
    }
    
    if (useSlant) {
        // Only if we had enough letters and digits to work with to compute the slants
        int nDigitsOrLettersM1 = countLettersOrDigits(itemText1);
        int nDigitsOrLettersM2 = countLettersOrDigits(itemText2);
        if (nDigitsOrLettersM1 + nDigitsOrLettersM2 > 10) {
            float slant1 = OCRResults::matchSlant(m1);
            float slant2 = OCRResults::matchSlant(m2);
            // If items indicate conflicting slants, ignore slants! See https://www.pivotaltracker.com/story/show/109205392 frame #4
            if (slant1 * slant2 > 0) {
                // 2/12/215 Compute weighted average so that a short messed up item doesn't throw us totally off
                float slant = (slant1 * nDigitsOrLettersM1 + slant2 * nDigitsOrLettersM2) / (nDigitsOrLettersM1 + nDigitsOrLettersM2);
                // Adjust firstM2Rect
                float midPointRect1 = lastM1Rect.origin.x + lastM1Rect.size.width/2 - 1;
                float midPointRect2 = firstM2Rect.origin.x + firstM2Rect.size.width/2 - 1;
                float delta = (midPointRect2 - midPointRect1) * slant;
                firstM2Rect.origin.y -= delta; // Adjust again the slant, to get item to the right to be on an imaginary straightened line
            }
        }
    }
    
    overlap = overlapAmountRectsYAxisAnd(lastM1Rect, firstM2Rect, 0.10);
    
    if ((height1 > 0) || (height1 > 0))
        percent = overlap / ((height1 + height2) / 2);
        
    return true;
} // yOverlapFacingSides

#define NEWTALLLETTERS 0

#if !NEWTALLLETTERS
// 11-19-2015 retiring the below implementation because:
// 1. It is not accurate considering we use a whitelist that excludes lowercase letters => many uppercase or digits will mislead us by having their height included in the stats when in reality they are a lowercase letter
// 2. It is not accurate when the OCR makes mistakes and maps broken/partial patterns to an uppercase letter
// 3. It aborts after semantics parsing when autocorrect changes the number of characters without adjusting rects
// Instead we will now simply go over all rects to determine the highest height (excluding dollar signs)

float OCRResults::averageHeightTallLetters(MatchPtr m) {
	if ((m == NULL) || !(m.isExists(matchKeyRects)) || !(m.isExists(matchKeyN)))
		return 0;
    
    // Do we have it cached?
	SmartPtr<float> obj = m[matchKeyAverageHeightTallLetters].castDown<float>();
	if (!obj.isNull()) {
		return *obj;
    }
    
    // Need to calculate
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	size_t nRects = *m[matchKeyN].castDown<size_t>();
    if (nRects < 1)
        return 0;
    wstring itemText = getStringFromMatch(m, matchKeyText);
#if DEBUG
    bool gotOrigText = false;
#endif
    // Check if we have the original text. If so, use it for the height calculation, better in case we corrected letters
    if (m.isExists(matchKeyOrigText)) {
        itemText = getStringFromMatch(m, matchKeyOrigText);
#if DEBUG
        gotOrigText = true;
#endif
    } else {
        itemText = getStringFromMatch(m, matchKeyText);
    }
                       
    // Abort if rects count doesn't match letter count! This means any attempt to identify tall letters is futile
    if (nRects != itemText.length()) {
#if DEBUG
        DebugLog("OCRGlobalResults::averageHeightTallLetters: aborting parsing of [%s] nRects=[%d] vs string length=[%d] (got orig text:%s)", toUTF8(itemText).c_str(), (unsigned short)nRects, (unsigned short)itemText.length(), (gotOrigText? "Yes":"No"));
#endif
        return 0;
    }
                       
	float totalHeight = 0;
    float totalHeightAllRects = 0;
    float totalHeightAllLetters = 0;
    int nRelevant = 0;
    int nLetters = 0;
    int nAllRects = 0;
	for (int i=0; (i<nRects) && (i<itemText.length()); i++) {
        wchar_t ch = itemText[i];
		CGRect r = *(*rects)[i];
		if (isDigit(ch) || isUpper(ch) || (isLower(ch) && isTallAbove(ch) && !isTallBelow(ch))) {
			nRelevant++;
            totalHeight += r.size.height;
        } else if (nRelevant == 0) {
            if ((totalHeightAllLetters <= 0) && (ch != ' ')) {
                totalHeightAllRects += r.size.height;
                nAllRects++;
            }
            if (isLetterOrDigit(ch)) {
                totalHeightAllLetters += r.size.height;
                nLetters++;
            }
        }
	}
    
    float av;
    if (nRelevant != 0)
        av = totalHeight / nRelevant;
    else if (nLetters != 0)
        av = totalHeightAllLetters / nLetters;
    else if (nAllRects > 0)
        av = totalHeightAllRects / nAllRects;
    else
        av = 0;
    obj = SmartPtr<float>(new float(av));
    m[matchKeyAverageHeightTallLetters] = obj.castDown<EmptyType>();
	
    return av;
} // averageHeighTallLetters

#else
/*
    Challenges:
    - $ sign is above and below other letters (taller) and would increase the height by 21% is included (on one receipt $ height was 23 versus digit height 19). Can be detected by the fact next char is above $ baseline AND below $ top
    - a single abnormally tall character could hurt us if we decided that is the tall letter height. We would find only one char of similar height (that one), all others would be deemed too much smaller
    - punctuation poses the challenge of presenting us with small chars, which we shouldn't confuse with lowercase chars
 */
float OCRResults::averageHeightTallLetters(MatchPtr m) {
	if ((m == NULL) || !(m.isExists(matchKeyRects)) || !(m.isExists(matchKeyN)))
		return 0;
    
    // Do we have it cached?
	SmartPtr<float> obj = m[matchKeyAverageHeightTallLetters].castDown<float>();
	if (!obj.isNull()) {
		return *obj;
    }
    
    // Need to calculate
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	size_t nRects = *m[matchKeyN].castDown<size_t>();
    if (nRects < 1)
        return 0;
    
    // First pass: determine what is
                       
	float totalHeight = 0;
    float totalHeightAllRects = 0;
    float totalHeightAllLetters = 0;
    int nRelevant = 0;
    int nLetters = 0;
    int nAllRects = 0;
	for (int i=0; i<nRects; i++) {
		CGRect r = *(*rects)[i];
		if (isDigit(ch) || isUpper(ch) || (isLower(ch) && isTallAbove(ch) && !isTallBelow(ch))) {
			nRelevant++;
            totalHeight += r.size.height;
        } else if (nRelevant == 0) {
            if ((totalHeightAllLetters <= 0) && (ch != ' ')) {
                totalHeightAllRects += r.size.height;
                nAllRects++;
            }
            if (isLetterOrDigit(ch)) {
                totalHeightAllLetters += r.size.height;
                nLetters++;
            }
        }
	}
    
    float av;
    if (nRelevant != 0)
        av = totalHeight / nRelevant;
    else if (nLetters != 0)
        av = totalHeightAllLetters / nLetters;
    else if (nAllRects > 0)
        av = totalHeightAllRects / nAllRects;
    else
        av = 0;
    obj = SmartPtr<float>(new float(av));
    m[matchKeyAverageHeightTallLetters] = obj.castDown<EmptyType>();
	
    return av;
} // averageHeighTallLetters
#endif

// Return an int value if exists, or else returns the specified default value (and sets it in the match)
int OCRResults::getIntIfExists(MatchPtr m, std::string type, int defaultValue) {
    if (m.isExists(type)) {
        return getIntFromMatch(m, type);
    } else {
        m[type] = (SmartPtr<int> (new int(defaultValue))).castDown<EmptyType>();
        return defaultValue;
    }
}

CGRect OCRResults::matchRange(MatchPtr m){
//#if DEBUG
//    wstring& itemText = getStringFromMatch(m, matchKeyText);
//    if (itemText.substr(0,5) == L"CLEAN") {
//        DebugLog("FOUND IT!");
//    }
//#endif
	SmartPtr<float> vXMin = m[matchKeyXMin].castDown<float>();
	SmartPtr<float> vXMax = m[matchKeyXMax].castDown<float>();
	SmartPtr<float> vYMin = m[matchKeyYMin].castDown<float>();
    SmartPtr<float> vYMax = m[matchKeyYMax].castDown<float>();
	if ((vXMin == NULL) || (vXMax == NULL) || (vYMin == NULL) || (vYMax == NULL)) {
		// Need to compute range
		SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
		SmartPtr<size_t> tmpNptr = m[matchKeyN].castDown<size_t>();
		if (rects.isNull()) {
            return Rect(0,0,0,0);
        }
		int n = (int)*tmpNptr;
		CGRect r = OCRResults::rangeWithNRects(n, rects->begin());
		if ((r.size.width == 0) || (r.size.height == 0)) {
            DebugLog("MatchRange: warning, returning empty range for item [%s], NRects=%d", toUTF8(getStringFromMatch(m, matchKeyText)).c_str(), getIntFromMatch(m, matchKeyN));
			return r;
		}
		float minX = r.origin.x; float maxX = r.origin.x + r.size.width - 1;
		float minY = r.origin.y; float maxY = r.origin.y + r.size.height - 1;
		vXMin = SmartPtr<float>(new float(minX));
		m[matchKeyXMin] = vXMin.castDown<EmptyType>();
		vXMax = SmartPtr<float>(new float(maxX));
		m[matchKeyXMax] = vXMax.castDown<EmptyType>();
		vYMin = SmartPtr<float>(new float(minY));
		m[matchKeyYMin] = vYMin.castDown<EmptyType>();
		vYMax = SmartPtr<float>(new float(maxY));
		m[matchKeyYMax] = vYMax.castDown<EmptyType>();

		return (r);
	}
	float minX = *vXMin;
	float maxX = *vXMax;
	float minY = *vYMin;
	float maxY = *vYMax;

	return (CGRect(minX, minY, maxX-minX+1, maxY-minY+1));
}

CGRect OCRResults::rangeWithNRects(int n, RectVector::iterator rects){
	float minX = 32000; float maxX = 0;
	float minY = 32000; float maxY = 0;
	for (int i = 0; i<n; ++i,++rects) {
		Rect r = **rects;
		if ((r.size.width < 2) && (r.size.height < 2)) {
			// Space - skip it, just in case it has out of wack coordinates
			continue;
		}
		if (rectLeft(r) < minX)
			minX = rectLeft(r);
		if (rectRight(r) > maxX)
			maxX = rectRight(r);
		if (rectBottom(r) < minY)
			minY = rectBottom(r);
		if (rectTop(r) > maxY)
			maxY = rectTop(r);
	}
	if ((maxX < minX) || (maxY < minY)) {
	//if ((maxX <= minX) || (maxY <= minY)) {
#if DEBUG
		if (n != 0) {
			ErrorLog("OCRGlobalResults::rangeWithNRects: - error, got illegal range [%.2f - %.2f] - [%.2f - %.2f], n=%d",
				 minX, maxX, minY, maxY, n);
		}
#endif
		return CGRect(0,0,0,0);
	} else {
		return (CGRect(minX, minY, maxX-minX+1, maxY-minY+1));
	}
}


float OCRResults::matchSlant(MatchPtr m) {
//#if DEBUG
//    wstring& itemText = getStringFromMatch(m, matchKeyText);
//    if (itemText.substr(0,5) == L"CLEAN") {
//        DebugLog("FOUND IT!");
//    }
//#endif
    if (m.isExists(matchKeySlant))
        return (getFloatFromMatch(m, matchKeySlant));
    
    if (!m.isExists(matchKeyRects) || !m.isExists(matchKeyN))
        return 0;
    
    SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
    int n = getIntFromMatch(m, matchKeyN);
    wstring itemText = getStringFromMatch(m, matchKeyText);
    if (m.isExists(matchKeyOrigText))
        itemText = getStringFromMatch(m, matchKeyOrigText);
    if (((int)rects->size() != n) || (itemText.length() != n))
        return 0;
    // linear regression: see http://www.statisticshowto.com/how-to-find-a-linear-regression-equation/
    /* Need:
        n, sum(x), sum(y), sum(xy), sum(x2), sum(y2)
     */
    int nLettersOrDigits = 0;
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    //float sumY2 = 0;
    for (int i=0; i<n; i++) {
        CGRect r = *(*rects)[i];
        wchar_t ch = itemText[i];
        if (!isLetterOrDigit(ch))
            continue;
        nLettersOrDigits++;
        float x = rectLeft(r) + r.size.width/2 - 1;
        float y = rectTop(r);   // Assumes all letters rest on the baseline
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
        //sumY2 += y * y;
    }
    //float base = (sumY*sumX2 - sumX*sumXY) / (LettersOrDigits*sumX2 - sumX*sumX);
    float slant = 0;
    if (n >= 3) {
        slant = (nLettersOrDigits*sumXY - sumX * sumY) / (nLettersOrDigits*sumX2 - sumX*sumX);
    }
    m[matchKeySlant] = (SmartPtr<float> (new float(slant))).castDown<EmptyType>();
    
    return slant;
} // matchSlant

float OCRResults::averageLetterWidthInMatch(MatchPtr m) {
    if (m == NULL)
        return 0;                       
	SmartPtr<float> obj = m[matchKeyAverageLetterWidth].castDown<float>();
	if (!obj.isNull()) {
		return *obj;
    }
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	size_t nRects = *m[matchKeyN].castDown<size_t>();
	if (nRects == 0)
		return 0;
	float totalWidth = 0;
	for (int i=0; i<nRects; i++) {
		CGRect r = *(*rects)[i];
		totalWidth += r.size.width;
	}
	float res = totalWidth / nRects;
	obj = SmartPtr<float>(new float(res));
    m[matchKeyAverageLetterWidth] = obj.castDown<EmptyType>();
	return (res);
} // averageLetterWidthInMatch

/* Note: uses matchKeyOrigText if set, otherwise matchKeyText
    Accepts dollar, %, dot within the sequence (but only one) - for both maxSequence as well as maxPriceSequence - to accept prices. Reason dots are accepted even for maxSequence is to be conservative, in case some code tests for maxSequence to check for meaningful stuff (although it should test for maxPriceSequence really)
    Returns false if text and nRects didn't match, true otherwise.
    - maxSequence: counts digits, accepts separator
    - maxPriceSequenceL: counts digits and lookalikes (e.g. 'B'), accept separator. Meant to simulate looking for a price.
    - uniform: output param, indicates if within the max sequence we found digits were within 15% of the average for the sequence
    - productWidth: if a sequence of length product len (for that retailer) was found, set to the width (in pixels) of that sequence
 */
bool OCRResults::averageDigitHeightWidthSpacingInMatch(MatchPtr m, float& width, float& height, float& spacing, bool &uniform, int &maxNormalSequence, float &maxNormalSequenceAverageHeight, int &maxPriceSequence, bool &uniformPriceSequence, float &maxPriceSequenceAverageHeight, float &productWidth, retailer_s *retailerParams) {
    if (m == NULL)
        return 0;

    // Don't cache, because caching would force us to recalculate each time rects change - and in any case we only use this info once per match
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	int nRects = (int)(*m[matchKeyN].castDown<size_t>());
	if (nRects == 0)
		return false;
	
    float totalHeight = 0;
    int numHeight = 0;
    // Same as the above but only within sequences (not for entire text)
    float totalHeightSequence = 0;
    int numHeightSequence = 0;
    // Same as the above but only within price sequences (not for entire text)
    float totalHeightPriceSequence = 0;
    int numHeightPriceSequence = 0;
    
	float totalWidth = 0;
    int numWidth = 0;
    float totalSpacing = 0;
    int numSpacings = 0;
    wstring itemText;
    if (m.isExists(matchKeyOrigText)) {
        itemText = getStringFromMatch(m, matchKeyOrigText);
//#if DEBUG
//        if (itemText.find(L"3.98") != wstring::npos) {
//            DebugLog("");
//        }
//#endif
    } else {
        itemText = getStringFromMatch(m, matchKeyText);
//#if DEBUG
//        if (itemText.find(L"3.98") != wstring::npos) {
//            DebugLog("");
//        }
//#endif
    }
    if (rects->size() == 0) {
        ErrorLog("averageDigitHeightWidthSpacingInMatch: ERROR, item [%s] has empty rects even though nRects=%d", toUTF8(itemText).c_str(), (int)nRects);
        return false;
    }
    if (itemText.length() != (int)nRects) {
        ErrorLog("averageDigitHeightWidthSpacingInMatch: mismatch item length [%s, length=%d] versus nRects=%d", toUTF8(itemText).c_str(), (int)itemText.length(), (int)nRects);
        return false;
    }
    
    maxNormalSequence = 0;
    maxNormalSequenceAverageHeight = 0;
    bool maxSequenceHasUniformHeight = true;
    bool uniformSequence = true;
    int countNormalSequence = 0;
    
    maxPriceSequence = 0;
    maxNormalSequenceAverageHeight = 0;
    bool maxPriceSequenceHasUniformHeight = true;
    uniformPriceSequence = true;
    int countPriceSequence = 0;
    
    // Variable to attempt finding a product number
    int maxLookalikeSequence = 0, countLookalikeSequence = 0;
    int startIndexLookalikeSequence = -1, endIndexLookalikeSequence = -1;
    productWidth = 0;
    
    int numDots = 0, numDashes = 0, numPercent = 0, numDollars = 0;
	for (int i=0; i<(int)nRects; i++) {
		CGRect r = *(*rects)[i];
        wchar_t ch = itemText[i];
        if (ch == ' ') {
            // Straight digits sequence
            if ((countNormalSequence > maxNormalSequence)
                // Same length but uniform
                || ((countNormalSequence == maxNormalSequence) && (!maxSequenceHasUniformHeight && uniformSequence))) {
                // New max sequence, make note of it!
                maxNormalSequence = countNormalSequence;
                maxSequenceHasUniformHeight = uniformSequence;
                if (numHeightSequence > 0)
                    maxNormalSequenceAverageHeight = totalHeightSequence / numHeightSequence;
                else
                    maxNormalSequenceAverageHeight = 0;
            }
            // Looalikes sequence
            if ((countLookalikeSequence > 0) && (countLookalikeSequence > maxLookalikeSequence)) {
                maxLookalikeSequence = countLookalikeSequence;
                endIndexLookalikeSequence = i-1;
                // Is this a product number?
                if ((productWidth <= 0) && (retailerParams != NULL) && (countLookalikeSequence == retailerParams->productNumberLen)) {
                    CGRect rStart = *(*rects)[startIndexLookalikeSequence];
                    CGRect rEnd = *(*rects)[endIndexLookalikeSequence];
                    productWidth = rectRight(rEnd) - rectLeft(rStart) + 1;
                }
            }
            // Price-style sequence
            if ((countPriceSequence > maxPriceSequence)
                // Same length but uniform
                || ((countPriceSequence == maxPriceSequence) && (!maxPriceSequenceHasUniformHeight && uniformPriceSequence))) {
                // New max sequence, make note of it!
                maxPriceSequence = countPriceSequence;
                maxPriceSequenceHasUniformHeight = uniformPriceSequence;
                if (numHeightPriceSequence > 0)
                    maxPriceSequenceAverageHeight = totalHeightPriceSequence / numHeightPriceSequence;
                else
                    maxPriceSequenceAverageHeight = 0;
            }
            countNormalSequence = countLookalikeSequence = countPriceSequence = 0;
            numDots = numDashes = numPercent = numDollars = 0;
            // No need to initialize anything else, we'll do that when we encouter the start of the next potential sequence
            continue;
        }
        
        // Handle digits lookalikes first, then just the regular digits stuff
        bool currentCharIsDigitLookalike = false;
        if (isDigitLookalikeExtended(ch)) {
            currentCharIsDigitLookalike = true;
            if (countLookalikeSequence == 0) {
                startIndexLookalikeSequence = endIndexLookalikeSequence = i;
            }
            if (countPriceSequence == 0) {
                // New sequence, initialize everything
                uniformPriceSequence = true;
                numHeightPriceSequence = 0;
                totalHeightPriceSequence = 0;
            }
            
            // Update stats for price sequence
            if (numHeightPriceSequence > 0) {
                float avHeight = totalHeightPriceSequence / numHeightPriceSequence;
                if (((avHeight > 10) && ((r.size.height >  avHeight * 1.15) || (r.size.height < avHeight * 0.85)))
                    // Below 10 allow an average of 8 followed by a height 10 item (or vice-versa)
                    || ((avHeight <= 10) && ((r.size.height >  avHeight * 1.25) || (r.size.height < avHeight * 0.80))))
                    uniformPriceSequence = false;
            }
            totalHeightPriceSequence += r.size.height; numHeightPriceSequence++;
            
            // Lookalikes are legit for countLookalikeSequence and countPriceSequence
            countLookalikeSequence++;
            countPriceSequence++;
            // Don't continue here, need to handle normal digit sequences
        }
        else {
            // Not a digit loookalike - end countLookalikeSequence
            if (countLookalikeSequence > maxLookalikeSequence) {
                maxLookalikeSequence = countLookalikeSequence;
                endIndexLookalikeSequence = i-1;
                // Is this a product number?
                if ((productWidth <= 0) && (retailerParams != NULL) && (countLookalikeSequence == retailerParams->productNumberLen)) {
                    CGRect rStart = *(*rects)[startIndexLookalikeSequence];
                    CGRect rEnd = *(*rects)[endIndexLookalikeSequence];
                    productWidth = rectRight(rEnd) - rectLeft(rStart) + 1;
                }
            }
            countLookalikeSequence = 0;
            // No need to initialize anything else, we'll do that when we encouter the start of the next potential sequence
        }
        
        bool currentCharIsDigit = false;
        if (isDigit(ch)) {
            currentCharIsDigit = true;
            if (countNormalSequence == 0) {
                // Start of a new sequence!
                uniformSequence = true; // Until proven otherwise
                numHeightSequence = 0; totalHeightSequence = 0;
            }
            countNormalSequence++;
            
            // Accounting for sizes
            if (ch != '1') {
                totalWidth += r.size.width;
                numWidth++;
            }
            totalHeight += r.size.height; numHeight++;
            
            if (numHeightSequence > 0) {
                float avHeight = totalHeightSequence / numHeightSequence;
                if (((r.size.height >  avHeight * 1.15) || (r.size.height < avHeight * 0.85)))
                    uniformSequence = false;
            }
            totalHeightSequence += r.size.height; numHeightSequence++;
        
            // Spacing - only between digits
            if ((i>0) && isDigit(itemText[i-1])) {
                CGRect rPrev = *(*rects)[i-1];
                float spaceBetween = rectSpaceBetweenRects(rPrev, r);
                totalSpacing += spaceBetween; numSpacings++;
            }
        } // digit
        else {
            // Not a digit - end countNormalSequence
            if ((countNormalSequence > maxNormalSequence)
                // Same length but uniform
                || ((countNormalSequence == maxNormalSequence) && (!maxSequenceHasUniformHeight && uniformSequence))) {
                // New max sequence, make note of it!
                maxNormalSequence = countNormalSequence;
                maxSequenceHasUniformHeight = uniformSequence;
                if (numHeightSequence > 0)
                    maxNormalSequenceAverageHeight = totalHeightSequence / numHeightSequence;
                else
                    maxNormalSequenceAverageHeight = 0;
            }
            // No need to initialize anything else, we'll do that when we encouter the start of the next potential sequence
        }
        
        // Non-digit, end this price sequence? Note: we already terminated any normal and lookalike sequence(s).
        if (!currentCharIsDigitLookalike) {
            bool terminatePriceSequence = true;
            if (isDecimalPointLookalike(ch)) {
                numDots++;
                if (numDots < 2)
                    terminatePriceSequence = false;
            } else if (ch == '-') {
                numDashes++;
                if (numDashes < 2)
                    terminatePriceSequence = false;
            } else if (ch == '%') {
                numPercent++;
                if (numPercent < 2)
                    terminatePriceSequence = false;
            } else if (ch == '$') {
                numDollars++;
                if (numDollars < 2)
                    terminatePriceSequence = false;
            }
            // End sequence ?
            if (terminatePriceSequence) {
                if ((countPriceSequence > maxPriceSequence)
                    // Same length but uniform
                    || ((countPriceSequence == maxPriceSequence) && (!maxPriceSequenceHasUniformHeight && uniformPriceSequence))) {
                    // New max sequence, make note of it!
                    maxPriceSequence = countPriceSequence;
                    maxPriceSequenceHasUniformHeight = uniformPriceSequence;
                    if (numHeightPriceSequence > 0)
                        maxPriceSequenceAverageHeight = totalHeightPriceSequence / numHeightPriceSequence;
                    else
                        maxPriceSequenceAverageHeight = 0;
                }
                countPriceSequence = 0;
                numDots = numDashes = numPercent = numDollars = 0;
                uniformSequence = uniformPriceSequence = true;
            }
        } // non-digit (usually ending sequence)
  	} // iterate over all chars
    // Case where the line ended before we accounted for the sequence in progress
    if (countNormalSequence > maxNormalSequence) {
        maxNormalSequence = countNormalSequence;
        maxSequenceHasUniformHeight = uniformSequence;
        if (numHeightSequence > 0)
            maxNormalSequenceAverageHeight = totalHeightSequence / numHeightSequence;
        else
            maxNormalSequenceAverageHeight = 0;
    }
    // Case where the line ended before we accounted for the price sequence in progress
    if (countPriceSequence > maxPriceSequence) {
        maxPriceSequence = countPriceSequence;
        maxPriceSequenceHasUniformHeight = uniformPriceSequence;
        if (numHeightPriceSequence > 0)
            maxPriceSequenceAverageHeight = totalHeightPriceSequence / numHeightPriceSequence;
        else
            maxPriceSequenceAverageHeight = 0;
    }
    if (countLookalikeSequence > maxLookalikeSequence) {
        if (countLookalikeSequence > maxLookalikeSequence) {
            maxLookalikeSequence = countLookalikeSequence;
            endIndexLookalikeSequence = nRects-1;
            // Is this a product number?
            if ((productWidth <= 0) && (retailerParams != NULL) && (countLookalikeSequence == retailerParams->productNumberLen)) {
                CGRect rStart = *(*rects)[startIndexLookalikeSequence];
                CGRect rEnd = *(*rects)[endIndexLookalikeSequence];
                productWidth = rectRight(rEnd) - rectLeft(rStart) + 1;
            }
        }
    }
    if (numWidth > 0)
        width = totalWidth / numWidth;
    else
        width = 0;
    if (numHeight > 0)
        height = totalHeight / numHeight;
    else
    if (numSpacings > 0)
        spacing = totalSpacing / numSpacings;
    else
        spacing = 0;
    
    if (maxNormalSequence > 0)
        uniform = maxSequenceHasUniformHeight;
    else
        uniform = false;
    
    if (maxPriceSequence > 0)
        uniformPriceSequence = maxSequenceHasUniformHeight;
    else
        uniformPriceSequence = false;

	return (true);
} // averageDigitHeightWidthSpacingInMatch

/* averageUppercaseHeightWidthSpacingInMatch:
    Returns false if text and nRects didn't match, true otherwise.
    - spacing: output parameter for the average space between uppercase letters
    - unform: output parameter, indicates if within the max sequence found all letter heights were within 15% of average in sequence
 */
bool OCRResults::averageUppercaseHeightWidthSpacingInMatch(MatchPtr m, float& width, float& height, float& spacing, bool &uniform, int &maxSequence, float &maxSequenceAverageHeight) {
    if (m == NULL)
        return 0;                       

    // Don't cache, because caching would force us to recalculate each time rects change - and in any case we only use this info once per match
	SmartPtr<vector<SmartPtr<CGRect> > > rects = m[matchKeyRects].castDown<vector<SmartPtr<CGRect> > >();
	int nRects = (int)(*m[matchKeyN].castDown<size_t>());
	if (nRects == 0)
		return false;
	float totalHeight = 0;
    int numHeight = 0;
    // Same as the above but only within sequences (not for entire text)
    float totalHeightSequence = 0;
    int numHeightSequence = 0;
	float totalWidth = 0;
    int numWidth = 0;
    float totalSpacing = 0;
    int numSpacings = 0;
    wstring itemText;
    if (m.isExists(matchKeyOrigText)) {
        itemText = getStringFromMatch(m, matchKeyOrigText);
    } else {
        itemText = getStringFromMatch(m, matchKeyText);
    }
    if (rects->size() == 0) {
        ErrorLog("averageUppercaseHeightWidthSpacingInMatch: ERROR, item [%s] has empty rects even though nRects=%d", toUTF8(itemText).c_str(), (int)nRects);
        return false;
    }
    if (itemText.length() != (int)nRects) {
        ErrorLog("averageUppercaseHeightWidthSpacingInMatch: mismatch item length [%s, length=%d] versus nRects=%d", toUTF8(itemText).c_str(), (int)itemText.length(), (int)nRects);
        return false;
    }

    int countNormalSequence = 0;
    maxSequence = 0;
    maxSequenceAverageHeight = 0;
    bool maxSequenceHasUniformHeight = true;
    bool uniformSequence = true;
	for (int i=0; i<(int)nRects; i++) {
		CGRect r = *(*rects)[i];
        wchar_t ch = itemText[i];
        
        if (isUpper(ch)) {
            if (countNormalSequence == 0) {
                // Start of a new sequence!
                uniformSequence = true; // Until proven otherwise
                numHeightSequence = 0; totalHeightSequence = 0;
            }
            countNormalSequence++;
            
            // Accounting for width & height stats
            totalWidth += r.size.width; numWidth++;
            totalHeight += r.size.height; numHeight++;
            
            // Specific within current sequence
            if (numHeightSequence > 0) {
                // Not the first in the sequence, check fit to average
                float avHeight = totalHeightSequence / numHeightSequence;
                if (((r.size.height >  avHeight * 1.15) || (r.size.height < avHeight * 0.85)))
                    uniformSequence = false;
            }
            totalHeightSequence += r.size.height; numHeightSequence++;
            
            // Spacing accounting (only between uppercase letters)
            if ((i>0) && isUpper(itemText[i-1])) {
                CGRect rPrev = *(*rects)[i-1];
                float spaceBetween = rectSpaceBetweenRects(rPrev, r);
                totalSpacing += spaceBetween; numSpacings++;
            }
        }
        // Not an uppercase letter - end current sequence, reset
        else {
            if ((countNormalSequence > maxSequence)
                // Same length but uniform
                || ((countNormalSequence == maxSequence) && (!maxSequenceHasUniformHeight && uniformSequence))) {
                // New winning sequence!
                maxSequence = countNormalSequence;
                maxSequenceHasUniformHeight = uniformSequence;
                if (numHeightSequence > 0)
                    maxSequenceAverageHeight = totalHeightSequence / numHeightSequence;
                else
                    maxSequenceAverageHeight = 0;
            }
            countNormalSequence = 0;
            // No need to initialize anything else, we'll do that when we encouter the start of the next potential sequence
        }
	} // iterate over all chars
    
    // Case where sequence ended at end of line
    if ((countNormalSequence > maxSequence)
        // Same length but uniform
        || ((countNormalSequence == maxSequence) && (!maxSequenceHasUniformHeight && uniformSequence))) {
        // New winning sequence!
        maxSequence = countNormalSequence;
        maxSequenceHasUniformHeight = uniformSequence;
        if (numHeightSequence > 0)
            maxSequenceAverageHeight = totalHeightSequence / numHeightSequence;
        else
            maxSequenceAverageHeight = 0;
    }
    
    if (numWidth > 0)
        width = totalWidth / numWidth;
    else
        width = 0;
    if (numHeight > 0)
        height = totalHeight / numHeight;
    else
    if (numSpacings > 0)
        spacing = totalSpacing / numSpacings;
    else
        spacing = 0;
    
    if (maxSequence > 0)
        uniform = maxSequenceHasUniformHeight;
    else
        uniform = false;

	return (true);
} // averageUppercaseLetterHeightWidthSpacingInMatch


bool OCRResults::matchesAligned(MatchPtr m1, MatchPtr m2, int alignment) {
    CGRect rect1 = OCRResults::matchRange(m1);
    if (rect1.size.width == 0)
        return false;
    CGRect rect2 = OCRResults::matchRange(m2);
    if (rect2.size.width == 0)
        return false; 
    float m1LetterWidth = OCRResults::averageLetterWidthInMatch(m1);
    if (m1LetterWidth <= 0)
        return false;
    float idealXPosition;
    float actualPosition;
    if (alignment == ALIGNED_CENTER) {
        idealXPosition = rectCenterX(rect1);
        actualPosition = rectCenterX(rect2);
    } else {
        // Aligned left
        idealXPosition = rectLeft(rect1);
        actualPosition = rectLeft(rect2);
    }
    if (abs(actualPosition - idealXPosition) < m1LetterWidth * 2)
        return true;
    else
        return false;
} // matchesAligned


// Returns match on same line and to the right of a given item, only if closer than acceptableNumberOfSpaces of spaces away (set to -1 if anything on same line works, as is the case for receipts)
// - assumeBlocksSet = true means we know SetBlocks was called earlier and thus want to only look at the next item(s) following current item
MatchPtr OCRResults::findItemJustAfterMatchAtIndexInMatchesExcluding(int i,MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool assumeBlocksSet, OCRResults *results) {
    if (assumeBlocksSet) {
        if (i>=(matches.size()-1))
        return MatchPtr();

        MatchPtr matchBefore = matches[i];

        // Skip all derived types and excluded, if any
        for (i++; i<matches.size(); i++) {
            MatchPtr d = matches[i];
            float conf = *d[matchKeyConfidence].castDown<float>();

            // Stop if valid candidate: not derived (confidence == -1) and not in excluded

            if ((conf >= 0) && ((ex == NULL) || (std::find(ex->begin(), ex->end(), d) == ex->end())))

            break;
        }

        if (i >= matches.size())
            return MatchPtr();

        i--;

        MatchPtr match = matches[i+1];

        CGRect matchRect = OCRResults::matchRange(match);
        wstring matchText = getStringFromMatch(match, matchKeyText);
        CGRect matchBeforeRect = OCRResults::matchRange(matchBefore);

        // In some cases an item appearing after the current item is there because it's below - but lies actually to the LEFT of the current item, which makes it unsuitable to be the item after (i.e. to the right) of current item.

        if (rectLeft(matchRect) < rectLeft(matchBeforeRect))
            return MatchPtr();

        wstring matchBeforeText = getStringFromMatch(match, matchKeyText);

        if ((matchText.length() == 0) || (matchBeforeText.length() == 0))
            return MatchPtr();

        if (OCRResults::overlappingRectsYAxisAnd(matchBeforeRect, matchRect)) {

            if (acceptableNumberOfSpaces >= 0) {
                float estimatedCharWidthMatchBefore = OCRResults::averageLetterWidthInMatch(matchBefore);
                float estimatedCharWidthMatch = OCRResults::averageLetterWidthInMatch(match);
                float estimatedCharWidth = min(estimatedCharWidthMatchBefore, estimatedCharWidthMatch);
                float spaceBetween = rectLeft(matchRect) - rectRight(matchBeforeRect) - 1;

                // No width data, suspicious, better declare that the item has no item just after
                if (estimatedCharWidth == 0)
                    return MatchPtr();

                if (spaceBetween/estimatedCharWidth < 4.0)
                    return match;
                else
                    return MatchPtr();
            } else
                return match;
        } else
            return MatchPtr();
    } else
    {
        if (matches.size() == 1)
            return MatchPtr();  // Only one item (the current item), can't find another after!

        MatchPtr matchBefore = matches[i];
        CGRect matchBeforeRect = OCRResults::matchRange(matchBefore);
        float heightTallestMatchBefore = OCRResults::heightTallestChar(matchBefore);
        float estimatedCharWidthMatchBefore = OCRResults::averageLetterWidthInMatch(matchBefore);
        int bestDistanceAwayXAxis = -1; // Distance after current along X axis of best match after
        float bestPercentOverlap = -1;
        MatchPtr bestMatchAfter = MatchPtr();

        for (int j=0; j<matches.size(); j++) {
            if (j == i) continue;   // Skip match itself
            MatchPtr matchAfter = matches[j];
            if (std::find(ex->begin(), ex->end(), matchAfter) != ex->end())
                continue;    // Already taken (but not yet deleted), skip
            float conf = *matchAfter[matchKeyConfidence].castDown<float>();
            if (conf < 0) continue; // Skip derived types
            // Stop if valid candidate: not derived (confidence == -1) and not in excluded
            if ((ex != NULL) && (std::find(ex->begin(), ex->end(), matchAfter) != ex->end()))
                continue;   // Skip
            
            float heightTallestCandidate = OCRResults::heightTallestChar(matchAfter);
            CGRect matchAfterRect = OCRResults::matchRange(matchAfter);
            // 2015-10-03 some time ago we switched from using the below to using the "facing Y" method - but not for the findItemJustBefore API yet
            //float overlapYAmount = overlapAmountRectsYAxisAnd(matchBeforeRect, matchRect);
            float overlapPercent = 0;
            float overlapYAmount = 0;
#if DEBUG
            wstring textBefore = getStringFromMatch(matchBefore, matchKeyText);
            wstring textAfter = getStringFromMatch(matchAfter, matchKeyText);
            if ((textBefore == L"LEL CRITTER . I .IJ. F") && (textAfter == L"$8.79")) {
                DebugLog("");
            }
#endif
            if (!yOverlapFacingSides(matchBefore, matchAfter, overlapYAmount, overlapPercent, true)) {
                overlapYAmount = overlapAmountRectsYAxisAnd(matchBeforeRect, matchAfterRect);
            }
            
            if (overlapYAmount > 0) {
                float estimatedCharWidthMatch = OCRResults::averageLetterWidthInMatch(matchAfter);
                float estimatedCharWidth = min(estimatedCharWidthMatchBefore, estimatedCharWidthMatch);
                float spaceBetweenBeforeAfter = rectLeft(matchAfterRect) - rectRight(matchBeforeRect) - 1;
                // Allow SOME overlap, see https://www.pivotaltracker.com/story/show/109205392 where we refused to collate items on the same line because of a 12 pixels overlap
                if ((spaceBetweenBeforeAfter < 0) && ((results == NULL) || ((results->globalStats.averageWidthDigits.average > 0) && (-spaceBetweenBeforeAfter > results->globalStats.averageWidthDigits.average * 2))))
                    continue; // Only looking to the right!
                // No width data, suspicious, better declare that the item has no item just after
                if (estimatedCharWidth == 0)
                    return MatchPtr();
                if ((acceptableNumberOfSpaces < 0)
                    || (spaceBetweenBeforeAfter/estimatedCharWidth < acceptableNumberOfSpaces)) {
                    // Remember this item?
                    if ((bestDistanceAwayXAxis < 0)
                        // Closer to the right
                        || ((spaceBetweenBeforeAfter < bestDistanceAwayXAxis)
                            // And decent overlap
                            && ((overlapPercent > 0.25)
                                // Or not so great overlap but better than what we had so far (and closer to the right so take it!)
                                || (overlapPercent > bestPercentOverlap)))
                        // Not closer to the right but ... twice as good in Y overlap. Note: not certain this right, could catch bogus stuff far away on slanted line, overlapping hugely because of slant.
                        || ((overlapPercent > 0.25) && (overlapPercent > bestPercentOverlap * 2))) {
                        // Not so fast: it is possible that there is a slight overlap between matchBefore and this one - BUT there also is elsewhere a match entirely to the left of this one, with a better Y overlap. If so, skip this one (let it get associated with that other match later)
                        bool skipThisOne = false;
                        if (spaceBetweenBeforeAfter > 0) { // No need to find a better fit if items are even overlapping!
                            for (int k=0; k<matches.size(); k++) {
                                if ((k == i) || (k == j)) continue;
                                MatchPtr alternateMatchBefore = matches[k];
                                if (std::find(ex->begin(), ex->end(), alternateMatchBefore) != ex->end())
                                    continue;    // Already taken (but not yet deleted), skip
                                CGRect alternateMatchBeforeRect = OCRResults::matchRange(alternateMatchBefore);
                                if (OCRResults::overlappingRectsYAxisAnd(alternateMatchBeforeRect, matchAfterRect)) {
                                    float alternateSpaceBetween = rectLeft(matchAfterRect) - rectRight(alternateMatchBeforeRect) - 1;
                                    if (alternateSpaceBetween > 0) {
                                        //WAS: float alternateOverlapYAmount = overlapAmountRectsYAxisAnd(alternateMatchBeforeRect, matchRect);
                                        float alternateOverlapYAmount, alternateOverlapPercent;
                                        if (!yOverlapFacingSides(alternateMatchBefore, matchAfter, alternateOverlapYAmount, alternateOverlapPercent, true)) {
                                            alternateOverlapYAmount = overlapAmountRectsYAxisAnd(alternateMatchBeforeRect, matchAfterRect);
                                        }
                                        
                                        float heightTallestAlternateBefore = OCRResults::heightTallestChar(alternateMatchBefore);
                                        // PQ911 need to ignore items with apparent better Y overlap in favor or item much closer, for two reasons:
                                        // 1. Slant calculations could lead us astray, especially with short items and has a greater impact as items are further apart
                                        // 2. When overlap is 50%+ and items are close (say 3-5 letters away), no need to even consider anything further away!
                                        //float adjustedOverlapYAmount = overlapYAmount * ( 1 + alternateSpaceBetween/spaceBetween * 0.25);
                                        if (alternateOverlapYAmount > overlapYAmount) {
                                            if ((heightTallestCandidate <= 0) || (heightTallestAlternateBefore <= 0) || (heightTallestAlternateBefore > heightTallestMatchBefore * 2)) {
                                                DebugLog("findItemJustAfterMatchAtIndexInMatchesExcluding: NOT ignoring item [%s] height=%.0f Y overlapping with [%s] height=%.0f because it overlaps more with item [%s] height=%.0f - weird heights!", toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(), heightTallestCandidate, toUTF8(getStringFromMatch(matchBefore, matchKeyText)).c_str(), heightTallestMatchBefore, toUTF8(getStringFromMatch(alternateMatchBefore, matchKeyText)).c_str(), heightTallestAlternateBefore);
                                            } else if ((overlapPercent > 0.40) && (alternateSpaceBetween > spaceBetweenBeforeAfter * 1.5)){
                                                    DebugLog("findItemJustAfterMatchAtIndexInMatchesExcluding: ignoring item [%s] [%.0f,%.0f - %.0f,%.0f] from consideration as better choice than [%s] to follow [%s] because its distance is %.0f (compared with %.0f) and current overlap percent is %.0f%%", toUTF8(getStringFromMatch(alternateMatchBefore, matchKeyText)).c_str(), rectLeft(alternateMatchBeforeRect), rectBottom(alternateMatchBeforeRect), rectRight(alternateMatchBeforeRect), rectTop(alternateMatchBeforeRect), toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchBefore, matchKeyText)).c_str(), alternateSpaceBetween, spaceBetweenBeforeAfter, overlapPercent);
                                            } else {
                                                // PQ911 if we reject current item from consideration as the item next on the line, shouldn't we make a note that this item can't be the start of a new line (because there is another item who will grab this one on its line, i.e. let that one item be the start of a line) 
                                                DebugLog("findItemJustAfterMatchAtIndexInMatchesExcluding: ignoring item [%s] Y overlapping with [%s] because it overlaps more with item [%s]", toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchBefore, matchKeyText)).c_str(), toUTF8(getStringFromMatch(alternateMatchBefore, matchKeyText)).c_str());
                                                skipThisOne = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            } // Looking for tighter fit to the left (better Y overlap)
                        } // spaceBetween > 0
                        if (skipThisOne) continue;
                        bestDistanceAwayXAxis = spaceBetweenBeforeAfter;
                        bestPercentOverlap = overlapPercent;
                        bestMatchAfter = matchAfter;
                    }
                }
            }
        }

        return bestMatchAfter;
    }
} // findItemJustAfterMatchAtIndexInMatchesExcluding

// Returns match on same line and to the right of a given item, only if closer than acceptableNumberOfSpaces of spaces away (set to -1 if anything on same line works, as is the case for receipts)
MatchPtr  OCRResults::findItemJustAfterMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool assumeBlocksSet, OCRResults *results) {

	MatchVector::iterator pos = std::find(matches.begin(), matches.end(), match);

    int i = (int)(pos - matches.begin());

	if (pos == matches.end())
		return MatchPtr();
	else
		return (this->findItemJustAfterMatchAtIndexInMatchesExcluding(i, matches, ex, acceptableNumberOfSpaces, assumeBlocksSet, results));
}

// Returns match on same line and to the right of a given item, only if closer than acceptableNumberOfSpaces of spaces away (set to -1 if anything on same line works, as is the case for receipts)
// - assumeBlocksSet = true means we know SetBlocks was called earlier and thus want to only look at the next item(s) following current item
MatchPtr OCRResults::findStartOfTopmostLine(MatchVector& matches, int acceptableNumberOfSpaces) {

    if (matches.size() == 0)
        return MatchPtr();
    if (matches.size() == 1)
        return (matches[0]);

    // Find item with lowest Y (topmost)
    float minY = MAXFLOAT;
    MatchPtr bestMatch;
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        CGRect rect = OCRResults::matchRange(d);
        if (rect.origin.y < minY) {
            minY = rect.origin.y;
            bestMatch = d;
        }
    }
    
#if DEBUG
    CGRect rect = OCRResults::matchRange(bestMatch);
    LayoutLog("findStartOfTopmostLine: initial topmost item is [%s] at index [%d] range [%d,%d - %d,%d]", toUTF8(getStringFromMatch(bestMatch, matchKeyText)).c_str(), OCRResults::indexOfMatchInMatches(bestMatch, matches), (int)rect.origin.x, (int)rect.origin.y, (int)(rect.origin.x + rect.size.width - 1), (int)(rect.origin.y + rect.size.height - 1));
    if (getStringFromMatch(bestMatch, matchKeyText) == L"3.72 11") {
        DebugLog("");
    }
#endif
    
    MatchVectorPtr matchesAlreadyConsidered(new MatchVector);
    matchesAlreadyConsidered->push_back(bestMatch);
    
    // Now look left as many times as necessary, to make sure we pick the start of that line. This call is from setBlocks and once we start there all items on the same lines will get added next (as long as we started with the start of the line)
    MatchPtr itemLeft = findItemJustBeforeMatchInMatchesExcluding(bestMatch, matches, matchesAlreadyConsidered, -1, false);
    while (itemLeft != NULL) {
        bestMatch = itemLeft;
#if DEBUG
        CGRect rect = OCRResults::matchRange(bestMatch);
        LayoutLog("findStartOfTopmostLine: adjust topmost item by looking left to [%s] at index [%d] range [%d,%d - %d,%d]", toUTF8(getStringFromMatch(bestMatch, matchKeyText)).c_str(), OCRResults::indexOfMatchInMatches(bestMatch, matches), (int)rect.origin.x, (int)rect.origin.y, (int)(rect.origin.x + rect.size.width - 1), (int)(rect.origin.y + rect.size.height - 1));
#endif
        matchesAlreadyConsidered->push_back(bestMatch);
        itemLeft = findItemJustBeforeMatchInMatchesExcluding(bestMatch, matches, matchesAlreadyConsidered, -1, false);
    }
    
    return bestMatch;
} // findStartOfTopmostLine

int OCRResults::indexOfMatchInMatches(MatchPtr m, MatchVector& matches) {
    for (int i=0; i<matches.size(); i++) {
        if (matches[i] == m)
            return i;
    }
    return -1;
}

MatchPtr OCRResults::findItemJustAboveMatchAtIndexInMatchesExcluding(int i, MatchVector& matches, MatchVectorPtr ex){
	MatchPtr matchBelow = matches[i];

	// Skip all derived types, if any
	for (i--; i>=0; i--) {
		MatchPtr d = matches[i];
		SmartPtr<float> fl = d[matchKeyConfidence].castDown<float>();
		float conf = *d[matchKeyConfidence].castDown<float>();
		// Stop if valid candidate: not derived (confidence == -1) and not in excluded
		if ((conf >= 0) && ((ex == NULL) || (std::find(ex->begin(), ex->end(), d) == ex->end())))
			break;

	   int n = 0;
	   if(d->count(matchKeyN) != 0) {
           SmartPtr<int> nPtr = d[matchKeyN].castDown<int>();
           if (nPtr != NULL)
               n = *nPtr;
       }
	   // Stop if valid candidate: has rects and not in excluded
	   if ((n > 0) && ((ex == NULL) || (std::find(ex->begin(), ex->end(), d) == ex->end())))
	       break;
	}

	if (i < 0)
		return MatchPtr();

	i++;

	CGRect matchBelowRect = OCRResults::matchRange(matchBelow);
	bool foundGreatFit = false;
	MatchPtr bestFit;
	float bestSpacingRatio = 2000.0;
	// 07-22-2010 Not sure why I removed the line below, it does seem right to stop before current item since the matches are always ordered by position
	//for (int j=i-1; j>=0; j--) {
	//for (int j=0; j<matches.count; j++) {
	for (int j=i-1; j>=0; j--) {
//		if (j==i)
//			continue;
		MatchPtr matchAbove = matches[j];
		if ((ex != NULL) && (std::find(ex->begin(), ex->end(), matchAbove) != ex->end()))
			continue;

		CGRect matchAboveRect = OCRResults::matchRange(matchAbove);
		CGRect applicableRect;
		if (matchAboveRect.size.height > matchBelowRect.size.height)
			applicableRect = matchBelowRect;
		else
			applicableRect = matchAboveRect;
			
		if (applicableRect.size.height == 0) {
#if DEBUG
			if (*matchAbove[matchKeyConfidence].castDown<float>() >= 0) {
				DebugLog("ERROR: findItemJustAboveMatchAtIndexInMatchesExcluding found a non-derived match where applicableRect.size.height is 0!");
			}
#endif		
			continue;
		}

		// Require that the Y spacing between the boxes not exceed 0.56 x the height of the smaller of the two boxes
		float ySpacing = rectBottom(matchBelowRect) - rectTop(matchAboveRect);
		float spaceRatio = ySpacing / applicableRect.size.height;
		if (spaceRatio > 1.25) {
			// Too far above, but keep going through the matches
			continue;
		}
		if (rectBottom(matchAboveRect) > rectBottom(matchBelowRect)) {
			// matchAbove topline starts below matchBelow topline => no good
			continue;
		}
		float belowMatchMidPoint = (rectLeft(matchBelowRect) + rectRight(matchBelowRect))/2;
		if ((belowMatchMidPoint >= rectLeft(matchAboveRect)) && (belowMatchMidPoint <= rectRight(matchAboveRect))) {
			if (spaceRatio < bestSpacingRatio * 1.2) {
				foundGreatFit = true;
				bestFit = matchAbove;
				bestSpacingRatio = spaceRatio;
			} else {
				DebugLog("findItemJustAboveMatch [%s] AtIndexInMatchesExcluding: ignoring item [%s], spaceRatio=%.2f versus best ratio=%.2f",
				toUTF8(getStringFromMatch(matchBelow, matchKeyText)).c_str(),
				toUTF8(getStringFromMatch(matchAbove, matchKeyText)).c_str(),
				spaceRatio, bestSpacingRatio);
			}
		} else if ((!foundGreatFit) && OCRUtilsOverlappingX(matchBelowRect, matchAboveRect)) {
			// Only update best fit if current is no way further away!
			if (spaceRatio < bestSpacingRatio * 1.2) {
				bestFit = matchAbove;
				bestSpacingRatio = spaceRatio;
			} else {
				DebugLog("findItemJustAboveMatch [%s] AtIndexInMatchesExcluding: ignoring item [%s], spaceRatio=%.2f versus best ratio=%.2f",
					toUTF8(getStringFromMatch(matchBelow, matchKeyText)).c_str(),
					toUTF8(getStringFromMatch(matchAbove, matchKeyText)).c_str(),
					spaceRatio, bestSpacingRatio);
			}
		}
	}
	return (bestFit);
} // findItemJustAboveMatchAtIndexInMatchesExcluding


MatchPtr OCRResults::findItemJustAboveMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex){
	if (match == NULL)
		return MatchPtr();
	for (int i=0; i<matches.size(); i++) {
		MatchPtr d = matches[i];
		if (d == match) {
			return (findItemJustAboveMatchAtIndexInMatchesExcluding(i, matches, ex));
		}
	}
	ErrorLog("OCRGlobalResults::findItemJustAboveMatch:InMatches: could not find match [%ls] in matches array", getStringFromMatch(match, matchKeyText).c_str());
	return MatchPtr();
}

MatchPtr OCRResults::findItemJustBelowMatchAtIndexInMatchesFindBestExcluding(int i, MatchVector& matches,bool findBest, MatchVectorPtr ex, bool lax) {
	MatchPtr matchAbove = matches[i];

	// Skip all derived types, if any
	for (i++; i<matches.size(); i++) {
		MatchPtr d = matches[i];
		float conf = *d[matchKeyConfidence].castDown<float>();
		// Stop if valid candidate: not derived (confidence == -1) and not in excluded
		if ((conf >= 0) && ((ex == NULL) || (std::find(ex->begin(), ex->end(), d) == ex->end())))
			break;
	}

	if (i >= matches.size())
		return MatchPtr();

	i--;

	CGRect matchAboveRect = OCRResults::matchRange(matchAbove);
	bool foundGreatFit = false, foundFit = false;
	CGRect bestRect;
    float bestXSpacing = 0;
	MatchPtr bestFit;
	//for (int j=0; j<matches.size(); j++) {
	for (int j=i+1; j<matches.size(); j++) {
		//if (j==i)
		//	continue;	// Don't test against match itself!
		MatchPtr matchBelow = matches[j];
		 if( ((ex != NULL) && (std::find(ex->begin(), ex->end(), matchBelow) != ex->end())))
		     continue;
		CGRect matchBelowRect = OCRResults::matchRange(matchBelow);
        
        // If no X overlap, we won't consider this one
        if (!OCRUtilsOverlappingX(matchBelowRect, matchAboveRect)) {
            continue;
        }
        
		CGRect applicableRect;
        if (lax) {
            if (matchAboveRect.size.height > matchBelowRect.size.height)
                applicableRect = matchAboveRect;
            else
                applicableRect = matchBelowRect;
        } else {
            // Pick the smallest of the two ranges (to be more strict)
            if (matchAboveRect.size.height > matchBelowRect.size.height)
                applicableRect = matchBelowRect;
            else
                applicableRect = matchAboveRect;
        }

		// Require that the Y spacing between the boxes not exceed 0.56 x the height of the smaller of the two boxes
		// Kaiser Permanente (Sandy Clements): "Senior Nurse" had height = 36 and gap above was = 10 => 0.28 * height. 0.56 is twice that (i.e. huge margin)
		// 01-29-2010: we were using the height of the below rect before but that caused use to bunch together items were the below rect was much higher
		float ySpacing = rectBottom(matchBelowRect) - rectTop(matchAboveRect) - 1;
		if (ySpacing > applicableRect.size.height * (lax? 1.5:1.25)) {
			// Too far, but keep going through the matches
			continue;
		}
		if (rectBottom(matchBelowRect) < rectBottom(matchAboveRect)) {
			// matchBelow topline starts above matchAbove topline => no good
			continue;
		}
        // If an item below is too different in size compared to reference match, behave as if findBest == true
        bool findBestNow = findBest;
        if (!findBestNow && ((matchBelowRect.size.height > matchAboveRect.size.height * 2) || (matchBelowRect.size.height < matchAboveRect.size.height * 0.50)))
            findBestNow = true;
                       
        // If we are looking for a best fit, just return this one
        if (!findBestNow) {
            return matchBelow;
        }
           
        float belowMatchMidPoint = (rectLeft(matchBelowRect) + rectRight(matchBelowRect)) / 2;
        float xSpacing = abs(rectLeft(matchAboveRect) - rectLeft(matchBelowRect));
        // Is this a "great fit" (meaning mid point of the below rect falls within match above)
		if ((belowMatchMidPoint >= rectLeft(matchAboveRect)) && (belowMatchMidPoint <= rectRight(matchAboveRect))) {
                // If we don't have a great fit yet
			if (!foundGreatFit  
                // Or we do have one but we are a better match in terms of X
                || ((xSpacing < bestXSpacing) 
                    // And the good fit is NOT entirely below existing great fit (if it is, ignore it - even if better X, can't be right compared to one entirely above)
                    && (rectBottom(matchBelowRect) < rectTop(bestRect)))) {
				foundGreatFit = true;
				bestFit = matchBelow;
                bestRect = matchBelowRect;
                bestXSpacing = xSpacing;
			}
		} else if (!foundGreatFit) {
            // No fit yet - take anything!
            if (!foundFit) {
                // No fit yet, take this one (has X overlap etc)
                foundFit = true;
                bestFit = matchBelow;
                bestRect = matchBelowRect;
                bestXSpacing = abs(rectLeft(matchAboveRect) - rectLeft(matchBelowRect));
            }
            // We have a fit, see if this one is better
                    // Better match in terms of X
            else if ((xSpacing < bestXSpacing)
                    // And the good fit is NOT entirely below existing great fit (if it is, ignore it - even if better X, can't be right compared to one entirely above)
                    && (rectBottom(matchBelowRect) < rectTop(bestRect))) {
                bestFit = matchBelow;
                bestRect = matchBelowRect;
                bestXSpacing = xSpacing;
            }
		} // !foundGreatFit
	} // Iterate on matches
	return (bestFit);
}

MatchPtr OCRResults::findItemJustBelowMatchInMatchesFindBestExcluding(MatchPtr& m, MatchVector& matches,bool findBest, MatchVectorPtr ex, bool lax) {
    MatchVector::iterator pos = std::find(matches.begin(), matches.end(), m);

    int i = (int)(pos - matches.begin());

	if (pos == matches.end()) {
        ErrorLog("OCRGlobalResults::findItemJustBelowMatch:InMatches: could not find match [%ls] in matches array", getStringFromMatch(m, matchKeyText).c_str());
		return MatchPtr();
    }
	else
		return this->findItemJustBelowMatchAtIndexInMatchesFindBestExcluding(i, matches, findBest, ex, lax);
}

MatchPtr OCRResults::findItemJustBeforeMatchAtIndexInMatchesExcluding(int i, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool blocksSet) {

    if (blocksSet) {
	if (i==0)
		return MatchPtr();

	MatchPtr match = matches[i];

	// Skip all derived types, if any
	for (i--; i>=0; i--) {
		MatchPtr d = matches[i];
		float conf = *d[matchKeyConfidence].castDown<float>();
		// Stop if valid candidate: not derived (confidence == -1) and not in excluded
		if ((conf >= 0) && ((ex == NULL) || (std::find(ex->begin(), ex->end(), d) == ex->end())))
			break;
	}

	if (i < 0)
		return MatchPtr();

	i++;

	CGRect matchRect = OCRResults::matchRange(match);
	wstring matchText = getStringFromMatch(match, matchKeyText);
	MatchPtr matchBefore = matches[i-1];
	CGRect matchBeforeRect = OCRResults::matchRange(matchBefore);
                       
    // In some cases an item appearing before current item is there because it's above - but lies actually to the RIGHT of the current item, which makes it unsuitable to be the item before (i.e. to the left) of current item.
    if (rectLeft(matchBeforeRect) > rectLeft(matchRect))
        return MatchPtr();
                       
	wstring matchBeforeText = getStringFromMatch(matchBefore, matchKeyText);

	if ((matchText.length() == 0) || (matchBeforeText.length() == 0))
		return MatchPtr();
    

	if (OCRResults::overlappingRectsYAxisAnd(matchBeforeRect, matchRect)) {
                       
       // If item before overlaps along Y axis and we were tagged as adjacent take it!
        // TODO: use increasing indices for tags, to detect when we have on a line: "foo" + "bar | foobar" - "foo" is not part of the same item
       if (match.isExists(matchKeyStatus) && (getUnsignedLongFromMatch(match, matchKeyStatus) & matchKeyStatusTouchingLeftSideItem))
        {
               // Attention: need to keep going left in case we had an item with more than one separator!
               int matchBeforeInSeparatorsSequence = (matchBefore.isExists(matchKeyStatus) && (getUnsignedLongFromMatch(matchBefore, matchKeyStatus) & matchKeyStatusTouchingLeftSideItem));
               int p=i-1;
               if (matchBeforeInSeparatorsSequence) {
                    while (p>0) {
                        // Continue to go left
                        p--;
                        MatchPtr matchTry = matches[p];
                        CGRect matchTryRect = OCRResults::matchRange(matchTry);
               
                        if ((rectLeft(matchTryRect) > rectLeft(matchBeforeRect))
                            || (!(OCRResults::overlappingRectsYAxisAnd(matchTryRect, matchBeforeRect)))) {
                            // Break without advancing matchBefore because this one is not suitable
                            break;
                        }
               
                        matchBefore = matchTry;
               
                        // Did we reach the start of the sequence with separators?
                        if ((!matchTry.isExists(matchKeyStatus)) || !(getUnsignedLongFromMatch(matchTry, matchKeyStatus) & matchKeyStatusTouchingLeftSideItem)) {
                            // Suitable and w/o the sequence status - take it
                            break;
                        }
               
                        matchBeforeRect = matchTryRect;
                        p--;
                    } // while
               }

            return matchBefore;
       }
                       
		float estimatedCharWidthMatchBefore = matchBeforeRect.size.width / matchBeforeText.length();
		float estimatedCharWidthMatch = matchRect.size.width / matchText.length();
		float estimatedCharWidth = min(estimatedCharWidthMatchBefore, estimatedCharWidthMatch);
		float spaceBetween = rectLeft(matchRect) - rectRight(matchBeforeRect);
		if (spaceBetween/estimatedCharWidth < 4.0)
			return matchBefore;
		else
			return MatchPtr();
	} else
		return MatchPtr();
    } else {
        if (matches.size() == 1)
            return MatchPtr();  // Only one item (the current item), can't find another before!

        MatchPtr matchAfter = matches[i];
        CGRect matchAfterRect = OCRResults::matchRange(matchAfter);
        float heightTallestMatchAfter = OCRResults::heightTallestChar(matchAfter);
        float estimatedCharWidthMatchAfter = OCRResults::averageLetterWidthInMatch(matchAfter);
        int bestDistanceAwayXAxis = -1; // Distance after current along X axis of best match before
        MatchPtr bestMatchBefore = MatchPtr();

        for (int j=0; j<matches.size(); j++) {
            if (j == i) continue;   // Skip match itself
            MatchPtr d = matches[j];
            if (std::find(ex->begin(), ex->end(), d) != ex->end())
                continue;    // Already taken (but not yet deleted), skip
            float conf = *d[matchKeyConfidence].castDown<float>();
            if (conf < 0) continue; // Skip derived types
            // Stop if valid candidate: not derived (confidence == -1) and not in excluded
            if ((ex != NULL) && (std::find(ex->begin(), ex->end(), d) != ex->end()))
                continue;   // Skip
            
            float heightTallestCandidate = OCRResults::heightTallestChar(d);
            CGRect matchRect = OCRResults::matchRange(d);
            //float overlapYAmount = overlapAmountRectsYAxisAnd(matchAfterRect, matchRect);
            float overlapPercent = 0;
            float overlapYAmount = 0;
            //PQTODO try with useSlant=true post V1 submission to App Store
            if (!yOverlapFacingSides(d, matchAfter, overlapYAmount, overlapPercent, true)) {
                overlapYAmount = overlapAmountRectsYAxisAnd(matchAfterRect, matchRect);
            }
            
            // Check Y overlap and require that item being considered ends before the input item starts
            if ((overlapYAmount > 0) && (rectRight(matchRect) < rectLeft(matchAfterRect))) {
                float estimatedCharWidthMatch = OCRResults::averageLetterWidthInMatch(d);
                float estimatedCharWidth = min(estimatedCharWidthMatchAfter, estimatedCharWidthMatch);
                // Calculate space between the end of input item and the start of the item being considered
                float spaceBetween = rectLeft(matchAfterRect) - rectRight(matchRect) - 1;
                // 2015-06-16 no idea what the below is all about, item being considered ends before input item so how could it end after?
                // Reject items left which end past the end of current item: this can happen when a long line is below a short line, ends after it, with a slant up: there will be an overlap caused by the right side of the long text below
//                if (spaceBetween < 0) {
//                    LayoutLog("findItemJustBeforeMatchAtIndexInMatchesExcluding: skipping potential item [%s] range [%d,%d - %d,%d] as possibly to the left of item [%s] range [%d,%d - %d,%d] because too long (ends past current item)", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(),(int)matchRect.origin.x, (int)matchRect.origin.y, (int)(matchRect.origin.x + matchRect.size.width - 1), (int)(matchRect.origin.y + matchRect.size.height - 1), toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(),(int)matchAfterRect.origin.x, (int)matchAfterRect.origin.y, (int)(matchAfterRect.origin.x + matchAfterRect.size.width - 1), (int)(matchAfterRect.origin.y + matchAfterRect.size.height - 1));
//                    continue; // Only looking to the left!
//                }
                // No width data, suspicious, better declare that the item has no item just before
                if (estimatedCharWidth == 0)
                    return MatchPtr();
                if ((acceptableNumberOfSpaces < 0)
                    || (spaceBetween/estimatedCharWidth < acceptableNumberOfSpaces)) {
                    // Remember this item?
                    if ((bestDistanceAwayXAxis < 0) || (spaceBetween < bestDistanceAwayXAxis)) {
                        // Not so fast: it is possible that there is a slight overlap between matchAfter and this one - BUT there also is elsewhere a match entirely to the left of this one, with a better Y overlap. If so, skip this one (let it get associated with that other match later)
                        bool skipThisOne = false;
                        for (int k=0; k<matches.size(); k++) {
                            if ((k == i) || (k == j)) continue;
                            MatchPtr alternateMatchBefore = matches[k];
                            if (std::find(ex->begin(), ex->end(), alternateMatchBefore) != ex->end())
                                continue;    // Already taken (but not yet deleted), skip
                            CGRect alternateMatchBeforeRect = OCRResults::matchRange(alternateMatchBefore);
                            if (OCRResults::overlappingRectsYAxisAnd(alternateMatchBeforeRect, matchRect)) {
                                float alternateSpaceBetween = rectLeft(matchRect) - rectRight(alternateMatchBeforeRect) - 1;
                                if (alternateSpaceBetween > 0) {
                                    float alternateOverlapYAmount = overlapAmountRectsYAxisAnd(alternateMatchBeforeRect, matchRect);
                                    float heightTallestAlternateBefore = OCRResults::heightTallestChar(alternateMatchBefore);
                                    // TODO need to ignore items with apparent better Y overlap in favor or item much closer, for two reasons:
                                    // 1. Slant calculations could lead us astray, especially with short items and has a greater impact as items are further apart
                                    // 2. When overlap is 50%+ and items are close (say 3-5 letters away), no need to even consider anything further away!
                                    if (alternateOverlapYAmount > overlapYAmount) {
                                        if ((heightTallestCandidate <= 0) || (heightTallestAlternateBefore <= 0) || (heightTallestAlternateBefore > heightTallestMatchAfter * 2)) {
                                            DebugLog("findItemJustBeforeMatchAtIndexInMatchesExcluding: NOT ignoring item [%s] height=%.0f Y overlapping with [%s] height=%.0f because it overlaps more with item [%s] height=%.0f - weird heights!", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), heightTallestCandidate, toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(), heightTallestMatchAfter, toUTF8(getStringFromMatch(alternateMatchBefore, matchKeyText)).c_str(), heightTallestAlternateBefore);
                                        } else {
                                            DebugLog("findItemJustBeforeMatchAtIndexInMatchesExcluding: ignoring item [%s] Y overlapping with [%s] because it overlaps more with item [%s]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), toUTF8(getStringFromMatch(matchAfter, matchKeyText)).c_str(), toUTF8(getStringFromMatch(alternateMatchBefore, matchKeyText)).c_str());
                                            skipThisOne = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        if (skipThisOne) continue;
                        bestDistanceAwayXAxis = spaceBetween;
                        bestMatchBefore = d;
                    }
                }
            }
        }

        return bestMatchBefore;
    }
}

MatchPtr OCRResults::findItemJustBeforeMatchInMatchesExcluding(MatchPtr& match, MatchVector& matches, MatchVectorPtr ex, int acceptableNumberOfSpaces, bool blocksSet) {

	MatchVector::iterator pos = std::find(matches.begin(), matches.end(), match);

    int i = (int)(pos - matches.begin());

	if (pos == matches.end())
		return MatchPtr();
	else
		return findItemJustBeforeMatchAtIndexInMatchesExcluding(i, matches, ex, acceptableNumberOfSpaces, blocksSet);
}

void OCRResults::setBlockandLineandColumnandPositioninMatch(int block, int line, int col, int pos, MatchPtr  m) {
	SmartPtr<int> tmpBlock(new int(block));
	SmartPtr<int> tmpLine(new int(line));
	SmartPtr<int> tmpColumn(new int(col));
	SmartPtr<int> tmpPos(new int(pos));

	m[matchKeyBlock] = tmpBlock.castDown<EmptyType>();
	m[matchKeyLine] = tmpLine.castDown<EmptyType>();
	m[matchKeyColumn] = tmpColumn.castDown<EmptyType>();
	m[matchKeyPosition] = tmpPos.castDown<EmptyType>();
}

string OCRResults::stringForOCRElementType(OCRElementType tp) {

	if (tp == OCRElementTypeUnknown)
		return ("Ignored");
	else if (tp == OCRElementTypeInvalid)
		return ("Invalid");
	else if (tp == OCRElementTypeNotAnalyzed)
		return ("Not Analyzed");
    else if (tp == OCRElementTypeProductNumber)
		return ("Product Number");
    else if (tp == OCRElementTypeProductDescription)
		return ("Product Description");
	else if (tp == OCRElementTypePrice)
		return ("Price");
	else if (tp == OCRElementTypePriceDiscount)
		return ("Discount");
    else if (tp == OCRElementTypePotentialPriceDiscount)
        return ("Potential Price Discount");
	else if (tp == OCRElementTypeSubtotal)
		return ("Subtotal");
	else if (tp == OCRElementTypeTotal)
		return ("Total");
    else if (tp == OCRElementTypeDate)
		return ("Date");
    else if (tp == OCRElementTypeProductQuantity)
		return ("Quantity");
    else if (tp == OCRElementTypeIgnoredProductNumber)
		return ("Ignored Product");
    else if (tp == OCRElementTypeMarkerOfPriceOnNextLine)
        return ("Marker Of Price On NextLine");
    else if (tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine)
        return ("Product Number With Marker Of Price On Next Line");
    else if (tp == OCRElementTypeDateMonth)
		return ("Month");
    else if (tp == OCRElementTypeDateMonthLetters)
		return ("Month (letters)");
    else if (tp == OCRElementTypeDateYear)
		return ("Year");
    else if (tp == OCRElementTypeDateDay)
		return ("Day");
    else if (tp == OCRElementTypeDateHour)
		return ("Hour");
    else if (tp == OCRElementTypeDateMinutes)
		return ("Minutes");
    else if (tp == OCRElementSubTypeDollar)
		return ("$");
    else if (tp == OCRElementTypeProductPrice)
        return ("Product Price");
    else if (tp == OCRElementTypePhoneNumber)
        return ("Phone Number");
    else if (tp == OCRElementSubTypeDateDateDivider)
        return ("Date Divider");
    else if (tp == OCRElementTypeDateAMPM)
        return ("AP/PM");

	DebugLog("OCRGlobalResults::stringForOCRElementType: received unexpected type [%d]", tp);
	return ("Invalid");
}

// Searches all matches on given line for the matchKeyGlobalLine property
int OCRResults::globalLineForLineAtIndex(int lineIndex, MatchVector &matches) {

//    MatchPtr d = matches[lineIndex];
//    if (d.isExists(matchKeyGlobalLine)) {
//        return (getIntFromMatch(d, matchKeyGlobalLine));
//    } else
//        return -1;

    int lineStart = indexFirstElementOnLineAtIndex(lineIndex, &matches);
    int lineEnd = indexLastElementOnLineAtIndex(lineIndex, &matches);
    for (int i=lineStart; i<=lineEnd; i++) {
        MatchPtr d = matches[i];
        if (d.isExists(matchKeyGlobalLine)) {
            return (getIntFromMatch(d, matchKeyGlobalLine));
        }
    }
    return -1;
}

/* Return value: TRUE if existing results were modified in a "real" way, for example setting text, text2 or text3 of an item to a new value, but not when just increasing the confidence or the count of text/text2/text3
    - newInfo: set to TRUE only when changes are impactful, e.g. changing text either due to new text coming in with higher confidence than current text, or new text equal to existing text2 (increasing its count and getting upgraded)
 */
bool OCRResults::mergeNewItemWithExistingItem (int indexNewItem, MatchVector &newMatches, int indexExistingItem, MatchVector &existingMatches) {

    bool updatedExisting = false;
    MatchPtr newResultsItem = newMatches[indexNewItem];
    float newConfidence = getFloatFromMatch(newResultsItem, matchKeyConfidence);
    MatchPtr existingResultsItem = existingMatches[indexExistingItem];
    OCRElementType tpToCreate = (OCRElementType)getIntFromMatch(existingResultsItem, matchKeyElementType);
    int targetExistingLineNumber = getIntFromMatch(existingResultsItem, matchKeyLine);
    float existingConfidence = getFloatFromMatch(existingResultsItem, matchKeyConfidence);
//                        if (round(existingConfidence) == 100) {
//                            // Never try to update such an item (usually means it's one of 3 grouped items: confirmed product price + quantity below + price per item below, all matching)
//                            continue;
//                        }
    wstring newText = getStringFromMatch(newResultsItem, matchKeyText);
    wstring existingText = getStringFromMatch(existingResultsItem, matchKeyText);
//#if DEBUG
//    if ((newText == L"1029.99") && (existingText == L"29.99")) {
//        DebugLog("");
//    }
//#endif
    bool newWins = false, replace = false;
    bool win = winnerExistingVersusNewMatch(existingResultsItem, newResultsItem, &newWins, &replace);
    if (win && !replace) {
        // In this case we just want to keep both items but make sure the winner is on top: easiest is to assign the higher priority to the winner
        if ((newWins && (newConfidence < existingConfidence)) || (!newWins && (existingConfidence < newConfidence))) {
            newResultsItem[matchKeyConfidence] = (SmartPtr<float> (new float(existingConfidence))).castDown<EmptyType>();
            existingResultsItem[matchKeyConfidence] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
        }
    } else if (win && replace) {
        if (!newWins)
            return (updatedExisting);   // Just ignore the new item, current is deemed definitely superior
        // Completely replace existing item with new values
        MergingLog("importNewResults: nuking existing [%s] item text [%s] in favor of new item [%s] deemed definitely superior [existing conf=%.0f, new conf=%.0f] at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(existingText).c_str(), toUTF8(newText).c_str(),  existingConfidence, newConfidence, targetExistingLineNumber);
        existingResultsItem[matchKeyText] = (SmartPtr<wstring> (new wstring(newText))).castDown<EmptyType>();
        if (newResultsItem.isExists(matchKeyFullLineText)) {
            existingResultsItem[matchKeyFullLineText] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
        } else {
            if (existingResultsItem.isExists(matchKeyFullLineText))
                existingResultsItem->erase(matchKeyFullLineText);
        }
        existingResultsItem[matchKeyCountMatched] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
        existingResultsItem[matchKeyConfidence] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
        if (newResultsItem.isExists(matchKeyStatus)) {
            unsigned long status = getUnsignedLongFromMatch(newResultsItem, matchKeyStatus);
            existingResultsItem[matchKeyStatus] = (SmartPtr<unsigned long> (new unsigned long(status))).castDown<EmptyType>();
        }
        if (existingResultsItem.isExists(matchKeyText2)) {
            existingResultsItem->erase(matchKeyText2);
            existingResultsItem->erase(matchKeyConfidence2);
            existingResultsItem->erase(matchKeyCountMatched2);
            if (existingResultsItem.isExists(matchKeyFullLineText2))
                existingResultsItem->erase(matchKeyFullLineText2);
        }
        if (existingResultsItem.isExists(matchKeyText3)) {
            existingResultsItem->erase(matchKeyText3);
            existingResultsItem->erase(matchKeyConfidence3);
            existingResultsItem->erase(matchKeyCountMatched3);
            if (existingResultsItem.isExists(matchKeyFullLineText3))
                existingResultsItem->erase(matchKeyFullLineText3);
        }
        // 2015-11-15 we now set text2 with alternate OCR text, if exists set in existing results
        if (newResultsItem.isExists(matchKeyText2)) {
            existingResultsItem[matchKeyText2] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyText2)))).castDown<EmptyType>();
            if (newResultsItem.isExists(matchKeyConfidence2))
                existingResultsItem[matchKeyConfidence2] = (SmartPtr<float> (new float(getFloatFromMatch(newResultsItem, matchKeyConfidence2)))).castDown<EmptyType>();
            else
                existingResultsItem[matchKeyConfidence2] = (SmartPtr<float> (new float(1.00))).castDown<EmptyType>(); // No conf for alternate OCR text, set to low confidence
            existingResultsItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
        }
        return true;    // Updated existing
    }
    if (existingText == newText) {
        // We got that text already, just increment its count and confidence (if new conf higher)
        existingResultsItem[matchKeyCountMatched] = (SmartPtr<int> (new int(getIntIfExists(existingResultsItem, matchKeyCountMatched, 1) + 1))).castDown<EmptyType>();
#if DEBUG
        if (newConfidence > existingConfidence) {
            MergingLog("importNewResults: updating existing [%s] item text [%s] - increment count to %d and increasing confidence from existing conf=%.0f to new conf=%.0f at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), getIntIfExists(existingResultsItem, matchKeyCountMatched, 1)+1, existingConfidence, newConfidence, targetExistingLineNumber);
        } else {
            MergingLog("importNewResults: updating existing [%s] item text [%s] - increment count to %d (new confidence %.0f lower than existing %.0f) at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), getIntIfExists(existingResultsItem, matchKeyCountMatched, 1)+1, newConfidence, existingConfidence, targetExistingLineNumber);
        }
#endif
        if (newConfidence > existingConfidence) {
            // Update with increased confidence
            existingResultsItem[matchKeyConfidence] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
            if (newResultsItem.isExists(matchKeyFullLineText))
                existingResultsItem[matchKeyFullLineText] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
            //updatedExisting = true;
        }
        // 2015-11-15 we now set text2 with alternate OCR text, if exists set in existing results
        if (newResultsItem.isExists(matchKeyText2)) {
            wstring alternateOCRText = getStringFromMatch(newResultsItem, matchKeyText2);
            float alternateConfidence = 1.00;
            if (newResultsItem.isExists(matchKeyConfidence2))
                alternateConfidence = getFloatFromMatch(newResultsItem, matchKeyConfidence2);
            // Matches existing text2?
            if (existingResultsItem.isExists(matchKeyText2)) {
                if (getStringFromMatch(existingResultsItem, matchKeyText2) == alternateOCRText) {
                    // Just increment count. No need to check confidence bc we assume alternate OCR text is low confidence
                    existingResultsItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(getIntFromMatch(existingResultsItem, matchKeyCountMatched2) + 1))).castDown<EmptyType>();
                } else {
                    // Different text2. Try text3
                    if (existingResultsItem.isExists(matchKeyText3)) {
                        if (getStringFromMatch(existingResultsItem, matchKeyText3) == alternateOCRText) {
                            // Just increment count. No need to check confidence bc we assume alternate OCR text is low confidence
                            existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(getIntFromMatch(existingResultsItem, matchKeyCountMatched3 ) + 1))).castDown<EmptyType>();
                        }
                        // Text3 exists and is different - ignore alternate OCR text
                    }
                }
            } else {
                // No existing text2
                existingResultsItem[matchKeyText2] = (SmartPtr<wstring> (new wstring(alternateOCRText))).castDown<EmptyType>();
                existingResultsItem[matchKeyConfidence2] = (SmartPtr<float> (new float(alternateConfidence))).castDown<EmptyType>();
                existingResultsItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
            }
        }
        return (updatedExisting);
    }
    // new text != existing text
    else {
        // New text didn't match matchKeyText. Perhaps matchKeyText2?
        if (existingResultsItem.isExists(matchKeyText2)) {
            wstring existingText2 = getStringFromMatch(existingResultsItem, matchKeyText2);
            float existingConfidence2 = getFloatFromMatch(existingResultsItem, matchKeyConfidence2);
            if (newText == existingText2) {
                // We got that text already, just increment its count and confidence (if new conf higher)
                existingResultsItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(getIntIfExists(existingResultsItem, matchKeyCountMatched2, 1) + 1))).castDown<EmptyType>();
#if DEBUG
                if (newConfidence > existingConfidence2) {
                    MergingLog("importNewResults: updating existing [%s] item text2 [%s] - increment count to %d and increasing confidence from existing conf=%.0f to new conf=%.0f at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), getIntIfExists(existingResultsItem, matchKeyCountMatched2, 1), existingConfidence2, newConfidence, targetExistingLineNumber);
                } else {
                    MergingLog("importNewResults: updating exiting [%s] item text [%s] - increment count to %d (new confidence %.0f lower than existing %.0f) at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), getIntIfExists(existingResultsItem, matchKeyCountMatched2, 1)+1, newConfidence, existingConfidence2, targetExistingLineNumber);
                }
#endif
                if (newConfidence > existingConfidence2) {
                    existingResultsItem[matchKeyConfidence2] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
                    if (newResultsItem.isExists(matchKeyFullLineText))
                        existingResultsItem[matchKeyFullLineText2] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
                    //updatedExisting = true;
                }
                sortText123(existingResultsItem);
                // 2015-11-15 we now set text2 with alternate OCR text in fresh frames, if exists set in existing results
                if (newResultsItem.isExists(matchKeyText3)) {
                    wstring alternateOCRText = getStringFromMatch(newResultsItem, matchKeyText2);
                    float alternateConfidence = 1.00;
                    if (newResultsItem.isExists(matchKeyConfidence2))
                        alternateConfidence = getFloatFromMatch(newResultsItem, matchKeyConfidence2);
                    // Matches existing text3?
                    if (existingResultsItem.isExists(matchKeyText3)) {
                        if (getStringFromMatch(existingResultsItem, matchKeyText3) == alternateOCRText) {
                            // Just increment count. No need to check confidence bc we assume alternate OCR text is low confidence
                            existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(getIntFromMatch(existingResultsItem, matchKeyCountMatched3)))).castDown<EmptyType>();
                        }
                        // Different text3 - ignore alternate OCR text
                    } else {
                        // No existing text3
                        existingResultsItem[matchKeyText3] = (SmartPtr<wstring> (new wstring(alternateOCRText))).castDown<EmptyType>();
                        existingResultsItem[matchKeyConfidence3] = (SmartPtr<float> (new float(alternateConfidence))).castDown<EmptyType>();
                        existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
                    }
                } // alternate OCR text exists
                return (updatedExisting);
            } // End new text == text 2
            // New text different than text2. Check text3
            else {
                if (existingResultsItem.isExists(matchKeyText3)) {
                    wstring existingText3 = getStringFromMatch(existingResultsItem, matchKeyText3);
                    int existingMatchCount3 = getIntIfExists(existingResultsItem, matchKeyCountMatched3, 1);
                    float existingConfidence3 = getFloatFromMatch(existingResultsItem, matchKeyConfidence3);
                    if (newText == existingText3) {
                        // We got that text already, just increment its count and confidence (if new conf higher)
                        existingMatchCount3++;
                        existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(existingMatchCount3))).castDown<EmptyType>();
                        if (newConfidence > existingConfidence3) {
                            existingResultsItem[matchKeyConfidence3] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
                            if (newResultsItem.isExists(matchKeyFullLineText))
                                existingResultsItem[matchKeyFullLineText3] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
                            //updatedExisting = true;
                            existingConfidence3 = newConfidence;
                        }
                        sortText123(existingResultsItem);
                        return (updatedExisting);
                    } // End newText == text3
                    else {
                        // New text different than text3. Nuke text3 only if has lower confidence and count 1
                        if ((tpToCreate == OCRElementTypePrice) && (checkPriceQuantityLinkage(indexExistingItem, existingMatches, newText, &this->retailerParams)))
                            return (updatedExisting);
                        if ((existingMatchCount3 == 1) && (newConfidence > existingConfidence3)) {
                            MergingLog("importNewResults: multitext - replacing existing [%s] item text3 [%s] conf=%.0f count=%d with [%s] conf=%.0f count=%d at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(existingText3).c_str(), existingConfidence3, existingMatchCount3, toUTF8(newText).c_str(), newConfidence, 1, targetExistingLineNumber);
                            existingResultsItem[matchKeyText3] = (SmartPtr<wstring> (new wstring(newText))).castDown<EmptyType>();
                            existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
                            existingResultsItem[matchKeyConfidence3] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
                            if ((tpToCreate != OCRElementTypeProductDescription) || !this->retailerParams.noProductNumbers) //PQ912
                                updatedExisting = true;
                        }
                        sortText123(existingResultsItem);
                        return (updatedExisting);
                    } // New text != text 2
                } // End There is a text3
                // No text 3
                else {
                    if ((tpToCreate == OCRElementTypePrice) && (checkPriceQuantityLinkage(indexExistingItem, existingMatches, newText, &this->retailerParams)))
                        return (updatedExisting);
                    // Just set new text as text3
//#if DEBUG
//                    if ((newText == L"065676136851") || (newText == L"065876136851")) {
//                        DebugLog("");
//                    }
//#endif
                    MergingLog("importNewResults: multitext - creating [%s] item text3 [%s] conf=%.0f count=%d at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), newConfidence, 1, targetExistingLineNumber);
                    existingResultsItem[matchKeyText3] = (SmartPtr<wstring> (new wstring(newText))).castDown<EmptyType>();
                    if (newResultsItem.isExists(matchKeyFullLineText))
                        existingResultsItem[matchKeyFullLineText3] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
                    existingResultsItem[matchKeyConfidence3] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
                    existingResultsItem[matchKeyCountMatched3] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
                    sortText123(existingResultsItem); // Re-sort as needed
                    if ((tpToCreate != OCRElementTypeProductDescription) && !this->retailerParams.noProductNumbers)
                        updatedExisting = true;
                    return (updatedExisting);
                }
            } // end new text != text 2
            // new text == text 2
        } // End There is a text2
        // No text 2
        else {
            if ((tpToCreate == OCRElementTypePrice) && (checkPriceQuantityLinkage(indexExistingItem, existingMatches, newText, &this->retailerParams)))
                return (updatedExisting);
            // Just set new text as text2
//#if DEBUG
//            if ((newText == L"065676136851") || (newText == L"065876136851")) {
//                DebugLog("");
//            }
//#endif
            MergingLog("importNewResults: multitext - creating [%s] item text2 [%s] conf=%.0f count=%d at line %d", (stringForOCRElementType(tpToCreate)).c_str(), toUTF8(newText).c_str(), newConfidence, 1, targetExistingLineNumber);
            existingResultsItem[matchKeyText2] = (SmartPtr<wstring> (new wstring(newText))).castDown<EmptyType>();
            if (newResultsItem.isExists(matchKeyFullLineText))
                existingResultsItem[matchKeyFullLineText2] = (SmartPtr<wstring> (new wstring(getStringFromMatch(newResultsItem, matchKeyFullLineText)))).castDown<EmptyType>();
            existingResultsItem[matchKeyConfidence2] = (SmartPtr<float> (new float(newConfidence))).castDown<EmptyType>();
            existingResultsItem[matchKeyCountMatched2] = (SmartPtr<int> (new int(1))).castDown<EmptyType>();
            sortText123(existingResultsItem); // Re-sort as needed
            updatedExisting = true;
            return (updatedExisting);
        }
    } // new text != existing text

} // mergeNewItemWithExistingItem

void OCRResults::countCharsAroundItemAtIndex(int index, MatchVector &matches, int &charsBefore, int &charsAfter) {
    int startLine = indexFirstElementOnLineAtIndex(index, &matches);
    int endLine = indexLastElementOnLineAtIndex(index, &matches);
    charsBefore = 0; charsAfter = 0;
    for (int i=startLine; i<=endLine; i++) {
        if (i == index) continue;
        MatchPtr d = matches[i];
        wstring itemText = getStringFromMatch(d, matchKeyText);
        if (i < index)
            charsBefore += itemText.length();
        else
            charsAfter += itemText.length();
    }
    return;
}

// Merges newMatch into existingMatch. Assumes caller has ascertained that newMatch follows existingMatch. Does NOT delete the 2nd match, caller needs to do that.
bool OCRResults::mergeM1WithM2(MatchPtr existingMatch, MatchPtr newMatch) {
	// Merge text
    wstring existingText = getStringFromMatch(existingMatch, matchKeyText);
	wstring newText = getStringFromMatch(newMatch, matchKeyText);
    
#if DEBUG
    if ((existingText == L"$33.11. 32 27") && (newText == L"11")) {
        DebugLog("");
    }
#endif

    RectVectorPtr existingRects = existingMatch[matchKeyRects].castDown<RectVector>();
    RectVectorPtr newRects = newMatch[matchKeyRects].castDown<RectVector>();
    
    // Test if existing match ends into new match. If so we need to pick just one set of rects / text for the overlap! Hard to know which to pick, for now let's just go with new match, trimming existing by the right number of elements. See https://drive.google.com/open?id=0B4jSQhcYsC9VS0V4QmpLUFVSYzg
    Rect firstNewRect = *(*newRects)[0];
    int numToTrim = 0;
    for (int i=(int)existingRects->size()-1; i>=0; i--) {
        Rect r = *(*existingRects)[i];
        float overlapAmount = r.origin.x + r.size.width - 1 - firstNewRect.origin.x + 1;
        if (overlapAmount > r.size.width * 0.50)
            numToTrim++;
        else
            break;
    }
    if (numToTrim > 0) {
        for (int j=0; j<numToTrim; j++)
            existingRects->erase(existingRects->begin() + existingRects->size() - 1);
#if DEBUG
        wstring trimmedText = existingText.substr(existingText.length()-numToTrim, numToTrim);
        DebugLog("mergeM1WithM2: merging [%s] + [%s] while dropping [%s]", toUTF8(existingText).c_str(), toUTF8(newText).c_str(), toUTF8(trimmedText).c_str());
#endif
        existingText = existingText.substr(0, existingText.length()-numToTrim);
    }
	// Merge rects
	copyRects(newRects->begin(), (int)newRects->size(), *existingRects);

	wstring mergedText = existingText + newText;
	//DebugLog("mergedMatch: merging [%s] into [%s], result [%s]", toUTF8(newText).c_str(), toUTF8(existingText).c_str(), toUTF8(mergedText).c_str());
    existingMatch[matchKeyN] = SmartPtr<size_t>(new size_t(existingRects->size())).castDown<EmptyType>();
	existingMatch[matchKeyText] = SmartPtr<wstring>(new wstring(mergedText)).castDown<EmptyType>();
    
    // Set confidence as a weighted average of the two matches
    if (existingMatch.isExists(matchKeyConfidence) && newMatch.isExists(matchKeyConfidence)) {
        float combinedConfidence = (getFloatFromMatch(existingMatch, matchKeyConfidence) * existingText.length() + getFloatFromMatch(newMatch, matchKeyConfidence) * newText.length()) / (existingText.length() + newText.length());
        existingMatch[matchKeyConfidence] = SmartPtr<float>(new float(combinedConfidence)).castDown<EmptyType>();
    }

    // Set percentVerticalLines and numNonSpaceLetter as a weighted average of the two matches
    int numNonSpaceLettersExisting = 0, numNonSpaceLettersNew = 0;
    float percentVerticalExisting = -1, percentVerticalNew = -1;
    int numNonSpaceLettersCombined = 0; float percentVerticalCombined = -1;
    if (existingMatch.isExists(matchKeyNumNonSpaceLetters))
        numNonSpaceLettersExisting = getIntFromMatch(existingMatch, matchKeyNumNonSpaceLetters);
    else
        numNonSpaceLettersExisting = 0;
    if (existingMatch.isExists(matchKeyPercentVerticalLines))
        percentVerticalExisting = getFloatFromMatch(existingMatch, matchKeyPercentVerticalLines);
    else
        percentVerticalExisting = -1;
    if (newMatch.isExists(matchKeyNumNonSpaceLetters))
        numNonSpaceLettersNew = getIntFromMatch(newMatch, matchKeyNumNonSpaceLetters);
    else
        numNonSpaceLettersNew = 0;
    if (newMatch.isExists(matchKeyPercentVerticalLines))
        percentVerticalNew = getFloatFromMatch(newMatch, matchKeyPercentVerticalLines);
    else
        percentVerticalNew = -1;

    if (percentVerticalExisting >= 0) numNonSpaceLettersCombined += numNonSpaceLettersExisting;
    if (percentVerticalNew >= 0) numNonSpaceLettersCombined += numNonSpaceLettersNew;
    if (numNonSpaceLettersCombined > 0) {
        percentVerticalCombined = (((percentVerticalExisting >= 0)? percentVerticalExisting * (float)numNonSpaceLettersExisting:0 )+ ((percentVerticalNew >= 0)? percentVerticalNew * (float)numNonSpaceLettersNew:0))/numNonSpaceLettersCombined;
    }
    existingMatch[matchKeyPercentVerticalLines] = SmartPtr<float>(new float(percentVerticalCombined)).castDown<EmptyType>();
    existingMatch[matchKeyNumNonSpaceLetters] = SmartPtr<int>(new int(numNonSpaceLettersCombined)).castDown<EmptyType>();


	return true;
} // mergeM1WithM2

bool OCRResults::typeRelevantToUser(OCRElementType tp, retailer_t *retailerParams) {
    if ((tp == OCRElementTypeProductNumber) || (tp == OCRElementTypeProductQuantity) || (tp == OCRElementTypeProductPrice) || (tp == OCRElementTypePrice) || (tp == OCRElementTypeProductDescription) || (tp == OCRElementTypeDate) || (tp == OCRElementTypeSubtotal) || (tp == OCRElementTypeTotalWithoutTax) || (tp == OCRElementTypeMergeablePattern1) || (tp == OCRElementTypePhoneNumber))
        return true;
    else
        return false;
}

//end // OCR results
}
