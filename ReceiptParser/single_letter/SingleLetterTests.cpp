#include "SingleLetterTests.h"
#include "cimage.h"

SingleLetterTests::~SingleLetterTests() { 
    delete m_im; 
}

ConnectedComponentList SingleLetterTests::getConnectedComponents(float minSize /*= 0.03 */) {
	// Analyze connected components
	CCTYPE_LIST raw_result = analyzecomp(minSize);
    return convert(raw_result);
}

// TODO: Cache raw_result?
ConnectedComponentList SingleLetterTests::getInverseConnectedComponents(
        bool include_background, /*= false*/
        float minSize /*= 0.03 */) {
	// Analyze connected components (for the letter size)
	CCTYPE_LIST raw_result = analyzecomp();
    int letter_size = raw_result[0][4];

    // Create inverse image and analyze connect components
	cimage* inv_im = m_im->inverse();
	CCTYPE_LIST raw_i_result = inv_im->analyzecomp(
            include_background, letter_size, minSize);
	delete inv_im;

    return convert(raw_i_result);
}

bool SingleLetterTests::getOpenings(
        OpeningsTestResults& result,
        OpeningsSide side, float start_position, float end_position, 
        BoundsRequired require_start_bound /*= Bound*/,
        BoundsRequired require_end_bound /*= Bound*/,
        bool only_main_component /*= true*/,
        bool bounds_in_range /*= false */) {

    LimitedOpeningsTestResults res;
    bool success = getOpeningsLimited(res, side, start_position, end_position, 
                                      1/*test_depth=100%*/, require_start_bound, require_end_bound, only_main_component, bounds_in_range);

    result = (OpeningsTestResults&)res;

    return success;
}

// test_depth: indicates over which span to compute all metrics of the opening. For example when testing the opening of a 'C' it's important to know how wide the opening is on the right. The normal opening test would not give us the right "min width" value because that value could be on the left side where the C curves. Limiting the test depth to say 0.50 of the width stops in the middle of the C and makes sure min width will relate to the actual opening we care about. test_depth also limits the maxDepth possible value, which can't exceed test_depth, so if you require maxDepth > 0.50, need to set test_depth to 0.51 (or more).
bool SingleLetterTests::getOpeningsLimited(
        LimitedOpeningsTestResults& result,
        OpeningsSide side, float start_position, float end_position, 
        float test_depth, 
        BoundsRequired require_start_bound /*= Bound*/,
        BoundsRequired require_end_bound /*= Bound*/,
        bool only_main_component /*= true*/,
        bool bounds_in_range /*= false */) {

    CCTYPE_LIST cc = analyzecomp();
    if (cc.size() <= 1) {
        // No CC found. Aborting.
        return false;
    } 
    
    float width, maxdepth, maxwidth, area, max_depth_coord, min_width, minwidth_start, minwidth_end, minwidth_depth;
    m_im->openings_test(
            only_main_component, 
            only_main_component ? cc[1] : cc[0], 
            start_position, end_position, 
            (cimage::BoundsRequired)require_start_bound, (cimage::BoundsRequired)require_end_bound, //ugly cast of equivalent types
            bounds_in_range,
            test_depth, 
            (side == Top) || (side == Bottom), 
            ((side == Top) || (side == Left)) ? 1 : -1, 
            maxdepth, width, maxwidth, area, max_depth_coord, min_width, minwidth_start, minwidth_end, minwidth_depth);

    result.maxDepth = maxdepth;
    result.maxDepthCoord = max_depth_coord;
    result.avgWidth = width;
    result.maxWidth = maxwidth;
    result.area = area;

    result.minWidth = min_width;
    result.minWidthStartCoord = minwidth_start;
    result.minWidthEndCoord = minwidth_end;
    result.minWidthDepth = minwidth_depth;

    return (maxdepth > 0);
}


