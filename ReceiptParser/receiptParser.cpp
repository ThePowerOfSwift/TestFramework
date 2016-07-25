//
//  receiptParser.cpp
//
//  Created by Patrick Questembert on 3/18/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

#define CODE_CPP 1 // Indicates to shared include files source file is C++ (not OBJ-C)

#import "common.hh"
#import "ocr.hh"
#import "OCRClass.hh"
#import "receiptParser.hh"
#import "OCRUtils.hh"
#import "OCRUtils.h"
#import "RegexUtils.hh"
#import "LineValidation.hh"
#import "WFScanDelegate.h"

#if ANDROID
#define atof(x) myAtoF(x)
#endif

namespace ocrparser {

static bool firstFrame = true;
static OCRResults globalResults;

void resetParser() {
    firstFrame = true;
    globalResults = *new OCRResults(); // Not needed, but just in case, to make extra sure we jettison any ancient results
}

MatchVector getCurrentOCRResults() {
    return globalResults.matchesAddressBook;
}

vector<simpleMatch> exportCurrentOCRResultsVector() {
    vector<simpleMatch> result;
    MatchVector matches = globalResults.matchesAddressBook;
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        wstring text = getStringFromMatch(d, matchKeyText);
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        int n = getIntFromMatch(d, matchKeyN);
        simpleMatch m;
        m.text = text; m.type = tp; m.n = n;
        result.push_back(m);
    }
    return result;
}

float serverPriceForProduct(scannedProduct product, vector<serverPrice> &serverPrices) {
    for (int k=0; k<serverPrices.size(); k++) {
        serverPrice sPrice = serverPrices[k];
        if (sPrice.sku == product.sku) {
            return sPrice.price;
        }
    }
    return 0.0;
}

bool sanityPriceCheck(float scannedPrice, int quantity, float serverPrice, retailer_s *retailerParams, int lineNumber, vector<int> &linesProductsWithPossibleQuantities, float &suggestedPrice, int &suggestedQuantity) {

    return true;

    if (serverPrice <= 0.01)
        return true;

    if (scannedPrice < serverPrice / retailerParams->sanityCheckFactor) {
        DebugLog("sanityPriceCheck: Rejecting %.2f scanned price with server price %.2f, too low with sanity factor=%.2f", scannedPrice, serverPrice, retailerParams->sanityCheckFactor);
        suggestedPrice = 0;
        suggestedQuantity = 0;
        return false;
    }
    
     if ((scannedPrice > serverPrice * retailerParams->sanityCheckFactor) || ((serverPrice <= 5) && (scannedPrice > serverPrice * 2.0))) {
        // Price too high - give it a chance if this might be a case of a missed quantity
        bool mayHaveQuantity = (quantity > 1);
        if (!mayHaveQuantity) {
            for (int i=0; i<linesProductsWithPossibleQuantities.size(); i++) {
                if (linesProductsWithPossibleQuantities[i] == lineNumber) {
                    mayHaveQuantity = true;
                    break;
                }
            }
        }
        if (mayHaveQuantity) {
            int possibleQuantity = round(scannedPrice * quantity / serverPrice);
            if ((possibleQuantity > 1) && (abs(possibleQuantity * serverPrice - scannedPrice * quantity) <= 0.01)) {
                suggestedQuantity = possibleQuantity;
                suggestedPrice = serverPrice;
                DebugLog("sanityPriceCheck: Replacing %.2f scanned price with server price %.2f & quantity=%d", scannedPrice, serverPrice, suggestedQuantity);
                return false;
            }
        }
        // Test again, for the sake of the sub-$5.00 scanned price whereby we tried to find a quantity but failed
        if (scannedPrice > serverPrice * retailerParams->sanityCheckFactor) {
            DebugLog("sanityPriceCheck: Rejecting %.2f scanned price with server price %.2f, too high with sanity factor=%.2f", scannedPrice, serverPrice, retailerParams->sanityCheckFactor);
            suggestedPrice = 0;
            suggestedQuantity = 0;
            return false;
        }
    }
    
    char *tmpPriceCStr = (char *)malloc(100);
    
    sprintf(tmpPriceCStr, "%.2f", scannedPrice);
    wstring scannedPriceStr = toUTF32(tmpPriceCStr);
    
    sprintf(tmpPriceCStr, "%.2f", serverPrice);
    wstring serverPriceStr = toUTF32(tmpPriceCStr);
    
    free(tmpPriceCStr);
    
    if (((serverPriceStr.length() - scannedPriceStr.length() == 1) && (serverPriceStr.substr(1, wstring::npos) == scannedPriceStr))
        || ((scannedPriceStr.length() - serverPriceStr.length() == 1) && (scannedPriceStr.substr(1, wstring::npos) == serverPriceStr))) {
        DebugLog("sanityPriceCheck: Rejecting %.2f scanned price with %.2f server price because of presumed single digit error", scannedPrice, serverPrice);
        return false;
    }
    return true;
}

/* Builds product dictionaries from a set of C++ scanning results, ignoring all items that don't represent a product. Each product has these properties:
    - sku (NSString): required, the number for this product at the retailer
    - description (NSString): optional, always exists on receipts but may have been missed by the OCR. Currently when we find a product number and a price on the same line we return whatever text is found between them as the product description
    - price (NSNumber - float): optional, always exists on receipts but may have been missed by the OCR
    - quantity (NSNumber - int): always set to 1 for now
 */
void exportCurrentScanResults (vector<scannedProduct> *productsVectorPtr, vector<float> *discountsVectorPtr, float *subTotalPtr, float *totalPtr, wstring *datePtr, vector<scannedItem> *phonesVectorPtr, vector<serverPrice> serverPrices) {

	MatchVector matches = globalResults.matchesAddressBook;
    retailer_s *retailerParams = &globalResults.retailerParams;
	
    vector<scannedProduct> products;
    vector<int> productLinesWithPossibleQuantity;    // Subject of lines where products were found, for products which may have a (possibly missed) quantity
    vector<float> discounts;
    vector<scannedItem> phones;
    *subTotalPtr = -1;
    *datePtr = wstring();
    
    bool ignoreDiscountsBecauseSubtotalFound = false;
    
    float totalPrices = 0, subtotalValue = -1;
    int numPrices = 0;
    int indexMissingPrice = -1;
    int indexOfLastProduct = -1;
    bool abortMissingPriceOptimization = false;
    vector<int>excludedLines;   // Lines to be ignored (i.e. not processed to add elements to the iOS output)
    float totalMinusTaxValue = -1;
	  
	for (int i=0; i < matches.size(); i++) {
        MatchPtr item = matches[i];
		  
        OCRElementType tp = getElementTypeFromMatch(item, matchKeyElementType);
        
        int currentLine = -1;

		if (item.isExists(matchKeyLine))
			currentLine = getIntFromMatch(item, matchKeyLine);
        
        if (std::find(excludedLines.begin(), excludedLines.end(), currentLine) != excludedLines.end()) {
            // Skip
            i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, &matches);
            continue;
        }
        
        if (tp == OCRElementTypeTotal) { //PQTODO I thought "OCRElementTypeTotal" is used when we FAILED to find the tax amount? Anyhow, this will be N/A once I detect TOTAL and tax/rebates the right way
            // This is the total line, set only if we found a price just above (presumed to be the tax amount)
            // Need to find the price property on that same line
            int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matches, NULL);
            if (priceIndex > i) {
                MatchPtr subtotalPriceItem = matches[priceIndex];
                wstring tmpSubtotalPriceItemText = getStringFromMatch(subtotalPriceItem, matchKeyText);
                float tmpPriceValue = atof(toUTF8(tmpSubtotalPriceItemText).c_str());
                totalMinusTaxValue = tmpPriceValue;
                if (totalPtr != NULL)
                    *totalPtr = tmpPriceValue;
            }
        }
        
        if (tp == OCRElementTypeBal) {
            // Only for Kmart right now, e.g. TAX 1.79 BAL 4.75
            // Balance amount is the next item
            if (i == matches.size()-1) break;
            int balIndex = i;
            int balPriceIndex = i+1;
            MatchPtr balPriceItem = matches[balPriceIndex];
            OCRElementType balPriceType = getElementTypeFromMatch(balPriceItem, matchKeyElementType);
            if (((balPriceType == OCRElementTypePrice) || (balPriceType == OCRElementTypeBalPrice)) && (getIntFromMatch(balPriceItem, matchKeyLine) == currentLine)) {
                wstring balPriceText = getStringFromMatch(balPriceItem, matchKeyText);
                float balPriceValue = atof(toUTF8(balPriceText).c_str());
                int taxIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypeTax, &matches, NULL);
                if ((taxIndex >= 0) && (taxIndex < balIndex - 1) && (getIntFromMatch(matches[taxIndex], matchKeyLine) == currentLine)) {
                    int taxPriceIndex = taxIndex + 1;
                    MatchPtr taxPriceItem = matches[taxPriceIndex];
                    OCRElementType taxPriceType = getElementTypeFromMatch(taxPriceItem, matchKeyElementType);
                    if ((taxPriceType == OCRElementTypePrice) || (taxPriceType == OCRElementTypeTaxPrice)) {
                        wstring taxPriceText = getStringFromMatch(taxPriceItem, matchKeyText);
                        float taxPriceValue = atof(toUTF8(taxPriceText).c_str());
                        if (taxPriceValue < balPriceValue) {
                            totalMinusTaxValue = balPriceValue - taxPriceValue;
                        }
                    }
                }
            }
        }
        
        if (tp == OCRElementTypeSubtotal) {
            // Create struct to hold the attributes of this product - but only if there is no other subtotal later, as in the Walmart case
            bool foundOtherProduct = false;
            for (int j=i+1; j<matches.size(); j++) {
                MatchPtr dd = matches[j];
                if (getElementTypeFromMatch(dd, matchKeyText) == OCRElementTypeSubtotal) {
                    foundOtherProduct = true;
                    break;
                }
            }
            if (!foundOtherProduct) {
                // Need to find the price property on that same line
                int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &matches, NULL);
                if (priceIndex > 0) {
                    MatchPtr subtotalPriceItem = matches[priceIndex];
                    wstring tmpSubtotalPriceItemText = getStringFromMatch(subtotalPriceItem, matchKeyText);
                    float tmpPriceValue = atof(toUTF8(tmpSubtotalPriceItemText).c_str());
                    subtotalValue = tmpPriceValue;
                    
                    ignoreDiscountsBecauseSubtotalFound = true;
                    
                    continue;
                }
            }
        }
        
        if (tp == OCRElementTypePriceDiscount) {
            if (!ignoreDiscountsBecauseSubtotalFound) {
                // Create dictionary to hold the attributes of this item
                wstring tmpPriceDiscountText = getStringFromMatch(item, matchKeyText);
                float tmpPriceDiscountValue = atof(toUTF8(tmpPriceDiscountText).c_str());
                totalPrices -= tmpPriceDiscountValue;
                
                // Add to the vector of discounts
                discounts.push_back(tmpPriceDiscountValue);
            }
            
            continue;
        }
        
        if (tp == OCRElementTypeDate) {
        
            wstring tmpDateText = getStringFromMatch(item, matchKeyText);
            // Set date output param
            *datePtr = tmpDateText;
            
            continue;
        }
        
        if (tp == OCRElementTypePhoneNumber) {
            scannedItem newItem;
            newItem.itemText = getStringFromMatch(item, matchKeyText);
            if (item.isExists(matchKeyText2)) {
                newItem.itemText2 = getStringFromMatch(item, matchKeyText2);
            }
            // Set alternative sku's (text3) if set
            if (item.isExists(matchKeyText3)) {
                newItem.itemText3 = getStringFromMatch(item, matchKeyText3);
            }
            newItem.line = getIntFromMatch(item, matchKeyLine);
            
            // Set description if found (before or after)
            bool foundDescription = false;
            if (i > 0) {
                MatchPtr descriptionItem = matches[i-1];
                if (getIntFromMatch(descriptionItem, matchKeyLine) == newItem.line) {
                foundDescription = true;
                    newItem.description = RegexUtilsBeautifyProductDescription(getStringFromMatch(descriptionItem, matchKeyText), &globalResults);
                    if (descriptionItem.isExists(matchKeyText2)) {
                        newItem.description2 = RegexUtilsBeautifyProductDescription(getStringFromMatch(descriptionItem, matchKeyText2), &globalResults);
                    }
                    if (descriptionItem.isExists(matchKeyText3)) {
                        newItem.itemText3 = RegexUtilsBeautifyProductDescription(getStringFromMatch(descriptionItem, matchKeyText3), &globalResults);
                    }
                }
            }
            if (!foundDescription && (i<(int)matches.size()-1)) {
                MatchPtr descriptionItem = matches[i+1];
                if (getIntFromMatch(descriptionItem, matchKeyLine) == newItem.line) {
                foundDescription = true;
                    newItem.description = getStringFromMatch(descriptionItem, matchKeyText);
                    if (descriptionItem.isExists(matchKeyText2)) {
                        newItem.description2 = getStringFromMatch(descriptionItem, matchKeyText2);
                    }
                    if (descriptionItem.isExists(matchKeyText3)) {
                        newItem.itemText3 = getStringFromMatch(descriptionItem, matchKeyText3);
                    }
                }
            }
            
            phones.push_back(newItem);
        }
        
        // Unless retailer has no product numbers, at that stage we must have a product number. For retailers like CVS we will put together product entries based on finding a description + product price
        if (globalResults.retailerParams.noProductNumbers) {
            if (tp != OCRElementTypeProductDescription) continue;
        } else {
            if (tp != OCRElementTypeProductNumber) continue;
        }
        
        int productIndex = -1;
        ocrparser::MatchPtr productItem;
        
        int descriptionIndex = -1;
        ocrparser::MatchPtr descriptionItem;
        
        if (!globalResults.retailerParams.noProductNumbers) {
            productIndex = i;
            productItem = item;
        } else {
            descriptionIndex = i;
            descriptionItem = item;
        }
        
        // Create dictionary to hold the attributes of this product
        scannedProduct newProduct;
        newProduct.description = wstring();
        newProduct.line = 0;
        newProduct.sku = wstring();
        newProduct.price = newProduct.price2 = newProduct.price3 = -1;
        newProduct.quantity = 1;
        newProduct.totalPrice = -1;
        newProduct.ignored = newProduct.not_eligigle = false;
        
        // Set sku property
        if (globalResults.retailerParams.noProductNumbers) {
            // Set product number to the line number (temporary hack)
            char *productCSTR = (char *)malloc(100);
            sprintf(productCSTR, "%d", currentLine);
            newProduct.sku = wstring(toUTF32(productCSTR));
            free(productCSTR);
        } else {
            wstring tmpString = getStringFromMatch(item, matchKeyText);
            newProduct.sku = tmpString;
#if DEBUG
            if (tmpString == L"054500010000") {
                DebugLog("");
            }
#endif
        }
        
        // Set not_eligible property
        if (item.isExists(matchKeyStatus) && (getUnsignedLongFromMatch(item, matchKeyStatus) & matchKeyStatusHasCoupon))
            newProduct.not_eligigle = true;
        
        indexOfLastProduct = i;
        
        if (!globalResults.retailerParams.noProductNumbers) {
            // Set alternative sku's (text2) if set
            if (item.isExists(matchKeyText2)) {
                newProduct.sku2 = getStringFromMatch(item, matchKeyText2);
            }
            // Set alternative sku's (text3) if set
            if (item.isExists(matchKeyText3)) {
                newProduct.sku3 = getStringFromMatch(item, matchKeyText3);
            }
        }
        
        if (currentLine < 0)
            continue;   // Something is wrong, don't try to find description and price

        // Add description (if present). For retailers w/o product numbers we already have the description (current item itself)
        if (!globalResults.retailerParams.noProductNumbers) {
            descriptionIndex = OCRResults::descriptionIndexForProductAtIndex(i, matches, NULL, retailerParams);
            if (descriptionIndex >= 0)
                descriptionItem = matches[descriptionIndex];
        }
        
        wstring descriptionString;
        if (descriptionIndex >= 0) {
        
            descriptionString = getStringFromMatch(descriptionItem, matchKeyText);
            newProduct.description = descriptionString;
            
            // Set alternative desc (text2) if set
            if (descriptionItem.isExists(matchKeyText2)) {
                newProduct.description2 = getStringFromMatch(descriptionItem, matchKeyText2);
            }
            // Set alternative desc (text3) if set
            if (descriptionItem.isExists(matchKeyText3)) {
                newProduct.description3 = getStringFromMatch(descriptionItem, matchKeyText3);
            }
        } // adding description
        
        // Add price (if present) but first need to test if we have a quantity & price per item (depending on the retailer, on the line just below, or two lines below or something entirely different like Staples where quantity is on the first line (before description) and price per item is after the product number on the line below)
        float priceValue = -1.0, priceValue2 = -1, priceValue3 = -1, discountValue = -1;
        int quantityValue = -1;
        float pricePerItemValue = -1;
        int pricePerItemIndex = -1, quantityItemIndex = -1;
        int indexPossibleQuantityLine = i;
        // e.g. Staples
        if (globalResults.retailerParams.quantityAtDescriptionStart) {
            // Set description index only for retailers with product numbers - for others we already set it above
            if (!globalResults.retailerParams.noProductNumbers)
                descriptionIndex = OCRResults::descriptionIndexForProductAtIndex(i, matches, NULL, retailerParams);
            if (descriptionIndex > 0) {
                // Whether or not we find a quantity item, we already know there *may* be a quantity so make a note of that
                productLinesWithPossibleQuantity.push_back(currentLine);
                int tmpQuantityIndex = descriptionIndex - 1;
                MatchPtr tmpQuantityMatch = matches[tmpQuantityIndex];
                MatchPtr tmpDescriptionMatch = matches[descriptionIndex];
                    // Quantity on same line as description (just before it)
                if ((getIntFromMatch(tmpQuantityMatch, matchKeyLine) == getIntFromMatch(tmpDescriptionMatch, matchKeyLine))
                    // Indeed has the quantity type
                    && (getElementTypeFromMatch(tmpQuantityMatch, matchKeyElementType) == OCRElementTypeProductQuantity)) {
                    // Got quantity, now also need price per item
                    if (retailerParams->pricePerItemAfterProduct) {
                        int tmpPricePerItemIndex = OCRResults::elementWithTypeInlineAtIndex(productIndex, OCRElementTypePricePerItem, &matches, retailerParams);
                        if (tmpPricePerItemIndex > productIndex) {
                            // Found everything, now set the values we need to remember
                            pricePerItemIndex = tmpPricePerItemIndex;
                            quantityItemIndex = tmpQuantityIndex;
                            MatchPtr tmpPricePerItemMatch = matches[pricePerItemIndex];
                            wstring pricePerItemString = getStringFromMatch(tmpPricePerItemMatch, matchKeyText);
                            pricePerItemValue = atof(toUTF8(pricePerItemString).c_str());

                            wstring quantityString = getStringFromMatch(tmpQuantityMatch, matchKeyText);
                            quantityValue = atof(toUTF8(quantityString).c_str());
                        } // Found price per item after product (on same line)
                    } // Looking for price per item after product
                } // Found quantity
            } // Found description
        } else if (retailerParams->hasQuantities) {
            // Look for next line
            for (int possibleQuantityLine=1; possibleQuantityLine<=retailerParams->maxLinesBetweenProductAndQuantity; possibleQuantityLine++) {
                // First time through, the line below will have indexPossibleQuantityLine pointing to the product item, and indexOfItemOnNextLine will get set to point to the next line
                // If retailer allows for up to 2 lines to find the quantity, after the end of the first pass, we set indexPossibleQuantityLine to indexOfItemOnNextLine
                int indexOfItemOnNextLine = OCRResults::indexOfElementPastLineOfMatchAtIndex(indexPossibleQuantityLine, &matches);
                if (indexOfItemOnNextLine > 0) {
                    // Got a next line
                    MatchPtr itemOnNextLine = matches[indexOfItemOnNextLine];
                    // Got a line number and it's the current line + 1?
                    int newLineNumber = getIntFromMatch(itemOnNextLine, matchKeyLine);
                    if (newLineNumber != currentLine+possibleQuantityLine) {
                        // Not the expected line number, means there are blank lines. Could be a case like Home Depot where the line below the product is treated as "meaningless" (no typed data) but there IS a quantity just below. Test for this case.
                        if ((newLineNumber - currentLine > possibleQuantityLine) && (newLineNumber - currentLine <= retailerParams->maxLinesBetweenProductAndQuantity)) {
                            // Give the next line(s) a chance
                            possibleQuantityLine = newLineNumber - currentLine;
                        } else {
                            // Abort, done searching for a quantity
                            break;
                        }
                    }

                    // Before we look for a quantity on this line, check if there is a product number on this line: if so, abort this search!
                    int nextProductIndex = OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypeProductNumber, &matches, NULL);
                    if (nextProductIndex >= 0) {
                        break; // Break out of loop looking at next 2 lines for possible quantity
                    }
                    
                    // Whether or not we find a quantity item, we already know there *may* be a quantity so make a note of that
                    productLinesWithPossibleQuantity.push_back(currentLine);
                    
                    // Now search for a quantity item and a price item on that line
                    quantityItemIndex = OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypeProductQuantity, &matches, NULL);
                    if (quantityItemIndex > 0) {
                        if (retailerParams->quantityPPItemTotal) {
                            // In this case the price per item always follows right after the quantity
                            pricePerItemIndex = quantityItemIndex + 1;
                            if (pricePerItemIndex < matches.size()) {
                                // Check it's on the same line and of type price!
                                MatchPtr pricePerItemItem = matches[pricePerItemIndex];
                                int pricePerItemLine = getIntFromMatch(pricePerItemItem, matchKeyLine);
                                if (pricePerItemLine == currentLine + possibleQuantityLine) {
                                    // Finally, check type
                                    OCRElementType tp = getElementTypeFromMatch(pricePerItemItem, matchKeyElementType);
                                    if (tp != OCRElementTypePrice)
                                        pricePerItemIndex = -1;
                                } else {
                                    pricePerItemIndex = -1;
                                }
                            } else {
                                pricePerItemIndex = -1;
                            }
                        } else {
                            pricePerItemIndex = OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypePrice, &matches, NULL);
                        }
                        if (pricePerItemIndex > 0) {
                            // Got quantity + price per item!
                            MatchPtr pricePerItemItem = matches[pricePerItemIndex];
                            wstring pricePerItemString = getStringFromMatch(pricePerItemItem, matchKeyText);
                            pricePerItemValue = atof(toUTF8(pricePerItemString).c_str());

                            MatchPtr quantityItem = matches[quantityItemIndex];
                            wstring quantityString = getStringFromMatch(quantityItem, matchKeyText);
                            quantityValue = atoi(toUTF8(quantityString).c_str());
                            
                            break;
                        } // found price
                        else {
                            // Found quantity but no price? Strange - abort
                            break;
                        }
                    } // found quantity
                    indexPossibleQuantityLine = indexOfItemOnNextLine; // Move to next line
                } // Got a new line
                else {
                    break;
                }
            } // Search for quantity
        } // if retailer has quantities
        
        // Look for price on product line even if we already set the price & quantity, because then we still want to set confidence number
        wstring partialPriceString;
        bool foundPartialPrice = false;
        int tmpIndexMissingPrice = -1;
        int priceLine = -1;
        int priceIndex = -1;
        if (!globalResults.retailerParams.noProductNumbers) {
            priceIndex = OCRResults::priceIndexForProductAtIndex(i, matches, &priceLine, retailerParams);
        } else if (descriptionIndex >= 0) {
            priceIndex = OCRResults::productPriceIndexForDescriptionAtIndex(descriptionIndex, OCRElementTypeProductPrice, matches, retailerParams);
        }
        if ((priceIndex >= 0) && (priceLine > currentLine))
            excludedLines.push_back(priceLine); // We grabbed the price from the line below, mark it as one to ignore (skip)
        if (priceIndex >= 0) {
            MatchPtr priceItem = matches[priceIndex];
            // Remember price (will be set in a property later)
            wstring priceItemString = getStringFromMatch(priceItem, matchKeyText);
            float tmpPriceValue = atof(toUTF8(priceItemString).c_str());

#if !DEBUG
            if (tmpPriceValue < retailerParams->maxPrice) {
#endif
                priceValue = tmpPriceValue;
//#if DEBUG
//                std::string productNumberStr = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
//                NSString *productNumber = [NSString stringWithUTF8String:productNumberStr.c_str()];
//                if ([productNumber isEqualToString:@"062230309"] && priceItem.isExists(matchKeyText2)) {
//                    NSLog(@"");
//                }
//#endif
                if (quantityValue > 0) {
                    // Now check if product price happens to be exactly equal to quantity * price per item. If so, set confidence to 100 for both so that no frames ever override that!
                    int priceValue100 = (int)(priceValue * 100);
                    int pricePerItemValue100 = (int)(pricePerItemValue * 100);
                    if ((pricePerItemValue > 0) && (quantityValue > 0) && (priceValue > 0) && (priceValue100 == pricePerItemValue100 * quantityValue)) {
                        MatchPtr pricePerItemItem = matches[pricePerItemIndex];
                        MatchPtr quantityItem = matches[quantityItemIndex];
                        pricePerItemItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                        quantityItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                        priceItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                    }
                }
                
                // Set alternative prices (if exist)
                if (priceItem.isExists(matchKeyText2)) {
                    wstring tmpString = getStringFromMatch(priceItem, matchKeyText2);
                    priceValue2 = atof(toUTF8(tmpString).c_str());
                }
                if (priceItem.isExists(matchKeyText3)) {
                    wstring tmpString = getStringFromMatch(priceItem, matchKeyText3);
                    priceValue3 = atof(toUTF8(tmpString).c_str());
                }
                
                // If we have a server price, check if perhaps that price matches text2 or text3!
                if ((serverPrices.size() > 0) && ((priceValue2 > 0) || (priceValue3 > 0))) {
                    // Do we have that product in the server prices?
                    wstring productNumber = getStringFromMatch(item, matchKeyText);
//#if DEBUG
//                    if ([productNumber isEqualToString:@"062230309"] && (serverPrices != nil)) {
//                        NSLog(@"");
//                    }
//#endif
                    int priceValue100 = round(priceValue * 100);
                    for (int k=0; k<serverPrices.size(); k++) {
                        serverPrice sPrice = serverPrices[k];
                        wstring serverSKU = sPrice.sku;
                        if (serverSKU == productNumber) {
                            float serverPriceValue = sPrice.price;
                            int serverPriceValue100 = round(serverPriceValue * 100);
                            if (serverPriceValue100 != priceValue100) {
                                if ((priceValue2 > 0) && (priceValue2 == serverPriceValue)) {
                                    // Upgrade price2 to price!
                                    float tmpPriceValue = priceValue;
                                    wstring tmpPriceText = getStringFromMatch(priceItem, matchKeyText);
                                    wstring priceText2 = getStringFromMatch(priceItem, matchKeyText2);
                                    DebugLog("exportCurrentScanResults: upgrading price text2 [%f] (instead of [%f]) - matched server price!", serverPriceValue, priceValue);
                                    priceValue = priceValue2;
                                    priceValue2 = tmpPriceValue;
                                    priceItem[matchKeyText] = SmartPtr<wstring> (new wstring(priceText2)).castDown<EmptyType>();
                                    priceItem[matchKeyText2] = SmartPtr<wstring> (new wstring(tmpPriceText)).castDown<EmptyType>();
                                    priceItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                                } else if ((priceValue3 > 0) && (priceValue3 == serverPriceValue)) {
                                    // Upgrade price3 to price!
                                    float tmpPriceValue = priceValue;
                                    wstring tmpPriceText = getStringFromMatch(priceItem, matchKeyText);
                                    wstring priceText3 = getStringFromMatch(priceItem, matchKeyText3);
                                    DebugLog("exportCurrentScanResults: upgrading price text3 [%f] (instead of [%f]) - matched server price!", serverPriceValue, priceValue);
                                    priceValue = priceValue3;
                                    priceValue3 = tmpPriceValue;
                                    priceItem[matchKeyText] = SmartPtr<wstring> (new wstring(priceText3)).castDown<EmptyType>();
                                    priceItem[matchKeyText3] = SmartPtr<wstring> (new wstring(tmpPriceText)).castDown<EmptyType>();
                                    priceItem[matchKeyConfidence] = (SmartPtr<float> (new float(100.0))).castDown<EmptyType>();
                                }
                            }
                            break;
                        }
                    }
                }
                
#if !DEBUG
            } // price < 10,000
#endif
        } else {
            // No price, perhaps a discount?
            int discountIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePriceDiscount, &matches, retailerParams);
            // In case we failed to upgrade a potential price discount (shouldn't happen but happened in https://www.pivotaltracker.com/story/show/112241041)
            if (discountIndex < 0)
                discountIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePotentialPriceDiscount, &matches, retailerParams);
            if (discountIndex > 0) {
                MatchPtr dd = matches[discountIndex];
                wstring tmpString = getStringFromMatch(dd, matchKeyText);
                discountValue = atof(toUTF8(tmpString).c_str());
                
            } else {
                // No price or discount - perhaps a partial price?
                int tmpPartialPriceIndex = OCRResults::partialPriceIndexForProductAtIndex(i, matches, &priceLine, retailerParams);
                if (tmpPartialPriceIndex >= 0) {
                    // If we have server prices and we have a price for this product, see if we can upgrade the partial price!
                    wstring productNumber = getStringFromMatch(item, matchKeyText);
                    if (serverPrices.size() > 0) {
                        // Do we have that product in the server prices?
                        for (int k=0; k<serverPrices.size(); k++) {
                            serverPrice sPrice = serverPrices[k];
                            wstring serverSKU = sPrice.sku;
                            if (serverSKU == productNumber) {
                                //NSNumber *serverPriceNumber = serverPrice[@"serverPrice"];
                                //NSString *serverPriceString = [NSString stringWithFormat:@"%f", [serverPriceNumber floatValue]];
                                float serverPriceValue = sPrice.price;
                                // Close enough to the partial price?
                                bool doIt = false;
                                MatchPtr partialPriceElement = matches[tmpPartialPriceIndex];
                                // Get partial price text string
                                wstring tmpStr = getStringFromMatch(partialPriceElement, matchKeyText);
                                if (OCRResults::closeMatchPriceToPartialPrice(serverPriceValue, tmpStr))
                                    doIt = true;
                                if (!doIt) {
                                    if (partialPriceElement.isExists(matchKeyText2)) {
                                        wstring tmpStr = getStringFromMatch(partialPriceElement, matchKeyText2);
                                        if (OCRResults::closeMatchPriceToPartialPrice(serverPriceValue, tmpStr))
                                            doIt = true;
                                    }
                                }
                                if (!doIt) {
                                    if (partialPriceElement.isExists(matchKeyText3)) {
                                        wstring tmpStr = getStringFromMatch(partialPriceElement, matchKeyText3);
                                        if (OCRResults::closeMatchPriceToPartialPrice(serverPriceValue, tmpStr))
                                            doIt = true;
                                    }
                                }
                                if (doIt) {
                                    // Adjust item previously only a partialPrice
                                    partialPriceElement[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
                                    char *serverPriceCStr = (char *)malloc(50);
                                    sprintf(serverPriceCStr, "%.2f", serverPriceValue);
                                    wstring serverPriceString = toUTF32(serverPriceCStr);
                                    free(serverPriceCStr);
                                    partialPriceElement[matchKeyText] = SmartPtr<wstring> (new wstring(serverPriceString)).castDown<EmptyType>();
                                    
                                    // Set priceValue for the sake of code later, to state we actually found a price after all
                                    priceIndex = tmpPartialPriceIndex;
                                    int priceLine = getIntFromMatch(partialPriceElement, matchKeyLine);
                                    if ((priceIndex >= 0) && (priceLine > currentLine))
                                        excludedLines.push_back(priceLine); // We grabbed the price from the line below, mark it as one to ignore (skip)
                                    priceValue = serverPriceValue;
                                }
                                break;  // Whether or not we accepted this price, we found it in the serverPrices, quit looking
                            } // product number matches
                        } // searching server prices
                    } // server prices exist
                    if (priceValue < 0) {
                        MatchPtr d = matches[tmpPartialPriceIndex];
//                        std::string tmpString = ocrparser::toUTF8 (*d[textKeyStr].castDown <wstring>());
//                        partialPriceString = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                        partialPriceString = getStringFromMatch(d, matchKeyText);
                        foundPartialPrice = true;
                        tmpIndexMissingPrice = tmpPartialPriceIndex;
                    }
                }
            }
        }
        
        // Check if there is another line later, with the same product, but with a discountPrice instead of price, and same $ value: that's a voided product
        if (retailerParams->hasVoidedProducts) {
            bool skipThisLine = false;
            if (priceValue >= 0) {
                MatchPtr productPriceItem = matches[priceIndex];
                wstring productPriceString = getStringFromMatch(productPriceItem, matchKeyText);
                // Now look for a line with same product but discount price
                wstring productString = getStringFromMatch(item, matchKeyText);
                int nextLine = OCRResults::indexOfElementPastLineOfMatchAtIndex(i, &matches);
                for (int j=nextLine; (j>0) && (j < matches.size()); j++) {
                    MatchPtr otherItem = matches[j];
                    int otherLine = getIntFromMatch(otherItem, matchKeyLine);
                    if (std::find(excludedLines.begin(), excludedLines.end(), otherLine) != excludedLines.end()) {
                        // Skip
                        j =ocrparser::OCRResults::indexLastElementOnLineAtIndex(j, &matches);
                        continue;
                    }
                    OCRElementType otherTp = getElementTypeFromMatch(otherItem, matchKeyElementType);
                    if (otherTp != OCRElementTypeProductNumber)
                        continue;
                    //std::string tmpCString = ocrparser::toUTF8 (*otherItem[textKeyStr].castDown <wstring>());
                    //NSMutableString *otherProductString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding:NSUTF8StringEncoding];
                    wstring otherProductString = getStringFromMatch(otherItem, matchKeyText);
                    if (otherProductString == productString) {
                        // Now check if we have a discount price!
                        int discountIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypePriceDiscount, &matches, NULL);
                        if (discountIndex > 0) {
                            // Found it!
                            MatchPtr discountItem = matches[discountIndex];
                            //std::string tmpCString = ocrparser::toUTF8 (*discountItem[textKeyStr].castDown <wstring>());
                            //NSMutableString *discountString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding: NSUTF8StringEncoding];
                            wstring discountString = getStringFromMatch(discountItem, matchKeyText);
                            if (discountString == productPriceString) {
                                // Ignore product AND the matching voided product line with the discount
                                excludedLines.push_back(currentLine);
                                excludedLines.push_back(otherLine);
                                // Skip this entire line and continue
                                i = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
                                skipThisLine = true;
                                break; // Out of j loop
                            }
                        }
                    }
                } // searching for the matching voided product line
            } // Got price
            if (skipThisLine)
                continue;
        } // retailer has voided products
        
        // Sanity check: if quantity > 99, nuke it unless quantity x price per item == total price (on product line)
        if (quantityValue > 99) {
            bool nuke = true;
            if ((pricePerItemValue > 0) && (priceValue > 0)) {
                int priceValue100 = (int)(priceValue * 100);
                int pricePerItemValue100 = (int)(pricePerItemValue * 100);
                if (priceValue100 == pricePerItemValue100 * quantityValue) {
                    nuke = false;
                }
            }
            if (nuke) {
                // Cancel the finding of that quantity altogether
                quantityValue = pricePerItemValue = -1;
            }
        }
        
        // Is there a quantity / price per item for this product? If so, look for that product in server prices and see if server price matches the price per item. Note: for now only do the below for retailers w/product numbers
        if ((productIndex >= 0) && productItem.isExists(matchKeyQuantity) && productItem.isExists(matchKeyPricePerItem) && (quantityValue <= 0) && (serverPrices.size() > 0)) {
            int possiblePricePerItem_100 = round(getFloatFromMatch(productItem, matchKeyPricePerItem) * 100);
            int possibleQuantity = getIntFromMatch(productItem, matchKeyQuantity);
            int possiblePricePerItem2_100 = -1;
            if (productItem.isExists(matchKeyPricePerItem2)) possiblePricePerItem2_100 = round(getFloatFromMatch(productItem, matchKeyPricePerItem2) * 100);
            int possibleQuantity2 = -1;
            if (productItem.isExists(matchKeyQuantity2)) possibleQuantity2 = getIntFromMatch(productItem, matchKeyQuantity2);
            int possiblePricePerItem3_100 = -1;
            if (productItem.isExists(matchKeyPricePerItem3)) possiblePricePerItem3_100 = round(getFloatFromMatch(productItem, matchKeyPricePerItem3) * 100);
            int possibleQuantity3 = -1;
            if (productItem.isExists(matchKeyQuantity3)) possibleQuantity3 = getIntFromMatch(productItem, matchKeyQuantity3);
            wstring productNumber = getStringFromMatch(productItem, matchKeyText);
            // Do we have that product in the server prices?
            for (int k=0; k<serverPrices.size(); k++) {
                serverPrice sPrice = serverPrices[k];
                wstring serverSKU = sPrice.sku;
                if (serverSKU == productNumber) {
                    float serverPriceValue = sPrice.price;
                    int serverPriceValue100 = round(serverPriceValue * 100);
                    if (serverPriceValue100 == possiblePricePerItem_100) {
                        priceValue = -1;
                        quantityValue = possibleQuantity;
                        pricePerItemValue = serverPriceValue;
                    } else if ((serverPriceValue100 == possiblePricePerItem2_100) && (possibleQuantity2 > 0)) {
                        priceValue = -1;
                        quantityValue = possibleQuantity2;
                        pricePerItemValue = serverPriceValue;
                    } else if ((serverPriceValue100 == possiblePricePerItem3_100) && (possibleQuantity3 > 0)) {
                        priceValue = -1;
                        quantityValue = possibleQuantity3;
                        pricePerItemValue = serverPriceValue;
                    }
                    break;
                }
            }
        }
        
        // Add price (if found)
        if ((priceValue >= 0) || ((quantityValue > 0) && (pricePerItemValue > 0))) {
            //newValueWithRange = [NSMutableDictionary dictionary];
            
            // If we have a quantity & price per item, trust the price per item
            //NSNumber *tmpFloat;
            if ((quantityValue > 0) && (pricePerItemValue > 0))
                //tmpFloat = [NSNumber numberWithFloat:pricePerItemValue];
                newProduct.price = pricePerItemValue;
            else
                //tmpFloat = [NSNumber numberWithFloat:priceValue];
                newProduct.price = priceValue;
            //[newValueWithRange setObject:tmpFloat forKey:@"value"];
        
            //[newProduct setObject:newValueWithRange forKey:@"price"];
            
            if (priceValue2 > 0) {
//                newValueWithRange = [NSMutableDictionary dictionary];
//                NSNumber *tmpFloat = [NSNumber numberWithFloat:priceValue2];
//                [newValueWithRange setObject:tmpFloat forKey:@"value"];
//                [newProduct setObject:newValueWithRange forKey:@"price2"];
                newProduct.price2 = priceValue2;
            }
            if (priceValue3 > 0) {
//                newValueWithRange = [NSMutableDictionary dictionary];
//                NSNumber *tmpFloat = [NSNumber numberWithFloat:priceValue3];
//                [newValueWithRange setObject:tmpFloat forKey:@"value"];
//                [newProduct setObject:newValueWithRange forKey:@"price3"];
                newProduct.price3 = priceValue3;
            }
            
            if ((quantityValue > 0) && (pricePerItemValue > 0))
                totalPrices += pricePerItemValue * quantityValue;
            else
                totalPrices += priceValue;
            numPrices++;
        } else {
            // No price, perhaps a discount line?
            if (discountValue > 0) {
//                NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
//                NSNumber *floatObj = [NSNumber numberWithFloat:discountValue];
//                
                totalPrices -= discountValue;
//                
//                [newValue setObject:floatObj forKey:@"value"];
//                    
//                [resultsDict[@"discounts"] addObject:newValue];
                
                discounts.push_back(discountValue);
                
                i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, &matches);
                continue;
            } else {
            // No price. Set partialPrice (if found)
                if (foundPartialPrice) {
                    //[newProduct setObject:partialPriceString forKey:@"partialPrice"];
                    
                    newProduct.partialPrice = partialPriceString;
     
                    // Try to turn this partial price into a price later, if we have a subtotal + other conditions
                    if (!abortMissingPriceOptimization) {
                        if (indexMissingPrice < 0) {
                            indexMissingPrice = tmpIndexMissingPrice;   // If we decide this is close enough to the price we expect, we'll need to fix the text & type of this item
                            //objMissingPrice = newProduct;
                        } else {
                            // Found a 2nd product missing a price, abort (can't guess two prices with one total)
                            abortMissingPriceOptimization = true;
                        }
                    }
                }
            }
        }
        
        // Add quantity
        if (quantityValue > 0) {
            //[newProduct setObject:[NSNumber numberWithInt:quantityValue] forKey:@"quantity"];
            newProduct.quantity = quantityValue;
            
            // If we have a quantity, also create a "totalprice"
            if (priceValue > 0)
                //[newProduct setObject:[NSNumber numberWithFloat:priceValue] forKey:@"totalprice"];
                newProduct.totalPrice = priceValue;
        } else
            //[newProduct setObject:[NSNumber numberWithInt:1] forKey:@"quantity"];
            newProduct.quantity = 1;
        
        // Add line number
