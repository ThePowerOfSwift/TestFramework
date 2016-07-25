#ifndef __CIMAGE_H__
#define __CIMAGE_H__

#include "slcommon.h"
#include <vector>
#include <algorithm>

using namespace std;
#ifndef __CONSOLE__
using namespace ocrparser;
#endif
typedef unsigned char ubyte;

class cimage {
	public:
    enum BoundsRequired { Unbound = 0, Bound = 1, Any = 2 };

	cimage(ubyte* large_image, int large_height, int large_width, Rect rect, bool neighbors4);
	cimage(int height, int width, bool neighbors4);
	cimage(char filename[], bool neighbors4);
	virtual ~cimage();
    inline bool is_good() { return (m_buffer != NULL); }

	cimage* inverse(); //returns the inverse image
	
	inline ubyte getat(int h, int w);
	inline void setat(int h, int w, ubyte val);
	
	int m_height;
	int m_width;
    bool m_neighbors4; //true if working with 4-neighbors
	
	CCTYPE_LIST analyzecomp(bool include_frame, int letter_size = 0, float minSize = 0.03);
    bool top_horiz_bar_test();
	SegmentList linetest_vertical(float linecenter, float linewidth);
	SegmentList linetest_vertical_absolute(int startw, int endw);
    SegmentList linetest_horizontal(float linecenter, float linewidth);
    SegmentList linetest_horizontal_absolute(int starth, int endw);
    ocrparser::CGRect linetest_BoxForNHorizontalSegments(int nSegments, float linecenter, float linewidth, SegmentList &segments);
    ocrparser::CGRect linetest_BoxForNHorizontalSegments_absolute(int nSegments, int starth, int endh, SegmentList &segments);
    
	void debug_print();
    float bottom_right_corner_test(CCTYPE concomp); 
    float top_right_corner_test(CCTYPE concomp);
    float flat_right_test(CCTYPE concomp);
    HORZTEST_TYPE horz_integration_test(CCTYPE concomp); 
    HORZTESTMAX_TYPE horz_integration_maxtest(CCTYPE concomp, float max_threshold);
    bool side_test(bool only_main_component, CCTYPE concomp, bool is_topbottom, int direction, float requiredLen);
    void openings_test(bool only_main_component, CCTYPE concomp, float start_position, float end_position, BoundsRequired start_bound_required, BoundsRequired end_bound_required, bool bounds_in_tange, float letter_thickness, bool is_topbottom, int direction, float& max_depth, float& width, float& max_width, float& area, float& max_depth_coord, float& min_width, float& minwidth_start, float& minwidth_end, float& minwidth_depth);
    void old_openings_test(CCTYPE concomp, float start_position, float end_position, bool bounds_required, float letter_thickness, bool is_topbottom, int direction, float& max_depth, float& width, float& opening_width, float& max_width, float& area);

	private:
	ubyte* m_buffer;
    cimage* m_cached_big_component;
    vector<int>* m_integrated; //horizontal integration of the largest element

    void integrate(CCTYPE concomp); // integrate the largest component
};

#endif // __CIMAGE_H__
