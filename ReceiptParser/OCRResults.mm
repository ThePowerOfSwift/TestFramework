//
//  OCRGlobalResults.m
//  Windfall
//
//  Created by Patrick Questembert on 4/1/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//
// Implement an iOS class with helper function to build or convert results from the OCR parser

#import "OCRResults.h"
#import "ocr.hh"
#import "OCRClass.hh"
#import "receiptParser.hh"

@implementation OCRGlobalResults

//PQTOTO moved to C++, comment out
+(bool)closeMatchPrice:(float)price toPartialPrice:(NSString *)partialPrice {
    // Strip leading dollar sign
    if ([[partialPrice substringToIndex:1] isEqualToString:@"$"]) {
        partialPrice = [partialPrice substringFromIndex:1];
    }
    // Strip space(s)
    while ([[partialPrice substringToIndex:1] isEqualToString:@" "]) {
        partialPrice = [partialPrice substringFromIndex:1];
    }
    
    int roundedPrice = round(price * 100);
    if (roundedPrice < 10)
        return false;   // We don't really expect prices lower than $0.10, best to ignore these (simplifies length tests below)
    NSString *priceStr = [NSString stringWithFormat:@"%d", roundedPrice];
    // If less than $1.00 need to prepend "0", i.e. "99" -> "099"
    if (roundedPrice < 100)
        priceStr = [@"0" stringByAppendingString:priceStr];
    // Insert decimal dot
    priceStr = [NSString stringWithFormat:@"%@.%@", [priceStr substringToIndex:priceStr.length - 2], [priceStr substringFromIndex:priceStr.length - 2]];
    
    // Missing 2nd decimal (e.g. X.Y)?
    if ((partialPrice.length >= 3) && [[partialPrice substringToIndex:3] isEqualToString:[priceStr substringToIndex:3]]) {
        return true;
    }
    
    // 3 chars after decimal point (one too many) and either first digit or last digit match?
    if (partialPrice.length >= 5) {
        NSRange rangeDot = [partialPrice rangeOfString:@"."];
        if ((rangeDot.location != NSNotFound) && (rangeDot.location == partialPrice.length - 4))
        {
            //int firstDecimalPartialPrice = (int)[[NSNumber numberWithChar:[partialPrice characterAtIndex:rangeDot.location+1]] integerValue];
            int firstDecimalPartialPrice = [[partialPrice substringWithRange:NSMakeRange(rangeDot.location+1, 1)] intValue];
            int firstDecimalPrice = (int)(price * 10 + 0.50) % 10;
            if (firstDecimalPartialPrice == firstDecimalPrice) {
                return true;
            }
            //int secondDecimalPartialPrice = (int)[[NSNumber numberWithChar:[partialPrice characterAtIndex:rangeDot.location+3]] integerValue];
            int secondDecimalPartialPrice = [[partialPrice substringWithRange:NSMakeRange(rangeDot.location+3, 1)] intValue];
            int secondDecimalPrice = (int)(price * 100 + 0.50) % 10;
            if (secondDecimalPartialPrice == secondDecimalPrice) {
                return true;
            }
            
        }
    }
    
    return false;
}