//        newValueWithRange = [NSMutableDictionary dictionary];
//        NSNumber *tmpInt = [NSNumber numberWithInt:currentLine];
//        [newValueWithRange setObject:tmpInt forKey:@"value"];
//        [newProduct setObject:newValueWithRange forKey:@"line"];
        newProduct.line = currentLine;
        
        // Check if it's not a false product. If so, just mark it but let it live as a product (so that we take its price into account). Only if we got a price for it (otherwise it's likely to be a false positive generated by noise)
        unsigned long status = 0;
        if ((productIndex >= 0) && productItem.isExists(matchKeyStatus)) {
            status = getUnsignedLongFromMatch(productItem, matchKeyStatus);
        }
        if (status & matchKeyStatusNotAProduct) {
            if (priceValue > 0)
                //[newProduct setObject:@(YES) forKey:@"ignored"];
                newProduct.ignored = true;
            else {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
                continue;
            }
        }
    
        // Don't add this product if retailer is Walmart, description contains "REF" and there is no price. That's an item found after the subtotal and it is not a product. Note: need to add more generic code that stops looking for anything past the subtotal line.
        if (!((retailerParams->code == RETAILER_WALMART_CODE) && (descriptionIndex >= 0) && (descriptionString.find(L"REF") != wstring::npos) && (priceValue <= 0))
            // Don't even return products w/o prices when product number len is 5 or less - too many false positives
            && ((priceValue > 0) || (retailerParams->productNumberLen > 5))) {
            // Add the completed product dictionary to the array
            products.push_back(newProduct);
#if DEBUG
            if (newProduct.sku == L"054500010000") {
                DebugLog("");
            }
#endif
        }
        
        // Skip pass all elements we checked that were on the same block & line (we don't expect other products on the same block/line)
        i = OCRResults::indexLastElementOnLineAtIndex(i, &matches);
    } // for i, iterating over C++ results vector
    
    // Special pass to look for products without a price, in case we have the same product elsewhere with a price => adopt that price
    for (int k=0; k<products.size(); k++) {
        productStruct p = products[k];
        if (p.price < 0) {
            for (int n=0; n<products.size(); n++) {
                if (n == k) continue;
                productStruct otherP = products[n];
                if ((otherP.price > 0) && (otherP.quantity == 1) && (otherP.sku == p.sku)) {
                    p.price = otherP.price;
                    p.price2 = otherP.price2;
                    p.price3 = otherP.price3;
                    // Remove old product entry & update with new
                    DebugLog("exportCurrentScanResults: setting missing price [%.2f] for dup product [%s]", p.price, ocrparser::toUTF8(p.sku).c_str());
                    products.erase(products.begin() + k);
                    products.insert(products.begin() + k, p);
                    totalPrices += p.price;
                }
            }
        }
    }
    
    // Set subtotal output param (if we have a subtotal or a totalMinusTaxValue)
    if (totalMinusTaxValue >= 0) {
        if (subtotalValue < 0) {
            DebugLog("exportCurrentScanResults: using total [%.2f], no subtotal found", totalMinusTaxValue);
            subtotalValue = totalMinusTaxValue; // No subtotal, use the total minus tax!
        } else {
            int subtotalValue100 = round(subtotalValue * 100);
            int totalPrices100 = round(totalPrices * 100);
            int totalMinusTaxValue100 = round(totalMinusTaxValue * 100);
            if ((subtotalValue100 != totalPrices100) && (totalMinusTaxValue100 == totalPrices100)) {
                DebugLog("exportCurrentScanResults: using total [%.2f], matching total prices (unlike subtotal [%.2f])", totalMinusTaxValue, subtotalValue);
              subtotalValue = totalMinusTaxValue;
            }
        }
    }
    if (subtotalValue >= 0) {
#if DEBUG
        float discountsTotal = 0;
        for (int y=0; y<discounts.size(); y++)
            discountsTotal += discounts[y];
        DebugLog("exportCurrentScanResults: subtotal = %.2f (total prices = %.2f, total discounts = %.2f)", subtotalValue, totalPrices, discountsTotal);
        DebugLog("");
#endif
//#if DEBUG
//        if (abs(subtotalValue - totalPrices) > 0.01) {
//            bool found = false;
//            for (int i=0; i < matches.size(); i++) {
//                MatchPtr item = matches[i];
//              
//                OCRElementType tp = getElementTypeFromMatch(item, matchKeyElementType);
//                
//                if (tp != OCRElementTypePrice)
//                    continue;
//                
//                wstring itemText = getStringFromMatch(item, matchKeyText);
//                if ((itemText == L"3139.98") || (itemText == L"31319.96")) {
//                    found = true;
//                    break;
//                }
//            }
//            DebugLog("exportCurrentScanResults: subtotal mismatch");
//            if (!found) {
//                DebugLog("3139.98 or 31319.96 NOT found");
//            }
//        }
//#endif
        *subTotalPtr = subtotalValue;
    }
    
    // Remember if the subtotal matches the sum of all prices (minus all discounts): this is very important because it implies that from now on we will not accept any scanning activities that change any price or add new products
    if ((subtotalValue >= 0) && (abs(totalPrices - subtotalValue) < 0.01) && (!retailerParams->hasMultipleSubtotals)) {
        if (!globalResults.subtotalMatchesSumOfPricesMinusDiscounts)
            DebugLog("exportCurrentScanResults: SUBTOTAL MATCHES SUM OF PRODUCTS (subtotal = %.2f, total prices = %.2f)", subtotalValue, totalPrices);
        globalResults.subtotalMatchesSumOfPricesMinusDiscounts = true;
    } else {
#if DEBUG
        if (globalResults.subtotalMatchesSumOfPricesMinusDiscounts) {
            DebugLog("exportCurrentScanResults: WARNING, we thought subtotal matched prices prior, now no longer (subtotal = %.2f, total prices = %.2f)", subtotalValue, totalPrices);
        }
#endif
        globalResults.subtotalMatchesSumOfPricesMinusDiscounts = false;
    }
    
    // Special pass: if subtotal doesn't match sum of product prices, perhaps a match can be achieved by chosing a price2 or price3 alternative?
    if ((subtotalValue > 0) && (abs(totalPrices - subtotalValue) > 0.01)) {
        float missingDollars = subtotalValue - totalPrices;
        for (int i=0; i<products.size(); i++) {
            scannedProduct p = products[i];
            if (p.price2 > 0) {
                float delta = (p.price2 - p.price) * p.quantity;
                if (abs(delta - missingDollars) < 0.01) {
                    // Found it!
                    DebugLog("exportCurrentScanResults: product [%s] using price2=%.2f instead of price=%.2f to make up subtotal %.2f (total prices = %.2f)", toUTF8(p.sku).c_str(), p.price2, p.price, subtotalValue, totalPrices);
                    float tmpPrice = p.price;
                    p.price = p.price2;
                    p.price2 = tmpPrice;
                    // Delete from vector
                    products.erase(products.begin() + i);
                    products.insert(products.begin() + i, p);
                    totalPrices = subtotalValue;
                    break;
                }
            } else if (p.price3 > 0) {
                float delta = (p.price3 - p.price) * p.quantity;
                if (abs(delta - missingDollars) < 0.01) {
                    // Found it!
                    DebugLog("exportCurrentScanResults: product [%s] using price3=%.2f instead of price=%.2f to make up subtotal %.2f (total prices = %.2f)", toUTF8(p.sku).c_str(), p.price3, p.price, subtotalValue, totalPrices);
                    float tmpPrice = p.price;
                    p.price = p.price3;
                    p.price3 = tmpPrice;
                    // Delete from vector
                    products.erase(products.begin() + i);
                    products.insert(products.begin() + i, p);
                    totalPrices = subtotalValue;
                    break;
                }
            }
        }
    }
    
    vector<scannedProduct> validatedProducts;
    if (serverPrices.size() > 0) {
        // Now review all products we found and validate their prices versus the server price we found for them (if any)
        // TODO disqualify scanned prices and server prices which are in themselves greater (after subtrating the discounts) than the subtotal
        bool pricesMatchSubtotal = abs(totalPrices - subtotalValue) < 0.01;
        for (int i=0; i<products.size(); i++) {
            scannedProduct newProduct = products[i];
            
            if (pricesMatchSubtotal && (newProduct.quantity <= 1)) {
                validatedProducts.push_back(newProduct);
                continue;   // Only check items with a quantity when prices match the subtotal!
            }

            if (newProduct.price <= 0) {
                validatedProducts.push_back(newProduct);
                continue;   // We didn't find a price for this product, nothing to validate
            }
            
            float serverPriceValue = serverPriceForProduct(newProduct, serverPrices);
            if (serverPriceValue > 0) {
                float suggestedPrice = 0;
                int suggestedQuantity = 0;
                if (!sanityPriceCheck(newProduct.price, newProduct.quantity, serverPriceValue, retailerParams, newProduct.line, productLinesWithPossibleQuantity, suggestedPrice, suggestedQuantity)) {
                    if (suggestedPrice <= 0) {
                        // Failed sanity and no suggested alternative price - try price 2 & price 3
                        if (newProduct.price2 > 0) {
                            float suggestedPrice = 0; int suggestedQuantity = 0;
                            if (sanityPriceCheck(newProduct.price2, newProduct.quantity, serverPriceValue, retailerParams, newProduct.line, productLinesWithPossibleQuantity, suggestedPrice, suggestedQuantity)|| (suggestedPrice > 0)) {
                                if (suggestedPrice <= 0) {
                                    // Adopt price2
                                    float oldPrice = newProduct.price;
                                    newProduct.price = newProduct.price2;
                                    newProduct.price2 = oldPrice;
                                } else {
                                    // Adopt suggested price
                                    newProduct.price = suggestedPrice;
                                    if (suggestedQuantity >= 1)
                                        newProduct.quantity = suggestedQuantity;
                                    else
                                        newProduct.quantity = 1;
                                }
                            } else {
                                if (newProduct.price3 > 0) {
                                    float suggestedPrice = 0; int suggestedQuantity = 0;
                                    if (sanityPriceCheck(newProduct.price3, newProduct.quantity, serverPriceValue, retailerParams, newProduct.line, productLinesWithPossibleQuantity, suggestedPrice, suggestedQuantity) || (suggestedPrice > 0)) {
                                        if (suggestedPrice <= 0) {
                                            // Adopt price3
                                            float oldPrice = newProduct.price;
                                            float oldPrice2 = newProduct.price2;
                                            newProduct.price = newProduct.price3;
                                            newProduct.price2 = oldPrice;
                                            newProduct.price3 = oldPrice2;
                                        } else {
                                            // Adopt suggested price
                                            newProduct.price = suggestedPrice;
                                            if (suggestedQuantity >= 1)
                                                newProduct.quantity = suggestedQuantity;
                                            else
                                                newProduct.quantity = 1;
                                        }
                                    } else {
                                        // Kill the price of this product
                                        newProduct.price = newProduct.price2 = newProduct.price3 = -1;
                                        newProduct.quantity = 1;
                                    }
                                } else {
                                    // Kill the price of this product
                                    newProduct.price = newProduct.price2 = -1;
                                    newProduct.quantity = 1;
                                }
                            }
                        } else {
                            // No price2, kill the price of this product
                            newProduct.price = -1;
                            newProduct.quantity = 1;
                        }
                    } else {
                        // Failed sanity but an alternative price was suggested, adopt it
                        newProduct.price = suggestedPrice;
                        if (suggestedQuantity >= 1)
                            newProduct.quantity = suggestedQuantity;
                        else
                            newProduct.quantity = 1;
                    }
                } // Price failed sanity check
            } // found server price for this product
            
            // Whether or not this product was found in server prices and whether or not it passed sanity check as-is (unchanged) or was modified following the sanity check, add it to the validated array
            validatedProducts.push_back(newProduct);
        } // Iterate over all products
    } // If we got server prices
    
    // Special check if we have a subtotal and only one item has no price but has a partial price that looks like what we miss
    if (!abortMissingPriceOptimization && (subtotalValue > 0) && (indexMissingPrice >= 0)) {
        bool doIt = false;
        MatchPtr d = matches[indexMissingPrice];
        // Get partial price text string
        //NSString *partialText = [objMissingPrice objectForKey:@"partialPrice"];
        wstring partialText = getStringFromMatch(d, matchKeyText);
        float deltaPrice = subtotalValue - totalPrices;
        if (OCRResults::closeMatchPriceToPartialPrice(deltaPrice, partialText))
            doIt = true;
        if (!doIt) {
            if (d.isExists(matchKeyText2)) {
                //std::string tmpString = ocrparser::toUTF8 (*d[matchKeyText2].castDown <wstring>());
                //NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                wstring tmpStr = getStringFromMatch(d, matchKeyText2);
                if (OCRResults::closeMatchPriceToPartialPrice(deltaPrice, tmpStr))
                    doIt = true;
            }
        }
        if (!doIt) {
            if (d.isExists(matchKeyText3)) {
                //std::string tmpString = ocrparser::toUTF8 (*d[matchKeyText3].castDown <wstring>());
                //NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                wstring tmpStr = getStringFromMatch(d, matchKeyText3);
                if (OCRResults::closeMatchPriceToPartialPrice(deltaPrice, tmpStr))
                    doIt = true;
            }
        }
        if (doIt) {
            // Adjust item previously only a partialPrice
            d[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
            //NSString *tmpStr = [NSString stringWithFormat:@"%f", deltaPrice];
            //string tmpS = [tmpStr UTF8String];
            // std::string -> std::wstring
            //std::wstring ws;
            //ws.assign(tmpS.begin(), tmpS.end());
            char *deltaPriceCStr = (char *)malloc(50);
            sprintf(deltaPriceCStr, "%.2f", deltaPrice);
            wstring deltaPriceStr = toUTF32(deltaPriceCStr);
            free(deltaPriceCStr);
            DebugLog("exportCurrentScanResults: upgrading partial price [%s] to price [%s] based on single product price missing to make sub-total up", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), deltaPriceCStr);
            d[matchKeyText] = SmartPtr<wstring> (new wstring(deltaPriceStr)).castDown<EmptyType>();

            // And the iOS dictionary PQTODO - need to remove from vector, adjust then add back at the same spot (although I think order doesn't matter)
//            [objMissingPrice removeObjectForKey:@"partialPrice"];
//            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
//            NSNumber *tmpFloat = [NSNumber numberWithFloat:deltaPrice];
//            [newValueWithRange setObject:tmpFloat forKey:@"value"];
//            [objMissingPrice setObject:newValueWithRange forKey:@"price"];
        }
    }
    
    // PQTODO - do the below later
    // If a prices array was provided, check if the sum of all provided price make up the total. If yes, use all provided prices instead of the missing prices!
//    if ((serverPrices.size() > 0) && (subtotalValue > 0)) {
//        int totalPrices100 = totalPrices * 100;
//        int subTotalValue100 = subtotalValue * 100;
//        if (totalPrices100 != subTotalValue100) {
//            NSArray *products = resultsDict[@"products"];
//            // Iterate through resultsDic to collect products with missing prices while also checking each is found in the serverPrices
//            float totalMissingPrices = 0;
//            BOOL productMissingPriceNotFoundInServerPrices = false;
//            NSMutableArray *productsMissingPrices = [NSMutableArray array];
//            for (NSDictionary *d in products) {
//                if ((d[@"sku"] != nil) && (d[@"price"] == nil)) {
//                    NSString *productNumber = d[@"sku"][@"value"];
//                    // Do we have that product in the server prices?
//                    BOOL found = false;
//                    for (NSDictionary *serverPrice in serverPrices) {
//                        NSString *serverSKU = serverPrice[@"sku"];
//                        if ([serverSKU isEqualToString:productNumber]) {
//                            totalMissingPrices += [serverPrice[@"serverPrice"] floatValue];
//                            // Remember that product to fix its price later (if everything checks out)
//                            NSDictionary *newProduct = [NSDictionary dictionaryWithObjectsAndKeys:d, @"product", serverPrice[@"serverPrice"], @"serverPrice", nil];
//                            [productsMissingPrices addObject:newProduct];
//                            found = true;
//                            break;
//                        }
//                    }
//                    if (!found) {
//                        productMissingPriceNotFoundInServerPrices = true;
//                        break;
//                    }
//                }
//            }
//            if (!productMissingPriceNotFoundInServerPrices) {
//                int totalMissingPrices100 = totalMissingPrices * 100;
//                if (subTotalValue100 - totalPrices100 == totalMissingPrices100) {
//                    // Do it! Fill in all missing prices!
//                    for (NSDictionary *p in productsMissingPrices) {
//                        NSMutableDictionary *product = p[@"product"];
//                        NSMutableDictionary *price = [NSMutableDictionary dictionary];
//                        price[@"value"] = p[@"serverPrice"];
//                        product[@"price"] = price;
//                    }
//                }
//            }
//        } // subtotal doesn't match sum of prices
//    } // got server prices and subtotal
    
    if (validatedProducts.size() > 0)
        *productsVectorPtr = validatedProducts;
    else
        *productsVectorPtr = products;
    *discountsVectorPtr = discounts;

    *phonesVectorPtr = phones;
    
	return;
} // exportCurrentScanResults
//#endif // 0


simpleMatch *exportCurrentOCRResultsArray(int *numItems) {
    MatchVector matches = globalResults.matchesAddressBook;
    struct simpleM *result = (simpleMatch *)malloc(sizeof(struct simpleM) * matches.size());
    for (int i=0; i<matches.size(); i++) {
        MatchPtr d = matches[i];
        wstring text = getStringFromMatch(d, matchKeyText);
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        int n = getIntFromMatch(d, matchKeyN);
        simpleMatch m;
        m.text = text; m.type = tp; m.n = n;
        *(result + i) = m;
    }
    *numItems = (int)matches.size();
    return result;
}

retailer_t getCurrentRetailersParams() {
    return globalResults.retailerParams;
}

