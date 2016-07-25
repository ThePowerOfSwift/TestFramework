//
//  LineValidation.hh
//  Windfall
//
//  Created by Patrick Questembert on 4/18/15.
//  Copyright (c) 2015 Windfall. All rights reserved.
//

#ifndef __Windfall__LineValidation__
#define __Windfall__LineValidation__

#include <stdio.h>

#import "OCRClass.hh"
#import "OCRPage.hh"

namespace ocrparser {

// Possible flags set in the flags2 field of an OCRRect struct
#define FLAGS2_NO_SPACE                0x02

void OCRValidateCheckMissingSpaces(SmartPtr<OCRWord> line, OCRResults *results);
void OCRValidateFinalPass(SmartPtr<OCRWord> line, OCRResults *results);
#if OOCR_ELIMINATE_NARROW_SPACES
void OCRValidateCheckSpuriousSpaces(SmartPtr<OCRWord> line, OCRResults *results);
#endif
void OCRValidate(SmartPtr<OCRWord> line, OCRResults *results);

} // namespace ocrparser

#endif /* defined(__Windfall__LineValidation__) */
