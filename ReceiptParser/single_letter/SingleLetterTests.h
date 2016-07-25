#ifndef SINGLE_LETTER_TESTS
#define SINGLE_LETTER_TESTS

#include <vector>
#include "slcommon.h"

struct ConnectedComponent {
    int ymin;
    int xmin;
    int ymax;
    int xmax;
    int area;
    float getHeight() { return ymax-ymin+1; }
    float getWidth() {  return xmax-xmin+1; }
};

typedef std::vector<ConnectedComponent> ConnectedComponentList;

//Describes test results for an openings test
struct OpeningsTestResults {
    float maxDepth; //The maximal depth detected
    float maxDepthCoord; //the first coordinate where the maxDepth was detected
    float avgWidth; //The average width along the line of the max depth
    float maxWidth; //The maximal width along the line of the max depth
    float area;     //The sum of all widths along the line of the max depth
};

//Describes test results for an openings test with limited depth
struct LimitedOpeningsTestResults : public OpeningsTestResults {
    float minWidth;             //The minimal width along the (first) line with the max depth
    float minWidthStartCoord;   //The starting coordinate of outer (first) minimal width
    float minWidthEndCoord;     //The ending coordinate of the outer (first) minimal width
    float minWidthDepth;        //The depth in which the minimal width was detected
    float getMinWidthCoord() { return (minWidthStartCoord+minWidthEndCoord)/2; }
};

// Forward declaration
class cimage;

class SingleLetterTests {
private:
    cimage* m_im;
public:
    enum OpeningsSide { Top, Bottom, Left, Right };
    enum BoundsRequired { Unbound = 0, Bound = 1, Any = 2 };

    SingleLetterTests(cimage* im) : m_im(im), m_is_analyzecomp_cached(false) {}
    virtual ~SingleLetterTests();

    ConnectedComponentList getConnectedComponents(float minSize = 0.03);
    ConnectedComponentList getInverseConnectedComponents(
            bool include_background = false, float minSize = 0.03);
    bool getOpenings(OpeningsTestResults& result, OpeningsSide side, 
            float start_position, float end_position, 
            BoundsRequired require_start_bound = Bound, BoundsRequired require_end_bound = Bound, bool only_main_component = true, bool bounds_in_range = false);
    bool getOpeningsLimited(
            LimitedOpeningsTestResults& result, OpeningsSide side, 
            float start_position, float end_position, 
            float test_depth, 
            BoundsRequired require_start_bound = Bound, BoundsRequired require_end_bound = Bound, bool only_main_component = true, bool bounds_in_range = false);
    bool getSide(
            OpeningsSide side, 
            float requiredLen,
            bool only_main_component = true);
    
    SegmentList getHorizontalSegments(float position, float line_width);
    SegmentList getHorizontalSegmentsPixels(int start, int end);
    ocrparser::CGRect getBoxForNHorizontalSegments(int nSegments, float position, float line_width, SegmentList &segments);
    SegmentList getVerticalSegments(float position, float line_width);
    SegmentList getVerticalSegmentsPixels(int start, int end);

private:
    ConnectedComponentList convert(CCTYPE_LIST data);
    SegmentList convert(SegmentList data);
    ocrparser::CGRect convert(ocrparser::CGRect rect);
	CCTYPE_LIST analyzecomp(float minSize = 0.03);
    CCTYPE_LIST m_cached_analyzecomp;
    bool m_is_analyzecomp_cached;
    float m_minSize_for_cached_analyzed_comp;
};

#endif