// Processes a set of OCR results from MicroBlink and returns a set of matches (void for now)
ocrparser::MatchVector processBlinkOCRResults(MicroBlinkOCRChar *results, uint8_t *binaryImageData, size_t binaryImageWidth, size_t binaryImageHeight, int retailerId, retailer_s *retailerParamsPtr, bool newSequence, bool *meaningfulFramePtr, bool *oldDataPtr, bool *done, int *receiptImgIndexPtr, unsigned long *frameStatusPtr) {

    retailer_t retailerParams;
    
    // First, set default in some of the fields, to avoid dire consequences when we forget to set these for a new retailer
    retailerParams.uniformSpacingDigitsAndLetters = true;
    retailerParams.noProductNumbers = false;
    retailerParams.productNumberLen = retailerParams.shorterProductNumberLen = 0;
    retailerParams.minProductNumberLen = retailerParams.maxProductNumberLen = 0;
    retailerParams.productNumber2 = "";
    retailerParams.productNumber2GroupActions = "";
    retailerParams.charsToIgnoreAfterProductRegex = NULL;
    retailerParams.pricesAfterProduct = true;
    retailerParams.hasDetachedLeadingChar = false;
    retailerParams.hasDiscountsWithoutProductNumbers = false;
    retailerParams.hasMultipleSubtotals = false;
    retailerParams.hasMultipleDates = true;     // Err on the side of caution, true means we won't rely of the date to merge
    retailerParams.hasQuantities = false;
    retailerParams.allProductsHaveQuantities = false;
    retailerParams.quantityPPItemTotal = false;
    retailerParams.maxLinesBetweenProductAndQuantity = 0;
    retailerParams.quantityAtDescriptionStart = false;
    retailerParams.quantityAtDescriptionStart = false;
    retailerParams.pricePerItemAfterProduct = false;
    retailerParams.pricePerItemBeforeTotalPrice = false;
    retailerParams.extraCharsAfterPricePerItem = 0;
    retailerParams.quantityPPItemTotal = false;
    retailerParams.pricePerItemTaggedAsPrice = false;
    retailerParams.hasAdditionalPricePerItemWithSlash = false;
    retailerParams.pricesHaveDollar = false;
    retailerParams.noZeroBelowOneDollar = false;
    retailerParams.hasVoidedProducts = false;
    retailerParams.hasExtraCharsAfterPrices = false;
    retailerParams.extraCharsAfterPricesRegex = NULL;
    retailerParams.extraCharsAfterPricesMeansProductPrice = false;
    retailerParams.hasSubtotal = true;
    //retailerParams.subtotalRegex = "(?: |^)(?:(?i:sub|(?:.){3}total)|SLJB)";
    //retailerParams.subtotalRegex = "(?: |^)[.\\-*',~:]*(?i:((?:.){3}|[^\\s]*b)t(?:o|0)ta.)[.\\-*',~:]*($| )";
    //retailerParams.subtotalRegex = "(?: |^)[.\\-*',~:]*(?i:((?:.){3,5}|[^\\s]*b)t(?:o|0)ta.|sub[^\\s]{5,7})[.\\-*',~:]*($| )";
    //retailerParams.subtotalRegex = "(?:^)[.\\s\\-*',~:]*(?i:((?:.){3,5}|[^\\s]*b)t(?:o|0)ta.|sub[^\\s]{5,7}|sub(?:.){3}al)[.\\s\\-*',~:]*(?:$)";
    // 2015-11-12 adding "SLRTDTAL" based on one Staples receipt
    retailerParams.subtotalRegex = "(?:^)[.\\s\\-*',~:]*(?i:((?:.){3,5}|[^\\s]*(?: )?b)(?: )?t(?: )?(?:o|0)(?: )?t(?: )?a(?: )?[^\\s]|s(?: )?u(?: )?b(?: )?[^\\s]{5,7}|SLRTDTAL|SLBFOTAL|SUBTOT|(?:S|5|\\$)(?:U|J|IJ)(?:B|D|8|3|E(?:\\.)?)T(?:O|0|D)T(?:A|R)(?:L|I\\.)|s(?: )?u(?: )?b(?: )?[^\\s]{3}(?: )?a(?: )?l)[.\\s\\-*',~:]*%extracharsaftersubtotal(?:$)";
    retailerParams.extraCharsAfterSubtotalRegex = "";
    retailerParams.hasTotal = true;
    // 2015-11-05 one example where 'L' got erased, leaving just "TOTA"
    retailerParams.totalRegex = "(?:^)[.\\s\\-*',~:]*(?:(?i:T(?:o|0)t(?:a| )(?:l|1))|TOT(?:A|R)|T(?:.)TAL|T1TTFL)[.\\s\\-*',~:]*(?:$)";
    retailerParams.totalIncludesTaxAbove = true;
    retailerParams.maxCharsOnLineAfterPricePerItem = -1;
    retailerParams.letterHeightToRangeWidth = 0;
    retailerParams.typedRangeToImageWidthTooFar = 0.50;
    retailerParams.productAttributes = RETAILER_FLAG_ALIGNED_LEFT; // That's aligned relative to the productsWithPriceRange (not just within the products column). Begs the question if that's a good default, works only when prices follow product numbers (to the right)
    retailerParams.productPricesAttributes = RETAILER_FLAG_ALIGNED_RIGHT;
    retailerParams.descriptionAlignedWithProduct = false;
    retailerParams.descriptionAlignedLeft = false;
    retailerParams.descriptionOverlapsWithProduct = false;
    retailerParams.maxPrice = 1000.00;
    retailerParams.lineSpacing = 0.28; // Target default, as a percentage of uppercase/digit height
    retailerParams.minProductNumberEndToPriceStart = 0;
    retailerParams.minProductNumberStartToPriceStart = 0;
    retailerParams.descriptionStartToPriceEnd = -1;
    retailerParams.leftMarginLeftOfDescription = -1;  // As a function of digit width (anything entirely left of that has to be removed)
    retailerParams.rightMarginRightOfPrices = -1; // As a function of digit width (anything entirely right of that has to be removed)
    retailerParams.widthToProductWidth = 0; // Product width divided by receipt width from the left side of products to the right side of prices. 3.86 for Babiesrus
    retailerParams.productLeftSideToPriceRightSideNumDigits = 0; // For example, for BabiesRUs a 12-digit product numbers takes 436 pixels (i.e. 36.33 per digit) and space between left side of product and roght side of price is 1,604 => 44 digits
    retailerParams.subtotalLineExcessWidthDigits = 0; // Relevant only together with productLeftSideToPriceRightSideNumDigits, set when subtotal line may be confused with a product but is wider (e.g. 4.4 digits widths for Trader Joe's)
    retailerParams.leftMarginToProductWidth = 0; // Margin to the left of products, used to make sure we don't accidentally eliminate something there as noise, as a percentage of product width. 0.13 for Babiesrus
    retailerParams.rightMarginToProductWidth = 0; // Margin to the right of prices, used to make sure we don't accidentally eliminate something there as noise, as a percentage of product width. 0.48 for Babiesrus. Note: use it to capture the case where some types of lines that protude to the right are optional. OK to be liberal, all that matters is that we don't go beyond the receipt right edge
    retailerParams.sanityCheckFactor = WFSanityCheckFactor; // Reject scanned price smaller or larger than server price by that factor
    retailerParams.productNumbersLeftmost = false;
    retailerParams.productPricesRightmost = false;
    retailerParams.has0WithDiagonal = false;
    retailerParams.minItemLen = 3;
    retailerParams.hasSloped6 = false;
    retailerParams.minSpaceAroundDotsAsPercentOfWidth = 0.20;   // We haven't see retailers with smaller spacing around dots (that's 2.8 pixel for a 14 pixels digits width)
    retailerParams.maxSpaceAroundDotsAsPercentOfWidth = 0.71;      // That's 5 out of width 7 (used to delete spurious spaces that are less than that * 1.15)
    retailerParams.allUppercase = false;
    retailerParams.hasCoupons = false;  // Are there coupons with the meaning that item doesn't qualify for price
    retailerParams.productNumberUPCWhenPadded = false;
    retailerParams.stripAtDescEnd = NULL;
    retailerParams.discountStartingRegex = NULL;
    retailerParams.discountEndingRegex = NULL;
    retailerParams.productDescriptionMinLen = 3;
    retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.00; // Smallest spacing we have ever seen on receipts is CVS (1.00)
    retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0;
    retailerParams.hasTaxBalTotal = false;  // e.g. TAX 1.79 BAL 4.75" (Kmart)
    retailerParams.taxRegex = "";      // The "TAX" part of the TaxBal combo above, e.g. "TAX"
    retailerParams.balRegex = "";      // The "BAL" part of the TaxBal combo above, e.g. "BAL"
    retailerParams.hasProductsWithMarkerAndPriceOnNextLine = false;
    retailerParams.hasPhone = true;
    // Part 1: 212-123-4567 (Target, store name to the left)
    // Part 2: ( 281 ) 351 - 2616 (Walmart, no store name, address below)
    // Part 3: 212.123.4567 (CVS)
    retailerParams.phoneRegex = "(?:(?:^| )(?:[%digit]{3}\\-[%digit]{3}\\-[%digit]{4}|(?:\\(|C|1|l)(?: )?[%digit]{3}(?: )?(?:\\)|J|1|l)(?: )?[%digit]{3}(?: )?\\-(?: )?[%digit]{4}|[%digit]{3}\\.[%digit]{3}\\.[%digit]{4})(?:$| ))";
    retailerParams.dateCode = DATE_REGEX_GENERIC_CODE;
    retailerParams.mergeablePattern1Regex = NULL; // Unique pattern we can use to merge, e.g. "REG#02 TRN#1259 CSHR#1146440 STR#1059" for CVS
    retailerParams.mergeablePatterns1BeforeProducts = false;  // Set if mergeable pattern 1 can only appear before any products
    
    if (retailerId == RETAILER_TARGET_CODE) {
        retailerParams.code = RETAILER_TARGET_CODE;
        retailerParams.pricesHaveDollar = true;
        retailerParams.productNumberLen = 9;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = "T,F,FT,FN,FB";
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 4; // "ea" possibly broken into 4 chars)
        retailerParams.maxLinesBetweenProductAndQuantity = 2;
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-";
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        retailerParams.hasPhone = true;
        retailerParams.phoneRegex = "(?:(?:^| )[%digit]{3}\\-[%digit]{3}\\-[%digit]{4}(?:$| ))";    // e.g. 212-123-4567
        // width = 504, h=18
        retailerParams.digitWidthToHeightRatio = 0.6127;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.45;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.64;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.18;
        retailerParams.letterHeightToRangeWidth = 0.039;
        retailerParams.productRangeWidthDividedByLetterHeight = 0;  // Set for each retailer but init to 0 in case we forget to set for one (no good default)
        // Product width = 137, total width = 619; left margin = 20, right margin = 20
        retailerParams.widthToProductWidth = 4.52;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = true;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_WALMART_CODE) {
        retailerParams.code = RETAILER_WALMART_CODE;
        retailerParams.productNumberLen = 12;
        retailerParams.productNumberUPC = false;
        retailerParams.productAttributes = 0; // Clears RETAILER_FLAG_ALIGNED_LEFT
        retailerParams.charsToIgnoreAfterProductRegex = "."; // Should be ony H or K but OCR errors may return X instead etc.
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = "F";
        retailerParams.hasQuantities = true; // eg "2 AT   1 FOR     2.50  5.00 O", see https://drive.google.com/open?id=0B4jSQhcYsC9VTDJXM1RjVEoteUE
        retailerParams.maxLinesBetweenProductAndQuantity = 2;
        retailerParams.quantityPPItemTotal = true;
        retailerParams.pricePerItemTaggedAsPrice = true;
        retailerParams.hasMultipleSubtotals = true;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-(?:O|D|0|X)";
        retailerParams.hasVoidedProducts = true;
        retailerParams.hasProductsWithMarkerAndPriceOnNextLine = true;
        retailerParams.productsWithMarkerAndPriceOnNextLineRegex = "(?:K|X)(?:(?:I|1)|R|F)"; // e.g. ONIONS is 000000004093KI
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5436;
        // Width around text with a small margin (but not as wide as the actual receipt) = 2,055, letter height = 70 => 0.034
        retailerParams.letterHeightToRangeWidth = 0.034;
        retailerParams.productRangeWidthDividedByLetterHeight = 8.7;
        // Product width = 119, total width = 379; left margin = 20, right margin = 20
        retailerParams.widthToProductWidth = 3.18;
        retailerParams.leftMarginToProductWidth = 0.17;
        retailerParams.rightMarginToProductWidth = 0.17;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_TOYSRUS_CODE) {
        retailerParams.code = RETAILER_TOYSRUS_CODE;
        retailerParams.lineSpacing = 0.47;
        retailerParams.productNumberLen = 12;
        retailerParams.shorterProductNumberLen = 6;
        retailerParams.shorterProductsValid = false;
        retailerParams.productNumberUPC = true;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = true;
        retailerParams.productPricesAttributes = RETAILER_FLAG_ALIGNED_RIGHT;
        retailerParams.stripAtDescEnd = "TI";
        retailerParams.hasQuantities = true;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.pricePerItemAfterProduct = false;
        retailerParams.pricePerItemBeforeTotalPrice = false;
        retailerParams.extraCharsAfterSubtotalRegex = "(?:[^\\s]{2}(?: )?[^\\s](?: )*)?";
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-";
        retailerParams.dateCode = DATE_REGEX_GENERIC_CODE;
        retailerParams.digitWidthToHeightRatio = 0.4179;
        retailerParams.lineSpacing = 0.42;
        retailerParams.letterHeightToRangeWidth = 0.043;
        retailerParams.typedRangeToImageWidthTooFar = 0.40;
        // See babiesrus
        retailerParams.widthToProductWidth = 3.86;
        retailerParams.leftMarginToProductWidth = 0.13;
        retailerParams.rightMarginToProductWidth = 0.48;
    } else if (retailerId == RETAILER_BABIESRUS_CODE) {
        retailerParams.code = RETAILER_BABIESRUS_CODE;
        retailerParams.lineSpacing = 0.47;
        retailerParams.productNumberLen = 12;
        retailerParams.shorterProductNumberLen = 6;
        retailerParams.shorterProductsValid = false;                
        retailerParams.productNumberUPC = true;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = true;
        retailerParams.stripAtDescEnd = "TI";
        retailerParams.hasQuantities = true;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.pricePerItemAfterProduct = false;
        retailerParams.pricePerItemBeforeTotalPrice = false;
        retailerParams.extraCharsAfterSubtotalRegex = "(?:[^\\s]{2}(?: )?[^\\s](?: )*)?";
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-";
        retailerParams.dateCode = DATE_REGEX_GENERIC_CODE;
        retailerParams.digitWidthToHeightRatio = 0.4179;
        retailerParams.lineSpacing = 0.42;
        retailerParams.productLeftSideToPriceRightSideNumDigits = 44; // For BabiesRUs a 12-digit product numbers takes 436 pixels (i.e. 36.33 per digit) and space between left side of product and roght side of price is 1,604 => 44 digits
        retailerParams.letterHeightToRangeWidth = 0.043;
        retailerParams.typedRangeToImageWidthTooFar = 0.40;
        // Product width = 114, total width = 440; left margin = 15, right margin = 55
        retailerParams.widthToProductWidth = 3.86;
        retailerParams.leftMarginToProductWidth = 0.13;
        retailerParams.rightMarginToProductWidth = 0.48;
        
    } else if (retailerId == RETAILER_STAPLES_CODE) {
        retailerParams.code = RETAILER_STAPLES_CODE;
        retailerParams.productNumberLen = 12;
        retailerParams.shorterProductNumberLen = 6;
        retailerParams.shorterProductsValid = false;                
        retailerParams.productNumberUPC = true;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionAlignedWithProduct = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.hasQuantities = false;   // TODO - there ARE quantities, handle later
        retailerParams.quantityAtDescriptionStart = true;
        retailerParams.pricePerItemAfterProduct = true;
        retailerParams.pricePerItemBeforeTotalPrice = true;
        retailerParams.extraCharsAfterPricePerItem = 3;
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = "\\-";
        retailerParams.hasExtraCharsAfterPrices = true;
        retailerParams.extraCharsAfterPricesRegex = "N";
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.4771;
        retailerParams.hasDetachedLeadingChar = true;
        // Width = 2,165, letter height=85 => 0.039
        retailerParams.letterHeightToRangeWidth = 0.039;
        // Product width = 573, total width = 2090; left margin = 0, right margin = 78
        retailerParams.widthToProductWidth = 3.65;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0.136;
        
    } else if (retailerId == RETAILER_BESTBUY_CODE) {
        retailerParams.code = RETAILER_BESTBUY_CODE;
        retailerParams.productNumberLen = 7;
        retailerParams.shorterProductsValid = false;                
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = true;
        retailerParams.descriptionAlignedWithProduct = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = false;   // TODO - check
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-";
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5754;
        retailerParams.lineSpacing = 0.30; // Old receipts had this = 0.85, new ones have 0.30
        // Width = 400, letter height=15
        retailerParams.letterHeightToRangeWidth = 0.0375;
        // Product width = 65, total width = 404; left margin = 20, right margin = 20
        retailerParams.widthToProductWidth = 6.215;
        retailerParams.leftMarginToProductWidth = 0.308;
        retailerParams.rightMarginToProductWidth = 0.308;
        
    } else if (retailerId == RETAILER_OFFICEDEPOT_CODE) {
        retailerParams.code = RETAILER_OFFICEDEPOT_CODE;
        retailerParams.productNumberLen = 6;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = false;   // TODO - Check
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.hasDiscounts = false;    // Check
        retailerParams.dateCode = DATE_REGEX_YYYY_CODE;
        // Width = 418, letter height=14
        retailerParams.letterHeightToRangeWidth = 0.0335;
        // Product width = 53, total width = 395; left margin = 0, right margin = 20
        retailerParams.widthToProductWidth = 7.45;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0.377;
        
    } else if (retailerId == RETAILER_HOMEDEPOT_CODE) {
        // Note: TOTAL line price has a $ sign, but no other prices on the receipt. See https://drive.google.com/open?id=0B4jSQhcYsC9VTkM3b194WUE2dzg
        retailerParams.code = RETAILER_HOMEDEPOT_CODE;
        retailerParams.productNumberLen = 12;
        retailerParams.shorterProductNumberLen = 6;
        retailerParams.productNumberUPC = true;
        retailerParams.productNumber2 = "((?:[%digit]){4})((?: )?(?:\\-)(?: )?)(?:(?:[%digit]){3})((?: )?(?:\\-)(?: )?)(?:(?:[%digit]){3})";    // e.g. 0000-123-456 for Home Depot
        retailerParams.productNumber2GroupActions = "300"; // Capturing groups to preserve (1) or delete (0) or strip leading '0's (3)- "300" for Home Depot (2 groups to delete)
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = true;
        retailerParams.pricePerItemAfterProduct = false;
        retailerParams.pricePerItemBeforeTotalPrice = true;
        retailerParams.quantityPPItemTotal = true;
        retailerParams.maxLinesBetweenProductAndQuantity = 2;   // 2nd line, when present, is the full product description, then comes the quantity line
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = "\\-";
        retailerParams.hasExtraCharsAfterPrices = true;
        retailerParams.extraCharsAfterPricesRegex = "N";
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_AMPM_OPTIONAL_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5813;
        // Width = 687, letter height=26
        retailerParams.letterHeightToRangeWidth = 0.0378;
        // Product width = 204, total width = 661; left margin = 0, right margin = 0
        retailerParams.widthToProductWidth = 3.24;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0;
    } else if (retailerId == RETAILER_LOWES_CODE) {
        // I believe Lowe's doesn't have voided products showing up on receipt based on a purchase I made where I asked to take one of two identical items off. Only one item showed up on the receipt, without any mentioned of the voided product. Cashier confirmed my understanding.
        retailerParams.code = RETAILER_LOWES_CODE;
        retailerParams.minProductNumberLen = 4;
        retailerParams.maxProductNumberLen = 6;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = true;
        retailerParams.maxLinesBetweenProductAndQuantity = 2;
        retailerParams.hasDiscounts = false;        // Have one receipt with a discount (minus sign on left side) but product price includes the discount, see https://drive.google.com/open?id=0B4jSQhcYsC9VVElPdFZIbWVBNlU
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        retailerParams.lineSpacing = 0.87;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VVElPdFZIbWVBNlU
        retailerParams.minProductNumberEndToPriceStart = 25; // 6-digit product width = 53, distance from end of product to price start = 269 (30 digits+space), width = 369 (7x product width)
        // Product width = 73, total width = 524; left margin = 20, right margin = 40
        retailerParams.widthToProductWidth = 7;
        retailerParams.leftMarginToProductWidth = 0.27;
        retailerParams.rightMarginToProductWidth = 0.548;
    } else if (retailerId == RETAILER_MACYS_CODE) {
        retailerParams.code = RETAILER_MACYS_CODE;
        retailerParams.minProductNumberLen = 9;
        retailerParams.maxProductNumberLen = 12;
        retailerParams.productNumberUPC = false;
        retailerParams.productNumberUPCWhenPadded = true;     // Means we pad missing leading digits with '0's but only for the purpose of the UPC check
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionAlignedWithProduct = true;
        retailerParams.priceAboveProduct = true;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = false;           // Check
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.hasDiscounts = false;        // Check
        retailerParams.dateCode = DATE_REGEX_AMPM_NOSECONDS_HHMM_YY_CODE;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9Vby1MVHVLSEdvTXM
        // 337 pixels from product end to price start, when product has 12 digits and takes 205 pixels => 19.8 digits+space from product end to price start
        retailerParams.minProductNumberEndToPriceStart = 15; // Normal is 19.8 digits, don't know if Macy's has bogus products but we have a decent permissive margin
        // Product width = 64, total width = 198; left margin = 10, right margin = 10
        retailerParams.widthToProductWidth = 3.09;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0;
        retailerParams.hasCoupons = true;  // Are there coupons with the meaning that item doesn't qualify for price matching?
        retailerParams.couponRegex = "(^(?:[%digit]){2})(\\%(?: )(?i:c)|[~\\s]{1,2}(?: )(?i:cou))";
        retailerParams.numLinesBetweenProductAndCoupon = 2;        
    } else if (retailerId == RETAILER_BBBY_CODE) {
        // Note: TOTAL line price has a $ sign, but no other prices on the receipt. See https://drive.google.com/open?id=0B4jSQhcYsC9VTkM3b194WUE2dzg
        retailerParams.code = RETAILER_BBBY_CODE;
        retailerParams.minProductNumberLen = 9;
        retailerParams.maxProductNumberLen = 11;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasQuantities = false;   // Check
        retailerParams.hasDiscounts = false;    // Check
        retailerParams.dateCode = DATE_REGEX_YY_SLASHES_DASH_HHMM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5640;
        retailerParams.lineSpacing = 0.82;
    } else if (retailerId == RETAILER_SAMSCLUB_CODE) {
        retailerParams.code = RETAILER_SAMSCLUB_CODE;
        retailerParams.minProductNumberLen = 4;
        retailerParams.maxProductNumberLen = 6;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.stripAtDescEnd = "F,I";
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-(?:N)?";
        retailerParams.hasQuantities = false;   // Check that
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        retailerParams.lineSpacing = 0.227;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VeDB6UGliX08xdUE, 6-digit product width = 74 (=> digit + space width around 12), distance from end of product to left of price (if > $100) = 20 digits+space
        retailerParams.minProductNumberEndToPriceStart = 16; // See above example, total receipt width = 445; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 6.00;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_COSTCO_CODE) {
        retailerParams.code = RETAILER_COSTCO_CODE;
        retailerParams.minProductNumberLen = 4;
        retailerParams.maxProductNumberLen = 10; // Some "product numbers" are 10 (see https://drive.google.com/open?id=0B4jSQhcYsC9VOVF1bDQ2TEFUdkk) although these are coupons
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.productAttributes &= (~RETAILER_FLAG_ALIGNED_LEFT); // Clear the flag
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-(?:N)?";
        retailerParams.hasQuantities = false;   // Check that
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        retailerParams.lineSpacing = 0.57;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VZkhWZktuS2tuOFE, 6-digit product width = 109 (=> digit + space width around 18.17), distance from end of product to left of price (if > $100) = 349 (19.2 digits+space)
        retailerParams.minProductNumberEndToPriceStart = 15; // See above example, total receipt width = 695; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 6.4;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_BJSWHOLESALE_CODE) {
        retailerParams.code = RETAILER_BJSWHOLESALE_CODE;
        retailerParams.minProductNumberLen = 9;
        retailerParams.maxProductNumberLen = 12;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-(?:.)?"; // It's N|B||T|F but so many possibilities that it's best to just accept any single char
        retailerParams.hasQuantities = false;   // Check that
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        // Digit height = 14, spacing = 12
        retailerParams.lineSpacing = 0.86;
        // 12-digit product width = 125 (=> digit + space width around 10.4), distance from start of product to left of price (if > $100) = 343 (33 digits+space). NOTE: products are aligned left and have variable length so can't use minProductNumberEndToPriceStart
        retailerParams.minProductNumberStartToPriceStart = 28;
        // See above example, total receipt width = 421; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 3.4;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_KOHLS_CODE) {
        retailerParams.code = RETAILER_KOHLS_CODE;
        retailerParams.productNumberLen = 12;
        retailerParams.productNumberUPC = true;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasDiscounts = false; // TODO - check
        retailerParams.hasQuantities = false;   // Check that
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        // Digit height = 14, spacing = 12
        retailerParams.lineSpacing = 0.86;
        // 12-digit product width = 125 (=> digit + space width around 10.4), distance from start of product to left of price (if > $100) = 343 (33 digits+space). NOTE: products are aligned left and have variable length so can't use minProductNumberEndToPriceStart
        retailerParams.minProductNumberStartToPriceStart = 28;
        // See above example, total receipt width = 421; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 3.4;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_KMART_CODE) {
        retailerParams.code = RETAILER_KMART_CODE;
        retailerParams.productNumberLen = 11;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = "A";
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 4; // "ea" possibly broken into 4 chars)
        retailerParams.quantityPPItemTotal = true;
        retailerParams.pricePerItemTaggedAsPrice = true;
        retailerParams.maxLinesBetweenProductAndQuantity = 1; // Even though the space below product line is taller than usual
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-(?:.)?";
        retailerParams.hasDiscountsWithoutProductNumbers = true; // See https://drive.google.com/open?id=0B4jSQhcYsC9VdVpqUGJueDdVRW8
        retailerParams.dateCode = DATE_REGEX_YY_DASHES_OR_SLASHES_HHMMSS_CODE;
        // Digit height = 17, line spacing = 20
        retailerParams.lineSpacing = 1.18;
        // Product width = 132 (11 digits), total width = 463; left margin = 20, right margin = 20
        retailerParams.widthToProductWidth = 3.5;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0;
        retailerParams.productNumbersLeftmost = false;
        retailerParams.productPricesRightmost = true;
        retailerParams.noZeroBelowOneDollar = true;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 2.25;   // w = 12, smallest space = 27 (very spaced)
        retailerParams.hasSubtotal = false;
        retailerParams.hasTaxBalTotal = true;  // e.g. TAX 1.79 BAL 4.75" (Kmart)
        retailerParams.taxRegex = "(?:(?:^| )TAX(?:$| ))";      // The "TAX" part of the TaxBal combo above, e.g. "TAX"
        retailerParams.balRegex = "(?:(?:^| )BAL(?:$| ))";      // The "BAL" part of the TaxBal combo above, e.g. "BAL"
    } else if (retailerId == RETAILER_DUANEREADE_CODE) {
        retailerParams.code = RETAILER_DUANEREADE_CODE;
        retailerParams.productNumberLen = 11;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionOverlapsWithProduct = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasDiscounts = false; // TODO - check
        retailerParams.hasQuantities = true;
        retailerParams.quantityPPItemTotal = false;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        // Digit height = 85, spacing = 15
        retailerParams.lineSpacing = 0.18;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VV3lzU1QwbDU4S0E
        // 11-digit product width = 579 (=> digit + space width around 52.6), distance from end of product to left of price (if > $100) = 800 (15 digits+space). NOTE: products are aligned left and have fixed length 11 digits so OK to use minProductNumberEndToPriceStart
        retailerParams.minProductNumberEndToPriceStart = 12;
        // See above example, total receipt width = 1,890; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 3.2;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_WALGREENS_CODE) {
        retailerParams.code = RETAILER_WALGREENS_CODE;
        retailerParams.productNumberLen = 11;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionOverlapsWithProduct = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = "\\-";   // See https://drive.google.com/open?id=0B4jSQhcYsC9VZTU2VDJHSUtKMmc
        retailerParams.discountEndingRegex = "\\-MFGC"; // See https://drive.google.com/open?id=0B4jSQhcYsC9VSkpZSnNGbjU1UGc
        retailerParams.hasDiscountsWithoutProductNumbers = true; // See https://drive.google.com/open?id=0B4jSQhcYsC9VZTU2VDJHSUtKMmc and https://drive.google.com/open?id=0B4jSQhcYsC9VLUs0QUY4R2ZVVVE
        retailerParams.hasQuantities = true;
        retailerParams.quantityPPItemTotal = false;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.hasAdditionalPricePerItemWithSlash = true; // e.g. "2 @ 1.79 or 2/3.00" (correct price per item is 1.50 based on total price associated with the product above)
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        // Digit height = 85, spacing = 15
        retailerParams.lineSpacing = 0.18;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VV3lzU1QwbDU4S0E
        // 11-digit product width = 579 (=> digit + space width around 52.6), distance from end of product to left of price (if > $100) = 800 (15 digits+space). NOTE: products are aligned left and have fixed length 11 digits so OK to use minProductNumberEndToPriceStart
        retailerParams.minProductNumberEndToPriceStart = 12;
        // See above example, total receipt width = 1,890; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 3.2;
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_ALBERTSONS_CODE) {
        retailerParams.code = RETAILER_ALBERTSONS_CODE;
        retailerParams.minProductNumberLen = 9;
        retailerParams.maxProductNumberLen = 11;
        retailerParams.shorterProductNumberLen = 4;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionOverlapsWithProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.maxPrice = 10000.00;
        retailerParams.noZeroBelowOneDollar = true;
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = "\\-";
        retailerParams.hasQuantities = false; // TODO check
        retailerParams.quantityPPItemTotal = false;
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_CODE;
        // Digit height = 27, spacing = 12
        retailerParams.lineSpacing = 0.44;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VV3lzU1QwbDU4S0E
        // 11-digit product width = 207 (=> digit + space width around 18.8), distance from end of product to left of price (if > $100) = 90 (4.8 digits+space). NOTE: products are aligned left and have fixed length 11 digits so OK to use minProductNumberEndToPriceStart
        retailerParams.minProductNumberEndToPriceStart = 3;
        // See above example, total receipt width = 1,890; left margin = 30, right margin = 30
        retailerParams.widthToProductWidth = 3.2; // PQ911 how can we use this with a variable length product?
        retailerParams.leftMarginToProductWidth = 0.40;
        retailerParams.rightMarginToProductWidth = 0.40;
        retailerParams.hasSloped6 = true;
    } else if (retailerId == RETAILER_CVS_CODE) {
        // CVS has NO product numbers
        // - need to validate prices based on location and where the description starts otherwise we'll get tons of bogus "products", including all the lines at the end (subtotal, tax, etc)!
        // - avoid tagging products after the subtotal
        // - merging will be a challenge because of the absence of product numbers
        // - some descriptions include stuff tagged as price (see https://drive.google.com/open?id=0B4jSQhcYsC9VU1U3OG1EY0V0NlE), will help avoid if I require the extra digit for prices (then for subtotal I can specifically test the string following as a price). For now I just merged back the original text of such bogus prices back into the description.
        // - handle dates (e.g. "March 27, 2016        4:48 PM"). Note: challenging b/c we exclude lowercase letters
        retailerParams.code = RETAILER_CVS_CODE;
        retailerParams.noProductNumbers = true;
        retailerParams.productNumberLen = 0;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionOverlapsWithProduct = false;
        retailerParams.descriptionAlignedLeft = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.noZeroBelowOneDollar = true;
        retailerParams.maxPrice = 10000.00;
        retailerParams.hasExtraCharsAfterPrices = true;
        retailerParams.extraCharsAfterPricesRegex = "."; // Can be F,B,T or N (at least), might as well accept any single char. Only for product prices, not for the subtotal or total
        retailerParams.extraCharsAfterPricesMeansProductPrice = true;
        retailerParams.hasDiscounts = false;    // TODO check
        retailerParams.hasQuantities = false;   // Set to false even though products DO have quantities because we instead use the quantityAtDescriptionStart indicator
        retailerParams.allProductsHaveQuantities = true;    // Any product without a quantity at line start is suspect
        retailerParams.quantityAtDescriptionStart = true;
        retailerParams.quantityPPItemTotal = false;
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.dateCode = DATE_REGEX_MONTH_DD_COMMA_YYYY_CODE; // e.g. March 27, 2016        4:48 PM
        // Digit height = 21, spacing = 15
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.00;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.40;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.10;    // Almost touching (not including around decimal dot)
        retailerParams.lineSpacing = 0.71;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VU1U3OG1EY0V0NlE
        // 11-digit product width = 207 (=> digit + space width around 18.8), distance from end of product to left of price (if > $100) = 90 (4.8 digits+space). NOTE: products are aligned left and have fixed length 11 digits so OK to use minProductNumberEndToPriceStart
        retailerParams.minProductNumberEndToPriceStart = 0;
        retailerParams.widthToProductWidth = 0;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0;
        retailerParams.descriptionStartToPriceEnd = 36.11;
        retailerParams.leftMarginLeftOfDescription = 3.72;  // As a function of digit width (anything entirely left of that has to be removed)
        retailerParams.rightMarginRightOfPrices = 14.07; // As a function of digit width (anything entirely right of that has to be removed)
        retailerParams.hasSloped6 = true;
        retailerParams.hasPhone = true;
        retailerParams.phoneRegex = "(?:[%digit]{3}\\.[%digit]{3}\\.[%digit]{4})";    // e.g. 212.123.4567
        retailerParams.mergeablePattern1Regex = "(R[^\\s]G|[^\\s]EG|RE[^\\s])[^\\s]{3,}\\s(T[^\\s]N|TR[^\\s]|[^\\s]RN)[^\\s]{4,}\\s(CS[^\\s]{2}|C[^\\s]H[^\\s]|[^\\s]{2}HR)[^\\s]{7,}\\s(S[^\\s]R|[^\\s]TR|ST[^\\s])[^\\s]{3,}"; // e.g. "REG#02 TRN#1259 CSHR#1146440 STR#1059" for CVS
        retailerParams.mergeablePatterns1BeforeProducts = true; // Set if mergeable pattern 1 can only appear before any products
        retailerParams.mergeablePattern1MinLen = 32;    // Actually 37 but to be on the safe side
        retailerParams.hasSloped6 = true;
        retailerParams.minSpaceAroundDotsAsPercentOfWidth = 0.30;   // Actually 0.45 but to be on the safe side
        retailerParams.maxSpaceAroundDotsAsPercentOfWidth = 0.97;   // See https://drive.google.com/open?id=0B4jSQhcYsC9VaFdUNFlLMGs5eUk, widest near '1'
        retailerParams.allUppercase = true;
        retailerParams.letterHeightToRangeWidth = 0.0354; // Set to the av. height of a digit/width of a CVS receipt (without margins)
        retailerParams.digitWidthToHeightRatio = 0.57;
    } else if (retailerId == RETAILER_TRADERJOES_CODE) {
        // CVS has NO product numbers
        // - need to validate prices based on location and where the description starts otherwise we'll get tons of bogus "products", including all the lines at the end (subtotal, tax, etc)!
        // - avoid tagging products after the subtotal
        // - merging will be a challenge because of the absence of product numbers
        // - some descriptions include stuff tagged as price (see https://drive.google.com/open?id=0B4jSQhcYsC9VU1U3OG1EY0V0NlE), will help avoid if I require the extra digit for prices (then for subtotal I can specifically test the string following as a price). For now I just merged back the original text of such bogus prices back into the description.
        // - handle dates (e.g. "March 27, 2016        4:48 PM"). Note: challenging b/c we exclude lowercase letters
        retailerParams.code = RETAILER_TRADERJOES_CODE;
        retailerParams.noProductNumbers = true;
        retailerParams.productNumberLen = 0;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.descriptionOverlapsWithProduct = false;
        retailerParams.descriptionAlignedLeft = true;
        retailerParams.priceAboveProduct = false;
        retailerParams.noZeroBelowOneDollar = true;
        retailerParams.maxPrice = 1000.00;
        retailerParams.hasExtraCharsAfterPrices = false;
        retailerParams.extraCharsAfterPricesMeansProductPrice = false;
        retailerParams.hasDiscounts = false;    // TODO check
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.quantityAtDescriptionStart = false;
        retailerParams.quantityPPItemTotal = false; // Total price in on the product line, not on the quantity line
        retailerParams.dateCode = DATE_REGEX_YYYY_HHMMSS_CODE;
        // Digit height = 21, spacing = 15
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.17;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.58;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.10;    // Almost touching (not including around decimal dot)
        retailerParams.lineSpacing = 0.2215;
        // See https://drive.google.com/open?id=0B4jSQhcYsC9VU1U3OG1EY0V0NlE
        // 11-digit product width = 207 (=> digit + space width around 18.8), distance from end of product to left of price (if > $100) = 90 (4.8 digits+space). NOTE: products are aligned left and have fixed length 11 digits so OK to use minProductNumberEndToPriceStart
        retailerParams.minProductNumberEndToPriceStart = 0;
        retailerParams.widthToProductWidth = 0;
        retailerParams.leftMarginToProductWidth = 0;
        retailerParams.rightMarginToProductWidth = 0;
        retailerParams.descriptionStartToPriceEnd = 42.375;
        retailerParams.subtotalLineExcessWidthDigits = 4.4;
        retailerParams.leftMarginLeftOfDescription = 1.198;  // As a function of digit width (anything entirely left of that has to be removed)
        retailerParams.rightMarginRightOfPrices = 4.03; // As a function of digit width (anything entirely right of that has to be removed)
        retailerParams.hasSloped6 = true;
        retailerParams.hasPhone = true;
        retailerParams.phoneRegex = "(?:\\([%digit]{3}\\)\\s[%digit]{3}\\-[%digit]{4})";    // e.g. (212) 123-4567
        retailerParams.mergeablePatterns1BeforeProducts = true; // Set if mergeable pattern 1 can only appear before any products
        retailerParams.hasSloped6 = false;
        retailerParams.minSpaceAroundDotsAsPercentOfWidth = 0.36;   // See $103.15 subtotal on https://drive.google.com/open?id=0B4jSQhcYsC9VelVnNjBvLTdOd3M, space = 2 versus digit width = 5.558
        retailerParams.maxSpaceAroundDotsAsPercentOfWidth = 0.74;   // See https://drive.google.com/open?id=0B4jSQhcYsC9VaFdUNFlLMGs5eUk, widest near '1'
        retailerParams.allUppercase = true;
        retailerParams.letterHeightToRangeWidth = 0.03805; // Set to the av. height of a digit/width of receipt (without margins)
        retailerParams.digitWidthToHeightRatio = 0.50;
    } else if (retailerId == RETAILER_PETSMART_CODE) {
        retailerParams.code = RETAILER_PETSMART_CODE;
        retailerParams.pricesHaveDollar = false;
        retailerParams.productNumberLen = 13;   // Note: leading digit appears to always be 0, should be enforced
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 3;
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = true;
        retailerParams.discountEndingRegex = "\\-\\."; // e.g. "0.80-N"
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        // width = 504, h=18
        retailerParams.digitWidthToHeightRatio = 0.53939;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.27;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.64;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.18;
        retailerParams.letterHeightToRangeWidth = 0.039;
        retailerParams.productRangeWidthDividedByLetterHeight = 8.4;  // Set for each retailer but init to 0 in case we forget to set for one (no good default)
        // Product width = 137, total width = 619; left margin = 20, right margin = 20
        retailerParams.widthToProductWidth = 2.82;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = true;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_PETCO_CODE) {
        retailerParams.code = RETAILER_PETCO_CODE;
        retailerParams.pricesHaveDollar = false;
        retailerParams.productNumberLen = 9;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 3;
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = "\\-";
        retailerParams.discountEndingRegex = NULL;
        retailerParams.dateCode = DATE_REGEX_YY_SLASHES_HMM_AMPM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5137;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.5;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.5;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.167;
        retailerParams.letterHeightToRangeWidth = 0.0433;
        retailerParams.productRangeWidthDividedByLetterHeight = 5.15;
        retailerParams.widthToProductWidth = 4.53;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = true;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_FAMILYDOLLAR_CODE) {
        retailerParams.code = RETAILER_FAMILYDOLLAR_CODE;
        retailerParams.has0WithDiagonal = true;
        retailerParams.pricesHaveDollar = false;
        retailerParams.productNumberLen = 12;
        retailerParams.productNumberUPC = true;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = true;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.hasMultipleDates = true;
        retailerParams.hasDiscounts = true;
        retailerParams.discountStartingRegex = NULL;
        retailerParams.discountEndingRegex = "\\-\\.";
        retailerParams.dateCode = DATE_REGEX_YYYY_DASHES_HHMM_AMPM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.63;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.667;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.667;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.167;
        retailerParams.letterHeightToRangeWidth = 0.02744;
        retailerParams.productRangeWidthDividedByLetterHeight = 10.44;
        retailerParams.widthToProductWidth = 4.53;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = false;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_FREDMEYER_CODE) {
        retailerParams.code = RETAILER_FREDMEYER_CODE;
        retailerParams.pricesHaveDollar = false;
        retailerParams.productNumberLen = 10;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = true;
        retailerParams.descriptionBeforeProduct = false;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 2;
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = false;
        retailerParams.discountEndingRegex = NULL;
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5417;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.385;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.615;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.23;
        retailerParams.letterHeightToRangeWidth = 0.04428;
        retailerParams.productRangeWidthDividedByLetterHeight = 6.458;
        retailerParams.widthToProductWidth = 3.44;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = true;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_MARSHALLS_CODE) {
        retailerParams.code = RETAILER_MARSHALLS_CODE;
        retailerParams.pricesHaveDollar = false;
        retailerParams.productNumberLen = 9;
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = false; // Verify
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 0;
        retailerParams.hasMultipleDates = false;
        retailerParams.hasDiscounts = false;
        retailerParams.discountEndingRegex = NULL;
        retailerParams.dateCode = DATE_REGEX_YYYY_AMPM_NOSECONDS_CODE;
        retailerParams.digitWidthToHeightRatio = 0.496;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.36;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.36;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.18;
        retailerParams.letterHeightToRangeWidth = 0.0418;
        retailerParams.productRangeWidthDividedByLetterHeight = 4.948;
        retailerParams.widthToProductWidth = 4.79;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = false;
        retailerParams.productPricesRightmost = true;
    } else if (retailerId == RETAILER_WINCO_CODE) {
        retailerParams.code = RETAILER_WINCO_CODE;
        retailerParams.pricesHaveDollar = false;
        retailerParams.noZeroBelowOneDollar = true;
        retailerParams.extraCharsAfterPricesRegex = "(?: \\.{2,3})";
        retailerParams.productNumberLen = 10;
        retailerParams.shorterProductNumberLen = 4;
        retailerParams.shorterProductsValid = true;
        retailerParams.productAttributes &= ~(RETAILER_FLAG_ALIGNED_LEFT | RETAILER_FLAG_ALIGNED_RIGHT); // Product prices not aligned left or right within the productsWithPrices range
        retailerParams.productNumberUPC = false;
        retailerParams.descriptionBeforePrice = false;
        retailerParams.descriptionBeforeProduct = true;
        retailerParams.descriptionAboveProduct = false;
        retailerParams.descriptionBelowProduct = false;
        retailerParams.priceAboveProduct = false;
        retailerParams.stripAtDescEnd = NULL;
        retailerParams.hasQuantities = true;
        retailerParams.maxCharsOnLineAfterPricePerItem = 0;
        retailerParams.maxLinesBetweenProductAndQuantity = 1;
        retailerParams.hasMultipleDates = true;
        retailerParams.hasDiscounts = false;
        retailerParams.discountEndingRegex = NULL;
        retailerParams.dateCode = DATE_REGEX_YY_HHMM_CODE;
        retailerParams.digitWidthToHeightRatio = 0.5656;
        retailerParams.lineSpacing = 0.27;
        retailerParams.spaceWidthAsFunctionOfDigitWidth = 1.13;
        retailerParams.spaceWidthAroundDecimalDotAsFunctionOfDigitWidth = 0.48;
        retailerParams.spaceBetweenLettersAsFunctionOfDigitWidth = 0.16;
        retailerParams.letterHeightToRangeWidth = 0.05;
        retailerParams.productRangeWidthDividedByLetterHeight = 6.03;
        retailerParams.widthToProductWidth = 4.21;
        retailerParams.leftMarginToProductWidth = 0.146;
        retailerParams.rightMarginToProductWidth = 0.146;
        retailerParams.productNumbersLeftmost = false;
        retailerParams.productPricesRightmost = true;
    }
    
    if (retailerParams.minProductNumberLen == 0)
        retailerParams.minProductNumberLen = retailerParams.productNumberLen;
    if (retailerParams.maxProductNumberLen == 0)
        retailerParams.maxProductNumberLen = retailerParams.productNumberLen;
    if ((retailerParams.productNumberLen == 0) && (retailerParams.maxProductNumberLen > 0))
        retailerParams.productNumberLen = retailerParams.maxProductNumberLen;  // Used by OCRValidate.cpp to force certain replacements to make up product numbers - set to longest product number we expect
    
    if (retailerParamsPtr != NULL) {
        *retailerParamsPtr = retailerParams;
    }

    // Create a OCRResults class with the BlinkOCR scan results
    OCRResults curResults;
    curResults.retailerParams = retailerParams;
    
    curResults.retailerLineComponents.push_back(OCRElementTypePhoneNumber);
    curResults.retailerLineComponents.push_back(OCRElementTypeDate);
    if (retailerParams.code == RETAILER_WALMART_CODE) {
        curResults.retailerLineComponents.push_back(OCRElementTypeProductDescription);
        curResults.retailerLineComponents.push_back(OCRElementTypeProductNumber);
        curResults.retailerLineComponents.push_back(OCRElementTypeProductQuantity);
        curResults.retailerLineComponents.push_back(OCRElementTypeSubtotal);
        curResults.retailerLineComponents.push_back(OCRElementTypePartialSubtotal);
        curResults.retailerLineComponents.push_back(OCRElementTypeTotal);
        if (retailerParams.hasDiscountsWithoutProductNumbers) {
            curResults.retailerLineComponents.push_back(OCRElementTypeTax);
            curResults.retailerLineComponents.push_back(OCRElementTypeTaxPrice);
            curResults.retailerLineComponents.push_back(OCRElementTypeBal);
            curResults.retailerLineComponents.push_back(OCRElementTypeBalPrice);
        }
        curResults.retailerLineComponents.push_back(OCRElementTypePrice);
        curResults.retailerLineComponents.push_back(OCRElementTypePartialProductPrice);
        curResults.retailerLineComponents.push_back(OCRElementTypePriceDiscount);
        curResults.retailerLineComponents.push_back(OCRElementTypePotentialPriceDiscount);
    } else {
        if (!retailerParams.noProductNumbers)
            curResults.retailerLineComponents.push_back(OCRElementTypeProductNumber);
        curResults.retailerLineComponents.push_back(OCRElementTypeProductQuantity);
        curResults.retailerLineComponents.push_back(OCRElementTypeProductDescription);
        curResults.retailerLineComponents.push_back(OCRElementTypeSubtotal);
        if (retailerParams.hasMultipleSubtotals)
            curResults.retailerLineComponents.push_back(OCRElementTypePartialSubtotal);
        curResults.retailerLineComponents.push_back(OCRElementTypeTotal);
        if (retailerParams.hasDiscountsWithoutProductNumbers) {
            curResults.retailerLineComponents.push_back(OCRElementTypeTax);
            curResults.retailerLineComponents.push_back(OCRElementTypeTaxPrice);
            curResults.retailerLineComponents.push_back(OCRElementTypeBal);
            curResults.retailerLineComponents.push_back(OCRElementTypeBalPrice);
        }
        curResults.retailerLineComponents.push_back(OCRElementTypePrice);
        if (retailerParams.noProductNumbers)
            curResults.retailerLineComponents.push_back(OCRElementTypeProductPrice);
        curResults.retailerLineComponents.push_back(OCRElementTypePartialProductPrice);
        curResults.retailerLineComponents.push_back(OCRElementTypePriceDiscount);
        curResults.retailerLineComponents.push_back(OCRElementTypePotentialPriceDiscount);
    }
    
    curResults.frameStatus = 0;
    curResults.imageRange.origin.x = curResults.imageRange.origin.y = 0;
    if (binaryImageWidth > 0) {
        curResults.imageRange.size.width = binaryImageWidth;
        curResults.imageRange.size.height = binaryImageHeight;
    } else {
        curResults.imageRange.size.width = 720;
        curResults.imageRange.size.height = 960;
    }
    
    if (binaryImageData != NULL) {
        curResults.binaryImageData = binaryImageData;
        curResults.binaryImageWidth = binaryImageWidth;
        curResults.binaryImageHeight = binaryImageHeight;
        curResults.imageTests = true;
    }
    
    // Allocate a page to store the OCR results
    curResults.page = SmartPtr<OCRPage>(new OCRPage(0));
    
    // Import all OCR elements
    bool lineGroupingDisabled = true;
    int index = 0;
    float xMinOCRRange = 10000, xMaxOCRRange = -1;
    float yMinOCRRange = 10000, yMaxOCRRange = -1;
    MicroBlinkOCRChar *currentCharPtr = results + index;
    while (currentCharPtr->value != 0) {
        SmartPtr<OCRRect> newElement = SmartPtr<OCRRect>(new OCRRect);
        newElement->ch = currentCharPtr->value;
        if (newElement->ch == 0xa)
            lineGroupingDisabled = false;
        // For spaces set width = height = 1 because our parsing code expects that for spaces (e.g. OCRResults::rangeWithNRects)
        if (currentCharPtr->value == ' ') {
            newElement->rect = CGRect(currentCharPtr->ll.x,
                                  currentCharPtr->ul.y,
                                  1,    // Width
                                  1);   // Height
        } else {
            newElement->rect = CGRect(currentCharPtr->ll.x,
                                  currentCharPtr->ul.y,
                                  currentCharPtr->lr.x - currentCharPtr->ll.x,    // Width
                                  currentCharPtr->lr.y - currentCharPtr->ur.y);   // Height
            
            if (rectLeft(newElement->rect) < xMinOCRRange)
                xMinOCRRange = rectLeft(newElement->rect);
            if (rectRight(newElement->rect) > xMaxOCRRange)
                xMaxOCRRange = rectRight(newElement->rect);
            if (rectBottom(newElement->rect) < yMinOCRRange)
                yMinOCRRange = rectBottom(newElement->rect);
            if (rectTop(newElement->rect) > yMaxOCRRange)
                yMaxOCRRange = rectTop(newElement->rect);
        }
        
        newElement->confidence = currentCharPtr->quality;

        // Append new rect to list
        curResults.page->addRect(newElement);
        index++;
        currentCharPtr = results + index;
        newElement.setNull();   // Release reference, helps with memory management
    }
    
    curResults.OCRRange.origin.x = xMinOCRRange;
    curResults.OCRRange.size.width = xMaxOCRRange - xMinOCRRange + 1;
    curResults.OCRRange.origin.y = yMinOCRRange;
    curResults.OCRRange.size.height = yMaxOCRRange - yMinOCRRange + 1;
    
    if (lineGroupingDisabled) {
        DebugLog("processBlinkOCRResults: before building lines - got %d chars [%s]", (int)(curResults.page->text().length()), toUTF8(curResults.page->text()).c_str());
        curResults.buildLines();
        DebugLog("processBlinkOCRResults: after  building lines - got %d chars [%s]", (int)(curResults.page->text().length()), toUTF8(curResults.page->text()).c_str());
    }
    
    // Cleanup outliers by cataloging all long (4+) sequences of uppercase letters or digits that are not all the same (to weed out non-text yielding bogus identical characters like the barcode yielding all 1's). We then remove tallest / shortest 20% from this list and finally get an aggregate average height which will be our baseline to then eliminate anything significantly taller than that.
    vector<int> sequencesHeightsLengths;    // Number of values used for the average of the corresponding item in sequencesHeights
    vector<float> sequencesHeights;         // Average height for each text item meeting the requirement (at least MIN_VALID_SEQUENCE, not all same)

    vector<int> sequencesLengthsDigitsWidths;
    vector<float> sequencesDigitsWidths;
    
    vector<int> sequencesLengthsDigits1Widths;
    vector<float> sequencesDigits1Widths;
    
    vector<int> sequencesLengthsUppercaseWidths;
    vector<float> sequencesUppercaseWidths;
    
    int numConsecutiveUppercase = 0, numConsecutiveUppercaseValidForWidth = 0;
    bool allUppercaseSame = true;
    float sumHeightsUppercase = 0, sumWidthsUppercase = 0;
    int numConsecutiveDigits = 0, numConsecutiveDigitsValidForWidth = 0, numConsecutiveDigits1 = 0;
    bool allDigitsSame = true;
    float sumHeightsDigits = 0, sumWidthsDigits = 0, sumWidthsDigits1 = 0;
    wchar_t lastChar = '\0';
#define MIN_VALID_SEQUENCE      4
    for (int i=0; i<curResults.page->rects.size(); i++) {
        OCRRectPtr r = curResults.page->rects[i];
        wchar_t currentChar = r->ch;
        // End of digits or uppercase sequence
        if (!isUpper(currentChar) && !isDigit(currentChar)) {
            // If we had something worthwhile thus now, add to vectors
            if ((numConsecutiveDigits >= MIN_VALID_SEQUENCE) && !allDigitsSame && (sumHeightsDigits > 0)) {
                sequencesHeightsLengths.push_back(numConsecutiveDigits);
                sequencesHeights.push_back(sumHeightsDigits/numConsecutiveDigits);
                if (numConsecutiveDigitsValidForWidth >= MIN_VALID_SEQUENCE) {
                    int n = numConsecutiveDigitsValidForWidth;
                    float val = sumWidthsDigits/numConsecutiveDigitsValidForWidth;
                    vector<int> &lengthesV = sequencesLengthsDigitsWidths;
                    vector<float> &valuesV = sequencesDigitsWidths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
                if (numConsecutiveDigits1 > 0) {
                    int n = numConsecutiveDigits1;
                    float val = sumWidthsDigits1/numConsecutiveDigits1;
                    vector<int> &lengthesV = sequencesLengthsDigits1Widths;
                    vector<float> &valuesV = sequencesDigits1Widths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
            } else if ((numConsecutiveUppercase >= MIN_VALID_SEQUENCE) && !allUppercaseSame && (sumHeightsUppercase > 0)) {
                {
                    int n = numConsecutiveUppercase;
                    float val = sumHeightsUppercase/numConsecutiveUppercase;
                    vector<int> &lengthesV = sequencesHeightsLengths;
                    vector<float> &valuesV = sequencesHeights;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }

                {
                    int n = numConsecutiveUppercaseValidForWidth;
                    float val = sumWidthsUppercase/numConsecutiveUppercaseValidForWidth;
                    vector<int> &lengthesV = sequencesLengthsUppercaseWidths;
                    vector<float> &valuesV = sequencesUppercaseWidths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
            }
            // Reset everything
            numConsecutiveUppercase = numConsecutiveUppercaseValidForWidth =  numConsecutiveDigits = numConsecutiveDigitsValidForWidth = numConsecutiveDigits1 = 0;
            sumHeightsDigits = sumHeightsUppercase = sumWidthsDigits = sumWidthsDigits = sumWidthsUppercase = 0;
            allDigitsSame = allUppercaseSame = true;
            lastChar = '\0';
            continue;
        }
        
        if (isDigit(currentChar)) {
            // Terminates valid uppercase sequence?
            if ((numConsecutiveUppercase >= MIN_VALID_SEQUENCE) && !allUppercaseSame && (sumHeightsUppercase > 0)) {
                {
                    int n = numConsecutiveUppercase;
                    float val = sumHeightsUppercase/numConsecutiveUppercase;
                    vector<int> &lengthesV = sequencesHeightsLengths;
                    vector<float> &valuesV = sequencesHeights;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
                {
                    int n = numConsecutiveUppercaseValidForWidth;
                    float val = sumWidthsUppercase/numConsecutiveUppercaseValidForWidth;
                    vector<int> &lengthesV = sequencesLengthsUppercaseWidths;
                    vector<float> &valuesV = sequencesUppercaseWidths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
            }
            numConsecutiveUppercase = numConsecutiveUppercaseValidForWidth = 0; sumHeightsUppercase = sumWidthsUppercase = 0; allUppercaseSame = true;
            if ((numConsecutiveDigits > 0) && (currentChar != lastChar))
                allDigitsSame = false;
            sumHeightsDigits += r->rect.size.height;
            if (currentChar != '1') {
                sumWidthsDigits += r->rect.size.width;
                numConsecutiveDigitsValidForWidth++;
            } else {
                sumWidthsDigits1 += r->rect.size.width;
                numConsecutiveDigits1++;
            }
            numConsecutiveDigits++;
        } else if (isUpper(currentChar)) {
            // Terminates valid digits sequence?
            if ((numConsecutiveDigits >= MIN_VALID_SEQUENCE) && !allDigitsSame && (sumHeightsDigits > 0)) {
                {
                    int n = numConsecutiveDigits;
                    float val = sumHeightsDigits/numConsecutiveDigits;
                    vector<int> &lengthesV = sequencesHeightsLengths;
                    vector<float> &valuesV = sequencesHeights;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
                if (numConsecutiveDigitsValidForWidth >= MIN_VALID_SEQUENCE) {
                    int n = numConsecutiveDigitsValidForWidth;
                    float val = sumWidthsDigits/numConsecutiveDigitsValidForWidth;
                    vector<int> &lengthesV = sequencesLengthsDigitsWidths;
                    vector<float> &valuesV = sequencesDigitsWidths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
                if (numConsecutiveDigits1 > 0) {
                    int n = numConsecutiveDigits1;
                    float val = sumWidthsDigits1/numConsecutiveDigits1;
                    vector<int> &lengthesV = sequencesLengthsDigits1Widths;
                    vector<float> &valuesV = sequencesDigits1Widths;
                    int pos = insertToFloatVectorInOrder(valuesV, val);
                    if (pos < lengthesV.size()) {
                        lengthesV.insert(lengthesV.begin() + pos, n);
                    } else {
                        lengthesV.push_back(n);
                    }
                }
            }
            numConsecutiveDigits = numConsecutiveDigitsValidForWidth = numConsecutiveDigits1 = 0; sumHeightsDigits = sumWidthsDigits = sumWidthsDigits1 = 0; allDigitsSame = true;
            if ((numConsecutiveUppercase > 0) && (currentChar != lastChar))
                allUppercaseSame = false;
            sumHeightsUppercase += r->rect.size.height;
            numConsecutiveUppercase++;
            numConsecutiveUppercaseValidForWidth++;
        }
        lastChar = currentChar;
    }
    
//#if DEBUG
//    for (int i=0; i<sequencesHeights.size(); i++) {
//        DebugLog("Sequence #%d: length=%d, av. height=%.0f", i, sequencesLenghts[i], sequencesHeights[i]);
//    }
//#endif
    
    // Eliminate top/bottom 20% of averages
    if (sequencesHeights.size() >= 5) {
        int percent20 = (int)floor((float)sequencesHeights.size()/5);
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            sequencesHeights.erase(sequencesHeights.begin());
            sequencesHeightsLengths.erase(sequencesHeightsLengths.begin());
            sequencesHeights.erase(sequencesHeights.end()-1);
            sequencesHeightsLengths.erase(sequencesHeightsLengths.end()-1);
        }
    }
    
    if (sequencesDigitsWidths.size() >= 5) {
        int percent20 = (int)floor((float)sequencesDigitsWidths.size()/5);
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            sequencesDigitsWidths.erase(sequencesDigitsWidths.begin());
            sequencesLengthsDigitsWidths.erase(sequencesLengthsDigitsWidths.begin());
            sequencesDigitsWidths.erase(sequencesDigitsWidths.end()-1);
            sequencesLengthsDigitsWidths.erase(sequencesLengthsDigitsWidths.end()-1);
        }
    }
    
    if (sequencesDigits1Widths.size() >= 5) {
        int percent20 = (int)floor((float)sequencesDigits1Widths.size()/5);
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            sequencesDigits1Widths.erase(sequencesDigits1Widths.begin());
            sequencesLengthsDigits1Widths.erase(sequencesLengthsDigits1Widths.begin());
            sequencesDigits1Widths.erase(sequencesDigits1Widths.end()-1);
            sequencesLengthsDigits1Widths.erase(sequencesLengthsDigits1Widths.end()-1);
        }
    }
    
    if (sequencesUppercaseWidths.size() >= 5) {
        int percent20 = (int)floor((float)sequencesUppercaseWidths.size()/5);
        if (percent20 == 0) percent20 = 1;
        for (int i=0; i<percent20; i++) {
            sequencesUppercaseWidths.erase(sequencesUppercaseWidths.begin());
            sequencesLengthsUppercaseWidths.erase(sequencesLengthsUppercaseWidths.begin());
            sequencesUppercaseWidths.erase(sequencesUppercaseWidths.end()-1);
            sequencesLengthsUppercaseWidths.erase(sequencesLengthsUppercaseWidths.end()-1);
        }
    }

//#if DEBUG
//    DebugLog("--- After removing top/bottom 20%% ---")
//    for (int i=0; i<sequencesHeights.size(); i++) {
//        DebugLog("Sequence #%d: length=%d, av. height=%.0f", i, sequencesLenghts[i], sequencesHeights[i]);
//    }
//#endif
    
    // Calculate one average from all elements
    float sumHeights = 0;
    int numHeights = 0;
    for (int i=0; i<sequencesHeightsLengths.size(); i++) {
        sumHeights += (sequencesHeights[i] * sequencesHeightsLengths[i]);
        numHeights += sequencesHeightsLengths[i];
    }
    
    float overallHeightAverage = ((numHeights>0)? sumHeights/numHeights:0);
    // Note: assumes digits and uppercase letters have same height
    curResults.globalStats.averageHeightDigits.average = overallHeightAverage;
    curResults.globalStats.averageHeightDigits.count = numHeights;
    curResults.globalStats.averageHeightDigits.sum = sumHeights;
    curResults.globalStats.averageHeightUppercase.average = overallHeightAverage;
    curResults.globalStats.averageHeightUppercase.count = numHeights;
    curResults.globalStats.averageHeightUppercase.sum = sumHeights;
    curResults.globalStats.averageHeight.average = overallHeightAverage;
    curResults.globalStats.averageHeight.count = numHeights;
    curResults.globalStats.averageHeight.sum = sumHeights;
    
    float sumDigitWidths = 0;
    int numDigitWidths = 0;
    for (int i=0; i<sequencesLengthsDigitsWidths.size(); i++) {
        sumDigitWidths += (sequencesDigitsWidths[i] * sequencesLengthsDigitsWidths[i]);
        numDigitWidths += sequencesLengthsDigitsWidths[i];
    }
    float digitWidthAverage = ((numDigitWidths>0)? sumDigitWidths/numDigitWidths:0);
    // On some receipts letters get either chopped in half vertically or segmented wrongly because there is no space between letters (like CVS). This leads to often incorrect average letter width while letter height is much more reliable => use this ratio to derive digit width from digit height
    if ((curResults.retailerParams.digitWidthToHeightRatio > 0) && (numHeights > numDigitWidths * 2))
        digitWidthAverage = overallHeightAverage * curResults.retailerParams.digitWidthToHeightRatio;
    curResults.globalStats.averageWidthDigits.average = digitWidthAverage;
    curResults.globalStats.averageWidthDigits.count = numDigitWidths;
    curResults.globalStats.averageWidthDigits.sum = sumDigitWidths;
    
    // 2016-05-15 removing the below as it caused us to skip all image tests on https://drive.google.com/open?id=0B4jSQhcYsC9VNVJHMS1wdko5TEE. No harm trying to do OCR, is there?
    //if (numWidths < 3)
    //    curResults.imageTests = false;  // No point doing OCR validation on a frame so suspicious that it has fewer than 3 digits!
    
    float sumDigit1Widths = 0;
    float numDigit1Widths = 0;
    for (int i=0; i<sequencesLengthsDigits1Widths.size(); i++) {
        sumDigit1Widths += (sequencesDigits1Widths[i] * sequencesLengthsDigits1Widths[i]);
        numDigit1Widths += sequencesLengthsDigits1Widths[i];
    }
    float digit1WidthAverage = sumDigit1Widths / numDigit1Widths;
    curResults.globalStats.averageWidthDigit1.average = digit1WidthAverage;
    curResults.globalStats.averageWidthDigit1.count = numDigit1Widths;
    curResults.globalStats.averageWidthDigit1.sum = sumDigit1Widths;

    float sumUppercaseWidths = 0;
    float numUppercaseWidths = 0;
    for (int i=0; i<sequencesLengthsUppercaseWidths.size(); i++) {
        sumUppercaseWidths += (sequencesUppercaseWidths[i] * sequencesLengthsUppercaseWidths[i]);
        numUppercaseWidths += sequencesLengthsUppercaseWidths[i];
    }
    float uppercaseWidthAverage = sumUppercaseWidths / numUppercaseWidths;
    curResults.globalStats.averageWidthUppercase.average = uppercaseWidthAverage;
    curResults.globalStats.averageWidthUppercase.count = numUppercaseWidths;
    curResults.globalStats.averageWidthUppercase.sum = sumUppercaseWidths;
    
    // Also set the generic width
    if (numUppercaseWidths + numDigitWidths > 0) {
        curResults.globalStats.averageWidth.average = (uppercaseWidthAverage * numUppercaseWidths + digitWidthAverage * numDigitWidths)/(numUppercaseWidths + numDigitWidths);
        curResults.globalStats.averageWidth.count = numUppercaseWidths + numDigitWidths;
        curResults.globalStats.averageWidth.sum = uppercaseWidthAverage * numUppercaseWidths + digitWidthAverage * numDigitWidths;
    }
    
    DebugLog("processBlinkOCRResults: about to remove outliers");
    
    //bool gotBarcode = false;
    // Finally, iterate over all rects to remove outliers relative to this average!
    for (int i=0; i<curResults.page->rects.size(); i++) {
        OCRRectPtr r = curResults.page->rects[i];
        // Some receipts have font that IS 3x larger than average height! See https://drive.google.com/open?id=0B4jSQhcYsC9VSEV0WmtZVTFJT2s . Changing from 1.5x to 4x
        if ((r->ch != '\n') && (r->rect.size.height >= 4 * overallHeightAverage)) {
            // Before we remove, test whether this might be the start of the barcode!
            // 74 versus average 23 for Target, with width between 5 and 11 (i.e. height = at least 6x width), spacing is at most 3 (height / 25)
//            if (!gotBarcode && (barCodeRectPtr != NULL) && (r->rect.size.height >= 2.0 * overallHeightAverage) && (r->rect.size.height >= 2.0 * r->rect.size.width)) {
//                CGRect firstRect = r->rect;
//                CGRect lastRect = firstRect;
//                float minY = MAXFLOAT, maxY = -1;
//                int countOutliers = 0;  // Allow up to 2 rect too small / big
//                AverageWithCount averageHeightBarcode (firstRect.size.height, firstRect.size.height, 1);
//                averageHeightBarcode.sum = firstRect.size.height; averageHeightBarcode.count = 1;
//                int j;
//                for (j=i+1; j<curResults.page->rects.size(); j++) {
//                    OCRRectPtr p = curResults.page->rects[j];
//                    if (p->ch == '\n')
//                        break;
//                    float spacing = rectSpaceBetweenRects(curResults.page->rects[j-1]->rect, p->rect);
//                    if (spacing > averageHeightBarcode.average / 4)
//                        break;
//                    if ((p->rect.size.height > averageHeightBarcode.average * 1.5) || (p->rect.size.height < averageHeightBarcode.average * 0.5)) {
//                        if (++countOutliers > 2)
//                            break;
//                    }
//                    averageHeightBarcode.sum += p->rect.size.height; averageHeightBarcode.count++; averageHeightBarcode.average = averageHeightBarcode.sum/averageHeightBarcode.count;
//                    lastRect = p->rect;
//                    if (rectBottom(lastRect) < minY) minY = rectBottom(lastRect);
//                    if (rectTop(lastRect) > maxY) maxY = rectTop(lastRect);
//                }
//                if (j > i + 1) {
//                    CGRect rangeBarcode;
//                    rangeBarcode.origin.x = rectLeft(firstRect);
//                    rangeBarcode.origin.y = minY;
//                    rangeBarcode.size.width = rectRight(lastRect) - rectLeft(firstRect) + 1;
//                    rangeBarcode.size.height = maxY - minY + 1;
//#if DEBUG
//                    if (j > i + 20) {
//                        WarningLog("processBlinkOCRResults: found likely barcode in range [%.0f,%.0f - %.0f,%.0f] w=%.0f,h=%.0f at index [%d]", rectLeft(rangeBarcode), rectBottom(rangeBarcode), rectRight(rangeBarcode), rectTop(rangeBarcode), rangeBarcode.size.width, rangeBarcode.size.height, i);
//                    }
//#endif
//                    // Normal barcode is around 313 wide and 60 tall (5.22x). On another receipt found w=330, h=80 (4.125x)
//                    // Do NOT test against range height because of possible slant making it much taller than actual element height
//                    if ((rangeBarcode.size.width > 3.9 * averageHeightBarcode.average) && (rangeBarcode.size.width < 5.8 * averageHeightBarcode.average)) {
//                        gotBarcode = true;
//                        WarningLog("processBlinkOCRResults: found barcode in range [%.0f,%.0f - %.0f,%.0f] w=%.0f,h=%.0f at index [%d]", rectLeft(rangeBarcode), rectBottom(rangeBarcode), rectRight(rangeBarcode), rectTop(rangeBarcode), rangeBarcode.size.width, rangeBarcode.size.height, i);
//                        // Add a margin all around of 5% of height
//                        rangeBarcode.origin.x -= averageHeightBarcode.average * 0.05;
//                        rangeBarcode.size.width += averageHeightBarcode.average * 0.10;
//                        rangeBarcode.origin.y -= averageHeightBarcode.average * 0.05;
//                        rangeBarcode.size.height += averageHeightBarcode.average * 0.10;
//                        *barCodeRectPtr = rangeBarcode; // Return to caller
//                    }
//                }
//            }
            // Remove!
            LayoutLog("processBlinkOCRResults: removing outlier [%c] at index [%d], height=%.0f versus average [%.0f]", r->ch, i, r->rect.size.height, overallHeightAverage);
            curResults.page->removeRectAtIndex(i);
            if ((i < curResults.page->rects.size()) && ((i == 0) || (curResults.page->rects[i-1]->ch == '\n'))) {
                // Two sequential newlines, remove current!
                curResults.page->removeRectAtIndex(i);
                i--; // So loop doesn't skip new element at index i
            }
            i--; // So loop doesn't skip new element at index i
        } else {
            // Add to global stats only if close to global height average (even if not eliminated as outlier, we want clean data only)
            if ((r->rect.size.height < overallHeightAverage * 1.15) && (r->rect.size.height > overallHeightAverage * 0.85)) {
                if (r->ch == '1') {
                    curResults.globalStats.averageWidthDigit1 = curResults.globalStats.updateAverageWithValueAdding(curResults.globalStats.averageWidthDigit1, r->rect.size.width, true);
                    curResults.globalStats.averageHeightDigit1 = curResults.globalStats.updateAverageWithValueAdding(curResults.globalStats.averageHeightDigit1, r->rect.size.width, true);
                }
            }
        }
    } // Remove outliers
    
    DebugLog("processBlinkOCRResults: got %d chars [%s]", (int)(curResults.page->text().length()), toUTF8(curResults.page->text()).c_str());
    
    // Next: assign lines to matches
    // Build fragments & add them to the matchesAddressBook array
	curResults.buildAllWords();

//#if 0
    // In some case Blink breaks strings in the middle of a word (e.g. product number), see https://www.pivotaltracker.com/story/show/109484110
    // Special pass to merge such items. Note: this is the first of 3 merging passes. It is very possible that the 2nd pass is un needed
    for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
        MatchPtr itemBefore = curResults.matchesAddressBook[i];
        CGRect rectBefore = OCRResults::matchRange(itemBefore);
        float tallLetterBefore = OCRResults::averageHeightTallLetters(itemBefore);
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            if (itemBeforeText == L"63") {
                DebugLog("");
            }
#endif
        for (int j=0; j<(int)curResults.matchesAddressBook.size(); j++) {
            if (j == i) continue;
        
            MatchPtr itemAfter = curResults.matchesAddressBook[j];
            CGRect rectAfter = OCRResults::matchRange(itemAfter);
            float tallLetterAfter = OCRResults::averageHeightTallLetters(itemAfter);
            
            float averageHeightTallLetter = (getIntFromMatch(itemBefore, matchKeyN) * tallLetterBefore + getIntFromMatch(itemAfter, matchKeyN) * tallLetterAfter) / (getIntFromMatch(itemBefore, matchKeyN) + getIntFromMatch(itemAfter, matchKeyN));
            
            float spaceBetween = rectLeft(rectAfter) - rectRight(rectBefore);
            
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            wstring itemAfterText = getStringFromMatch(itemAfter, matchKeyText);
            if ((itemBeforeText == L"212260421 ..N") && (itemAfterText == L"2 @ $2.69 38")) {
                DebugLog("");
            }
#endif
            if (abs(spaceBetween) < averageHeightTallLetter * 3) {
                // Now also test if there is a Y overlap
                float overlapAmount, overlapPercent;
                OCRResults::yOverlapFacingSides(itemBefore, itemAfter, overlapAmount, overlapPercent, false);
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            wstring itemAfterText = getStringFromMatch(itemAfter, matchKeyText);
            if ((itemBeforeText == L"63") && (itemAfterText == L"TEG")) {
                DebugLog("");
            }
#endif
                if (overlapPercent > 0.50) {
                    // Merge!
                    DebugLog("processBlinkOCRResults: early-early merging [%s] + [%s] into one item [%s]", toUTF8(getStringFromMatch(itemBefore, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemAfter, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemBefore, matchKeyText) + getStringFromMatch(itemAfter, matchKeyText)).c_str());
                    OCRResults::mergeM1WithM2(itemBefore, itemAfter);
                    unsigned long statusValue = 0;
                    if (itemBefore.isExists(matchKeyStatus)) {
                        statusValue = getUnsignedLongFromMatch(itemBefore, matchKeyStatus);
                    }
                    statusValue |= matchKeyStatusMerged;
                    itemBefore[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(statusValue)).castDown<EmptyType>();
                    // Delete item that got merged
                    curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + j);
                    if (j < i) {
                        i--;    // We removed an item with index smaller than i, actual new i is therefore one less
                    }
                    // Continue looking for items possibly needing to get merged into itemBefore - but need to update rectBefore to reflect the newly merged item
                    // Invalidate range now that we merged!
                    itemBefore->erase(matchKeyXMin);
                    itemBefore->erase(matchKeyXMax);
                    itemBefore->erase(matchKeyYMin);
                    itemBefore->erase(matchKeyXMax);
                    itemBefore->erase(matchKeyAverageHeightTallLetters);
                    rectBefore = OCRResults::matchRange(itemBefore);
                    j--;    // We deleted the item at position j, step back so that loop increments j back to where we are now
                    continue;
                } // Y overlapping
            } // space less than height before / after
        } // check all other items
    } // checking for items to merge
//#endif //0
    
    // Now build OCRWord structures out of each match, so that we can then apply OCR corrections to them
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {

        MatchPtr notAnalyzedMatch = curResults.matchesAddressBook[i];
        wstring *newText = &getStringFromMatch(notAnalyzedMatch, matchKeyText);
    
        int notAnalyzedMatchRectsN = (int)*notAnalyzedMatch[matchKeyN].castDown<size_t>();
        RectVectorPtr notAnalyzedMatchRects = notAnalyzedMatch[matchKeyRects].castDown<RectVector>();
        OCRWordPtr notAnalyzedOCRFragment = OCRWordPtr(new OCRWord(notAnalyzedMatchRectsN));
        float notAnalyzedMatchConfidence = *notAnalyzedMatch[matchKeyConfidence].castDown<float>();
						
        int numberOfLetters = (int)min(newText->length(), static_cast<size_t>(notAnalyzedMatchRectsN));
        bool sameNumberOfRectAndLetters = true;
        if (newText->length() != notAnalyzedMatchRectsN) {
            sameNumberOfRectAndLetters = false;
            ErrorLog("adjustTypes: error, item [%s] length %d not equal to N rects %d", toUTF8(*newText).c_str(), (unsigned short)newText->length(), notAnalyzedMatchRectsN);
        }
        // Add letters one by one, this will also calculate averages
        bool foundSpace = false;
        int indexOneBeforeLast = 0;
        int numVerticalLineLetters = 0, numNonSpaceLetters = 0;
        for (int j=0; sameNumberOfRectAndLetters && (j < numberOfLetters); j++) {
            if ((j != 0) && (j != (numberOfLetters - 1)) && ((*newText)[j] == ' '))
                foundSpace = true;
            else if ((j == numberOfLetters - 1) && (j > 4))
            {
                // Eliminate last letter if far from previous letter and abnormally larger
                CGRect beforeLastRect = *(*notAnalyzedMatchRects)[j-1];
                CGRect lastRect = *(*notAnalyzedMatchRects)[j];
                if (((*newText)[j-1] == ' ') && (j - 2 > 1)) {
                    lastRect = *(*notAnalyzedMatchRects)[j-2];
                    indexOneBeforeLast = j-2;
                } else {
                    indexOneBeforeLast = j-1;
                }
                float spacing = rectLeft(lastRect) - rectRight(beforeLastRect) + 1;
                if ((spacing > notAnalyzedOCRFragment->averageSpacing.average * 2.0)
                    && (notAnalyzedOCRFragment->averageHeightNormalLowercase.count > 3)
                    && (spacing > notAnalyzedOCRFragment->averageWidthNormalLowercase.average * 1.5)
                    && (lastRect.size.height > notAnalyzedOCRFragment->averageHeightNormalLowercase.average * 1.5)
                    && (lastRect.size.width > notAnalyzedOCRFragment->averageWidthNormalLowercase.average * 1.5)
                    && (lastRect.size.height > beforeLastRect.size.height * 1.5))
               {
                    // Truncate
                    DebugLog("processBlinkOCRResults: removing large char at the end of [%s]", toUTF8(*newText).c_str());
                    notAnalyzedMatch[matchKeyN] = SmartPtr<size_t>(new size_t(indexOneBeforeLast + 1)).castDown<EmptyType>();
                    notAnalyzedMatch[matchKeyText] = SmartPtr<wstring>(new wstring((*newText).substr(0, indexOneBeforeLast + 1))).castDown<EmptyType>();
                    newText = &getStringFromMatch(notAnalyzedMatch, matchKeyText);
                    numberOfLetters = indexOneBeforeLast + 1;
                    break;
               }
            }
            if ((*newText)[j] != ' ')
                numNonSpaceLetters++;
            if (isVerticalLine((*newText)[j]))
                numVerticalLineLetters++;
            notAnalyzedOCRFragment->addLetterWithRectConfidence((*newText)[j], *(*notAnalyzedMatchRects)[j], notAnalyzedMatchConfidence);
        }
       
        // Set into the fragment. We don't use it at this time.
        // notAnalyzedMatch[matchKeyOCRFragment] = notAnalyzedOCRFragment.castDown<EmptyType>();

        // Process that line and fix characters in it as needed
 
        OCRValidate(notAnalyzedOCRFragment, &curResults);

        // Update item text with new OCRFragment characters (after possible corrections)
        notAnalyzedOCRFragment->setUpdateDirtyBits(); // Not required (individual functions called by OCRUtilsValidate() set it) but just in case
        wstring textString = notAnalyzedOCRFragment->text();
        notAnalyzedMatch[matchKeyText] = SmartPtr<wstring>(new wstring(textString)).castDown<EmptyType>();
        wstring textString2 = notAnalyzedOCRFragment->text2();
        if ((textString2.length() == textString.length()) && (textString2 != textString))
            notAnalyzedMatch[matchKeyText2] = SmartPtr<wstring>(new wstring(textString2)).castDown<EmptyType>();
        newText = NULL;
        
        // Remember stats about percentage of vertical lines - an indicator of possible blurriness
        float percentVertical = ((numNonSpaceLetters > 0)? (float)numVerticalLineLetters/(float)numNonSpaceLetters:-1);
#if DEBUG
        if (percentVertical > 1)
            DebugLog("ERROR [%s] -> percent vertical = %2f", toUTF8(textString).c_str(), percentVertical);
#endif
        notAnalyzedMatch[matchKeyPercentVerticalLines] = (SmartPtr<float> (new float(percentVertical))).castDown<EmptyType>();
        notAnalyzedMatch[matchKeyNumNonSpaceLetters] = (SmartPtr<int> (new int(numNonSpaceLetters))).castDown<EmptyType>();
 
        // Update item rects after possible corrections
        notAnalyzedOCRFragment->adjustRectsInMatch(*notAnalyzedMatch);
    } // iterating over matches
    
//#if DEBUG
//    if (globalResults.frameNumber + 1 == 7) {
//        DebugLog("");
//    }
//#endif

    // Before organizing in blocks, compute range where non-noise stuff has been found, in order to then eliminate noise items (anything that doesn't fit in that range or which just has too few normal chars)
    
    // Compute the area where "normal" elements where found, e.g. "$3.97" but not bogus stuff like "...N"
    float realResultsXMin = 20000, realResultsXMax = -1, realResultsYMin = 20000, realResultsYMax = -1;;
    int numRealItems = 0, numProducts = 0;
    float sumProductWidths = 0;
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        
        wstring itemText = getStringFromMatch(d, matchKeyText);
    
        // Test if it appears to be a normal "real" text item
        /* Criterias:
            - no two consecutive non-letters, i.e. .. or -. etc
            - at least 4 consecutive letters, i.e. $3.45, ABCD etc
            - digits all similar height, same for uppercase letters
        */
        CGRect currentRect = OCRResults::matchRange(d);
//#if DEBUG
//                wstring tmpItemText = getStringFromMatch(d, matchKeyText);
//                if ((tmpItemText == L"..%$...'.'") || (tmpItemText == L"......")) {
//                    DebugLog("");
//                }
//#endif
        float digitWidth = 0, digitHeight = 0, digitSpacing = 0;
        bool uniformDigits = false; int maxDigitsSequence = 0; float averageDigitHeight = 0;
        bool uniformPriceSequence = false; int maxLengthPriceSequence = 0; float averagePriceSequenceHeight = 0;
        float letterWidth = 0, letterHeight = 0, letterSpacing = 0;
        bool uniformLetters = false; int maxLettersSequence = 0; float averageLetterHeight = 0;
        float productWidth = 0;
        bool digitsCheck = OCRResults::averageDigitHeightWidthSpacingInMatch(d, digitWidth, digitHeight, digitSpacing, uniformDigits, maxDigitsSequence, averageDigitHeight, maxLengthPriceSequence, uniformPriceSequence, averagePriceSequenceHeight, productWidth, &curResults.retailerParams);
        if (productWidth > 0) {
            numProducts++;
            sumProductWidths += productWidth;
        }
        bool lettersCheck = OCRResults::averageUppercaseHeightWidthSpacingInMatch(d, letterWidth, letterHeight, letterSpacing, uniformLetters, maxLettersSequence, averageLetterHeight);
        if (digitsCheck || lettersCheck) {
            // 2015-09-29 removing uniform requirement
            if ((maxDigitsSequence >= 4)|| (maxLettersSequence >= 4)) {
#if DEBUG
                wstring tmpItemText = getStringFromMatch(d, matchKeyText);
                DebugLog("Adding [%s] to real elements range [%.0f,%.0f - %.0f,%.0f]", toUTF8(tmpItemText).c_str(), currentRect.origin.x, currentRect.origin.y, currentRect.origin.x + currentRect.size.width - 1, currentRect.origin.y + curResults.realResultsRange.size.height - 1);
                if (currentRect.origin.x < 130) {
                    DebugLog("");
                }
                if (rectRight(currentRect) > 560) {
                    DebugLog("");
                }
#endif
                numRealItems++;
                if (currentRect.origin.x < realResultsXMin)
                    realResultsXMin = currentRect.origin.x;
                if (currentRect.origin.x + currentRect.size.width - 1 > realResultsXMax)
                    realResultsXMax = currentRect.origin.x + currentRect.size.width - 1;
                if (currentRect.origin.y < realResultsYMin)
                    realResultsYMin = currentRect.origin.y;
                if (currentRect.origin.y + currentRect.size.height - 1 > realResultsYMax)
                    realResultsYMax = currentRect.origin.y + currentRect.size.height - 1;
            }
            else {
                LayoutLog("Real range: rejecting [%s]", toUTF8(itemText).c_str());
            }
        }
    } // Computing real results area
    
    if (numProducts > 0) {
        curResults.productWidth = sumProductWidths / numProducts;
        curResults.productDigitWidth = curResults.productWidth / curResults.retailerParams.productNumberLen;
    }

    if (realResultsXMax > 0) {
        curResults.realResultsRange.origin.x = realResultsXMin;
        curResults.realResultsRange.origin.y = realResultsYMin;
        curResults.realResultsRange.size.width = realResultsXMax - realResultsXMin + 1;
        curResults.realResultsRange.size.height = realResultsYMax - realResultsYMin + 1;
        DebugLog("Real elements range [%.0f,%.0f - %.0f,%.0f] product width=%.0f [%d products]", curResults.realResultsRange.origin.x, curResults.realResultsRange.origin.y, curResults.realResultsRange.origin.x + curResults.realResultsRange.size.width - 1, curResults.realResultsRange.origin.y + curResults.realResultsRange.size.height - 1, curResults.productWidth, numProducts);
        
        // Eliminate noise only if we set metrics relating receipt width to product width
            // Found products (and their width)
        if ((curResults.productWidth > 0)
            // We know what width to expect relative to product width
            && (curResults.retailerParams.widthToProductWidth > 0)
            // Real results range width we found is wide enough (15% less is OK)
            && (curResults.realResultsRange.size.width > curResults.productWidth * curResults.retailerParams.widthToProductWidth * 0.85)) {
            float leftMargin = 0, rightMargin = 0;
            if (curResults.retailerParams.leftMarginToProductWidth > 0)
                leftMargin = curResults.productWidth * curResults.retailerParams.leftMarginToProductWidth;
            if (curResults.retailerParams.rightMarginToProductWidth > 0)
                rightMargin = curResults.productWidth * curResults.retailerParams.rightMarginToProductWidth;
            if (curResults.realResultsRange.origin.x - leftMargin < 0)
                leftMargin = curResults.realResultsRange.origin.x;
            curResults.realResultsRange.origin.x -= leftMargin;
            float rightX = curResults.realResultsRange.origin.x + curResults.realResultsRange.size.width - 1 + rightMargin;
            if (rightX >= curResults.imageRange.size.width)
                rightX = curResults.realResultsRange.size.width - 1;
            curResults.realResultsRange.size.width = rightX - curResults.realResultsRange.origin.x + 1;
            DebugLog("Adjusted real elements range [%.0f,%.0f - %.0f,%.0f]", curResults.realResultsRange.origin.x, curResults.realResultsRange.origin.y, curResults.realResultsRange.origin.x + curResults.realResultsRange.size.width - 1, curResults.realResultsRange.origin.y + curResults.realResultsRange.size.height - 1);

            for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
                MatchPtr d = curResults.matchesAddressBook[i];
                CGRect matchRect = OCRResults::matchRange(d);
                wstring itemText = getStringFromMatch(d, matchKeyText);
//#if DEBUG
//                wstring tmpItemText = getStringFromMatch(d, matchKeyText);
//                if (/*(tmpItemText == L"..%$...'.'") ||(tmpItemText == L"......") || (tmpItemText == L".$ ' A.., '") || */(tmpItemText == L". .44 .")) {
//                    DebugLog("");
//                }
//#endif
                    
                if (!(OCRResults::overlappingRectsXAxisAnd(matchRect, curResults.realResultsRange))) {
#if DEBUG
                    DebugLog("processBlinkOCRResults: removing noise [%s] with range [%.0f,%.0f - %.0f,%.0f] outside of real results range [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), rectLeft(matchRect), rectBottom(matchRect), rectRight(matchRect), rectTop(matchRect), rectLeft(curResults.realResultsRange), rectBottom(curResults.realResultsRange), rectRight(curResults.realResultsRange), rectTop(curResults.realResultsRange));
#endif
                
                    // Remove!
                    curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
                    i--; continue;
                }
            } // iterate over matches to remove those outside of real results area
        } // Have valid metrics about receipt width and product width
#if DEBUG
        else {
            DebugLog("");
        }
#endif
    } // Got real results area
    
    
//#if 0
    // In some case Blink breaks strings in the middle of a word (e.g. product number), see https://www.pivotaltracker.com/story/show/109484110
    // Special pass to merge such items
    for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
        MatchPtr itemBefore = curResults.matchesAddressBook[i];
        CGRect rectBefore = OCRResults::matchRange(itemBefore);
        float tallLetterBefore = OCRResults::averageHeightTallLetters(itemBefore);
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            if (itemBeforeText == L"63") {
                DebugLog("");
            }
#endif
        for (int j=0; j<(int)curResults.matchesAddressBook.size(); j++) {
            if (j == i) continue;
        
            MatchPtr itemAfter = curResults.matchesAddressBook[j];
            CGRect rectAfter = OCRResults::matchRange(itemAfter);
            float tallLetterAfter = OCRResults::averageHeightTallLetters(itemAfter);
            
            float averageHeightTallLetter = (getIntFromMatch(itemBefore, matchKeyN) * tallLetterBefore + getIntFromMatch(itemAfter, matchKeyN) * tallLetterAfter) / (getIntFromMatch(itemBefore, matchKeyN) + getIntFromMatch(itemAfter, matchKeyN));
            
            float spaceBetween = rectLeft(rectAfter) - rectRight(rectBefore);
            
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            wstring itemAfterText = getStringFromMatch(itemAfter, matchKeyText);
            if ((itemBeforeText == L"212260421 ..N") && (itemAfterText == L"2 @ $2.69 38")) {
                DebugLog("");
            }
#endif
            if (abs(spaceBetween) < averageHeightTallLetter * 3) {
                // Now also test if there is a Y overlap
                float overlapAmount, overlapPercent;
                OCRResults::yOverlapFacingSides(itemBefore, itemAfter, overlapAmount, overlapPercent, false);
#if DEBUG
            wstring itemBeforeText = getStringFromMatch(itemBefore, matchKeyText);
            wstring itemAfterText = getStringFromMatch(itemAfter, matchKeyText);
            if ((itemBeforeText == L"63") && (itemAfterText == L"TEG")) {
                DebugLog("");
            }
#endif
                if (overlapPercent > 0.50) {
                    // Merge!
                    DebugLog("processBlinkOCRResults: early merging [%s] + [%s] into one item [%s]", toUTF8(getStringFromMatch(itemBefore, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemAfter, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemBefore, matchKeyText) + getStringFromMatch(itemAfter, matchKeyText)).c_str());
                    OCRResults::mergeM1WithM2(itemBefore, itemAfter);
                    int statusValue = 0;
                    if (itemBefore.isExists(matchKeyStatus)) {
                        statusValue = getIntFromMatch(itemBefore, matchKeyStatus);
                    }
                    statusValue |= matchKeyStatusMerged;
                    itemBefore[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(statusValue)).castDown<EmptyType>();
                    // Delete item that got merged
                    curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + j);
                    if (j < i) {
                        i--;    // We removed an item with index smaller than i, actual new i is therefore one less
                    }
                    // Continue looking for items possibly needing to get merged into itemBefore - but need to update rectBefore to reflect the newly merged item
                    // Invalidate range now that we merged!
                    itemBefore->erase(matchKeyXMin);
                    itemBefore->erase(matchKeyXMax);
                    itemBefore->erase(matchKeyYMin);
                    itemBefore->erase(matchKeyXMax);
                    itemBefore->erase(matchKeyAverageHeightTallLetters);
                    rectBefore = OCRResults::matchRange(itemBefore);
                    j--;    // We deleted the item at position j, step back so that loop increments j back to where we are now
                    continue;
                } // Y overlapping
            } // space less than height before / after
        } // check all other items
    } // checking for items to merge