bool SingleLetterTests::getSide(
        OpeningsSide side,
        float requiredLen,
        bool only_main_component /* = true*/) { 

    //PQ11 if I call getSide multiple times on a same class instance, won't the call below repeat the same analysis over and over again?
    CCTYPE_LIST cc = analyzecomp();
    if (cc.size() <= 1) {
        // No CC found. Aborting.
        return false;
    } 
    
    bool hasTop = m_im->side_test(
            only_main_component, 
            only_main_component ? cc[1] : cc[0],    // Either the whole thing or else the main component
            (side == Top) || (side == Bottom),      // True if we are testing the top or the bottom
            ((side == Top) || (side == Left)) ? 1 : -1, // 1 if we are testing the top or left, -1 otherwise (i.e. it's the step - moving forward or backward)
            requiredLen);    

    return (hasTop);
}




// Looks for intersections along the X axis
SegmentList SingleLetterTests::getHorizontalSegments(float position, float line_width) {
    return convert(m_im->linetest_horizontal(100.0f * position, 100.0f * line_width));
}

// Returns the box containing all pixels wihin the range having exactly the specified number of horizontal intersections
ocrparser::CGRect SingleLetterTests::getBoxForNHorizontalSegments(int nSegments, float position, float line_width, SegmentList &segments) {
    CGRect box = convert(m_im->linetest_BoxForNHorizontalSegments(nSegments, 100.0f * position, 100.0f * line_width, segments));
    segments = convert(segments);
    return box;
}

// Parameters:
// - start: at which Y coordinate to start
// - end: at which Y coordinate to end (included), i.e. passing (height-1,height-1) means horizontal segments at the bottom of the letter
SegmentList SingleLetterTests::getHorizontalSegmentsPixels(int start, int end) {
    return convert(m_im->linetest_horizontal_absolute(start + 1, end + 2));
}

// Looks for intersections along the Y axis
SegmentList SingleLetterTests::getVerticalSegments(float position, float line_width) {
    return convert(m_im->linetest_vertical(100.0f * position, 100.0f * line_width));
}

// Parameters:
// - start: at which X coordinate to start
// - end: at which X coordinate to end (included), i.e. passing (0,0) means vertical segments at the leftmost side of the letter
SegmentList SingleLetterTests::getVerticalSegmentsPixels(int start, int end) {
    return convert(m_im->linetest_vertical_absolute(start + 1, end + 2));
}

ConnectedComponentList SingleLetterTests::convert(CCTYPE_LIST data) {
    ConnectedComponentList result(data.size());

    int idx = 0;
    for(CCTYPE_LIST::iterator iter = data.begin(); iter != data.end(); iter++) {
        ConnectedComponent cc;
        cc.ymin = ((*iter)[Y_MIN_COORD]) -1;
        cc.xmin = ((*iter)[X_MIN_COORD]) -1;
        cc.ymax = ((*iter)[Y_MAX_COORD]) -1;
        cc.xmax = ((*iter)[X_MAX_COORD]) -1;
        cc.area = (*iter)[SIZE_COORD];
        result[idx++] = cc;
    }

    return result;
}

ocrparser::CGRect SingleLetterTests::convert(ocrparser::CGRect rect) {
    CGRect result = CGRect(rect.origin.x - 1, rect.origin.y - 1, rect.size.width, rect.size.height);
    
    return result;
}

SegmentList SingleLetterTests::convert(SegmentList data) {
    SegmentList result(data.size());
    
    int idx = 0;
    for(SegmentList::iterator iter = data.begin(); iter != data.end(); iter++) {
        result[idx++] = Segment(iter->startPos - 1, iter->endPos - 1);
    }
    
    return result;
}

CCTYPE_LIST SingleLetterTests::analyzecomp(float minSize) {
    if (!m_is_analyzecomp_cached || (minSize != m_minSize_for_cached_analyzed_comp)) {
        m_cached_analyzecomp = m_im->analyzecomp(true, 0, minSize);
        m_minSize_for_cached_analyzed_comp = minSize;
    }
    return m_cached_analyzecomp;
}