+(NSMutableArray *)buildMatchesFromMatchVector:(void *)resultsFromOCR ToMatches:(NSMutableArray *)matches {

	ocrparser::MatchVector *resultsWithTypes = (ocrparser::MatchVector *) resultsFromOCR;
											  
	static const std::string confidenceKeyStr ([@matchKeyConfidence cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string blockKeyStr ([@matchKeyBlock cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string lineKeyStr ([@matchKeyLine cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string globalLineKeyStr ([@matchKeyGlobalLine cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string columnKeyStr ([@matchKeyColumn cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string positionKeyStr ([@matchKeyPosition cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string NKeyStr ([@matchKeyN cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string statusKeyStr ([@matchKeyStatus cStringUsingEncoding:NSASCIIStringEncoding]);
	
    if (matches == nil) {
		matches = [NSMutableArray array];
	}
	  
	for (int i=0; i < resultsWithTypes->size(); i++) {
		  ocrparser::MatchPtr item = (*resultsWithTypes)[i];
		  NSMutableDictionary *newItem = [[NSMutableDictionary alloc] init];
		  
		  // Type
		  if (item.isExists(matchKeyElementType)) {
			  OCRElementType tmpTp = (*item[matchKeyElementType].castDown <OCRElementType>());

			  NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpTp];
			  [newItem setObject:tmpTypeObj forKey:@matchKeyElementType];
		  }
		  
		  // Text
		  if (item.isExists(matchKeyText)) {
			  std::string tmpString = ocrparser::toUTF8 (*item[matchKeyText].castDown <wstring>());
			  NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
			  [newItem setObject:tmpStr forKey:@matchKeyText];
              // Count
              if (item.isExists(matchKeyCountMatched)) {
                int tmpN = (*item[matchKeyCountMatched].castDown <int>());
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:tmpN];
                [newItem setObject:tmpTypeObj forKey:@matchKeyCountMatched];
              }
		  }
          // Full line text
		  if (item.isExists(matchKeyFullLineText)) {
			  std::string tmpString = ocrparser::toUTF8 (*item[matchKeyFullLineText].castDown <wstring>());
			  NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
			  [newItem setObject:tmpStr forKey:@matchKeyFullLineText];
		  }
          // Text2
		  if (item.isExists(matchKeyText2)) {
			  std::string tmpString = ocrparser::toUTF8 (*item[matchKeyText2].castDown <wstring>());
			  NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
			  [newItem setObject:tmpStr forKey:@matchKeyText2];
              // Confidence2
              if (item.isExists(matchKeyConfidence2)) {
                float tmpConf = (*item[matchKeyConfidence2].castDown <float>());
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithFloat:(float)tmpConf];
                [newItem setObject:tmpTypeObj forKey:@matchKeyConfidence2];
              }
              // Count2
              if (item.isExists(matchKeyCountMatched2)) {
                int tmpN = (*item[matchKeyCountMatched2].castDown <int>());
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:tmpN];
                [newItem setObject:tmpTypeObj forKey:@matchKeyCountMatched2];
              }
		  }
          // Text3
		  if (item.isExists(matchKeyText3)) {
			  std::string tmpString = ocrparser::toUTF8 (*item[matchKeyText3].castDown <wstring>());
			  NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
			  [newItem setObject:tmpStr forKey:@matchKeyText3];
              // Confidence3
              if (item.isExists(matchKeyConfidence3)) {
                float tmpConf = (*item[matchKeyConfidence3].castDown <float>());
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithFloat:(float)tmpConf];
                [newItem setObject:tmpTypeObj forKey:@matchKeyConfidence3];
              }
              // Count3
              if (item.isExists(matchKeyCountMatched3)) {
                int tmpN = (*item[matchKeyCountMatched3].castDown <int>());
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:tmpN];
                [newItem setObject:tmpTypeObj forKey:@matchKeyCountMatched3];
              }
		  }
/* Issues building around the CGRect struct, punting for now
		  if (item.isExists([@matchKeyRects cStringUsingEncoding:NSASCIIStringEncoding])) {
			  ocrparser::RectVectorPtr rectsPtr = (item[[@matchKeyRects cStringUsingEncoding:NSASCIIStringEncoding]].castDown <ocrparser::RectVector>());
			  NSInteger nRects = ((rectsPtr == NULL)? 0:rectsPtr->size());
			  if (nRects != 0) {
				  CGRect *rectsArray = (CGRect *)malloc(sizeof(CGRect) * nRects);
				  for (int j=0; j<nRects; j++) {
					  ocrparser::RectPtr rPtr = (*rectsPtr)[j];
					  CGRect tmpRect = CGRectMake (rPtr->origin.x, rPtr->origin.y, rPtr->size.width, rPtr->size.height);
					  rectsArray [j] = tmpRect;
				  }
				  NSNumber *tmpRects = [[NSNumber alloc] initWithInteger:(NSInteger)rectsArray];
				  [newItem setObject:tmpRects forKey:@matchKeyRects];
			  }
			  
			  NSNumber *tmpN = [[NSNumber alloc] initWithInteger:nRects];
			  [newItem setObject:tmpN forKey:@matchKeyN];
		  } 
*/
          // N (number of rects)
          static const std::string lineKeyStr (matchKeyLine);
		  if (item.isExists(NKeyStr)) {
			  
			  int tmpN = (*item[NKeyStr].castDown <int>());
			  NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:tmpN];
			  [newItem setObject:tmpTypeObj forKey:@matchKeyN];
		  }
		  // Confidence
		  if (item.isExists(confidenceKeyStr)) {
			  
			  float tmpConf = (*item[confidenceKeyStr].castDown <float>());
			  NSNumber *tmpTypeObj = [[NSNumber alloc] initWithFloat:(float)tmpConf];
			  [newItem setObject:tmpTypeObj forKey:@matchKeyConfidence];
		  }
		// Block
		if (item.isExists(blockKeyStr)) {
			
			int tmpValue = (*item[blockKeyStr].castDown <int>());
			NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpValue];
			[newItem setObject:tmpTypeObj forKey:@matchKeyBlock];
		}
		// Line
		if (item.isExists(lineKeyStr)) {
			
			int tmpValue = (*item[lineKeyStr].castDown <int>());
			NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpValue];
			[newItem setObject:tmpTypeObj forKey:@matchKeyLine];
		}
        // Global Line
		if (item.isExists(globalLineKeyStr)) {
			
			int tmpValue = (*item[globalLineKeyStr].castDown <int>());
			NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpValue];
			[newItem setObject:tmpTypeObj forKey:@matchKeyGlobalLine];
		}
		// Column
		if (item.isExists(columnKeyStr)) {
			
			int tmpValue = (*item[columnKeyStr].castDown <int>());
			NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpValue];
			[newItem setObject:tmpTypeObj forKey:@matchKeyColumn];
		}
		// Position
		if (item.isExists(positionKeyStr)) {
			
			int tmpValue = (*item[positionKeyStr].castDown <int>());
			NSNumber *tmpTypeObj = [[NSNumber alloc] initWithInt:(int)tmpValue];
			[newItem setObject:tmpTypeObj forKey:@matchKeyPosition];
		}
		// Status
		if (item.isExists(statusKeyStr)) {
			unsigned long tmpValue = (*item[statusKeyStr].castDown <unsigned long>());
            if (tmpValue != 0) {
                NSNumber *tmpTypeObj = [[NSNumber alloc] initWithUnsignedLong:tmpValue];
                [newItem setObject:tmpTypeObj forKey:@matchKeyStatus];
            }
		}
		
		// Finally, add the new item at the end (we assume that items were already ordered
		[matches addObject:newItem];
	  }

	return (matches);
}

+(NSString *)printMatchesFromMatchVector:(void *)resultsFromOCR {

	ocrparser::MatchVector *resultsWithTypes = (ocrparser::MatchVector *) resultsFromOCR;
											  
	static const std::string confidenceKeyStr ([@matchKeyConfidence cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string blockKeyStr ([@matchKeyBlock cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string lineKeyStr ([@matchKeyLine cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string columnKeyStr ([@matchKeyColumn cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string positionKeyStr ([@matchKeyPosition cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string NKeyStr ([@matchKeyN cStringUsingEncoding:NSASCIIStringEncoding]);
	
    NSString *result = [NSString string];
    int currentLine = -1;
    OCRElementType tp = OCRElementTypeUnknown;
	  
	for (int i=0; i < resultsWithTypes->size(); i++) {
		  ocrparser::MatchPtr item = (*resultsWithTypes)[i];
        
        if (item.isExists(lineKeyStr)) {
			int tmpValue = (*item[lineKeyStr].castDown <int>());
            if (tmpValue != currentLine) {
                if (currentLine != -1)
                    result = [result stringByAppendingFormat:@"\n%d#:", tmpValue];
                else
                result = [result stringByAppendingFormat:@"%d#:", tmpValue];
                currentLine = tmpValue;
                tp = OCRElementTypeUnknown;
            }
		}
		  
        // Type
        if (item.isExists(matchKeyElementType)) {
            tp = (*item[matchKeyElementType].castDown <OCRElementType>());
        }
		  
        // Text
        if (item.isExists(matchKeyText)) {
            std::string tmpString = ocrparser::toUTF8 (*item[matchKeyText].castDown <wstring>());
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            if (tp == OCRElementTypeProductNumber)
                result = [result stringByAppendingFormat:@" [%@]", tmpStr];
            else if (tp == OCRElementTypeProductDescription)
                result = [result stringByAppendingFormat:@" \"%@\"", tmpStr];
            else if (tp == OCRElementTypePrice)
                result = [result stringByAppendingFormat:@" {%@}", tmpStr];
            else if (tp == OCRElementTypeSubtotal)
                result = [result stringByAppendingFormat:@" =%@=", tmpStr];
            else if (tp == OCRElementTypeProductQuantity)
                result = [result stringByAppendingFormat:@" <%@>", tmpStr];
            else
                result = [result stringByAppendingFormat:@" %@", tmpStr];
        }

	  }

	return (result);
}

#if 0
/* Builds product dictionaries from a set of C++ scanning results, ignoring all items that don't represent a product. Each product has these properties:
    - sku (NSString): required, the number for this product at the retailer
    - description (NSString): optional, always exists on receipts but may have been missed by the OCR. Currently when we find a product number and a price on the same line we return whatever text is found between them as the product description
    - price (NSNumber - float): optional, always exists on receipts but may have been missed by the OCR
    - quantity (NSNumber - int): always set to 1 for now
 */

+(NSDictionary *)buildResultsFromMatchVector:(void *)resultsFromOCR withRetailerParams:(retailer_t *)retailerParams andServerPrices:(NSArray *)serverPrices {

	ocrparser::MatchVector *resultsWithTypes = (ocrparser::MatchVector *) resultsFromOCR;
	
    static const std::string typeKeyStr ([@matchKeyElementType cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string textKeyStr ([@matchKeyText cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string text2KeyStr ([@matchKeyText2 cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string text3KeyStr ([@matchKeyText3 cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string confidenceKeyStr ([@matchKeyConfidence cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string blockKeyStr ([@matchKeyBlock cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string lineKeyStr ([@matchKeyLine cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string columnKeyStr ([@matchKeyColumn cStringUsingEncoding:NSASCIIStringEncoding]);
	static const std::string positionKeyStr ([@matchKeyPosition cStringUsingEncoding:NSASCIIStringEncoding]);
    static const std::string statusKeyStr ([@matchKeyStatus cStringUsingEncoding:NSASCIIStringEncoding]);
	
    NSMutableDictionary *resultsDict = [NSMutableDictionary dictionary];
    
    resultsDict[@"products"] = [NSMutableArray array];
    resultsDict[@"discounts"] = [NSMutableArray array];
    
    BOOL ignoreDiscountsBecauseSubtotalFound = NO;
    
    float totalPrices = 0, subtotalValue = 0;
    int numPrices = 0;
    int indexMissingPrice = -1;
    int indexOfLastProduct = -1;
    bool abortMissingPriceOptimization = false;
    vector<int>excludedLines;   // Lines to be ignored (i.e. not processed to add elements to the iOS output)
    NSMutableDictionary *objMissingPrice = nil;
	  
	for (int i=0; i < resultsWithTypes->size(); i++) {
        ocrparser::MatchPtr item = (*resultsWithTypes)[i];
		  
        OCRElementType tp = (*item[typeKeyStr].castDown <OCRElementType>());
        
        int currentBlock = -1, currentLine = -1;
        
		if (item.isExists(blockKeyStr))
			currentBlock = (*item[blockKeyStr].castDown <int>());

		if (item.isExists(lineKeyStr))
			currentLine = (*item[lineKeyStr].castDown <int>());
        
        if (std::find(excludedLines.begin(), excludedLines.end(), currentLine) != excludedLines.end()) {
            // Skip
            i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, resultsWithTypes);
            continue;
        }
        
        if (tp == OCRElementTypeSubtotal) {
            // Create dictionary to hold the attributes of this product - but only if there is no other subtotal later, as in the Walmart case
            bool foundOtherProduct = false;
            for (int j=i+1; j<resultsWithTypes->size(); j++) {
                ocrparser::MatchPtr dd = (*resultsWithTypes)[j];
                if (ocrparser::getElementTypeFromMatch(dd, matchKeyText) == OCRElementTypeSubtotal) {
                    foundOtherProduct = true;
                    break;
                }
            }
            if (!foundOtherProduct) {
                // Need to find the price property on that same line
                int priceIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePrice, resultsWithTypes, NULL);
                if (priceIndex > 0) {
                    ocrparser::MatchPtr priceItem = (*resultsWithTypes)[priceIndex];
                    std::string tmpString = ocrparser::toUTF8 (*priceItem[textKeyStr].castDown <wstring>());
                    NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                    subtotalValue = [tmpStr floatValue];
                    NSNumber *floatObj = [NSNumber numberWithFloat:subtotalValue];
                
                    // Add subtotal property
                    NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
                    [newValue setObject:floatObj forKey:@"value"];
                    
                    resultsDict[@"subtotal"] = newValue;
                    
                    ignoreDiscountsBecauseSubtotalFound = YES;
                    
                    continue;
                }
            }
        }
        
        if (tp == OCRElementTypePriceDiscount) {
            if (!ignoreDiscountsBecauseSubtotalFound) {
                // Create dictionary to hold the attributes of this item
                std::string tmpString = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
                NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                float discountValue = [tmpStr floatValue];
                totalPrices -= discountValue;
                NSNumber *floatObj = [NSNumber numberWithFloat:discountValue];
                
                NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
                [newValue setObject:floatObj forKey:@"value"];
                    
                [resultsDict[@"discounts"] addObject:newValue];
            }
            
            continue;
        }
        
        if (tp == OCRElementTypeDate) {
        
            // Add date property
            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
            std::string tmpString = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            [newValueWithRange setObject:tmpStr forKey:@"value"];
            
            resultsDict[@"date"] = newValueWithRange;
            
            continue;
        }
        
        if (tp != OCRElementTypeProductNumber)
            continue;
        
        int productIndex = i;
        ocrparser::MatchPtr productItem = item;
        
        // Create dictionary to hold the attributes of this product
        NSMutableDictionary *newProduct = [NSMutableDictionary dictionary];
        
        // Add sku property
        NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
        std::string tmpString = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
#if DEBUG
        if ([tmpStr isEqualToString:@"009031891"]) {
            NSLog(@"");
        }
#endif
        [newValueWithRange setObject:tmpStr forKey:@"value"];
        
        indexOfLastProduct = i;
        
        [newProduct setObject:newValueWithRange forKey:@"sku"];
        
        // Set alternative sku (text2) if set
        if (item.isExists(text2KeyStr)) {
            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
            std::string tmpString = ocrparser::toUTF8 (*item[text2KeyStr].castDown <wstring>());
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            [newValueWithRange setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValueWithRange forKey:@"sku2"];
        }
        
        // Set alternative sku (text3) if set
        if (item.isExists(text3KeyStr)) {
            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
            std::string tmpString = ocrparser::toUTF8 (*item[text3KeyStr].castDown <wstring>());
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            [newValueWithRange setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValueWithRange forKey:@"sku3"];
        }
        
        if ((currentBlock < 0) || (currentLine < 0))
            continue;   // Something is wrong, don't try to find description and price

        // Add description (if present)
        NSMutableString *descriptionString;
        int descriptionIndex = ocrparser::OCRResults::descriptionIndexForProductAtIndex(i, *resultsWithTypes, NULL, retailerParams);
        
        if (descriptionIndex >= 0) {
            // Add description!
            ocrparser::MatchPtr d = (*resultsWithTypes)[descriptionIndex];
            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
        
            std::string tmpString = ocrparser::toUTF8 (*d[textKeyStr].castDown <wstring>());
            descriptionString = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            [newValueWithRange setObject:descriptionString forKey:@"value"];
        
            [newProduct setObject:newValueWithRange forKey:@"description"];
        } // adding description
        
        // Add price (if present) but first need to test if we have a quantity & price per item (depending on the retailer, on the line just below, or two lines below or something entirely different like Staples where quantity is on the first line (before description) and price per item is after the product number on the line below)
        float priceValue = -1.0, priceValue2 = -1, priceValue3 = -1, discountValue = -1;
        int quantityValue = -1;
        float pricePerItemValue = -1;
        int pricePerItemIndex = -1, quantityItemIndex = -1;
        int indexPossibleQuantityLine = i;
        // e.g. Staples
        if (retailerParams->quantityAtDescriptionStart) {
            int descriptionIndex = ocrparser::OCRResults::descriptionIndexForProductAtIndex(i, *resultsWithTypes, NULL, retailerParams);
            if (descriptionIndex > 0) {
                int tmpQuantityIndex = descriptionIndex - 1;
                ocrparser::MatchPtr tmpQuantityMatch = (*resultsWithTypes)[tmpQuantityIndex];
                ocrparser::MatchPtr tmpDescriptionMatch = (*resultsWithTypes)[descriptionIndex];
                    // Quantity on same line as description (just before it)
                if (((*tmpQuantityMatch[lineKeyStr].castDown <int>()) == (*tmpDescriptionMatch[lineKeyStr].castDown <int>()))
                    // Indeed has the quantity type
                    && ((*tmpQuantityMatch[typeKeyStr].castDown <OCRElementType>()) == OCRElementTypeProductQuantity)) {
                    // Got quantity, now also need price per item
                    if (retailerParams->pricePerItemAfterProduct) {
                        int tmpPricePerItemIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(productIndex, OCRElementTypePricePerItem, resultsWithTypes, retailerParams);
                        if (tmpPricePerItemIndex > productIndex) {
                            // Found everything, now set the values we need to remember
                            pricePerItemIndex = tmpPricePerItemIndex;
                            quantityItemIndex = tmpQuantityIndex;
                            ocrparser::MatchPtr tmpPricePerItemMatch = (*resultsWithTypes)[pricePerItemIndex];
                            std::string pricePerItemCString = ocrparser::toUTF8 ((*tmpPricePerItemMatch[textKeyStr].castDown <wstring>()));
                            NSMutableString *pricePerItemString = [[NSMutableString alloc] initWithCString:pricePerItemCString.c_str() encoding:NSUTF8StringEncoding];
                            pricePerItemValue = [pricePerItemString floatValue];

                            std::string quantityCString = ocrparser::toUTF8 ((*tmpQuantityMatch[textKeyStr].castDown <wstring>()));
                            NSMutableString *quantityString = [[NSMutableString alloc] initWithCString:quantityCString.c_str() encoding:NSUTF8StringEncoding];
                            quantityValue = [quantityString intValue];
                        } // Found price per item after product (on same line)
                    } // Looking for price per item after product
                } // Found quantity
            } // Found description
        } else if (retailerParams->hasQuantities) {
            // Look for next line
            for (int possibleQuantityLine=1; possibleQuantityLine<=retailerParams->maxLinesBetweenProductAndQuantity; possibleQuantityLine++) {
                // First time through, the line below will have indexPossibleQuantityLine pointing to the product item, and indexOfItemOnNextLine will get set to point to the next line
                // If retailer allows for up to 2 lines to find the quantity, after the end of the first pass, we set indexPossibleQuantityLine to indexOfItemOnNextLine
                int indexOfItemOnNextLine = ocrparser::OCRResults::indexOfElementPastLineOfMatchAtIndex(indexPossibleQuantityLine, resultsWithTypes);
                if (indexOfItemOnNextLine > 0) {
                    // Got a next line
                    ocrparser::MatchPtr itemOnNextLine = (*resultsWithTypes)[indexOfItemOnNextLine];
                    // Got a line number and it's the current line + 1?
                    int newLineNumber = (*itemOnNextLine[lineKeyStr].castDown <int>());
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
                    int nextProductIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypeProductNumber, resultsWithTypes, NULL);
                    if (nextProductIndex >= 0) {
                        break; // Break out of loop looking at next 2 lines for possible quantity
                    }
                    // Now search for a quantity item and a price item on that line
                    quantityItemIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypeProductQuantity, resultsWithTypes, NULL);
                    if (quantityItemIndex > 0) {
                        if (retailerParams->quantityPPItemTotal) {
                            // In this case the price per item always follows right after the quantity
                            pricePerItemIndex = quantityItemIndex + 1;
                            if (pricePerItemIndex < resultsWithTypes->size()) {
                                // Check it's on the same line and of type price!
                                ocrparser::MatchPtr pricePerItemItem = (*resultsWithTypes)[pricePerItemIndex];
                                int pricePerItemLine = (*pricePerItemItem[lineKeyStr].castDown <int>());
                                if (pricePerItemLine == currentLine + possibleQuantityLine) {
                                    // Finally, check type
                                    OCRElementType tp = (*pricePerItemItem[typeKeyStr].castDown <OCRElementType>());
                                    if (tp != OCRElementTypePrice)
                                        pricePerItemIndex = -1;
                                } else {
                                    pricePerItemIndex = -1;
                                }
                            } else {
                                pricePerItemIndex = -1;
                            }
                        } else {
                            pricePerItemIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(indexOfItemOnNextLine, OCRElementTypePrice, resultsWithTypes, NULL);
                        }
                        if (pricePerItemIndex > 0) {
                            // Got quantity + price per item!
                            ocrparser::MatchPtr pricePerItemItem = (*resultsWithTypes)[pricePerItemIndex];
                            std::string pricePerItemCString = ocrparser::toUTF8 ((*pricePerItemItem[textKeyStr].castDown <wstring>()));
                            NSMutableString *pricePerItemString = [[NSMutableString alloc] initWithCString:pricePerItemCString.c_str() encoding:NSUTF8StringEncoding];
                            pricePerItemValue = [pricePerItemString floatValue];

                            ocrparser::MatchPtr quantityItem = (*resultsWithTypes)[quantityItemIndex];
                            std::string quantityCString = ocrparser::toUTF8 ((*quantityItem[textKeyStr].castDown <wstring>()));
                            NSMutableString *quantityString = [[NSMutableString alloc] initWithCString:quantityCString.c_str() encoding:NSUTF8StringEncoding];
                            quantityValue = [quantityString intValue];
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
        NSMutableString *partialPriceString;
        bool foundPartialPrice = false;
        int tmpIndexMissingPrice = -1;
        int priceLine = -1;
        int priceIndex = ocrparser::OCRResults::priceIndexForProductAtIndex(i, *resultsWithTypes, &priceLine, retailerParams);
        if ((priceIndex >= 0) && (priceLine > currentLine))
            excludedLines.push_back(priceLine); // We grabbed the price from the line below, mark it as one to ignore (skip)
        if (priceIndex >= 0) {
            ocrparser::MatchPtr priceItem = (*resultsWithTypes)[priceIndex];
            // Remember price (will be set in a property later)
            std::string tmpString = ocrparser::toUTF8 (*priceItem[textKeyStr].castDown <wstring>());
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
            float tmpPriceValue = [tmpStr floatValue];
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
                        priceItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
                        ocrparser::MatchPtr pricePerItemItem = (*resultsWithTypes)[pricePerItemIndex];
                        ocrparser::MatchPtr quantityItem = (*resultsWithTypes)[quantityItemIndex];
                        pricePerItemItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
                        quantityItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
                        priceItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
                    }
                }
                
                // Set alternative prices (if exist)
                if (priceItem.isExists(text2KeyStr)) {
                    std::string tmpString = ocrparser::toUTF8 (*priceItem[text2KeyStr].castDown <wstring>());
                    NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                    priceValue2 = [tmpStr floatValue];
                }
                if (priceItem.isExists(text3KeyStr)) {
                    std::string tmpString = ocrparser::toUTF8 (*priceItem[text3KeyStr].castDown <wstring>());
                    NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                    priceValue3 = [tmpStr floatValue];
                }
                
                // If we have a server price, check if perhaps that price matches text2 or text3!
                if ((serverPrices != nil) && ((priceValue2 > 0) || (priceValue3 > 0))) {
                    // Do we have that product in the server prices?
                    std::string productNumberStr = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
                    NSString *productNumber = [NSString stringWithUTF8String:productNumberStr.c_str()];
//#if DEBUG
//                    if ([productNumber isEqualToString:@"062230309"] && (serverPrices != nil)) {
//                        NSLog(@"");
//                    }
//#endif
                    int priceValue100 = round(priceValue * 100);
                    for (NSDictionary *serverPrice in serverPrices) {
                        NSString *serverSKU = serverPrice[@"sku"];
                        if ([serverSKU isEqualToString:productNumber]) {
                            NSNumber *serverPriceNumber = serverPrice[@"serverPrice"];
                            NSString *serverPriceString = [NSString stringWithFormat:@"%f", [serverPriceNumber floatValue]];
                            float serverPriceValue = [serverPriceString floatValue];
                            int serverPriceValue100 = round(serverPriceValue * 100);
                            if (serverPriceValue100 != priceValue100) {
                                if ((priceValue2 > 0) && (priceValue2 == serverPriceValue)) {
                                    // Upgrade price2 to price!
                                    float tmpPriceValue = priceValue;
                                    wstring tmpPriceText = *priceItem[textKeyStr].castDown <wstring>();
                                    wstring priceText2 = *priceItem[text2KeyStr].castDown <wstring>();
                                    NSLog(@"buildResultsFromMatchVector: upgrading price text2 [%f] (instead of [%f]) - matched server price!", serverPriceValue, priceValue);
                                    priceValue = priceValue2;
                                    priceValue2 = tmpPriceValue;
                                    priceItem[matchKeyText] = ocrparser::SmartPtr<wstring> (new wstring(priceText2)).castDown<ocrparser::EmptyType>();
                                    priceItem[matchKeyText2] = ocrparser::SmartPtr<wstring> (new wstring(tmpPriceText)).castDown<ocrparser::EmptyType>();
                                    priceItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
                                } else if ((priceValue3 > 0) && (priceValue3 == serverPriceValue)) {
                                    // Upgrade price3 to price!
                                    float tmpPriceValue = priceValue;
                                    wstring tmpPriceText = *priceItem[textKeyStr].castDown <wstring>();
                                    wstring priceText3 = *priceItem[text3KeyStr].castDown <wstring>();
                                    NSLog(@"buildResultsFromMatchVector: upgrading price text3 [%f] (instead of [%f]) - matched server price!", serverPriceValue, priceValue);
                                    priceValue = priceValue3;
                                    priceValue3 = tmpPriceValue;
                                    priceItem[matchKeyText] = ocrparser::SmartPtr<wstring> (new wstring(priceText3)).castDown<ocrparser::EmptyType>();
                                    priceItem[matchKeyText3] = ocrparser::SmartPtr<wstring> (new wstring(tmpPriceText)).castDown<ocrparser::EmptyType>();
                                    priceItem[confidenceKeyStr] = (ocrparser::SmartPtr<float> (new float(100.0))).castDown<ocrparser::EmptyType>();
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
            int discountIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(i, OCRElementTypePriceDiscount, resultsWithTypes, retailerParams);
            if (discountIndex > 0) {
                ocrparser::MatchPtr dd = (*resultsWithTypes)[discountIndex];
                std::string tmpString = ocrparser::toUTF8 (*dd[textKeyStr].castDown <wstring>());
                NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                discountValue = [tmpStr floatValue];
            } else {
                // No price or discount - perhaps a partial price?
                int tmpPartialPriceIndex = ocrparser::OCRResults::partialPriceIndexForProductAtIndex(i, *resultsWithTypes, &priceLine, retailerParams);
                if (tmpPartialPriceIndex >= 0) {
                    // If we have server prices and we have a price for this product, see if we can upgrade the partial price!
                    std::string productNumberStr = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
                    NSString *productNumber = [NSString stringWithUTF8String:productNumberStr.c_str()];
                    if (serverPrices != nil) {
                        // Do we have that product in the server prices?
                        for (NSDictionary *serverPrice in serverPrices) {
                            NSString *serverSKU = serverPrice[@"sku"];
                            if ([serverSKU isEqualToString:productNumber]) {
                                NSNumber *serverPriceNumber = serverPrice[@"serverPrice"];
                                NSString *serverPriceString = [NSString stringWithFormat:@"%f", [serverPriceNumber floatValue]];
                                float serverPriceValue = [serverPriceString floatValue];
                                // Close enough to the partial price?
                                BOOL doIt = false;
                                ocrparser::MatchPtr partialPriceElement = (*resultsWithTypes)[tmpPartialPriceIndex];
                                // Get partial price text string
                                std::string tmpString = ocrparser::toUTF8 (*partialPriceElement[matchKeyText].castDown <wstring>());
                                NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                                if ([OCRGlobalResults closeMatchPrice:serverPriceValue toPartialPrice:tmpStr])
                                    doIt = true;
                                if (!doIt) {
                                    if (partialPriceElement.isExists(matchKeyText2)) {
                                        std::string tmpString = ocrparser::toUTF8 (*partialPriceElement[matchKeyText2].castDown <wstring>());
                                        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                                        if ([OCRGlobalResults closeMatchPrice:priceValue toPartialPrice:tmpStr])
                                            doIt = true;
                                    }
                                }
                                if (!doIt) {
                                    if (partialPriceElement.isExists(matchKeyText3)) {
                                        std::string tmpString = ocrparser::toUTF8 (*partialPriceElement[matchKeyText3].castDown <wstring>());
                                        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                                        if ([OCRGlobalResults closeMatchPrice:serverPriceValue toPartialPrice:tmpStr])
                                            doIt = true;
                                    }
                                }
                                if (doIt) {
                                    // Adjust item previously only a partialPrice
                                    partialPriceElement[matchKeyElementType] = ocrparser::SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<ocrparser::EmptyType>();
                                    string tmpS = [serverPriceString UTF8String];
                                    std::wstring ws;
                                    ws.assign(tmpS.begin(), tmpS.end());
                                    partialPriceElement[matchKeyText] = ocrparser::SmartPtr<wstring> (new wstring(ws)).castDown<ocrparser::EmptyType>();
                                    
                                    
                                    // Set priceValue for the sake of code later, to state we actually found a price after all
                                    priceIndex = tmpPartialPriceIndex;
                                    int priceLine =  (*partialPriceElement[lineKeyStr].castDown <int>());
                                    if ((priceIndex >= 0) && (priceLine > currentLine))
                                        excludedLines.push_back(priceLine); // We grabbed the price from the line below, mark it as one to ignore (skip)
                                    priceValue = [serverPriceString floatValue];
                                }
                                break;  // Whether or not we accepted this price, we found it in the serverPrices, quit looking
                            } // product number matches
                        } // searching server prices
                    } // server prices exist
                    if (priceValue < 0) {
                        ocrparser::MatchPtr d = (*resultsWithTypes)[tmpPartialPriceIndex];
                        std::string tmpString = ocrparser::toUTF8 (*d[textKeyStr].castDown <wstring>());
                        partialPriceString = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
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
                ocrparser::MatchPtr productPriceItem = (*resultsWithTypes)[priceIndex];
                std::string tmpCString = ocrparser::toUTF8 (*productPriceItem[textKeyStr].castDown <wstring>());
                NSMutableString *productPriceString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding: NSUTF8StringEncoding];
                // Now look for a line with same product but discount price
                tmpCString = ocrparser::toUTF8 (*item[textKeyStr].castDown <wstring>());
                NSMutableString *productString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding:NSUTF8StringEncoding];
                int nextLine = ocrparser::OCRResults::indexOfElementPastLineOfMatchAtIndex(i, resultsWithTypes);
                for (int j=nextLine; (j>0) && (j < resultsWithTypes->size()); j++) {
                    ocrparser::MatchPtr otherItem = (*resultsWithTypes)[j];
                    int otherLine = (*otherItem[lineKeyStr].castDown <int>());
                    if (std::find(excludedLines.begin(), excludedLines.end(), otherLine) != excludedLines.end()) {
                        // Skip
                        j =ocrparser::OCRResults::indexLastElementOnLineAtIndex(j, resultsWithTypes);
                        continue;
                    }
                    OCRElementType otherTp = (*otherItem[typeKeyStr].castDown <OCRElementType>());
                    if (otherTp != OCRElementTypeProductNumber)
                        continue;
                    std::string tmpCString = ocrparser::toUTF8 (*otherItem[textKeyStr].castDown <wstring>());
                    NSMutableString *otherProductString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding:NSUTF8StringEncoding];
                    if ([otherProductString isEqualToString:productString]) {
                        // Now check if we have a discount price!
                        int discountIndex = ocrparser::OCRResults::elementWithTypeInlineAtIndex(j, OCRElementTypePriceDiscount, resultsWithTypes, NULL);
                        if (discountIndex > 0) {
                            // Found it!
                            ocrparser::MatchPtr discountItem = (*resultsWithTypes)[discountIndex];
                            std::string tmpCString = ocrparser::toUTF8 (*discountItem[textKeyStr].castDown <wstring>());
                            NSMutableString *discountString = [[NSMutableString alloc] initWithCString:tmpCString.c_str() encoding: NSUTF8StringEncoding];
                            if ([discountString isEqualToString:productPriceString]) {
                                // Ignore product AND the matching voided product line with the discount
                                excludedLines.push_back(currentLine);
                                excludedLines.push_back(otherLine);
                                // Skip this entire line and continue
                                i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, resultsWithTypes);
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
        
        // Add price (if found)
        if ((priceValue >= 0) || ((quantityValue > 0) && (pricePerItemValue > 0))) {
            newValueWithRange = [NSMutableDictionary dictionary];
            
            // If we have a quantity & price per item, trust the price per item
            NSNumber *tmpFloat;
            if ((quantityValue > 0) && (pricePerItemValue > 0))
                tmpFloat = [NSNumber numberWithFloat:pricePerItemValue];
            else
                tmpFloat = [NSNumber numberWithFloat:priceValue];
            [newValueWithRange setObject:tmpFloat forKey:@"value"];
        
            [newProduct setObject:newValueWithRange forKey:@"price"];
            
            if (priceValue2 > 0) {
                newValueWithRange = [NSMutableDictionary dictionary];
                NSNumber *tmpFloat = [NSNumber numberWithFloat:priceValue2];
                [newValueWithRange setObject:tmpFloat forKey:@"value"];
                [newProduct setObject:newValueWithRange forKey:@"price2"];
            }
            if (priceValue3 > 0) {
                newValueWithRange = [NSMutableDictionary dictionary];
                NSNumber *tmpFloat = [NSNumber numberWithFloat:priceValue3];
                [newValueWithRange setObject:tmpFloat forKey:@"value"];
                [newProduct setObject:newValueWithRange forKey:@"price3"];
            }
            
            if ((quantityValue > 0) && (pricePerItemValue > 0))
                totalPrices += pricePerItemValue * quantityValue;
            else
                totalPrices += priceValue;
            numPrices++;
        } else {
            // No price, perhaps a discount line?
            if (discountValue > 0) {
                NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
                NSNumber *floatObj = [NSNumber numberWithFloat:discountValue];
                
                totalPrices -= discountValue;
                
                [newValue setObject:floatObj forKey:@"value"];
                    
                [resultsDict[@"discounts"] addObject:newValue];
                
                i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, resultsWithTypes);
                continue;
            } else {
            // No price. Set partialPrice (if found)
                if (foundPartialPrice) {
                    [newProduct setObject:partialPriceString forKey:@"partialPrice"];
     
                    // Try to turn this partial price into a price later, if we have a subtotal + other conditions
                    if (!abortMissingPriceOptimization) {
                        if (indexMissingPrice < 0) {
                            indexMissingPrice = tmpIndexMissingPrice;   // If we decide this is close enough to the price we expect, we'll need to fix the text & type of this item
                            objMissingPrice = newProduct;
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
            [newProduct setObject:[NSNumber numberWithInt:quantityValue] forKey:@"quantity"];
            // If we have a quantity, also create a "totalprice"
            if (priceValue > 0)
                [newProduct setObject:[NSNumber numberWithFloat:priceValue] forKey:@"totalprice"];
        } else
            [newProduct setObject:[NSNumber numberWithInt:1] forKey:@"quantity"];
        
        // Add line number
        newValueWithRange = [NSMutableDictionary dictionary];
        NSNumber *tmpInt = [NSNumber numberWithInt:currentLine];
        [newValueWithRange setObject:tmpInt forKey:@"value"];
        [newProduct setObject:newValueWithRange forKey:@"line"];
        
        // Check if it's not a false product. If so, just mark it but let it live as a product (so that we take its price into account). Only if we got a price for it (otherwise it's likely to be a false positive generated by noise)
        unsigned long status = 0;
        if (productItem.isExists(statusKeyStr)) {
            status = *productItem[statusKeyStr].castDown <unsigned long>();
        }
        if (status & matchKeyStatusNotAProduct) {
            if (priceValue > 0)
                [newProduct setObject:@(YES) forKey:@"ignored"];
            else {
                i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, resultsWithTypes);
                continue;
            }
        }
    
        // Don't add this product if retailer is Walmart, description contains "REF" and there is no price. That's an item found after the subtotal and it is not a product. Note: need to add more generic code that stops looking for anything past the subtotal line.
        if (!((retailerParams->code == RETAILER_WALMART_CODE) && (descriptionIndex >= 0) && ([descriptionString rangeOfString:@"REF"].length != 0)&& (priceValue <= 0))
            // Don't even return products w/o prices when product number len is 5 or less - too many false positives
            && ((priceValue > 0) || (retailerParams->productNumberLen > 5))) {
            // Add the completed product dictionary to the array
            [resultsDict[@"products"] addObject:newProduct];
        }
        
        // Skip pass all elements we checked that were on the same block & line (we don't expect other products on the same block/line)
        i = ocrparser::OCRResults::indexLastElementOnLineAtIndex(i, resultsWithTypes);
    } // for i, iterating over C++ results vector
    
    // Special check if we have a subtotal and only one item has no price but has a partial price that looks like what we miss
    if (!abortMissingPriceOptimization && (subtotalValue > 0) && (indexMissingPrice >= 0)) {
        BOOL doIt = false;
        ocrparser::MatchPtr d = (*resultsWithTypes)[indexMissingPrice];
        // Get partial price text string
        NSString *partialText = [objMissingPrice objectForKey:@"partialPrice"];
        float deltaPrice = subtotalValue - totalPrices;
        if ([OCRGlobalResults closeMatchPrice:deltaPrice toPartialPrice:partialText])
            doIt = true;
        if (!doIt) {
            if (d.isExists(matchKeyText2)) {
                std::string tmpString = ocrparser::toUTF8 (*d[matchKeyText2].castDown <wstring>());
                NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                if ([OCRGlobalResults closeMatchPrice:deltaPrice toPartialPrice:tmpStr])
                    doIt = true;
            }
        }
        if (!doIt) {
            if (d.isExists(matchKeyText3)) {
                std::string tmpString = ocrparser::toUTF8 (*d[matchKeyText3].castDown <wstring>());
                NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:tmpString.c_str() encoding:NSUTF8StringEncoding];
                if ([OCRGlobalResults closeMatchPrice:deltaPrice toPartialPrice:tmpStr])
                    doIt = true;
            }
        }
        if (doIt) {
            // Adjust item previously only a partialPrice
            d[matchKeyElementType] = ocrparser::SmartPtr<OCRElementType> (new OCRElementType(OCRElementTypePrice)).castDown<ocrparser::EmptyType>();
            NSString *tmpStr = [NSString stringWithFormat:@"%f", deltaPrice];
            string tmpS = [tmpStr UTF8String];
            // std::string -> std::wstring
            std::wstring ws;
            ws.assign(tmpS.begin(), tmpS.end());
            d[matchKeyText] = ocrparser::SmartPtr<wstring> (new wstring(ws)).castDown<ocrparser::EmptyType>();

            // And the iOS dictionary
            [objMissingPrice removeObjectForKey:@"partialPrice"];
            NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
            NSNumber *tmpFloat = [NSNumber numberWithFloat:deltaPrice];
            [newValueWithRange setObject:tmpFloat forKey:@"value"];
            [objMissingPrice setObject:newValueWithRange forKey:@"price"];
        }
    }
    
    // If a prices array was provided, check if the sum of all provided price make up the total. If yes, use all provided prices instead of the missing prices!
    if ((serverPrices != nil) && (subtotalValue > 0)) {
        int totalPrices100 = totalPrices * 100;
        int subTotalValue100 = subtotalValue * 100;
        if (totalPrices100 != subTotalValue100) {
            NSArray *products = resultsDict[@"products"];
            // Iterate through resultsDic to collect products with missing prices while also checking each is found in the serverPrices
            float totalMissingPrices = 0;
            BOOL productMissingPriceNotFoundInServerPrices = false;
            NSMutableArray *productsMissingPrices = [NSMutableArray array];
            for (NSDictionary *d in products) {
                if ((d[@"sku"] != nil) && (d[@"price"] == nil)) {
                    NSString *productNumber = d[@"sku"][@"value"];
                    // Do we have that product in the server prices?
                    BOOL found = false;
                    for (NSDictionary *serverPrice in serverPrices) {
                        NSString *serverSKU = serverPrice[@"sku"];
                        if ([serverSKU isEqualToString:productNumber]) {
                            totalMissingPrices += [serverPrice[@"serverPrice"] floatValue];
                            // Remember that product to fix its price later (if everything checks out)
                            NSDictionary *newProduct = [NSDictionary dictionaryWithObjectsAndKeys:d, @"product", serverPrice[@"serverPrice"], @"serverPrice", nil];
                            [productsMissingPrices addObject:newProduct];
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        productMissingPriceNotFoundInServerPrices = true;
                        break;
                    }
                }
            }
            if (!productMissingPriceNotFoundInServerPrices) {
                int totalMissingPrices100 = totalMissingPrices * 100;
                if (subTotalValue100 - totalPrices100 == totalMissingPrices100) {
                    // Do it! Fill in all missing prices!
                    for (NSDictionary *p in productsMissingPrices) {
                        NSMutableDictionary *product = p[@"product"];
                        NSMutableDictionary *price = [NSMutableDictionary dictionary];
                        price[@"value"] = p[@"serverPrice"];
                        product[@"price"] = price;
                    }
                }
            }
        } // subtotal doesn't match sum of prices
    } // got server prices and subtotal
    
	return resultsDict;
}
#endif // 0

+(NSDictionary *)buildiOSResultsWithProductsVector:(vector<scannedProduct> &)products DiscountsVector:(vector<float> &)discounts Subtotal:(float)subTotal Total:(float)total Date:(wstring)dateString AndPhones:(vector<scannedItem> &)phones
{

    NSMutableDictionary *resultsDict = [NSMutableDictionary dictionary];
    resultsDict[@"products"] = [NSMutableArray array];
    resultsDict[@"discounts"] = [NSMutableArray array];
    resultsDict[@"phones"] = [NSMutableArray array];
    
    // Add subtotal property (if present)
    if (subTotal >= 0) {
        NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
        NSNumber *floatObj = [NSNumber numberWithFloat:subTotal];
        [newValue setObject:floatObj forKey:@"value"];
        
        resultsDict[@"subtotal"] = newValue;
    }
    
    // Add total property (if present)
    if (total >= 0) {
        NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
        NSNumber *floatObj = [NSNumber numberWithFloat:total];
        [newValue setObject:floatObj forKey:@"value"];
        
        resultsDict[@"total"] = newValue;
    }
    
    // Add date (if present)
    if (dateString.length() > 0) {
        NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(dateString).c_str() encoding:NSUTF8StringEncoding];
        [newValueWithRange setObject:tmpStr forKey:@"value"];
        
        resultsDict[@"date"] = newValueWithRange;
    }
    
    // Build discounts
    for (int i=0; i<discounts.size(); i++) {
        float discountValue = discounts[i];
        NSNumber *floatObj = [NSNumber numberWithFloat:discountValue];
        
        NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
        [newValue setObject:floatObj forKey:@"value"];
            
        [resultsDict[@"discounts"] addObject:newValue];
    }
    
    // Build products
    for (int i=0; i<products.size(); i++) {
        scannedProduct p = products[i];
        
        // Create dictionary to hold the attributes of this product
        NSMutableDictionary *newProduct = [NSMutableDictionary dictionary];
        
        // Add sku property
        NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.sku).c_str() encoding:NSUTF8StringEncoding];

        [newValueWithRange setObject:tmpStr forKey:@"value"];
        
        [newProduct setObject:newValueWithRange forKey:@"sku"];
        
        // Set alternative sku2 (text2) if set
        if (p.sku2.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.sku2).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValue forKey:@"sku2"];
        }
        // Set alternative sku3 (text3) if set
        if (p.sku3.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.sku3).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValue forKey:@"sku3"];
        }
        
        // Add line number
        newValueWithRange = [NSMutableDictionary dictionary];
        NSNumber *tmpInt = [NSNumber numberWithInt:p.line];
        [newValueWithRange setObject:tmpInt forKey:@"value"];
        [newProduct setObject:newValueWithRange forKey:@"line"];
        
        // Add description (if present)
        if (p.description.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSString *descriptionString = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:descriptionString forKey:@"value"];
        
            [newProduct setObject:newValue forKey:@"description"];
        }
        
        // Set alternative desc2 (text2) if set
        if (p.description2.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description2).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValue forKey:@"description2"];
        }
        // Set alternative desc3 (text3) if set
        if (p.description3.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description3).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newProduct setObject:newValue forKey:@"description3"];
        }
        
        // Add partial price (if present)
        if (p.partialPrice.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSString *descriptionString = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.partialPrice).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:descriptionString forKey:@"value"];
        
            [newProduct setObject:newValue forKey:@"partialprice"];
        }
        
        // Add price (if found)
        if (p.price >= 0) {
            newValueWithRange = [NSMutableDictionary dictionary];
            NSNumber *tmpFloat = [NSNumber numberWithFloat:p.price];
            [newValueWithRange setObject:tmpFloat forKey:@"value"];
        
            [newProduct setObject:newValueWithRange forKey:@"price"];
            
            if (p.price2 > 0) {
                newValueWithRange = [NSMutableDictionary dictionary];
                NSNumber *tmpFloat = [NSNumber numberWithFloat:p.price2];
                [newValueWithRange setObject:tmpFloat forKey:@"value"];
                [newProduct setObject:newValueWithRange forKey:@"price2"];
            }
            if (p.price3 > 0) {
                newValueWithRange = [NSMutableDictionary dictionary];
                NSNumber *tmpFloat = [NSNumber numberWithFloat:p.price3];
                [newValueWithRange setObject:tmpFloat forKey:@"value"];
                [newProduct setObject:newValueWithRange forKey:@"price3"];
            }
        } else if (p.sku.length() <= 7){
            // If short product and no price, skip
            continue;
        }
        
        // Set quantity
        [newProduct setObject:[NSNumber numberWithInt:p.quantity] forKey:@"quantity"];

        if (p.totalPrice > 0)
            [newProduct setObject:[NSNumber numberWithFloat:p.totalPrice] forKey:@"totalprice"];
            
        if (p.ignored)
            [newProduct setObject:@(YES) forKey:@"ignored"];
        
        if (p.not_eligigle)
            [newProduct setObject:@(YES) forKey:@"not_eligible"];
        
        [resultsDict[@"products"] addObject:newProduct];
    }
    
    // Build phones
    for (int i=0; i<phones.size(); i++) {
        scannedItem p = phones[i];
        
        // Create dictionary to hold the attributes of this item
        NSMutableDictionary *newItem = [NSMutableDictionary dictionary];
        
        // Add itemText property
        NSMutableDictionary *newValueWithRange = [NSMutableDictionary dictionary];
        NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.itemText).c_str() encoding:NSUTF8StringEncoding];

        [newValueWithRange setObject:tmpStr forKey:@"value"];
        
        [newItem setObject:newValueWithRange forKey:@"text"];
        
        if (p.itemText2.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.itemText2).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newItem setObject:newValue forKey:@"text2"];
        }
        if (p.itemText3.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.itemText3).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newItem setObject:newValue forKey:@"sku3"];
        }
        
        // Add line number
        newValueWithRange = [NSMutableDictionary dictionary];
        NSNumber *tmpInt = [NSNumber numberWithInt:p.line];
        [newValueWithRange setObject:tmpInt forKey:@"value"];
        [newItem setObject:newValueWithRange forKey:@"line"];
        
        // Add description (if present)
        if (p.description.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSString *descriptionString = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:descriptionString forKey:@"value"];
        
            [newItem setObject:newValue forKey:@"description"];
        }
        
        // Set alternative desc2 (text2) if set
        if (p.description2.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description2).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newItem setObject:newValue forKey:@"description2"];
        }
        // Set alternative desc3 (text3) if set
        if (p.description3.length() > 0) {
            NSMutableDictionary *newValue = [NSMutableDictionary dictionary];
            NSMutableString *tmpStr = [[NSMutableString alloc] initWithCString:ocrparser::toUTF8(p.description3).c_str() encoding:NSUTF8StringEncoding];
            [newValue setObject:tmpStr forKey:@"value"];
            [newItem setObject:newValue forKey:@"description3"];
        }
        
        [resultsDict[@"phones"] addObject:newItem];
    } // Build phones
    
	return resultsDict;
} // buildiOSResultsFromVectorResults


@end