//#endif // 0
    
    // Eliminate noise items
    for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        CGRect matchRect = OCRResults::matchRange(d);
        wstring itemText = getStringFromMatch(d, matchKeyText);
#if DEBUG
        wstring tmpItemText = getStringFromMatch(d, matchKeyText);
        if (tmpItemText == L"-2.B0") {
            DebugLog("");
        }
#endif
        // Remove any item without any digits or uppercase letters
        int nDigits = countLongestDigitsSequence(itemText);
        int nLetters = countLongestLettersSequence(itemText);
        
        float digitWidth = 0, digitHeight = 0, digitSpacing = 0;
        bool uniformDigits = false; int maxDigitsSequence = 0; float averageDigitHeight = 0;
        bool uniformPriceSequence = false; int maxLengthPriceSequence = 0; float averagePriceSequenceHeight = 0;
        float letterWidth = 0, letterHeight = 0, letterSpacing = 0;
        bool uniformLetters = false; int maxLettersSequence = 0; float averageLetterHeight = 0;
        float productWidth = 0;
        bool digitsCheck = OCRResults::averageDigitHeightWidthSpacingInMatch(d, digitWidth, digitHeight, digitSpacing, uniformDigits, maxDigitsSequence, averageDigitHeight, maxLengthPriceSequence, uniformPriceSequence, averagePriceSequenceHeight, productWidth, &curResults.retailerParams);
        if (productWidth > 0) {
            numProducts++;
            sumProductWidths += productWidth;
        }
        bool lettersCheck = OCRResults::averageUppercaseHeightWidthSpacingInMatch(d, letterWidth, letterHeight, letterSpacing, uniformLetters, maxLettersSequence, averageLetterHeight);
