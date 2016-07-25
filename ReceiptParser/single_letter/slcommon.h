#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef __CONSOLE__
#include "ocr.hh"
#endif //__CONSOLE__

#ifdef TARGET_IPHONE
#ifdef CODE_CPP
void displayDebugMessage(const char* msg);
#define NSLog(args...) {char *buffer = (char *)malloc(2048); snprintf(buffer, 2048, args);displayDebugMessage(buffer); free(buffer);}
#endif
#elif defined(ANDROID)
#include <android/log.h>
#define NSLog(args...) {__android_log_print(ANDROID_LOG_DEBUG,"windfall", args);}
#else
#define NSLog(args...) {printf(args);printf("\n");fflush(stdout);}
#endif

#if DEBUG_LOG
#define DebugLog(args...) NSLog(args)
#else
#define DebugLog(args...)
#endif

#include <string>
#include <vector>
#include <map>

struct Segment {
    int startPos;
    int endPos;
    
    Segment(int _startPos, int _endPos) : startPos(_startPos), endPos(_endPos) {} 
    Segment() : startPos(-1), endPos(-1) {} 
};

typedef std::vector<Segment> SegmentList;

typedef std::vector<float> HORZTEST_TYPE;
typedef std::vector<int> CCTYPE;
typedef std::vector<CCTYPE> CCTYPE_LIST;
typedef std::map<char, float> GROUP_SCORES;
typedef std::vector<std::vector<float> > HORZTESTMAX_TYPE;
typedef std::map<std::string, float> OUTPUT_PARAMS;

// [y_min,x_min,y_max,x_max,s]
// CCTYPE coordinates
enum { 
    Y_MIN_COORD = 0,
    X_MIN_COORD = 1,
    Y_MAX_COORD = 2,
    X_MAX_COORD = 3,
    SIZE_COORD  = 4
};


typedef struct {
    GROUP_SCORES group_scores;
    OUTPUT_PARAMS output_params;
} SL_RESULTS;

extern const std::string OUTPUT_PARAM_BODY_HEIGHT;
extern const std::string OUTPUT_PARAM_I_BODY_HEIGHT;

extern const std::vector<char> il_group;
SL_RESULTS il_group_scores(
    CCTYPE_LIST& conn_comps, CCTYPE_LIST& inv_conn_comps, 
    int horz_cuts, int vert_cuts, 
    float bottom_right_corner_height_percent, float top_right_corner_height_percent,
    HORZTEST_TYPE& horz_test,
    HORZTESTMAX_TYPE& horzmax_test,
    char ocr_letter);

extern const std::vector<char> oa_group;
SL_RESULTS oa_group_scores(
    CCTYPE_LIST& conn_comps, CCTYPE_LIST& inv_conn_comps, 
    int horz_cuts, int vert_cuts, 
    float bottom_right_corner_height_percent, float top_right_corner_height_percent,
    HORZTEST_TYPE& horz_test, float flat_right,
    float right_size_opening_depth_percent,
    char ocr_letter);

void print_sl_results(GROUP_SCORES scores);

class cimage;
SL_RESULTS get_single_letter_scores(char letter, cimage& ci);
char suggest_sl_replacement(char orig_letter, GROUP_SCORES& group_scores, bool force_replace);

enum {
    GET_OPENINGS_TOP = 0,
    GET_OPENINGS_BOTTOM = 1,
    GET_OPENINGS_LEFT = 2,
    GET_OPENINGS_RIGHT = 3
};
bool get_openings(cimage& ci,           //input image to work on
        int side,                       //use GET_OPENINGS_... enum here
        float start_pos, float end_pos, //starting/ending position in the range of [0,1] relative to the low coordinate. 
                                        //0 is the top or left corner, and 1 is the bottom or right
        bool bounds_required,           //if true widths are not taken into account if they are not bounded from both sides
        float letter_thickness,         //the approximate thickness of the brush [0,1] (percentage of the dimension)
        float* outWidth, float* outHeight, //width and height of the main component (accepts NULL)
        float* outMaxDepth,             //max depth detected
        float* outAverageWidth,         //The average width in the point with the max depth
        float* outOpeningWidth,         //The width of the first letter_thickness pixels in the point of max depth
        float* outMaxWidth,             //max width in the point of max depth
        float* outArea                  //area inside the opening
        );
 
float get_letter_frequency(char letter);
float get_o_like_a_score(cimage& ci);
bool is_in_il_group(char letter);
bool is_in_oa_group(char letter);


#ifdef __CONSOLE__
struct origin_t {
	float x;
	float y;
	origin_t():x(0),y(0){}
	origin_t(float _x, float _y):x(_x),y(_y){}

	bool operator==(const origin_t& l)const {
		return (x == l.x) && (y == l.y);
	}
};

struct dimensions {
	float width;
	float height;
	dimensions():width(0), height(0){}
	dimensions(float w, float h):width(w), height(h){}
	bool operator==(const dimensions& l) const {return (width == l.width)
		&& (height == l.height);}
};

struct Rect {
	origin_t origin;
	dimensions size;
	Rect(float x, float y, float width, float height):origin(x,y),
	size(width,height){}

	Rect(){}


	bool operator==(const Rect& l) const {return (origin == l.origin) &&
		(size == l.size);}
};
#endif //__CONSOLE__

#endif //__COMMON_H__
