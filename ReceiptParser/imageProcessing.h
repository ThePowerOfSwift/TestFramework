#ifndef WINDFALL_IMAGE_PROCESSING_
#define WINDFALL_IMAGE_PROCESSING_

//
//  imageProcessing.h
//
//  Created by Patrick Questembert on 4/21/15
//  Copyright 2015 Windfall Labs Holdings LLC, All rights reserved.
//

namespace SBC_ImageProcessing {
	// input - 8 bit (gray level) input, binary - location of binary (also 8 bit) output, output - 8 bit output
	// return true on success
	bool PreprocessImage(unsigned char* input, unsigned char* binary, unsigned char* output, int w, int h);
};

#endif  // WINDFALL_IMAGE_PROCESSING_