//        if ((((nDigits > 0) && !digitsCheck) || ((nLetters > 0) && !lettersCheck))
//            || ((maxDigitsSequence != nDigits) || (maxLettersSequence != nLetters))) {
//            DebugLog("processBlinkOCRResults: ERROR mismatch for item [%s] maxDigitsSequence=%d vs nDigits=%d, maxLettersSequence=%d vs nLetters=%d", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), maxDigitsSequence, nDigits, maxLettersSequence, nLetters);
//            DebugLog("");
//        }

        // Use maxDigitsSequence rather than nDigits to protect prices: averageDigitHeightWidthSpacingInMatch counts the dot, i.e. will return 3 for "1.99"
        if (((digitsCheck && (maxDigitsSequence < 2) && (maxLengthPriceSequence < 2))
                // Also reject when sequence is just 2 (accept with 3 and above)
                || ((digitsCheck && !(curResults.retailerParams.noZeroBelowOneDollar) && (maxDigitsSequence == 2) && (maxLengthPriceSequence == 2))
                    // And either letters are not uniform heights
                    && ((!uniformDigits && !uniformPriceSequence)
                        // Or their average height is abnormal
                        || (((averageDigitHeight < curResults.globalStats.averageHeightUppercase.average * 0.50) || (averageDigitHeight > curResults.globalStats.averageHeightUppercase.average * 1.50))
                            && ((averagePriceSequenceHeight < curResults.globalStats.averageHeightUppercase.average * 0.50) || (averagePriceSequenceHeight > curResults.globalStats.averageHeightUppercase.average * 1.50)))
                        )
                    )
            )
            && ((nLetters < 2)
                            // Also reject when sequence is just 2 (accept with 3 and above)
                || ((nLetters == 2)
                    // And either letters are not uniform heights
                    && ((lettersCheck && !uniformLetters)
                        // Or their average height is abnormal
                        || ((averageLetterHeight < curResults.globalStats.averageHeightUppercase.average * 0.50) || (averageLetterHeight > curResults.globalStats.averageHeightUppercase.average * 1.50))
                        )
                    )
                )) {
#if DEBUG
            DebugLog("processBlinkOCRResults: removing noise [%s] with range [%.0f,%.0f - %.0f,%.0f] not enough digits (%d) or letters (%d)", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), rectLeft(matchRect), rectBottom(matchRect), rectRight(matchRect), rectTop(matchRect), nDigits, nLetters);
            if ((nLetters == 2) && !uniformLetters) {
                DebugLog("");
            }
#endif
            curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
            i--; continue;
        }
    } // Remove noise, iterate over matches

#if DEBUG
    if (globalResults.frameNumber == 5) {
        DebugLog("");
    }
#endif

    // Organize in blocks, lines and columns
    curResults.setBlocks();

    // In some case Blink breaks strings in the middle of a word (e.g. product number), see https://www.pivotaltracker.com/story/show/109484110
    // Special pass to merge such items
    for (int i=0; i<(int)curResults.matchesAddressBook.size()-1; i++) {
        int indexStartLine = i;
        int indexEndLine = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
        for (int j=indexStartLine; j<indexEndLine; j++) {
            MatchPtr itemBefore = curResults.matchesAddressBook[j];
            CGRect rectBefore = OCRResults::matchRange(itemBefore);
            float tallLetterBefore = OCRResults::averageHeightTallLetters(itemBefore);
        
            MatchPtr itemAfter = curResults.matchesAddressBook[j+1];
            CGRect rectAfter = OCRResults::matchRange(itemAfter);
            
            if (rectLeft(rectAfter) < rectLeft(rectBefore)) {
                DebugLog("processBlinkOCRResults: ERROR item %d [%s] [%.0f,%.0f - %.0f,%.0f] before item %d [%s] [%.0f,%.0f - %.0f,%.0f] even though first item is NOT left of next item", j, toUTF8(getStringFromMatch(itemBefore, matchKeyText)).c_str(), rectLeft(rectBefore), rectBottom(rectBefore), rectRight(rectBefore), rectTop(rectBefore), j+1, toUTF8(getStringFromMatch(itemAfter, matchKeyText)).c_str(), rectLeft(rectAfter), rectBottom(rectAfter), rectRight(rectAfter), rectTop(rectAfter));
                continue;
            }
            float tallLetterAfter = OCRResults::averageHeightTallLetters(itemAfter);
            
            float averageHeightTallLetter = (getIntFromMatch(itemBefore, matchKeyN) * tallLetterBefore + getIntFromMatch(itemAfter, matchKeyN) * tallLetterAfter) / (getIntFromMatch(itemBefore, matchKeyN) + getIntFromMatch(itemAfter, matchKeyN));
            
            float spaceBetween = rectLeft(rectAfter) - rectRight(rectBefore);
            if (spaceBetween < averageHeightTallLetter) {
                // Merge!
                DebugLog("processBlinkOCRResults: merging [%s] + [%s] into one item", toUTF8(getStringFromMatch(itemBefore, matchKeyText)).c_str(), toUTF8(getStringFromMatch(itemAfter, matchKeyText)).c_str());
#if DEBUG
                if (spaceBetween < 0) {
                    DebugLog("");
                }
#endif
                OCRResults::mergeM1WithM2(itemBefore, itemAfter);
                int statusValue = 0;
                if (itemBefore.isExists(matchKeyStatus)) {
                    statusValue = getIntFromMatch(itemBefore, matchKeyStatus);
                }
                statusValue |= matchKeyStatusMerged;
                itemBefore[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(statusValue)).castDown<EmptyType>();
                // Invalidate range now that we merged!
                itemBefore->erase(matchKeyXMin);
                itemBefore->erase(matchKeyXMax);
                itemBefore->erase(matchKeyYMin);
                itemBefore->erase(matchKeyXMax);
                // Delete item after
                curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + j + 1);
                indexEndLine--;
                j--; // Stay on this item in case the next-next item also needs to be merged!
                continue;
            }
        } // iterating over current line
        i = indexEndLine; continue;
    } // checking for items to merge
    
    // Eliminate items that are too short
    if (curResults.retailerParams.minItemLen > 0) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            wstring& newText = getStringFromMatch(d, matchKeyText);
            if (newText.length() < curResults.retailerParams.minItemLen)
                curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin()+i--);
        }
    }
    
    // And now we need to re-check for missing spaces!
    // Now build OCRWord structures out of each match, so that we can then apply OCR corrections to them
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {

        MatchPtr notAnalyzedMatch = curResults.matchesAddressBook[i];
        
        if (!notAnalyzedMatch.isExists(matchKeyStatus) || !(getIntFromMatch(notAnalyzedMatch, matchKeyStatus) & matchKeyStatusMerged)) {
            continue;
        }
        
        wstring *newText = &getStringFromMatch(notAnalyzedMatch, matchKeyText);
    
        int notAnalyzedMatchRectsN = (int)*notAnalyzedMatch[matchKeyN].castDown<size_t>();
        RectVectorPtr notAnalyzedMatchRects = notAnalyzedMatch[matchKeyRects].castDown<RectVector>();
						
        if (newText->length() != notAnalyzedMatchRectsN) {
            ErrorLog("adjustTypes: error, item [%s] length %d not equal to N rects %d", toUTF8(*newText).c_str(), (unsigned short)newText->length(), notAnalyzedMatchRectsN);
            continue;
        }
        
        LayoutLog("processMicroBlinkOCRResults: re-checking spaces in item [%s]", toUTF8(*newText).c_str());
        
        OCRWordPtr notAnalyzedOCRFragment = OCRWordPtr(new OCRWord(notAnalyzedMatchRectsN));
        float notAnalyzedMatchConfidence = *notAnalyzedMatch[matchKeyConfidence].castDown<float>();
        // Add letters one by one, this will also calculate averages
        for (int j=0; j<newText->length(); j++) {
            notAnalyzedOCRFragment->addLetterWithRectConfidence((*newText)[j], *(*notAnalyzedMatchRects)[j], notAnalyzedMatchConfidence);
        }

        OCRValidateCheckMissingSpaces(notAnalyzedOCRFragment, &curResults);

        // Update item text with new OCRFragment characters (after possible corrections)
        notAnalyzedOCRFragment->setUpdateDirtyBits(); // Not required (individual functions called by OCRUtilsValidate() set it) but just in case
        wstring textString = notAnalyzedOCRFragment->text();
        if (textString != *newText) {
            LayoutLog("processMicroBlinkOCRResults: FOUND SPACE [%s] -> [%s]", toUTF8(*newText).c_str(), toUTF8(textString).c_str());
            notAnalyzedMatch[matchKeyText] = SmartPtr<wstring>(new wstring(textString)).castDown<EmptyType>();

            // PQ911 check also text2
            // Update item rects after possible corrections
            notAnalyzedOCRFragment->adjustRectsInMatch(*notAnalyzedMatch);
        }
    } // iterating over matches
    
    // Run checks that depend on having entire lines
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {

        MatchPtr notAnalyzedMatch = curResults.matchesAddressBook[i];
        
        wstring *newText = &getStringFromMatch(notAnalyzedMatch, matchKeyText);
    
        int notAnalyzedMatchRectsN = (int)*notAnalyzedMatch[matchKeyN].castDown<size_t>();
        RectVectorPtr notAnalyzedMatchRects = notAnalyzedMatch[matchKeyRects].castDown<RectVector>();
						
        if (newText->length() != notAnalyzedMatchRectsN) {
            ErrorLog("adjustTypes: error, item [%s] length %d not equal to N rects %d", toUTF8(*newText).c_str(), (unsigned short)newText->length(), notAnalyzedMatchRectsN);
            continue;
        }
        
        OCRWordPtr notAnalyzedOCRFragment = OCRWordPtr(new OCRWord(notAnalyzedMatchRectsN));
        float notAnalyzedMatchConfidence = *notAnalyzedMatch[matchKeyConfidence].castDown<float>();
        // Add letters one by one, this will also calculate averages
        for (int j=0; j<newText->length(); j++) {
            notAnalyzedOCRFragment->addLetterWithRectConfidence((*newText)[j], *(*notAnalyzedMatchRects)[j], notAnalyzedMatchConfidence);
        }
        
        OCRValidateFinalPass(notAnalyzedOCRFragment, &curResults);

        // Update item text with new OCRFragment characters (after possible corrections)
        notAnalyzedOCRFragment->setUpdateDirtyBits(); // Not required (individual functions called by OCRUtilsValidate() set it) but just in case
        wstring textString = notAnalyzedOCRFragment->text();
        if (textString != *newText) {
            LayoutLog("processMicroBlinkOCRResults: FOUND SPACE [%s] -> [%s]", toUTF8(*newText).c_str(), toUTF8(textString).c_str());
            notAnalyzedMatch[matchKeyText] = SmartPtr<wstring>(new wstring(textString)).castDown<EmptyType>();
            
            // Update item rects after possible corrections
            notAnalyzedOCRFragment->adjustRectsInMatch(*notAnalyzedMatch);
        }
    } // iterating over matches
    
    //OCRResults::checkForSpecificMatches(curResults.matchesAddressBook);
    
    // Parse into semantic types
    int currentColumn = 0, currentBlock = 0, currentLine = 0;
    for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        float aveHeightTallLetters = OCRResults::averageHeightTallLetters(d);    // Make that call just to set the property in the match, so that it's available to us later. It's important to make that call before the semantic parsing because that stage messes up to correspondance between text and rect (because of text changes)
        
        // Remember block / line so we can set them into the parse elements. SetBlocks just before should have set these values for all matches, just being defensive.
        if (d.isExists(matchKeyBlock) && d.isExists(matchKeyLine)) {
            int newBlock = getIntFromMatch(d, matchKeyBlock);
            int newLine = getIntFromMatch(d, matchKeyLine);
            if ((newBlock != currentBlock) || (newLine != currentLine)) {
                currentColumn = 0;  // Reset column
            }
            currentBlock = newBlock; currentLine = newLine;
        } else {
            currentBlock = currentLine = currentColumn = 0;
        }
        
        float parentPercentVerticalLineLetters = ((d.isExists(matchKeyPercentVerticalLines))? getFloatFromMatch(d, matchKeyPercentVerticalLines):-1);
        int parentNumNonSpaceLetters = ((d.isExists(matchKeyNumNonSpaceLetters))? getIntFromMatch(d, matchKeyNumNonSpaceLetters):0);
        wstring &currentText = getStringFromMatch(d, matchKeyText);
        wstring currentText2 = wstring();
        if (d.isExists(matchKeyText2))
            currentText2 = getStringFromMatch(d, matchKeyText2);
        RectVectorPtr currentMatchRects = d[matchKeyRects].castDown<RectVector>();
        MatchVectorPtr resFragments = RegexUtilsParseFragmentIntoTypes(currentText, currentText2, &curResults,d);
        // RegexUtilsParseFragmentIntoTypes returns a array of typed elements which replace d
        int numNewMatches = 0;
        for (int k=0; k<resFragments->size(); k++) {
            MatchPtr dd = (*resFragments)[k];
            wstring& newItemText = getStringFromMatch(dd, matchKeyText);
            // Test for empty elements just in case (shouldn't happen I think)
            if (newItemText.length() == 0) {
                DebugLog("adjustTypes: eliminating empty text item at index %d", k);
                resFragments->erase(resFragments->begin() + k);
                k--; // So that k++ keeps us looking at the current element!
                continue;
            }
            
//            lastMatch = this->addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex(matchesAddressBook, newItemText,
//											(gotOrigText? &itemOrigText:NULL), tp,
//                                            notAnalyzedMatchRects, newItemRange.location, newItemRange.location+newItemRange.length-1,
//                                            mergedStatus, notAnalyzedMatchConf, i++);
//                       lastMatch[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(notAnalyzedMatchAverageHeightTallLetters)).castDown<EmptyType>();
            
            // For now simply add each fragment to matchesAddresBook after d (later we will remove d)
            int rectsStartIndex = -1, rectsEndIndex = -1;
            if (dd.isExists(matchKeyRange)) {
                SmartPtr<Range> rangeObject = dd[matchKeyRange].castDown<Range>();
                if (rangeObject != NULL) {
                    Range r = *rangeObject;
                    rectsStartIndex = r.location;
                    rectsEndIndex = r.location + r.length - 1;
                    DebugLog("processBlinkOCRResults: inserting sub-match [%s] type [%s] within [%s] range [%d - %d]", toUTF8(newItemText).c_str(),  OCRResults::stringForOCRElementType((OCRElementType)getIntFromMatch(dd, matchKeyElementType)).c_str(), toUTF8(currentText).c_str(), rectsStartIndex, rectsEndIndex);
                }
            } else {
                DebugLog("processBlinkOCRResults: inserting sub-match [%s] type [%s] within [%s] [error, NO range]", toUTF8(newItemText).c_str(), toUTF8(currentText).c_str(), OCRResults::stringForOCRElementType((OCRElementType)getIntFromMatch(dd, matchKeyElementType)).c_str());
            }
            // Note that to be precise confidence ought to be calculated based on the actual rects within the fragment - instead for now we just inherit the confidence of the parent match from which the fragments were derived
            // addNewFragmentToWithTextAndTypeAndRectsFromIndexToIndexWithStatusConfidenceAtIndex(MatchVector& targetMatches, wstring newText, OCRElementType tp, RectVectorPtr rects, int start, int end, float conf , int index);
            // PQ912 need to add status property + vertical letters stuff
            MatchPtr newMatch = OCRResults::addSubFragmentWithTextAndTypeAndRectsFromIndexToIndexWithConfidenceBlockLineColumnAtIndex(curResults.matchesAddressBook, newItemText, (OCRElementType)getIntFromMatch(dd, matchKeyElementType), currentMatchRects, rectsStartIndex, rectsEndIndex, getFloatFromMatch(d, matchKeyConfidence), currentBlock, currentLine, currentColumn, i + 1 + numNewMatches);
            if (dd.isExists(matchKeyOrigText)) {
                newMatch[matchKeyOrigText] = SmartPtr<wstring>(new wstring(getStringFromMatch(dd, matchKeyOrigText))).castDown<EmptyType>();
            }
            if (dd.isExists(matchKeyText2)) {
                wstring text2 = getStringFromMatch(dd, matchKeyText2);
                if (text2 != newItemText) {
                    newMatch[matchKeyText2] = SmartPtr<wstring>(new wstring(text2)).castDown<EmptyType>();
                    newMatch[matchKeyConfidence2] = SmartPtr<float>(new float(1.00)).castDown<EmptyType>(); // Assume OCR alternate text is low confidence
                    newMatch[matchKeyCountMatched2] = SmartPtr<int>(new int(1.00)).castDown<EmptyType>();
                }
            }
            if (dd.isExists(matchKeyStatus)) {
                unsigned long value = getUnsignedLongFromMatch(dd, matchKeyStatus);
                newMatch[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(value)).castDown<EmptyType>();
            }
            float fragmentPercentVerticalLineLetters = parentPercentVerticalLineLetters;
            int fragmentNumNonSpaceLetters = parentNumNonSpaceLetters;
            if (fragmentNumNonSpaceLetters > 0) {
                // Revert to a count matching length of the fragment, to avoid skewing results
                int currentNonSpaceLetters = countNonSpace(newItemText);
                if (currentNonSpaceLetters == 0) {
                    fragmentPercentVerticalLineLetters = -1;
                    fragmentNumNonSpaceLetters = 0;
                } else if (currentNonSpaceLetters < fragmentNumNonSpaceLetters)
                    fragmentNumNonSpaceLetters = currentNonSpaceLetters;
            } else {
                fragmentPercentVerticalLineLetters = -1;
                fragmentNumNonSpaceLetters = 0;
            }
            newMatch[matchKeyPercentVerticalLines] = SmartPtr<float>(new float(fragmentPercentVerticalLineLetters)).castDown<EmptyType>();
            newMatch[matchKeyNumNonSpaceLetters] = SmartPtr<int>(new int(fragmentNumNonSpaceLetters)).castDown<EmptyType>();
            
            newMatch[matchKeyAverageHeightTallLetters] = SmartPtr<float>(new float(aveHeightTallLetters)).castDown<EmptyType>();
            newMatch[matchKeyFullLineText] = SmartPtr<wstring>(new wstring(currentText)).castDown<EmptyType>();
            numNewMatches++;
        } // for k, processing of all returned typed elements
        // Now delete original match. This will release the rects array, which is OK since the fragments should contain copies of the relevant rects.
        curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
        i += (numNewMatches - 1); // So that we look at the element past all the elements we got in the resFragments array. Substracting 1 because when only one element is returned, we don't want to change the value of i
    } // for i, iterate over all matches to determine semantic types
    
    // Special pass for Walmart coupons where the dash in the price was missed, as in: "COUPON 3800 053800050000 F 1.00-0"
    if (retailerParams.code == RETAILER_WALMART_CODE) {
        for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            if (getElementTypeFromMatch(d, matchKeyElementType) == OCRElementTypeUnknown) {
                // Expecting first element to start with "COUPON", a price on the line and "X0" or "XO" following that
                wstring couponText = getStringFromMatch(d, matchKeyText);
                if ((couponText.length() >= 6) && (couponText.substr(0,6) == L"COUPON")) {
                    int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &curResults.matchesAddressBook, &retailerParams);
                    if ((priceIndex > 0) && (priceIndex + 1 < curResults.matchesAddressBook.size())) {
                        MatchPtr discountPostfix = curResults.matchesAddressBook[priceIndex+1];
                        int lineNumber = getIntFromMatch(d, matchKeyLine);
                        int lineNumberPostFix = getIntFromMatch(discountPostfix, matchKeyLine);
                        if (lineNumber == lineNumberPostFix) {
                            wstring postFixText = getStringFromMatch(discountPostfix, matchKeyText);
                            if ((postFixText.length() >= 1) && ((postFixText[postFixText.length()-1] == '0') || (postFixText[postFixText.length()-1] == 'O'))) {
                                MatchPtr priceItem = curResults.matchesAddressBook[priceIndex];
                                priceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePotentialPriceDiscount)).castDown<EmptyType>();
                                // Delete postFix
                                curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + priceIndex+1);
                                i = priceIndex;
                                continue;
                            }
                        }
                    }
                }
            }
        }
    } // Walmart pass for missed coupons
    
    // Special pass to unmap short products from lines where a date was found, for example Home Depot: 6845 00058 30351 08/17/15 07:10 PM (where 00058 was returned as 000558). To be on the safe side, do this only for products shorter than 8 digits
    if (((curResults.retailerParams.shorterProductNumberLen > 0) && (curResults.retailerParams.shorterProductNumberLen < 8))
        || (curResults.retailerParams.productNumberLen < 8)) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp == OCRElementTypeDate) {
                int firstElementOnLine = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook);
                int lastElementOnLine = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
                // Now examine elements on that line and remove short product number & prices
                for (int j=firstElementOnLine; j<=lastElementOnLine; j++) {
                    if (j == i) continue;
                    MatchPtr dd = curResults.matchesAddressBook[j];
                    OCRElementType tptp = getElementTypeFromMatch(dd, matchKeyElementType);
                    if ((tptp == OCRElementTypeProductNumber) || (tptp == OCRElementTypeIgnoredProductNumber)) {
                        wstring itemText = getStringFromMatch(dd, matchKeyText);
                        if (itemText.length() < 8) {
                            // Untag!
                            LayoutLog("processBlinkOCRResults: unmapping item [%s] or type [%s] at index [%d] - on date line", toUTF8(itemText).c_str(), OCRResults::stringForOCRElementType(tptp).c_str(), j);
                            dd[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                        }
                    }
                }
                i = lastElementOnLine;
            }
        }
    } // special pass to unmap short products from date lines
    
    // Special pass to unmap short products from lines just below the subtotal, see
    if ((curResults.retailerParams.descriptionAboveProduct)
        && (((curResults.retailerParams.shorterProductNumberLen > 0) && (curResults.retailerParams.shorterProductNumberLen < 8))
            || (curResults.retailerParams.productNumberLen < 8))) {
        for (int i=2; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if ((tp != OCRElementTypeProductNumber) && (tp != OCRElementTypeIgnoredProductNumber))
                continue;
            wstring itemText = getStringFromMatch(d, matchKeyText);
            if (itemText.length() >= 8) {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook); continue;
            }
            int indexLineAbove = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook) - 1;
            if (indexLineAbove >= 1) {
                int indexFirstItemOnLineAbove = OCRResults::indexFirstElementOnLineAtIndex(indexLineAbove, &curResults.matchesAddressBook);
                MatchPtr matchFirstItemOnLineAbove = curResults.matchesAddressBook[indexFirstItemOnLineAbove];
                OCRElementType tpFirstItemOnLineAbove = getElementTypeFromMatch(matchFirstItemOnLineAbove, matchKeyElementType);
                int indexPriceOnLineAbove = OCRResults::elementWithTypeInlineAtIndex(indexFirstItemOnLineAbove, OCRElementTypePrice, &curResults.matchesAddressBook, &curResults.retailerParams);
                if (((tpFirstItemOnLineAbove == OCRElementTypeProductDescription) || (tpFirstItemOnLineAbove == OCRElementTypeUnknown)) && (indexPriceOnLineAbove > indexFirstItemOnLineAbove)) {
                    wstring textFirstItemOnLineAbove = getStringFromMatch(matchFirstItemOnLineAbove, matchKeyText);
                    if (testForSubtotal(textFirstItemOnLineAbove, &curResults)) {
                        // Unmap false product
                        LayoutLog("processBlinkOCRResults: unmapping item [%s] of type [%s] at index [%d] - below subtotal", toUTF8(itemText).c_str(), OCRResults::stringForOCRElementType(tp).c_str(), i);
                        d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                        // Tag subtotal
                        matchFirstItemOnLineAbove[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeSubtotal)).castDown<EmptyType>();
                        break;
                    }
                }
            }
            // Move to next line
            i = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            continue;
        }
    } // special pass to unmap short products below SUBTOTAL
    
    // Special pass to map elements of type OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine to OCRElementTypeProductNumber w/special flag. The idea is to let this kind of items be treated everywhere as a product, except when converting into an iOS array when we'll cue on the flag to search for the price on the next line
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if (tp == OCRElementTypeProductNumberWithMarkerOfPriceOnNextLine) {
            int status = 0;
            if (d.isExists(matchKeyStatus))
                status = getIntFromMatch(d, matchKeyStatus);
            status |= matchKeyStatusProductWithPriceOnNextLine;
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(status)).castDown<EmptyType>();
            d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductNumber)).castDown<EmptyType>();
        } else if (tp == OCRElementTypeIgnoredProductNumber) {
            int status = 0;
            if (d.isExists(matchKeyStatus))
                status = getIntFromMatch(d, matchKeyStatus);
            status |= matchKeyStatusNotAProduct;
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(status)).castDown<EmptyType>();
            d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductNumber)).castDown<EmptyType>();
        }
    }
    
    // Special pass to consider upgrading OCRElementTypePotentialPriceDiscount elements to OCRElementTypePriceDiscount
    // At this time isWithinPricesColumn only works when prices are on the same line as product number
    if (curResults.retailerParams.pricesAfterProduct) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp != OCRElementTypePotentialPriceDiscount)
                continue;
            if (OCRResults::isWithinPricesColumn(i, curResults.matchesAddressBook, &curResults)) {
                // Upgrade type!
                d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePriceDiscount)).castDown<EmptyType>();
                continue;
            }
        } // pass to upgrade OCRElementTypePotentialPriceDiscount elements
    }
    
    OCRResults::checkForProductsOnDiscountLines(curResults.matchesAddressBook);
    
    if (curResults.retailerParams.pricesHaveDollar) {
        // Special pass to detect prices where leading $ is possibly missed
        for (int i=0; i<(int)curResults.matchesAddressBook.size() - 1; i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp != OCRElementTypePrice)
                continue;
            if (!d.isExists(matchKeyOrigText)) {
                ErrorLog("processBlinkOCRResults: missing orig text in item [%s] or type [PRICE] at index [%d]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), i);
                continue;
            }
            wstring& itemOrigText = getStringFromMatch(d, matchKeyOrigText);
            if (itemOrigText[0] == L'$')
                continue;
            
            // We only test for preceding dollar sign if original price had two leading digits (in which case we used the first one as a suspected dollar, which may have been wrong)
            if (isDigit(itemOrigText[0]) && isDigit(itemOrigText[1])) {
                // No leading dollar sign! Inspect image left of the range for this price
                // Need to know digit width and average spacing, because that's how we are going to determine where to look
                float digitWidth, digitHeight, spacing;
                bool uniform; int lenghtNormalSequence; float averageHeightDigits = 0;
                bool uniformPriceSequence; int lenghtMaxPriceSequence; float averageHeightPriceSequence = 0;
                float productWidth = 0;
                if (!OCRResults::averageDigitHeightWidthSpacingInMatch(d, digitWidth, digitHeight, spacing, uniform, lenghtNormalSequence, averageHeightDigits, lenghtMaxPriceSequence, uniformPriceSequence, averageHeightPriceSequence, productWidth, &curResults.retailerParams)
                        || (digitWidth <= 0) || (digitHeight <= 0)) {
                    //PQTODO: accept also prices with all 1's
                    ErrorLog("processBlinkOCRResults: couldn't get digits metrics for item [%s] or type [PRICE] at index [%d] w=%.0f,h=%.0f,spacing=%.0f,", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), i, digitWidth, digitHeight, spacing);
                    continue;
                }
                
                if (spacing <= 0)
                    spacing = digitWidth / 5; // Typical: width=14, spacing=3
                // Get left edge of current item as the basis for the rect we want to inspect
                CGRect r = OCRResults::matchRange(d);
                CGRect targetR = r;
                targetR.origin.x = rectLeft(r) - spacing - digitWidth * 1.10;   // Add some extra pixels to make sure to capture the $
                targetR.origin.y -= targetR.size.height * 0.05;
                targetR.size.height *= 1.10;
                targetR.size.width = rectLeft(r) - targetR.origin.x;
                if (OCRUtilsPossibleDollar(targetR, d, &curResults)) {
                    // Found missing dollar! Means we need to add back leading digit to the price (it was stripped as an assumed dollar)
                    wstring& itemText = getStringFromMatch(d, matchKeyText);
                    ReplacingLog("processBlinkOCRResults: adding back leading [%c] to [%s], previously thought to be a $", (char)itemOrigText[0], toUTF8(itemText).c_str());
                    itemText = itemOrigText[0] + itemText;
                    d[matchKeyText] = SmartPtr<wstring>(new wstring(itemText)).castDown<EmptyType>();
                }
            }
        }
    } // prices have dollar
    
    int numProductPrices = 0;
    // Special pass to mark prices which were found on the same line as a product number
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if (tp != OCRElementTypePrice)
            continue;
        
        // Mark whether or not this price appears to be located within the prices column
        int matchStatus = 0;
        if (d.isExists(matchKeyStatus))
            matchStatus = getIntFromMatch(d, matchKeyStatus);
        else
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(0)).castDown<EmptyType>();
#if DEBUG
 //       if (i == 68) {
            wstring txt = getStringFromMatch(d, matchKeyText);
            if (txt == L"113.16") {
                DebugLog("");
            }
//        }
#endif
        // At this time isWithinPricesColumn only works when prices are on the same line as product number
        if (curResults.retailerParams.pricesAfterProduct) {
            if (OCRResults::isWithinPricesColumn(i, curResults.matchesAddressBook, &curResults)) {
                matchStatus |= matchKeyStatusInPricesColumn;
                d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(matchStatus)).castDown<EmptyType>();
            }
        }
        bool lineHasProduct = false;
        if (!d.isExists(matchKeyLine))
            continue;
        int currentLine = getIntFromMatch(d, matchKeyLine);
        if (curResults.retailerParams.pricesAfterProduct) {
            for (int j=i-1; j>=0; j--) {
                MatchPtr dd = curResults.matchesAddressBook[j];
                if (!dd.isExists(matchKeyLine) || getIntFromMatch(dd, matchKeyLine) != currentLine)
                    break;
                OCRElementType tpDD = getElementTypeFromMatch(dd, matchKeyElementType);
                if (tpDD == OCRElementTypeProductNumber) {
                    lineHasProduct = true;
                    break;
                }
            }
        } else {
            for (int j=i+1; j<curResults.matchesAddressBook.size(); j++) {
                MatchPtr dd = curResults.matchesAddressBook[j];
                if (!dd.isExists(matchKeyLine) || getIntFromMatch(dd, matchKeyLine) != currentLine)
                    break;
                OCRElementType tpDD = getElementTypeFromMatch(dd, matchKeyElementType);
                if (tpDD == OCRElementTypeProductNumber) {
                    lineHasProduct = true;
                    break;
                }
            }
        }
        if (lineHasProduct) {
            int status = 0;
            if (d.isExists(matchKeyStatus))
                status = getIntFromMatch(d, matchKeyStatus);
            status |= matchKeyStatusPriceCameWithProduct;
            d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(status)).castDown<EmptyType>();
            numProductPrices++;
            continue;
        }
    } // pass to mark prices that came with products
    
    // Compute the area where product prices were found
    // Compute the area where products were found
    // Compute the area where ANY element with type was found
    // Already calculated area where "normal" elements where found, e.g. "$3.97" but not bogus stuff like "...N"
    float productPricesXMin = 20000, productPricesXMax = -1, productPricesYMin = 20000, productPricesYMax = -1;
    float typedResultsXMin = 20000, typedResultsXMax = -1, typedResultsYMin = 20000, typedResultsYMax = -1;
    float productsXMin = 20000, productsXMax = -1, productsYMin = 20000, productsYMax = -1;
    float productsWithPriceXMin = 20000, productsWithPriceXMax = -1, productsWithPriceYMin = 20000, productsWithPriceYMax = -1;
    int numProductsWithPrice = 0;
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        
        wstring itemText = getStringFromMatch(d, matchKeyText);
//#if DEBUG
//        if (itemText == L"5.38") {
//            DebugLog("");
//        }
//#endif
        // Test if it appears to be a normal "real" text item
        /* Criterias:
            - no two consecutive non-letters, i.e. .. or -. etc
            - at least 4 consecutive letters, i.e. $3.45, ABCD etc
            - digits all similar height, same for uppercase letters
        */
        CGRect currentRect = OCRResults::matchRange(d);
        
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if ((tp == OCRElementTypeUnknown) || (tp == OCRElementTypeNotAnalyzed) || (tp == OCRElementTypeInvalid))
            continue;
        if (tp == OCRElementTypeProductNumber) {
            if (currentRect.origin.x < productsXMin)
                productsXMin = currentRect.origin.x;
            if (currentRect.origin.x + currentRect.size.width - 1 > productsXMax)
                productsXMax = currentRect.origin.x + currentRect.size.width - 1;
            if (currentRect.origin.y < productsYMin)
                productsYMin = currentRect.origin.y;
            if (currentRect.origin.y + currentRect.size.height - 1 > productsYMax)
                productsYMax = currentRect.origin.y + currentRect.size.height - 1;
            int priceIndex = OCRResults::priceIndexForProductAtIndex(i, curResults.matchesAddressBook, NULL, &curResults.retailerParams);
            if (priceIndex >= 0) {
                numProductsWithPrice++;
                if (currentRect.origin.x < productsWithPriceXMin)
                    productsWithPriceXMin = currentRect.origin.x;
                if (currentRect.origin.x + currentRect.size.width - 1 > productsWithPriceXMax)
                    productsWithPriceXMax = currentRect.origin.x + currentRect.size.width - 1;
                if (currentRect.origin.y < productsWithPriceYMin)
                    productsWithPriceYMin = currentRect.origin.y;
                if (currentRect.origin.y + currentRect.size.height - 1 > productsWithPriceYMax)
                    productsWithPriceYMax = currentRect.origin.y + currentRect.size.height - 1;
            }
        }
        // We have some kind of recognized type => update typedResultsRange variables
        if (currentRect.origin.x < typedResultsXMin)
            typedResultsXMin = currentRect.origin.x;
        if (currentRect.origin.x + currentRect.size.width - 1 > typedResultsXMax)
            typedResultsXMax = currentRect.origin.x + currentRect.size.width - 1;
        if (currentRect.origin.y < typedResultsYMin)
            typedResultsYMin = currentRect.origin.y;
        if (currentRect.origin.y + currentRect.size.height - 1 > typedResultsYMax)
            typedResultsYMax = currentRect.origin.y + currentRect.size.height - 1;
        
        if (tp != OCRElementTypePrice)
            continue;
        int status = 0;
        if (d.isExists(matchKeyStatus))
            status = getIntFromMatch(d, matchKeyStatus);
        if (!((status & matchKeyStatusPriceCameWithProduct) && (status & matchKeyStatusInPricesColumn)))
            continue;
        // Update productPricesRange
        if (currentRect.origin.x < productPricesXMin)
            productPricesXMin = currentRect.origin.x;
        if (currentRect.origin.x + currentRect.size.width - 1 > productPricesXMax)
            productPricesXMax = currentRect.origin.x + currentRect.size.width - 1;
        if (currentRect.origin.y < productPricesYMin)
            productPricesYMin = currentRect.origin.y;
        if (currentRect.origin.y + currentRect.size.height - 1 > productPricesYMax)
            productPricesYMax = currentRect.origin.y + currentRect.size.height - 1;
    }
    if (productPricesXMax > 0) {
        curResults.productPricesRange.origin.x = productPricesXMin;
        curResults.productPricesRange.origin.y = productPricesYMin;
        curResults.productPricesRange.size.width = productPricesXMax - productPricesXMin + 1;
        curResults.productPricesRange.size.height = productPricesYMax - productPricesYMin + 1;
        DebugLog("Product prices range [%.0f,%.0f - %.0f,%.0f]", curResults.productPricesRange.origin.x, curResults.productPricesRange.origin.y, curResults.productPricesRange.origin.x + curResults.productPricesRange.size.width - 1, curResults.productPricesRange.origin.y + curResults.productPricesRange.size.height - 1);
    }
    if (typedResultsXMax > 0) {
        curResults.typedResultsRange.origin.x = typedResultsXMin;
        curResults.typedResultsRange.origin.y = typedResultsYMin;
        curResults.typedResultsRange.size.width = typedResultsXMax - typedResultsXMin + 1;
        curResults.typedResultsRange.size.height = typedResultsYMax - typedResultsYMin + 1;
        DebugLog("Typed elements range [%.0f,%.0f - %.0f,%.0f]", curResults.typedResultsRange.origin.x, curResults.typedResultsRange.origin.y, curResults.typedResultsRange.origin.x + curResults.typedResultsRange.size.width - 1, curResults.typedResultsRange.origin.y + curResults.typedResultsRange.size.height - 1);
    }
    if (productsXMax > 0) {
        curResults.productsRange.origin.x = productsXMin;
        curResults.productsRange.origin.y = productPricesYMin;
        curResults.productsRange.size.width = productsXMax - productsXMin + 1;
        curResults.productsRange.size.height = productsYMax - productsYMin + 1;
        DebugLog("Products range [%.0f,%.0f - %.0f,%.0f]", curResults.productsRange.origin.x, curResults.productsRange.origin.y, curResults.productsRange.origin.x + curResults.productsRange.size.width - 1, curResults.productsRange.origin.y + curResults.productsRange.size.height - 1);
    }
    if (productsWithPriceXMax > 0) {
        curResults.productsWithPriceRange.origin.x = productsWithPriceXMin;
        curResults.productsWithPriceRange.origin.y = productsWithPriceYMin;
        curResults.productsWithPriceRange.size.width = productsWithPriceXMax - productsWithPriceXMin + 1;
        curResults.productsWithPriceRange.size.height = productsWithPriceYMax - productsWithPriceYMin + 1;
        curResults.numProductsWithPrice = numProductsWithPrice;
        DebugLog("%d products with price range [%.0f,%.0f - %.0f,%.0f]", curResults.numProductsWithPrice, curResults.productsWithPriceRange.origin.x, curResults.productsWithPriceRange.origin.y, curResults.productsWithPriceRange.origin.x + curResults.productsWithPriceRange.size.width - 1, curResults.productsWithPriceRange.origin.y + curResults.productsWithPriceRange.size.height - 1);
    }
    
    /* Eliminate products way too far in the middle of the productsWithPriceRange (see https://drive.google.com/open?id=0B4jSQhcYsC9VYlctWmhjX3ZQZUU) under two conditions:
        - they have no price
        - or, there is an overwhelming majority of products with prices that say range extends much farther to the left compared with the suspect products
    */
    if (curResults.retailerParams.productAttributes & RETAILER_FLAG_ALIGNED_LEFT) {
        CGRect referenceRect = curResults.productsWithPriceRange;
        if ((referenceRect.size.width <= 0) && (curResults.productWidth > 0) && (curResults.realResultsRange.size.width > curResults.productWidth * 5))
            referenceRect = curResults.realResultsRange;
        if (referenceRect.size.width > 0) {
            int numProductsAlignedLeft = 0, numProductsNotAlignedLeft = 0;
            for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
                MatchPtr d = curResults.matchesAddressBook[i];
                if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeProductNumber) continue;
                CGRect itemRange = OCRResults::matchRange(d);
                float offset = rectLeft(itemRange) - rectLeft(referenceRect);
                // 0.18 comes from a Target receipt where range width = 485 while a product number is barely longer than 90
                if ((offset >= 0) && (offset / referenceRect.size.width < 0.18))
                    numProductsAlignedLeft++;
                else
                    numProductsNotAlignedLeft++;
            }
            if (numProductsNotAlignedLeft > 0) {
                // Now eliminate the wayward products, if we have enough confidence to do that
                for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
                    MatchPtr d = curResults.matchesAddressBook[i];
                    if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeProductNumber) continue;
                    CGRect itemRange = OCRResults::matchRange(d);
                    float offset = rectLeft(itemRange) - rectLeft(referenceRect);
                    // 0.18 comes from a Target receipt where range width = 485 while a product number is barely longer than 90
                    if (offset / referenceRect.size.width > 0.18) {
                        // Eliminate if no price and we have at least 2 aligned products
                        int priceIndex = OCRResults::priceIndexForProductAtIndex(i, curResults.matchesAddressBook, NULL, &curResults.retailerParams);
                        if (((priceIndex < 0) && (numProductsAlignedLeft >= 2))
                            || ((numProductsNotAlignedLeft == 1) && (numProductsAlignedLeft >= 3))
                            || (numProductsAlignedLeft > numProductsNotAlignedLeft * 6)) {
                            DebugLog("processBlinkOCRResults: removing misaligned product [%s] with range [%.0f,%.0f - %.0f,%.0f] outside of product-prices range [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), rectLeft(itemRange), rectBottom(itemRange), rectRight(itemRange), rectTop(itemRange), rectLeft(referenceRect), rectBottom(referenceRect), rectRight(referenceRect), rectTop(referenceRect));
                            curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
                            i--; continue;
                        }
                    }
                }
            }
        }
    } // products aligned left
    
    // Now that we have ranges for products and for product prices, and if retailer is such that products are leftmost and product prices are right-most, eliminate lines where there are no item within the range from product number to product prices, and all items are unknown type
    if ((curResults.productPricesRange.size.width > 0) && (curResults.productsWithPriceRange.size.width > 0)) {
        float widthBasedOnProductsWithPrices = rectRight(curResults.productPricesRange) - rectLeft(curResults.productsWithPriceRange) + 1;
        float ratioWidthToProductWidthBasedOnProductWithPrices = widthBasedOnProductsWithPrices / curResults.productsWithPriceRange.size.width;
        if ((curResults.retailerParams.productPricesRightmost) && (curResults.retailerParams.productNumbersLeftmost) && (curResults.retailerParams.widthToProductWidth > 0) && (ratioWidthToProductWidthBasedOnProductWithPrices > curResults.retailerParams.widthToProductWidth * 0.85)) {
            CGRect validRange = CreateCombinedRect(curResults.productsWithPriceRange, curResults.productPricesRange);
            bool removedItem = false;
            for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
                MatchPtr d = curResults.matchesAddressBook[i];
                if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeUnknown)
                    continue;
                CGRect matchRect = OCRResults::matchRange(d);
                if ((rectRight(matchRect) < rectLeft(validRange)) || (rectLeft(matchRect) > rectRight(validRange))) {
                    // Remove!
                    DebugLog("processBlinkOCRResults: removing noise [%s] with range [%.0f,%.0f - %.0f,%.0f] outside of product-prices range [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), rectLeft(matchRect), rectBottom(matchRect), rectRight(matchRect), rectTop(matchRect), rectLeft(validRange), rectBottom(validRange), rectRight(validRange), rectTop(validRange));
                    curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
                    removedItem = true;
                    i--; continue;
                }
            } // check all matches for possible removal
            if (removedItem) {
                curResults.setBlocks(true);
            }
        }
    }
    
    if (firstFrame || (globalResults.matchesAddressBook.size() == 0)) {
        OCRResults::adjustAllLinesToHighStartIfNeeded(curResults.matchesAddressBook);
    }
    
    // Special pass to mark items that look almost like a complete price (e.g. have a $ + a digit), with a product, and in the right column
    // PQTODO in case we only have digits in the right location, yet not enough to call it a price (after looking for the missing dollar), write code searching for the $. For example if we have 12.9 in the right spot, search for $ left of that
    if (curResults.productPricesRange.size.width > 0) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if (tp != OCRElementTypeUnknown)
                continue;
            
//#if DEBUG
//            wstring tmpText = getStringFromMatch(d, matchKeyText);
//            if (tmpText == L"0. .4") {
//                DebugLog("");
//            }
//#endif
            
            int entirelyContainedOrNoOverlap = 0;
            wstring validText;
            vector<SmartPtr<CGRect> > validRects;
            if (!OCRResults::textAndRectsInRange(d, curResults.productPricesRange, &entirelyContainedOrNoOverlap, true, false, &validRects, &validText))
                continue;
            
            if (entirelyContainedOrNoOverlap == 1)
                validText = getStringFromMatch(d, matchKeyText);
            
            if (validText.length() < 1)
                continue;
            
            // Test that the element *might* be a price
            size_t locDollar = validText.find(L'$');
            int countDigits = false;
            for (int j=0; j<validText.length(); j++) {
                wchar_t ch = validText[j];
                if (isDigit(ch)) {
                    if ((locDollar == wstring::npos) || (j > (int)locDollar))
                        countDigits++;
                }
            }
            
            if (!( ((locDollar != wstring::npos) && (countDigits >=1))
                  || (countDigits >= 2)))
                continue;
            
            // Success! We have text that might be a price. Adjust matches as needed.
            // Are we just updating the current match (e.g. its text was "$1.2" and we think it's a possible price) or truncating existing + adding new match (e.g. its text was "NUTRASWEET N $1.2")
            wstring existingText = getStringFromMatch(d, matchKeyText);
            bool createNewMatch = false;
            if (existingText != validText) {
                // Yep, need to update existing
                int entirelyContainedOrNoOverlap = 0;
                wstring textBefore, textAfter;
                vector<SmartPtr<CGRect> > rectsBefore, rectsAfter;
                if (OCRResults::textAndRectsOutsideRange(d, curResults.productPricesRange, &entirelyContainedOrNoOverlap, true, false, &rectsBefore, &textBefore, &rectsAfter, &textAfter)) {
                    // Ignore text after for now, consider only text before
                    if (textBefore.length() > 0) {
                        // Update existing match
                        d[matchKeyText] = SmartPtr<wstring>(new wstring(textBefore)).castDown<EmptyType>();
                        if (d.isExists(matchKeyOrigText))
                            d->erase(matchKeyOrigText);
                        d[matchKeyRects] = SmartPtr<vector<SmartPtr<CGRect> > > (new vector<SmartPtr<CGRect> >(rectsBefore)).castDown<EmptyType>();
                        createNewMatch = true;
                    }
                }
            }
            
            int matchStatus = 0;
            if (d.isExists(matchKeyStatus))
                matchStatus = getIntFromMatch(d, matchKeyStatus);
            else
                d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(0)).castDown<EmptyType>();
            matchStatus |= matchKeyStatusInPricesColumn;

            // Also make a note if item is on a line that has a product
            bool lineHasProduct = false;
            if (!d.isExists(matchKeyLine))
                continue;
            int currentLine = getIntFromMatch(d, matchKeyLine);
            if (curResults.retailerParams.pricesAfterProduct) {
                for (int j=i-1; j>=0; j--) {
                    MatchPtr dd = curResults.matchesAddressBook[j];
                    if (!dd.isExists(matchKeyLine) || getIntFromMatch(dd, matchKeyLine) != currentLine)
                        break;
                    OCRElementType tpDD = getElementTypeFromMatch(dd, matchKeyElementType);
                    if (tpDD == OCRElementTypeProductNumber) {
                        lineHasProduct = true;
                        break;
                    }
                }
            } else {
                for (int j=i+1; j<curResults.matchesAddressBook.size(); j++) {
                    MatchPtr dd = curResults.matchesAddressBook[j];
                    if (!dd.isExists(matchKeyLine) || getIntFromMatch(dd, matchKeyLine) != currentLine)
                        break;
                    OCRElementType tpDD = getElementTypeFromMatch(dd, matchKeyElementType);
                    if (tpDD == OCRElementTypeProductNumber) {
                        lineHasProduct = true;
                        break;
                    }
                }
            }
            if (lineHasProduct)
                matchStatus |= matchKeyStatusPriceCameWithProduct;
            // Finally, update existing match or create a new one
            if (createNewMatch) {
                LayoutLog("Split match [%s] into [%s] + partial price [%s] at line %d", toUTF8(existingText).c_str(), toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), toUTF8(validText).c_str(), currentLine);
                MatchPtr newM = OCRResults::copyMatch(d);
                // Inherits block, column (we don't use column so no need to increment it), line, confidence
                // PQTODO preserve original OCR confidence values and use to calculate the exact values for the price sub-range
                newM[matchKeyText] = SmartPtr<wstring> (new wstring(validText)).castDown<EmptyType>();
                newM[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePartialProductPrice)).castDown<EmptyType>();
                newM[matchKeyStatus] = SmartPtr<unsigned long> (new unsigned long(matchStatus)).castDown<EmptyType>();
                newM[matchKeyN] = SmartPtr<int> (new int((int)validText.length())).castDown<EmptyType>();
                //newM[matchKeyRects] = validRects.castDown<EmptyType>();
                newM[matchKeyRects] = SmartPtr<vector<SmartPtr<CGRect> > > (new vector<SmartPtr<CGRect> >(validRects)).castDown<EmptyType>();
                curResults.matchesAddressBook.insert(curResults.matchesAddressBook.begin() + i + 1, newM);
                OCRResults::checkPartialPrice (newM, &curResults); // NOTE: will adjust text and upgrade to OCRElementPrice as needed
                i++; // To skip the newly added match, loop will also increment
            } else {
                LayoutLog("Tagged match [%s] as partial price at line %d", toUTF8(validText).c_str(), currentLine);
                d[matchKeyText] = SmartPtr<wstring> (new wstring(validText)).castDown<EmptyType>();
                d[matchKeyElementType] = SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePartialProductPrice)).castDown<EmptyType>();
                d[matchKeyStatus] = SmartPtr<unsigned long> (new unsigned long(matchStatus)).castDown<EmptyType>();
                int tmpN = (int)validText.length();
                d[matchKeyN] = SmartPtr<int> (new int(tmpN)).castDown<EmptyType>();
                if (d.isExists(matchKeyOrigText))
                    d->erase(matchKeyOrigText);
                // Update rects only if we carved something out of another item, not if entire item was within prices range
                if (!entirelyContainedOrNoOverlap) {
                    d[matchKeyRects] = SmartPtr<vector<SmartPtr<CGRect> > > (new vector<SmartPtr<CGRect> >(validRects)).castDown<EmptyType>();
                }
                OCRResults::checkPartialPrice (d, &curResults); // NOTE: will adjust text and upgrade to OCRElementPrice as needed
            }
            
        } // pass to mark possible prices
    } // got prices range
    
    // Special pass to mark prices located within the products area (suspect). We then check that flag in priceIndexForProduct to disqualify these from getting associated with a product.
    if ((curResults.productsWithPriceRange.size.width > 0) && (retailerParams.productRangeWidthDividedByLetterHeight > 0)) {
        float actualRatioProductAreaWidthDividedByLetterHeight = (float)curResults.productsWithPriceRange.size.width / curResults.globalStats.averageHeight.average;
        LayoutLog("productsRange.size.width=%.2f, ratio=%.2f, retailerParams.productRangeWidthDividedByLetterHeight=%.2f", (float)curResults.productsWithPriceRange.size.width, actualRatioProductAreaWidthDividedByLetterHeight, retailerParams.productRangeWidthDividedByLetterHeight);
        if ((actualRatioProductAreaWidthDividedByLetterHeight > retailerParams.productRangeWidthDividedByLetterHeight * 0.80) && (actualRatioProductAreaWidthDividedByLetterHeight < retailerParams.productRangeWidthDividedByLetterHeight * 1.20)) {
            for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
                MatchPtr d = curResults.matchesAddressBook[i];
                OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
                if ((tp != OCRElementTypePrice) && (tp != OCRElementTypePriceDiscount))
                    continue;
                CGRect matchRange = OCRResults::matchRange(d);
                if (OCRUtilsOverlappingX(matchRange, curResults.productsWithPriceRange)) {
                    int status = 0;
                    if (d.isExists(matchKeyStatus))
                        status = getIntFromMatch(d, matchKeyStatus);
                    d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(status | matchKeyStatusInPricesColumn)).castDown<EmptyType>();
                } else if (rectRight(matchRange) < rectLeft(curResults.productsWithPriceRange)) {
                    int status = 0;
                    if (d.isExists(matchKeyStatus))
                        status = getIntFromMatch(d, matchKeyStatus);
                    d[matchKeyStatus] = SmartPtr<unsigned long>(new unsigned long(status | matchKeyStatusBeforeProductsColumn)).castDown<EmptyType>();
                }
            }
        } // Ratio makes sense
    }
    
    // Special pass for retailers with products aligned left, to eliminate products tagged located to the right of that range, e.g. phone number on top of Bestbuy receipts. The way this pass works is by comparing with the range of products WITH a price (if that range exists): the hope is that the bogus products to be unmapped are without a price (hence didn't contribute to the range, and will be outside of it)
    // TODO this only unmaps bad products if real products were found on the same frame. Need to try to also do that when a bogus product is found on its own in a frame without products/prices. This requires having some kind of unified coordinates system across frames (not trivial)
    if ((retailerParams.productAttributes & RETAILER_FLAG_ALIGNED_LEFT) && (curResults.productsWithPriceRange.size.width > 0) && (curResults.productDigitWidth > 0)) {
        for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            if (getElementTypeFromMatch(d, matchKeyElementType) == OCRElementTypeProductNumber) {
                CGRect range = OCRResults::matchRange(d);
#if DEBUG
                if (getStringFromMatch(d, matchKeyText) == L"061826") {
                    DebugLog("");
                }
#endif
                if ((rectLeft(range) - rectLeft(curResults.productsWithPriceRange)) / curResults.productDigitWidth > 5) {
                    // One more check: verify that the space to the end of the price of that product is also too narrow
                    bool doit = true;
                    if (curResults.retailerParams.productLeftSideToPriceRightSideNumDigits > 0) {
                        int lineNumber;
                        int priceIndex = OCRResults::priceIndexForProductAtIndex(i, curResults.matchesAddressBook, &lineNumber, &curResults.retailerParams);
                        if (priceIndex >= 0) {
                            MatchPtr priceItem = curResults.matchesAddressBook[priceIndex];
                            CGRect priceRange = OCRResults::matchRange(priceItem);
                            if ((rectRight(priceRange) - rectLeft(range)) / curResults.productDigitWidth > curResults.retailerParams.productLeftSideToPriceRightSideNumDigits * 0.85) {
                                doit = false;   // Abort, seems like there is a wide enough space between product and price!
                            }
                        }
                    }
                    if (doit) {
                        // Undo type!
                        WarningLog("processBlinkOCRResults: un-mapping bad product [%s] because of position (too far right) at index [%d]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), i);
                        d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                    }
                } else {
                    i = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
                    continue;
                }
            }
        }
    }
    
    // Pass to set product descriptions
    int numProductNumbers = 0, numTypedItems = 0;
    int numProductPricesWithDescription = 0;    // This variable is only used when retailer has no product numbers, i.e. a product is defined as a product price with a description before it.
    vector<matchingList> alignedGroups;  // To identify outliers later
    // Whatever is unknown and squeezed between product number and price must be a product description
    // Whatever text is found between product number and price must be a product description (e.g. for Target), or description is above product number and to the left of prices (ToysRUs)
    // Also: this pass will make a note of whether or not we found a product number
    for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if ((tp != OCRElementTypeUnknown) && (tp != OCRElementTypeInvalid) && (tp != OCRElementTypeNotAnalyzed))
            numTypedItems++;
        int currentLine = getIntFromMatch(d, matchKeyLine);
        if ((curResults.retailerParams.noProductNumbers) && (curResults.retailerParams.descriptionBeforePrice)) {
            // No product numbers! Must derive descriptions anyhow
                // Accept only OCRElementTypeProductPrice if product prices have an extra char (which tags them as OCRElementTypeProductPrice), e.g. CVS
            if ((tp != OCRElementTypeProductPrice)
                // Or, just look for prices for retailers where product prices don't have such an extra char (like Trader Joe's)
                && (curResults.retailerParams.extraCharsAfterPricesMeansProductPrice || (tp != OCRElementTypePrice)))
                continue;
            int indexFirstItemOnCurrentLine = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            int indexLastItemOnCurrentLine = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            if (curResults.retailerParams.descriptionBeforePrice) {
                int priceItemIndex = -1; // Until proven otherwise we do not have a valid candidate
                int descriptionIndex = -1;
                if (curResults.retailerParams.descriptionStartToPriceEnd > 0) {
                    // If we have this metric, select the first price on current line that is at the right distance from the description just before it
                    for (int j=i; j <= indexLastItemOnCurrentLine; j++) {
                        if (i == indexFirstItemOnCurrentLine)
                            continue;   // Must have a description on the same line before, so this price must be a spurious price found at the start of the description, or this is not a product line at all
                        MatchPtr priceItem = curResults.matchesAddressBook[j];
                        OCRElementType priceType = getElementTypeFromMatch(priceItem, matchKeyElementType);
                        if ((priceType != OCRElementTypeProductPrice)
                            && (curResults.retailerParams.extraCharsAfterPricesMeansProductPrice || (priceType != OCRElementTypePrice)))
                            continue;
                        int tmpDescriptionIndex = j-1;
                        MatchPtr tmpDescriptionItem = curResults.matchesAddressBook[tmpDescriptionIndex];
                        OCRElementType tmpDescriptionType = getElementTypeFromMatch(tmpDescriptionItem, matchKeyElementType);
                        CGRect descriptionRange = OCRResults::matchRange(tmpDescriptionItem);
                        CGRect priceRange = OCRResults::matchRange(priceItem);
                        float descriptionLeftToPriceEnd = (rectRight(priceRange) - rectLeft(descriptionRange) + 1) / curResults.globalStats.averageWidthDigits.average;
                        if ((tmpDescriptionType != OCRElementTypeUnknown) || (descriptionLeftToPriceEnd < curResults.retailerParams.descriptionStartToPriceEnd * 0.90)) {
                            // Description too close, see if it was a spurious price tagged incorrectly, or perhaps a description broken into several items. Go left to try to find a better description start
                            for (int k=tmpDescriptionIndex-1; k>=indexFirstItemOnCurrentLine; k--) {
                                MatchPtr alternateDescriptionItem = curResults.matchesAddressBook[k];
                                OCRElementType alternateDescriptionType = getElementTypeFromMatch(alternateDescriptionItem, matchKeyElementType);
                                CGRect alternateDescriptionRange = OCRResults::matchRange(alternateDescriptionItem);
                                float alternateDescriptionLeftToPriceEnd = (rectRight(priceRange) - rectLeft(alternateDescriptionRange) + 1) / curResults.globalStats.averageWidthDigits.average;
                                if ((alternateDescriptionType == OCRElementTypeUnknown) && (alternateDescriptionLeftToPriceEnd < curResults.retailerParams.descriptionStartToPriceEnd * 1.10)) {
                                    // That's better (but keep going in case desc was broken into more than 2 pieces)
                                    tmpDescriptionIndex = k;
                                    tmpDescriptionType = alternateDescriptionType;
                                    descriptionRange = alternateDescriptionRange;
                                    descriptionLeftToPriceEnd = alternateDescriptionLeftToPriceEnd;
                                    continue;
                                } else
                                    break; // We went too far left
                            }
                        }
                        if (tmpDescriptionType != OCRElementTypeUnknown) {
                            WarningLog("processBlinkOCRResults: rejecting description [%s] for price [%s], not unknown type, ranges [%.0f,%.0f - %.0f,%.0f] - [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(curResults.matchesAddressBook[tmpDescriptionIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(priceItem, matchKeyText)).c_str(), rectLeft(descriptionRange), rectBottom(descriptionRange), rectRight(descriptionRange), rectTop(descriptionRange), rectLeft(priceRange), rectBottom(priceRange), rectRight(priceRange), rectTop(priceRange));
                            continue;
                        }
                        if (descriptionLeftToPriceEnd < curResults.retailerParams.descriptionStartToPriceEnd * 0.90) {
                            // Too close even after we tried to go left - skip this price
                            WarningLog("processBlinkOCRResults: rejecting description [%s] for price [%s], too close, ranges [%.0f,%.0f - %.0f,%.0f] - [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(curResults.matchesAddressBook[tmpDescriptionIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(priceItem, matchKeyText)).c_str(), rectLeft(descriptionRange), rectBottom(descriptionRange), rectRight(descriptionRange), rectTop(descriptionRange), rectLeft(priceRange), rectBottom(priceRange), rectRight(priceRange), rectTop(priceRange));
                            continue;
                        }
                        if (descriptionLeftToPriceEnd >= curResults.retailerParams.descriptionStartToPriceEnd * 1.10) {
                            // We went to far right, no hope of finding suitable price candidates beyond that
                            WarningLog("processBlinkOCRResults: rejecting description [%s] for price [%s], too far, ranges [%.0f,%.0f - %.0f,%.0f] - [%.0f,%.0f - %.0f,%.0f]", toUTF8(getStringFromMatch(curResults.matchesAddressBook[tmpDescriptionIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(priceItem, matchKeyText)).c_str(), rectLeft(descriptionRange), rectBottom(descriptionRange), rectRight(descriptionRange), rectTop(descriptionRange), rectLeft(priceRange), rectBottom(priceRange), rectRight(priceRange), rectTop(priceRange));
                            break;
                        }
                        if ((descriptionLeftToPriceEnd < curResults.retailerParams.descriptionStartToPriceEnd * 1.10)
                            && (descriptionLeftToPriceEnd > curResults.retailerParams.descriptionStartToPriceEnd * 0.90)) {
                            // Found a valid candidate. No need to keep going, to avoid tagging extra text after the price + an extra price following, e.g. "1 OB GLIDE CMFT FLS 43.7 2.64T SWED 50X 2.65"
                            priceItemIndex = j;
                            descriptionIndex = tmpDescriptionIndex; // Remember that index since it is not always priceItemIndex-1
                            // One last check: is this the subtotal line???
                            if ((curResults.retailerParams.subtotalLineExcessWidthDigits > 3) && (descriptionLeftToPriceEnd - curResults.retailerParams.descriptionStartToPriceEnd > curResults.retailerParams.subtotalLineExcessWidthDigits  * 0.85)) {
                                LayoutLog("processBlinkOCRResults: rejecting description [%s] for price [%s], suspected SUBTOTAL!", toUTF8(getStringFromMatch(curResults.matchesAddressBook[tmpDescriptionIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(curResults.matchesAddressBook[priceItemIndex], matchKeyText)).c_str());
                                i = indexLastItemOnCurrentLine; continue;
                            }
                            LayoutLog("processBlinkOCRResults: tagging description [%s] for price [%s]", toUTF8(getStringFromMatch(curResults.matchesAddressBook[tmpDescriptionIndex], matchKeyText)).c_str(), toUTF8(getStringFromMatch(curResults.matchesAddressBook[priceItemIndex], matchKeyText)).c_str());
                            break;
                        }
                    } // Find a price on this line with a suitably distanced description
                } // Got metrics about description - price distance
                else {
                    // Ignore spurious prices tagged within the description by picking the rightmost price on this line
                    priceItemIndex = i;
                    for (int j=i+1; j <= indexLastItemOnCurrentLine; j++) {
                        MatchPtr alternativePriceItem = curResults.matchesAddressBook[j];
                        OCRElementType alternativePriceType = getElementTypeFromMatch(alternativePriceItem, matchKeyElementType);
                        if ((alternativePriceType == OCRElementTypeProductPrice) || (!curResults.retailerParams.extraCharsAfterPricesMeansProductPrice && (alternativePriceType == OCRElementTypePrice)))
                            priceItemIndex = j;
                    }
                    if (curResults.retailerParams.descriptionAlignedLeft)
                        descriptionIndex = indexFirstItemOnCurrentLine;
                    else
                        descriptionIndex = priceItemIndex - 1;
                }
                
                if (descriptionIndex < 0) {
                    // Skip this line
                    i = indexLastItemOnCurrentLine; continue;
                }
                MatchPtr descriptionItem = curResults.matchesAddressBook[descriptionIndex];

                // That's it, tag as the description!
                // Save previous type in case we need to undo
                descriptionItem[matchKeyElementOrigType] = SmartPtr<OCRElementType>(new OCRElementType(getElementTypeFromMatch(descriptionItem, matchKeyElementType))).castDown<EmptyType>();
                descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductDescription)).castDown<EmptyType>();
                wstring itemText = getStringFromMatch(descriptionItem, matchKeyText);
//#if DEBUG
//                if (itemText == L"19.28") {
//                    DebugLog("Found it!")
//                }
//#endif
                wstring formattedText = RegexUtilsBeautifyProductDescription(itemText, &curResults);
                if (formattedText != itemText) {
                    if (!descriptionItem.isExists(matchKeyOrigText)) {
                        descriptionItem[matchKeyOrigText] = SmartPtr<wstring>(new wstring(itemText)).castDown<EmptyType>();
                    }
                    descriptionItem[matchKeyText] = SmartPtr<wstring>(new wstring(formattedText)).castDown<EmptyType>();
                }
                numProductPricesWithDescription++;
                
                // If retailer doesn't set OCRElementTypeProductPrice by detecting an extra char, set it now that we selected a description for it!
                MatchPtr priceItem = curResults.matchesAddressBook[priceItemIndex];
                priceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductPrice)).castDown<EmptyType>();
                
                // Untag any product price after priceItemIndex on current line (there is a reason we rejected them!)
                for (int j=priceItemIndex+1; j<=indexLastItemOnCurrentLine; j++) {
                    MatchPtr extraItem = curResults.matchesAddressBook[j];
                    if (getElementTypeFromMatch(extraItem, matchKeyElementType) != OCRElementTypeProductPrice) continue;
                    extraItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
                }
                
                // Undo possible tagging of prices within descriptions (see https://drive.google.com/open?id=0B4jSQhcYsC9VU1U3OG1EY0V0NlE). We know when to do it by the fact that description index is less than priceItemIndex - 1
                if (descriptionIndex < priceItemIndex - 1) {
                    // Need to append price(s) back to the description text
                    wstring newText = getStringFromMatch(descriptionItem, matchKeyText);
                    RegexUtilsStripTrailing(newText, L" ");
                    for (int j=descriptionIndex+1; j<priceItemIndex; j++) {
                        MatchPtr itemToUndo = curResults.matchesAddressBook[descriptionIndex+1]; // Use this fixed index because we keep deleting the current item
                        wstring extraText;
                        if (itemToUndo.isExists(matchKeyOrigText))
                            extraText = getStringFromMatch(itemToUndo, matchKeyOrigText);
                        else
                            extraText = getStringFromMatch(itemToUndo, matchKeyText);
                        RegexUtilsStripLeading(extraText, L" ");
                        newText += L" " + extraText;
                        curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + descriptionIndex + 1);
                        indexLastItemOnCurrentLine--;
                        priceItemIndex--;
                    }
                    LayoutLog("processBlinkOCRResults: fixed description back to [%s]", toUTF8(newText).c_str());
                    wstring formattedText = RegexUtilsBeautifyProductDescription(newText, &curResults);
                    if (formattedText != itemText) {
                        if (!descriptionItem.isExists(matchKeyOrigText)) {
                            descriptionItem[matchKeyOrigText] = SmartPtr<wstring>(new wstring(newText)).castDown<EmptyType>();
                        }
                    }
                    descriptionItem[matchKeyText] = SmartPtr<wstring>(new wstring(formattedText)).castDown<EmptyType>();
                }
                // Make note of it (to spot outliers later)
                CGRect productPriceRect = OCRResults::matchRange(curResults.matchesAddressBook[priceItemIndex]);
                int productPriceEndPosition = rectRight(productPriceRect) / curResults.globalStats.averageWidthDigits.average;
                matchingItem p;
                p.index = priceItemIndex;
                p.index1 = descriptionIndex;
                p.floatValue = productPriceEndPosition;
                CGRect descriptionRange = OCRResults::matchRange(descriptionItem);
                p.r = CreateCombinedRect(descriptionRange, productPriceRect);
                p.text = getStringFromMatch(curResults.matchesAddressBook[priceItemIndex], matchKeyText);
                // Do we have a list whose average is 3 digits width away from us?
                bool addedToList = false;
                for (int kk=0; kk<alignedGroups.size(); kk++) {
                    matchingList l = alignedGroups[kk];
                    int delta = abs(l.average - p.floatValue);
                    if (delta <= 3) {
                        // Add to this list
                        l.list.push_back(p);
                        l.average = (l.average * l.count + p.floatValue) / ++l.count;
                        addedToList = true;
                        alignedGroups.erase(alignedGroups.begin() + kk);
                        alignedGroups.insert(alignedGroups.begin() + kk, l);
                        break;
                    }
                }
                if (!addedToList) {
                    matchingList newL;
                    newL.list.push_back(p);
                    newL.average = p.floatValue;
                    newL.count = 1;
                    alignedGroups.push_back(newL);
                }
            }
            // Skip to next line
            i = indexLastItemOnCurrentLine; continue;
        }
        
        if (tp != OCRElementTypeProductNumber)
            continue;
        MatchPtr productItem = d;
        numProductNumbers++;
        if ((curResults.retailerParams.priceAboveProduct) && (curResults.retailerParams.descriptionBeforePrice)) {
            // Description above product and to the left of the price
            int indexStartCurrentLine = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            if (indexStartCurrentLine > 0) {
                int priceIndex = OCRResults::elementWithTypeInlineAtIndex(indexStartCurrentLine-1, OCRElementTypePrice, &curResults.matchesAddressBook, NULL);
                if (priceIndex > 0) {
                    // Description is the element just before the price - if on the same line!
                    MatchPtr priceItem = curResults.matchesAddressBook[priceIndex];
                    int priceLine = getIntFromMatch(priceItem, matchKeyLine);
                    int descriptionIndex = priceIndex - 1;
                    MatchPtr descriptionItem = curResults.matchesAddressBook[descriptionIndex];
                    // Make sure there isn't a better unknown item to become the description as in Macy's or Staples or Best Buy receipts:
                    //  e.g. DRESS SHOES          #N#  100.00
                    if ((curResults.productsWithPriceRange.size.width > 0) && (curResults.retailerParams.descriptionAlignedWithProduct)) {
                        int indexStartPriceLine = OCRResults::indexFirstElementOnLineAtIndex(priceIndex, &curResults.matchesAddressBook);
                        CGRect descriptionRange = OCRResults::matchRange(descriptionItem);
                        float bestDelta = abs(rectLeft(descriptionRange) - rectLeft(curResults.productsWithPriceRange));
                        for (int k=indexStartPriceLine; k<descriptionIndex; k++) {
                            MatchPtr alternateDescriptionItem = curResults.matchesAddressBook[k];
                            CGRect alternateDescriptionRange = OCRResults::matchRange(alternateDescriptionItem);
                            float newDelta = abs(rectLeft(alternateDescriptionRange) - rectLeft(curResults.productsWithPriceRange));
                            if (newDelta < bestDelta) {
                                bestDelta = newDelta;
                                descriptionIndex = k;
                                descriptionItem = alternateDescriptionItem;
                            }
                        }
                    }
                    if (getElementTypeFromMatch(descriptionItem, matchKeyElementType) == OCRElementTypeUnknown) {
                        int descriptionLine = getIntFromMatch(descriptionItem, matchKeyLine);
                        if (descriptionLine == priceLine) {
                            wstring& nextText = getStringFromMatch(descriptionItem, matchKeyText);
                            if (RegexUtilsAutoCorrect(nextText, &curResults, OCRElementTypeProductDescription, OCRElementTypeUnknown, true, true)) {
                                descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductDescription)).castDown<EmptyType>();
                                descriptionItem[matchKeyText] = SmartPtr<wstring>(new wstring(nextText)).castDown<EmptyType>();
                            }
                        }
                    }
                }
            }
        } else if ((curResults.retailerParams.descriptionAboveProduct || curResults.retailerParams.descriptionBelowProduct) && !curResults.retailerParams.descriptionBeforePrice) {
            int indexDescriptionLine = -1;
            int deltaFromProductLine = 0;
            if (curResults.retailerParams.descriptionAboveProduct) {
                // Description above product on its own
                int indexStartCurrentLine = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook);
                indexDescriptionLine = indexStartCurrentLine - 1;
                deltaFromProductLine = -1;
            } else {
                // Description below product on its own
                int indexEndCurrentLine = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
                indexDescriptionLine = indexEndCurrentLine + 1;
                deltaFromProductLine = 1;
            }
            // Description is not always on its own, see https://drive.google.com/open?id=0B4jSQhcYsC9VYXlnM2VrQThBZnM
            if ((indexDescriptionLine > 0) && (indexDescriptionLine < ((int)curResults.matchesAddressBook.size()-1))) {
                if (curResults.retailerParams.descriptionAlignedWithProduct || curResults.retailerParams.descriptionOverlapsWithProduct) {
                    int indexStartDescriptionLine = OCRResults::indexFirstElementOnLineAtIndex(indexDescriptionLine, &curResults.matchesAddressBook);
                    int indexEndDescriptionLine = OCRResults::indexLastElementOnLineAtIndex(indexDescriptionLine, &curResults.matchesAddressBook);
                    int descriptionLine = getIntFromMatch(curResults.matchesAddressBook[indexStartDescriptionLine], matchKeyLine);
                    if (descriptionLine == currentLine + deltaFromProductLine) {
                        CGRect productRange = OCRResults::matchRange(d);
                        int bestDescriptionIndex = -1;
                        float bestOverlap = 0;
                        for (int j=indexStartDescriptionLine; j<=indexEndDescriptionLine; j++) {
                            MatchPtr dd = curResults.matchesAddressBook[j];
                            if (getElementTypeFromMatch(dd, matchKeyElementType) != OCRElementTypeUnknown)
                                continue;
                            CGRect newRange = OCRResults::matchRange(dd);
                            float overlap = OCRResults::overlapAmountRectsXAxisAnd(productRange, newRange);
                            if (overlap > bestOverlap) {
                                bestDescriptionIndex = j;
                                bestOverlap = overlap;
                            }
                        }
                        if (bestDescriptionIndex >= 0) {
                            MatchPtr descriptionItem = curResults.matchesAddressBook[bestDescriptionIndex];
                            wstring& nextText = getStringFromMatch(descriptionItem, matchKeyText);
                            if (RegexUtilsAutoCorrect(nextText, &curResults, OCRElementTypeProductDescription, OCRElementTypeUnknown, true, true)) {
                                descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductDescription)).castDown<EmptyType>();
                                descriptionItem[matchKeyText] = SmartPtr<wstring>(new wstring(nextText)).castDown<EmptyType>();
                            }
                        }
                    }
                } else {
                    int descriptionIndex = indexDescriptionLine;
                    MatchPtr descriptionItem = curResults.matchesAddressBook[descriptionIndex];
                    if (getElementTypeFromMatch(descriptionItem, matchKeyElementType) == OCRElementTypeUnknown) {
                        int descriptionLine = getIntFromMatch(descriptionItem, matchKeyLine);
                        if (descriptionLine == currentLine + deltaFromProductLine) {
                            wstring& nextText = getStringFromMatch(descriptionItem, matchKeyText);
                            if (RegexUtilsAutoCorrect(nextText, &curResults, OCRElementTypeProductDescription, OCRElementTypeUnknown, true, true)) {
                                descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductDescription)).castDown<EmptyType>();
                                descriptionItem[matchKeyText] = SmartPtr<wstring>(new wstring(nextText)).castDown<EmptyType>();
                            }
                        }
                    }
                }
            }
        } else {
            // If the next element is on the same line and is not a price, take it as product description!
            int currentBlock = getIntFromMatch(d, matchKeyBlock);
            MatchPtr descD;
            if ((curResults.retailerParams.pricesAfterProduct) && (curResults.retailerParams.descriptionBeforePrice)) {
                if (i < (int)curResults.matchesAddressBook.size() - 1)
                    descD = curResults.matchesAddressBook[i+1];
                else
                    continue;
            } else if (curResults.retailerParams.descriptionBeforeProduct) {
                if (i > 0)
                    descD = curResults.matchesAddressBook[i-1];
                else
                    continue;
            } else
                continue;
            if ((getElementTypeFromMatch(descD, matchKeyElementType) == OCRElementTypeUnknown)
                && (getElementTypeFromMatch(descD, matchKeyBlock) == currentBlock)
                && (getElementTypeFromMatch(descD, matchKeyLine) == currentLine)) {
                wstring& nextText = getStringFromMatch(descD, matchKeyText);
                if (RegexUtilsAutoCorrect(nextText, &curResults, OCRElementTypeProductDescription, OCRElementTypeUnknown, true, true)) {
                    descD[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductDescription)).castDown<EmptyType>();
                    descD[matchKeyText] = SmartPtr<wstring>(new wstring(nextText)).castDown<EmptyType>();
                    if (descD.isExists(matchKeyText2)) {
                        wstring& nextText2 = getStringFromMatch(descD, matchKeyText2);
                        RegexUtilsAutoCorrect(nextText2, &curResults, OCRElementTypeProductDescription, OCRElementTypeUnknown, true, true);
                        descD[matchKeyText2] = SmartPtr<wstring>(new wstring(nextText2)).castDown<EmptyType>();
                    }
                }
                i++; continue;
            }
        } // Normal case, desc on same line as the product
    }
    
    // PQ911
    // When product prices have no $ signs and we have  group of lines after a blank line + having $ in the price => subtotal / total group, untag all products after these lines
    if (!curResults.retailerParams.pricesHaveDollar) {
        for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            if ((tp != OCRElementTypeProductPrice) && (tp != OCRElementTypePrice))
                continue;

            // Does this price have a $?
            unsigned long status = 0;
            if (d.isExists(matchKeyStatus))
                status = getUnsignedLongFromMatch(d, matchKeyStatus);
            
            if (!(status & matchKeyStatusHasDollar))
                continue;
            CGRect topDollarRange = OCRResults::matchRange(d);
            
            // TODO: check that the price is aligned right, to avoid latching onto a price in the middle of something like "You saved $2.50, regular price $3.40"
            
            // Now find one more price with a dollar in the next 2 lines with X coordinates overlapping with topDollarRange
            bool foundDollarPricesCluster = false;
            int topDollarLine = getIntFromMatch(d, matchKeyLine);
            int j = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook) + 1;
            for (; j<(int)curResults.matchesAddressBook.size(); j++) {
                MatchPtr dd = curResults.matchesAddressBook[j];
                int currentLine = getIntFromMatch(d, matchKeyLine);
                if (currentLine > topDollarLine + 2)
                    break;
                int indexPrice = OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypeProductPrice, &curResults.matchesAddressBook, &curResults.retailerParams);
                if (indexPrice == -1) {
                    indexPrice = OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypePrice, &curResults.matchesAddressBook, &curResults.retailerParams);
                }
                if (indexPrice != -1) {
                    MatchPtr otherPriceElement = curResults.matchesAddressBook[indexPrice];
                    // Does this price have a $?
                    unsigned long status = 0;
                    if (otherPriceElement.isExists(matchKeyStatus))
                        status = getUnsignedLongFromMatch(otherPriceElement, matchKeyStatus);
                    CGRect otherPriceRange = OCRResults::matchRange(otherPriceElement);
            
                    if ((status & matchKeyStatusHasDollar) && OCRUtilsOverlappingX(otherPriceRange, topDollarRange)) {
                        foundDollarPricesCluster = true; break;
                    }
                }
            } // search for nearby other price with dollar
            if (foundDollarPricesCluster) {
                // Untag all products below line at index i (including that line)
                for (int j = i; j < (int)curResults.matchesAddressBook.size(); j++) {
                    if (!curResults.retailerParams.noProductNumbers) {
                        // Untag OCRElementTypeProductNumber element (if any on this line)
                        int indexProduct = OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypeProductNumber, &curResults.matchesAddressBook, &curResults.retailerParams);
                        if (indexProduct != -1) {
                            MatchPtr productItem = curResults.matchesAddressBook[indexProduct];
                            LayoutLog("processBlinkOCRResults: untagging product [%s], below subtotal group", toUTF8(getStringFromMatch(productItem, matchKeyText)).c_str());
                            productItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                        }
                    } else {
                        // Untag OCRElementTypeProductDescription and downgrade product price to price
                        int indexDescription = OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypeProductDescription, &curResults.matchesAddressBook, &curResults.retailerParams);
                        if (indexDescription != -1) {
                            MatchPtr descriptionItem = curResults.matchesAddressBook[indexDescription];
                            LayoutLog("processBlinkOCRResults: untagging description [%s], below subtotal group", toUTF8(getStringFromMatch(descriptionItem, matchKeyText)).c_str());
                            descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                        }
                        int indexProductPrice = OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypeProductPrice, &curResults.matchesAddressBook, &curResults.retailerParams);
                        if (indexProductPrice != -1) {
                            MatchPtr productPriceItem = curResults.matchesAddressBook[indexProductPrice];
                            LayoutLog("processBlinkOCRResults: untagging product price [%s], below subtotal group", toUTF8(getStringFromMatch(productPriceItem, matchKeyText)).c_str());
                            productPriceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductPrice)).castDown<EmptyType>();
                            // Remove from aligned products list
                            bool removed = false;
                            for (int k=0; (k<alignedGroups.size()) && !removed; k++) {
                                vector<matchingItem> l = alignedGroups[k].list;
                                for (int kk=0; kk<l.size(); kk++) {
                                    if (l[kk].index == indexProductPrice) {
                                        // Remove from this list
                                        if (l.size() == 1) {
                                            // Remove from alignedGroups altogether!
                                            alignedGroups.erase(alignedGroups.begin() + k);
                                            removed = true; break; // leave kk loop, i.e. stop looking at the list we just removed
                                        } else {
                                            l.erase(l.begin() + kk);
                                            alignedGroups[k].list = l;
                                            alignedGroups[k].count--;
                                            removed = true; break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } // found dollar prices cluster
        } // iterate over matches
    } // product prices don't have dollars
    
    // Undo bad descriptions when there is a single outlier with a description misaligned compared to the majority
    if ((curResults.retailerParams.noProductNumbers) && (curResults.retailerParams.descriptionBeforePrice) && (curResults.retailerParams.descriptionAlignedLeft)) {
        // First figure out where most product prices are aligned
        int indexLargestAlignedGroup = -1, indexRightMostGroup = -1;
        int sizeLargestGroup = 0;
        float valueRightMostGroup = -1;
        for (int i=0; i<alignedGroups.size(); i++) {
            matchingList l = alignedGroups[i];
            if (l.count > sizeLargestGroup) {
                indexLargestAlignedGroup = i;
                sizeLargestGroup = l.count;
            }
            if (l.average > valueRightMostGroup) {
                indexRightMostGroup = i;
                valueRightMostGroup = l.average;
            }
        }
        if ((alignedGroups.size() > 1) && (sizeLargestGroup >= 2)) {
            bool canUntagOutlierPrices = true;
            // Proceed to eliminate outliers only if all groups other than largest have only one item (otherwise could be a legit group)
            for (int i=0; i<alignedGroups.size(); i++) {
                if (i == indexLargestAlignedGroup) continue;
                matchingList l = alignedGroups[i];
                if (l.count > 1) {
                    canUntagOutlierPrices = false;
                    break;
                }
            }
            // Could also be the subtotal + tax + total + mastercard set of lines: detect this by observing that one group has indexes that are all past the other group (could require AND adjacent lines AND the highest price in that group is higher than any other price in all other groups - don't do either for now). See https://drive.google.com/open?id=0B4jSQhcYsC9VTUczR2FkN3RzOXc
            if (!canUntagOutlierPrices) {
                int lowestIndexRightMostList = alignedGroups[indexRightMostGroup].list[0].index;
//                // Don't require subtotal group to have highest price - could fail if we got wrong subtotal price or if any product price was abnormally high (due to OCR error)
//                float highestPriceInRightMostList = -1;
//                matchingList listRightMost = alignedGroups[indexRightMostGroup];
//                for (int i=0; i<listRightMost.list.size(); i++) {
//                    matchingItem p = listRightMost.list[i];
//                    float currentPrice = atof(toUTF8(p.text).c_str());
//                    if (currentPrice > highestPriceInRightMostList)
//                        highestPriceInRightMostList = currentPrice;
//                }
                canUntagOutlierPrices = true;
                for (int i=0; i<alignedGroups.size(); i++) {
                    if (i == indexRightMostGroup) continue;
                    matchingList l = alignedGroups[i];
                    int highestIndexCurrentList = l.list[(int)l.list.size()-1].index;
                    if (highestIndexCurrentList >= lowestIndexRightMostList) {
                        canUntagOutlierPrices = false;
                        break;
                    }
                }
            }
            if (canUntagOutlierPrices) {
                //float idealEndProductRect = alignedGroups[indexLargestAlignedGroup].average;
                // Better idealEndProductRect is the end of the last product above the outliers
                float idealEndProductRect = alignedGroups[indexLargestAlignedGroup].list[alignedGroups[(int)indexLargestAlignedGroup].list.size()-1].floatValue;
                for (int i=1; i<curResults.matchesAddressBook.size(); i++) {
                    MatchPtr productPriceItem = curResults.matchesAddressBook[i];
                    if (getElementTypeFromMatch(productPriceItem, matchKeyElementType) != OCRElementTypeProductPrice)
                        continue;
                    int currentLineNumber = getIntFromMatch(productPriceItem, matchKeyLine);
                    MatchPtr descriptionItem = curResults.matchesAddressBook[i-1];
                    if (getIntFromMatch(descriptionItem, matchKeyLine) != currentLineNumber)
                        continue;
                    if (getElementTypeFromMatch(descriptionItem, matchKeyElementType) != OCRElementTypeProductDescription)
                        continue;
                    // Outlier?
                    CGRect productPriceRect = OCRResults::matchRange(productPriceItem);
                    int productPriceEndPosition = rectRight(productPriceRect) / curResults.globalStats.averageWidthDigits.average;
                    if (abs(productPriceEndPosition - idealEndProductRect) > 3) {
                        // Outlier! Untag description + product price
                        LayoutLog("processBlinkOCRResults: undoing desc+product price for [%s]+[%s]", toUTF8(getStringFromMatch(descriptionItem, matchKeyText)).c_str(), toUTF8(getStringFromMatch(productPriceItem, matchKeyText)).c_str());
                        OCRElementType tpReplacement = OCRElementTypeUnknown;
                        if (descriptionItem.isExists(matchKeyElementOrigType)) tpReplacement = getElementTypeFromMatch(descriptionItem, matchKeyElementOrigType);
                        descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(tpReplacement)).castDown<EmptyType>();
                        productPriceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
                    }
                }
            } // canUntagOutlierPrices
        } else if ((alignedGroups.size() == 1) && (sizeLargestGroup >= 2)) {
            // Even if there is only one group, check for a situation where at some point one product price ends significantly more to the right than the previous product + all other past that point do too
            float lastProductPriceEnd = -1;
            matchingList l = alignedGroups[0];
            vector<matchingItem> v = l.list;
            for (int i=0; i<v.size(); i++) {
                if (lastProductPriceEnd < 0) {
                    lastProductPriceEnd = rectRight(v[i].r);
                    continue;
                }
                float currentProductPriceEnd = rectRight(v[i].r);
                if (currentProductPriceEnd - lastProductPriceEnd > 2 * curResults.globalStats.averageWidthDigits.average) {
                    bool doit = true;
                    // Now check if all the rest of the list match the same criteria
                    for (int j=i+1; j<v.size(); j++) {
                        float currentProductPriceEnd = rectRight(v[j].r);
                        if (currentProductPriceEnd - lastProductPriceEnd < 2 * curResults.globalStats.averageWidthDigits.average) {
                            doit = false;
                            break;
                        }
                    }
                    if (doit) {
                        // Untag all descriptions starting with i and beyond
                        for (int j=i; j<v.size(); j++) {
                            matchingItem p = v[j];
                            MatchPtr productPriceItem = curResults.matchesAddressBook[p.index];
                            MatchPtr descriptionItem = curResults.matchesAddressBook[p.index1];
                            LayoutLog("processBlinkOCRResults: undoing desc+product price for [%s]+[%s]", toUTF8(getStringFromMatch(descriptionItem, matchKeyText)).c_str(), toUTF8(getStringFromMatch(productPriceItem, matchKeyText)).c_str());
                            productPriceItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePrice)).castDown<EmptyType>();
                            descriptionItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                        }
                        break;
                    } else {
                        break;
                    }
                } // found first product shifted right
            } // check all items
        }
        
        // One more thing: do we have margin metrics relative to the description + price area? If so, we can nuke noise!
        if ((curResults.retailerParams.leftMarginLeftOfDescription > 0) && ((curResults.retailerParams.rightMarginRightOfPrices > 0))) {
            // First calculate X range for all valid product description + product prices pairs
            float minXDescriptionsStart = 10000, maxXProductPricesEnd = 0;
            for (int i=1; i<curResults.matchesAddressBook.size(); i++) {
                MatchPtr productPriceItem = curResults.matchesAddressBook[i];
                if (getElementTypeFromMatch(productPriceItem, matchKeyElementType) != OCRElementTypeProductPrice)
                    continue;
                int currentLineNumber = getIntFromMatch(productPriceItem, matchKeyLine);
                MatchPtr descriptionItem = curResults.matchesAddressBook[i-1];
                if (getIntFromMatch(descriptionItem, matchKeyLine) != currentLineNumber)
                    continue;
                if (getElementTypeFromMatch(descriptionItem, matchKeyElementType) != OCRElementTypeProductDescription)
                    continue;
                CGRect descriptionRect = OCRResults::matchRange(descriptionItem);
                CGRect productPriceRect = OCRResults::matchRange(productPriceItem);
                if (rectLeft(descriptionRect) < minXDescriptionsStart)
                    minXDescriptionsStart = rectLeft(descriptionRect);
                if (rectRight(productPriceRect) > maxXProductPricesEnd)
                    maxXProductPricesEnd = rectRight(productPriceRect);
            }
            if ((maxXProductPricesEnd > 0) && (minXDescriptionsStart < maxXProductPricesEnd)) {
                float leftMargin = minXDescriptionsStart - curResults.retailerParams.leftMarginLeftOfDescription * curResults.globalStats.averageWidthDigits.average;
                float rightMargin = maxXProductPricesEnd + curResults.retailerParams.rightMarginRightOfPrices * curResults.globalStats.averageWidthDigits.average;
                bool deletedItem = false;
                for (int i=1; i<curResults.matchesAddressBook.size(); i++) {
                    MatchPtr d = curResults.matchesAddressBook[i];
                    CGRect itemRect = OCRResults::matchRange(d);
                    if ((rectRight(itemRect) < leftMargin)
                        || (rectLeft(itemRect) >  rightMargin)) {
                        // Delete!
                        LayoutLog("processBlinkOCRResults: removing [%s] outside margins!", toUTF8(getStringFromMatch(d, matchKeyText)).c_str());
                        curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin() + i);
                        deletedItem = true;
                        i--;
                    }
                }
                if (deletedItem) {
                    curResults.setBlocks(true);
                }
            }
        }
    }
    
    // For text-based retailers, check if one of the products is actually the subtotal line. If so, adjust it and all other products found past it
    if (curResults.retailerParams.noProductNumbers && !curResults.gotSubtotal)
        curResults.checkForSubtotalTaggedAsProduct();
    
    // Special pass: look for quantity items where the '@' was grabbed as a digit, e.g. "3@22.00" -> 30322.00. If there is another price on that line, this is probably a quantity + price (3 @ 22.00)
    if ((curResults.retailerParams.hasQuantities) && (curResults.retailerParams.quantityPPItemTotal)) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            int nextI = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            if (OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypeProductNumber, &curResults.matchesAddressBook, &curResults.retailerParams) >= 0) {
                i = nextI; continue; // Not expected on same line as product
            }
            bool noTotalPrice = false;
            vector<int> pricesList = OCRResults::elementsListWithTypeInlineAtIndex(i, OCRElementTypePrice, &curResults.matchesAddressBook);
            if (pricesList.size() == 0) {
                i = nextI; continue; // No prices at all here, moving on
            } else if (pricesList.size() < 2) {
                //i = nextI; continue;
                noTotalPrice = true;
            }
            int indexFirstPrice = pricesList[0];
            MatchPtr matchFirstPrice = curResults.matchesAddressBook[indexFirstPrice];
            wstring textFirstPrice = getStringFromMatch(matchFirstPrice, matchKeyText);
            if (textFirstPrice.length() < 6) { // Minimum length is 6 (e.g. 201.99)
                i = nextI; continue;
            }
//#if DEBUG
//            if ((textFirstPrice == L"3139.98") || (textFirstPrice ==L"31319.96")) {
//                DebugLog("");
//            }
//#endif
            // Note that we could test here that the product for which this price (e.g. 2 lines above for Home Depot) might be a quantity doesn't have a price on the product line. Not doing for now because the test below is fairly secure, checking that what we put together matches the equation quantity x price per item = total
            // Is there a '0' (possible '@' converted) with at least one digit before and at least one digit after, such that the implied quantity x price per item = 2nd price found on that line? If so, assume that '0' was a '@'
            float totalPriceValue = -1;
            int totalPriceValue100 = -1;
            if (!noTotalPrice) {
                totalPriceValue = atof(toUTF8(getStringFromMatch(curResults.matchesAddressBook[pricesList[1]], matchKeyText)).c_str());
                if (totalPriceValue <= 0) {
                    //i = nextI; continue;
                    noTotalPrice = true;
                }
                totalPriceValue100 = round(totalPriceValue * 100);
            }
            wstring relevantRange = textFirstPrice.substr(1, textFirstPrice.size() - 4 - 1);
            bool hasAtSign = false;
            if (matchFirstPrice.isExists(matchKeyFullLineText)) {
                wstring tmpText = getStringFromMatch(matchFirstPrice, matchKeyFullLineText);
                size_t locAt = tmpText.find(L"@");
                if ((locAt != wstring::npos) && (locAt > 0))
                    hasAtSign = true;
            }
            //size_t loc0 = relevantRange.find(L"0", 0);    // From after the first digit till 2nd digit before decimal point
            for (int locAtSign=0; locAtSign<relevantRange.length(); locAtSign++) {
                wstring possibleQuantityString = textFirstPrice.substr(0,locAtSign+1);
                int possibleQuantityValue = atoi(toUTF8(possibleQuantityString).c_str());
                if (possibleQuantityValue <= 1)
                    continue;
                if  (possibleQuantityValue > MAX_QUANTITY) {
                    break;  // Done, further iterating will only produce larger quantities
                }
                bool doneWithThisItem = false;
                bool setPossibleQuantityInProductMatch = false; // Remembers if we found a possible quantity at all, albeit not matching total
                int possibleQuantityValueToSetInProductMatch = -1;
                float possiblePricePerItemValueToSetInProductMatch = -1.00;
                for (int grabbingExtraChars=0; (grabbingExtraChars<=1) && (locAtSign+grabbingExtraChars < relevantRange.length()); grabbingExtraChars++) {
                    wstring possiblePricePerItemString = textFirstPrice.substr(locAtSign+1+1+grabbingExtraChars, wstring::npos);
                    wchar_t chAtSign = relevantRange[locAtSign];
                    if (noTotalPrice && (((chAtSign != '0') && (chAtSign != 'O')) || (grabbingExtraChars>0)))
                        continue;   // If no total price available, keep going since we only record these cases when no total to match against
                    float possiblePricePerItemValue = atof(toUTF8(possiblePricePerItemString).c_str());
                    int quantityXPricePerItem100 = round(possibleQuantityValue * possiblePricePerItemValue * 100);
                    if ((!noTotalPrice) && (quantityXPricePerItem100 == totalPriceValue100)) {
                        // Found it! Create quantity item + adjust price
//#if DEBUG
//                        if (grabbingExtraChars > 0) {
//                            DebugLog("");
//                        }
//#endif
                        WarningLog("processBlinkOCRResults: splitting price [%s] into [%s x %s] = [%s]", toUTF8(textFirstPrice).c_str(), toUTF8(possibleQuantityString).c_str(), toUTF8(possiblePricePerItemString).c_str(), toUTF8(getStringFromMatch(curResults.matchesAddressBook[pricesList[1]], matchKeyText)).c_str());
                        MatchPtr quantityItem = OCRResults::copyMatch(matchFirstPrice);
                        quantityItem[matchKeyText] = SmartPtr<wstring> (new wstring(possibleQuantityString)).castDown<EmptyType>();
                        quantityItem[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeProductQuantity)).castDown<EmptyType>();
                        curResults.matchesAddressBook.insert(curResults.matchesAddressBook.begin() + indexFirstPrice, quantityItem);
                        // Adjust priceText
                        matchFirstPrice[matchKeyText] = SmartPtr<wstring> (new wstring(possiblePricePerItemString)).castDown<EmptyType>();
                        nextI++; // We inserted one item
                        setPossibleQuantityInProductMatch = false;
                        doneWithThisItem = true;
                        break;
                    } else if ((noTotalPrice || hasAtSign) && (grabbingExtraChars == 0) && (chAtSign == '0')) {
                        // No total price or quantity x price didn't match total price, so instead we just want to identify the related product and attach to it this possible quantity + price. Later, if & when we get a server price matching this price per item, set quantity and price
                        setPossibleQuantityInProductMatch = true;
                        possibleQuantityValueToSetInProductMatch = possibleQuantityValue;
                        possiblePricePerItemValueToSetInProductMatch = possiblePricePerItemValue;
                    }
                } // grabbing 0 or 1 additional char
                if ((setPossibleQuantityInProductMatch) && (possibleQuantityValueToSetInProductMatch > 1) && (possiblePricePerItemValueToSetInProductMatch > 0)){
                    int productIndex = OCRResults::productIndexForQuantityAtIndex(indexFirstPrice, curResults.matchesAddressBook, &curResults.retailerParams);
                    if (productIndex >= 0) {
                        // Remember quantity & price per item within that product match
                        MatchPtr productItem = curResults.matchesAddressBook[productIndex];
                        productItem[matchKeyQuantity] = SmartPtr<int>(new int(possibleQuantityValueToSetInProductMatch)).castDown<EmptyType>();
                        productItem[matchKeyPricePerItem] = (SmartPtr<float> (new float(possiblePricePerItemValueToSetInProductMatch))).castDown<EmptyType>();
                    }
                }
                if (doneWithThisItem) break;
            } // for locAtSign, try all possible locations for '@'
            i = nextI; continue;
        } // for i, iterate over all matches
    }
    
    // Special pass: look for subtotal line
    curResults.gotSubtotal = false;
    if (curResults.matchesAddressBook.size() > 0) {
        for (int i=(int)curResults.matchesAddressBook.size() - 1; i>0; i--) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            
            if (tp != OCRElementTypeUnknown)
                continue;
            
            wstring itemText = getStringFromMatch(d, matchKeyText);
#if DEBUG
            if (itemText == L"SUBTOT")
                DebugLog("");
#endif
            if ((itemText.length() >= 6) // Accepting 6 because we added "SUBTOT" 2/29/2016
                && testForSubtotal(itemText, &curResults)) {
                int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &curResults.matchesAddressBook, NULL);
                if (priceIndex > 0) {
                    // Found it!
                    if (curResults.gotSubtotal)
                        d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePartialSubtotal)).castDown<EmptyType>();
                    else
                        d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeSubtotal)).castDown<EmptyType>();
                    curResults.lastFrame = true;
                    WarningLog("processBlinkOCRResults: found subtotal (%s)", toUTF8(getStringFromMatch(curResults.matchesAddressBook[priceIndex], matchKeyText)).c_str());
                    curResults.gotSubtotal = true;
                    if (!curResults.retailerParams.hasMultipleSubtotals) break;
                }
            }
            // Move to previous line - wrong: we could have "SUBTOTAL" + price + unknown on same line
            // i = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook);
        } // searching for subtotal
    }
    
    curResults.checkForTotal();
    
    curResults.checkForTaxBal();
    
    // For retailers without product numbers, and with quantities at the start of the line, it's important to parse for quantities before merging otherwise we'll get "1 XYZ" in the text2 and text3 of description items
    if (retailerParams.quantityAtDescriptionStart && retailerParams.noProductNumbers)
        curResults.checkForQuantities();
    
    // Special pass: look for phone number
    if (curResults.retailerParams.hasPhone) {
        for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            
            if (tp != OCRElementTypeUnknown)
                continue;
            
            wstring itemText = getStringFromMatch(d, matchKeyText);
#if DEBUG
            if (itemText == L"718.388.2799")
                DebugLog("");
#endif
            if ((itemText.length() >= 12) // Valid US phone number is 10 digits + 2 spaces or separators
                && testForPhoneNumber(itemText, &curResults)) {
                // Found it!
                d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypePhoneNumber)).castDown<EmptyType>();
                WarningLog("processBlinkOCRResults: found phone number (%s)", toUTF8(itemText).c_str());
            }
        } // searching for phone number
    } // has phone
    
    curResults.gotMergeablePattern1 = false;
    // Special pass: look for pattern 1
    if (curResults.retailerParams.mergeablePattern1Regex != NULL) {
        for (int i=0; i<(int)curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            
            if (tp != OCRElementTypeUnknown)
                continue;
            
            wstring itemText = getStringFromMatch(d, matchKeyText);
//#if DEBUG
//            if (itemText == L"REGX02 TRIJ81259 CSI1R41146440 STRS1059")
//                DebugLog("");
//#endif
            if ((itemText.length() >= curResults.retailerParams.mergeablePattern1MinLen)
                    && testForRegexPattern(itemText, curResults.retailerParams.mergeablePattern1Regex, OCRElementTypeMergeablePattern1, &curResults)) {
                // Found it!
                d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeMergeablePattern1)).castDown<EmptyType>();
                WarningLog("processBlinkOCRResults: found Mergeable Pattern 1 (%s)", toUTF8(itemText).c_str());
                curResults.gotMergeablePattern1 = true;
            }
        } // searching for Mergeable Pattern 1
    } // has pattern 1
    
    /*
    // If we still don't have a subtotal, consider looking instead for a total line
    // Actually, look for total ALWAYS, might be better than subtotal in some receipts
    //if ((curResults.matchesAddressBook.size() > 0) && !curResults.gotSubtotal && curResults.retailerParams.hasTotal)
    {
        bool abort = false;
        for (int i=(int)curResults.matchesAddressBook.size() - 1; (i>0) && (!abort); i--) {
            MatchPtr d = curResults.matchesAddressBook[i];
            OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
            
            if (tp != OCRElementTypeUnknown) {
                if (tp == OCRElementTypeProductNumber) {
                    break; // Not expecting products past the total!
                }
                continue;
            }
            
            wstring itemText = getStringFromMatch(d, matchKeyText);
#if DEBUG
            if (itemText == L"TOTA") {
                DebugLog("");
            }
#endif
            if ((itemText.length() >= 5)
                && testForTotal(itemText, &curResults)) {
                int priceIndex = OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, &curResults.matchesAddressBook, NULL);
                if (priceIndex > i) {
                    // Found it! Can we find a price just above (and no product number on that line)? If so, it's the tax amount, substract from total and label it a subtotal!
                    if (curResults.retailerParams.hasSubtotal && curResults.retailerParams.totalIncludesTaxAbove) {
                        int lineNumberTotal = getIntFromMatch(d, matchKeyLine);
                        MatchPtr totalPriceItem = curResults.matchesAddressBook[priceIndex];
                        CGRect totalPriceRange = OCRResults::matchRange(totalPriceItem);
                        int endIndexTaxLine = OCRResults::indexFirstElementOnLineAtIndex(priceIndex, &curResults.matchesAddressBook) - 1;
                        // Check there IS a line above and its line number is one less than total line
                        if ((endIndexTaxLine > 0) && (getIntFromMatch(curResults.matchesAddressBook[endIndexTaxLine], matchKeyLine) == lineNumberTotal - 1)) {
                            // Make sure no product numbers on this line
                            if (OCRResults::elementWithTypeInlineAtIndex(endIndexTaxLine, OCRElementTypeProductNumber, &curResults.matchesAddressBook, &curResults.retailerParams) == -1) {
                                int startIndexTaxLine = OCRResults::indexFirstElementOnLineAtIndex(endIndexTaxLine, &curResults.matchesAddressBook);
                                // Now look for a price, starting with the end
                                for (int j=endIndexTaxLine; j>= startIndexTaxLine; j--) {
                                    MatchPtr possibleTaxItem = curResults.matchesAddressBook[j];
                                    OCRElementType tp = getElementTypeFromMatch(possibleTaxItem, matchKeyElementType);
                                    if (tp == OCRElementTypePrice) {
                                        // Check X overlap with our total price!
                                        CGRect possibleTaxItemRange = OCRResults::matchRange(curResults.matchesAddressBook[priceIndex]);
                                        if (OCRUtilsOverlappingX(totalPriceRange, possibleTaxItemRange)) {
                                            wstring taxText = getStringFromMatch(possibleTaxItem, matchKeyText);
                                            float taxValue = atof(toUTF8(taxText).c_str());
                                            wstring totalText = getStringFromMatch(totalPriceItem, matchKeyText);
                                            float totalValue = atof(toUTF8(totalText).c_str());
                                            if (totalValue > taxValue) {
                                                totalValue -= taxValue;
                                                char *newTotalCSTR = (char *)malloc(100);
                                                sprintf(newTotalCSTR, "%.2f", totalValue);
                                                WarningLog("processBlinkOCRResults: found total (%s) and tax above (%s) => net total=%s", toUTF8(totalText).c_str(), toUTF8(taxText).c_str(), newTotalCSTR);
                                                totalText = wstring(toUTF32(newTotalCSTR));
                                                free(newTotalCSTR);
                                                totalPriceItem[matchKeyText] = SmartPtr<wstring> (new wstring(totalText)).castDown<EmptyType>();
                                                int flags = 0;
                                                if (totalPriceItem.isExists(matchKeyStatus))
                                                    flags = getIntFromMatch(totalPriceItem, matchKeyStatus);
                                                flags |= matchKeyStatusHasTaxAbove;
                                                totalPriceItem[matchKeyStatus] = SmartPtr<int>(new int(flags)).castDown<EmptyType>();
                                                d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotal)).castDown<EmptyType>();
                                                curResults.gotTotal = true;
                                                abort = true; break;
                                            } // tax value < total value
                                        } // tax item X overlaps total price
                                    } // found price on line above
                                } // iterate over items on line above looking for tax item
                                if (abort)
                                    break;
                            } // no product on line above
                        } // endIndex valid
                    } // Has tax above total
// For now don't set as "total" if we couldn't get the tax amount above
//                    d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeTotal)).castDown<EmptyType>();
//                    curResults.lastFrame = true;
//                    WarningLog("processBlinkOCRResults: found total (%s)", toUTF8(getStringFromMatch(curResults.matchesAddressBook[priceIndex], matchKeyText)).c_str());
//                    curResults.gotSubtotal = true;
//                    break;
                } // found price on same line
            } // matched total regex
        } // searching for total
    }
    */

    // Special pass: eliminate bogus product too close to their price (or to the prices ranges)
    // e.g. eliminate the invoice number (below) and zip code (above) on Lowes receipts
    if ((curResults.retailerParams.minProductNumberEndToPriceStart > 0) && (curResults.globalStats.averageWidthDigits.count > 5)) {
        bool validPricesRange = false;
        // When no price for a product, use the prices range only if that range itself is such that it passes the test relative to the products range. This will avoid situations where bogus prices and/or products flagged areas too close to each other!
        if ((curResults.productPricesRange.size.width > 0) && (curResults.productsWithPriceRange.size.width > 0)) {
            int numDigits = (rectLeft(curResults.productPricesRange) - rectRight(curResults.productsWithPriceRange)) / curResults.globalStats.averageWidthDigits.average;
            if (numDigits >= curResults.retailerParams.minProductNumberEndToPriceStart)
                validPricesRange = true;
        }
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeProductNumber)
                continue;
            CGRect itemRange = OCRResults::matchRange(d);
            CGRect refRange;
            refRange.size.width = 0;
            int endOfLine = -1, startOfLine = -1;
            if (curResults.retailerParams.priceAboveProduct) {
                endOfLine = OCRResults::indexFirstElementOnLineAtIndex(i, &curResults.matchesAddressBook) - 1;
                if (startOfLine < 0) {
                    i = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook); // Skip current line
                    continue;
                }
                startOfLine = OCRResults::indexFirstElementOnLineAtIndex(endOfLine, &curResults.matchesAddressBook);
            } else {
                startOfLine = i+1;  // Look only after the product number
                endOfLine = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook);
            }
//#if DEBUG
//            wstring tmpText = getStringFromMatch(d, matchKeyText);
//            if (tmpText == L"2491")
//                DebugLog("");
//#endif
            // Find the rightmost price (to give this product the best chance to pass the test)
            for (int j=endOfLine; j>=startOfLine; j--) {
                MatchPtr priceItem = curResults.matchesAddressBook[j];
                if (getElementTypeFromMatch(priceItem, matchKeyElementType) == OCRElementTypePrice) {
                    refRange = OCRResults::matchRange(priceItem);
                    break;
                }
            }
            if ((refRange.size.width <= 0) && validPricesRange) {
                refRange = curResults.productPricesRange;
            }
            if (refRange.size.width > 0) {
                wstring itemText = getStringFromMatch(d, matchKeyText);
                float digitWidth = itemRange.size.width / itemText.length(); // Best estimate because taken on current line + takes into account the space between digits
                int numDigits = (rectLeft(refRange) - rectRight(itemRange) - 1) / digitWidth;
                if (numDigits < curResults.retailerParams.minProductNumberEndToPriceStart) {
                    // Eliminate!
                    LayoutLog("Eliminating product [%s] at index [%d], too close to prices [distance=%.0f, digit width=%.0f, num digits=%d, expected %d]", toUTF8(getStringFromMatch(d, matchKeyText)).c_str(), i, rectLeft(refRange) - rectRight(itemRange) - 1, digitWidth, numDigits, curResults.retailerParams.minProductNumberEndToPriceStart);
                    // Keep it but set to unknown
                    d[matchKeyElementType] = SmartPtr<OCRElementType>(new OCRElementType(OCRElementTypeUnknown)).castDown<EmptyType>();
                }
            }
            // Skip line
            i = endOfLine; // Loop will increment
        }
    } // Eliminate bogus products
    
    curResults.numProducts = 0;
    curResults.gotDate = false;
    // Special pass: count products and not if we got a date
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        OCRElementType tp = getElementTypeFromMatch(d, matchKeyElementType);
        if (tp == OCRElementTypeProductNumber)
            curResults.numProducts++;
        else if (tp == OCRElementTypeDate)
            curResults.gotDate = true;
        else if (tp == OCRElementTypePhoneNumber)
            curResults.gotPhone = true;
    }
    
    // Eliminate any short items that don't have a type yet (careful not to eliminate quantities (they have length = 1 usually)
    if (curResults.retailerParams.minItemLen > 0) {
        for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
            MatchPtr d = curResults.matchesAddressBook[i];
            wstring& newText = getStringFromMatch(d, matchKeyText);
            OCRElementType tp = (OCRElementType)getIntFromMatch(d, matchKeyElementType);
            if ((newText.length() < curResults.retailerParams.minItemLen) && (tp == OCRElementTypeUnknown)) {
                LayoutLog("Eliminating item [%s] at index [%d]", toUTF8(newText).c_str(), i);
                curResults.matchesAddressBook.erase(curResults.matchesAddressBook.begin()+i--);
            }
        }
    }
    
    curResults.numProductsWithoutProductNumbers = 0;
    curResults.numProductsWithQuantities = 0;
    // For retailers without product numbers, count desc + product price combos and how many of these had a quantity
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeProductDescription) continue;
        int priceIndex = OCRResults::productPriceIndexForDescriptionAtIndex(i, OCRElementTypeProductPrice, curResults.matchesAddressBook, &curResults.retailerParams);
        if (priceIndex <= 0) {
            i = OCRResults::indexLastElementOnLineAtIndex(i, &curResults.matchesAddressBook); continue;
        }
        curResults.numProductsWithoutProductNumbers++;
        int status = 0;
        if (d.isExists(matchKeyStatus)) {
            status = getIntFromMatch(d, matchKeyStatus);
        }
        if (status & matchKeyStatusHasQuantity)
            curResults.numProductsWithQuantities++;
    }
    
    // Blurry test: calculate the percentage of vertical lines for all non-unknown items tagged
    int numNonSpaceLettersCombined = 0;
#if DEBUG
    int numItemsConsidered = 0;
#endif
    float percentageVerticalLinesCombined = 0;
    for (int i=0; i<curResults.matchesAddressBook.size(); i++) {
        MatchPtr d = curResults.matchesAddressBook[i];
        if (!OCRResults::typeRelevantToUser(getElementTypeFromMatch(d, matchKeyElementType), &curResults.retailerParams))
            continue;
        if (d.isExists(matchKeyPercentVerticalLines) && d.isExists(matchKeyNumNonSpaceLetters)) {
            int currentNumNonSpaceLetters = getIntFromMatch(d, matchKeyNumNonSpaceLetters);
            float currentPercentageVerticalLines = getFloatFromMatch(d, matchKeyPercentVerticalLines);
            if ((currentNumNonSpaceLetters > 0) && (currentPercentageVerticalLines >= 0)) {
                percentageVerticalLinesCombined = (percentageVerticalLinesCombined * numNonSpaceLettersCombined + currentPercentageVerticalLines * currentNumNonSpaceLetters)/(numNonSpaceLettersCombined + currentNumNonSpaceLetters);
                numNonSpaceLettersCombined += currentNumNonSpaceLetters;
#if DEBUG
                numItemsConsidered++;
#endif                
            }
        }
    }
    MergingLog("importNewResults: frame %d - percentage vertical lines = %.2f (number of relevant letters = %d, accross %d items)", globalResults.frameNumber + 1, percentageVerticalLinesCombined, numNonSpaceLettersCombined, numItemsConsidered);
    
    bool validFrame = true; // TODO: for now we only detect ignored frame when importing, not if the very first frame is bogus.
    
    if (percentageVerticalLinesCombined > 0.30) {
        MergingLog("importNewResults: REJECTING frame %d - percentage vertical lines = %.2f (number of relevant letters = %d, accross %d items)", globalResults.frameNumber + 1, percentageVerticalLinesCombined, numNonSpaceLettersCombined, numItemsConsidered);
        validFrame = false;
    }
    
    // Set the various frame flags
    /* Character height for Target at closest range is 30 (width = 720), i.e. 4.17%. We can use this ratio as a guideline to differentiate between:
        1. Receipt aligned left but not truncated: in this case the ratio between letter height and typedRange (or OCRRange is not types) will be around the same as 4.17%
        2. Receipt truncated left: in this case the ratio will be bigger, i.e. letter height will be a higher percentage of the width
    */
    float letterHeightToWidthRatio = -1;
    if (curResults.globalStats.averageHeight.count >= 9) {
        if (curResults.realResultsRange.size.width > 0) {
            letterHeightToWidthRatio = curResults.globalStats.averageHeight.average / curResults.realResultsRange.size.width;
        } else if (curResults.typedResultsRange.size.width > 0) {
            letterHeightToWidthRatio = curResults.globalStats.averageHeight.average / curResults.typedResultsRange.size.width;
        } else if (curResults.OCRRange.size.width > 0) {
            letterHeightToWidthRatio = curResults.globalStats.averageHeight.average / curResults.OCRRange.size.width;
        }
    }
    float leftMargin = -1, rightMargin = -1;
    if (curResults.realResultsRange.size.width > 0) {
        CGRect relevantRange = curResults.realResultsRange;
//        if (curResults.typedResultsRange.size.width > 0)
//            relevantRange = curResults.typedResultsRange;
//        else if (curResults.OCRRange.size.width > 0)
//            relevantRange = curResults.OCRRange;
        leftMargin = relevantRange.origin.x / curResults.imageRange.size.width;
        rightMargin = (curResults.imageRange.size.width - (relevantRange.origin.x + relevantRange.size.width - 1)) / curResults.imageRange.size.width;
    }
    
    // Remember the highest product line number
    curResults.currentHighestProductLineNumber = OCRResults::highestProductLineNumber(curResults.matchesAddressBook);
    
    if (curResults.typedResultsRange.size.width > 0) {
//        float typedRatio = curResults.typedResultsRange.size.width / curResults.imageRange.size.width;
//        if (typedRatio < retailerParams.typedRangeToImageWidthTooFar) {
//            curResults.frameStatus |= SCAN_STATUS_TOOFAR;
//            LayoutLog("IMAGE STATS: too far!");
//        }
    } else {
        // No typed results at all, this is a bad frame
        curResults.frameStatus |= SCAN_STATUS_NODATA;
        validFrame = false;
        LayoutLog("IMAGE STATS: no data!");
    }
    
    if (curResults.retailerParams.noProductNumbers && !firstFrame) {
        MergingLog("importNewResults: frame %d - numProductsWithoutProductNumbers=%d, numProductsWithQuantities=%d", globalResults.frameNumber + 1, curResults.numProductsWithoutProductNumbers, curResults.numProductsWithQuantities);
        if ((curResults.numProductsWithoutProductNumbers == 0) || (curResults.retailerParams.allProductsHaveQuantities && (curResults.numProductsWithQuantities <= curResults.numProductsWithoutProductNumbers * 0.50))) {
            MergingLog("importNewResults: IGNORING frame %d - numProductsWithoutProductNumbers=%d, numProductsWithQuantities=%d", globalResults.frameNumber + 1, curResults.numProductsWithoutProductNumbers, curResults.numProductsWithQuantities);
            validFrame = false;
        } else if ((globalResults.numProductsWithoutProductNumbers <= 1) && (curResults.numProductsWithoutProductNumbers >= 2)) {
            MergingLog("importNewResults: throwing out everything we have so far, only %d products without product numbers and frame %d has %d numProductsWithoutProductNumbers", globalResults.numProductsWithoutProductNumbers, globalResults.frameNumber + 1, curResults.numProductsWithoutProductNumbers);
            firstFrame = true;
        }
    }
    
    // Too close? If we are too close, it means by definition that the image width is less that the receipt width, while the letter height is growing. It follows therefore that letter height / image width must exceed retailerParams.letterHeightToRangeWidth
    if ((retailerParams.letterHeightToRangeWidth > 0) && (curResults.globalStats.averageHeight.count > 20)) {
        float letterHeightToImageWidthRatio = curResults.globalStats.averageHeight.average / curResults.imageRange.size.width;
        if (letterHeightToImageWidthRatio > retailerParams.letterHeightToRangeWidth * 1.19) {
            if (curResults.retailerParams.code != RETAILER_CVS_CODE)
                curResults.frameStatus |= SCAN_STATUS_TOOCLOSE;
            LayoutLog("IMAGE STATS: too close!");
//            if (retailerParams.noProductNumbers) {
//                MergingLog("importNewResults: IGNORING frame %d because too close", globalResults.frameNumber + 1);
//                validFrame = false;
//            }
        } else if (letterHeightToImageWidthRatio < retailerParams.letterHeightToRangeWidth * 0.65) {
            curResults.frameStatus |= SCAN_STATUS_TOOFAR;
            LayoutLog("IMAGE STATS: too far!");
            float minRequiredRejectRatio = (retailerParams.noProductNumbers? 0.60:0.45);
            if (letterHeightToImageWidthRatio < retailerParams.letterHeightToRangeWidth * minRequiredRejectRatio) {
                int nValidResults = (curResults.retailerParams.noProductNumbers? curResults.numProductsWithoutProductNumbers: curResults.numProducts);
                if (curResults.gotSubtotal) nValidResults++;
                if (curResults.gotDate) nValidResults++;
                if (curResults.gotMergeablePattern1) nValidResults++;
                // Don't reject if we found OCRElementTypeMergeablePattern1 because finding it means *some* measure of accuracy + hope that we will use it as an anchor to merge
                // 2016-05-13 reject even if we found the mergeable pattern, otherwise we still probably accept junk
                if ((nValidResults < 5) /*&& !curResults.gotMergeablePattern1*/
                    // For CVS and other text receipts, fail in any case below 50%
                    || (retailerParams.noProductNumbers && (letterHeightToImageWidthRatio < retailerParams.letterHeightToRangeWidth * 0.50))) {
                    MergingLog("importNewResults: IGNORING frame %d because too far and not enough products (%d) in %d lines", globalResults.frameNumber + 1, curResults.numProducts, curResults.currentHighestProductLineNumber);
                    validFrame = false;
                }
            }
        }
    }
    
    if (curResults.globalStats.averageHeight.count >= 9) {
        // For BabiesRUS ratio is around 75 / 1750 = 0.043. Applying 19% marging => 0.051
        // Target: 21 / 545 = 0.0385. Applying 19% marging => 0.046
//        float limitForFarEnough = 0.0385 * 1.19;
//        if (retailerParams.letterHeightToRangeWidth > 0)
//            limitForFarEnough = retailerParams.letterHeightToRangeWidth * 1.19;
//        if (letterHeightToWidthRatio > limitForFarEnough) {
//            curResults.frameStatus |= SCAN_STATUS_TOOCLOSE;
//            LayoutLog("IMAGE STATS: too close!");
//        }
        if ((letterHeightToWidthRatio > 0.04) && ((numProductNumbers == 0) || (numProductPrices == 0))) {
            // Truncation has occurred (left or right)!
            // At least 5 typed items yet no product numbers at all + left margin (and wider than right margin)
            if ((rightMargin > 0.10) && (leftMargin < 0.20) && (rightMargin > leftMargin) && (numProductNumbers == 0) && (numRealItems > 5)) {
                curResults.frameStatus |= SCAN_STATUS_TRUNCATEDLEFT;
                LayoutLog("IMAGE STATS: truncated left!");
            } else if ((leftMargin > 0.10) && (rightMargin < 0.25) && (leftMargin > rightMargin) && (numProductNumbers > 0) && (numProductPrices == 0)) {
                curResults.frameStatus |= SCAN_STATUS_TRUNCATEDRIGHT;
                LayoutLog("IMAGE STATS: truncated right!");
            }
        }
    }
    
#ifdef TARGET_IPHONE
    // This lines cause warning when building in debug mode. It is required in order to allow GDB to call these functions in debugging sessions
	void *rr = getOBJCMatches((void *)&curResults.matchesAddressBook);
    rr=rr;
#endif

    // 2015-10-09 If there are no product numbers in the new results (and no date + no subtotal + no phone), just skip that frame. The rational is as follows: no products mean no overlap with existing results, so we may have prices, but we really have no idea where this set of prices fit so it's simply dangerous to even take them into account! For example, say we scanned product 1-10 and now we get such a frame. We add prices for lines 11-12. Then we scan products 9-18 there is an overlap so now we'll merge the prices for products 11-12 with the bogus previous frame, which may well results in picking the wrong price
    if ((!curResults.retailerParams.noProductNumbers && (curResults.numProducts == 0)) && (!curResults.gotSubtotal) && (!curResults.gotDate) && (!curResults.gotPhone)) {
        validFrame = false;
        MergingLog("importNewResults: IGNORING frame %d", globalResults.frameNumber + 1);
    }

    if (firstFrame || newSequence) {
        if (validFrame) {
            // No scanned results yet => first frame, use current OCRResults class instance as our global results
            globalResults = curResults;
            globalResults.frameStatus = 0;  // First frame, no need to set any of the flags at this point (incl no need to set SCAN_STATUS_NONEWDATA, even though first frame scanned may be absent any info)
            firstFrame = false;
            globalResults.frameNumber = 1;
            globalResults.lastFrame = curResults.lastFrame;
            // The "meaningfulFrame" ivar is only set when importing an additional frame. For first frame just set if we got a product
            if ((numProductNumbers > 0) || (globalResults.retailerParams.noProductNumbers && (numProductPricesWithDescription > 0))) {
                globalResults.meaningfulFrame = true;
                globalResults.newInfoAdded = true;
                // Receipt images stuff
                globalResults.receiptImagesIndex = 0;   // First receipt image
                // Current frame is confirmed, i.e. we will not update it with a better frame, because it's the first one
                globalResults.lastConfirmedReceiptImageHighestProductLineNumber = curResults.currentHighestProductLineNumber;
                // No saved frame for the next frame, we'll start saving for this slot once a product is found beyond current last line
                globalResults.currentSavedReceiptImageHighestProductLineNumber = -1;
                MergingLog("processBlinkOCRResults: receipt images - saving image %d frame=%d, num products=%d, lastReceiptImageHighestProductLineNumber=%d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.lastConfirmedReceiptImageHighestProductLineNumber);
             } else {
                globalResults.meaningfulFrame = false;
                globalResults.newInfoAdded = false;
                globalResults.receiptImagesIndex = -1;  // No receipt images yet (this image is not worth saving, has no products)
                MergingLog("processBlinkOCRResults: receipt images - NOT saving image %d frame=%d, num products=%d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers);
            }
        } // valid frame
        else {
            // Keep firstFrame set, clear/reset all other variables just in case
            globalResults.frameNumber = 0;
            globalResults.receiptImagesIndex = -1;  // Ignore for the purpose of receipt images
            globalResults.frameStatus = 0;
            globalResults.lastFrame = 0;
            globalResults.meaningfulFrame = false;
            globalResults.newInfoAdded = false;
        }
    } else {
        if (done != NULL)
            *done = (curResults.lastFrame && globalResults.lastFrame);
        globalResults.lastFrame = curResults.lastFrame;
        globalResults.meaningfulFrame = false;
        globalResults.newInfoAdded = false;
        globalResults.frameStatus = 0;     // Clear flags before importing
//#if DEBUG
//        if (globalResults.frameNumber >= 14) {
//            DebugLog("");
//        }
//#endif
        
        // If flagged as not valid, do NOT merge it into current results as it may bring it bad stuff!
        if (validFrame) {
            validFrame = globalResults.importNewResults(&curResults);
        }
        globalResults.frameNumber++;
        
        // Receipt images stuff
        if (validFrame) {
            globalResults.currentHighestProductLineNumber = OCRResults::highestProductLineNumber(globalResults.matchesAddressBook);
            // Update saved receipt image (or start a new saved image)?
            if ((numProductNumbers > 0) || (globalResults.retailerParams.noProductNumbers && (numProductPricesWithDescription > 0))) {
                // First receipt image ever?
                if (globalResults.receiptImagesIndex < 0) {
                    globalResults.receiptImagesIndex = 0;
                    globalResults.lastConfirmedReceiptImageHighestProductLineNumber = globalResults.currentHighestProductLineNumber;
                    // No new image candidate yet
                    globalResults.currentSavedReceiptImageHighestProductLineNumber = -1;
                    MergingLog("processBlinkOCRResults: receipt images - saving image %d frame=%d, num products=%d, lastReceiptImageHighestProductLineNumber=%d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.lastConfirmedReceiptImageHighestProductLineNumber);
                } else {
                    // Not first receipt image
                    if (globalResults.currentSavedReceiptImageHighestProductLineNumber < 0) {
                        // No current saved frame (must be frames just after the initial saved frame)
                        // Did this frame contribute new products beyond the last confirmed frame?
                        if (globalResults.currentHighestProductLineNumber > globalResults.lastConfirmedReceiptImageHighestProductLineNumber) {
                            // New receipt image candidate
                            globalResults.receiptImagesIndex++;
                            globalResults.currentSavedReceiptImageHighestProductLineNumber = globalResults.currentHighestProductLineNumber;
                            globalResults.currentNumProductsWithPrice = curResults.numProductsWithPrice;
                            MergingLog("processBlinkOCRResults: receipt images - saving new image candidate %d frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                        }
                    } else {
                        // We have a current saved image candidate. Replace with current, ignore current or start a new image?
                        // No products beyond current image candidate => ignore
                        if (globalResults.currentHighestProductLineNumber < globalResults.currentSavedReceiptImageHighestProductLineNumber) {
                            MergingLog("processBlinkOCRResults: receipt images - at image %d, ignoring new frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                            //curResults.frameStatus |= SCAN_STATUS_NONEWDATA;
                            LayoutLog("IMAGE STATS: no new data!");
                        } else if (globalResults.currentHighestProductLineNumber == globalResults.currentSavedReceiptImageHighestProductLineNumber) {
                            // Ignore or update?
                            if (globalResults.currentNumProductsWithPrice < curResults.numProductsWithPrice) {
                            globalResults.currentSavedReceiptImageHighestProductLineNumber = globalResults.currentHighestProductLineNumber;
                                globalResults.currentNumProductsWithPrice = curResults.numProductsWithPrice;
                                MergingLog("processBlinkOCRResults: receipt images - at image %d, improving with new frame=%d, num products=%d, num products with price=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, curResults.numProductsWithPrice, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                            } else {
                                MergingLog("processBlinkOCRResults: receipt images - at image %d, ignoring new frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                                //curResults.frameStatus |= SCAN_STATUS_NONEWDATA;
                                LayoutLog("IMAGE STATS: no new data!");
                            }
                        } else {
                            // New frame contributed new products beyond the current image candidate (i.e. has more stuff)
                            // Still overlaps with last confirmed => better than current candidate
                            if (globalResults.currentLowestMatchingProductLineNumber <= globalResults.lastConfirmedReceiptImageHighestProductLineNumber) {
                                // Still overlapping with last confirmed image so simply update current saved image
                                globalResults.currentSavedReceiptImageHighestProductLineNumber = globalResults.currentHighestProductLineNumber;
                                globalResults.currentNumProductsWithPrice = curResults.numProductsWithPrice;
                                MergingLog("processBlinkOCRResults: receipt images - at image %d, improving with new frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                            } else {
                                // Start new image!
                                MergingLog("processBlinkOCRResults: receipt images - confirming image %d as final choice with new frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                                globalResults.lastConfirmedReceiptImageHighestProductLineNumber = globalResults.currentSavedReceiptImageHighestProductLineNumber;
                                globalResults.currentSavedReceiptImageHighestProductLineNumber = globalResults.currentHighestProductLineNumber;
                                globalResults.currentNumProductsWithPrice = curResults.numProductsWithPrice;
                                globalResults.receiptImagesIndex++;
                                MergingLog("processBlinkOCRResults: receipt images - starting image %d with new frame=%d, num products=%d, spanning lines %d - %d", globalResults.receiptImagesIndex, globalResults.frameNumber, numProductNumbers, globalResults.currentLowestMatchingProductLineNumber, globalResults.currentHighestProductLineNumber);
                            }
                        }
                    }
                }
            } // receipt image stuff
        } // valid frame?
    } // importing frame
    
    // For text-based retailers, check if one of the products is actually the subtotal line. If so, adjust it and all other products found past it
    if (globalResults.retailerParams.noProductNumbers && !globalResults.gotSubtotal)
        globalResults.checkForSubtotalTaggedAsProduct();
    
    // Whether it's a single frame (first frame) or resulting from having merged a new frame, look for quantity lines
    if (!(retailerParams.quantityAtDescriptionStart && retailerParams.noProductNumbers))
        globalResults.checkForQuantities();
        
    // Check for coupons
    globalResults.checkForCoupons();
    
    // Count current combined number of product without prices (only for retailers with no product numbers)
    if (globalResults.retailerParams.noProductNumbers) {
        globalResults.numProductsWithoutProductNumbers = 0;
        globalResults.numProductsWithQuantities = 0;
        for (int i=0; i<globalResults.matchesAddressBook.size(); i++) {
            MatchPtr d = globalResults.matchesAddressBook[i];
            if (getElementTypeFromMatch(d, matchKeyElementType) != OCRElementTypeProductDescription) continue;
            int priceIndex = OCRResults::productPriceIndexForDescriptionAtIndex(i, OCRElementTypeProductPrice, globalResults.matchesAddressBook, &globalResults.retailerParams);
            if (priceIndex <= 0) {
                i = OCRResults::indexLastElementOnLineAtIndex(i, &globalResults.matchesAddressBook); continue;
            }
            globalResults.numProductsWithoutProductNumbers++;
            int status = 0;
            if (d.isExists(matchKeyStatus)) {
                status = getIntFromMatch(d, matchKeyStatus);
            }
            if (status & matchKeyStatusHasQuantity)
                globalResults.numProductsWithQuantities++;
        }
    }
    
    if (meaningfulFramePtr != NULL)
        *meaningfulFramePtr = globalResults.meaningfulFrame;

    if (oldDataPtr != NULL) {
        // Set oldData = true only if we got product(s) or subtotal AND yet no new info was updated into existing results
        if (((curResults.retailerParams.noProductNumbers && ((curResults.numProductsWithoutProductNumbers > 0) || curResults.gotSubtotal))
            || (!curResults.retailerParams.noProductNumbers && ((curResults.numProducts > 0) || curResults.gotSubtotal)))
            && !globalResults.newInfoAdded) {
            *oldDataPtr = true;
            MergingLog("processBlinkOCRResults: frame=%d - OLD DATA", globalResults.frameNumber);
        } else
            *oldDataPtr = false;
    }
    
    if (receiptImgIndexPtr != NULL) {
        // The iOS app ignores frames from consideration as saved receipt images when the scanning code returns a receiptImgIndex set to -1 
        //if (curResults.frameStatus & SCAN_STATUS_NONEWDATA)
        if (!globalResults.newInfoAdded)
            *receiptImgIndexPtr = -1;
        else
            *receiptImgIndexPtr = globalResults.receiptImagesIndex;
    }
    if (frameStatusPtr != NULL)
        *frameStatusPtr = curResults.frameStatus;
    
    return globalResults.matchesAddressBook;
} // processBlinkOCRResults

void mySplit(wstring text, wchar_t ch, vector<wstring> &elements) {
    split(text, ch, elements);
}

vector<int> retailersForBarcode(wstring barcode) {
    vector<int> retailers;
    
    if (barcode.length() == 34) {
        retailers.push_back(RETAILER_BESTBUY_CODE);
        DebugLog("Detected Best Buy barcode");
    } else if (barcode.length() == 24) {
        retailers.push_back(RETAILER_LOWES_CODE);
        DebugLog("Detected Lowe's barcode");
    } else if (barcode.length() == 20) {
        retailers.push_back(RETAILER_BBBY_CODE);
        DebugLog("Detected BBBY barcode");
    } else if (barcode.length() == 19) {
        retailers.push_back(RETAILER_MACYS_CODE);
        DebugLog("Detected Macy's barcode");
    } else if (barcode.length() == 18) {
        if (barcode[0] == '2') {
            retailers.push_back(RETAILER_TARGET_CODE);
            DebugLog("Detected Target barcode");
        } else if (barcode[0] == '5') {
            retailers.push_back(RETAILER_BABIESRUS_CODE);
            retailers.push_back(RETAILER_TOYSRUS_CODE);
            DebugLog("Detected Babierus or Toysrus barcode");
        }
    } else if (barcode.length() == 17) {
        retailers.push_back(RETAILER_STAPLES_CODE);
        DebugLog("Detected Staples barcode");
    } else if (barcode.length() == 16) {
        retailers.push_back(RETAILER_WALMART_CODE);
        DebugLog("Detected Walmart barcode");
    } else if (barcode.length() == 14) {
        retailers.push_back(RETAILER_HOMEDEPOT_CODE);
        DebugLog("Detected Home Depot barcode");
    }
    
    return retailers;
}

} // namespace ocrparser